// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/experimental/TypedData.h" // used in "js/Conversions.h"
#pragma clang diagnostic pop

#include "js/Stream.h"

#include "builtin.h"
#include "builtins/transform-stream-default-controller.h"
#include "builtins/transform-stream.h"

/**
 * Implementation of the WHATWG TransformStream builtin.
 *
 * All algorithm names and steps refer to spec algorithms defined at
 * https://streams.spec.whatwg.org/#ts-default-controller-class
 */
// A JS class to use as the underlying sink for native writable streams, used
// for TransformStream.
namespace builtins {
JSObject *TransformStreamDefaultController::stream(JSObject *controller) {
  MOZ_ASSERT(is_instance(controller));
  return &JS::GetReservedSlot(controller, Slots::Stream).toObject();
}

TransformStreamDefaultController::TransformAlgorithmImplementation *
TransformStreamDefaultController::transformAlgorithm(JSObject *controller) {
  MOZ_ASSERT(is_instance(controller));
  return (TransformAlgorithmImplementation *)JS::GetReservedSlot(controller,
                                                                 Slots::TransformAlgorithm)
      .toPrivate();
}

TransformStreamDefaultController::FlushAlgorithmImplementation *
TransformStreamDefaultController::flushAlgorithm(JSObject *controller) {
  MOZ_ASSERT(is_instance(controller));
  return (FlushAlgorithmImplementation *)JS::GetReservedSlot(controller, Slots::FlushAlgorithm)
      .toPrivate();
}

/**
 * https://streams.spec.whatwg.org/#ts-default-controller-desired-size
 */
bool TransformStreamDefaultController::desiredSize_get(JSContext *cx, unsigned argc,
                                                       JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "get desiredSize")

  // 1.  Let readableController be [this].[stream].[readable].[controller].
  JSObject *stream = TransformStreamDefaultController::stream(self);
  JSObject *readable = TransformStream::readable(stream);
  double value;
  bool has_value;
  if (!JS::ReadableStreamGetDesiredSize(cx, readable, &has_value, &value)) {
    return false;
  }

  if (!has_value) {
    args.rval().setNull();
  } else {
    args.rval().set(JS_NumberValue(value));
  }

  return true;
}

/**
 * https://streams.spec.whatwg.org/#ts-default-controller-enqueue
 */
bool TransformStreamDefaultController::enqueue_js(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "enqueue")

  // 1.  Perform TransformStreamDefaultControllerEnqueue([this], chunk).
  if (!Enqueue(cx, self, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/**
 * https://streams.spec.whatwg.org/#ts-default-controller-error
 */
bool TransformStreamDefaultController::error_js(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "error")

  // 1.  Perform TransformStreamDefaultControllerError(this, e).
  // (inlined)
  JS::RootedObject stream(cx, TransformStreamDefaultController::stream(self));

  if (!TransformStream::Error(cx, stream, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/**
 * https://streams.spec.whatwg.org/#ts-default-controller-terminate
 */
bool TransformStreamDefaultController::terminate_js(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "terminate")

  // 1.  Perform TransformStreamDefaultControllerTerminate(this).
  if (!Terminate(cx, self)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

const JSFunctionSpec TransformStreamDefaultController::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec TransformStreamDefaultController::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec TransformStreamDefaultController::methods[] = {
    JS_FN("enqueue", enqueue_js, 1, JSPROP_ENUMERATE),
    JS_FN("error", error_js, 1, JSPROP_ENUMERATE),
    JS_FN("terminate", terminate_js, 0, JSPROP_ENUMERATE), JS_FS_END};

const JSPropertySpec TransformStreamDefaultController::properties[] = {
    JS_PSG("desiredSize", desiredSize_get, JSPROP_ENUMERATE), JS_PS_END};

JSObject *TransformStreamDefaultController::create(
    JSContext *cx, JS::HandleObject stream,
    TransformStreamDefaultController::TransformAlgorithmImplementation *transformAlgo,
    TransformStreamDefaultController::FlushAlgorithmImplementation *flushAlgo) {
  JS::RootedObject controller(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!controller)
    return nullptr;

  // 1.  Assert: stream [implements] `[TransformStream]`.
  MOZ_ASSERT(TransformStream::is_instance(stream));

  // 2.  Assert: stream.[controller] is undefined.
  MOZ_ASSERT(JS::GetReservedSlot(stream, TransformStream::Slots::Controller).isUndefined());

  // 3.  Set controller.[stream] to stream.
  JS::SetReservedSlot(controller, Slots::Stream, JS::ObjectValue(*stream));

  // 4.  Set stream.[controller] to controller.
  JS::SetReservedSlot(stream, TransformStream::Slots::Controller, JS::ObjectValue(*controller));

  // 5.  Set controller.[transformAlgorithm] to transformAlgorithm.
  JS::SetReservedSlot(controller, Slots::TransformAlgorithm,
                      JS::PrivateValue((void *)transformAlgo));

  // 6.  Set controller.[flushAlgorithm] to flushAlgorithm.
  JS::SetReservedSlot(controller, Slots::FlushAlgorithm, JS::PrivateValue((void *)flushAlgo));

  return controller;
}

void TransformStreamDefaultController::set_transformer(JSObject *controller, JS::Value transformer,
                                                       JSObject *transformFunction,
                                                       JSObject *flushFunction) {
  JS::SetReservedSlot(controller, Slots::Transformer, transformer);
  JS::SetReservedSlot(controller, Slots::TransformInput, JS::ObjectOrNullValue(transformFunction));
  JS::SetReservedSlot(controller, Slots::FlushInput, JS::ObjectOrNullValue(flushFunction));
}

/**
 * TransformStreamDefaultControllerEnqueue
 */
bool TransformStreamDefaultController::Enqueue(JSContext *cx, JS::HandleObject controller,
                                               JS::HandleValue chunk) {
  MOZ_ASSERT(is_instance(controller));

  // 1.  Let stream be controller.[stream].
  JS::RootedObject stream(cx, TransformStreamDefaultController::stream(controller));

  // 2.  Let readableController be stream.[readable].[controller].
  JS::RootedObject readable(cx, TransformStream::readable(stream));
  JS::RootedObject readableController(cx, JS::ReadableStreamGetController(cx, readable));
  MOZ_ASSERT(readableController);

  // 3.  If !
  // [ReadableStreamDefaultControllerCanCloseOrEnqueue](readableController) is
  // false, throw a `TypeError` exception.
  if (!JS::CheckReadableStreamControllerCanCloseOrEnqueue(cx, readableController, "enqueue")) {
    return false;
  }

  // 4.  Let enqueueResult be
  // ReadableStreamDefaultControllerEnqueue(readableController, chunk).
  bool enqueueResult = JS::ReadableStreamEnqueue(cx, readable, chunk);

  // 5.  If enqueueResult is an abrupt completion,
  if (!enqueueResult) {
    // 5.1.  Perform
    // TransformStreamErrorWritableAndUnblockWrite(stream,
    // enqueueResult.[Value]).
    JS::RootedValue resultValue(cx);
    if (!JS_GetPendingException(cx, &resultValue)) {
      return false;
    }
    JS_ClearPendingException(cx);

    if (!TransformStream::ErrorWritableAndUnblockWrite(cx, stream, resultValue)) {
      return false;
    }

    //     2.  Throw stream.[readable].[storedError].
    JS::RootedValue storedError(cx, JS::ReadableStreamGetStoredError(cx, readable));
    JS_SetPendingException(cx, storedError);
    return false;
  }

  // 6.  Let backpressure be
  // ReadableStreamDefaultControllerHasBackpressure(readableController).
  // (Inlined)
  bool backpressure = !JS::ReadableStreamControllerShouldCallPull(cx, readableController);

  // 7.  If backpressure is not stream.[backpressure],
  if (backpressure != TransformStream::backpressure(stream)) {
    //     1.  Assert: backpressure is true.
    MOZ_ASSERT(backpressure);

    //     2.  Perform ! TransformStreamSetBackpressure(stream, true).
    TransformStream::SetBackpressure(cx, stream, true);
  }

  return true;
}

/**
 * TransformStreamDefaultControllerTerminate
 */
bool TransformStreamDefaultController::Terminate(JSContext *cx, JS::HandleObject controller) {
  MOZ_ASSERT(is_instance(controller));

  // 1.  Let stream be controller.[stream].
  JS::RootedObject stream(cx, TransformStreamDefaultController::stream(controller));

  // 2.  Let readableController be stream.[readable].[controller].
  JS::RootedObject readable(cx, TransformStream::readable(stream));
  JS::RootedObject readableController(cx, JS::ReadableStreamGetController(cx, readable));
  MOZ_ASSERT(readableController);

  // 3.  Perform ! [ReadableStreamDefaultControllerClose](readableController).
  // Note: in https://github.com/whatwg/streams/pull/1029, the spec was changed
  // to make ReadableStreamDefaultControllerClose (and -Enqueue) return early if
  // ReadableStreamDefaultControllerCanCloseOrEnqueue is false. SpiderMonkey
  // hasn't been updated accordingly, so it'll throw an exception instead. To
  // avoid that, we do the check explicitly. While that also throws an
  // exception, we can just clear it and move on. Note that this is
  // future-proof: if SpiderMonkey is updated accordingly, it'll simply stop
  // throwing an exception, and the `else` branch will never be taken.
  if (JS::CheckReadableStreamControllerCanCloseOrEnqueue(cx, readableController, "close")) {
    if (!JS::ReadableStreamClose(cx, readable)) {
      return false;
    }
  } else {
    JS_ClearPendingException(cx);
  }

  // 4.  Let error be a
  // `[TypeError](https://tc39.es/ecma262/#sec-native-error-types-used-in-this-standard-typeerror)`
  // exception indicating that the stream has been terminated. JSAPI doesn't
  // allow us to create a proper error object with the right stack and all
  // without actually throwing it. So we do that and then immediately clear the
  // pending exception.
  JS::RootedValue error(cx);
  JS_ReportErrorLatin1(cx, "The TransformStream has been terminated");
  if (!JS_GetPendingException(cx, &error)) {
    return false;
  }
  JS_ClearPendingException(cx);

  // 5.  Perform ! [TransformStreamErrorWritableAndUnblockWrite](stream, error).
  return TransformStream::ErrorWritableAndUnblockWrite(cx, stream, error);
}

/**
 * Invoke the given callback in a way that treats it as a WebIDL callback
 * returning `Promise<undefined>`, by first calling it and then running step 14
 * of <invoke a callback function> and the conversion step from
 * https://webidl.spec.whatwg.org/#es-promise on the completion value.
 */
JSObject *TransformStreamDefaultController::InvokePromiseReturningCallback(
    JSContext *cx, JS::HandleValue receiver, JS::HandleValue callback, JS::HandleValueArray args) {
  JS::RootedValue rval(cx);
  if (!JS::Call(cx, receiver, callback, args, &rval)) {
    return PromiseRejectedWithPendingError(cx);
  }

  return JS::CallOriginalPromiseResolve(cx, rval);
}

/**
 * The TransformerAlgorithm to use for TransformStreams created using the JS
 * constructor, with or without a `transformer` passed in.
 *
 * Steps 2.* and 4 of SetUpTransformStreamDefaultControllerFromTransformer.
 */
JSObject *TransformStreamDefaultController::transform_algorithm_transformer(
    JSContext *cx, JS::HandleObject controller, JS::HandleValue chunk) {
  MOZ_ASSERT(is_instance(controller));

  // Step 2.  Let transformAlgorithm be the following steps, taking a chunk
  // argument:
  JS::RootedValue transformFunction(cx);
  transformFunction = JS::GetReservedSlot(controller, Slots::TransformInput);
  if (!transformFunction.isObject()) {
    // 2.1.  Let result be TransformStreamDefaultControllerEnqueue(controller,
    // chunk).
    if (!Enqueue(cx, controller, chunk)) {
      // 2.2.  If result is an abrupt completion, return a promise rejected with
      // result.[Value].
      return PromiseRejectedWithPendingError(cx);
    }

    // 2.3.  Otherwise, return a promise resolved with undefined.
    return JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue);
  }

  // Step 4.  If transformerDict[transform] exists, set transformAlgorithm to an
  // algorithm which takes an argument chunk and returns the result of invoking
  // transformerDict[transform] with argument list « chunk, controller » and
  // callback this value transformer.
  JS::RootedValue transformer(cx, JS::GetReservedSlot(controller, Slots::Transformer));
  JS::RootedValueArray<2> newArgs(cx);
  newArgs[0].set(chunk);
  newArgs[1].setObject(*controller);
  return InvokePromiseReturningCallback(cx, transformer, transformFunction, newArgs);
}

/**
 * The FlushAlgorithm to use for TransformStreams created using the JS
 * constructor, with or without a `transformer` passed in.
 *
 * Steps 3 and 5 of SetUpTransformStreamDefaultControllerFromTransformer.
 */
JSObject *
TransformStreamDefaultController::flush_algorithm_transformer(JSContext *cx,
                                                              JS::HandleObject controller) {
  MOZ_ASSERT(is_instance(controller));

  // Step 3.  Let flushAlgorithm be an algorithm which returns a promise
  // resolved with undefined.
  JS::RootedValue flushFunction(cx, JS::GetReservedSlot(controller, Slots::FlushInput));
  if (!flushFunction.isObject()) {
    return JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue);
  }

  // Step 5.  If transformerDict[flush] exists, set flushAlgorithm to an
  // algorithm which returns the result of invoking transformerDict[flush] with
  // argument list « controller » and callback this value transformer.
  JS::RootedValue transformer(cx, JS::GetReservedSlot(controller, Slots::Transformer));
  JS::RootedValueArray<1> newArgs(cx);
  newArgs[0].setObject(*controller);
  return InvokePromiseReturningCallback(cx, transformer, flushFunction, newArgs);
}

/**
 * SetUpTransformStreamDefaultController
 * https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller
 */
JSObject *TransformStreamDefaultController::SetUp(JSContext *cx, JS::HandleObject stream,
                                                  TransformAlgorithmImplementation *transformAlgo,
                                                  FlushAlgorithmImplementation *flushAlgo) {
  MOZ_ASSERT(TransformStream::is_instance(stream));

  // Step 1 of SetUpTransformStreamDefaultControllerFromTransformer and step 1-6
  // of this algorithm.
  JS::RootedObject controller(cx);
  controller = TransformStreamDefaultController::create(cx, stream, transformAlgo, flushAlgo);
  return controller;
}

/**
 * SetUpTransformStreamDefaultControllerFromTransformer
 * https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
 */
JSObject *TransformStreamDefaultController::SetUpFromTransformer(JSContext *cx,
                                                                 JS::HandleObject stream,
                                                                 JS::HandleValue transformer,
                                                                 JS::HandleObject transformFunction,
                                                                 JS::HandleObject flushFunction) {
  MOZ_ASSERT(TransformStream::is_instance(stream));

  // Step 1, moved into SetUpTransformStreamDefaultController.
  // Step 6.  Perform ! [SetUpTransformStreamDefaultController](stream,
  // controller, transformAlgorithm, flushAlgorithm).
  JS::RootedObject controller(cx);
  controller = SetUp(cx, stream, transform_algorithm_transformer, flush_algorithm_transformer);
  if (!controller)
    return nullptr;

  // Set the additional bits required to execute the transformer-based transform
  // and flush algorithms.
  set_transformer(controller, transformer, transformFunction, flushFunction);

  // Steps 2-5 implemented in dedicated functions above.
  return controller;
}

/**
 * Steps 2.* of TransformStreamDefaultControllerPerformTransform.
 */
bool TransformStreamDefaultController::transformPromise_catch_handler(JSContext *cx,
                                                                      JS::HandleObject controller,
                                                                      JS::HandleValue extra,
                                                                      JS::CallArgs args) {
  JS::RootedValue r(cx, args.get(0));
  //     1.  Perform ! [TransformStreamError](controller.[stream], r).
  JS::RootedObject streamObj(cx, stream(controller));
  if (!TransformStream::Error(cx, streamObj, r)) {
    return false;
  }

  //     2.  Throw r.
  JS_SetPendingException(cx, r);
  return false;
}

/**
 * TransformStreamDefaultControllerPerformTransform
 */
JSObject *TransformStreamDefaultController::PerformTransform(JSContext *cx,
                                                             JS::HandleObject controller,
                                                             JS::HandleValue chunk) {
  MOZ_ASSERT(is_instance(controller));

  // 1.  Let transformPromise be the result of performing
  // controller.[transformAlgorithm], passing chunk.
  TransformAlgorithmImplementation *transformAlgo = transformAlgorithm(controller);
  JS::RootedObject transformPromise(cx, transformAlgo(cx, controller, chunk));
  if (!transformPromise) {
    return nullptr;
  }

  // 2.  Return the result of reacting to transformPromise with the following
  // rejection steps given the argument r:
  JS::RootedObject catch_handler(cx);
  catch_handler = create_internal_method<transformPromise_catch_handler>(cx, controller);
  if (!catch_handler) {
    return nullptr;
  }

  return JS::CallOriginalPromiseThen(cx, transformPromise, nullptr, catch_handler);
}

/**
 * TransformStreamDefaultControllerClearAlgorithms
 */
void TransformStreamDefaultController::ClearAlgorithms(JSObject *controller) {
  MOZ_ASSERT(is_instance(controller));

  // 1.  Set controller.[transformAlgorithm] to undefined.
  JS::SetReservedSlot(controller, Slots::TransformAlgorithm, JS::PrivateValue(nullptr));
  JS::SetReservedSlot(controller, Slots::TransformInput, JS::UndefinedValue());

  // 2.  Set controller.[flushAlgorithm] to undefined.
  JS::SetReservedSlot(controller, Slots::FlushAlgorithm, JS::PrivateValue(nullptr));
  JS::SetReservedSlot(controller, Slots::FlushInput, JS::UndefinedValue());
}
} // namespace builtins
