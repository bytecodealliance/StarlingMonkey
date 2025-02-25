#include "event.h"
#include "event-target.h"
#include "custom-event.h"
#include "global-event-target.h"

namespace {

// Each event has the following associated flags that are all initially unset:
// - stop propagation flag
// - stop immediate propagation flag
// - canceled flag
// - in passive listener flag
// - composed flag
// - initialized flag
// - dispatch flag
//
// The extra booleans are not defined as flags in the specification, but we choose
// to retain them for implementation consistency and optimization, as this avoids
// the need for additional slots.
//
// clang-format off
enum class EventFlag : uint32_t {
  StopPropagation          = 1 << 0,
  StopImmediatePropagation = 1 << 1,
  Canceled                 = 1 << 2,
  InPassiveListener        = 1 << 3,
  Composed                 = 1 << 4,
  Initialized              = 1 << 5,
  Dispatch                 = 1 << 6,
  // Extra flags:
  Trusted                  = 1 << 7,
  Bubbles                  = 1 << 8,
  Cancelable               = 1 << 9,
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
  *bubbles = static_cast<Bubbles>(JS::ToBoolean(val));

  if (!JS_GetProperty(cx, obj, "cancelable", &val)) {
    return false;
  }
  *cancelable = static_cast<Cancelable>(JS::ToBoolean(val));

  if (!JS_GetProperty(cx, obj, "composed", &val)) {
    return false;
  }
  *composed = static_cast<Composed>(JS::ToBoolean(val));

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

#define DEFINE_EVENT_GETTER(fn, setter, expr)                 \
bool Event::fn(JSContext *cx, unsigned argc, JS::Value *vp) { \
  METHOD_HEADER(0);                                           \
  args.rval().setter(expr(self));                             \
  return true;                                                \
}

#define DEFINE_EVENT_SETTER(fn, setter, expr)                 \
bool Event::fn(JSContext *cx, unsigned argc, JS::Value *vp) { \
  METHOD_HEADER(0);                                           \
  setter(self, expr);                                         \
  return true;                                                \
}

#define DEFINE_EVENT_PHASE(fn, phase_expr)                    \
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

DEFINE_EVENT_SETTER(stopPropagation, set_stop_propagation, true)
DEFINE_EVENT_SETTER(stopImmediatePropagation, set_stop_immediate_propagation, true)
DEFINE_EVENT_SETTER(preventDefault, set_canceled, true)

DEFINE_EVENT_PHASE(NONE_get, Phase::NONE)
DEFINE_EVENT_PHASE(CAPTURING_PHASE_get, Phase::CAPTURING_PHASE)
DEFINE_EVENT_PHASE(AT_TARGET_get, Phase::AT_TARGET)
DEFINE_EVENT_PHASE(BUBBLING_PHASE_get, Phase::BUBBLING_PHASE)

bool Event::eventPhase_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  args.rval().setInt32(static_cast<int32_t>(phase(self)));
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

Event::Phase Event::phase(JSObject *self) {
  return static_cast<Phase>(JS::GetReservedSlot(self, Slots::EvtPhase).toInt32());
}

bool Event::is_bubbling(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Bubbles);
}

bool Event::is_cancelable(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Cancelable);
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
  auto flags = JS::GetReservedSlot(self, Slots::Flags).toInt32();
  return test_event_flag(flags, EventFlag::Trusted);
}

double Event::timestamp(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::TimeStamp).toNumber();
}

void Event::set_trusted(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, EventFlag::Trusted, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
}

void Event::set_dispatched(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));
  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  set_event_flag(&flags, EventFlag::Dispatch, val);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
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

void Event::set_canceled(JSObject *self, bool val) {
  MOZ_ASSERT(is_instance(self));

  auto flags = static_cast<uint32_t>(JS::GetReservedSlot(self, Slots::Flags).toInt32());

  // https://dom.spec.whatwg.org/#set-the-canceled-flag
  // To set the canceled flag, given an event event, if event's cancelable
  // attribute value is true and event's in passive listener flag is unset,
  // then set event's canceled flag, and do nothing otherwise.
  auto canceled = val
    && test_event_flag(flags, EventFlag::Cancelable)
    && !test_event_flag(flags, EventFlag::InPassiveListener);

  set_event_flag(&flags, EventFlag::Canceled, canceled);
  SetReservedSlot(self, Slots::Flags, JS::Int32Value(flags));
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

bool Event::init(JSContext *cx, HandleObject self, HandleValue type, HandleValue init)
{
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
  set_event_flag(&flags, EventFlag::Bubbles, static_cast<bool>(bubbles));
  set_event_flag(&flags, EventFlag::Composed, static_cast<bool>(composed));
  set_event_flag(&flags, EventFlag::Cancelable, static_cast<bool>(cancelable));

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
  if (!CustomEvent::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!global_event_target_init(engine->cx(), engine->global())) {
    return false;
  }

  return true;
}

} // namespace event
} // namespace web
} // namespace builtins
