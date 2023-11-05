#include "text-codec.h"
#include "text-encoder.h"
#include "text-decoder.h"

namespace builtins {
namespace web {
namespace text_codec {

bool install(core::Engine* engine) {
  if (!TextEncoder::init_class(engine->cx(), engine->global()))
    return false;
  if (!TextDecoder::init_class(engine->cx(), engine->global()))
    return false;
  return true;
}

} // namespace text_codec
} // namespace web
} // namespace builtins
