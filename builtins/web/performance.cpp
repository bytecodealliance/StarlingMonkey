#include "performance.h"
#include "builtin.h"
#include <chrono>

namespace {
using FpMilliseconds = std::chrono::duration<float, std::chrono::milliseconds::period>;
} // namespace

namespace builtins {

std::optional<std::chrono::steady_clock::time_point> Performance::timeOrigin;

// https://w3c.github.io/hr-time/#dom-performance-now
bool Performance::now(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  MOZ_ASSERT(builtins::Performance::timeOrigin.has_value());

  auto finish = std::chrono::high_resolution_clock::now();
  auto duration = FpMilliseconds(finish - builtins::Performance::timeOrigin.value()).count();

  JS::RootedValue elapsed(cx, JS::Float32Value(duration));
  args.rval().set(elapsed);
  return true;
}

bool Performance::timeOrigin_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  MOZ_ASSERT(builtins::Performance::timeOrigin.has_value());
  METHOD_HEADER(0);
  auto time = FpMilliseconds(builtins::Performance::timeOrigin.value().time_since_epoch()).count();
  JS::RootedValue elapsed(cx, JS::Float32Value(time));
  args.rval().set(elapsed);
  return true;
}

const JSFunctionSpec Performance::methods[] = {JS_FN("now", now, 0, JSPROP_ENUMERATE), JS_FS_END};

const JSPropertySpec Performance::properties[] = {
    JS_PSG("timeOrigin", timeOrigin_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "performance", JSPROP_READONLY), JS_PS_END};

const JSFunctionSpec Performance::static_methods[] = {JS_FS_END};
const JSPropertySpec Performance::static_properties[] = {JS_PS_END};

bool Performance::create(JSContext *cx, JS::HandleObject global) {
  JS::RootedObject performance(cx, JS_NewObjectWithGivenProto(cx, &builtins::Performance::class_,
                                                              builtins::Performance::proto_obj));
  if (!performance) {
    return false;
  }
  if (!JS_DefineProperty(cx, global, "performance", performance, 0)) {
    return false;
  }
  if (!JS_DefineProperties(cx, performance, properties)) {
    return false;
  }
  return JS_DefineFunctions(cx, performance, methods);
}

bool Performance::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS_ReportErrorUTF8(cx, "%s can't be instantiated directly", class_name);
  return false;
}

bool Performance::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}
} // namespace builtins
