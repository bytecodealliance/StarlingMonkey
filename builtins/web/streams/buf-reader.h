#ifndef BUILTINS_WEB_STREAMS_BUF_FREADER_H
#define BUILTINS_WEB_STREAMS_BUF_FREADER_H

#include "builtin.h"



namespace builtins::web::streams {

class BufReader : public BuiltinNoConstructor<BufReader> {
public:
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr const char *class_name = "NativeBufReader";

  enum Slots { User, Stream, Read, Position, Count };

  using Buffer = std::span<uint8_t>;

  // Read a slice of data from the underlying source into the specified buffer.
  //
  // This function uses the `start` parameter to indicate how much data has
  // already been enqueued or consumed by previous reads. The callee should read
  // up to `buf.size()` bytes into `buf`. The actual number of bytes read has
  // to be stored in `read`, and `done` set to true when no further data remains.
  using ReadFn = bool (JSContext *, HandleObject, Buffer, size_t, size_t *, bool *);

  static JSObject *user(JSObject *self);
  static JSObject *stream(JSObject *self);
  static ReadFn *read_fn(JSObject *self);
  static size_t position(JSObject *self);
  static void set_position(JSObject *self, size_t pos);

  static JSObject *create(JSContext *cx, HandleObject owner, ReadFn *read);
};

} // namespace builtins::web::streams



#endif // BUILTINS_WEB_STREAMS_BUF_FREADER_H
