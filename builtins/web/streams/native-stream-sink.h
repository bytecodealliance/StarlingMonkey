#ifndef JS_COMPUTE_RUNTIME_NATIVE_STREAM_SINK_H
#define JS_COMPUTE_RUNTIME_NATIVE_STREAM_SINK_H

#include "builtin.h"
#include "js-compute-builtins.h"

namespace builtins {

class NativeStreamSink : public BuiltinNoConstructor<NativeStreamSink> {
private:
public:
  static constexpr const char *class_name = "NativeStreamSink";

  enum Slots {
    Owner,          // TransformStream.
    Controller,     // The WritableStreamDefaultController.
    InternalWriter, // Only used to lock the stream if it's consumed internally.
    StartPromise,   // Used as the return value of `start`, can be undefined.
                    // Needed to properly implement TransformStream.
    WriteAlgorithm,
    AbortAlgorithm,
    CloseAlgorithm,
    // AbortAlgorithm, TODO: implement
    Count
  };

  typedef bool WriteAlgorithmImplementation(JSContext *cx, JS::CallArgs args,
                                            JS::HandleObject stream, JS::HandleObject owner,
                                            JS::HandleValue chunk);
  typedef bool AbortAlgorithmImplementation(JSContext *cx, JS::CallArgs args,
                                            JS::HandleObject stream, JS::HandleObject owner,
                                            JS::HandleValue reason);
  typedef bool CloseAlgorithmImplementation(JSContext *cx, JS::CallArgs args,
                                            JS::HandleObject stream, JS::HandleObject owner);

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSObject *owner(JSObject *self);
  static JS::Value startPromise(JSObject *self);
  static WriteAlgorithmImplementation *writeAlgorithm(JSObject *self);
  static AbortAlgorithmImplementation *abortAlgorithm(JSObject *self);
  static CloseAlgorithmImplementation *closeAlgorithm(JSObject *self);
  static JSObject *controller(JSObject *self);

  /**
   * Returns the underlying sink for the given controller iff it's an object,
   * nullptr otherwise.
   */
  static JSObject *get_controller_sink(JSContext *cx, JS::HandleObject controller);
  static JSObject *get_stream_sink(JSContext *cx, JS::HandleObject stream);
  static bool stream_has_native_sink(JSContext *cx, JS::HandleObject stream);
  static bool start(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool write(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool abort(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool close(JSContext *cx, unsigned argc, JS::Value *vp);
  static JSObject *create(JSContext *cx, JS::HandleObject owner, JS::HandleValue startPromise,
                          WriteAlgorithmImplementation *write, CloseAlgorithmImplementation *close,
                          AbortAlgorithmImplementation *abort);
};

} // namespace builtins
#endif