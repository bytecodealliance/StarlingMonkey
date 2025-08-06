#include "event-target.h"
#include "encode.h"
#include "event.h"

#include "../dom-exception.h"
#include "../abort/abort-signal.h"

#include "js/GCPolicyAPI.h"
#include "mozilla/Assertions.h"

namespace {

using builtins::web::abort::AbortSignal;

// https://dom.spec.whatwg.org/#concept-flatten-options
bool flatten_opts(JSContext *cx, JS::HandleValue opts, bool *rval) {
  // To flatten options, run these steps:
  //  - If options is a boolean, then return options.
  if (opts.isBoolean()) {
    *rval = opts.toBoolean();
    return true;
  }

  // Otherwise:
  // - Return options["capture"].
  if (opts.isObject()) {
    JS::RootedObject obj(cx, &opts.toObject());
    JS::RootedValue val(cx);

    if (!JS_GetProperty(cx, obj, "capture", &val)) {
      return false;
    }

    *rval = JS::ToBoolean(val);
  }

  return true;
}

// https://dom.spec.whatwg.org/#event-flatten-more
bool flatten_more_opts(JSContext *cx, JS::HandleValue opts, bool *capture, bool *once,
                       JS::MutableHandleValue passive, JS::MutableHandleValue signal) {
  // To flatten more options, run these steps:
  // - Let capture be the result of flattening options.
  *capture = false;
  if (!flatten_opts(cx, opts, capture)) {
    return false;
  }

  // - Let once be false.
  *once = false;
  // - Let passive and signal be null.
  passive.setNull();
  signal.setNull();

  if (!opts.isObject()) {
    return true;
  }

  // - If options is a dictionary:
  JS::RootedObject obj(cx, &opts.toObject());
  JS::RootedValue val(cx);

  // - Set once to options["once"].
  if (!JS_GetProperty(cx, obj, "once", &val)) {
    return false;
  }
  *once = JS::ToBoolean(val);

  // - If options["passive"] exists, then set passive to options["passive"].
  if (!JS_GetProperty(cx, obj, "passive", &val)) {
    return false;
  }

  if (!val.isUndefined()) {
    passive.setBoolean(JS::ToBoolean(val));
  }

  // - If options["signal"] exists, then set signal to options["signal"].
  if (!JS_GetProperty(cx, obj, "signal", &val)) {
    return false;
  }
  if (val.isObject() && AbortSignal::is_instance(val)) {
    signal.set(val);
  }

  // - Return capture, passive, once, and signal.
  return true;
}

// https://dom.spec.whatwg.org/#default-passive-value
bool default_passive_value() {
  // Return true if all of the following are true:
  // - type is one of "touchstart", "touchmove", "wheel", or "mousewheel". [TOUCH-EVENTS]
  // [UIEVENTS]
  // - eventTarget is a Window object, or is a node whose node document is eventTarget, or is a
  // node
  //  whose node document's document element is eventTarget, or is a node whose node document's
  //  body element is eventTarget. [HTML]
  // Return false.
  return false;
}

} // namespace

namespace JS {

template <typename T> struct GCPolicy<RefPtr<T>> {
  static void trace(JSTracer *trc, RefPtr<T> *tp, const char *name) {
    if (T *target = tp->get()) {
      GCPolicy<T>::trace(trc, target, name);
    }
  }
  static bool needsSweep(RefPtr<T> *tp) {
    if (T *target = tp->get()) {
      return GCPolicy<T>::needsSweep(target);
    }
    return false;
  }
  static bool isValid(const RefPtr<T> &t) {
    if (T *target = t.get()) {
      return GCPolicy<T>::isValid(*target);
    }
    return true;
  }
};

} // namespace JS

namespace builtins {
namespace web {
namespace event {

using EventFlag = Event::EventFlag;
using dom_exception::DOMException;
using abort::AbortSignal;
using abort::AbortAlgorithm;

struct Terminator : AbortAlgorithm {
  Heap<JSObject *> target;
  Heap<Value> type;
  Heap<Value> callback;
  Heap<Value> opts;

  Terminator(JSContext *cx, HandleObject target, HandleValue type, HandleValue callback, HandleValue opts)
      : target(target), type(type), callback(callback), opts(opts) {}

  bool run(JSContext *cx) override {
    RootedObject self(cx, target);
    RootedValue type_val(cx, type);
    RootedValue callback_val(cx, callback);
    RootedValue opts_val(cx, opts);

    return EventTarget::remove_listener(cx, self, type_val, callback_val, opts_val);
  }

  void trace(JSTracer *trc) override {
    JS::TraceEdge(trc, &target, "EventTarget Terminator target");
    JS::TraceEdge(trc, &type, "EventTarget Terminator type");
    JS::TraceEdge(trc, &callback, "EventTarget Terminator callback");
    JS::TraceEdge(trc, &opts, "EventTarget Terminator opts");
  }
};

const JSFunctionSpec EventTarget::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec EventTarget::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec EventTarget::methods[] = {
    JS_FN("addEventListener", EventTarget::addEventListener, 0, JSPROP_ENUMERATE),
    JS_FN("removeEventListener", EventTarget::removeEventListener, 0, JSPROP_ENUMERATE),
    JS_FN("dispatchEvent", EventTarget::dispatchEvent, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec EventTarget::properties[] = {
    JS_PS_END,
};

EventTarget::ListenerList *EventTarget::listeners(JSObject *self) {
  MOZ_ASSERT(is_instance(self));

  auto list = static_cast<ListenerList *>(
      JS::GetReservedSlot(self, static_cast<size_t>(EventTarget::Slots::Listeners)).toPrivate());

  MOZ_ASSERT(list);
  return list;
}

bool EventTarget::addEventListener(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2);

  RootedValue type(cx, args.get(0));
  RootedValue callback(cx, args.get(1));
  RootedValue opts(cx, args.get(2));

  args.rval().setUndefined();
  return add_listener(cx, self, type, callback, opts);
}

bool EventTarget::removeEventListener(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2);

  RootedValue type(cx, args.get(0));
  RootedValue callback(cx, args.get(1));
  RootedValue opts(cx, args.get(2));

  args.rval().setUndefined();
  return remove_listener(cx, self, type, callback, opts);
}

bool EventTarget::dispatchEvent(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);

  RootedValue event(cx, args.get(0));
  return dispatch_event(cx, self, event, args.rval());
}

// https://dom.spec.whatwg.org/#add-an-event-listener
bool EventTarget::add_listener(JSContext *cx, HandleObject self, HandleValue type_val,
                               HandleValue callback_val, HandleValue opts_val) {
  MOZ_ASSERT(is_instance(self));

  // 1. Let capture, passive, once, and signal be the result of flattening more options.
  bool capture = false, once = false, passive = false;
  RootedValue passive_val(cx);
  RootedValue signal_val(cx);

  if (!flatten_more_opts(cx, opts_val, &capture, &once, &passive_val, &signal_val)) {
    return false;
  }

  // 2. Add an event listener with this and an event listener whose type is type,
  // callback is callback, capture is capture, passive is passive, once is once, and signal is
  // signal.

  // - If eventTarget is a ServiceWorkerGlobalScope object, its service worker's script resource's
  // has
  //  ever been evaluated flag is set, and listener's type matches the type attribute value of any
  //  of the service worker events, then report a warning to the console that this might not give
  //  the expected results.
  //  N/A

  // - If listener's signal is not null and is aborted, then return.
  if (signal_val.isObject() && AbortSignal::is_instance(signal_val)) {
    RootedObject signal(cx, &signal_val.toObject());
    if (AbortSignal::is_aborted(signal)) {
      return true;
    }
  }

  // - If listener's callback is null, then return.
  if (callback_val.isNullOrUndefined()) {
    return true;
  }

  if (!callback_val.isObject()) {
    return api::throw_error(cx, api::Errors::TypeError, "addEventListener", "callback",
                            "be an object");
  }

  // - If listener's passive is null, then set it to the default passive value given listener's type
  // and eventTarget.
  passive = passive_val.isNullOrUndefined() ? default_passive_value() : passive_val.toBoolean();

  // - If eventTarget's event listener list does not contain an event listener whose type is
  // listener's
  //  type, callback is listener's callback, and capture is listener's capture, then append listener
  //  to eventTarget's event listener list.
  auto encoded = core::encode(cx, type_val);
  if (!encoded) {
    return false;
  }

  auto type = std::string_view(encoded);
  auto list = listeners(self);

  auto it = std::find_if(list->begin(), list->end(), [&](const auto &listener) {
    return type == listener->type && callback_val == listener->callback.get() &&
           capture == listener->capture;
  });

  if (it == list->end()) {
    auto listener = mozilla::MakeRefPtr<EventListener>();
    listener->callback = callback_val;
    listener->signal = signal_val;
    listener->type = type;
    listener->passive = passive;
    listener->capture = capture;
    listener->once = once;
    listener->removed = false;

    list->append(listener);
  } else if((*it)->removed) {
    // if existing listener was marked for removal, then move it to the end of the list
    // and update its removed flag. This is done to ensure the order of listeners. We only
    // update listener's properties that are not check for listener equality.
    (*it)->signal = signal_val;
    (*it)->passive = passive;
    (*it)->once = once;
    (*it)->removed = false;

    list->erase(it);
    list->append(*it);
  }

  // - If listener's signal is not null, then add the following abort steps to it:
  //  Remove an event listener with eventTarget and listener.
  if (signal_val.isObject() && AbortSignal::is_instance(signal_val)) {
    auto terminator = js::MakeUnique<Terminator>(cx, self, type_val, callback_val, opts_val);
    RootedObject signal(cx, &signal_val.toObject());
    AbortSignal::add_algorithm(signal, std::move(terminator));
  }

  return true;
}

// https://dom.spec.whatwg.org/#dom-eventtarget-removeeventlistener
bool EventTarget::remove_listener(JSContext *cx, HandleObject self, HandleValue type_val,
                                  HandleValue callback_val, HandleValue opts_val) {
  MOZ_ASSERT(is_instance(self));

  bool capture = false;
  if (!flatten_opts(cx, opts_val, &capture)) {
    return false;
  }

  auto encoded = core::encode(cx, type_val);
  if (!encoded) {
    return false;
  }

  auto type = std::string_view(encoded);
  auto list = listeners(self);

  auto it = std::find_if(list->begin(), list->end(), [&](const auto &listener) {
    return type == listener->type && callback_val == listener->callback.get() &&
           capture == listener->capture;
  });

  if (it != list->end()) {
    (*it)->removed = true;
    list->erase(it);
  }

  return true;
}

// https://dom.spec.whatwg.org/#dom-eventtarget-dispatchevent
bool EventTarget::dispatch_event(JSContext *cx, HandleObject self, HandleValue event_val,
                                 MutableHandleValue rval) {
  MOZ_ASSERT(is_instance(self));

  if (!Event::is_instance(event_val)) {
    return api::throw_error(cx, api::Errors::TypeError, "EventTarget.dispatch", "event",
                            "be an Event");
  }

  RootedObject event(cx, &event_val.toObject());

  // 1. If event's dispatch flag is set, or if its initialized flag is not set,
  //  then throw an "InvalidStateError" DOMException.
  if (Event::has_flag(event, EventFlag::Dispatch) ||
      !Event::has_flag(event, EventFlag::Initialized)) {
    return DOMException::raise(cx, "EventTarget#dispatchEvent invalid Event state",
                               "InvalidStateError");
  }

  // 2. Initialize event's isTrusted attribute to false.
  Event::set_flag(event, EventFlag::Trusted, false);

  // 3. Return the result of dispatching event to this.
  if (!dispatch(cx, self, event, nullptr, rval)) {
    return false;
  }

  return true;
}

// https://dom.spec.whatwg.org/#concept-event-dispatch
//
// StarlingMonkey currently doesn't support Node objects (i.e. every check with `isNode()` returns
// false), which means we don't need to build a full event propagation path that walks parent nodes,
// deals with shadow DOM retargeting, or handles activation behaviors. In a simplified version we
// assume that the event only ever targets the object on which it was dispatched.
bool EventTarget::dispatch(JSContext *cx, HandleObject self, HandleObject event,
                           HandleObject target_override, MutableHandleValue rval) {
  // 1. Set event's dispatch flag.
  Event::set_flag(event, EventFlag::Dispatch, true);
  // 2. Let targetOverride be target, if legacy target override flag is not given, and target's
  // associated Document otherwise.
  RootedObject target(cx, target_override ? target_override : self);
  // 3. Let activationTarget be null.
  //  N/A
  // 4. Let relatedTarget be the result of retargeting event's relatedTarget against target.
  //  N/A
  //  Retargeting will always result in related_target being the target if Node is not defined:
  //  https://dom.spec.whatwg.org/#retarget
  // 5. Let clearTargets be false.
  //  N/A
  // 6. If target is not relatedTarget or target is event's relatedTarget
  // In simplified version this is always true, because the result of retargeting self
  // against event's related target always returns self. This means that all the substeps
  // within step 6 of this algorithm effectively implement the same functionality as the
  // `invoke_listeners` function.
  if (!invoke_listeners(cx, target, event)) {
    return false;
  }

  // 7. Set event's eventPhase attribute to NONE.
  Event::set_phase(event, Event::Phase::NONE);
  // 8. Set event's currentTarget attribute to null.
  Event::set_current_target(event, nullptr);
  // 9. Set event's path to the empty list.
  //  - Implicitly done...
  // 10. Unset event's dispatch flag, stop propagation flag, and stop immediate propagation flag.
  Event::set_flag(event, EventFlag::Dispatch, false);
  Event::set_flag(event, EventFlag::StopPropagation, false);
  Event::set_flag(event, EventFlag::StopImmediatePropagation, false);

  // 11. If clearTargets is true:
  Event::set_related_target(event, nullptr);
  // 12. If activationTarget is non-null:
  // N/A
  // 13. Return false if event's canceled flag is set; otherwise true.
  rval.setBoolean(!Event::has_flag(event, EventFlag::Canceled));
  return true;
}

// https://dom.spec.whatwg.org/#concept-event-listener-invoke
bool EventTarget::invoke_listeners(JSContext *cx, HandleObject target, HandleObject event) {
  MOZ_ASSERT(is_instance(target));

  // 1. Set event's target to the shadow-adjusted target of the last struct in event's path,
  //   that is either struct or preceding struct, whose shadow-adjusted target is non-null.
  Event::set_phase(event, Event::Phase::AT_TARGET);
  Event::set_target(event, target);
  // 2. Set event's relatedTarget to struct's relatedTarget.
  Event::set_related_target(event, target);
  // 3. Set event's touch target list to struct's touch target list.
  // We only use a single target here as it would appear in a Even#path[0];
  // - shadow adjusted target == target
  // - relatedTarget == target

  // 4. If event's stop propagation flag is set, then return.
  if (Event::has_flag(event, EventFlag::StopPropagation)) {
    return true;
  }

  // 5. Initialize event's currentTarget attribute to struct's invocation target.
  Event::set_current_target(event, target);
  // 6. Let listeners be a clone of event's currentTarget attribute value's event listener list.
  auto list = listeners(target);
  JS::RootedVector<ListenerRef> list_clone(cx);
  if (!list_clone.reserve(list->length())) {
    return false;
  }

  for (auto &listener : *list) {
    list_clone.infallibleAppend(listener);
  }

  // 7. Let invocationTargetInShadowTree be struct's invocation-target-in-shadow-tree.
  // N/A
  // 8. Let found be the result of running inner invoke with event, listeners, phase,
  auto found = false;
  if (!inner_invoke(cx, event, list_clone, &found)) {
    return false;
  }

  // 9. If found is false and event's isTrusted attribute is true:
  // N/A
  return true;
}

// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
bool EventTarget::inner_invoke(JSContext *cx, HandleObject event,
                               JS::HandleVector<ListenerRef> list, bool *found) {
  RootedString type_str(cx, Event::type(event));
  auto event_type = core::encode(cx, type_str);
  if (!event_type) {
    return false;
  }

  auto type_sv = std::string_view(event_type);

  // 1. Let found be false.
  *found = false;

  bool listeners_removed = false;

  // 2. For each listener of listeners, whose removed is false:
  for (auto &listener : list) {
    if (listener->removed) {
      continue;
    }

    // 1. If event's type attribute value is not listener's type, then continue.
    if (listener->type != type_sv) {
      continue;
    }

    // 2. Set found to true.
    *found = true;

    // 3. If phase is "capturing" and listener's capture is false, then continue.
    // 4. If phase is "bubbling" and listener's capture is true, then continue.
    // N/A

    // 5. If listener's once is true, then remove an event listener given event's
    //  currentTarget attribute value and listener.
    if (listener->once) {
      // Removing the listener from the list is deferred until the end of the loop.
      listener->removed = true;
      listeners_removed = true;
    }

    // 6. Let global be listener callback's associated realm's global object.
    // 7. Let currentEvent be undefined.
    // 8. If global is a Window object:
    //  1. Set currentEvent to global's current event.
    //  2. If invocationTargetInShadowTree is false, then set global's current event to event.
    //  N/A
    // 9. If listener's passive is true, then set event's in passive listener flag.
    if (listener->passive) {
      Event::set_flag(event, EventFlag::InPassiveListener, true);
    }

    // 10. If global is a Window object, then record timing info for event listener given event and
    // listener. N/A
    //

    // 11. Call a user object's operation with listener's callback, "handleEvent", event,
    // and event's currentTarget attribute value.
    auto engine = api::Engine::get(cx);
    RootedValue callback_val(cx, listener->callback);
    RootedObject callback_obj(cx, &callback_val.toObject());

    RootedValue rval(cx);
    RootedValueArray<1> args(cx);
    args[0].setObject(*event);

    if (JS::IsCallable(callback_obj)) {
      auto global = engine->global();
      JS::Call(cx, global, callback_val, args, &rval);
    } else {
      RootedValue handle_fn(cx);
      if (!JS_GetProperty(cx, callback_obj, "handleEvent", &handle_fn)) {
        return false;
      }
      JS::Call(cx, callback_val, handle_fn, args, &rval);
    }

    if (JS_IsExceptionPending(cx)) {
      // TODO: report an exception as in spec:
      // https://html.spec.whatwg.org/multipage/webappapis.html#report-an-exception
      // https://github.com/bytecodealliance/StarlingMonkey/issues/239
      auto msg = "Exception in event listener for " + listener->type;
      engine->dump_pending_exception(msg.c_str());
      JS_ClearPendingException(cx);
    }

    // 12. Unset event's in passive listener flag.
    Event::set_flag(event, Event::EventFlag::InPassiveListener, false);
    // 13. If global is a Window object, then set global's current event to currentEvent.
    // N/A

    // 14. If event's stop immediate propagation flag is set, then break.
    if (Event::has_flag(event, EventFlag::StopImmediatePropagation)) {
      return true;
    }
  }

  if (listeners_removed) {
    auto current_target = Event::current_target(event);
    MOZ_ASSERT(is_instance(current_target));

    auto target_list = listeners(current_target);
    MOZ_ASSERT(target_list);
    target_list->eraseIf([](const auto &listener) { return listener->removed; });
  }

  return true;
}

JSObject *EventTarget::create(JSContext *cx) {
  JSObject *self = JS_NewObjectWithGivenProto(cx, &class_, proto_obj);
  if (!self) {
    return nullptr;
  }

  SetReservedSlot(self, Slots::Listeners, JS::PrivateValue(new ListenerList));
  return self;
}

bool EventTarget::init(JSContext *cx, HandleObject self) {
  SetReservedSlot(self, Slots::Listeners, JS::PrivateValue(new ListenerList));
  return true;
}

bool EventTarget::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("EventTarget", 0);

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }

  SetReservedSlot(self, Slots::Listeners, JS::PrivateValue(new ListenerList));

  args.rval().setObject(*self);
  return true;
}

void EventTarget::finalize(JS::GCContext *gcx, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto list = listeners(self);
  if (list) {
    delete list;
  }
}

void EventTarget::trace(JSTracer *trc, JSObject *self) {
  MOZ_ASSERT(is_instance(self));

  const JS::Value val = JS::GetReservedSlot(self, static_cast<size_t>(Slots::Listeners));
  if (val.isNullOrUndefined()) {
    // Nothing to trace
    return;
  }

  auto list = listeners(self);
  list->trace(trc);
}

bool EventTarget::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

} // namespace event
} // namespace web
} // namespace builtins
