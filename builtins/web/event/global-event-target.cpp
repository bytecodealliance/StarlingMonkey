#include "event-target.h"
#include "global-event-target.h"

namespace {
JS::PersistentRootedObject GLOBAL_EVENT_TARGET;
JS::PersistentRootedObjectVector *HANDLERS;
}

namespace builtins {
namespace web {
namespace event {

JSObject *global_event_target() {
  return GLOBAL_EVENT_TARGET;
}

static bool addEventListener(JSContext *cx, unsigned argc, Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "addEventListener", 2)) {
    return false;
  }

  RootedValue type(cx, args.get(0));
  RootedValue callback(cx, args.get(1));
  RootedValue opts(cx, args.get(2));

  args.rval().setUndefined();

  std::ignore = HANDLERS->append(&callback.toObject());
  return EventTarget::add_listener(cx, GLOBAL_EVENT_TARGET, type, callback, opts);
}

static bool removeEventListener(JSContext *cx, unsigned argc, Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "removeEventListener", 2)) {
    return false;
  }

  RootedValue type(cx, args.get(0));
  RootedValue callback(cx, args.get(1));
  RootedValue opts(cx, args.get(2));
  args.rval().setUndefined();

  HANDLERS->eraseIf([&](const auto &handler) { return handler == callback.toObjectOrNull(); });
  return EventTarget::remove_listener(cx, GLOBAL_EVENT_TARGET, type, callback, opts);
}

static bool dispatchEvent(JSContext *cx, unsigned argc, Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "dispatchEvent", 1)) {
    return false;
  }

  RootedValue event(cx, args.get(0));
  return EventTarget::dispatch_event(cx, GLOBAL_EVENT_TARGET, event, args.rval());
}

bool global_event_target_init(JSContext *cx, HandleObject global) {
  if (!JS_DefineFunction(cx, global, "addEventListener", addEventListener, 2, 0)) {
    return false;
  }

  if (!JS_DefineFunction(cx, global, "removeEventListener", removeEventListener, 2, 0)) {
    return false;
  }

  if (!JS_DefineFunction(cx, global, "dispatchEvent", dispatchEvent, 1, 0)) {
    return false;
  }

  auto global_event_target = EventTarget::create(cx);
  if (!global_event_target) {
    return false;
  }

  GLOBAL_EVENT_TARGET.init(cx, global_event_target);
  HANDLERS = new JS::PersistentRootedObjectVector(cx);
  return true;
}

} // namespace event
} // namespace web
} // namespace builtins
