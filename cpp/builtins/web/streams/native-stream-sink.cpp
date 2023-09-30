// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/experimental/TypedData.h" // used within "js/Stream.h"
#pragma clang diagnostic pop

#include "js/Stream.h"

#include "builtin.h"
#include "builtins/native-stream-sink.h"
#include "js-compute-builtins.h"

// A JS class to use as the underlying sink for native writable streams, used
// for TransformStream.
namespace builtins {

JSObject *NativeStreamSink::owner(JSObject *self) {
  return &JS::GetReservedSlot(self, Slots::Owner).toObject();
}

JS::Value NativeStreamSink::startPromise(JSObject *self) {
  return JS::GetReservedSlot(self, Slots::StartPromise);
}

NativeStreamSink::WriteAlgorithmImplementation *NativeStreamSink::writeAlgorithm(JSObject *self) {
  return (WriteAlgorithmImplementation *)JS::GetReservedSlot(self, Slots::WriteAlgorithm)
      .toPrivate();
}

NativeStreamSink::AbortAlgorithmImplementation *NativeStreamSink::abortAlgorithm(JSObject *self) {
  return (AbortAlgorithmImplementation *)JS::GetReservedSlot(self, Slots::AbortAlgorithm)
      .toPrivate();
}

NativeStreamSink::CloseAlgorithmImplementation *NativeStreamSink::closeAlgorithm(JSObject *self) {
  return (CloseAlgorithmImplementation *)JS::GetReservedSlot(self, Slots::CloseAlgorithm)
      .toPrivate();
}

JSObject *NativeStreamSink::controller(JSObject *self) {
  return &JS::GetReservedSlot(self, Slots::Controller).toObject();
}

/**
 * Returns the underlying sink for the given controller iff it's an object,
 * nullptr otherwise.
 */
JSObject *NativeStreamSink::get_controller_sink(JSContext *cx, JS::HandleObject controller) {
  JS::RootedValue sink(cx, JS::WritableStreamControllerGetUnderlyingSink(cx, controller));
  return sink.isObject() ? &sink.toObject() : nullptr;
}

JSObject *NativeStreamSink::get_stream_sink(JSContext *cx, JS::HandleObject stream) {
  JS::RootedObject controller(cx, JS::WritableStreamGetController(cx, stream));
  return get_controller_sink(cx, controller);
}

bool NativeStreamSink::stream_has_native_sink(JSContext *cx, JS::HandleObject stream) {
  MOZ_RELEASE_ASSERT(JS::IsWritableStream(stream));

  JSObject *sink = get_stream_sink(cx, stream);
  return is_instance(sink);
}

bool NativeStreamSink::start(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  MOZ_ASSERT(args[0].isObject());
  JS::RootedObject controller(cx, &args[0].toObject());
  MOZ_ASSERT(get_controller_sink(cx, controller) == self);

  JS::SetReservedSlot(self, Slots::Controller, args[0]);

  // For TransformStream, StartAlgorithm returns the same Promise for both the
  // readable and writable stream. All other native initializations of
  // WritableStream have StartAlgorithm return undefined.
  //
  // Instead of introducing both the StartAlgorithm as a pointer and
  // startPromise as a value, we just store the latter or undefined, and always
  // return it.
  args.rval().set(startPromise(self));
  return true;
}

bool NativeStreamSink::write(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  JS::RootedObject owner(cx, NativeStreamSink::owner(self));
  JS::HandleValue chunk(args[0]);

  WriteAlgorithmImplementation *write = writeAlgorithm(self);
  return write(cx, args, self, owner, chunk);
}

bool NativeStreamSink::abort(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  JS::RootedObject owner(cx, NativeStreamSink::owner(self));
  JS::HandleValue reason(args[0]);

  AbortAlgorithmImplementation *abort = abortAlgorithm(self);
  return abort(cx, args, self, owner, reason);
}

bool NativeStreamSink::close(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JS::RootedObject owner(cx, NativeStreamSink::owner(self));

  CloseAlgorithmImplementation *close = closeAlgorithm(self);
  return close(cx, args, self, owner);
}

const JSFunctionSpec NativeStreamSink::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec NativeStreamSink::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec NativeStreamSink::methods[] = {
    JS_FN("start", start, 1, 0), JS_FN("write", write, 2, 0), JS_FN("abort", abort, 2, 0),
    JS_FN("close", close, 1, 0), JS_FS_END};

const JSPropertySpec NativeStreamSink::properties[] = {JS_PS_END};

JSObject *NativeStreamSink::create(JSContext *cx, JS::HandleObject owner,
                                   JS::HandleValue startPromise,
                                   WriteAlgorithmImplementation *write,
                                   CloseAlgorithmImplementation *close,
                                   AbortAlgorithmImplementation *abort) {
  JS::RootedObject sink(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!sink)
    return nullptr;

  JS::SetReservedSlot(sink, Slots::Owner, JS::ObjectValue(*owner));
  JS::SetReservedSlot(sink, Slots::StartPromise, startPromise);
  JS::SetReservedSlot(sink, Slots::WriteAlgorithm, JS::PrivateValue((void *)write));
  JS::SetReservedSlot(sink, Slots::AbortAlgorithm, JS::PrivateValue((void *)abort));
  JS::SetReservedSlot(sink, Slots::CloseAlgorithm, JS::PrivateValue((void *)close));
  return sink;
}
} // namespace builtins
