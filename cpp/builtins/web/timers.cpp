#include "timers.h"
#include "core/event_loop.h"

/**
 * The `setTimeout` and `setInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval
 */
template <bool repeat>
bool setTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  // REQUEST_HANDLER_ONLY(repeat ? "setInterval" : "setTimeout");
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, repeat ? "setInterval" : "setTimeout", 1)) {
    return false;
  }

  if (!(args[0].isObject() && JS::IsCallable(&args[0].toObject()))) {
    JS_ReportErrorASCII(cx, "First argument to %s must be a function",
                        repeat ? "setInterval" : "setTimeout");
    return false;
  }
  RootedObject handler(cx, &args[0].toObject());

  int32_t delay = 0;
  if (args.length() > 1 && !JS::ToInt32(cx, args.get(1), &delay)) {
    return false;
  }
  if (delay < 0) {
    delay = 0;
  }

  JS::RootedValueVector handler_args(cx);
  if (args.length() > 2 && !handler_args.initCapacity(args.length() - 2)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  for (size_t i = 2; i < args.length(); i++) {
    handler_args.infallibleAppend(args[i]);
  }

  uint32_t id =
      core::EventLoop::add_timer(handler, delay, handler_args, repeat);

  args.rval().setInt32(id);
  return true;
}

/**
 * The `clearTimeout` and `clearInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-cleartimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-clearinterval
 */
template <bool interval>
bool clearTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  // REQUEST_HANDLER_ONLY(interval ? "clearInterval" : "clearTimeout");
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, interval ? "clearInterval" : "clearTimeout",
                           1)) {
    return false;
  }

  int32_t id = 0;
  if (!JS::ToInt32(cx, args[0], &id)) {
    return false;
  }

  core::EventLoop::remove_timer(uint32_t(id));

  args.rval().setUndefined();
  return true;
}

const JSFunctionSpec methods[] = {
    JS_FN("setInterval", setTimeout_or_interval<true>, 1, JSPROP_ENUMERATE),
    JS_FN("setTimeout", setTimeout_or_interval<false>, 1, JSPROP_ENUMERATE),
    JS_FS_END};

bool builtins::web::timers::add_to_global(JSContext *cx,
                                          JS::HandleObject global) {
  return JS_DefineFunctions(cx, global, methods);
}
