#include "queue-microtask.h"



namespace builtins::web::queue_microtask {

/**
 * The `queueMicrotask` global function
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#microtask-queuing
 */
bool queueMicrotask(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "queueMicrotask", 1)) {
    return false;
  }

  if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject())) {
    return api::throw_error(cx, api::Errors::TypeError, "queueMicrotask", "first argument",
      "be a function");
  }

  RootedObject callback(cx, &args[0].toObject());

  RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  if (promise == nullptr) {
    return false;
  }

  if (!JS::AddPromiseReactions(cx, promise, callback, nullptr)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

const JSFunctionSpec methods[] = {JS_FN("queueMicrotask", queueMicrotask, 1, JSPROP_ENUMERATE),
                                  JS_FS_END};

bool install(api::Engine *engine) {
  return JS_DefineFunctions(engine->cx(), engine->global(), methods);
}

} // namespace builtins::web::queue_microtask


