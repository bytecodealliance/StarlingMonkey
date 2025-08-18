#ifndef BUILTINS_WEB_EVENT_TARGET_H_
#define BUILTINS_WEB_EVENT_TARGET_H_

#include "builtin.h"
#include "extension-api.h"

#include "js/RefCounted.h"
#include "mozilla/RefPtr.h"

namespace builtins {
namespace web {
namespace event {

struct EventListener : public js::RefCounted<EventListener> {
  void trace(JSTracer *trc) {
    TraceEdge(trc, &callback, "EventListener callback");
    TraceEdge(trc, &signal, "EventListener signal");
  }

  JS::Heap<JS::Value> callback;
  JS::Heap<JS::Value> signal;

  std::string type;

  bool passive;
  bool capture;
  bool once;
  bool removed;

  // Define equality: only callback, type, and capture matter.
  bool operator==(const EventListener &other) const {
    return (callback == other.callback) && (type == other.type) && (capture == other.capture);
  }
};

class EventTarget : public BuiltinImpl<EventTarget, TraceableClassPolicy> {
  static bool addEventListener(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool removeEventListener(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool dispatchEvent(JSContext *cx, unsigned argc, JS::Value *vp);

  using ListenerRef = RefPtr<EventListener>;
  using ListenerList = JS::GCVector<ListenerRef, 0, js::SystemAllocPolicy>;

  static ListenerList *listeners(JSObject *self);

  static bool dispatch(JSContext *cx, HandleObject self, HandleObject event,
                       HandleObject target_override, MutableHandleValue rval);

  static bool inner_invoke(JSContext *cx, HandleObject event, JS::HandleVector<ListenerRef> list,
                           bool *found);

  static bool invoke_listeners(JSContext *cx, HandleObject target, HandleObject event);

  static bool on_abort(JSContext *cx, std::span<HeapValue> args);

public:
  static constexpr const char *class_name = "EventTarget";
  static constexpr unsigned ctor_length = 0;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  enum Slots { Listeners, Count };

  static bool add_listener(JSContext *cx, HandleObject self, HandleValue type, HandleValue callback,
                           HandleValue opts);
  static bool remove_listener(JSContext *cx, HandleObject self, HandleValue type,
                              HandleValue callback, HandleValue opts);
  static bool dispatch_event(JSContext *cx, HandleObject self, HandleValue event,
                             MutableHandleValue rval);

  static JSObject *create(JSContext *cx);

  static bool init(JSContext *cx, HandleObject self);
  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
  static void finalize(JS::GCContext *gcx, JSObject *self);
  static void trace(JSTracer *trc, JSObject *self);
};

} // namespace event
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_EVENT_TARGET_H_
