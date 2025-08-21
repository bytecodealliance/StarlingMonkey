#include "event.h"
#include "event-target.h"
#include "custom-event.h"
#include "global-event-target.h"

namespace {

using builtins::web::event::Event;

void set_event_flag(uint32_t *flags, Event::EventFlag flag, bool val) {
  if (val) {
    *flags |= static_cast<uint32_t>(flag);
  } else {
    *flags &= ~(static_cast<uint32_t>(flag));
  }
}

bool read_event_init(JSContext *cx, HandleValue initv,
                     bool *bubbles, bool *cancelable, bool *composed) {
  // Type checked upstream
  MOZ_ASSERT(initv.isObject());

  JS::RootedObject obj(cx, &initv.toObject());
  JS::RootedValue val(cx);

  if (!JS_GetProperty(cx, obj, "bubbles", &val)) {
    return false;
  }
  *bubbles = JS::ToBoolean(val);

  if (!JS_GetProperty(cx, obj, "cancelable", &val)) {
    return false;
  }
  *cancelable = JS::ToBoolean(val);

  if (!JS_GetProperty(cx, obj, "composed", &val)) {
    return false;
  }
  *composed = JS::ToBoolean(val);

  return true;
}

} // namespace



namespace builtins::web::event {

const JSFunctionSpec Event::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Event::static_properties[] = {
    JS_INT32_PS("NONE", static_cast<uint32_t>(Phase::NONE), JSPROP_ENUMERATE),
    JS_INT32_PS("CAPTURING_PHASE", static_cast<uint32_t>(Phase::CAPTURING_PHASE), JSPROP_ENUMERATE),
    JS_INT32_PS("AT_TARGET", static_cast<uint32_t>(Phase::AT_TARGET), JSPROP_ENUMERATE),
    JS_INT32_PS("BUBBLING_PHASE", static_cast<uint32_t>(Phase::BUBBLING_PHASE), JSPROP_ENUMERATE),
    JS_PS_END,
};

const JSFunctionSpec Event::methods[] = {
    JS_FN("stopPropagation", Event::stopPropagation, 0, JSPROP_ENUMERATE),
    JS_FN("stopImmediatePropagation", Event::stopImmediatePropagation, 0, JSPROP_ENUMERATE),
    JS_FN("preventDefault", Event::preventDefault, 0, JSPROP_ENUMERATE),
    JS_FN("composedPath", Event::composedPath, 0, JSPROP_ENUMERATE),
    JS_FN("initEvent", Event::initEvent, 3, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec Event::properties[] = {
    JS_PSG("type", Event::type_get, JSPROP_ENUMERATE),
    JS_PSG("target", Event::target_get, JSPROP_ENUMERATE),
    JS_PSG("currentTarget", Event::currentTarget_get, JSPROP_ENUMERATE),
    JS_PSG("srcElement", Event::srcElement_get, JSPROP_ENUMERATE),
    JS_PSG("eventPhase", Event::eventPhase_get, JSPROP_ENUMERATE),
    JS_PSG("bubbles", Event::bubbles_get, JSPROP_ENUMERATE),
    JS_PSG("cancelable", Event::cancelable_get, JSPROP_ENUMERATE),
    JS_PSG("defaultPrevented", Event::defaultPrevented_get, JSPROP_ENUMERATE),
    JS_PSG("composed", Event::composed_get, JSPROP_ENUMERATE),
    JS_PSG("isTrusted", Event::isTrusted_get, JSPROP_ENUMERATE),
    JS_PSG("timeStamp", Event::timeStamp_get, JSPROP_ENUMERATE),
    JS_PSGS("returnValue", Event::returnValue_get, Event::returnValue_set, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Event", JSPROP_READONLY),
    JS_PS_END,
};

#define DEFINE_EVENT_GETTER(fn, setter, expr, ...)                 \
bool Event::fn(JSContext *cx, unsigned argc, JS::Value *vp) {      \
  METHOD_HEADER(0);                                                \
  args.rval().setter(expr(self __VA_OPT__(,) __VA_ARGS__));        \
  return true;                                                     \
}

DEFINE_EVENT_GETTER(type_get, setString, type)
DEFINE_EVENT_GETTER(target_get, setObjectOrNull, target)
DEFINE_EVENT_GETTER(srcElement_get, setObjectOrNull, target)
DEFINE_EVENT_GETTER(currentTarget_get, setObjectOrNull, current_target)
DEFINE_EVENT_GETTER(timeStamp_get, setNumber, timestamp)
DEFINE_EVENT_GETTER(eventPhase_get, setInt32, (uint32_t)phase)

DEFINE_EVENT_GETTER(bubbles_get, setBoolean, has_flag, EventFlag::Bubbles)
DEFINE_EVENT_GETTER(cancelable_get, setBoolean, has_flag, EventFlag::Cancelable)
DEFINE_EVENT_GETTER(defaultPrevented_get, setBoolean, has_flag, EventFlag::Canceled)
DEFINE_EVENT_GETTER(returnValue_get, setBoolean, !has_flag, EventFlag::Canceled)
DEFINE_EVENT_GETTER(composed_get, setBoolean, has_flag, EventFlag::Composed)
DEFINE_EVENT_GETTER(isTrusted_get, setBoolean, has_flag, EventFlag::Trusted)

bool Event::stopPropagation(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  set_flag(self, EventFlag::StopPropagation, true);
  return true;
}

bool Event::stopImmediatePropagation(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  set_flag(self, EventFlag::StopImmediatePropagation, true);
  return true;
}

bool Event::preventDefault(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  set_canceled(self, true);
  return true;
}

bool Event::returnValue_set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);
  RootedValue val(cx, args.get(0));

  // setter steps are to set the canceled flag with this
  // if the given value is false; otherwise do nothing.
  if (!JS::ToBoolean(val)) {
    set_canceled(self, true);
  }

  return true;
}

bool Event::composedPath(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  RootedObject tgt(cx, current_target(self));
  RootedObject paths(cx);

  if (tgt) {
    RootedValueArray<1> arr(cx);
    arr[0].setObject(*tgt);
    paths = JS::NewArrayObject(cx, arr);
  } else {
    paths = JS::NewArrayObject(cx, 0);
  }

  if (!paths) {
    return false;
  }

  args.rval().setObject(*paths);
  return true;
}

JSString *Event::type(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Type).toString();
}

JSObject *Event::target(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Target).toObjectOrNull();
}

JSObject *Event::current_target(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::CurrentTarget).toObjectOrNull();
}

JSObject *Event::related_target(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::RelatedTarget).toObjectOrNull();
}

bool Event::has_flag(JSObject* self, EventFlag flag) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return (flags & static_cast<uint32_t>(flag)) != 0;
}

bool Event::set_flag(JSObject* self, EventFlag flag, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, flag, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
  return true;
}

Event::Phase Event::phase(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<Phase>(JS::GetReservedSlot(self, Slots::EvtPhase).toInt32());
}

double Event::timestamp(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::TimeStamp).toNumber();
}

void Event::set_phase(JSObject *self, Event::Phase phase) {
  MOZ_ASSERT(is_instance(self));
  SetReservedSlot(self, Slots::EvtPhase, JS::Int32Value(static_cast<uint32_t>(phase)));
}

void Event::set_target(JSObject *self, HandleObject target) {
  MOZ_ASSERT(is_instance(self));
  SetReservedSlot(self, Slots::Target, JS::ObjectOrNullValue(target));
}

void Event::set_current_target(JSObject *self, HandleObject target) {
  MOZ_ASSERT(is_instance(self));
  SetReservedSlot(self, Slots::CurrentTarget, JS::ObjectOrNullValue(target));
}

void Event::set_related_target(JSObject *self, HandleObject target) {
  MOZ_ASSERT(is_instance(self));
  SetReservedSlot(self, Slots::RelatedTarget, JS::ObjectOrNullValue(target));
}

void Event::set_canceled(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));

  // https://dom.spec.whatwg.org/#set-the-canceled-flag
  // To set the canceled flag, given an event event, if event's cancelable
  // attribute value is true and event's in passive listener flag is unset,
  // then set event's canceled flag, and do nothing otherwise.
  auto canceled = val
    && has_flag(self, EventFlag::Cancelable)
    && !has_flag(self, EventFlag::InPassiveListener);

  set_flag(self, EventFlag::Canceled, canceled);
}

// https://dom.spec.whatwg.org/#inner-event-creation-steps
bool Event::init(JSContext *cx, HandleObject self, HandleValue type, HandleValue init)
{
  auto *type_str = JS::ToString(cx, type);
  if (!type_str) {
    return false;
  }

  auto bubbles = false;
  auto composed = false;
  auto cancelable = false;

  if (init.isObject()) {
    read_event_init(cx, init, &bubbles, &cancelable, &composed);
  }

  // To initialize an event, with type, bubbles, and cancelable, run these steps:
  // - Set event's initialized flag.
  // - Unset event's stop propagation flag, stop immediate propagation flag, and canceled flag.
  auto flags = static_cast<uint32_t>(EventFlag::Initialized);
  set_event_flag(&flags, EventFlag::Bubbles, bubbles);
  set_event_flag(&flags, EventFlag::Composed, composed);
  set_event_flag(&flags, EventFlag::Cancelable, cancelable);

  // Set event's isTrusted attribute to false.
  // Set event's target to null.
  // Set event's type attribute to type.
  // Set event's bubbles attribute to bubbles.
  // Set event's cancelable attribute to cancelable.
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
  SetReservedSlot(self, Slots::Target, JS::NullValue());
  SetReservedSlot(self, Slots::CurrentTarget, JS::NullValue());
  SetReservedSlot(self, Slots::RelatedTarget, JS::NullValue());
  SetReservedSlot(self, Slots::Type, JS::StringValue(type_str));
  SetReservedSlot(self, Slots::TimeStamp, JS::NumberValue(1.0)); // TODO: implement timestamp
  SetReservedSlot(self, Slots::EvtPhase, JS::Int32Value(static_cast<uint32_t>(Event::Phase::NONE)));

  return true;
}

JSObject *Event::create(JSContext *cx, HandleValue type, HandleValue init) {
  RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self) {
    return nullptr;
  }

  if (!Event::init(cx, self, type, init)) {
    return nullptr;
  }

  return self;
}

// https://dom.spec.whatwg.org/#interface-event
bool Event::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("Event", 1);

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }

  RootedValue type(cx, args.get(0));
  RootedValue init(cx, args.get(1));

  if (!Event::init(cx, self, type, init)) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

// https://dom.spec.whatwg.org/#dom-event-initevent
bool Event::initEvent(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);

  // The initEvent(type, bubbles, cancelable) method steps are:
  // 1. If this's dispatch flag is set, then return.
  if (Event::has_flag(self, EventFlag::Dispatch)) {
    return true;
  }

  // 2. Initialize this with type, bubbles, and cancelable.
  RootedValue type(cx, args.get(0));
  RootedValue init(cx, args.get(1));

  return Event::init(cx, self, type, init);
}

bool Event::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

bool install(api::Engine *engine) {
  if (!Event::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!EventTarget::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!CustomEvent::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!global_event_target_init(engine->cx(), engine->global())) {
    return false;
  }

  return true;
}

} // namespace builtins::web::event


