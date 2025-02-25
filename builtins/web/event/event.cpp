#include "event.h"
#include "event-target.h"

namespace {

// Each event has the following associated flags that are all initially unset:
// - stop propagation flag
// - stop immediate propagation flag
// - canceled flag
// - in passive listener flag
// - composed flag
// - initialized flag
// - dispatch flag
// clang-format off
enum class EventFlag : uint32_t {
  StopPropagation          = 1 << 0,
  StopImmediatePropagation = 1 << 1,
  Canceled                 = 1 << 2,
  InPassiveListener        = 1 << 3,
  Composed                 = 1 << 4,
  Initialized              = 1 << 5,
  Dispatch                 = 1 << 6,
};
// clang-format on

enum class Bubbles: bool { No, Yes };
enum class Composed: bool { No, Yes };
enum class Cancelable: bool { No, Yes };

void set_event_flag(uint32_t *flags, EventFlag flag) {
  *flags |= static_cast<uint32_t>(flag);
}

void clear_event_flag(uint32_t *flags, EventFlag flag) {
  *flags &= ~(static_cast<uint32_t>(flag));
}

void set_event_flag(uint32_t *flags, EventFlag flag, bool set) {
  set ? set_event_flag(flags, flag) : clear_event_flag(flags, flag);
}

bool test_event_flag(uint32_t flags, EventFlag flag) {
  return (flags & static_cast<uint32_t>(flag)) != 0;
}

bool read_event_init(JSContext *cx, HandleValue initv,
                     Bubbles *bubbles, Cancelable *cancelable, Composed *composed) {
  // Type checked upstream
  MOZ_ASSERT(initv.isObject());

  JS::RootedObject obj(cx, &initv.toObject());
  JS::RootedValue val(cx);

  if (!JS_GetProperty(cx, obj, "bubbles", &val)) {
    return false;
  }
  if (val.isBoolean()) {
    *bubbles = static_cast<Bubbles>(val.toBoolean());
  }

  if (!JS_GetProperty(cx, obj, "cancelable", &val)) {
    return false;
  }
  if (val.isBoolean()) {
    *cancelable = static_cast<Cancelable>(val.toBoolean());
  }

  if (!JS_GetProperty(cx, obj, "composed", &val)) {
    return false;
  }
  if (val.isBoolean()) {
    *composed = static_cast<Composed>(val.toBoolean());
  }

  return true;
}

} // namespace

namespace builtins {
namespace web {
namespace event {

const JSFunctionSpec Event::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Event::static_properties[] = {
    JS_PSG("NONE", Event::NONE_get, JSPROP_ENUMERATE),
    JS_PSG("CAPTURING_PHASE", Event::CAPTURING_PHASE_get, JSPROP_ENUMERATE),
    JS_PSG("AT_TARGET", Event::AT_TARGET_get, JSPROP_ENUMERATE),
    JS_PSG("BUBBLING_PHASE", Event::BUBBLING_PHASE_get, JSPROP_ENUMERATE),
    JS_PS_END,
};

const JSFunctionSpec Event::methods[] = {
    JS_FN("stopPropagation", Event::stopPropagation, 0, JSPROP_ENUMERATE),
    JS_FN("stopImmediatePropagation", Event::stopImmediatePropagation, 0, JSPROP_ENUMERATE),
    JS_FN("preventDefault", Event::preventDefault, 0, JSPROP_ENUMERATE),
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
    JS_PSG("returnValue", Event::returnValue_get, JSPROP_ENUMERATE),
    JS_PSG("defaultPrevented", Event::defaultPrevented_get, JSPROP_ENUMERATE),
    JS_PSG("composed", Event::composed_get, JSPROP_ENUMERATE),
    JS_PSG("isTrusted", Event::isTrusted_get, JSPROP_ENUMERATE),
    JS_PSG("timeStamp", Event::timeStamp_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Event", JSPROP_READONLY),
    JS_PS_END,
};

#define DEFINE_EVENT_GETTER(fn, setter, expr)                 \
bool Event::fn(JSContext *cx, unsigned argc, JS::Value *vp) { \
  METHOD_HEADER(0);                                           \
  args.rval().setter(expr(self));                             \
  return true;                                                \
}

#define DEFINE_EVENT_PHASE_GETTER(fn, phase_expr)             \
bool Event::fn(JSContext *cx, unsigned argc, JS::Value *vp) { \
  CallArgs args = JS::CallArgsFromVp(argc, vp);               \
  args.rval().setInt32(static_cast<int32_t>(phase_expr));     \
  return true;                                                \
}

DEFINE_EVENT_GETTER(type_get, setString, type)
DEFINE_EVENT_GETTER(target_get, setObjectOrNull, target)
DEFINE_EVENT_GETTER(srcElement_get, setObjectOrNull, target)
DEFINE_EVENT_GETTER(currentTarget_get, setObjectOrNull, current_target)
DEFINE_EVENT_GETTER(bubbles_get, setBoolean, is_bubbling)
DEFINE_EVENT_GETTER(cancelable_get, setBoolean, is_cancelable)
DEFINE_EVENT_GETTER(defaultPrevented_get, setBoolean, is_default_prevented)
DEFINE_EVENT_GETTER(returnValue_get, setBoolean, !is_default_prevented)
DEFINE_EVENT_GETTER(composed_get, setBoolean, is_composed)
DEFINE_EVENT_GETTER(isTrusted_get, setBoolean, is_trusted)
DEFINE_EVENT_GETTER(timeStamp_get, setNumber, timestamp)
DEFINE_EVENT_GETTER(preventDefault, setBoolean, is_default_prevented)

DEFINE_EVENT_PHASE_GETTER(NONE_get, Phase::NONE)
DEFINE_EVENT_PHASE_GETTER(CAPTURING_PHASE_get, Phase::CAPTURING_PHASE)
DEFINE_EVENT_PHASE_GETTER(AT_TARGET_get, Phase::AT_TARGET)
DEFINE_EVENT_PHASE_GETTER(BUBBLING_PHASE_get, Phase::BUBBLING_PHASE)

bool Event::eventPhase_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  args.rval().setInt32(static_cast<int32_t>(phase(self)));
  return true;
}

bool Event::stopPropagation(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  set_stop_propagation(self, true);
  return true;
}

bool Event::stopImmediatePropagation(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  set_stop_immediate_propagation(self, true);
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

Event::Phase Event::phase(JSObject *self) {
  return static_cast<Phase>(JS::GetReservedSlot(self, Slots::EvtPhase).toInt32());
}

bool Event::is_bubbling(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Bubbles).toBoolean();
}

bool Event::is_cancelable(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Cancelable).toBoolean();
}

bool Event::is_stopped(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::StopPropagation);
}

bool Event::is_stopped_immediate(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::StopImmediatePropagation);
}

bool Event::is_default_prevented(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Canceled);
}

bool Event::is_composed(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Composed);
}

bool Event::is_dispatched(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Dispatch);
}

bool Event::is_initialized(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Initialized);
}

bool Event::is_trusted(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::Trusted).toBoolean();
}

double Event::timestamp(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::TimeStamp).toNumber();
}

void Event::set_trusted(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  SetReservedSlot(self, Slots::Trusted, JS::BooleanValue(val));
}

void Event::set_dispatched(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, EventFlag::Dispatch, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
}

void Event::set_phase(JSObject *self, Event::Phase phase) {
  MOZ_ASSERT(is_instance(self));
  SetReservedSlot(self, Slots::EvtPhase, JS::Int32Value(static_cast<uint32_t>(phase)));
}

void Event::set_target(JSObject *self, HandleObject target) {
  MOZ_ASSERT(is_instance(self));
  auto target_val = target ? JS::ObjectValue(*target) : JS::NullValue();
  SetReservedSlot(self, Slots::Target, target_val);
}

void Event::set_current_target(JSObject *self, HandleObject target) {
  MOZ_ASSERT(is_instance(self));
  auto target_val = target ? JS::ObjectValue(*target) : JS::NullValue();
  SetReservedSlot(self, Slots::CurrentTarget, target_val);
}

void Event::set_related_target(JSObject *self, HandleObject target) {
  MOZ_ASSERT(is_instance(self));
  auto target_val = target ? JS::ObjectValue(*target) : JS::NullValue();
  SetReservedSlot(self, Slots::RelatedTarget, target_val);
}

void Event::set_stop_propagation(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, EventFlag::StopPropagation, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
}

void Event::set_stop_immediate_propagation(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, EventFlag::StopImmediatePropagation, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
}

void Event::set_passive_listener(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, EventFlag::InPassiveListener, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
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

  auto type_str = JS::ToString(cx, type);
  if (!type_str) {
    return false;
  }

  auto bubbles = Bubbles::No;
  auto composed = Composed::No;
  auto cancelable = Cancelable::No;

  if (init.isObject()) {
    read_event_init(cx, init, &bubbles, &cancelable, &composed);
  }

  // To initialize an event, with type, bubbles, and cancelable, run these steps:
  // - Set event's initialized flag.
  // - Unset event's stop propagation flag, stop immediate propagation flag, and canceled flag.
  uint32_t flags = 0;
  set_event_flag(&flags, EventFlag::Initialized);
  set_event_flag(&flags, EventFlag::Composed, static_cast<bool>(composed));

  // Set event's isTrusted attribute to false.
  // Set event's target to null.
  // Set event's type attribute to type.
  // Set event's bubbles attribute to bubbles.
  // Set event's cancelable attribute to cancelable.
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
  SetReservedSlot(self, Slots::Trusted, JS::FalseValue());
  SetReservedSlot(self, Slots::Target, JS::NullValue());
  SetReservedSlot(self, Slots::CurrentTarget, JS::NullValue());
  SetReservedSlot(self, Slots::RelatedTarget, JS::NullValue());
  SetReservedSlot(self, Slots::Type, JS::StringValue(type_str));
  SetReservedSlot(self, Slots::Bubbles, JS::BooleanValue(static_cast<bool>(bubbles)));
  SetReservedSlot(self, Slots::Cancelable, JS::BooleanValue(static_cast<bool>(cancelable)));
  SetReservedSlot(self, Slots::TimeStamp, JS::NumberValue(1.0)); // TODO: implement timestamp
  SetReservedSlot(self, Slots::EvtPhase, JS::Int32Value(static_cast<uint32_t>(Event::Phase::NONE)));

  args.rval().setObject(*self);
  return true;
}

bool Event::initEvent(JSContext *cx, unsigned argc, JS::Value *vp) {
  return api::throw_error(cx, api::Errors::TypeError, "Event#initEvent", "obsolete", "use constructor");
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

  return true;
}

} // namespace event
} // namespace web
} // namespace builtins
