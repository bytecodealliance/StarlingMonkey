#ifndef JS_COMPUTE_RUNTIME_BUILTIN_PERFORMANCE_H
#define JS_COMPUTE_RUNTIME_BUILTIN_PERFORMANCE_H

#include "builtin.h"

namespace builtins {

class Performance : public BuiltinImpl<Performance> {
private:
public:
  static constexpr const char *class_name = "Performance";
  static const int ctor_length = 0;
  enum Slots { Count };
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec properties[];
  static const JSPropertySpec static_properties[];
  static std::optional<std::chrono::steady_clock::time_point> timeOrigin;

  static bool now(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool timeOrigin_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool create(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool init_class(JSContext *cx, JS::HandleObject global);
};

} // namespace builtins

#endif
