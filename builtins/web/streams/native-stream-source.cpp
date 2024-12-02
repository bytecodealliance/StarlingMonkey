// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/experimental/TypedData.h" // used within "js/Stream.h"
#pragma clang diagnostic pop

#include "js/Stream.h"

#include "native-stream-sink.h"
#include "native-stream-source.h"

#include "stream-errors.h"

// A JS class to use as the underlying source for native readable streams, used
// for Request/Response bodies and TransformStream.
namespace builtins {
namespace web {
namespace streams {

JSObject *NativeStreamSource::owner(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, Slots::Owner).toObject();
}

JS::Value NativeStreamSource::startPromise(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::StartPromise);
}

NativeStreamSource::PullAlgorithmImplementation *NativeStreamSource::pullAlgorithm(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return (PullAlgorithmImplementation *)JS::GetReservedSlot(self, Slots::PullAlgorithm).toPrivate();
}

NativeStreamSource::CancelAlgorithmImplementation *
NativeStreamSource::cancelAlgorithm(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return (CancelAlgorithmImplementation *)JS::GetReservedSlot(self, Slots::CancelAlgorithm)
      .toPrivate();
}

JSObject *NativeStreamSource::default_stream(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, NativeStreamSource::Slots::Stream).toObjectOrNull();
}

/**
 * Returns the underlying source for the given controller iff it's an object,
 * nullptr otherwise.
 */
JSObject *NativeStreamSource::get_controller_source(JSContext *cx, JS::HandleObject controller) {
  JS::RootedValue source(cx);
  bool success __attribute__((unused));
  success = JS::ReadableStreamControllerGetUnderlyingSource(cx, controller, &source);
  MOZ_ASSERT(success);
  return source.isObject() ? &source.toObject() : nullptr;
}

JSObject *NativeStreamSource::get_stream_source(JSContext *cx, JS::HandleObject stream) {
  MOZ_ASSERT(JS::IsReadableStream(stream));
  JS::RootedObject controller(cx, JS::ReadableStreamGetController(cx, stream));
  return get_controller_source(cx, controller);
}

bool NativeStreamSource::stream_has_native_source(JSContext *cx, JS::HandleObject stream) {
  JSObject *source = get_stream_source(cx, stream);
  return is_instance(source);
}

void NativeStreamSource::set_stream_piped_to_ts_writable(JSContext *cx, JS::HandleObject stream,
                                                         JS::HandleObject writable) {
  JS::RootedObject source(cx, NativeStreamSource::get_stream_source(cx, stream));
  MOZ_ASSERT(is_instance(source));
  JS::RootedObject sink(cx, NativeStreamSink::get_stream_sink(cx, writable));
  JS::RootedObject transform_stream(cx, NativeStreamSink::owner(sink));
  MOZ_ASSERT(transform_stream);
  JS::SetReservedSlot(source, Slots::PipedToTransformStream, JS::ObjectValue(*transform_stream));
}

JSObject *NativeStreamSource::piped_to_transform_stream(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, Slots::PipedToTransformStream).toObjectOrNull();
}

bool NativeStreamSource::lock_stream(JSContext *cx, JS::HandleObject stream) {
  MOZ_ASSERT(JS::IsReadableStream(stream));

  bool locked;
  JS::ReadableStreamIsLocked(cx, stream, &locked);
  if (locked) {
    return api::throw_error(cx, StreamErrors::StreamAlreadyLocked);
  }

  JS::RootedObject self(cx, get_stream_source(cx, stream));
  MOZ_ASSERT(is_instance(self));

  auto mode = JS::ReadableStreamReaderMode::Default;
  JS::RootedObject reader(cx, JS::ReadableStreamGetReader(cx, stream, mode));
  if (!reader)
    return false;

  JS::SetReservedSlot(self, Slots::InternalReader, JS::ObjectValue(*reader));
  return true;
}

bool NativeStreamSource::start(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  MOZ_ASSERT(args[0].isObject());
  JS::RootedObject __attribute__((unused)) controller(cx, &args[0].toObject());
  MOZ_ASSERT(get_controller_source(cx, controller) == self);

  // For TransformStream, StartAlgorithm returns the same Promise for both the
  // readable and writable stream. All other native initializations of
  // ReadableStream have StartAlgorithm return undefined. Instead of introducing
  // both the StartAlgorithm as a pointer and startPromise as a value, we just
  // store the latter or undefined, and always return it.
  args.rval().set(startPromise(self));
  return true;
}

bool NativeStreamSource::pull(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  JS::RootedObject owner(cx, NativeStreamSource::owner(self));
  JS::RootedObject controller(cx, &args[0].toObject());
  MOZ_ASSERT(get_controller_source(cx, controller) == self.get());

  PullAlgorithmImplementation *pull = pullAlgorithm(self);
  return pull(cx, args, self, owner, controller);
}

bool NativeStreamSource::cancel(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JS::RootedObject owner(cx, NativeStreamSource::owner(self));
  JS::HandleValue reason(args.get(0));

  CancelAlgorithmImplementation *cancel = cancelAlgorithm(self);
  return cancel(cx, args, self, owner, reason);
}

const JSFunctionSpec NativeStreamSource::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec NativeStreamSource::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec NativeStreamSource::methods[] = {
    JS_FN("start", start, 1, 0),
    JS_FN("pull", pull, 1, 0),
    JS_FN("cancel", cancel, 1, 0),
    JS_FS_END
};

const JSPropertySpec NativeStreamSource::properties[] = {
    JS_PS_END
};

JSObject *NativeStreamSource::create(JSContext *cx, JS::HandleObject owner, JS::HandleValue startPromise,
                                     PullAlgorithmImplementation *pull, CancelAlgorithmImplementation *cancel,
                                     JS::HandleFunction size, double highWaterMark) {
  JS::RootedObject source(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!source) {
    return nullptr;
  }

  // Initialize source slots before creating `default_stream`.
  JS::SetReservedSlot(source, Slots::Owner, JS::ObjectValue(*owner));
  JS::SetReservedSlot(source, Slots::StartPromise, startPromise);
  JS::SetReservedSlot(source, Slots::PullAlgorithm, JS::PrivateValue((void *)pull));
  JS::SetReservedSlot(source, Slots::CancelAlgorithm, JS::PrivateValue((void *)cancel));
  JS::SetReservedSlot(source, Slots::PipedToTransformStream, JS::NullValue());

  JS::RootedObject default_stream(cx, JS::NewReadableDefaultStreamObject(cx, source, size, highWaterMark));
  if (!default_stream) {
    return nullptr;
  }

  JS::SetReservedSlot(source, Slots::Stream, JS::ObjectValue(*default_stream));
  return source;
}
} // namespace streams
} // namespace web
} // namespace builtins
