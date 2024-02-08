#ifndef BUILTINS_TEXT_DECODER_H
#define BUILTINS_TEXT_DECODER_H

#include "builtin.h"

namespace builtins {

class TextDecoder final : public BuiltinImpl<TextDecoder> {
  static bool decode(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool encoding_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool fatal_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool ignoreBOM_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "TextDecoder";

  enum class Slots {
    Decoder,
    Encoding,
    Fatal,
    IgnoreBOM,
    Count,
  };

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
