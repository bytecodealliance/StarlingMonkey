#ifndef JS_COMPUTE_RUNTIME_NATIVE_STREAM_SOURCE_H
#define JS_COMPUTE_RUNTIME_NATIVE_STREAM_SOURCE_H

#include "builtin.h"
#include "js-compute-builtins.h"

namespace builtins {
class NativeStreamSource : public BuiltinNoConstructor<NativeStreamSource> {
private:
public:
  static constexpr const char *class_name = "NativeStreamSource";
  enum Slots {
    Owner,          // Request or Response object, or TransformStream.
    Controller,     // The ReadableStreamDefaultController.
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
  typedef bool PullAlgorithmImplementation(JSContext *cx, JS::CallArgs args,
                                           JS::HandleObject stream, JS::HandleObject owner,
                                           JS::HandleObject controller);
  typedef bool CancelAlgorithmImplementation(JSContext *cx, JS::CallArgs args,
                                             JS::HandleObject stream, JS::HandleObject owner,
                                             JS::HandleValue reason);
  static JSObject *owner(JSObject *self);
  static JSObject *stream(JSObject *self);
  static JS::Value startPromise(JSObject *self);
  static PullAlgorithmImplementation *pullAlgorithm(JSObject *self);
  static CancelAlgorithmImplementation *cancelAlgorithm(JSObject *self);
  static JSObject *controller(JSObject *self);
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
  static JSObject *create(JSContext *cx, JS::HandleObject owner, JS::HandleValue startPromise,
                          PullAlgorithmImplementation *pull, CancelAlgorithmImplementation *cancel);
};
} // namespace builtins
#endif