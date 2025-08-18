#include "abort-signal.h"
#include "abort-controller.h"

#include "../dom-exception.h"
#include "../event/event.h"
#include "../timers.h"



namespace builtins::web::abort {

using event::Event;
using event::EventTarget;

const JSFunctionSpec AbortSignal::static_methods[] = {
    JS_FN("abort", abort, 1, JSPROP_ENUMERATE),
    JS_FN("timeout", timeout, 1, JSPROP_ENUMERATE),
    JS_FN("any", any, 1, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec AbortSignal::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec AbortSignal::methods[] = {
    JS_FN("throwIfAborted", throwIfAborted, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec AbortSignal::properties[] = {
    JS_PSG("reason", reason_get, JSPROP_ENUMERATE),
    JS_PSG("aborted", aborted_get, JSPROP_ENUMERATE),
    JS_PSGS("onabort", onabort_get, onabort_set, JSPROP_ENUMERATE),
    JS_PS_END,
};

bool AbortSignal::aborted_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  args.rval().setBoolean(is_aborted(self));
  return true;
}

bool AbortSignal::reason_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  args.rval().set(JS::GetReservedSlot(self, Slots::Reason));
  return true;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-onabort
bool AbortSignal::onabort_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  args.rval().set(JS::GetReservedSlot(self, Slots::OnAbort));
  return true;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-onabort
bool AbortSignal::onabort_set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);

  RootedValue new_callback(cx, args.get(0));
  RootedValue curr_callback(cx, JS::GetReservedSlot(self, Slots::OnAbort));

  RootedValue opts(cx, JS::FalseValue());
  RootedValue type(cx, JS::StringValue(abort_type_atom));

  if (curr_callback.isObject()) {
    if (!EventTarget::remove_listener(cx, self, type, curr_callback, opts)) {
      return false;
    }
  }

  if (new_callback.isObject()) {
    if (!EventTarget::add_listener(cx, self, type, new_callback, opts)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-timeout
bool AbortSignal::timeout(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "timeout", 1)) {
    return false;
  }

  RootedObject self(cx, create_with_timeout(cx, args.get(0)));
  if (self == nullptr) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-abort
bool AbortSignal::abort(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "abort", 1)) {
    return false;
  }

  // The abort() method steps are inlined in the AbortSignal::create_with_reason method.
  RootedObject self(cx, create_with_reason(cx, args.get(0)));
  if (self == nullptr) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-any
bool AbortSignal::any(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "any", 1)) {
    return false;
  }

  // The any() method steps are inlined in the AbortSignal::create_with_signals method.
  RootedObject self(cx, create_with_signals(cx, args));
  if (self == nullptr) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-throwifaborted
bool AbortSignal::throwIfAborted(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

   // Steps: Throw this's abort reason, if this's AbortController has signaled
   // to abort; otherwise, does nothing.
  if (is_aborted(self)) {
    RootedValue reason(cx, JS::GetReservedSlot(self, Slots::Reason));
    JS_SetPendingException(cx, reason);
  }

  return true;
}

// Handler that is called when the AbortSignal timeout occurs
bool AbortSignal::on_timeout(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "on_timeout", 2)) {
    return false;
  }

  RootedValue self_val(cx, args.get(0));
  RootedValue timeout_val(cx, args.get(1));

  MOZ_ASSERT(is_instance(self_val));

  RootedObject self(cx, &self_val.toObject());
  return abort(cx, self, timeout_val);
}

AbortSignal::AlgorithmList *AbortSignal::algorithms(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<AlgorithmList *>(JS::GetReservedSlot(self, Slots::Algorithms).toPrivate());
}

WeakIndexSet *AbortSignal::source_signals(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<WeakIndexSet *>(JS::GetReservedSlot(self, Slots::SourceSignals).toPrivate());
}

WeakIndexSet *AbortSignal::dependent_signals(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<WeakIndexSet *>(
      JS::GetReservedSlot(self, Slots::DependentSignals).toPrivate());
}

Value AbortSignal::reason(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Reason);
}

// https://dom.spec.whatwg.org/#abortsignal-add
bool AbortSignal::add_algorithm(JSObject *self, js::UniquePtr<AbortAlgorithm> algorithm) {
  MOZ_ASSERT(is_instance(self));

  // To add an algorithm algorithm to an AbortSignal object signal:
  // 1. If signal is aborted, then return.
  if (is_aborted(self)) {
    return false;
  }

  // 2. Append algorithm to signal's abort algorithms.
  auto *algorithms = AbortSignal::algorithms(self);
  return algorithms->append(std::move(algorithm));
}

bool AbortSignal::is_dependent(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Dependent).toBoolean();
}

// https://dom.spec.whatwg.org/#abortsignal-aborted
bool AbortSignal::is_aborted(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  // An AbortSignal object is aborted when its abort reason is not undefined.
  return !JS::GetReservedSlot(self, Slots::Reason).isUndefined();
}

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
bool AbortSignal::abort(JSContext *cx, HandleObject self, HandleValue reason) {
  MOZ_ASSERT(is_instance(self));

  // 1. If signal is aborted, then return.
  if (is_aborted(self)) {
    return true;
  }

  // 2. Set signal's abort reason to reason if it is given;
  // otherwise to a new "AbortError" DOMException.
  set_reason(cx, self, reason);

  // 3. Let dependentSignalsToAbort be a new list.
  JS::RootedObjectVector dep_signals_to_abort(cx);

  // 4. For each dependentSignal of signal's dependent signals:
  auto *dep_signals = dependent_signals(self);
  for (auto const &sig : dep_signals->items()) {
    RootedObject signal(cx, sig);
    // 1. If dependentSignal is not aborted:
    if (!is_aborted(signal)) {
      // 1. Set dependentSignal's abort reason to signal's abort reason.
      set_reason(cx, signal, reason);
      // 2. Append dependentSignal to dependentSignalsToAbort.
      if (!dep_signals_to_abort.append(signal)) {
        return false;
      }
    }
  };

  // 5. Run the abort steps for signal.
  run_abort_steps(cx, self);

  // 6. For each dependentSignal of dependentSignalsToAbort, run the abort steps for
  // dependentSignal.
  for (auto &sig : dep_signals_to_abort) {
    RootedObject signal(cx, sig);
    run_abort_steps(cx, signal);
  }

  return true;
}

// https://dom.spec.whatwg.org/#run-the-abort-steps
bool AbortSignal::run_abort_steps(JSContext *cx, HandleObject self) {
  // To run the abort steps for an AbortSignal signal:
  // 1. For each algorithm of signal's abort algorithms: run algorithm.
  auto *algorithms = AbortSignal::algorithms(self);
  for (auto &algorithm : *algorithms) {
    if (!algorithm->run(cx)) { return false;
}
  }

  // 2. Empty signals's abort algorithms.
  algorithms->clear();

  // 3. Fire an event named abort at signal.
  RootedValue res_val(cx);
  RootedValue type_val(cx, JS::StringValue(abort_type_atom));

  RootedObject event(cx, Event::create(cx, type_val, JS::NullHandleValue));
  RootedValue event_val(cx, JS::ObjectValue(*event));

  return EventTarget::dispatch_event(cx, self, event_val, &res_val);
}

// Set signal's abort reason to reason if it is given; otherwise to a new "AbortError" DOMException.
bool AbortSignal::set_reason(JSContext *cx, HandleObject self, HandleValue reason) {
  if (!reason.isUndefined()) {
    SetReservedSlot(self, Slots::Reason, reason);
  } else {
    RootedObject exception(cx, dom_exception::DOMException::create(cx, "AbortError", "AbortError"));
    if (exception == nullptr) {
      return false;
    }

    SetReservedSlot(self, Slots::Reason, JS::ObjectValue(*exception));
  }

  return true;
}

// https://dom.spec.whatwg.org/#interface-AbortSignal
JSObject *AbortSignal::create(JSContext *cx) {
  RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (self == nullptr) {
    return nullptr;
  }

  // An AbortSignal object has an associated abort reason, which is initially undefined.
  SetReservedSlot(self, Slots::Reason, JS::UndefinedValue());
  // An AbortSignal object has associated abort algorithms, which is initially empty.
  SetReservedSlot(self, Slots::Algorithms, JS::PrivateValue(new AlgorithmList));
  // An AbortSignal object has a dependent (a boolean), which is initially false.
  SetReservedSlot(self, Slots::Dependent, JS::FalseValue());
  // An AbortSignal object has associated source signals, which is initially empty.
  SetReservedSlot(self, Slots::SourceSignals, JS::PrivateValue(new WeakIndexSet));
  // An AbortSignal object has associated dependent signals, which is initially empty.
  SetReservedSlot(self, Slots::DependentSignals, JS::PrivateValue(new WeakIndexSet));
  // cache the onabort handler
  SetReservedSlot(self, Slots::OnAbort, JS::NullValue());

  if (!EventTarget::init(cx, self)) {
    return nullptr;
  }

  return self;
};

// https://dom.spec.whatwg.org/#dom-abortsignal-abort
//
// Returns an AbortSignal instance whose abort reason is set to reason if not undefined;
// otherwise to an "AbortError" DOMException.
JSObject *AbortSignal::create_with_reason(JSContext *cx, HandleValue reason) {
  // 1. Let signal be a new AbortSignal object.
  RootedObject self(cx, create(cx));
  if (self == nullptr) {
    return nullptr;
  }

  // 2. Set signal's abort reason to reason if it is given; otherwise to a new "AbortError"
  // DOMException.
  if (!set_reason(cx, self, reason)) {
    return nullptr;
  }
  // 3. Return signal.
  return self;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-timeout
//
// Returns an AbortSignal instance which will be aborted in milliseconds milliseconds.
// Its abort reason will be set to a "TimeoutError" DOMException.
JSObject *AbortSignal::create_with_timeout(JSContext *cx, HandleValue timeout) {
  // 1. Let signal be a new AbortSignal object.
  RootedObject self(cx, create(cx));
  if (self == nullptr) {
    return nullptr;
  }

  double ms = 0;
  if (!JS::ToNumber(cx, timeout, &ms)) {
    return nullptr;
  }

  // 2. Let global be signal's relevant global object.
  // 3. Run steps after a timeout given global, "AbortSignal-timeout", milliseconds, and the
  //  following step:
  //
  //  Queue a global task on the timer task source given global to signal abort given signal and
  //  a new "TimeoutError" DOMException. For the duration of this timeout, if signal has any
  //  event listeners registered for its abort event, there must be a strong reference from
  //  global to signal.
  int32_t timer_id = 0;

  JS::RootedObject exception(cx, dom_exception::DOMException::create(cx, "TimeoutError", "TimeoutError"));
  if (exception == nullptr) {
    return nullptr;
  }
  JS::RootedFunction on_timeout(cx, JS_NewFunction(cx, AbortSignal::on_timeout, 2, 0, nullptr));
  if (on_timeout == nullptr) {
    return nullptr;
  }

  JS::RootedValueVector args(cx);
  if (!args.append(JS::ObjectValue(*self)) || !args.append(JS::ObjectValue(*exception))) {
    return nullptr;
  }

  JS::RootedObject handler(cx, JS_GetFunctionObject(on_timeout));
  if (!timers::set_timeout(cx, handler, args, ms, &timer_id)) {
    return nullptr;
  }

  // 4. Return signal.
  return self;
}

// https://dom.spec.whatwg.org/#dom-abortsignal-any
//
// Returns an AbortSignal instance which will be aborted once any of signals is aborted.
// Its abort reason will be set to whichever one of signals caused it to be aborted.
JSObject *AbortSignal::create_with_signals(JSContext *cx, HandleValueArray signals) {
  // Method steps are to return the result of creating a dependent abort signal from signals using
  // AbortSignal and the current realm. https://dom.spec.whatwg.org/#create-a-dependent-abort-signal

  // 1. Let resultSignal be a new object implementing signalInterface using realm.
  RootedObject self(cx, create(cx));
  if (self == nullptr) {
    return nullptr;
  }

  // 2. For each signal of signals: if signal is aborted, then set resultSignal's abort reason to
  // signal's abort reason and return resultSignal.
  for (size_t i = 0; i < signals.length(); ++i) {
    RootedObject signal(cx, &signals[i].toObject());

    if (is_aborted(signal)) {
      SetReservedSlot(self, Slots::Reason, reason(signal));
      return self;
    }
  }

  // 3. Set resultSignal's dependent to true.
  SetReservedSlot(self, Slots::Dependent, JS::TrueValue());
  auto *our_signals = source_signals(self);

  // 4. For each signal of signals:
  for (size_t i = 0; i < signals.length(); ++i) {
    RootedObject signal(cx, &signals[i].toObject());

    // 1. If signal's dependent is false:
    if (!is_dependent(signal)) {
      // 1. Append signal to resultSignal's source signals.
      our_signals->insert(signal);
      // 2. Append resultSignal to signal's dependent signals.
      auto *their_signals = dependent_signals(signal);
      their_signals->insert(self);
    }
    // 2. Otherwise...
    else {
      auto *src_signals = source_signals(signal);
      // for each sourceSignal of signal's source signals:
      for (auto const &source : src_signals->items()) {
        // 1. Assert: sourceSignal is not aborted and not dependent.
        MOZ_ASSERT(!is_aborted(source) && !is_dependent(source));
        // 2. Append sourceSignal to resultSignal's source signals.
        our_signals->insert(source);
        // 3. Append resultSignal to sourceSignal's dependent signals.
        auto *their_signals = dependent_signals(source);
        their_signals->insert(self);
      };
    }
  }

  // 5. Return resultSignal.
  return self;
}

bool AbortSignal::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  return api::throw_error(cx, api::Errors::NoCtorBuiltin, class_name);
}

void AbortSignal::finalize(JS::GCContext *gcx, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  EventTarget::finalize(gcx, self);
}

void AbortSignal::trace(JSTracer *trc, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  EventTarget::trace(trc, self);

  auto has_sources = !JS::GetReservedSlot(self, Slots::SourceSignals).isNullOrUndefined();
  if (has_sources) {
    auto *srcsig = source_signals(self);
    srcsig->trace(trc);
    srcsig->traceWeak(trc);
  }

  auto has_deps = !JS::GetReservedSlot(self, Slots::DependentSignals).isNullOrUndefined();
  if (has_deps) {
    auto *depsig = dependent_signals(self);
    depsig->trace(trc);
    depsig->traceWeak(trc);
  }

  auto has_algorithms = !JS::GetReservedSlot(self, Slots::Algorithms).isNullOrUndefined();
  if (has_algorithms) {
    auto *algorithms = AbortSignal::algorithms(self);
    algorithms->trace(trc);
  }
}

bool AbortSignal::init_class(JSContext *cx, JS::HandleObject global) {
  EventTarget::register_subclass(&class_);

  if (!init_class_impl(cx, global, EventTarget::proto_obj)) {
    return false;
  }

  if ((abort_type_atom = JS_AtomizeAndPinString(cx, "abort")) == nullptr) {
    return false;
  }

  return true;
}

bool install(api::Engine *engine) {
  if (!AbortSignal::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!AbortController::init_class(engine->cx(), engine->global())) {
    return false;
  }

  return true;
}

JSString *AbortSignal::abort_type_atom = nullptr;

} // namespace builtins::web::abort


