#ifndef BUILTINS_WEB_EVENT_H_
#define BUILTINS_WEB_EVENT_H_

#include "builtin.h"

namespace builtins {
namespace web {
namespace event {

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

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSString *type(JSObject *self);
  static JSObject *target(JSObject *self);
  static JSObject *related_target(JSObject *self);
  static JSObject *current_target(JSObject *self);

  static bool is_bubbling(JSObject *self);
  static bool is_cancelable(JSObject *self);
  static bool is_stopped(JSObject *self);
  static bool is_stopped_immediate(JSObject *self);
  static bool is_default_prevented(JSObject *self);
  static bool is_composed(JSObject *self);
  static bool is_dispatched(JSObject *self);
  static bool is_initialized(JSObject *self);
  static bool is_trusted(JSObject *self);

  static Phase phase(JSObject *self);
  static double timestamp(JSObject *self);

  static void set_trusted(JSObject *self, bool val);
  static void set_dispatched(JSObject *self, bool val);
  static void set_passive_listener(JSObject *self, bool val);
  static void set_stop_propagation(JSObject *self, bool val);
  static void set_stop_immediate_propagation(JSObject *self, bool val);
  static void set_canceled(JSObject *self, bool val);
  static void set_phase(JSObject *self, Phase phase);
  static void set_target(JSObject *self, HandleObject target);
  static void set_current_target(JSObject *self, HandleObject target);
  static void set_related_target(JSObject *self, HandleObject target);

  static constexpr unsigned ctor_length = 1;
  enum Slots {
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

  static bool init(JSContext *cx, HandleObject self, HandleValue type, HandleValue init);
  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

bool install(api::Engine *engine);

} // namespace event
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_EVENT_H_
