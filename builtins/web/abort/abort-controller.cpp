#include "abort-controller.h"
#include "abort-signal.h"

namespace builtins {
namespace web {
namespace abort {

const JSFunctionSpec AbortController::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec AbortController::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec AbortController::methods[] = {
    JS_FN("abort", abort, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec AbortController::properties[] = {
    JS_PSG("signal", signal_get, JSPROP_ENUMERATE),
    JS_PS_END,
};

bool AbortController::signal_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  args.rval().set(JS::GetReservedSlot(self, Slots::Signal));
  return true;
}

bool AbortController::abort(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  RootedValue reason(cx, args.get(0));
  RootedObject signal(cx, JS::GetReservedSlot(self, Slots::Signal).toObjectOrNull());
  if (!signal) {
    return false;
  }

  return AbortSignal::abort(cx, signal, reason);
}

bool AbortController::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("AbortController", 0);

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }

  RootedObject signal(cx, AbortSignal::create(cx));
  if (!signal) {
    return false;
  }

  SetReservedSlot(self, Slots::Signal, JS::ObjectValue(*signal));

  args.rval().setObject(*self);
  return true;
}

bool AbortController::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

}  // namespace abort
}  // namespace web
}  // namespace builtins
