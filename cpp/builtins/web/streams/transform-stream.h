#ifndef JS_COMPUTE_RUNTIME_TRANSFORM_STREAM_H
#define JS_COMPUTE_RUNTIME_TRANSFORM_STREAM_H

#include "builtin.h"
#include "js-compute-builtins.h"

namespace builtins {

class TransformStream : public BuiltinImpl<TransformStream> {
private:
public:
  static constexpr const char *class_name = "TransformStream";
  static const int ctor_length = 0;

  enum Slots {
    Controller,
    Readable,
    Writable,
    Backpressure,
    BackpressureChangePromise,
    Owner,       // The target RequestOrResponse object if the stream's readable end is
                 // used as a body.
    UsedAsMixin, // `true` if the TransformStream is used in another transforming
                 // builtin, such as CompressionStream.
    HasTransformer,
    Count
  };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSObject *owner(JSObject *self);
  static void set_owner(JSObject *self, JSObject *owner);
  static JSObject *readable(JSObject *self);
  static bool is_ts_readable(JSContext *cx, JS::HandleObject readable);
  static JSObject *ts_from_readable(JSContext *cx, JS::HandleObject readable);
  static bool readable_used_as_body(JSObject *self);
  static void set_readable_used_as_body(JSContext *cx, JS::HandleObject readable,
                                        JS::HandleObject target);
  static JSObject *writable(JSObject *self);
  static bool is_ts_writable(JSContext *cx, JS::HandleObject writable);
  static JSObject *ts_from_writable(JSContext *cx, JS::HandleObject writable);
  static JSObject *controller(JSObject *self);
  static bool backpressure(JSObject *self);
  static JSObject *backpressureChangePromise(JSObject *self);
  static bool used_as_mixin(JSObject *self);
  static void set_used_as_mixin(JSObject *self);
  static bool readable_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool writable_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool Error(JSContext *cx, JS::HandleObject stream, JS::HandleValue error);
  static bool DefaultSourcePullAlgorithm(JSContext *cx, JS::CallArgs args,
                                         JS::HandleObject readable, JS::HandleObject stream,
                                         JS::HandleObject controller);
  static bool DefaultSourceCancelAlgorithm(JSContext *cx, JS::CallArgs args,
                                           JS::HandleObject readable, JS::HandleObject stream,
                                           JS::HandleValue reason);
  static bool default_sink_write_algo_then_handler(JSContext *cx, JS::HandleObject stream,
                                                   JS::HandleValue chunk, JS::CallArgs args);
  static bool DefaultSinkWriteAlgorithm(JSContext *cx, JS::CallArgs args,
                                        JS::HandleObject writableController,
                                        JS::HandleObject stream, JS::HandleValue chunk);
  static bool DefaultSinkAbortAlgorithm(JSContext *cx, JS::CallArgs args,
                                        JS::HandleObject writableController,
                                        JS::HandleObject stream, JS::HandleValue reason);
  static bool default_sink_close_algo_then_handler(JSContext *cx, JS::HandleObject stream,
                                                   JS::HandleValue extra, JS::CallArgs args);
  static bool default_sink_close_algo_catch_handler(JSContext *cx, JS::HandleObject stream,
                                                    JS::HandleValue extra, JS::CallArgs args);
  static bool DefaultSinkCloseAlgorithm(JSContext *cx, JS::CallArgs args,
                                        JS::HandleObject writableController,
                                        JS::HandleObject stream);
  static bool SetBackpressure(JSContext *cx, JS::HandleObject stream, bool backpressure);
  static bool Initialize(JSContext *cx, JS::HandleObject stream, JS::HandleObject startPromise,
                         double writableHighWaterMark, JS::HandleFunction writableSizeAlgorithm,
                         double readableHighWaterMark, JS::HandleFunction readableSizeAlgorithm);
  static bool ErrorWritableAndUnblockWrite(JSContext *cx, JS::HandleObject stream,
                                           JS::HandleValue error);
  static JSObject *create(JSContext *cx, JS::HandleObject self, double writableHighWaterMark,
                          JS::HandleFunction writableSizeAlgorithm, double readableHighWaterMark,
                          JS::HandleFunction readableSizeAlgorithm, JS::HandleValue transformer,
                          JS::HandleObject startFunction, JS::HandleObject transformFunction,
                          JS::HandleObject flushFunction);
  static JSObject *create(JSContext *cx, double writableHighWaterMark,
                          JS::HandleFunction writableSizeAlgorithm, double readableHighWaterMark,
                          JS::HandleFunction readableSizeAlgorithm, JS::HandleValue transformer,
                          JS::HandleObject startFunction, JS::HandleObject transformFunction,
                          JS::HandleObject flushFunction);
  static JSObject *create_rs_proxy(JSContext *cx, JS::HandleObject input_readable);
};

} // namespace builtins

#endif
