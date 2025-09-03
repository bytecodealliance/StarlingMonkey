#ifndef BUILTINS_WEB_EVENT_H_
#define BUILTINS_WEB_EVENT_H_

#include "builtin.h"



namespace builtins::web::event {

class Event : public BuiltinImpl<Event> {
  static bool type_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool target_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool currentTarget_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool srcElement_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool eventPhase_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool bubbles_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool cancelable_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool returnValue_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool defaultPrevented_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool composed_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool isTrusted_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool timeStamp_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool returnValue_set(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool NONE_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool CAPTURING_PHASE_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool AT_TARGET_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool BUBBLING_PHASE_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool stopPropagation(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool stopImmediatePropagation(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool preventDefault(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool composedPath(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool initEvent(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "Event";

  enum class Phase : uint8_t { NONE = 0, CAPTURING_PHASE = 1, AT_TARGET = 2, BUBBLING_PHASE = 3 };

  // (From https://dom.spec.whatwg.org/#stop-propagation-flag and onwards:)
  // Each event has the following associated flags that are all initially unset:
  // - stop propagation flag
  // - stop immediate propagation flag
  // - canceled flag
  // - in passive listener flag
  // - composed flag
  // - initialized flag
  // - dispatch flag
  //
  // Note: we store the flags on instances instead of the class itself, since that way
  // we can combine them with the following instance attributes without any overhead:
  // - Trusted
  // - Bubbles
  // - Cancelable
  // clang-format off
  enum class EventFlag : uint16_t {
    // Event type flags:
    StopPropagation          = 1 << 0,
    StopImmediatePropagation = 1 << 1,
    Canceled                 = 1 << 2,
    InPassiveListener        = 1 << 3,
    Composed                 = 1 << 4,
    Initialized              = 1 << 5,
    Dispatch                 = 1 << 6,
    // Instance attributes:
    Trusted                  = 1 << 7,
    Bubbles                  = 1 << 8,
    Cancelable               = 1 << 9,
  };
  // clang-format on

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSString *type(JSObject *self);
  static JSObject *target(JSObject *self);
  static JSObject *related_target(JSObject *self);
  static JSObject *current_target(JSObject *self);

  static bool has_flag(JSObject* self, EventFlag flag);
  static bool set_flag(JSObject* self, EventFlag flag, bool val);

  static Phase phase(JSObject *self);
  static double timestamp(JSObject *self);

  static void set_canceled(JSObject *self, bool val);
  static void set_phase(JSObject *self, Phase phase);
  static void set_target(JSObject *self, HandleObject target);
  static void set_current_target(JSObject *self, HandleObject target);
  static void set_related_target(JSObject *self, HandleObject target);

  static constexpr unsigned ctor_length = 1;
  enum Slots : uint8_t {
    Flags,
    Target,
    RelatedTarget,
    CurrentTarget,
    Type,
    TimeStamp,
    EvtPhase,
    Path,
    Count
  };

  static JSObject*create(JSContext *cx, HandleValue type, HandleValue init);

  static bool init(JSContext *cx, HandleObject self, HandleValue type, HandleValue init);
  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

bool install(api::Engine *engine);

} // namespace builtins::web::event



#endif // BUILTINS_WEB_EVENT_H_
