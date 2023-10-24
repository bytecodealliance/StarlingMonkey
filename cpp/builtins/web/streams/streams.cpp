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

bool add_to_global(JSContext *cx, JS::HandleObject global) {
  if (!NativeStreamSource::init_class(cx, global))
    return false;
  if (!NativeStreamSink::init_class(cx, global))
    return false;
  if (!TransformStreamDefaultController::init_class(cx, global))
    return false;
  if (!TransformStream::init_class(cx, global))
    return false;
  if (!CompressionStream::init_class(cx, global))
    return false;
  if (!DecompressionStream::init_class(cx, global))
    return false;
  return true;
}

} // namespace streams
} // namespace web
} // namespace builtins
