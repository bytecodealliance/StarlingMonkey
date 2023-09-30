#ifndef BUILTIN_TEXT_ENCODER_H
#define BUILTIN_TEXT_ENCODER_H

#include "builtin.h"

namespace builtins {

class TextEncoder final : public BuiltinImpl<TextEncoder> {
  static bool encode(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool encodeInto(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool encoding_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "TextEncoder";

  enum class Slots { Count };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 0;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
};

} // namespace builtins

#endif
