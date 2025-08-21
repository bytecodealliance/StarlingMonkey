#ifndef BUILTINS_WEB_STREAMS_NATIVE_STREAM_SOURCE_H
#define BUILTINS_WEB_STREAMS_NATIVE_STREAM_SOURCE_H

#include "builtin.h"



namespace builtins::web::streams {
class NativeStreamSource : public BuiltinNoConstructor<NativeStreamSource> {
private:
public:
  static constexpr const char *class_name = "NativeStreamSource";
  enum Slots : uint8_t {
    Owner,          // Request or Response object, or TransformStream.
    Stream,         // The ReadableStreamDefaultObject.
    InternalReader, // Only used to lock the stream if it's consumed internally.
    StartPromise,   // Used as the return value of `start`, can be undefined.
                    // Needed to properly implement TransformStream.
    PullAlgorithm,
    CancelAlgorithm,
    PipedToTransformStream, // The TransformStream this source's stream is piped
                            // to, if any. Only applies if the source backs a
                            // RequestOrResponse's body.
    Count
  };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];
  using PullAlgorithmImplementation = bool (JSContext *, JS::CallArgs, JS::HandleObject, JS::HandleObject, JS::HandleObject);
  using CancelAlgorithmImplementation = bool (JSContext *, JS::CallArgs, JS::HandleObject, JS::HandleObject, JS::HandleValue);
  static JSObject *owner(JSObject *self);
  static JSObject *stream(JSObject *self);
  static JS::Value startPromise(JSObject *self);
  static PullAlgorithmImplementation *pullAlgorithm(JSObject *self);
  static CancelAlgorithmImplementation *cancelAlgorithm(JSObject *self);
  static JSObject *get_controller_source(JSContext *cx, JS::HandleObject controller);
  static JSObject *get_stream_source(JSContext *cx, JS::HandleObject stream);
  static bool stream_has_native_source(JSContext *cx, JS::HandleObject stream);
  static bool stream_is_body(JSContext *cx, JS::HandleObject stream);
  static void set_stream_piped_to_ts_writable(JSContext *cx, JS::HandleObject stream,
                                              JS::HandleObject writable);
  static JSObject *piped_to_transform_stream(JSObject *self);
  static bool lock_stream(JSContext *cx, JS::HandleObject stream);
  static bool start(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool pull(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool cancel(JSContext *cx, unsigned argc, JS::Value *vp);

  // Create an instance of `NativStreamSource`
  //
  // `NativeStreamSource` internally creates a `ReadableDefaultStreamObject` instance. To prevent an
  // eager pull, we choose to overwrite the default `highWaterMark` value, setting it to 0.0. With
  // the default `highWaterMark` of 1.0, the stream implementation automatically triggers a pull,
  // which means we enqueue a read from the host handle even though we often have no interest in it
  // at all.
  static JSObject *create(JSContext *cx, JS::HandleObject owner, JS::HandleValue startPromise,
                          PullAlgorithmImplementation *pull, CancelAlgorithmImplementation *cancel,
                          JS::HandleFunction size = nullptr, double highWaterMark = 0.0);
};
} // namespace builtins::web::streams


#endif
