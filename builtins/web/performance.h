#ifndef JS_COMPUTE_RUNTIME_BUILTIN_PERFORMANCE_H
#define JS_COMPUTE_RUNTIME_BUILTIN_PERFORMANCE_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace performance {

class Performance : public BuiltinNoConstructor<Performance> {
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
  static bool init_class(JSContext *cx, JS::HandleObject global);
};

bool install(api::Engine *engine);

} // namespace performance
} // namespace web
} // namespace builtins

#endif
