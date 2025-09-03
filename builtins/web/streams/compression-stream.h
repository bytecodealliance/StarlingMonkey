#ifndef BUILTINS_WEB_STREAMS_COMPRESSION_STREAM_H
#define BUILTINS_WEB_STREAMS_COMPRESSION_STREAM_H

#include "builtin.h"



namespace builtins::web::streams {

/**
 * Implementation of the WICG CompressionStream builtin.
 *
 * All algorithm names and steps refer to spec algorithms defined at
 * https://streams.spec.whatwg.org/#ts-class
 */
class CompressionStream : public BuiltinImpl<CompressionStream> {
  static bool transformAlgorithm(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool flushAlgorithm(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool readable_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool writable_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "CompressionStream";

  enum Slots : uint8_t { Transform, Format, State, Buffer, Count };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
};

} // namespace builtins::web::streams



#endif
