#include "text-codec.h"
#include "text-encoder.h"
#include "text-decoder.h"

namespace builtins {
namespace web {
namespace text_codec {

bool add_to_global(JSContext *cx, JS::HandleObject global) {
  if (!TextEncoder::init_class(cx, global))
    return false;
  if (!TextDecoder::init_class(cx, global))
    return false;
  return true;
}

} // namespace text_codec
} // namespace web
} // namespace builtins
