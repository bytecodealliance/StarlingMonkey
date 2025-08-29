#include "text-codec.h"
#include "text-decoder.h"
#include "text-encoder.h"

namespace builtins::web::text_codec {

bool install(api::Engine *engine) {
  if (!TextEncoder::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!TextDecoder::init_class(engine->cx(), engine->global())) {
    return false;
  }
  return true;
}

} // namespace builtins::web::text_codec


