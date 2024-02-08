#ifndef JS_COMPUTE_RUNTIME_SUBTLE_CRYPTO_H
#define JS_COMPUTE_RUNTIME_SUBTLE_CRYPTO_H

#include "builtin.h"
#include "crypto-algorithm.h"

namespace builtins {

enum class Operations : uint8_t {
  Encrypt,
  Decrypt,
  Sign,
  Verify,
  Digest,
  GenerateKey,
  DeriveBits,
  ImportKey,
  WrapKey,
  UnwrapKey,
  GetKeyLength
};

class SubtleCrypto : public BuiltinImpl<SubtleCrypto> {
private:
public:
  static constexpr const char *class_name = "SubtleCrypto";
  static const int ctor_length = 0;

  enum Slots { Count };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];
  static bool digest(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool importKey(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool sign(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool verify(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool init_class(JSContext *cx, JS::HandleObject global);
};

} // namespace builtins
#endif