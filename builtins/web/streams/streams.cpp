#include "streams.h"
#include "buf-reader.h"
#include "compression-stream.h"
#include "decompression-stream.h"
#include "event_loop.h"
#include "native-stream-sink.h"
#include "native-stream-source.h"
#include "transform-stream-default-controller.h"
#include "transform-stream.h"



namespace builtins::web::streams {

bool install(api::Engine *engine) {
  if (!NativeStreamSource::init_class(engine->cx(), engine->global())) {
    return false;
}
  if (!NativeStreamSink::init_class(engine->cx(), engine->global())) {
    return false;
}
  if (!TransformStreamDefaultController::init_class(engine->cx(), engine->global())) {
    return false;
}
  if (!TransformStream::init_class(engine->cx(), engine->global())) {
    return false;
}
  if (!CompressionStream::init_class(engine->cx(), engine->global())) {
    return false;
}
  if (!DecompressionStream::init_class(engine->cx(), engine->global())) {
    return false;
}
  if (!BufReader::init_class(engine->cx(), engine->global())) {
    return false;
}
  return true;
}

} // namespace builtins::web::streams


