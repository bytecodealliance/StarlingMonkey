#ifndef JS_COMPUTE_RUNTIME_DECOMPRESSION_STREAM_H
#define JS_COMPUTE_RUNTIME_DECOMPRESSION_STREAM_H

#include "builtin.h"
#include "js-compute-builtins.h"

namespace builtins {

/**
 * Implementation of the WICG DecompressionStream builtin.
 *
 * All algorithm names and steps refer to spec algorithms defined at
 * https://wicg.github.io/compression/#decompression-stream
 */
class DecompressionStream : public BuiltinImpl<DecompressionStream> {
  static bool transformAlgorithm(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool flushAlgorithm(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool readable_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool writable_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "DecompressionStream";

  enum Slots { Transform, Format, State, Buffer, Count };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
};

} // namespace builtins

#endif
