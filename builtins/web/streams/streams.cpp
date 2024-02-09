#include "event_loop.h"
#include "streams.h"
#include "native-stream-source.h"
#include "native-stream-sink.h"
#include "transform-stream.h"
#include "transform-stream-default-controller.h"
#include "compression-stream.h"
#include "decompression-stream.h"

namespace builtins {
namespace web {
namespace streams {

bool install(api::Engine* engine) {
  if (!NativeStreamSource::init_class(engine->cx(), engine->global()))
    return false;
  if (!NativeStreamSink::init_class(engine->cx(), engine->global()))
    return false;
  if (!TransformStreamDefaultController::init_class(engine->cx(), engine->global()))
    return false;
  if (!TransformStream::init_class(engine->cx(), engine->global()))
    return false;
  if (!CompressionStream::init_class(engine->cx(), engine->global()))
    return false;
  if (!DecompressionStream::init_class(engine->cx(), engine->global()))
    return false;
  return true;
}

} // namespace streams
} // namespace web
} // namespace builtins
