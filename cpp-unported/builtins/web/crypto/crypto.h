#ifndef JS_COMPUTE_RUNTIME_BUILTIN_CRYPTO_H
#define JS_COMPUTE_RUNTIME_BUILTIN_CRYPTO_H

#include "builtin.h"
#include "js-compute-builtins.h"

namespace builtins {

class Crypto : public BuiltinImpl<Crypto> {
private:
public:
  static constexpr const char *class_name = "Crypto";
  static const int ctor_length = 0;

  static JS::PersistentRooted<JSObject *> subtle;

  enum Slots { Count };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool subtle_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool get_random_values(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool random_uuid(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool init_class(JSContext *cx, JS::HandleObject global);
};

} // namespace builtins

#endif