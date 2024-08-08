#include "request-response.h"

#include "../streams/native-stream-source.h"
#include "../streams/transform-stream.h"
#include "../url.h"
#include "encode.h"
#include "event_loop.h"
#include "extension-api.h"
#include "fetch_event.h"
#include "host_api.h"
#include "picosha2.h"

#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/JSON.h"
#include "js/Stream.h"
#include <algorithm>
#include <iostream>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "../worker-location.h"
#include "js/experimental/TypedData.h"
#pragma clang diagnostic pop

namespace builtins::web::streams {

JSObject *NativeStreamSource::stream(JSObject *self) {
  return web::fetch::RequestOrResponse::body_stream(owner(self));
}

bool NativeStreamSource::stream_is_body(JSContext *cx, JS::HandleObject stream) {
  JSObject *stream_source = get_stream_source(cx, stream);
  return NativeStreamSource::is_instance(stream_source) &&
         web::fetch::RequestOrResponse::is_instance(owner(stream_source));
}

} // namespace builtins::web::streams

namespace builtins::web::fetch {

static api::Engine *ENGINE;

bool error_stream_controller_with_pending_exception(JSContext *cx, HandleObject controller) {
  RootedValue exn(cx);
  if (!JS_GetPendingException(cx, &exn))
    return false;
  JS_ClearPendingException(cx);

  RootedValueArray<1> args(cx);
  args[0].set(exn);
  RootedValue r(cx);
  return JS::Call(cx, controller, "error", args, &r);
}

constexpr size_t HANDLE_READ_CHUNK_SIZE = 8192;

class BodyFutureTask final : public api::AsyncTask {
  Heap<JSObject *> body_source_;
  host_api::HttpIncomingBody *incoming_body_;

public:
  explicit BodyFutureTask(const HandleObject body_source) : body_source_(body_source) {
    auto owner = streams::NativeStreamSource::owner(body_source_);
    incoming_body_ = RequestOrResponse::incoming_body_handle(owner);
    auto res = incoming_body_->subscribe();
    MOZ_ASSERT(!res.is_err(), "Subscribing to a future should never fail");
    handle_ = res.unwrap();
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    // MOZ_ASSERT(ready());
    JSContext *cx = engine->cx();
    RootedObject owner(cx, streams::NativeStreamSource::owner(body_source_));
    RootedObject controller(cx, streams::NativeStreamSource::controller(body_source_));
    auto body = RequestOrResponse::incoming_body_handle(owner);

    auto read_res = body->read(HANDLE_READ_CHUNK_SIZE);
    if (auto *err = read_res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return error_stream_controller_with_pending_exception(cx, controller);
    }

    auto &chunk = read_res.unwrap();
    if (chunk.done) {
      RootedValue r(cx);
      return Call(cx, controller, "close", HandleValueArray::empty(), &r);
    }

    // We don't release control of chunk's data until after we've checked that
    // the array buffer allocation has been successful, as that ensures that the
    // return path frees chunk automatically when necessary.
    auto &bytes = chunk.bytes;
    RootedObject buffer(
        cx, JS::NewArrayBufferWithContents(cx, bytes.len, bytes.ptr.get(),
                                           JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
    if (!buffer) {
      return error_stream_controller_with_pending_exception(cx, controller);
    }

    // At this point `buffer` has taken full ownership of the chunk's data.
    std::ignore = bytes.ptr.release();

    RootedObject byte_array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, bytes.len));
    if (!byte_array) {
      return false;
    }

    RootedValueArray<1> enqueue_args(cx);
    enqueue_args[0].setObject(*byte_array);
    RootedValue r(cx);
    if (!JS::Call(cx, controller, "enqueue", enqueue_args, &r)) {
      return error_stream_controller_with_pending_exception(cx, controller);
    }

    return cancel(engine);
  }

  [[nodiscard]] bool cancel(api::Engine *engine) override {
    // TODO(TS): implement
    handle_ = -1;
    return true;
  }

  void trace(JSTracer *trc) override { TraceEdge(trc, &body_source_, "body source for future"); }
};

namespace {
// https://fetch.spec.whatwg.org/#concept-method-normalize
// Returns `true` if the method name was normalized, `false` otherwise.
bool normalize_http_method(char *method) {
  static const char *names[6] = {"DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT"};

  for (const auto name : names) {
    if (strcasecmp(method, name) == 0) {
      if (strcmp(method, name) == 0) {
        return false;
      }

      // Note: Safe because `strcasecmp` returning 0 above guarantees
      // same-length strings.
      strcpy(method, name);
      return true;
    }
  }

  return false;
}

struct ReadResult {
  UniqueChars buffer;
  size_t length;
};

} // namespace

host_api::HttpRequestResponseBase *RequestOrResponse::handle(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  auto slot = JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::RequestOrResponse));
  return static_cast<host_api::HttpRequestResponseBase *>(slot.toPrivate());
}

bool RequestOrResponse::is_instance(JSObject *obj) {
  return Request::is_instance(obj) || Response::is_instance(obj);
}

bool RequestOrResponse::is_incoming(JSObject *obj) {
  auto handle = RequestOrResponse::handle(obj);
  return handle && handle->is_incoming();
}

host_api::HttpHeadersReadOnly *RequestOrResponse::headers_handle(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  auto res = handle(obj)->headers();
  MOZ_ASSERT(!res.is_err(), "TODO: proper error handling");
  return res.unwrap();
}

bool RequestOrResponse::has_body(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::HasBody)).toBoolean();
}

host_api::HttpIncomingBody *RequestOrResponse::incoming_body_handle(JSObject *obj) {
  MOZ_ASSERT(is_incoming(obj));
  if (handle(obj)->is_request()) {
    return reinterpret_cast<host_api::HttpIncomingRequest *>(handle(obj))->body().unwrap();
  } else {
    return reinterpret_cast<host_api::HttpIncomingResponse *>(handle(obj))->body().unwrap();
  }
}

host_api::HttpOutgoingBody *RequestOrResponse::outgoing_body_handle(JSObject *obj) {
  MOZ_ASSERT(!is_incoming(obj));
  if (handle(obj)->is_request()) {
    return reinterpret_cast<host_api::HttpOutgoingRequest *>(handle(obj))->body().unwrap();
  } else {
    return reinterpret_cast<host_api::HttpOutgoingResponse *>(handle(obj))->body().unwrap();
  }
}

JSObject *RequestOrResponse::body_stream(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::BodyStream)).toObjectOrNull();
}

JSObject *RequestOrResponse::body_source(JSContext *cx, JS::HandleObject obj) {
  MOZ_ASSERT(has_body(obj));
  JS::RootedObject stream(cx, body_stream(obj));
  return streams::NativeStreamSource::get_stream_source(cx, stream);
}

bool RequestOrResponse::body_used(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::BodyUsed)).toBoolean();
}

bool RequestOrResponse::mark_body_used(JSContext *cx, JS::HandleObject obj) {
  MOZ_ASSERT(!body_used(obj));
  JS::SetReservedSlot(obj, static_cast<uint32_t>(Slots::BodyUsed), JS::BooleanValue(true));

  JS::RootedObject stream(cx, body_stream(obj));
  if (stream && streams::NativeStreamSource::stream_is_body(cx, stream)) {
    RootedObject source(cx, streams::NativeStreamSource::get_stream_source(cx, stream));
    if (streams::NativeStreamSource::piped_to_transform_stream(source)) {
      return true;
    }
    if (!streams::NativeStreamSource::lock_stream(cx, stream)) {
      // The only reason why marking the body as used could fail here is that
      // it's a disturbed ReadableStream. To improve error reporting, we clear
      // the current exception and throw a better one.
      JS_ClearPendingException(cx);
      return api::throw_error(cx, FetchErrors::BodyStreamUnusable);
    }
  }

  return true;
}

JS::Value RequestOrResponse::url(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  JS::Value val = JS::GetReservedSlot(obj, static_cast<uint32_t>(RequestOrResponse::Slots::URL));
  MOZ_ASSERT(val.isString());
  return val;
}

void RequestOrResponse::set_url(JSObject *obj, JS::Value url) {
  MOZ_ASSERT(is_instance(obj));
  MOZ_ASSERT(url.isString());
  JS::SetReservedSlot(obj, static_cast<uint32_t>(RequestOrResponse::Slots::URL), url);
}

/**
 * Implementation of the `body is unusable` concept at
 * https://fetch.spec.whatwg.org/#body-unusable
 */
bool RequestOrResponse::body_unusable(JSContext *cx, JS::HandleObject body) {
  MOZ_ASSERT(JS::IsReadableStream(body));
  bool disturbed;
  bool locked;
  MOZ_RELEASE_ASSERT(JS::ReadableStreamIsDisturbed(cx, body, &disturbed) &&
                     JS::ReadableStreamIsLocked(cx, body, &locked));
  return disturbed || locked;
}

/**
 * Implementation of the `extract a body` algorithm at
 * https://fetch.spec.whatwg.org/#concept-bodyinit-extract
 *
 * Note: also includes the steps applying the `Content-Type` header from the
 * Request and Response constructors in step 36 and 8 of those, respectively.
 */
bool RequestOrResponse::extract_body(JSContext *cx, JS::HandleObject self,
                                     JS::HandleValue body_val) {
  MOZ_ASSERT(is_instance(self));
  MOZ_ASSERT(!has_body(self));
  MOZ_ASSERT(!body_val.isNullOrUndefined());

  const char *content_type = nullptr;
  mozilla::Maybe<size_t> content_length;

  // We currently support five types of body inputs:
  // - byte sequence
  // - buffer source
  // - USV strings
  // - URLSearchParams
  // - ReadableStream
  // After the other other options are checked explicitly, all other inputs are
  // encoded to a UTF8 string to be treated as a USV string.
  // TODO: Support the other possible inputs to Body.

  JS::RootedObject body_obj(cx, body_val.isObject() ? &body_val.toObject() : nullptr);

  if (body_obj && JS::IsReadableStream(body_obj)) {
    if (RequestOrResponse::body_unusable(cx, body_obj)) {
      return api::throw_error(cx, FetchErrors::BodyStreamUnusable);
    }

    JS_SetReservedSlot(self, static_cast<uint32_t>(RequestOrResponse::Slots::BodyStream), body_val);

    // Ensure that we take the right steps for shortcutting operations on
    // TransformStreams later on.
    if (streams::TransformStream::is_ts_readable(cx, body_obj)) {
      // But only if the TransformStream isn't used as a mixin by other
      // builtins.
      if (!streams::TransformStream::used_as_mixin(
              streams::TransformStream::ts_from_readable(cx, body_obj))) {
        streams::TransformStream::set_readable_used_as_body(cx, body_obj, self);
      }
    }
  } else {
    RootedValue chunk(cx);
    RootedObject buffer(cx);
    char *buf = nullptr;
    size_t length = 0;

    if (body_obj && JS_IsArrayBufferViewObject(body_obj)) {
      length = JS_GetArrayBufferViewByteLength(body_obj);
      buf = static_cast<char *>(js_malloc(length));
      if (!buf) {
        return false;
      }

      bool is_shared;
      JS::AutoCheckCannotGC noGC(cx);
      auto temp_buf = JS_GetArrayBufferViewData(body_obj, &is_shared, noGC);
      memcpy(buf, temp_buf, length);
    } else if (body_obj && IsArrayBufferObject(body_obj)) {
      buffer = CopyArrayBuffer(cx, body_obj);
      if (!buffer) {
        return false;
      }
      length = GetArrayBufferByteLength(buffer);
    } else if (body_obj && url::URLSearchParams::is_instance(body_obj)) {
      auto slice = url::URLSearchParams::serialize(cx, body_obj);
      buf = (char *)slice.data;
      length = slice.len;
      content_type = "application/x-www-form-urlencoded;charset=UTF-8";
    } else {
      auto text = core::encode(cx, body_val);
      if (!text.ptr) {
        return false;
      }
      buf = text.ptr.release();
      length = text.len;
      content_type = "text/plain;charset=UTF-8";
    }

    if (!buffer) {
      MOZ_ASSERT_IF(length, buf);
      buffer = NewArrayBufferWithContents(cx, length, buf,
                                          JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory);
      if (!buffer) {
        js_free(buf);
        return false;
      }
    }

    RootedObject array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, length));
    if (!array) {
      return false;
    }
    chunk.setObject(*array);

    // Set a __proto__-less source so modifying Object.prototype doesn't change the behavior.
    RootedObject source(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
    if (!source) {
      return false;
    }
    RootedObject body_stream(cx, JS::NewReadableDefaultStreamObject(cx, source, nullptr, 0.0));
    if (!body_stream) {
      return false;
    }

    mozilla::DebugOnly<bool> disturbed;
    MOZ_ASSERT(ReadableStreamIsDisturbed(cx, body_stream, &disturbed));
    MOZ_ASSERT(!disturbed);

    if (!ReadableStreamEnqueue(cx, body_stream, chunk) || !ReadableStreamClose(cx, body_stream)) {
      return false;
    }

    JS_SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyStream), ObjectValue(*body_stream));
    content_length.emplace(length);
  }

  if (content_type || content_length.isSome()) {
    JS::RootedObject headers(cx, RequestOrResponse::headers(cx, self));
    if (!headers) {
      return false;
    }

    if (content_length.isSome()) {
      auto length_str = std::to_string(content_length.value());
      if (!Headers::set_valid_if_undefined(cx, headers, "content-length", length_str)) {
        return false;
      }
    }

    // Step 36.3 of Request constructor / 8.4 of Response constructor.
    if (content_type &&
        !Headers::set_valid_if_undefined(cx, headers, "content-type", content_type)) {
      return false;
    }
  }

  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::HasBody), JS::BooleanValue(true));
  return true;
}

JSObject *RequestOrResponse::maybe_headers(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Headers)).toObjectOrNull();
}

unique_ptr<host_api::HttpHeaders> RequestOrResponse::headers_handle_clone(JSContext *cx,
                                                                          HandleObject self) {
  MOZ_ASSERT(is_instance(self));

  RootedObject headers(cx, maybe_headers(self));
  if (headers) {
    return Headers::handle_clone(cx, headers);
  }

  auto handle = RequestOrResponse::handle(self);
  if (!handle) {
    return std::make_unique<host_api::HttpHeaders>();
  }

  auto res = handle->headers();
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return nullptr;
  }
  return unique_ptr<host_api::HttpHeaders>(res.unwrap()->clone());
}

bool finish_outgoing_body_streaming(JSContext *cx, HandleObject body_owner) {
  // If no `body_owner` was passed, that means we sent a response: those aren't always
  // reified during `respondWith` processing, and we don't need the instance here.
  // That means, if we don't have the `body_owner`, we can advance the FetchState to
  // `responseDone`.
  // (Note that even if we encountered an error while streaming, `responseDone` is the
  // right state: `respondedWithError` is for when sending a response at all failed.)
  // TODO(TS): factor this out to remove dependency on fetch-event.h
  if (!body_owner || Response::is_instance(body_owner)) {
    fetch_event::FetchEvent::set_state(fetch_event::FetchEvent::instance(),
                                       fetch_event::FetchEvent::State::responseDone);
    return true;
  }

  auto body = RequestOrResponse::outgoing_body_handle(body_owner);
  auto res = body->close();
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  if (Request::is_instance(body_owner)) {
    auto pending_handle = static_cast<host_api::FutureHttpIncomingResponse *>(
        GetReservedSlot(body_owner, static_cast<uint32_t>(Request::Slots::PendingResponseHandle))
            .toPrivate());
    SetReservedSlot(body_owner, static_cast<uint32_t>(Request::Slots::PendingResponseHandle),
                    PrivateValue(nullptr));
    ENGINE->queue_async_task(new ResponseFutureTask(body_owner, pending_handle));
  }

  return true;
}

bool RequestOrResponse::append_body(JSContext *cx, JS::HandleObject self, JS::HandleObject source) {
  MOZ_ASSERT(!body_used(source));
  MOZ_ASSERT(!body_used(self));
  MOZ_ASSERT(self != source);
  host_api::HttpIncomingBody *source_body = incoming_body_handle(source);
  host_api::HttpOutgoingBody *dest_body = outgoing_body_handle(self);
  auto res = dest_body->append(ENGINE, source_body, finish_outgoing_body_streaming, self);
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  mozilla::DebugOnly<bool> success = mark_body_used(cx, source);
  MOZ_ASSERT(success);
  if (body_stream(source) != body_stream(self)) {
    success = mark_body_used(cx, self);
    MOZ_ASSERT(success);
  }

  return true;
}

JSObject *RequestOrResponse::headers(JSContext *cx, JS::HandleObject obj) {
  JSObject *headers = maybe_headers(obj);
  if (!headers) {
    // Incoming request and incoming response headers are immutable per service worker
    // and fetch specs respectively.
    Headers::HeadersGuard guard = is_incoming(obj)            ? Headers::HeadersGuard::Immutable
                                  : Request::is_instance(obj) ? Headers::HeadersGuard::Request
                                                              : Headers::HeadersGuard::Response;
    host_api::HttpHeadersReadOnly *handle;
    if (is_incoming(obj) && (handle = headers_handle(obj))) {
      headers = Headers::create(cx, handle, guard);
    } else {
      headers = Headers::create(cx, guard);
    }
    if (!headers) {
      return nullptr;
    }

    JS_SetReservedSlot(obj, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));
  }

  return headers;
}

template <RequestOrResponse::BodyReadResult result_type>
bool RequestOrResponse::parse_body(JSContext *cx, JS::HandleObject self, JS::UniqueChars buf,
                                   size_t len) {
  JS::RootedObject result_promise(
      cx, &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise)).toObject());
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise), JS::UndefinedValue());
  JS::RootedValue result(cx);

  if constexpr (result_type == RequestOrResponse::BodyReadResult::ArrayBuffer) {
    JS::RootedObject array_buffer(
        cx, JS::NewArrayBufferWithContents(cx, len, buf.get(),
                                           JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
    if (!array_buffer) {
      return RejectPromiseWithPendingError(cx, result_promise);
    }
    static_cast<void>(buf.release());
    result.setObject(*array_buffer);
  } else {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(buf.get(), len)));
    if (!text) {
      return RejectPromiseWithPendingError(cx, result_promise);
    }

    if constexpr (result_type == RequestOrResponse::BodyReadResult::Text) {
      result.setString(text);
    } else {
      MOZ_ASSERT(result_type == RequestOrResponse::BodyReadResult::JSON);
      if (!JS_ParseJSON(cx, text, &result)) {
        return RejectPromiseWithPendingError(cx, result_promise);
      }
    }
  }

  return JS::ResolvePromise(cx, result_promise, result);
}

bool RequestOrResponse::content_stream_read_then_handler(JSContext *cx, JS::HandleObject self,
                                                         JS::HandleValue extra, JS::CallArgs args) {
  JS::RootedObject then_handler(cx, &args.callee());
  // The reader is stored in the catch handler, which we need here as well.
  // So we get that first, then the reader.
  MOZ_ASSERT(extra.isObject());
  JS::RootedObject catch_handler(cx, &extra.toObject());
#ifdef DEBUG
  bool foundContents;
  if (!JS_HasElement(cx, catch_handler, 1, &foundContents)) {
    return false;
  }
  MOZ_ASSERT(foundContents);
#endif
  JS::RootedValue contents_val(cx);
  if (!JS_GetElement(cx, catch_handler, 1, &contents_val)) {
    return false;
  }
  MOZ_ASSERT(contents_val.isObject());
  JS::RootedObject contents(cx, &contents_val.toObject());
  if (!contents) {
    return false;
  }
#ifdef DEBUG
  bool contentsIsArray;
  if (!JS::IsArrayObject(cx, contents, &contentsIsArray)) {
    return false;
  }
  MOZ_ASSERT(contentsIsArray);
#endif

  auto reader_val = js::GetFunctionNativeReserved(catch_handler, 1);
  MOZ_ASSERT(reader_val.isObject());
  JS::RootedObject reader(cx, &reader_val.toObject());

  // We're guaranteed to work with a native ReadableStreamDefaultReader here as we used
  // `JS::ReadableStreamDefaultReaderRead(cx, reader)`, which in turn is guaranteed to return {done:
  // bool, value: any} objects to read promise then callbacks.
  MOZ_ASSERT(args[0].isObject());
  JS::RootedObject chunk_obj(cx, &args[0].toObject());
  JS::RootedValue done_val(cx);
  JS::RootedValue value(cx);
#ifdef DEBUG
  bool hasValue;
  if (!JS_HasProperty(cx, chunk_obj, "value", &hasValue)) {
    return false;
  }
  MOZ_ASSERT(hasValue);
#endif
  if (!JS_GetProperty(cx, chunk_obj, "value", &value)) {
    return false;
  }
#ifdef DEBUG
  bool hasDone;
  if (!JS_HasProperty(cx, chunk_obj, "done", &hasDone)) {
    return false;
  }
  MOZ_ASSERT(hasDone);
#endif
  if (!JS_GetProperty(cx, chunk_obj, "done", &done_val)) {
    return false;
  }
  MOZ_ASSERT(done_val.isBoolean());
  if (done_val.toBoolean()) {
    // We finished reading the stream
    // Now we need to iterate/reduce `contents` JS Array into UniqueChars
    uint32_t contentsLength;
    if (!JS::GetArrayLength(cx, contents, &contentsLength)) {
      return false;
    }

    size_t total_length = 0;
    RootedValue val(cx);

    for (size_t index = 0; index < contentsLength; index++) {
      if (!JS_GetElement(cx, contents, index, &val)) {
        return false;
      }
      JSObject *array = &val.toObject();
      size_t length = JS_GetTypedArrayByteLength(array);
      total_length += length;
    }

    JS::UniqueChars buf{static_cast<char *>(JS_malloc(cx, total_length))};
    if (!buf) {
      JS_ReportOutOfMemory(cx);
      return false;
    }

    size_t offset = 0;
    // In this loop we are inserting each entry in `contents` into `buf`
    for (uint32_t index = 0; index < contentsLength; index++) {
      if (!JS_GetElement(cx, contents, index, &val)) {
        return false;
      }
      JSObject *array = &val.toObject();
      bool is_shared;
      size_t length = JS_GetTypedArrayByteLength(array);
      JS::AutoCheckCannotGC nogc(cx);
      auto bytes = reinterpret_cast<char *>(JS_GetUint8ArrayData(array, &is_shared, nogc));
      memcpy(buf.get() + offset, bytes, length);
      offset += length;
    }

    mozilla::DebugOnly foundBodyParser = false;
    MOZ_ASSERT(JS_HasElement(cx, catch_handler, 2, &foundBodyParser));
    MOZ_ASSERT(foundBodyParser);

    // Now we can call parse_body on the result
    JS::RootedValue body_parser(cx);
    if (!JS_GetElement(cx, catch_handler, 2, &body_parser)) {
      return false;
    }
    auto parse_body = (ParseBodyCB *)body_parser.toPrivate();
    return parse_body(cx, self, std::move(buf), offset);
  }

  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, chunk_obj, "value", &val)) {
    return false;
  }

  // The read operation can return anything since this stream comes from the guest
  // If it is not a UInt8Array -- reject with a TypeError
  if (!val.isObject() || !JS_IsUint8Array(&val.toObject())) {
    api::throw_error(cx, FetchErrors::InvalidStreamChunk);
    JS::RootedObject result_promise(cx);
    result_promise =
        &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise)).toObject();
    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise), JS::UndefinedValue());

    return RejectPromiseWithPendingError(cx, result_promise);
  }

  {
    uint32_t contentsLength;
    if (!JS::GetArrayLength(cx, contents, &contentsLength)) {
      return false;
    }
    if (!JS_SetElement(cx, contents, contentsLength, val)) {
      return false;
    }
  }

  // Read the next chunk.
  JS::RootedObject promise(cx, JS::ReadableStreamDefaultReaderRead(cx, reader));
  if (!promise)
    return false;
  return JS::AddPromiseReactions(cx, promise, then_handler, catch_handler);
}

bool RequestOrResponse::content_stream_read_catch_handler(JSContext *cx, JS::HandleObject self,
                                                          JS::HandleValue extra,
                                                          JS::CallArgs args) {
  // The stream errored when being consumed
  // we need to propagate the stream error
  MOZ_ASSERT(extra.isObject());
  JS::RootedObject reader(cx, &extra.toObject());
  JS::RootedValue stream_val(cx);
  if (!JS_GetElement(cx, reader, 1, &stream_val)) {
    return false;
  }
  MOZ_ASSERT(stream_val.isObject());
  JS::RootedObject stream(cx, &stream_val.toObject());
  if (!stream) {
    return false;
  }
  MOZ_ASSERT(JS::IsReadableStream(stream));
#ifdef DEBUG
  bool isError;
  if (!JS::ReadableStreamIsErrored(cx, stream, &isError)) {
    return false;
  }
  MOZ_ASSERT(isError);
#endif
  JS::RootedValue error(cx, JS::ReadableStreamGetStoredError(cx, stream));
  JS_ClearPendingException(cx);
  JS_SetPendingException(cx, error, JS::ExceptionStackBehavior::DoNotCapture);
  JS::RootedObject result_promise(cx);
  result_promise =
      &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise)).toObject();
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise), JS::UndefinedValue());

  return RejectPromiseWithPendingError(cx, result_promise);
}

bool RequestOrResponse::consume_content_stream_for_bodyAll(JSContext *cx, JS::HandleObject self,
                                                           JS::HandleValue stream_val,
                                                           JS::CallArgs args) {
  // The body_parser is stored in the stream object, which we need here as well.
  JS::RootedObject stream(cx, &stream_val.toObject());
  JS::RootedValue body_parser(cx);
  if (!JS_GetElement(cx, stream, 1, &body_parser)) {
    return false;
  }
  MOZ_ASSERT(JS::IsReadableStream(stream));
  if (RequestOrResponse::body_unusable(cx, stream)) {
    api::throw_error(cx, FetchErrors::BodyStreamUnusable);
    JS::RootedObject result_promise(cx);
    result_promise =
        &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise)).toObject();
    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise), JS::UndefinedValue());
    return RejectPromiseWithPendingError(cx, result_promise);
  }
  JS::Rooted<JSObject *> unwrappedReader(
      cx, JS::ReadableStreamGetReader(cx, stream, JS::ReadableStreamReaderMode::Default));
  if (!unwrappedReader) {
    return false;
  }

  // contents is the JS Array we store the stream chunks within, to later convert to
  // arrayBuffer/json/text
  JS::Rooted<JSObject *> contents(cx, JS::NewArrayObject(cx, 0));
  if (!contents) {
    return false;
  }

  JS::RootedValue extra(cx, JS::ObjectValue(*unwrappedReader));
  // TODO: confirm whether this is observable to the JS application
  if (!JS_SetElement(cx, unwrappedReader, 1, stream)) {
    return false;
  }

  // Create handlers for both `then` and `catch`.
  // These are functions with two reserved slots, in which we store all
  // information required to perform the reactions. We store the actually
  // required information on the catch handler, and a reference to that on the
  // then handler. This allows us to reuse these functions for the next read
  // operation in the then handler. The catch handler won't ever have a need to
  // perform another operation in this way.
  JS::RootedObject catch_handler(
      cx, create_internal_method<content_stream_read_catch_handler>(cx, self, extra));
  if (!catch_handler) {
    return false;
  }

  extra.setObject(*catch_handler);
  if (!JS_SetElement(cx, catch_handler, 1, contents)) {
    return false;
  }
  if (!JS_SetElement(cx, catch_handler, 2, body_parser)) {
    return false;
  }
  JS::RootedObject then_handler(
      cx, create_internal_method<content_stream_read_then_handler>(cx, self, extra));
  if (!then_handler) {
    return false;
  }

  // Read the next chunk.
  JS::RootedObject promise(cx, JS::ReadableStreamDefaultReaderRead(cx, unwrappedReader));
  if (!promise) {
    return false;
  }
  return JS::AddPromiseReactions(cx, promise, then_handler, catch_handler);
}

template <RequestOrResponse::BodyReadResult result_type>
bool RequestOrResponse::bodyAll(JSContext *cx, JS::CallArgs args, JS::HandleObject self) {
  // TODO: mark body as consumed when operating on stream, too.
  if (body_used(self)) {
    api::throw_error(cx, FetchErrors::BodyStreamUnusable);
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  JS::RootedObject bodyAll_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!bodyAll_promise) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise),
                      JS::ObjectValue(*bodyAll_promise));

  // If the Request/Response doesn't have a body, empty default results need to
  // be returned.
  if (!has_body(self)) {
    JS::UniqueChars chars;
    if (!parse_body<result_type>(cx, self, std::move(chars), 0)) {
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    args.rval().setObject(*bodyAll_promise);
    return true;
  }

  JS::RootedValue body_parser(cx, JS::PrivateValue((void *)parse_body<result_type>));

  // TODO(performance): don't reify a ReadableStream for body handles—use an AsyncTask instead
  JS::RootedObject stream(cx, body_stream(self));
  if (!stream) {
    stream = create_body_stream(cx, self);
    if (!stream)
      return false;
  }

  if (!JS_SetElement(cx, stream, 1, body_parser)) {
    return false;
  }

  SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyUsed), JS::BooleanValue(true));
  JS::RootedValue extra(cx, JS::ObjectValue(*stream));
  if (!enqueue_internal_method<consume_content_stream_for_bodyAll>(cx, self, extra)) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  args.rval().setObject(*bodyAll_promise);
  return true;
}

bool RequestOrResponse::body_source_pull_algorithm(JSContext *cx, CallArgs args,
                                                   HandleObject source, HandleObject body_owner,
                                                   HandleObject controller) {
  // If the stream has been piped to a TransformStream whose readable end was
  // then passed to a Request or Response as the body, we can just append the
  // entire source body to the destination using a single native hostcall, and
  // then close the source stream, instead of reading and writing it in
  // individual chunks. Note that even in situations where multiple streams are
  // piped to the same destination this is guaranteed to happen in the right
  // order: ReadableStream#pipeTo locks the destination WritableStream until the
  // source ReadableStream is closed/canceled, so only one stream can ever be
  // piped in at the same time.
  RootedObject pipe_dest(cx, streams::NativeStreamSource::piped_to_transform_stream(source));
  if (pipe_dest) {
    if (streams::TransformStream::readable_used_as_body(pipe_dest)) {
      RootedObject dest_owner(cx, streams::TransformStream::owner(pipe_dest));
      MOZ_ASSERT(!JS_IsExceptionPending(cx));
      if (!append_body(cx, dest_owner, body_owner)) {
        return false;
      }

      MOZ_ASSERT(!JS_IsExceptionPending(cx));
      RootedObject stream(cx, streams::NativeStreamSource::stream(source));
      bool success = ReadableStreamClose(cx, stream);
      MOZ_RELEASE_ASSERT(success);

      args.rval().setUndefined();
      MOZ_ASSERT(!JS_IsExceptionPending(cx));
      return true;
    }
  }

  ENGINE->queue_async_task(new BodyFutureTask(source));

  args.rval().setUndefined();
  return true;
}

bool RequestOrResponse::body_source_cancel_algorithm(JSContext *cx, JS::CallArgs args,
                                                     JS::HandleObject stream,
                                                     JS::HandleObject owner,
                                                     JS::HandleValue reason) {
  // TODO: implement or add a comment explaining why no implementation is required.
  args.rval().setUndefined();
  return true;
}

bool reader_for_outgoing_body_then_handler(JSContext *cx, JS::HandleObject body_owner,
                                           JS::HandleValue extra, JS::CallArgs args) {
  JS::RootedObject then_handler(cx, &args.callee());
  // The reader is stored in the catch handler, which we need here as well.
  // So we get that first, then the reader.
  JS::RootedObject catch_handler(cx, &extra.toObject());
  JS::RootedObject reader(cx, &js::GetFunctionNativeReserved(catch_handler, 1).toObject());

  // We're guaranteed to work with a native ReadableStreamDefaultReader here,
  // which in turn is guaranteed to vend {done: bool, value: any} objects to
  // read promise then callbacks.
  JS::RootedObject chunk_obj(cx, &args[0].toObject());
  JS::RootedValue done_val(cx);
  if (!JS_GetProperty(cx, chunk_obj, "done", &done_val))
    return false;

  if (done_val.toBoolean()) {
    return finish_outgoing_body_streaming(cx, body_owner);
  }

  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, chunk_obj, "value", &val))
    return false;

  // The read operation returned something that's not a Uint8Array
  if (!val.isObject() || !JS_IsUint8Array(&val.toObject())) {
    // reject the request promise
    if (Request::is_instance(body_owner)) {
      JS::RootedObject response_promise(cx, Request::response_promise(body_owner));

      api::throw_error(cx, FetchErrors::InvalidStreamChunk);
      return RejectPromiseWithPendingError(cx, response_promise);
    }

    // TODO: should we also create a rejected promise if a response reads something that's not a
    // Uint8Array?
    fprintf(stderr, "Error: read operation on body ReadableStream didn't respond with a "
                    "Uint8Array. Received value: ");
    ENGINE->dump_value(val, stderr);
    return false;
  }

  RootedObject array(cx, &val.toObject());
  size_t length = JS_GetTypedArrayByteLength(array);
  bool is_shared;
  RootedObject buffer(cx, JS_GetArrayBufferViewBuffer(cx, array, &is_shared));
  MOZ_ASSERT(!is_shared);
  auto bytes = static_cast<uint8_t *>(StealArrayBufferContents(cx, buffer));
  // TODO: change this to write in chunks, respecting backpressure.
  auto body = RequestOrResponse::outgoing_body_handle(body_owner);
  auto res = body->write_all(bytes, length);
  js_free(bytes);

  // Needs to be outside the nogc block in case we need to create an exception.
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  // Read the next chunk.
  JS::RootedObject promise(cx, JS::ReadableStreamDefaultReaderRead(cx, reader));
  if (!promise) {
    return false;
  }

  return JS::AddPromiseReactions(cx, promise, then_handler, catch_handler);
}

bool reader_for_outgoing_body_catch_handler(JSContext *cx, JS::HandleObject body_owner,
                                            JS::HandleValue extra, JS::CallArgs args) {
  fetch_event::FetchEvent::decrease_interest();

  // TODO: check if this should create a rejected promise instead, so an
  // in-content handler for unhandled rejections could deal with it. The body
  // stream errored during the streaming send. Not much we can do, but at least
  // close the stream, and warn.
  fprintf(stderr, "Warning: body ReadableStream closed during body streaming. Exception: ");
  ENGINE->dump_value(args.get(0), stderr);

  return finish_outgoing_body_streaming(cx, body_owner);
}

bool RequestOrResponse::maybe_stream_body(JSContext *cx, JS::HandleObject body_owner,
                                          host_api::HttpOutgoingBodyOwner *destination,
                                          bool *requires_streaming) {
  *requires_streaming = false;
  if (!has_body(body_owner)) {
    return true;
  }

  // First, handle direct forwarding of incoming bodies.
  // Those can be handled by direct use of async tasks and the host API, without needing
  // to use JS streams at all.
  if (is_incoming(body_owner)) {
    auto *source_body = incoming_body_handle(body_owner);
    auto *dest_body = destination->body().unwrap();
    auto res = dest_body->append(ENGINE, source_body, finish_outgoing_body_streaming, nullptr);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
    MOZ_RELEASE_ASSERT(RequestOrResponse::mark_body_used(cx, body_owner));

    *requires_streaming = true;
    return true;
  }

  JS::RootedObject stream(cx, body_stream(body_owner));
  if (!stream) {
    return true;
  }

  if (body_unusable(cx, stream)) {
    return api::throw_error(cx, FetchErrors::BodyStreamUnusable);
  }

  // If the body stream is backed by an HTTP body handle, we can directly pipe
  // that handle into the body we're about to send.
  if (streams::NativeStreamSource::stream_is_body(cx, stream)) {
    MOZ_ASSERT(!is_incoming(body_owner));
    // First, directly append the source's body to the target's and lock the stream.
    JS::RootedObject stream_source(cx, streams::NativeStreamSource::get_stream_source(cx, stream));
    JS::RootedObject source_owner(cx, streams::NativeStreamSource::owner(stream_source));
    if (!append_body(cx, body_owner, source_owner)) {
      return false;
    }

    *requires_streaming = true;
    return true;
  }

  JS::RootedObject reader(
      cx, JS::ReadableStreamGetReader(cx, stream, JS::ReadableStreamReaderMode::Default));
  if (!reader)
    return false;

  // Create handlers for both `then` and `catch`.
  // These are functions with two reserved slots, in which we store all
  // information required to perform the reactions. We store the actually
  // required information on the catch handler, and a reference to that on the
  // then handler. This allows us to reuse these functions for the next read
  // operation in the then handler. The catch handler won't ever have a need to
  // perform another operation in this way.
  JS::RootedObject catch_handler(cx);
  JS::RootedValue extra(cx, JS::ObjectValue(*reader));
  catch_handler =
      create_internal_method<reader_for_outgoing_body_catch_handler>(cx, body_owner, extra);
  if (!catch_handler)
    return false;

  JS::RootedObject then_handler(cx);
  extra.setObject(*catch_handler);
  then_handler =
      create_internal_method<reader_for_outgoing_body_then_handler>(cx, body_owner, extra);
  if (!then_handler)
    return false;

  JS::RootedObject promise(cx, JS::ReadableStreamDefaultReaderRead(cx, reader));
  if (!promise)
    return false;
  if (!JS::AddPromiseReactions(cx, promise, then_handler, catch_handler))
    return false;

  *requires_streaming = true;
  return true;
}

JSObject *RequestOrResponse::create_body_stream(JSContext *cx, JS::HandleObject owner) {
  MOZ_ASSERT(!body_stream(owner));
  MOZ_ASSERT(has_body(owner));
  JS::RootedObject source(cx, streams::NativeStreamSource::create(
                                  cx, owner, JS::UndefinedHandleValue, body_source_pull_algorithm,
                                  body_source_cancel_algorithm));
  if (!source)
    return nullptr;

  // Create a readable stream with a highwater mark of 0.0 to prevent an eager
  // pull. With the default HWM of 1.0, the streams implementation causes a
  // pull, which means we enqueue a read from the host handle, which we quite
  // often have no interest in at all.
  JS::RootedObject body_stream(cx, JS::NewReadableDefaultStreamObject(cx, source, nullptr, 0.0));
  if (!body_stream) {
    return nullptr;
  }

  // If the body has already been used without being reified as a ReadableStream,
  // lock the stream immediately.
  if (body_used(owner)) {
    MOZ_RELEASE_ASSERT(streams::NativeStreamSource::lock_stream(cx, body_stream));
  }

  JS_SetReservedSlot(owner, static_cast<uint32_t>(Slots::BodyStream),
                     JS::ObjectValue(*body_stream));
  return body_stream;
}

bool RequestOrResponse::body_get(JSContext *cx, JS::CallArgs args, JS::HandleObject self,
                                 bool create_if_undefined) {
  MOZ_ASSERT(is_instance(self));
  if (!has_body(self)) {
    args.rval().setNull();
    return true;
  }

  JS::RootedObject body_stream(cx, RequestOrResponse::body_stream(self));
  if (!body_stream && create_if_undefined) {
    body_stream = create_body_stream(cx, self);
    if (!body_stream)
      return false;
  }

  args.rval().setObjectOrNull(body_stream);
  return true;
}

host_api::HttpRequest *Request::request_handle(JSObject *obj) {
  auto base = RequestOrResponse::handle(obj);
  return reinterpret_cast<host_api::HttpRequest *>(base);
}

host_api::HttpOutgoingRequest *Request::outgoing_handle(JSObject *obj) {
  auto base = RequestOrResponse::handle(obj);
  MOZ_ASSERT(base->is_outgoing());
  return reinterpret_cast<host_api::HttpOutgoingRequest *>(base);
}

host_api::HttpIncomingRequest *Request::incoming_handle(JSObject *obj) {
  auto base = RequestOrResponse::handle(obj);
  MOZ_ASSERT(base->is_incoming());
  return reinterpret_cast<host_api::HttpIncomingRequest *>(base);
}

JSObject *Request::response_promise(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return &JS::GetReservedSlot(obj, static_cast<uint32_t>(Request::Slots::ResponsePromise))
              .toObject();
}

JSString *Request::method(HandleObject obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Method)).toString();
}

bool Request::method_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().setString(Request::method(self));
  return true;
}

bool Request::url_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().set(RequestOrResponse::url(self));
  return true;
}

bool Request::headers_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JSObject *headers = RequestOrResponse::headers(cx, self);
  if (!headers)
    return false;

  args.rval().setObject(*headers);
  return true;
}

template <RequestOrResponse::BodyReadResult result_type>
bool Request::bodyAll(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return RequestOrResponse::bodyAll<result_type>(cx, args, self);
}

bool Request::body_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return RequestOrResponse::body_get(cx, args, self, RequestOrResponse::is_incoming(self));
}

bool Request::bodyUsed_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  args.rval().setBoolean(RequestOrResponse::body_used(self));
  return true;
}

JSString *GET_atom;

/// https://fetch.spec.whatwg.org/#dom-request-clone
bool Request::clone(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  // clone operation step 1.
  // Let newRequest be a copy of request, except for its body.
  // Note that the spec doesn't say what it means to copy a request, exactly.
  // Since a request only has the fields "method", "url", and "headers", and the "Body" mixin,
  // we copy those three fields in this step.
  RootedObject new_request(cx, create(cx));
  if (!new_request) {
    return false;
  }
  init_slots(new_request);

  RootedValue cloned_headers_val(cx, JS::NullValue());
  RootedObject headers(cx, RequestOrResponse::maybe_headers(self));
  if (headers) {
    RootedValue headers_val(cx, ObjectValue(*headers));
    JSObject *cloned_headers = Headers::create(cx, headers_val, Headers::guard(headers));
    if (!cloned_headers) {
      return false;
    }
    cloned_headers_val.set(ObjectValue(*cloned_headers));
  } else if (RequestOrResponse::handle(self)) {
    auto handle = RequestOrResponse::headers_handle_clone(cx, self);
    JSObject *cloned_headers =
        Headers::create(cx, handle.release(),
                        RequestOrResponse::is_incoming(self) ? Headers::HeadersGuard::Immutable
                                                             : Headers::HeadersGuard::Request);
    if (!cloned_headers) {
      return false;
    }
    cloned_headers_val.set(ObjectValue(*cloned_headers));
  }

  SetReservedSlot(new_request, static_cast<uint32_t>(Slots::Headers), cloned_headers_val);
  Value url_val = GetReservedSlot(self, static_cast<uint32_t>(Slots::URL));
  SetReservedSlot(new_request, static_cast<uint32_t>(Slots::URL), url_val);
  Value method_val = JS::StringValue(method(self));
  ENGINE->dump_value(method_val, stderr);
  SetReservedSlot(new_request, static_cast<uint32_t>(Slots::Method), method_val);

  // clone operation step 2.
  // If request’s body is non-null, set newRequest’s body to the result of cloning request’s body.
  RootedObject new_body(cx);
  auto has_body = RequestOrResponse::has_body(self);
  if (!has_body) {
    args.rval().setObject(*new_request);
    return true;
  }

  // Here we get the current request's body stream and call ReadableStream.prototype.tee to
  // get two streams for the same content.
  // One of these is then used to replace the current request's body, the other is used as
  // the body of the clone.
  JS::RootedObject body_stream(cx, RequestOrResponse::body_stream(self));
  if (!body_stream) {
    body_stream = RequestOrResponse::create_body_stream(cx, self);
    if (!body_stream) {
      return false;
    }
  }

  if (RequestOrResponse::body_unusable(cx, body_stream)) {
    return api::throw_error(cx, FetchErrors::BodyStreamUnusable);
  }

  RootedObject self_body(cx);
  if (!ReadableStreamTee(cx, body_stream, &self_body, &new_body)) {
    return false;
  }

  SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyStream), ObjectValue(*self_body));
  SetReservedSlot(new_request, static_cast<uint32_t>(Slots::BodyStream), ObjectValue(*new_body));
  SetReservedSlot(new_request, static_cast<uint32_t>(Slots::HasBody), JS::BooleanValue(true));

  args.rval().setObject(*new_request);
  return true;
}

const JSFunctionSpec Request::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Request::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Request::methods[] = {
    JS_FN("arrayBuffer", Request::bodyAll<RequestOrResponse::BodyReadResult::ArrayBuffer>, 0,
          JSPROP_ENUMERATE),
    JS_FN("json", Request::bodyAll<RequestOrResponse::BodyReadResult::JSON>, 0, JSPROP_ENUMERATE),
    JS_FN("text", Request::bodyAll<RequestOrResponse::BodyReadResult::Text>, 0, JSPROP_ENUMERATE),
    JS_FN("clone", Request::clone, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec Request::properties[] = {
    JS_PSG("method", Request::method_get, JSPROP_ENUMERATE),
    JS_PSG("url", Request::url_get, JSPROP_ENUMERATE),
    JS_PSG("headers", Request::headers_get, JSPROP_ENUMERATE),
    JS_PSG("body", Request::body_get, JSPROP_ENUMERATE),
    JS_PSG("bodyUsed", Request::bodyUsed_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Request", JSPROP_READONLY),
    JS_PS_END,
};

bool Request::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
    return false;
  }

  // Initialize a pinned (i.e., never-moved, living forever) atom for the
  // default HTTP method.
  GET_atom = JS_AtomizeAndPinString(cx, "GET");
  return !!GET_atom;
}

void Request::init_slots(JSObject *requestInstance) {
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Request),
                      JS::PrivateValue(nullptr));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Headers), JS::NullValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::BodyStream), JS::NullValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::HasBody), JS::FalseValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::BodyUsed), JS::FalseValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Method),
                      JS::StringValue(GET_atom));
}

/**
 * Create a new Request object, roughly according to
 * https://fetch.spec.whatwg.org/#dom-request
 *
 * "Roughly" because not all aspects of Request handling make sense in StarlingMonkey.
 * The places where we deviate from the spec are called out inline.
 */
bool Request::initialize(JSContext *cx, JS::HandleObject request, JS::HandleValue input,
                         JS::HandleValue init_val, Headers::HeadersGuard guard) {
  init_slots(request);
  JS::RootedString url_str(cx);
  JS::RootedString method_str(cx);
  bool method_needs_normalization = false;

  JS::RootedObject input_request(cx);
  JS::RootedObject input_headers(cx);
  bool input_has_body = false;

  // 1.  Let `request` be null.
  // 4.  Let `signal` be null.
  // (implicit)

  // 2.  Let `fallbackMode` be null.
  // (N/A)

  // 3.  Let `baseURL` be this’s relevant settings object’s API base URL.
  // (implicit)

  // 6.  Otherwise:
  // (reordered because it's easier to check is_instance and otherwise
  // stringify.)
  if (is_instance(input)) {
    input_request = &input.toObject();
    input_has_body = RequestOrResponse::has_body(input_request);

    // 1.  Assert: `input` is a `Request` object.
    // 2.  Set `request` to `input`’s request.
    // (implicit)

    // 3.  Set `signal` to `input`’s signal.
    // (signals not yet supported)

    // 12.  Set `request` to a new request with the following properties:
    // (moved into step 6 because we can leave everything at the default values
    // if step 5 runs.) URL: `request`’s URL. Will actually be applied below.
    url_str = RequestOrResponse::url(input_request).toString();

    // method: `request`’s method.
    method_str = Request::method(input_request);

    // referrer: `request`’s referrer.
    // TODO: evaluate whether we want to implement support for setting the
    // `referer` [sic] header based on this or not.

    // cache mode: `request`’s cache mode.
    // TODO: implement support for cache mode-based headers setting.

    // header list: A copy of `request`’s header list.
    // Note: copying the headers is postponed, see step 32 below.
    // Note: we're forcing reification of the input request's headers here. That is suboptimal,
    // because we might end up not using them. Additionally, if the headers are represented
    // internally as a handle (e.g. because the input is an incoming request), we would in
    // principle not need to ever reify it just to get a clone.
    // Applying these optimizations is somewhat complex though, so for now we're not doing so.
    if (!(input_headers = RequestOrResponse::headers(cx, input_request))) {
      return false;
    }

    // The following properties aren't applicable:
    // unsafe-request flag: Set.
    // client: This’s relevant settings object.
    // window: `window`.
    // priority: `request`’s priority
    // origin: `request`’s origin.
    // referrer policy: `request`’s referrer policy.
    // mode: `request`’s mode.
    // credentials mode: `request`’s credentials mode.
    // redirect mode: `request`’s redirect mode.
    // integrity metadata: `request`’s integrity metadata.
    // keepalive: `request`’s keepalive.
    // reload-navigation flag: `request`’s reload-navigation flag.
    // history-navigation flag: `request`’s history-navigation flag.
    // URL list: A clone of `request`’s URL list.
  }

  // 5.  If `input` is a string, then:
  else {
    // 1.  Let `parsedURL` be the result of parsing `input` with `baseURL`.
    JS::RootedObject url_instance(
        cx, JS_NewObjectWithGivenProto(cx, &url::URL::class_, url::URL::proto_obj));
    if (!url_instance)
      return false;

    JS::RootedObject parsedURL(
        cx, url::URL::create(cx, url_instance, input, worker_location::WorkerLocation::url));

    // 2.  If `parsedURL` is failure, then throw a `TypeError`.
    if (!parsedURL) {
      return false;
    }

    // 3.  If `parsedURL` includes credentials, then throw a `TypeError`.
    // (N/A)

    // 4.  Set `request` to a new request whose URL is `parsedURL`.
    // Instead, we store `url_str` to apply below.
    JS::RootedValue url_val(cx, JS::ObjectValue(*parsedURL));
    url_str = JS::ToString(cx, url_val);
    if (!url_str) {
      return false;
    }

    // 5.  Set `fallbackMode` to "`cors`".
    // (N/A)
  }

  // 7.  Let `origin` be this’s relevant settings object’s origin.
  // 8.  Let `window` be "`client`".
  // 9.  If `request`’s window is an environment settings object and its origin
  // is same origin with `origin`, then set `window` to `request`’s window.
  // 10.  If `init`["window"] exists and is non-null, then throw a `TypeError.
  // 11.  If `init`["window"] exists, then set `window` to "`no-window`".
  // (N/A)

  // Extract all relevant properties from the init object.
  // TODO: evaluate how much we care about precisely matching evaluation order.
  // If "a lot", we need to make sure that all side effects that value
  // conversions might trigger occur in the right order—presumably by running
  // them all right here as WebIDL bindings would.
  JS::RootedValue method_val(cx);
  JS::RootedValue headers_val(cx);
  JS::RootedValue body_val(cx);

  bool is_get = true;
  bool is_get_or_head = is_get;
  host_api::HostString method;

  if (init_val.isObject()) {
    // TODO: investigate special-casing native Request objects here to not reify headers and bodies.
    JS::RootedObject init(cx, init_val.toObjectOrNull());
    if (!JS_GetProperty(cx, init, "method", &method_val) ||
        !JS_GetProperty(cx, init, "headers", &headers_val) ||
        !JS_GetProperty(cx, init, "body", &body_val)) {
      return false;
    }
  } else if (!init_val.isNullOrUndefined()) {
    api::throw_error(cx, FetchErrors::InvalidInitArg, "Request constructor");
    return false;
  }

  // 13.  If `init` is not empty, then:
  // 1.  If `request`’s mode is "`navigate`", then set it to "`same-origin`".
  // 2.  Unset `request`’s reload-navigation flag.
  // 3.  Unset `request`’s history-navigation flag.
  // 4.  Set `request`’s origin to "`client`".
  // 5.  Set `request`’s referrer to "`client`".
  // 6.  Set `request`’s referrer policy to the empty string.
  // 7.  Set `request`’s URL to `request`’s current URL.
  // 8.  Set `request`’s URL list to « `request`’s URL ».
  // (N/A)

  // 14.  If `init["referrer"]` exists, then:
  // TODO: implement support for referrer application.
  // 1.  Let `referrer` be `init["referrer"]`.
  // 2.  If `referrer` is the empty string, then set `request`’s referrer to
  // "`no-referrer`".
  // 3.  Otherwise:
  //   1.  Let `parsedReferrer` be the result of parsing `referrer` with
  //   `baseURL`.
  //   2.  If `parsedReferrer` is failure, then throw a `TypeError`.

  //   3.  If one of the following is true
  //     *   `parsedReferrer`’s scheme is "`about`" and path is the string
  //     "`client`"
  //     *   `parsedReferrer`’s origin is not same origin with `origin`
  //     then set `request`’s referrer to "`client`".
  //   (N/A)

  //   4.  Otherwise, set `request`’s referrer to `parsedReferrer`.

  // 15.  If `init["referrerPolicy"]` exists, then set `request`’s referrer
  // policy to it.
  // 16.  Let `mode` be `init["mode"]` if it exists, and `fallbackMode`
  // otherwise.
  // 17.  If `mode` is "`navigate`", then throw a `TypeError`.
  // 18.  If `mode` is non-null, set `request`’s mode to `mode`.
  // 19.  If `init["credentials"]` exists, then set `request`’s credentials mode
  // to it. (N/A)

  // 20.  If `init["cache"]` exists, then set `request`’s cache mode to it.
  // TODO: implement support for cache mode application.

  // 21.  If `request`’s cache mode is "`only-if-cached`" and `request`’s mode
  // is _not_
  //      "`same-origin`", then throw a TypeError.
  // 22.  If `init["redirect"]` exists, then set `request`’s redirect mode to
  // it.
  // 23.  If `init["integrity"]` exists, then set `request`’s integrity metadata
  // to it.
  // 24.  If `init["keepalive"]` exists, then set `request`’s keepalive to it.
  // (N/A)

  // 25.  If `init["method"]` exists, then:
  if (!method_val.isUndefined()) {
    // 1.  Let `method` be `init["method"]`.
    method_str = JS::ToString(cx, method_val);
    if (!method_str) {
      return false;
    }

    // 2.  If `method` is not a method or `method` is a forbidden method, then
    // throw a
    //     `TypeError`.
    // TODO: evaluate whether we should barr use of methods forbidden by the
    // WHATWG spec.

    // 3.  Normalize `method`.
    // Delayed to below to reduce some code duplication.
    method_needs_normalization = true;

    // 4.  Set `request`’s method to `method`.
    // Done below, unified with the non-init case.
  }

  // Apply the method derived in step 6 or 25.
  // This only needs to happen if the method was set explicitly and isn't the
  // default `GET`.
  if (method_str && !JS_StringEqualsLiteral(cx, method_str, "GET", &is_get)) {
    return false;
  }

  if (!is_get) {
    method = core::encode(cx, method_str);
    if (!method) {
      return false;
    }

    if (method_needs_normalization) {
      if (normalize_http_method(method.begin())) {
        // Replace the JS string with the normalized name.
        method_str = JS_NewStringCopyN(cx, method.begin(), method.len);
        if (!method_str) {
          return false;
        }
      }
    }

    is_get_or_head = strcmp(method.begin(), "GET") == 0 || strcmp(method.begin(), "HEAD") == 0;
  }

  // 26.  If `init["signal"]` exists, then set `signal` to it.
  // (signals NYI)

  // 27.  Set this’s request to `request`.
  // (implicit)

  // 28.  Set this’s signal to a new `AbortSignal` object with this’s relevant
  // Realm.
  // 29.  If `signal` is not null, then make this’s signal follow `signal`.
  // (signals NYI)

  // 30.  Set this’s headers to a new `Headers` object with this’s relevant
  // Realm, whose header list is `request`’s header list and guard is
  // "`request`". (implicit)

  // 31.  If this’s requests mode is "`no-cors`", then:
  // 1.  If this’s requests method is not a CORS-safelisted method, then throw a
  // `TypeError`.
  // 2.  Set this’s headers’s guard to "`request-no-cors`".
  // (N/A)

  // 32.  If `init` is not empty, then:
  // 1.  Let `headers` be a copy of this’s headers and its associated header
  // list.
  // 2.  If `init["headers"]` exists, then set `headers` to `init["headers"]`.
  // 3.  Empty this’s headers’s header list.
  // 4.  If `headers` is a `Headers` object, then for each `header` in its
  // header list, append (`header`’s name, `header`’s value) to this’s headers.
  // 5.  Otherwise, fill this’s headers with `headers`.
  // Note: the substeps of 32 are somewhat convoluted because they don't just
  // serve to ensure that the contents of `init["headers"]` are added to the
  // request's headers, but also that all headers, including those from the
  // `input` object are sanitized in accordance with the request's `mode`. Since
  // we don't implement this sanitization, we do a much simpler thing: if
  // `init["headers"]` exists, create the request's `headers` from that,
  // otherwise create it from the `init` object's `headers`, or create a new,
  // empty one.
  JS::RootedObject headers(cx);

  if (headers_val.isUndefined() && input_headers) {
    headers_val.setObject(*input_headers);
  }
  if (!headers_val.isUndefined()) {
    // incoming request headers are always immutable
    headers = Headers::create(cx, headers_val, guard);
    if (!headers) {
      return false;
    }
  }

  // 33.  Let `inputBody` be `input`’s requests body if `input` is a `Request`
  // object;
  //      otherwise null.
  // (skipped)

  // 34.  If either `init["body"]` exists and is non-null or `inputBody` is
  // non-null, and `request`’s method is ``GET`` or ``HEAD``, then throw a
  // TypeError.
  if ((input_has_body || !body_val.isNullOrUndefined()) && is_get_or_head) {
    api::throw_error(cx, FetchErrors::NonBodyRequestWithBody);
    return false;
  }

  // 35.  Let `initBody` be null.
  // (skipped)

  // Note: steps 36-41 boil down to "if there's an init body, use that.
  // Otherwise, if there's an input body, use that, but proxied through a
  // TransformStream to make sure it's not consumed by something else in the
  // meantime." Given that, we're restructuring things quite a bit below.

  auto url = core::encode(cx, url_str);
  if (!url) {
    return false;
  }

  // Store the URL, method, and headers derived above on the JS object.
  RequestOrResponse::set_url(request, StringValue(url_str));
  if (!is_get) {
    // Only store the method if it's not the default `GET`, because in that case
    // `method_str` might not be initialized.
    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::Method), JS::StringValue(method_str));
  }
  JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::Headers),
                      JS::ObjectOrNullValue(headers));

  // 36.  If `init["body"]` exists and is non-null, then:
  if (!body_val.isNullOrUndefined()) {
    // 1.  Let `Content-Type` be null.
    // 2.  Set `initBody` and `Content-Type` to the result of extracting
    // `init["body"]`, with
    //     `keepalive` set to `request`’s keepalive.
    // 3.  If `Content-Type` is non-null and this’s headers’s header list does
    // not contain
    //     ``Content-Type``, then append (``Content-Type``, `Content-Type`) to
    //     this’s headers.
    // Note: these steps are all inlined into RequestOrResponse::extract_body.
    if (!RequestOrResponse::extract_body(cx, request, body_val)) {
      return false;
    }
  } else if (input_has_body) {
    // 37.  Let `inputOrInitBody` be `initBody` if it is non-null; otherwise
    // `inputBody`. (implicit)
    // 38.  If `inputOrInitBody` is non-null and `inputOrInitBody`’s source is
    // null, then:
    // 1.  If this’s requests mode is neither "`same-origin`" nor "`cors`", then
    // throw a `TypeError.
    // 2.  Set this’s requests use-CORS-preflight flag.
    // (N/A)
    // 39.  Let `finalBody` be `inputOrInitBody`.
    // 40.  If `initBody` is null and `inputBody` is non-null, then:
    // (implicit)
    // 1.  If `input` is unusable, then throw a TypeError.
    // 2.  Set `finalBody` to the result of creating a proxy for `inputBody`.

    // All the above steps boil down to "if the input request has an unusable
    // body, throw. Otherwise, use the body." Our implementation is a bit more
    // involved, because we might not have a body reified as a ReadableStream at
    // all, in which case we can directly append the input body to the new
    // request's body with a single hostcall.

    JS::RootedObject inputBody(cx, RequestOrResponse::body_stream(input_request));

    // Throw an error if the input request's body isn't usable.
    if (RequestOrResponse::body_used(input_request) ||
        (inputBody && RequestOrResponse::body_unusable(cx, inputBody))) {
      api::throw_error(cx, FetchErrors::BodyStreamUnusable);
      return false;
    }

    if (!inputBody) {
      // If `inputBody` is null, that means that it was never created, and hence
      // content can't have access to it. Instead of reifying it here to pass it
      // into a TransformStream, we just append the body on the host side and
      // mark it as used on the input Request.
      RequestOrResponse::append_body(cx, request, input_request);
    } else {
      inputBody = streams::TransformStream::create_rs_proxy(cx, inputBody);
      if (!inputBody) {
        return false;
      }

      streams::TransformStream::set_readable_used_as_body(cx, inputBody, request);
      JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::BodyStream),
                          JS::ObjectValue(*inputBody));
    }

    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::HasBody), JS::BooleanValue(true));
  }

  // 41.  Set this’s requests body to `finalBody`.
  // (implicit)

  return true;
}

JSObject *Request::create(JSContext *cx) {
  JS::RootedObject requestInstance(
      cx, JS_NewObjectWithGivenProto(cx, &Request::class_, Request::proto_obj));
  return requestInstance;
}

bool Request::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("Request", 1);
  JS::RootedObject request(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!request || !initialize(cx, request, args[0], args.get(1), Headers::HeadersGuard::Request)) {
    return false;
  }

  args.rval().setObject(*request);
  return true;
}

// Needed for uniform access to Request and Response slots.
static_assert((int)Response::Slots::BodyStream == (int)Request::Slots::BodyStream);
static_assert((int)Response::Slots::HasBody == (int)Request::Slots::HasBody);
static_assert((int)Response::Slots::BodyUsed == (int)Request::Slots::BodyUsed);
static_assert((int)Response::Slots::Headers == (int)Request::Slots::Headers);
static_assert((int)Response::Slots::Response == (int)Request::Slots::Request);

host_api::HttpResponse *Response::response_handle(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return static_cast<host_api::HttpResponse *>(RequestOrResponse::handle(obj));
}

uint16_t Response::status(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return (uint16_t)JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Status)).toInt32();
}

JSString *Response::status_message(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::StatusMessage)).toString();
}

// TODO(jake): Remove this when the reason phrase host-call is implemented
void Response::set_status_message_from_code(JSContext *cx, JSObject *obj, uint16_t code) {
  auto phrase = "";

  switch (code) {
  case 100: // 100 Continue - https://tools.ietf.org/html/rfc7231#section-6.2.1
    phrase = "Continue";
    break;
  case 101: // 101 Switching Protocols - https://tools.ietf.org/html/rfc7231#section-6.2.2
    phrase = "Switching Protocols";
    break;
  case 102: // 102 Processing - https://tools.ietf.org/html/rfc2518
    phrase = "Processing";
    break;
  case 200: // 200 OK - https://tools.ietf.org/html/rfc7231#section-6.3.1
    phrase = "OK";
    break;
  case 201: // 201 Created - https://tools.ietf.org/html/rfc7231#section-6.3.2
    phrase = "Created";
    break;
  case 202: // 202 Accepted - https://tools.ietf.org/html/rfc7231#section-6.3.3
    phrase = "Accepted";
    break;
  case 203: // 203 Non-Authoritative Information - https://tools.ietf.org/html/rfc7231#section-6.3.4
    phrase = "Non Authoritative Information";
    break;
  case 204: // 204 No Content - https://tools.ietf.org/html/rfc7231#section-6.3.5
    phrase = "No Content";
    break;
  case 205: // 205 Reset Content - https://tools.ietf.org/html/rfc7231#section-6.3.6
    phrase = "Reset Content";
    break;
  case 206: // 206 Partial Content - https://tools.ietf.org/html/rfc7233#section-4.1
    phrase = "Partial Content";
    break;
  case 207: // 207 Multi-Status - https://tools.ietf.org/html/rfc4918
    phrase = "Multi-Status";
    break;
  case 208: // 208 Already Reported - https://tools.ietf.org/html/rfc5842
    phrase = "Already Reported";
    break;
  case 226: // 226 IM Used - https://tools.ietf.org/html/rfc3229
    phrase = "IM Used";
    break;
  case 300: // 300 Multiple Choices - https://tools.ietf.org/html/rfc7231#section-6.4.1
    phrase = "Multiple Choices";
    break;
  case 301: // 301 Moved Permanently - https://tools.ietf.org/html/rfc7231#section-6.4.2
    phrase = "Moved Permanently";
    break;
  case 302: // 302 Found - https://tools.ietf.org/html/rfc7231#section-6.4.3
    phrase = "Found";
    break;
  case 303: // 303 See Other - https://tools.ietf.org/html/rfc7231#section-6.4.4
    phrase = "See Other";
    break;
  case 304: // 304 Not Modified - https://tools.ietf.org/html/rfc7232#section-4.1
    phrase = "Not Modified";
    break;
  case 305: // 305 Use Proxy - https://tools.ietf.org/html/rfc7231#section-6.4.5
    phrase = "Use Proxy";
    break;
  case 307: // 307 Temporary Redirect - https://tools.ietf.org/html/rfc7231#section-6.4.7
    phrase = "Temporary Redirect";
    break;
  case 308: // 308 Permanent Redirect - https://tools.ietf.org/html/rfc7238
    phrase = "Permanent Redirect";
    break;
  case 400: // 400 Bad Request - https://tools.ietf.org/html/rfc7231#section-6.5.1
    phrase = "Bad Request";
    break;
  case 401: // 401 Unauthorized - https://tools.ietf.org/html/rfc7235#section-3.1
    phrase = "Unauthorized";
    break;
  case 402: // 402 Payment Required - https://tools.ietf.org/html/rfc7231#section-6.5.2
    phrase = "Payment Required";
    break;
  case 403: // 403 Forbidden - https://tools.ietf.org/html/rfc7231#section-6.5.3
    phrase = "Forbidden";
    break;
  case 404: // 404 Not Found - https://tools.ietf.org/html/rfc7231#section-6.5.4
    phrase = "Not Found";
    break;
  case 405: // 405 Method Not Allowed - https://tools.ietf.org/html/rfc7231#section-6.5.5
    phrase = "Method Not Allowed";
    break;
  case 406: // 406 Not Acceptable - https://tools.ietf.org/html/rfc7231#section-6.5.6
    phrase = "Not Acceptable";
    break;
  case 407: // 407 Proxy Authentication Required - https://tools.ietf.org/html/rfc7235#section-3.2
    phrase = "Proxy Authentication Required";
    break;
  case 408: // 408 Request Timeout - https://tools.ietf.org/html/rfc7231#section-6.5.7
    phrase = "Request Timeout";
    break;
  case 409: // 409 Conflict - https://tools.ietf.org/html/rfc7231#section-6.5.8
    phrase = "Conflict";
    break;
  case 410: // 410 Gone - https://tools.ietf.org/html/rfc7231#section-6.5.9
    phrase = "Gone";
    break;
  case 411: // 411 Length Required - https://tools.ietf.org/html/rfc7231#section-6.5.10
    phrase = "Length Required";
    break;
  case 412: // 412 Precondition Failed - https://tools.ietf.org/html/rfc7232#section-4.2
    phrase = "Precondition Failed";
    break;
  case 413: // 413 Payload Too Large - https://tools.ietf.org/html/rfc7231#section-6.5.11
    phrase = "Payload Too Large";
    break;
  case 414: // 414 URI Too Long - https://tools.ietf.org/html/rfc7231#section-6.5.12
    phrase = "URI Too Long";
    break;
  case 415: // 415 Unsupported Media Type - https://tools.ietf.org/html/rfc7231#section-6.5.13
    phrase = "Unsupported Media Type";
    break;
  case 416: // 416 Range Not Satisfiable - https://tools.ietf.org/html/rfc7233#section-4.4
    phrase = "Range Not Satisfiable";
    break;
  case 417: // 417 Expectation Failed - https://tools.ietf.org/html/rfc7231#section-6.5.14
    phrase = "Expectation Failed";
    break;
  case 418: // 418 I'm a teapot - https://tools.ietf.org/html/rfc2324
    phrase = "I'm a teapot";
    break;
  case 421: // 421 Misdirected Request - http://tools.ietf.org/html/rfc7540#section-9.1.2
    phrase = "Misdirected Request";
    break;
  case 422: // 422 Unprocessable Entity - https://tools.ietf.org/html/rfc4918
    phrase = "Unprocessable Entity";
    break;
  case 423: // 423 Locked - https://tools.ietf.org/html/rfc4918
    phrase = "Locked";
    break;
  case 424: // 424 Failed Dependency - https://tools.ietf.org/html/rfc4918
    phrase = "Failed Dependency";
    break;
  case 426: // 426 Upgrade Required - https://tools.ietf.org/html/rfc7231#section-6.5.15
    phrase = "Upgrade Required";
    break;
  case 428: // 428 Precondition Required - https://tools.ietf.org/html/rfc6585
    phrase = "Precondition Required";
    break;
  case 429: // 429 Too Many Requests - https://tools.ietf.org/html/rfc6585
    phrase = "Too Many Requests";
    break;
  case 431: // 431 Request Header Fields Too Large - https://tools.ietf.org/html/rfc6585
    phrase = "Request Header Fields Too Large";
    break;
  case 451: // 451 Unavailable For Legal Reasons - http://tools.ietf.org/html/rfc7725
    phrase = "Unavailable For Legal Reasons";
    break;
  case 500: // 500 Internal Server Error - https://tools.ietf.org/html/rfc7231#section-6.6.1
    phrase = "Internal Server Error";
    break;
  case 501: // 501 Not Implemented - https://tools.ietf.org/html/rfc7231#section-6.6.2
    phrase = "Not Implemented";
    break;
  case 502: // 502 Bad Gateway - https://tools.ietf.org/html/rfc7231#section-6.6.3
    phrase = "Bad Gateway";
    break;
  case 503: // 503 Service Unavailable - https://tools.ietf.org/html/rfc7231#section-6.6.4
    phrase = "Service Unavailable";
    break;
  case 504: // 504 Gateway Timeout - https://tools.ietf.org/html/rfc7231#section-6.6.5
    phrase = "Gateway Timeout";
    break;
  case 505: // 505 HTTP Version Not Supported - https://tools.ietf.org/html/rfc7231#section-6.6.6
    phrase = "HTTP Version Not Supported";
    break;
  case 506: // 506 Variant Also Negotiates - https://tools.ietf.org/html/rfc2295
    phrase = "Variant Also Negotiates";
    break;
  case 507: // 507 Insufficient Storage - https://tools.ietf.org/html/rfc4918
    phrase = "Insufficient Storage";
    break;
  case 508: // 508 Loop Detected - https://tools.ietf.org/html/rfc5842
    phrase = "Loop Detected";
    break;
  case 510: // 510 Not Extended - https://tools.ietf.org/html/rfc2774
    phrase = "Not Extended";
    break;
  case 511: // 511 Network Authentication Required - https://tools.ietf.org/html/rfc6585
    phrase = "Network Authentication Required";
    break;
  default:
    phrase = "";
    break;
  }
  JS::SetReservedSlot(obj, static_cast<uint32_t>(Slots::StatusMessage),
                      JS::StringValue(JS_NewStringCopyN(cx, phrase, strlen(phrase))));
}

bool Response::ok_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  uint16_t status = Response::status(self);
  args.rval().setBoolean(status >= 200 && status < 300);
  return true;
}

bool Response::status_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().setInt32(status(self));
  return true;
}

bool Response::statusText_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().setString(status_message(self));
  return true;
}

bool Response::url_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().set(RequestOrResponse::url(self));
  return true;
}

namespace {
JSString *type_default_atom;
JSString *type_error_atom;
} // namespace

bool Response::type_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().setString(status(self) == 0 ? type_error_atom : type_default_atom);
  return true;
}

bool Response::redirected_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().setBoolean(
      JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Redirected)).toBoolean());
  return true;
}

bool Response::headers_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JSObject *headers = RequestOrResponse::headers(cx, self);
  if (!headers)
    return false;

  args.rval().setObject(*headers);
  return true;
}

template <RequestOrResponse::BodyReadResult result_type>
bool Response::bodyAll(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return RequestOrResponse::bodyAll<result_type>(cx, args, self);
}

bool Response::body_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return RequestOrResponse::body_get(cx, args, self, true);
}

bool Response::bodyUsed_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  args.rval().setBoolean(RequestOrResponse::body_used(self));
  return true;
}

// https://fetch.spec.whatwg.org/#dom-response-redirect
// [NewObject] static Response redirect(USVString url, optional unsigned short status = 302);
bool Response::redirect(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "redirect", 1)) {
    return false;
  }

  // 1. Let parsedURL be the result of parsing url with current settings object’s API base
  // URL.
  jsurl::SpecString url_str = core::encode(cx, args.get(0));
  if (!url_str.data) {
    return false;
  }
  auto parsedURL =
      new_jsurl_with_base(&url_str, url::URL::url(worker_location::WorkerLocation::url));
  if (!parsedURL) {
    return api::throw_error(cx, api::Errors::TypeError, "Response.redirect", "url",
                            "be a valid URL");
  }

  // 3. If status is not a redirect status, then throw a RangeError.
  // A redirect status is a status that is 301, 302, 303, 307, or 308.
  auto statusVal = args.get(1);
  uint16_t status;
  if (statusVal.isUndefined()) {
    status = 302;
  } else {
    if (!ToUint16(cx, statusVal, &status)) {
      return false;
    }
  }
  if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308) {
    auto status_str = std::to_string(status);
    return api::throw_error(cx, FetchErrors::InvalidStatus, "Response.redirect",
                            status_str.c_str());
  }

  // 4. Let responseObject be the result of creating a Response object, given a new response,
  // "immutable", and this’s relevant Realm.
  RootedObject responseObject(cx, create(cx));
  if (!responseObject) {
    return false;
  }

  // 5. Set responseObject’s response’s status to status.
  SetReservedSlot(responseObject, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));
  SetReservedSlot(responseObject, static_cast<uint32_t>(Slots::StatusMessage),
                  JS::StringValue(JS_GetEmptyString(cx)));

  // 6. Let value be parsedURL, serialized and isomorphic encoded.
  // 7. Append (`Location`, value) to responseObject’s response’s header list.
  // TODO: redirect response headers should be immutable
  RootedObject headers(cx, RequestOrResponse::headers(cx, responseObject));
  if (!headers) {
    return false;
  }
  if (!Headers::set_valid_if_undefined(cx, headers, "location", url_str)) {
    return false;
  }

  // 8. Return responseObject.
  args.rval().setObjectOrNull(responseObject);
  return true;
}

// namespace {
// bool callbackCalled;
// bool write_json_to_buf(const char16_t *str, uint32_t strlen, void *out) {
//   callbackCalled = true;
//   auto outstr = static_cast<std::u16string *>(out);
//   outstr->append(str, strlen);

//   return true;
// }
// } // namespace

// bool Response::json(JSContext *cx, unsigned argc, JS::Value *vp) {
//   JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
//   if (!args.requireAtLeast(cx, "json", 1)) {
//     return false;
//   }
//   JS::RootedValue data(cx, args.get(0));
//   JS::RootedValue init_val(cx, args.get(1));
//   JS::RootedObject replacer(cx);
//   JS::RootedValue space(cx);

//   std::u16string out;
//   // 1. Let bytes the result of running serialize a JavaScript value to JSON bytes on data.
//   callbackCalled = false;
//   if (!JS::ToJSON(cx, data, replacer, space, &write_json_to_buf, &out)) {
//     return false;
//   }
//   if (!callbackCalled) {
//     return api::throw_error(cx, api::Errors::WrongType, "Response.json", "data", "be a valid JSON
//     value");
//   }
//   // 2. Let body be the result of extracting bytes.

//   // 3. Let responseObject be the result of creating a Response object, given a new response,
//   // "response", and this’s relevant Realm.
//   JS::RootedValue status_val(cx);
//   uint16_t status = 200;

//   JS::RootedValue statusText_val(cx);
//   JS::RootedString statusText(cx, JS_GetEmptyString(cx));
//   JS::RootedValue headers_val(cx);

//   if (init_val.isObject()) {
//     JS::RootedObject init(cx, init_val.toObjectOrNull());
//     if (!JS_GetProperty(cx, init, "status", &status_val) ||
//         !JS_GetProperty(cx, init, "statusText", &statusText_val) ||
//         !JS_GetProperty(cx, init, "headers", &headers_val)) {
//       return false;
//     }

//     if (!status_val.isUndefined() && !JS::ToUint16(cx, status_val, &status)) {
//       return false;
//     }

//     if (status == 204 || status == 205 || status == 304) {
//       auto status_str = std::to_string(status);
//       return api::throw_error(cx, FetchErrors::NonBodyResponseWithBody, "Response.json",
//         status_str.c_str());
//     }

//     if (!statusText_val.isUndefined() && !(statusText = JS::ToString(cx, statusText_val))) {
//       return false;
//     }

//   } else if (!init_val.isNullOrUndefined()) {
//     return api::throw_error(cx, FetchErrors::InvalidInitArg, "Response.json");
//   }

//   auto response_handle_res = host_api::HttpResp::make();
//   if (auto *err = response_handle_res.to_err()) {
//     HANDLE_ERROR(cx, *err);
//     return false;
//   }

//   auto response_handle = response_handle_res.unwrap();
//   if (!response_handle.is_valid()) {
//     return false;
//   }

//   auto make_res = host_api::HttpBody::make(response_handle);
//   if (auto *err = make_res.to_err()) {
//     HANDLE_ERROR(cx, *err);
//     return false;
//   }

//   auto body = make_res.unwrap();
//   JS::RootedString string(cx, JS_NewUCStringCopyN(cx, out.c_str(), out.length()));
//   auto stringChars = core::encode(cx, string);

//   auto write_res =
//       body.write_all(reinterpret_cast<uint8_t *>(stringChars.begin()), stringChars.len);
//   if (auto *err = write_res.to_err()) {
//     HANDLE_ERROR(cx, *err);
//     return false;
//   }
//   JS::RootedObject response_instance(cx, JS_NewObjectWithGivenProto(cx, &Response::class_,
//                                                                     Response::proto_obj));
//   if (!response_instance) {
//     return false;
//   }
//   JS::RootedObject response(cx, create(cx, response_instance, response_handle,
//                                        body, false));
//   if (!response) {
//     return false;
//   }

//   // Set `this`’s `response`’s `status` to `init`["status"].
//   auto set_res = response_handle.set_status(status);
//   if (auto *err = set_res.to_err()) {
//     HANDLE_ERROR(cx, *err);
//     return false;
//   }
//   // To ensure that we really have the same status value as the host,
//   // we always read it back here.
//   auto get_res = response_handle.get_status();
//   if (auto *err = get_res.to_err()) {
//     HANDLE_ERROR(cx, *err);
//     return false;
//   }
//   status = get_res.unwrap();

//   JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));

//   // Set `this`’s `response`’s `status message` to `init`["statusText"].
//   JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::StatusMessage),
//                       JS::StringValue(statusText));

//   // If `init`["headers"] `exists`, then `fill` `this`’s `headers` with
//   // `init`["headers"].
//   JS::RootedObject headers(cx);
//   JS::RootedObject headersInstance(
//       cx, JS_NewObjectWithGivenProto(cx, &Headers::class_, Headers::proto_obj));
//   if (!headersInstance)
//     return false;

//   headers = Headers::create(cx, headersInstance, Headers::Mode::ProxyToResponse,
//                                       response, headers_val);
//   if (!headers) {
//     return false;
//   }
//   // 4. Perform initialize a response given responseObject, init, and (body, "application/json").
//   if (!Headers::maybe_add(cx, headers, "content-type", "application/json")) {
//     return false;
//   }
//   JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers),
//   JS::ObjectValue(*headers)); JS::SetReservedSlot(response,
//   static_cast<uint32_t>(Slots::Redirected), JS::FalseValue()); JS::SetReservedSlot(response,
//   static_cast<uint32_t>(Slots::HasBody), JS::TrueValue()); RequestOrResponse::set_url(response,
//   JS_GetEmptyStringValue(cx));

//   // 5. Return responseObject.
//   args.rval().setObjectOrNull(response);
//   return true;
// }

const JSFunctionSpec Response::static_methods[] = {
    JS_FN("redirect", redirect, 1, JSPROP_ENUMERATE),
    // JS_FN("json", json, 1, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec Response::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Response::methods[] = {
    JS_FN("arrayBuffer", bodyAll<RequestOrResponse::BodyReadResult::ArrayBuffer>, 0,
          JSPROP_ENUMERATE),
    JS_FN("json", bodyAll<RequestOrResponse::BodyReadResult::JSON>, 0, JSPROP_ENUMERATE),
    JS_FN("text", bodyAll<RequestOrResponse::BodyReadResult::Text>, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec Response::properties[] = {
    JS_PSG("redirected", redirected_get, JSPROP_ENUMERATE),
    JS_PSG("type", type_get, JSPROP_ENUMERATE),
    JS_PSG("url", url_get, JSPROP_ENUMERATE),
    JS_PSG("status", status_get, JSPROP_ENUMERATE),
    JS_PSG("ok", ok_get, JSPROP_ENUMERATE),
    JS_PSG("statusText", statusText_get, JSPROP_ENUMERATE),
    JS_PSG("headers", headers_get, JSPROP_ENUMERATE),
    JS_PSG("body", body_get, JSPROP_ENUMERATE),
    JS_PSG("bodyUsed", bodyUsed_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Response", JSPROP_READONLY),
    JS_PS_END,
};

/**
 * The `Response` constructor https://fetch.spec.whatwg.org/#dom-response
 */
bool Response::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("Response", 0);

  JS::RootedValue body_val(cx, args.get(0));
  JS::RootedValue init_val(cx, args.get(1));

  JS::RootedValue status_val(cx);
  uint16_t status = 200;

  JS::RootedValue statusText_val(cx);
  JS::RootedString statusText(cx, JS_GetEmptyString(cx));
  JS::RootedValue headers_val(cx);

  if (init_val.isObject()) {
    JS::RootedObject init(cx, init_val.toObjectOrNull());
    if (!JS_GetProperty(cx, init, "status", &status_val) ||
        !JS_GetProperty(cx, init, "statusText", &statusText_val) ||
        !JS_GetProperty(cx, init, "headers", &headers_val)) {
      return false;
    }

    if (!status_val.isUndefined() && !JS::ToUint16(cx, status_val, &status)) {
      return false;
    }

    if (!statusText_val.isUndefined() && !(statusText = JS::ToString(cx, statusText_val))) {
      return false;
    }

  } else if (!init_val.isNullOrUndefined()) {
    return api::throw_error(cx, FetchErrors::InvalidInitArg, "Response constructor");
  }

  // 1.  If `init`["status"] is not in the range 200 to 599, inclusive, then
  // `throw` a ``RangeError``.
  if (status < 200 || status > 599) {
    auto status_str = std::to_string(status);
    return api::throw_error(cx, FetchErrors::InvalidStatus, "Response constructor",
                            status_str.c_str());
  }

  // 2.  If `init`["statusText"] does not match the `reason-phrase` token
  // production, then `throw` a ``TypeError``. Skipped: the statusText can only
  // be consumed by the content creating it, so we're lenient about its format.

  // 3.  Set `this`’s `response` to a new `response`.
  // 5. (Reordered) Set `this`’s `response`’s `status` to `init`["status"].

  // 7.  (Reordered) If `init`["headers"] `exists`, then `fill` `this`’s `headers` with
  // `init`["headers"].
  JS::RootedObject headers(cx, Headers::create(cx, headers_val, Headers::HeadersGuard::Response));
  if (!headers) {
    return false;
  }

  JS::RootedObject response(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!response) {
    return false;
  }
  init_slots(response);

  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));

  // TODO: move this into the create function, given that it must not be called again later.
  RequestOrResponse::set_url(response, JS_GetEmptyStringValue(cx));

  // 4.  Set `this`’s `headers` to a `new` ``Headers`` object with `this`’s
  // `relevant Realm`,
  //     whose `header list` is `this`’s `response`’s `header list` and `guard`
  //     is "`response`".
  // (implicit)

  // To ensure that we really have the same status value as the host,
  // we always read it back here.
  // TODO: either convince ourselves that it's ok not to do this, or add a way to wasi-http to do
  // it. auto get_res = response_handle.get_status(); if (auto *err = get_res.to_err()) {
  //   HANDLE_ERROR(cx, *err);
  //   return false;
  // }
  // status = get_res.unwrap();

  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));

  // 6.  Set `this`’s `response`’s `status message` to `init`["statusText"].
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::StatusMessage),
                      JS::StringValue(statusText));

  // 8.  If `body` is non-null, then:
  if ((!body_val.isNullOrUndefined())) {
    //     1.  If `init`["status"] is a `null body status`, then `throw` a
    //     ``TypeError``.
    if (status == 204 || status == 205 || status == 304) {
      auto status_str = std::to_string(status);
      return api::throw_error(cx, FetchErrors::NonBodyResponseWithBody, "Response constructor",
                              status_str.c_str());
    }

    //     2.  Let `Content-Type` be null.
    //     3.  Set `this`’s `response`’s `body` and `Content-Type` to the result
    //     of `extracting`
    //         `body`.
    //     4.  If `Content-Type` is non-null and `this`’s `response`’s `header
    //     list` `does not
    //         contain` ``Content-Type``, then `append` (``Content-Type``,
    //         `Content-Type`) to `this`’s `response`’s `header list`.
    // Note: these steps are all inlined into RequestOrResponse::extract_body.
    if (!RequestOrResponse::extract_body(cx, response, body_val)) {
      return false;
    }
  }

  args.rval().setObject(*response);
  return true;
}

bool Response::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
    return false;
  }

  // Initialize a pinned (i.e., never-moved, living forever) atom for the
  // response type values.
  return (type_default_atom = JS_AtomizeAndPinString(cx, "default")) &&
         (type_error_atom = JS_AtomizeAndPinString(cx, "error"));
}

JSObject *Response::create(JSContext *cx) {
  RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self) {
    return nullptr;
  }
  init_slots(self);
  return self;
}

JSObject *Response::init_slots(HandleObject response) {
  MOZ_ASSERT(is_instance(response));

  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Response), JS::PrivateValue(nullptr));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers), JS::NullValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::BodyStream), JS::NullValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::HasBody), JS::FalseValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::BodyUsed), JS::FalseValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Redirected), JS::FalseValue());

  return response;
}

JSObject *Response::create_incoming(JSContext *cx, host_api::HttpIncomingResponse *response) {
  RootedObject self(cx, create(cx));
  if (!self) {
    return nullptr;
  }

  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::Response), PrivateValue(response));

  auto res = response->status();
  MOZ_ASSERT(!res.is_err(), "TODO: proper error handling");
  auto status = res.unwrap();
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));
  set_status_message_from_code(cx, self, status);

  if (!(status == 204 || status == 205 || status == 304)) {
    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::HasBody), JS::TrueValue());
  }

  return self;
}

namespace request_response {

bool install(api::Engine *engine) {
  ENGINE = engine;

  if (!Request::init_class(engine->cx(), engine->global()))
    return false;
  if (!Response::init_class(engine->cx(), engine->global()))
    return false;
  return true;
}

} // namespace request_response
} // namespace builtins::web::fetch
