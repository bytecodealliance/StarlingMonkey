#ifndef JS_COMPUTE_RUNTIME_TRANSFORM_STREAM_DEFAULT_CONTROLLER_H
#define JS_COMPUTE_RUNTIME_TRANSFORM_STREAM_DEFAULT_CONTROLLER_H

#include "builtin.h"
#include "js-compute-builtins.h"

namespace builtins {

class TransformStreamDefaultController
    : public BuiltinNoConstructor<TransformStreamDefaultController> {
private:
public:
  static constexpr const char *class_name = "TransformStreamDefaultController";

  enum Slots {
    Stream,
    Transformer,
    TransformAlgorithm,
    TransformInput, // JS::Value to be used by TransformAlgorithm, e.g. a
                    // JSFunction to call.
    FlushAlgorithm,
    FlushInput, // JS::Value to be used by FlushAlgorithm, e.g. a JSFunction to
                // call.
    Count
  };

  typedef JSObject *TransformAlgorithmImplementation(JSContext *cx, JS::HandleObject controller,
                                                     JS::HandleValue chunk);
  typedef JSObject *FlushAlgorithmImplementation(JSContext *cx, JS::HandleObject controller);

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool Enqueue(JSContext *cx, JS::HandleObject controller, JS::HandleValue chunk);
  static bool Terminate(JSContext *cx, JS::HandleObject controller);

  static JSObject *stream(JSObject *controller);
  static TransformAlgorithmImplementation *transformAlgorithm(JSObject *controller);
  static FlushAlgorithmImplementation *flushAlgorithm(JSObject *controller);
  static bool desiredSize_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool enqueue_js(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool error_js(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool terminate_js(JSContext *cx, unsigned argc, JS::Value *vp);
  static JSObject *create(JSContext *cx, JS::HandleObject stream,
                          TransformAlgorithmImplementation *transformAlgo,
                          FlushAlgorithmImplementation *flushAlgo);
  static void set_transformer(JSObject *controller, JS::Value transformer,
                              JSObject *transformFunction, JSObject *flushFunction);
  static JSObject *InvokePromiseReturningCallback(JSContext *cx, JS::HandleValue receiver,
                                                  JS::HandleValue callback,
                                                  JS::HandleValueArray args);
  static JSObject *transform_algorithm_transformer(JSContext *cx, JS::HandleObject controller,
                                                   JS::HandleValue chunk);
  static JSObject *flush_algorithm_transformer(JSContext *cx, JS::HandleObject controller);
  static JSObject *SetUp(JSContext *cx, JS::HandleObject stream,
                         TransformAlgorithmImplementation *transformAlgo,
                         FlushAlgorithmImplementation *flushAlgo);
  static JSObject *SetUpFromTransformer(JSContext *cx, JS::HandleObject stream,
                                        JS::HandleValue transformer,
                                        JS::HandleObject transformFunction,
                                        JS::HandleObject flushFunction);
  static bool transformPromise_catch_handler(JSContext *cx, JS::HandleObject controller,
                                             JS::HandleValue extra, JS::CallArgs args);
  static JSObject *PerformTransform(JSContext *cx, JS::HandleObject controller,
                                    JS::HandleValue chunk);
  static void ClearAlgorithms(JSObject *controller);
};

} // namespace builtins

#endif
