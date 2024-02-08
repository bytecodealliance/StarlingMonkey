// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/experimental/TypedData.h" // used in "js/Conversions.h"
#pragma clang diagnostic pop

#include "js/Conversions.h"
#include "js/Stream.h"

#include "builtins/native-stream-sink.h"
#include "builtins/native-stream-source.h"
#include "builtins/request-response.h"
#include "builtins/transform-stream-default-controller.h"
#include "builtins/transform-stream.h"

namespace ReadableStream_additions {
static JS::PersistentRooted<JSObject *> proto_obj;

bool is_instance(JSObject *obj) { return JS::IsReadableStream(obj); }

bool is_instance(JS::Value val) { return val.isObject() && is_instance(&val.toObject()); }

bool check_receiver(JSContext *cx, JS::HandleValue receiver, const char *method_name) {
  if (!is_instance(receiver)) {
    JS_ReportErrorUTF8(cx, "Method %s called on receiver that's not an instance of ReadableStream",
                       method_name);
    return false;
  }
  return true;
};

static JS::PersistentRooted<JS::Value> original_pipeTo;
static JS::PersistentRooted<JS::Value> overridden_pipeTo;

bool pipeTo(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  // If the receiver is backed by a native source and the destination is the
  // writable end of a TransformStream, set the TransformStream as the owner of
  // the receiver's source. This enables us to shortcut operations later on.
  JS::RootedObject target(cx, args[0].isObject() ? &args[0].toObject() : nullptr);
  if (target && builtins::NativeStreamSource::stream_has_native_source(cx, self) &&
      JS::IsWritableStream(target) && builtins::TransformStream::is_ts_writable(cx, target)) {
    if (auto ts = builtins::TransformStream::ts_from_writable(cx, target)) {
      auto streamHasTransformer =
          JS::GetReservedSlot(ts, builtins::TransformStream::Slots::HasTransformer).toBoolean();
      // We only want to apply the optimisation on identity-streams
      if (!streamHasTransformer) {
        builtins::NativeStreamSource::set_stream_piped_to_ts_writable(cx, self, target);
      }
    }
  }

  return JS::Call(cx, args.thisv(), original_pipeTo, JS::HandleValueArray(args), args.rval());
}

bool pipeThrough(JSContext *cx, JS::HandleObject source_readable, JS::HandleObject target_writable,
                 JS::HandleValue options) {
  // 1. If ! IsReadableStreamLocked(this) is true, throw a TypeError exception.
  bool locked;
  if (!JS::ReadableStreamIsLocked(cx, source_readable, &locked)) {
    return false;
  }
  if (locked) {
    JS_ReportErrorLatin1(cx, "pipeThrough called on a ReadableStream that's already locked");
    return false;
  }

  // 2. If ! IsWritableStreamLocked(transform["writable"]) is true, throw a
  // TypeError exception.
  if (JS::WritableStreamIsLocked(cx, target_writable)) {
    JS_ReportErrorLatin1(cx, "The writable end of the transform object passed to pipeThrough "
                             " passed to pipeThrough is already locked");
    return false;
  }

  // 3. Let signal be options["signal"] if it exists, or undefined otherwise.
  // (implicit, see note in step 4.)

  // 4. Let promise be ! ReadableStreamPipeTo(this, transform["writable"],
  // options
  // ["preventClose"], options["preventAbort"], options["preventCancel"],
  // signal). Note: instead of extracting the prevent* flags above, we just pass
  // the |options| argument as-is. pipeTo will fail eagerly if it fails to
  // extract the fields on |options|, so while skipping the extraction above
  // changes the order in which errors are reported and the error messages a
  // bit, it otherwise preserves semantics. In particular, the errors aren't
  // reported as rejected promises, as would be the case for those reported in
  // steps 1 and 2.
  JS::RootedValueArray<2> newArgs(cx);
  newArgs[0].setObject(*target_writable);
  newArgs[1].set(options);
  JS::RootedValue thisv(cx, JS::ObjectValue(*source_readable));
  JS::RootedValue rval(cx);
  if (!JS::Call(cx, thisv, overridden_pipeTo, newArgs, &rval)) {
    return false;
  }

  JS::RootedObject promise(cx, &rval.toObject());
  MOZ_ASSERT(JS::IsPromiseObject(promise));

  // 5. Set promise.[[PromiseIsHandled]] to true.
  // JSAPI doesn't provide a straightforward way to do this, but we can just
  // register null-reactions in a way that achieves it.
  if (!JS::AddPromiseReactionsIgnoringUnhandledRejection(cx, promise, nullptr, nullptr)) {
    return false;
  }

  return true;
}

bool pipeThrough(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  if (!args[0].isObject()) {
    JS_ReportErrorLatin1(cx, "First argument to pipeThrough must be an object");
    return false;
  }

  JS::RootedObject transform(cx, &args[0].toObject());
  JS::RootedObject readable(cx);
  JS::RootedObject writable(cx);

  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, transform, "readable", &val))
    return false;
  if (!val.isObject() || !JS::IsReadableStream(&val.toObject())) {
    JS_ReportErrorLatin1(cx, "First argument to pipeThrough must be an object with a "
                             "|readable| property that is an instance of ReadableStream");
    return false;
  }

  readable = &val.toObject();

  if (!JS_GetProperty(cx, transform, "writable", &val))
    return false;
  if (!val.isObject() || !JS::IsWritableStream(&val.toObject())) {
    JS_ReportErrorLatin1(cx, "First argument to pipeThrough must be an object with a "
                             "|writable| property that is an instance of WritableStream");
    return false;
  }
  writable = &val.toObject();

  if (!pipeThrough(cx, self, writable, args.get(1))) {
    return false;
  }

  // 6. Return transform["readable"].
  args.rval().setObject(*readable);
  return true;
}

bool initialize_additions(JSContext *cx, JS::HandleObject global) {
  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, global, "ReadableStream", &val)) {
    return false;
  }
  JS::RootedObject readableStream_builtin(cx, &val.toObject());

  if (!JS_GetProperty(cx, readableStream_builtin, "prototype", &val)) {
    return false;
  }
  proto_obj.init(cx, &val.toObject());
  MOZ_ASSERT(proto_obj);

  original_pipeTo.init(cx);
  overridden_pipeTo.init(cx);
  if (!JS_GetProperty(cx, proto_obj, "pipeTo", &original_pipeTo)) {
    return false;
  }
  MOZ_ASSERT(JS::IsCallable(&original_pipeTo.toObject()));

  JSFunction *pipeTo_fun = JS_DefineFunction(cx, proto_obj, "pipeTo", pipeTo, 1, JSPROP_ENUMERATE);
  if (!pipeTo_fun) {
    return false;
  }

  overridden_pipeTo.setObject(*JS_GetFunctionObject(pipeTo_fun));

  if (!JS_DefineFunction(cx, proto_obj, "pipeThrough", pipeThrough, 1, JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}
} // namespace ReadableStream_additions

bool ExtractFunction(JSContext *cx, JS::HandleObject obj, const char *name,
                     JS::MutableHandleObject func) {
  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, obj, name, &val)) {
    return false;
  }

  if (val.isUndefined()) {
    return true;
  }

  if (!val.isObject() || !JS::IsCallable(&val.toObject())) {
    JS_ReportErrorLatin1(cx, "%s should be a function", name);
    return false;
  }

  func.set(&val.toObject());
  return true;
}

bool ExtractStrategy(JSContext *cx, JS::HandleValue strategy, double default_hwm, double *hwm,
                     JS::MutableHandleFunction size) {
  if (strategy.isUndefined()) {
    *hwm = default_hwm;
    return true;
  }

  if (!strategy.isObject()) {
    JS_ReportErrorLatin1(cx, "Strategy passed to TransformStream constructor must be an object");
    return false;
  }

  JS::RootedObject strategy_obj(cx, &strategy.toObject());

  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, strategy_obj, "highWaterMark", &val)) {
    return false;
  }

  if (val.isUndefined()) {
    *hwm = default_hwm;
  } else {
    if (!JS::ToNumber(cx, val, hwm)) {
      return false;
    }
    if (std::isnan(*hwm) || *hwm < 0) {
      JS_ReportErrorLatin1(cx, "Invalid value for highWaterMark: %f", *hwm);
      return false;
    }
  }

  JS::RootedObject size_obj(cx);
  if (!ExtractFunction(cx, strategy_obj, "size", &size_obj)) {
    return false;
  }

  // JSAPI wants JSHandleFunction instances for the size algorithm, so that's
  // what it'll get.
  if (size_obj) {
    val.setObjectOrNull(size_obj);
    size.set(JS_ValueToFunction(cx, val));
  }

  return true;
}

/**
 * Implementation of the WHATWG TransformStream builtin.
 *
 * All algorithm names and steps refer to spec algorithms defined at
 * https://streams.spec.whatwg.org/#ts-class
 */
namespace builtins {
/**
 * The native object owning the sink underlying the TransformStream's readable
 * end.
 *
 * This can e.g. be a RequestOrResponse if the readable is used as a body.
 */
JSObject *TransformStream::owner(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, TransformStream::Slots::Owner).toObject();
}

void TransformStream::set_owner(JSObject *self, JSObject *owner) {
  MOZ_ASSERT(is_instance(self));
  MOZ_ASSERT(RequestOrResponse::is_instance(owner));
  MOZ_ASSERT(JS::GetReservedSlot(self, TransformStream::Slots::Owner).isUndefined());
  JS::SetReservedSlot(self, TransformStream::Slots::Owner, JS::ObjectValue(*owner));
}

JSObject *TransformStream::readable(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, TransformStream::Slots::Readable).toObject();
}

bool TransformStream::is_ts_readable(JSContext *cx, JS::HandleObject readable) {
  JSObject *source = builtins::NativeStreamSource::get_stream_source(cx, readable);
  if (!source || !builtins::NativeStreamSource::is_instance(source)) {
    return false;
  }
  JSObject *stream_owner = builtins::NativeStreamSource::owner(source);
  return stream_owner ? TransformStream::is_instance(stream_owner) : false;
}

JSObject *TransformStream::ts_from_readable(JSContext *cx, JS::HandleObject readable) {
  MOZ_ASSERT(is_ts_readable(cx, readable));
  JSObject *source = builtins::NativeStreamSource::get_stream_source(cx, readable);
  return builtins::NativeStreamSource::owner(source);
}

bool TransformStream::readable_used_as_body(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  // For now the owner can only be a RequestOrResponse, so no further checks are
  // needed.
  return JS::GetReservedSlot(self, TransformStream::Slots::Owner).isObject();
}

/**
 * Sets the |target| RequestOrResponse object as the owner of the
 * TransformStream |readable| is the readable end of.
 *
 * This allows us to later on short-cut piping from native body to native body.
 *
 * Asserts that |readable| is the readable end of a TransformStream, and that
 * that TransformStream is not used as a mixin by another builtin.
 */
void TransformStream::set_readable_used_as_body(JSContext *cx, JS::HandleObject readable,
                                                JS::HandleObject target) {
  JS::RootedObject ts(cx, ts_from_readable(cx, readable));
  MOZ_ASSERT(!used_as_mixin(ts));
  set_owner(ts, target);
}

JSObject *TransformStream::writable(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, TransformStream::Slots::Writable).toObject();
}

JSObject *TransformStream::ts_from_writable(JSContext *cx, JS::HandleObject writable) {
  MOZ_ASSERT(is_ts_writable(cx, writable));
  JSObject *sink = builtins::NativeStreamSink::get_stream_sink(cx, writable);
  if (!sink || !builtins::NativeStreamSink::is_instance(sink)) {
    return nullptr;
  }
  return builtins::NativeStreamSink::owner(sink);
}

bool TransformStream::is_ts_writable(JSContext *cx, JS::HandleObject writable) {
  JSObject *sink = builtins::NativeStreamSink::get_stream_sink(cx, writable);
  if (!sink || !builtins::NativeStreamSink::is_instance(sink)) {
    return false;
  }
  JSObject *stream_owner = builtins::NativeStreamSink::owner(sink);
  return stream_owner ? is_instance(stream_owner) : false;
}

JSObject *TransformStream::controller(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, TransformStream::Slots::Controller).toObject();
}

bool TransformStream::backpressure(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, TransformStream::Slots::Backpressure).toBoolean();
}

JSObject *TransformStream::backpressureChangePromise(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, TransformStream::Slots::BackpressureChangePromise)
      .toObjectOrNull();
}

bool TransformStream::used_as_mixin(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, TransformStream::Slots::UsedAsMixin).toBoolean();
}

void TransformStream::set_used_as_mixin(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  JS::SetReservedSlot(self, TransformStream::Slots::UsedAsMixin, JS::TrueValue());
}

bool TransformStream::readable_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "get readable")
  args.rval().setObject(*readable(self));
  return true;
}

bool TransformStream::writable_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "get writable")
  args.rval().setObject(*writable(self));
  return true;
}

const JSFunctionSpec TransformStream::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec TransformStream::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec TransformStream::methods[] = {JS_FS_END};

const JSPropertySpec TransformStream::properties[] = {
    JS_PSG("readable", readable_get, JSPROP_ENUMERATE),
    JS_PSG("writable", writable_get, JSPROP_ENUMERATE), JS_PS_END};

/**
 * https://streams.spec.whatwg.org/#ts-constructor
 */
bool TransformStream::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("TransformStream", 0);

  JS::RootedObject startFunction(cx);
  JS::RootedObject transformFunction(cx);
  JS::RootedObject flushFunction(cx);

  // 1.  If transformer is missing, set it to null.
  JS::RootedValue transformer(cx, args.get(0));
  if (transformer.isUndefined()) {
    transformer.setNull();
  }

  if (transformer.isObject()) {
    JS::RootedObject transformerDict(cx, &transformer.toObject());

    // 2.  Let transformerDict be transformer, [converted to an IDL value] of
    // type `
    //     [Transformer]`.
    // Note: we do the extraction of dict entries manually, because no WebIDL
    // codegen.
    if (!ExtractFunction(cx, transformerDict, "start", &startFunction)) {
      return false;
    }

    if (!ExtractFunction(cx, transformerDict, "transform", &transformFunction)) {
      return false;
    }

    if (!ExtractFunction(cx, transformerDict, "flush", &flushFunction)) {
      return false;
    }

    // 3.  If transformerDict["readableType"] [exists], throw a `[RangeError]`
    // exception.
    bool found;
    if (!JS_HasProperty(cx, transformerDict, "readableType", &found)) {
      return false;
    }
    if (found) {
      JS_ReportErrorLatin1(cx, "transformer.readableType is reserved for future use");
      return false;
    }

    // 4.  If transformerDict["writableType"] [exists], throw a `[RangeError]`
    // exception.
    if (!JS_HasProperty(cx, transformerDict, "writableType", &found)) {
      return false;
    }
    if (found) {
      JS_ReportErrorLatin1(cx, "transformer.writableType is reserved for future use");
      return false;
    }
  }

  // 5.  Let readableHighWaterMark be ? [ExtractHighWaterMark](readableStrategy,
  // 0).
  // 6.  Let readableSizeAlgorithm be !
  // [ExtractSizeAlgorithm](readableStrategy).
  double readableHighWaterMark;
  JS::RootedFunction readableSizeAlgorithm(cx);
  if (!ExtractStrategy(cx, args.get(2), 0, &readableHighWaterMark, &readableSizeAlgorithm)) {
    return false;
  }

  // 7.  Let writableHighWaterMark be ? [ExtractHighWaterMark](writableStrategy,
  // 1).
  // 8.  Let writableSizeAlgorithm be !
  // [ExtractSizeAlgorithm](writableStrategy).
  double writableHighWaterMark;
  JS::RootedFunction writableSizeAlgorithm(cx);
  if (!ExtractStrategy(cx, args.get(1), 1, &writableHighWaterMark, &writableSizeAlgorithm)) {
    return false;
  }

  // Steps 9-13.
  JS::RootedObject transformStreamInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  JS::RootedObject self(cx,
                        create(cx, transformStreamInstance, writableHighWaterMark,
                               writableSizeAlgorithm, readableHighWaterMark, readableSizeAlgorithm,
                               transformer, startFunction, transformFunction, flushFunction));
  if (!self)
    return false;

  JS::SetReservedSlot(self, TransformStream::Slots::HasTransformer,
                      JS::BooleanValue(JS::ToBoolean(transformer)));

  args.rval().setObject(*self);
  return true;
}

bool TransformStream::init_class(JSContext *cx, JS::HandleObject global) {
  bool ok = init_class_impl(cx, global);
  if (!ok)
    return false;

  return ReadableStream_additions::initialize_additions(cx, global);
}

/**
 * TransformStreamError
 */
bool TransformStream::Error(JSContext *cx, JS::HandleObject stream, JS::HandleValue error) {
  MOZ_ASSERT(is_instance(stream));

  // 1.  Perform
  // ReadableStreamDefaultControllerError(stream.[readable].[controller], e).
  JS::RootedObject readable(cx, TransformStream::readable(stream));
  if (!JS::ReadableStreamError(cx, readable, error)) {
    return false;
  }

  // 2.  Perform ! [TransformStreamErrorWritableAndUnblockWrite](stream, e).
  return ErrorWritableAndUnblockWrite(cx, stream, error);
}

/**
 * TransformStreamDefaultSourcePullAlgorithm
 */
bool TransformStream::DefaultSourcePullAlgorithm(JSContext *cx, JS::CallArgs args,
                                                 JS::HandleObject readable, JS::HandleObject stream,
                                                 JS::HandleObject controller) {
  // 1.  Assert: stream.[backpressure] is true.
  MOZ_ASSERT(backpressure(stream));

  // 2.  Assert: stream.[backpressureChangePromise] is not undefined.
  MOZ_ASSERT(backpressureChangePromise(stream));

  // 3.  Perform ! [TransformStreamSetBackpressure](stream, false).
  if (!SetBackpressure(cx, stream, false)) {
    return false;
  }

  // 4.  Return stream.[backpressureChangePromise].
  args.rval().setObject(*backpressureChangePromise(stream));
  return true;
}

/**
 * Steps 7.* of InitializeTransformStream
 */
bool TransformStream::DefaultSourceCancelAlgorithm(JSContext *cx, JS::CallArgs args,
                                                   JS::HandleObject readable,
                                                   JS::HandleObject stream,
                                                   JS::HandleValue reason) {
  MOZ_ASSERT(is_instance(stream));

  // 1.  Perform ! [TransformStreamErrorWritableAndUnblockWrite](stream,
  // reason).
  if (!ErrorWritableAndUnblockWrite(cx, stream, reason)) {
    return false;
  }

  // 2.  Return [a promise resolved with] undefined.
  args.rval().setUndefined();
  return true;
}

/**
 * Steps 2 and 3.* of DefaultSinkWriteAlgorithm.
 */
bool TransformStream::default_sink_write_algo_then_handler(JSContext *cx, JS::HandleObject stream,
                                                           JS::HandleValue chunk,
                                                           JS::CallArgs args) {
  // 3.1.  Let writable be stream.[writable].
  JS::RootedObject writable(cx, TransformStream::writable(stream));

  // 3.2.  Let state be writable.[state].
  auto state = JS::WritableStreamGetState(cx, writable);

  // 3.3.  If state is "`erroring`", throw writable.[storedError].
  if (state == JS::WritableStreamState::Erroring) {
    JS::RootedValue storedError(cx, JS::WritableStreamGetStoredError(cx, writable));
    JS_SetPendingException(cx, storedError);
    return false;
  }

  // 3.4.  Assert: state is "`writable`".
  MOZ_ASSERT(state == JS::WritableStreamState::Writable);

  // 2.  Let controller be stream.[controller].
  JS::RootedObject controller(cx, TransformStream::controller(stream));

  // 3.5.  Return TransformStreamDefaultControllerPerformTransform(controller,
  // chunk).
  JS::RootedObject transformPromise(cx);
  transformPromise =
      builtins::TransformStreamDefaultController::PerformTransform(cx, controller, chunk);
  if (!transformPromise) {
    return false;
  }

  args.rval().setObject(*transformPromise);
  return true;
}

bool TransformStream::DefaultSinkWriteAlgorithm(JSContext *cx, JS::CallArgs args,
                                                JS::HandleObject writableController,
                                                JS::HandleObject stream, JS::HandleValue chunk) {
  JS::RootedObject writable(cx, TransformStream::writable(stream));

  // 1.  Assert: stream.[writable].[state] is "`writable`".
  MOZ_ASSERT(JS::WritableStreamGetState(cx, writable) == JS::WritableStreamState::Writable);

  // 2. (reordered below)

  // 3.  If stream.[backpressure] is true,
  if (TransformStream::backpressure(stream)) {
    //     1.  Let backpressureChangePromise be
    //     stream.[backpressureChangePromise].
    JS::RootedObject changePromise(cx, TransformStream::backpressureChangePromise(stream));

    //     2.  Assert: backpressureChangePromise is not undefined.
    MOZ_ASSERT(changePromise);

    //     3.  Return the result of [reacting] to backpressureChangePromise with
    //     the following
    //         fulfillment steps:
    JS::RootedObject then_handler(cx);
    then_handler = create_internal_method<default_sink_write_algo_then_handler>(cx, stream, chunk);
    if (!then_handler)
      return false;

    JS::RootedObject result(cx);
    result = JS::CallOriginalPromiseThen(cx, changePromise, then_handler, nullptr);
    if (!result) {
      return false;
    }

    args.rval().setObject(*result);
    return true;
  }

  // 2.  Let controller be stream.[controller].
  JS::RootedObject controller(cx, TransformStream::controller(stream));

  // 4.  Return ! [TransformStreamDefaultControllerPerformTransform](controller,
  // chunk).
  JS::RootedObject transformPromise(cx);
  transformPromise =
      builtins::TransformStreamDefaultController::PerformTransform(cx, controller, chunk);
  if (!transformPromise) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  args.rval().setObject(*transformPromise);
  return true;
}

bool TransformStream::DefaultSinkAbortAlgorithm(JSContext *cx, JS::CallArgs args,
                                                JS::HandleObject writableController,
                                                JS::HandleObject stream, JS::HandleValue reason) {
  MOZ_ASSERT(is_instance(stream));

  // 1.  Perform ! [TransformStreamError](stream, reason).
  if (!Error(cx, stream, reason)) {
    return false;
  }

  // 2.  Return [a promise resolved with] undefined.
  JS::RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  if (!promise) {
    return false;
  }

  args.rval().setObject(*promise);
  return true;
}

/**
 * Steps 5.1.1-2 of DefaultSinkCloseAlgorithm.
 */
bool TransformStream::default_sink_close_algo_then_handler(JSContext *cx, JS::HandleObject stream,
                                                           JS::HandleValue extra,
                                                           JS::CallArgs args) {
  // 5.1.1.  If readable.[state] is "`errored`", throw readable.[storedError].
  JS::RootedObject readable(cx, &extra.toObject());
  bool is_errored;
  if (!JS::ReadableStreamIsErrored(cx, readable, &is_errored)) {
    return false;
  }

  if (is_errored) {
    JS::RootedValue storedError(cx, JS::ReadableStreamGetStoredError(cx, readable));
    JS_SetPendingException(cx, storedError);
    return false;
  }

  // 5.1.2.  Perform !
  // [ReadableStreamDefaultControllerClose](readable.[controller]).
  JS::RootedObject readableController(cx, JS::ReadableStreamGetController(cx, readable));
  // Note: in https://github.com/whatwg/streams/pull/1029, the spec was changed
  // to make ReadableStreamDefaultControllerClose (and -Enqueue) return early if
  // ReadableStreamDefaultControllerCanCloseOrEnqueue is false. SpiderMonkey
  // hasn't been updated accordingly, so it'll throw an exception instead. To
  // avoid that, we do the check explicitly. While that also throws an
  // exception, we can just clear it and move on. Note that this is
  // future-proof: if SpiderMonkey is updated accordingly, it'll simply stop
  // throwing an exception, and the `else` branch will never be taken.
  if (JS::CheckReadableStreamControllerCanCloseOrEnqueue(cx, readableController, "close")) {
    return JS::ReadableStreamClose(cx, readable);
  } else {
    JS_ClearPendingException(cx);
  }
  return true;
}

/**
 * Steps 5.2.1-2 of DefaultSinkCloseAlgorithm.
 */
bool TransformStream::default_sink_close_algo_catch_handler(JSContext *cx, JS::HandleObject stream,
                                                            JS::HandleValue extra,
                                                            JS::CallArgs args) {
  // 5.2.1.  Perform ! [TransformStreamError](stream, r).
  JS::HandleValue r = args.get(0);
  if (!Error(cx, stream, r)) {
    return false;
  }

  // 5.2.2.  Throw readable.[storedError].
  JS::RootedObject readable(cx, &extra.toObject());
  JS::RootedValue storedError(cx, JS::ReadableStreamGetStoredError(cx, readable));
  JS_SetPendingException(cx, storedError);
  return false;
}

bool TransformStream::DefaultSinkCloseAlgorithm(JSContext *cx, JS::CallArgs args,
                                                JS::HandleObject writableController,
                                                JS::HandleObject stream) {
  MOZ_ASSERT(is_instance(stream));

  // 1.  Let readable be stream.[readable].
  JS::RootedObject readable(cx, TransformStream::readable(stream));

  // 2.  Let controller be stream.[controller].
  JS::RootedObject controller(cx, TransformStream::controller(stream));

  // 3.  Let flushPromise be the result of performing
  // controller.[flushAlgorithm].
  auto flushAlgorithm = builtins::TransformStreamDefaultController::flushAlgorithm(controller);
  JS::RootedObject flushPromise(cx, flushAlgorithm(cx, controller));
  if (!flushPromise) {
    return false;
  }

  // 4.  Perform !
  // [TransformStreamDefaultControllerClearAlgorithms](controller).
  builtins::TransformStreamDefaultController::ClearAlgorithms(controller);

  // 5.  Return the result of [reacting] to flushPromise:
  // 5.1.  If flushPromise was fulfilled, then:
  // Sub-steps in handler above.
  JS::RootedObject then_handler(cx);
  JS::RootedValue extra(cx, JS::ObjectValue(*readable));
  then_handler = create_internal_method<default_sink_close_algo_then_handler>(cx, stream, extra);
  if (!then_handler)
    return false;

  // 5.2.  If flushPromise was rejected with reason r, then:
  // Sub-steps in handler above.
  JS::RootedObject catch_handler(cx);
  catch_handler = create_internal_method<default_sink_close_algo_catch_handler>(cx, stream, extra);
  if (!catch_handler)
    return false;

  JS::RootedObject result(cx);
  result = JS::CallOriginalPromiseThen(cx, flushPromise, then_handler, catch_handler);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * TransformStreamSetBackpressure
 */
bool TransformStream::SetBackpressure(JSContext *cx, JS::HandleObject stream, bool backpressure) {
  // 1.  Assert: stream.[backpressure] is not backpressure.
  MOZ_ASSERT(TransformStream::backpressure(stream) != backpressure);

  // 2.  If stream.[backpressureChangePromise] is not undefined, resolve
  // stream.[backpressureChangePromise] with undefined.
  JS::RootedObject changePromise(cx, backpressureChangePromise(stream));
  if (changePromise) {
    if (!JS::ResolvePromise(cx, changePromise, JS::UndefinedHandleValue)) {
      return false;
    }
  }

  // 3.  Set stream.[backpressureChangePromise] to a new promise.
  changePromise = JS::NewPromiseObject(cx, nullptr);
  if (!changePromise) {
    return false;
  }
  JS::SetReservedSlot(stream, TransformStream::Slots::BackpressureChangePromise,
                      JS::ObjectValue(*changePromise));

  // 4.  Set stream.[backpressure] to backpressure.
  JS::SetReservedSlot(stream, TransformStream::Slots::Backpressure, JS::BooleanValue(backpressure));

  return true;
}

/**
 * https://streams.spec.whatwg.org/#initialize-transform-stream
 * Steps 9-13.
 */
bool TransformStream::Initialize(JSContext *cx, JS::HandleObject stream,
                                 JS::HandleObject startPromise, double writableHighWaterMark,
                                 JS::HandleFunction writableSizeAlgorithm,
                                 double readableHighWaterMark,
                                 JS::HandleFunction readableSizeAlgorithm) {
  // Step 1.  Let startAlgorithm be an algorithm that returns startPromise.
  // (Inlined)

  // Steps 2-4 implemented as DefaultSink*Algorithm functions above.

  // Step 5.  Set stream.[writable] to ! [CreateWritableStream](startAlgorithm,
  // writeAlgorithm, closeAlgorithm, abortAlgorithm, writableHighWaterMark,
  // writableSizeAlgorithm).
  JS::RootedValue startPromiseVal(cx, JS::ObjectValue(*startPromise));
  JS::RootedObject sink(
      cx, builtins::NativeStreamSink::create(cx, stream, startPromiseVal, DefaultSinkWriteAlgorithm,
                                             DefaultSinkCloseAlgorithm, DefaultSinkAbortAlgorithm));
  if (!sink)
    return false;

  JS::RootedObject writable(cx);
  writable =
      JS::NewWritableDefaultStreamObject(cx, sink, writableSizeAlgorithm, writableHighWaterMark);
  if (!writable)
    return false;

  JS::SetReservedSlot(stream, TransformStream::Slots::Writable, JS::ObjectValue(*writable));

  // Step 6.  Let pullAlgorithm be the following steps:
  auto pullAlgorithm = DefaultSourcePullAlgorithm;

  // Step 7.  Let cancelAlgorithm be the following steps, taking a reason
  // argument: (Sub-steps moved into DefaultSourceCancelAlgorithm)
  auto cancelAlgorithm = DefaultSourceCancelAlgorithm;

  // Step 8.  Set stream.[readable] to ! [CreateReadableStream](startAlgorithm,
  // pullAlgorithm, cancelAlgorithm, readableHighWaterMark,
  // readableSizeAlgorithm).
  JS::RootedObject source(cx, builtins::NativeStreamSource::create(cx, stream, startPromiseVal,
                                                                   pullAlgorithm, cancelAlgorithm));
  if (!source)
    return false;

  JS::RootedObject readable(cx, JS::NewReadableDefaultStreamObject(
                                    cx, source, readableSizeAlgorithm, readableHighWaterMark));
  if (!readable)
    return false;

  JS::SetReservedSlot(stream, TransformStream::Slots::Readable, JS::ObjectValue(*readable));

  // Step 9.  Set stream.[backpressure] and stream.[backpressureChangePromise]
  // to undefined. As the note in the spec says, it's valid to instead set
  // `backpressure` to a boolean value early, which makes implementing
  // SetBackpressure easier.
  JS::SetReservedSlot(stream, TransformStream::Slots::Backpressure, JS::FalseValue());

  // For similar reasons, ensure that the backpressureChangePromise slot is
  // null.
  JS::SetReservedSlot(stream, TransformStream::Slots::BackpressureChangePromise, JS::NullValue());

  // Step 10. Perform ! [TransformStreamSetBackpressure](stream, true).
  if (!SetBackpressure(cx, stream, true)) {
    return false;
  }

  // Step 11. Set stream.[controller] to undefined.
  JS::SetReservedSlot(stream, TransformStream::Slots::Controller, JS::UndefinedValue());

  // Some transform streams are used as mixins in other builtins, which set
  // this to `true` as part of their construction.
  JS::SetReservedSlot(stream, TransformStream::Slots::UsedAsMixin, JS::FalseValue());

  return true;
}

bool TransformStream::ErrorWritableAndUnblockWrite(JSContext *cx, JS::HandleObject stream,
                                                   JS::HandleValue error) {
  MOZ_ASSERT(is_instance(stream));

  // 1.  Perform
  // TransformStreamDefaultControllerClearAlgorithms(stream.[controller]).
  builtins::TransformStreamDefaultController::ClearAlgorithms(controller(stream));

  // 2.  Perform
  // WritableStreamDefaultControllerErrorIfNeeded(stream.[writable].[controller],
  // e). (inlined)
  JS::RootedObject writable(cx, TransformStream::writable(stream));
  if (JS::WritableStreamGetState(cx, writable) == JS::WritableStreamState::Writable) {
    if (!JS::WritableStreamError(cx, writable, error)) {
      return false;
    }
  }

  // 3.  If stream.[backpressure] is true, perform
  // TransformStreamSetBackpressure(stream, false).
  if (backpressure(stream)) {
    if (!SetBackpressure(cx, stream, false)) {
      return false;
    }
  }

  return true;
}

/**
 * https://streams.spec.whatwg.org/#ts-constructor
 * Steps 9-13.
 */
JSObject *
TransformStream::create(JSContext *cx, JS::HandleObject self, double writableHighWaterMark,
                        JS::HandleFunction writableSizeAlgorithm, double readableHighWaterMark,
                        JS::HandleFunction readableSizeAlgorithm, JS::HandleValue transformer,
                        JS::HandleObject startFunction, JS::HandleObject transformFunction,
                        JS::HandleObject flushFunction) {

  // Step 9.
  JS::RootedObject startPromise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!startPromise)
    return nullptr;

  // Step 10.
  if (!Initialize(cx, self, startPromise, writableHighWaterMark, writableSizeAlgorithm,
                  readableHighWaterMark, readableSizeAlgorithm)) {
    return nullptr;
  }

  // Step 11.
  JS::RootedObject controller(cx);
  controller = builtins::TransformStreamDefaultController::SetUpFromTransformer(
      cx, self, transformer, transformFunction, flushFunction);
  if (!controller) {
    return nullptr;
  }

  JS::RootedValue rval(cx);

  // Step 12.
  // If transformerDict["start"], then resolve startPromise with the result of
  // invoking transformerDict["start"] with argument list «
  // this.[[[controller]]] » and callback this value transformer.
  if (startFunction) {
    JS::RootedValueArray<1> newArgs(cx);
    newArgs[0].setObject(*controller);
    if (!JS::Call(cx, transformer, startFunction, newArgs, &rval)) {
      return nullptr;
    }
  }

  // Step 13.
  // Otherwise, resolve startPromise with undefined.
  if (!JS::ResolvePromise(cx, startPromise, rval)) {
    return nullptr;
  }

  return self;
}

JSObject *TransformStream::create(JSContext *cx, double writableHighWaterMark,
                                  JS::HandleFunction writableSizeAlgorithm,
                                  double readableHighWaterMark,
                                  JS::HandleFunction readableSizeAlgorithm,
                                  JS::HandleValue transformer, JS::HandleObject startFunction,
                                  JS::HandleObject transformFunction,
                                  JS::HandleObject flushFunction) {
  JS::RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self)
    return nullptr;

  return TransformStream::create(cx, self, writableHighWaterMark, writableSizeAlgorithm,
                                 readableHighWaterMark, readableSizeAlgorithm, transformer,
                                 startFunction, transformFunction, flushFunction);
}

/**
 * Implementation of
 * https://streams.spec.whatwg.org/#readablestream-create-a-proxy
 */
JSObject *TransformStream::create_rs_proxy(JSContext *cx, JS::HandleObject input_readable) {
  MOZ_ASSERT(JS::IsReadableStream(input_readable));
  JS::RootedObject transform_stream(
      cx, create(cx, 1, nullptr, 0, nullptr, JS::UndefinedHandleValue, nullptr, nullptr, nullptr));
  if (!transform_stream) {
    return nullptr;
  }

  JS::RootedObject writable_end(cx, writable(transform_stream));

  if (!ReadableStream_additions::pipeThrough(cx, input_readable, writable_end,
                                             JS::UndefinedHandleValue)) {
    return nullptr;
  }

  return readable(transform_stream);
}
} // namespace builtins
