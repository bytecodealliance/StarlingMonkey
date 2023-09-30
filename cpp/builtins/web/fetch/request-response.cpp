#include "builtins/request-response.h"

#include "builtins/cache-override.h"
#include "builtins/client-info.h"
#include "builtins/fastly.h"
#include "builtins/fetch-event.h"
#include "builtins/kv-store.h"
#include "builtins/native-stream-source.h"
#include "builtins/shared/url.h"
#include "builtins/transform-stream.h"
#include "core/encode.h"
#include "core/event_loop.h"
#include "host_interface/host_api.h"
#include "third_party/picosha2.h"

#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/JSON.h"
#include "js/Stream.h"
#include <algorithm>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/experimental/TypedData.h"
#pragma clang diagnostic pop

namespace builtins {

namespace {
constexpr size_t HANDLE_READ_CHUNK_SIZE = 8192;

// https://fetch.spec.whatwg.org/#concept-method-normalize
// Returns `true` if the method name was normalized, `false` otherwise.
bool normalize_http_method(char *method) {
  static const char *names[6] = {"DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT"};

  for (size_t i = 0; i < 6; i++) {
    auto name = names[i];
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
  JS::UniqueChars buffer;
  size_t length;
};

// Returns a UniqueChars and the length of that string. The UniqueChars value is not
// null-terminated.
ReadResult read_from_handle_all(JSContext *cx, host_api::HttpBody body) {
  std::vector<host_api::HostString> chunks;
  size_t bytes_read = 0;
  while (true) {
    auto res = body.read(HANDLE_READ_CHUNK_SIZE);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return {nullptr, 0};
    }

    auto &chunk = res.unwrap();
    if (chunk.len == 0) {
      break;
    }

    bytes_read += chunk.len;
    chunks.emplace_back(std::move(chunk));
  }

  JS::UniqueChars buf;
  if (chunks.size() == 1) {
    // If there was only one chunk read, reuse that allocation.
    auto &chunk = chunks.back();
    buf = std::move(chunk.ptr);
  } else {
    // If there wasn't exactly one chunk read, we'll need to allocate a buffer to store the results.
    buf.reset(static_cast<char *>(JS_string_malloc(cx, bytes_read)));
    if (!buf) {
      JS_ReportOutOfMemory(cx);
      return {nullptr, 0};
    }

    char *end = buf.get();
    for (auto &chunk : chunks) {
      end = std::copy(chunk.ptr.get(), chunk.ptr.get() + chunk.len, end);
    }
  }

  return {std::move(buf), bytes_read};
}

template <InternalMethod fun>
bool enqueue_internal_method(JSContext *cx, JS::HandleObject receiver,
                             JS::HandleValue extra = JS::UndefinedHandleValue,
                             unsigned int nargs = 0, const char *name = "") {
  JS::RootedObject method(cx, create_internal_method<fun>(cx, receiver, extra, nargs, name));
  if (!method) {
    return false;
  }

  JS::RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  if (!promise) {
    return false;
  }

  return JS::AddPromiseReactions(cx, promise, method, nullptr);
}

} // namespace

bool RequestOrResponse::is_instance(JSObject *obj) {
  return Request::is_instance(obj) || Response::is_instance(obj) || KVStoreEntry::is_instance(obj);
}

uint32_t RequestOrResponse::handle(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return static_cast<uint32_t>(
      JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::RequestOrResponse)).toInt32());
}

bool RequestOrResponse::has_body(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::HasBody)).toBoolean();
}

host_api::HttpBody RequestOrResponse::body_handle(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return host_api::HttpBody(JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Body)).toInt32());
}

JSObject *RequestOrResponse::body_stream(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::BodyStream)).toObjectOrNull();
}

JSObject *RequestOrResponse::body_source(JSContext *cx, JS::HandleObject obj) {
  MOZ_ASSERT(has_body(obj));
  JS::RootedObject stream(cx, body_stream(obj));
  return builtins::NativeStreamSource::get_stream_source(cx, stream);
}

bool RequestOrResponse::body_used(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::BodyUsed)).toBoolean();
}

bool RequestOrResponse::mark_body_used(JSContext *cx, JS::HandleObject obj) {
  MOZ_ASSERT(!body_used(obj));
  JS::SetReservedSlot(obj, static_cast<uint32_t>(Slots::BodyUsed), JS::BooleanValue(true));

  JS::RootedObject stream(cx, body_stream(obj));
  if (stream && builtins::NativeStreamSource::stream_is_body(cx, stream)) {
    if (!builtins::NativeStreamSource::lock_stream(cx, stream)) {
      // The only reason why marking the body as used could fail here is that
      // it's a disturbed ReadableStream. To improve error reporting, we clear
      // the current exception and throw a better one.
      JS_ClearPendingException(cx);
      JS_ReportErrorLatin1(cx, "The ReadableStream body is already locked and can't be consumed");
      return false;
    }
  }

  return true;
}

/**
 * Moves an underlying body handle from one Request/Response object to another.
 *
 * Also marks the source object's body as consumed.
 */
bool RequestOrResponse::move_body_handle(JSContext *cx, JS::HandleObject from,
                                         JS::HandleObject to) {
  MOZ_ASSERT(is_instance(from));
  MOZ_ASSERT(is_instance(to));
  MOZ_ASSERT(!body_used(from));

  // Replace the receiving object's body handle with the body stream source's
  // underlying handle.
  // TODO: Let the host know we'll not use the old handle anymore, once C@E has
  // a hostcall for that.
  auto body = body_handle(from);
  JS::SetReservedSlot(to, static_cast<uint32_t>(Slots::Body), JS::Int32Value(body.handle));

  // Mark the source's body as used, and the stream as locked to prevent any
  // future attempts to use the underlying handle we just removed.
  return mark_body_used(cx, from);
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
 * Note: our implementation is somewhat different from what the spec describes
 * in that we immediately write all non-streaming body types to the host instead
 * of creating a stream for them. We don't have threads, so there's nothing "in
 * parallel" to be had anyway.
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
      JS_ReportErrorLatin1(cx, "Can't use a ReadableStream that's locked or has ever been "
                               "read from or canceled as a Request or Response body.");
      return false;
    }

    JS_SetReservedSlot(self, static_cast<uint32_t>(RequestOrResponse::Slots::BodyStream), body_val);

    // Ensure that we take the right steps for shortcutting operations on
    // TransformStreams later on.
    if (builtins::TransformStream::is_ts_readable(cx, body_obj)) {
      // But only if the TransformStream isn't used as a mixin by other
      // builtins.
      if (!builtins::TransformStream::used_as_mixin(
              builtins::TransformStream::ts_from_readable(cx, body_obj))) {
        builtins::TransformStream::set_readable_used_as_body(cx, body_obj, self);
      }
    }
  } else {
    mozilla::Maybe<JS::AutoCheckCannotGC> maybeNoGC;
    JS::UniqueChars text;
    char *buf;
    size_t length;

    if (body_obj && JS_IsArrayBufferViewObject(body_obj)) {
      // Short typed arrays have inline data which can move on GC, so assert
      // that no GC happens. (Which it doesn't, because we're not allocating
      // before `buf` goes out of scope.)
      maybeNoGC.emplace(cx);
      JS::AutoCheckCannotGC &noGC = maybeNoGC.ref();
      bool is_shared;
      length = JS_GetArrayBufferViewByteLength(body_obj);
      buf = (char *)JS_GetArrayBufferViewData(body_obj, &is_shared, noGC);
    } else if (body_obj && JS::IsArrayBufferObject(body_obj)) {
      bool is_shared;
      JS::GetArrayBufferLengthAndData(body_obj, &length, &is_shared, (uint8_t **)&buf);
    } else if (body_obj && builtins::URLSearchParams::is_instance(body_obj)) {
      auto slice = builtins::URLSearchParams::serialize(cx, body_obj);
      buf = (char *)slice.data;
      length = slice.len;
      content_type = "application/x-www-form-urlencoded;charset=UTF-8";
    } else {
      {
        auto str = core::encode(cx, body_val);
        text = std::move(str.ptr);
        length = str.len;
      }

      if (!text)
        return false;
      buf = text.get();
      content_type = "text/plain;charset=UTF-8";
    }

    host_api::HttpBody body{RequestOrResponse::body_handle(self)};
    auto write_res = body.write_all(reinterpret_cast<uint8_t *>(buf), length);

    // Ensure that the NoGC is reset, so throwing an error in HANDLE_ERROR
    // succeeds.
    if (maybeNoGC.isSome()) {
      maybeNoGC.reset();
    }

    if (auto *err = write_res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  }

  // Step 36.3 of Request constructor / 8.4 of Response constructor.
  if (content_type) {
    JS::RootedObject headers(
        cx, &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Headers)).toObject());
    if (!builtins::Headers::maybe_add(cx, headers, "content-type", content_type)) {
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

bool RequestOrResponse::append_body(JSContext *cx, JS::HandleObject self, JS::HandleObject source) {
  MOZ_ASSERT(!body_used(source));
  host_api::HttpBody source_body{body_handle(source)};
  host_api::HttpBody dest_body{body_handle(self)};
  auto res = dest_body.append(source_body);
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  return true;
}

template <Headers::Mode mode>
JSObject *RequestOrResponse::headers(JSContext *cx, JS::HandleObject obj) {
  JSObject *headers = maybe_headers(obj);
  if (!headers) {
    JS::RootedObject headersInstance(cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_,
                                                                    builtins::Headers::proto_obj));

    if (!headersInstance) {
      return nullptr;
    }

    headers = builtins::Headers::create(cx, headersInstance, mode, obj);
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
    JS::RootedObject array_buffer(cx, JS::NewArrayBufferWithContents(cx, len, buf.get()));
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
    // TODO(performance): investigate whether we can infer the size directly from `contents`
    size_t buf_size = HANDLE_READ_CHUNK_SIZE;
    // TODO(performance): make use of malloc slack.
    // https://github.com/fastly/js-compute-runtime/issues/217
    size_t offset = 0;
    // In this loop we are finding the length of each entry in `contents` and resizing the `buf`
    // until it is large enough to fit all the entries in `contents`
    for (uint32_t index = 0; index < contentsLength; index++) {
      JS::RootedValue val(cx);
      if (!JS_GetElement(cx, contents, index, &val)) {
        return false;
      }
      {
        JS::AutoCheckCannotGC nogc;
        MOZ_ASSERT(val.isObject());
        JSObject *array = &val.toObject();
        MOZ_ASSERT(JS_IsUint8Array(array));
        size_t length = JS_GetTypedArrayByteLength(array);
        if (length) {
          offset += length;
          // if buf is not big enough to fit the next uint8array's bytes then resize
          if (offset > buf_size) {
            buf_size =
                buf_size + (HANDLE_READ_CHUNK_SIZE * ((length / HANDLE_READ_CHUNK_SIZE) + 1));
          }
        }
      }
    }

    JS::UniqueChars buf{static_cast<char *>(JS_malloc(cx, buf_size + 1))};
    if (!buf) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
    // reset the offset for the next loop
    offset = 0;
    // In this loop we are inserting each entry in `contents` into `buf`
    for (uint32_t index = 0; index < contentsLength; index++) {
      JS::RootedValue val(cx);
      if (!JS_GetElement(cx, contents, index, &val)) {
        return false;
      }
      {
        JS::AutoCheckCannotGC nogc;
        MOZ_ASSERT(val.isObject());
        JSObject *array = &val.toObject();
        MOZ_ASSERT(JS_IsUint8Array(array));
        bool is_shared;
        size_t length = JS_GetTypedArrayByteLength(array);
        if (length) {
          static_assert(CHAR_BIT == 8, "Strange char");
          auto bytes = reinterpret_cast<char *>(JS_GetUint8ArrayData(array, &is_shared, nogc));
          memcpy(buf.get() + offset, bytes, length);
          offset += length;
        }
      }
    }
    buf[offset] = '\0';
#ifdef DEBUG
    bool foundBodyParser;
    if (!JS_HasElement(cx, catch_handler, 2, &foundBodyParser)) {
      return false;
    }
    MOZ_ASSERT(foundBodyParser);
#endif
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
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_RESPONSE_VALUE_NOT_UINT8ARRAY);
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
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_RESPONSE_BODY_DISTURBED_OR_LOCKED);
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

bool RequestOrResponse::consume_body_handle_for_bodyAll(JSContext *cx, JS::HandleObject self,
                                                        JS::HandleValue body_parser,
                                                        JS::CallArgs args) {
  auto body = body_handle(self);
  auto *parse_body = reinterpret_cast<ParseBodyCB *>(body_parser.toPrivate());
  auto [buf, bytes_read] = read_from_handle_all(cx, body);
  if (!buf) {
    JS::RootedObject result_promise(cx);
    result_promise =
        &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise)).toObject();
    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyAllPromise), JS::UndefinedValue());
    return RejectPromiseWithPendingError(cx, result_promise);
  }

  return parse_body(cx, self, std::move(buf), bytes_read);
}

template <RequestOrResponse::BodyReadResult result_type>
bool RequestOrResponse::bodyAll(JSContext *cx, JS::CallArgs args, JS::HandleObject self) {
  // TODO: mark body as consumed when operating on stream, too.
  if (body_used(self)) {
    JS_ReportErrorASCII(cx, "Body has already been consumed");
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

  if (!mark_body_used(cx, self)) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  JS::RootedValue body_parser(cx, JS::PrivateValue((void *)parse_body<result_type>));

  // If the body is a ReadableStream that's not backed by a body handle, we need to
  // manually read all chunks from the stream.
  // TODO(performance): ensure that we're properly shortcutting reads from TransformStream
  // readables.
  // https://github.com/fastly/js-compute-runtime/issues/218
  JS::RootedObject stream(cx, body_stream(self));
  if (stream && !builtins::NativeStreamSource::stream_is_body(cx, stream)) {
    if (!JS_SetElement(cx, stream, 1, body_parser)) {
      return false;
    }
    JS::RootedValue extra(cx, JS::ObjectValue(*stream));
    if (!enqueue_internal_method<consume_content_stream_for_bodyAll>(cx, self, extra)) {
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }
  } else {
    if (!enqueue_internal_method<consume_body_handle_for_bodyAll>(cx, self, body_parser)) {
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }
  }

  args.rval().setObject(*bodyAll_promise);
  return true;
}

bool RequestOrResponse::body_source_pull_algorithm(JSContext *cx, JS::CallArgs args,
                                                   JS::HandleObject source,
                                                   JS::HandleObject body_owner,
                                                   JS::HandleObject controller) {
  // If the stream has been piped to a TransformStream whose readable end was
  // then passed to a Request or Response as the body, we can just append the
  // entire source body to the destination using a single native hostcall, and
  // then close the source stream, instead of reading and writing it in
  // individual chunks. Note that even in situations where multiple streams are
  // piped to the same destination this is guaranteed to happen in the right
  // order: ReadableStream#pipeTo locks the destination WritableStream until the
  // source ReadableStream is closed/canceled, so only one stream can ever be
  // piped in at the same time.
  JS::RootedObject pipe_dest(cx, builtins::NativeStreamSource::piped_to_transform_stream(source));
  if (pipe_dest) {
    if (builtins::TransformStream::readable_used_as_body(pipe_dest)) {
      JS::RootedObject dest_owner(cx, builtins::TransformStream::owner(pipe_dest));
      if (!RequestOrResponse::append_body(cx, dest_owner, body_owner)) {
        return false;
      }

      JS::RootedObject stream(cx, builtins::NativeStreamSource::stream(source));
      bool success = JS::ReadableStreamClose(cx, stream);
      MOZ_RELEASE_ASSERT(success);

      args.rval().setUndefined();
      return true;
    }
  }

  // The actual read from the body needs to be delayed, because it'd otherwise
  // be a blocking operation in case the backend didn't yet send any data.
  // That would lead to situations where we block on I/O before processing
  // all pending Promises, which in turn can result in operations happening in
  // observably different behavior, up to and including causing deadlocks
  // because a body read response is blocked on content making another request.
  //
  // (This deadlock happens in automated tests, but admittedly might not happen
  // in real usage.)

  if (!core::EventLoop::queue_async_task(source))
    return false;

  args.rval().setUndefined();
  return true;
}

bool RequestOrResponse::body_source_cancel_algorithm(JSContext *cx, JS::CallArgs args,
                                                     JS::HandleObject stream,
                                                     JS::HandleObject owner,
                                                     JS::HandleValue reason) {
  args.rval().setUndefined();
  return true;
}

bool RequestOrResponse::body_reader_then_handler(JSContext *cx, JS::HandleObject body_owner,
                                                 JS::HandleValue extra, JS::CallArgs args) {
  JS::RootedObject then_handler(cx, &args.callee());
  // The reader is stored in the catch handler, which we need here as well.
  // So we get that first, then the reader.
  JS::RootedObject catch_handler(cx, &extra.toObject());
  JS::RootedObject reader(cx, &js::GetFunctionNativeReserved(catch_handler, 1).toObject());
  auto body = RequestOrResponse::body_handle(body_owner);

  // We're guaranteed to work with a native ReadableStreamDefaultReader here,
  // which in turn is guaranteed to vend {done: bool, value: any} objects to
  // read promise then callbacks.
  JS::RootedObject chunk_obj(cx, &args[0].toObject());
  JS::RootedValue done_val(cx);
  if (!JS_GetProperty(cx, chunk_obj, "done", &done_val))
    return false;

  if (done_val.toBoolean()) {
    // The only response we ever send is the one passed to
    // `FetchEvent#respondWith` to send to the client. As such, we can be
    // certain that if we have a response here, we can advance the FetchState to
    // `responseDone`.
    if (Response::is_instance(body_owner)) {
      FetchEvent::set_state(FetchEvent::instance(), FetchEvent::State::responseDone);
    }

    auto res = body.close();
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }

    if (Request::is_instance(body_owner)) {
      if (!core::EventLoop::queue_async_task(body_owner)) {
        return false;
      }
    }

    return true;
  }

  JS::RootedValue val(cx);
  if (!JS_GetProperty(cx, chunk_obj, "value", &val))
    return false;

  // The read operation returned something that's not a Uint8Array
  if (!val.isObject() || !JS_IsUint8Array(&val.toObject())) {
    // reject the request promise
    if (Request::is_instance(body_owner)) {
      JS::RootedObject response_promise(cx, Request::response_promise(body_owner));
      JS::RootedValue exn(cx);

      // TODO: this should be a TypeError, but I'm not sure how to make that work
      JS_ReportErrorUTF8(cx, "TypeError");
      if (!JS_GetPendingException(cx, &exn)) {
        return false;
      }
      JS_ClearPendingException(cx);

      return JS::RejectPromise(cx, response_promise, exn);
    }

    // TODO: should we also create a rejected promise if a response reads something that's not a
    // Uint8Array?
    fprintf(stderr, "Error: read operation on body ReadableStream didn't respond with a "
                    "Uint8Array. Received value: ");
    dump_value(cx, val, stderr);
    return false;
  }

  host_api::Result<host_api::Void> res;
  {
    JS::AutoCheckCannotGC nogc;
    JSObject *array = &val.toObject();
    bool is_shared;
    uint8_t *bytes = JS_GetUint8ArrayData(array, &is_shared, nogc);
    size_t length = JS_GetTypedArrayByteLength(array);
    res = body.write_all(bytes, length);
  }

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

bool RequestOrResponse::body_reader_catch_handler(JSContext *cx, JS::HandleObject body_owner,
                                                  JS::HandleValue extra, JS::CallArgs args) {
  // TODO: check if this should create a rejected promise instead, so an
  // in-content handler for unhandled rejections could deal with it. The body
  // stream errored during the streaming send. Not much we can do, but at least
  // close the stream, and warn.
  fprintf(stderr, "Warning: body ReadableStream closed during body streaming. Exception: ");
  dump_value(cx, args.get(0), stderr);

  // The only response we ever send is the one passed to
  // `FetchEvent#respondWith` to send to the client. As such, we can be certain
  // that if we have a response here, we can advance the FetchState to
  // `responseDone`. (Note that even though we encountered an error,
  // `responseDone` is the right state: `responsedWithError` is for when sending
  // a response at all failed.)
  if (Response::is_instance(body_owner)) {
    FetchEvent::set_state(FetchEvent::instance(), FetchEvent::State::responseDone);
  }
  return true;
}

bool RequestOrResponse::maybe_stream_body(JSContext *cx, JS::HandleObject body_owner,
                                          bool *requires_streaming) {
  JS::RootedObject stream(cx, RequestOrResponse::body_stream(body_owner));
  if (!stream) {
    return true;
  }

  if (RequestOrResponse::body_unusable(cx, stream)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_RESPONSE_BODY_DISTURBED_OR_LOCKED);
    return false;
  }

  // If the body stream is backed by a C@E body handle, we can directly pipe
  // that handle into the body we're about to send.
  if (builtins::NativeStreamSource::stream_is_body(cx, stream)) {
    // First, move the source's body handle to the target and lock the stream.
    JS::RootedObject stream_source(cx, builtins::NativeStreamSource::get_stream_source(cx, stream));
    JS::RootedObject source_owner(cx, builtins::NativeStreamSource::owner(stream_source));
    if (!RequestOrResponse::move_body_handle(cx, source_owner, body_owner)) {
      return false;
    }

    // Then, send the request/response without streaming. We know that content
    // won't append to this body handle, because we don't expose any means to do
    // so, so it's ok for it to be closed immediately.
    return true;
  }

  JS::RootedObject reader(
      cx, JS::ReadableStreamGetReader(cx, stream, JS::ReadableStreamReaderMode::Default));
  if (!reader)
    return false;

  bool is_closed;
  if (!JS::ReadableStreamReaderIsClosed(cx, reader, &is_closed))
    return false;

  // It's ok for the stream to be closed, as its contents might
  // already have fully been written to the body handle.
  // In that case, we can do a blocking send instead.
  if (is_closed) {
    return true;
  }

  // Create handlers for both `then` and `catch`.
  // These are functions with two reserved slots, in which we store all
  // information required to perform the reactions. We store the actually
  // required information on the catch handler, and a reference to that on the
  // then handler. This allows us to reuse these functions for the next read
  // operation in the then handler. The catch handler won't ever have a need to
  // perform another operation in this way.
  JS::RootedObject catch_handler(cx);
  JS::RootedValue extra(cx, JS::ObjectValue(*reader));
  catch_handler = create_internal_method<body_reader_catch_handler>(cx, body_owner, extra);
  if (!catch_handler)
    return false;

  JS::RootedObject then_handler(cx);
  extra.setObject(*catch_handler);
  then_handler = create_internal_method<body_reader_then_handler>(cx, body_owner, extra);
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
  MOZ_ASSERT(is_instance(owner));
  MOZ_ASSERT(!body_stream(owner));
  JS::RootedObject source(cx, builtins::NativeStreamSource::create(
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

  // TODO: immediately lock the stream if the owner's body is already used.

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

host_api::HttpReq Request::request_handle(JSObject *obj) {
  return host_api::HttpReq(
      JS::GetReservedSlot(obj, static_cast<uint32_t>(Request::Slots::Request)).toInt32());
}

host_api::HttpPendingReq Request::pending_handle(JSObject *obj) {
  host_api::HttpPendingReq res;

  JS::Value handle_val =
      JS::GetReservedSlot(obj, static_cast<uint32_t>(Request::Slots::PendingRequest));
  if (handle_val.isInt32()) {
    res = host_api::HttpPendingReq(handle_val.toInt32());
  }

  return res;
}

bool Request::is_downstream(JSObject *obj) {
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::IsDownstream)).toBoolean();
}

JSString *Request::backend(JSObject *obj) {
  auto val = JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Backend));
  return val.isString() ? val.toString() : nullptr;
}

JSObject *Request::response_promise(JSObject *obj) {
  return &JS::GetReservedSlot(obj, static_cast<uint32_t>(Request::Slots::ResponsePromise))
              .toObject();
}

JSString *Request::method(JSContext *cx, JS::HandleObject obj) {
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Method)).toString();
}

bool Request::set_cache_key(JSContext *cx, JS::HandleObject self, JS::HandleValue cache_key_val) {
  MOZ_ASSERT(is_instance(self));
  // Convert the key argument into a String following https://tc39.es/ecma262/#sec-tostring
  auto keyString = core::encode(cx, cache_key_val);
  if (!keyString) {
    return false;
  }
  std::string hex_str;
  picosha2::hash256_hex_string(keyString, hex_str);
  std::transform(hex_str.begin(), hex_str.end(), hex_str.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  JSObject *headers = RequestOrResponse::headers<builtins::Headers::Mode::ProxyToRequest>(cx, self);
  if (!headers) {
    return false;
  }
  JS::RootedObject headers_val(cx, headers);
  JS::RootedValue name_val(cx, JS::StringValue(JS_NewStringCopyN(cx, "fastly-xqd-cache-key", 20)));
  JS::RootedValue value_val(
      cx, JS::StringValue(JS_NewStringCopyN(cx, hex_str.c_str(), hex_str.length())));
  if (!builtins::Headers::append_header_value(cx, headers_val, name_val, value_val,
                                              "Request.prototype.setCacheKey")) {
    return false;
  }

  return true;
}

bool Request::set_cache_override(JSContext *cx, JS::HandleObject self,
                                 JS::HandleValue cache_override_val) {
  MOZ_ASSERT(is_instance(self));
  if (!builtins::CacheOverride::is_instance(cache_override_val)) {
    JS_ReportErrorUTF8(cx, "Value passed in as cacheOverride must be an "
                           "instance of CacheOverride");
    return false;
  }

  JS::RootedObject input(cx, &cache_override_val.toObject());
  JSObject *override = builtins::CacheOverride::clone(cx, input);
  if (!override) {
    return false;
  }

  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::CacheOverride),
                      JS::ObjectValue(*override));
  return true;
}

bool Request::apply_auto_decompress_gzip(JSContext *cx, JS::HandleObject self) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(is_instance(self));
  auto decompress =
      JS::GetReservedSlot(self, static_cast<uint32_t>(Request::Slots::AutoDecompressGzip))
          .toBoolean();
  if (!decompress) {
    return true;
  }

  auto res = Request::request_handle(self).auto_decompress_gzip();
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  return true;
}

/**
 * Apply the CacheOverride to a host-side request handle.
 */
bool Request::apply_cache_override(JSContext *cx, JS::HandleObject self) {
  MOZ_ASSERT(is_instance(self));
  JS::RootedObject override(
      cx, JS::GetReservedSlot(self, static_cast<uint32_t>(Request::Slots::CacheOverride))
              .toObjectOrNull());
  if (!override) {
    return true;
  }

  std::optional<uint32_t> ttl;
  JS::RootedValue val(cx, builtins::CacheOverride::ttl(override));
  if (!val.isUndefined()) {
    ttl = val.toInt32();
  }

  std::optional<uint32_t> stale_while_revalidate;
  val = builtins::CacheOverride::swr(override);
  if (!val.isUndefined()) {
    stale_while_revalidate = val.toInt32();
  }

  host_api::HostString sk_chars;
  std::optional<std::string_view> surrogate_key;
  val = builtins::CacheOverride::surrogate_key(override);
  if (!val.isUndefined()) {
    sk_chars = core::encode(cx, val);
    if (!sk_chars) {
      return false;
    }

    surrogate_key.emplace(sk_chars);
  }

  auto tag = builtins::CacheOverride::abi_tag(override);
  auto res =
      Request::request_handle(self).cache_override(tag, ttl, stale_while_revalidate, surrogate_key);
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  return true;
}

bool Request::method_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JSString *method = Request::method(cx, self);
  if (!method)
    return false;

  args.rval().setString(method);
  return true;
}

bool Request::url_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().set(RequestOrResponse::url(self));
  return true;
}

bool Request::version_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  auto res = request_handle(self).get_version();
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  args.rval().setInt32(res.unwrap());
  return true;
}

bool Request::headers_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JSObject *headers = RequestOrResponse::headers<builtins::Headers::Mode::ProxyToRequest>(cx, self);
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
  return RequestOrResponse::body_get(cx, args, self, is_downstream(self));
}

bool Request::bodyUsed_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  args.rval().setBoolean(RequestOrResponse::body_used(self));
  return true;
}

bool Request::setCacheOverride(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  if (!set_cache_override(cx, self, args[0]))
    return false;

  args.rval().setUndefined();
  return true;
}

bool Request::setCacheKey(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  if (!set_cache_key(cx, self, args[0])) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

JSString *GET_atom;

bool Request::clone(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  auto request_handle_res = host_api::HttpReq::make();
  if (auto *err = request_handle_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto request_handle = request_handle_res.unwrap();

  JS::RootedObject requestInstance(cx, Request::create_instance(cx));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Request),
                      JS::Int32Value(request_handle.handle));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::BodyUsed), JS::FalseValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::URL),
                      JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::URL)));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::IsDownstream),
                      JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::IsDownstream)));

  auto hasBody = RequestOrResponse::has_body(self);

  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::HasBody),
                      JS::BooleanValue(hasBody));

  if (hasBody) {
    if (RequestOrResponse::body_used(self)) {
      JS_ReportErrorLatin1(cx, "Request.prototype.clone: the request's body isn't usable.");
      return false;
    }

    // Here we get the current requests body stream and call ReadableStream.prototype.tee to return
    // two versions of the stream. Once we get the two streams, we create a new request handle and
    // attach one of the streams to the new handle and the other stream is attached to the request
    // handle that `clone()` was called upon.
    JS::RootedObject body_stream(cx, RequestOrResponse::body_stream(self));
    if (!body_stream) {
      body_stream = RequestOrResponse::create_body_stream(cx, self);
      if (!body_stream) {
        return false;
      }
    }
    JS::RootedValue tee_val(cx);
    if (!JS_GetProperty(cx, body_stream, "tee", &tee_val)) {
      return false;
    }
    JS::Rooted<JSFunction *> tee(cx, JS_GetObjectFunction(&tee_val.toObject()));
    if (!tee) {
      return false;
    }
    JS::RootedVector<JS::Value> argv(cx);
    JS::RootedValue rval(cx);
    if (!JS::Call(cx, body_stream, tee, argv, &rval)) {
      return false;
    }
    JS::RootedObject rval_array(cx, &rval.toObject());
    JS::RootedValue body1_val(cx);
    if (!JS_GetProperty(cx, rval_array, "0", &body1_val)) {
      return false;
    }
    JS::RootedValue body2_val(cx);
    if (!JS_GetProperty(cx, rval_array, "1", &body2_val)) {
      return false;
    }

    auto res = host_api::HttpBody::make();
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }

    auto body_handle = res.unwrap();
    if (!JS::IsReadableStream(&body1_val.toObject())) {
      return false;
    }
    body_stream.set(&body1_val.toObject());
    if (RequestOrResponse::body_unusable(cx, body_stream)) {
      JS_ReportErrorLatin1(cx, "Can't use a ReadableStream that's locked or has ever been "
                               "read from or canceled as a Request body.");
      return false;
    }

    JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Body),
                        JS::Int32Value(body_handle.handle));
    JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::BodyStream), body1_val);

    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyStream), body2_val);
    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BodyUsed), JS::FalseValue());
    JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::HasBody), JS::BooleanValue(true));
  }

  JS::RootedObject headers(cx);
  JS::RootedObject headers_obj(
      cx, RequestOrResponse::headers<builtins::Headers::Mode::ProxyToRequest>(cx, self));
  if (!headers_obj) {
    return false;
  }
  JS::RootedObject headersInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_, builtins::Headers::proto_obj));
  if (!headersInstance)
    return false;

  headers = builtins::Headers::create(cx, headersInstance, builtins::Headers::Mode::ProxyToRequest,
                                      requestInstance, headers_obj);

  if (!headers) {
    return false;
  }

  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Headers),
                      JS::ObjectValue(*headers));

  JSString *method = Request::method(cx, self);
  if (!method) {
    return false;
  }

  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Method),
                      JS::StringValue(method));
  JS::RootedValue cache_override(
      cx, JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::CacheOverride)));
  if (!cache_override.isNullOrUndefined()) {
    if (!set_cache_override(cx, requestInstance, cache_override)) {
      return false;
    }
  } else {
    JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::CacheOverride),
                        cache_override);
  }

  args.rval().setObject(*requestInstance);
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
    JS_FN("setCacheOverride", Request::setCacheOverride, 3, JSPROP_ENUMERATE),
    JS_FN("setCacheKey", Request::setCacheKey, 0, JSPROP_ENUMERATE),
    JS_FN("clone", Request::clone, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec Request::properties[] = {
    JS_PSG("method", Request::method_get, JSPROP_ENUMERATE),
    JS_PSG("url", Request::url_get, JSPROP_ENUMERATE),
    JS_PSG("version", Request::version_get, JSPROP_ENUMERATE),
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

JSObject *Request::create(JSContext *cx, JS::HandleObject requestInstance,
                          host_api::HttpReq request_handle, host_api::HttpBody body_handle,
                          bool is_downstream) {
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Request),
                      JS::Int32Value(request_handle.handle));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Headers), JS::NullValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Body),
                      JS::Int32Value(body_handle.handle));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::BodyStream), JS::NullValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::HasBody), JS::FalseValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::BodyUsed), JS::FalseValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::Method),
                      JS::StringValue(GET_atom));
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::CacheOverride),
                      JS::NullValue());
  JS::SetReservedSlot(requestInstance, static_cast<uint32_t>(Slots::IsDownstream),
                      JS::BooleanValue(is_downstream));

  return requestInstance;
}

/**
 * Create a new Request object, roughly according to
 * https://fetch.spec.whatwg.org/#dom-request
 *
 * "Roughly" because not all aspects of Request handling make sense in C@E.
 * The places where we deviate from the spec are called out inline.
 */
JSObject *Request::create(JSContext *cx, JS::HandleObject requestInstance, JS::HandleValue input,
                          JS::HandleValue init_val) {
  auto request_handle_res = host_api::HttpReq::make();
  if (auto *err = request_handle_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return nullptr;
  }

  auto body = host_api::HttpBody::make();
  if (auto *err = body.to_err()) {
    HANDLE_ERROR(cx, *err);
    return nullptr;
  }

  auto request_handle = request_handle_res.unwrap();
  JS::RootedObject request(cx, create(cx, requestInstance, request_handle, body.unwrap(), false));
  if (!request) {
    return nullptr;
  }

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
    method_str = Request::method(cx, input_request);
    if (!method_str) {
      return nullptr;
    }

    // referrer: `request`’s referrer.
    // TODO: evaluate whether we want to implement support for setting the
    // `referer` [sic] header based on this or not.

    // cache mode: `request`’s cache mode.
    // TODO: implement support for cache mode-based headers setting.

    // header list: A copy of `request`’s header list.
    // Note: copying the headers is postponed, see step 32 below.
    input_headers =
        RequestOrResponse::headers<builtins::Headers::Mode::ProxyToRequest>(cx, input_request);
    if (!input_headers) {
      return nullptr;
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
        cx, JS_NewObjectWithGivenProto(cx, &builtins::URL::class_, builtins::URL::proto_obj));
    if (!url_instance)
      return nullptr;

    JS::RootedObject parsedURL(
        cx, builtins::URL::create(cx, url_instance, input, builtins::Fastly::baseURL));

    // 2.  If `parsedURL` is failure, then throw a `TypeError`.
    if (!parsedURL) {
      return nullptr;
    }

    // 3.  If `parsedURL` includes credentials, then throw a `TypeError`.
    // (N/A)

    // 4.  Set `request` to a new request whose URL is `parsedURL`.
    // Instead, we store `url_str` to apply below.
    JS::RootedValue url_val(cx, JS::ObjectValue(*parsedURL));
    url_str = JS::ToString(cx, url_val);
    if (!url_str) {
      return nullptr;
    }

    // 5.  Set `fallbackMode` to "`cors`".
    // (N/A)
  }

  // Actually set the URL derived in steps 5 or 6 above.
  RequestOrResponse::set_url(request, StringValue(url_str));
  auto url = core::encode(cx, url_str);
  if (!url) {
    return nullptr;
  } else {
    auto res = request_handle.set_uri(url);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return nullptr;
    }
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
  JS::RootedValue backend_val(cx);
  JS::RootedValue cache_override(cx);
  JS::RootedValue fastly_val(cx);
  if (init_val.isObject()) {
    JS::RootedObject init(cx, init_val.toObjectOrNull());
    if (!JS_GetProperty(cx, init, "method", &method_val) ||
        !JS_GetProperty(cx, init, "headers", &headers_val) ||
        !JS_GetProperty(cx, init, "body", &body_val) ||
        !JS_GetProperty(cx, init, "backend", &backend_val) ||
        !JS_GetProperty(cx, init, "cacheOverride", &cache_override) ||
        !JS_GetProperty(cx, init, "fastly", &fastly_val)) {
      return nullptr;
    }
  } else if (!init_val.isNullOrUndefined()) {
    JS_ReportErrorLatin1(cx, "Request constructor: |init| parameter can't be converted to "
                             "a dictionary");
    return nullptr;
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
      return nullptr;
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
  bool is_get = true;
  if (method_str && !JS_StringEqualsLiteral(cx, method_str, "GET", &is_get)) {
    return nullptr;
  }

  bool is_get_or_head = is_get;

  if (!is_get) {
    auto method = core::encode(cx, method_str);
    if (!method) {
      return nullptr;
    }

    if (method_needs_normalization) {
      if (normalize_http_method(method.begin())) {
        // Replace the JS string with the normalized name.
        method_str = JS_NewStringCopyN(cx, method.begin(), method.len);
        if (!method_str) {
          return nullptr;
        }
      }
    }

    is_get_or_head = strcmp(method.begin(), "GET") == 0 || strcmp(method.begin(), "HEAD") == 0;

    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::Method), JS::StringValue(method_str));
    auto res = request_handle.set_method(method);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return nullptr;
    }
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
  if (!headers_val.isUndefined()) {
    JS::RootedObject headersInstance(cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_,
                                                                    builtins::Headers::proto_obj));
    if (!headersInstance)
      return nullptr;

    headers = builtins::Headers::create(
        cx, headersInstance, builtins::Headers::Mode::ProxyToRequest, request, headers_val);
  } else {
    JS::RootedObject headersInstance(cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_,
                                                                    builtins::Headers::proto_obj));
    if (!headersInstance)
      return nullptr;

    headers = builtins::Headers::create(
        cx, headersInstance, builtins::Headers::Mode::ProxyToRequest, request, input_headers);
  }

  if (!headers) {
    return nullptr;
  }

  JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));

  // 33.  Let `inputBody` be `input`’s requests body if `input` is a `Request`
  // object;
  //      otherwise null.
  // (skipped)

  // 34.  If either `init["body"]` exists and is non-null or `inputBody` is
  // non-null, and `request`’s method is ``GET`` or ``HEAD``, then throw a
  // TypeError.
  if ((input_has_body || !body_val.isNullOrUndefined()) && is_get_or_head) {
    JS_ReportErrorLatin1(cx, "Request constructor: HEAD or GET Request cannot have a body.");
    return nullptr;
  }

  // 35.  Let `initBody` be null.
  // (skipped)

  // Note: steps 36-41 boil down to "if there's an init body, use that.
  // Otherwise, if there's an input body, use that, but proxied through a
  // TransformStream to make sure it's not consumed by something else in the
  // meantime." Given that, we're restructuring things quite a bit below.

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
      return nullptr;
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
      JS_ReportErrorLatin1(cx, "Request constructor: the input request's body isn't usable.");
      return nullptr;
    }

    if (!inputBody) {
      // If `inputBody` is null, that means that it was never created, and hence
      // content can't have access to it. Instead of reifying it here to pass it
      // into a TransformStream, we just append the body on the host side and
      // mark it as used on the input Request.
      RequestOrResponse::append_body(cx, request, input_request);
      RequestOrResponse::mark_body_used(cx, input_request);
    } else {
      inputBody = builtins::TransformStream::create_rs_proxy(cx, inputBody);
      if (!inputBody) {
        return nullptr;
      }

      builtins::TransformStream::set_readable_used_as_body(cx, inputBody, request);
      JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::BodyStream),
                          JS::ObjectValue(*inputBody));
    }

    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::HasBody), JS::BooleanValue(true));
  }

  // 41.  Set this’s requests body to `finalBody`.
  // (implicit)

  // Apply the C@E-proprietary `backend` property.
  if (!backend_val.isUndefined()) {
    JS::RootedString backend(cx, JS::ToString(cx, backend_val));
    if (!backend) {
      return nullptr;
    }
    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::Backend), JS::StringValue(backend));
  } else if (input_request) {
    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::Backend),
                        JS::GetReservedSlot(input_request, static_cast<uint32_t>(Slots::Backend)));
  }

  // Apply the C@E-proprietary `cacheOverride` property.
  if (!cache_override.isUndefined()) {
    if (!set_cache_override(cx, request, cache_override)) {
      return nullptr;
    }
  } else if (input_request) {
    JS::SetReservedSlot(
        request, static_cast<uint32_t>(Slots::CacheOverride),
        JS::GetReservedSlot(input_request, static_cast<uint32_t>(Slots::CacheOverride)));
  }

  if (fastly_val.isObject()) {
    JS::RootedValue decompress_response_val(cx);
    JS::RootedObject fastly(cx, fastly_val.toObjectOrNull());
    if (!JS_GetProperty(cx, fastly, "decompressGzip", &decompress_response_val)) {
      return nullptr;
    }
    auto value = JS::ToBoolean(decompress_response_val);
    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::AutoDecompressGzip),
                        JS::BooleanValue(value));
  } else if (input_request) {
    JS::SetReservedSlot(
        request, static_cast<uint32_t>(Slots::AutoDecompressGzip),
        JS::GetReservedSlot(input_request, static_cast<uint32_t>(Slots::AutoDecompressGzip)));
  } else {
    JS::SetReservedSlot(request, static_cast<uint32_t>(Slots::AutoDecompressGzip),
                        JS::BooleanValue(false));
  }

  return request;
}

JSObject *Request::create_instance(JSContext *cx) {
  JS::RootedObject requestInstance(
      cx, JS_NewObjectWithGivenProto(cx, &Request::class_, Request::proto_obj));
  return requestInstance;
}

bool Request::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  REQUEST_HANDLER_ONLY("The Request builtin");
  CTOR_HEADER("Request", 1);
  JS::RootedObject requestInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  JS::RootedObject request(cx, create(cx, requestInstance, args[0], args.get(1)));
  if (!request)
    return false;

  args.rval().setObject(*request);
  return true;
}

// Needed for uniform access to Request and Response slots.
static_assert((int)Response::Slots::Body == (int)Request::Slots::Body);
static_assert((int)Response::Slots::BodyStream == (int)Request::Slots::BodyStream);
static_assert((int)Response::Slots::HasBody == (int)Request::Slots::HasBody);
static_assert((int)Response::Slots::BodyUsed == (int)Request::Slots::BodyUsed);
static_assert((int)Response::Slots::Headers == (int)Request::Slots::Headers);
static_assert((int)Response::Slots::Response == (int)Request::Slots::Request);

host_api::HttpResp Response::response_handle(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return host_api::HttpResp(
      JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::Response)).toInt32());
}

bool Response::is_upstream(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::IsUpstream)).toBoolean();
}

bool Response::is_grip_upgrade(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));
  return JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::IsGripUpgrade)).toBoolean();
}

const char *Response::grip_backend(JSObject *obj) {
  MOZ_ASSERT(is_instance(obj));

  auto backend = reinterpret_cast<char *>(
      JS::GetReservedSlot(obj, static_cast<uint32_t>(Slots::GripBackend)).toPrivate());
  MOZ_ASSERT(backend);
  return backend;
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

// TODO: store version client-side.
bool Response::version_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  auto res = response_handle(self).get_version();
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  args.rval().setInt32(res.unwrap());
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

  JSObject *headers =
      RequestOrResponse::headers<builtins::Headers::Mode::ProxyToResponse>(cx, self);
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
bool Response::redirect(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "redirect", 1)) {
    return false;
  }
  auto url = args.get(0);
  // 1. Let parsedURL be the result of parsing url with current settings object’s API base URL.
  JS::RootedObject urlInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::URL::class_, builtins::URL::proto_obj));
  if (!urlInstance) {
    return false;
  }
  JS::RootedObject parsedURL(
      cx, builtins::URL::create(cx, urlInstance, url, builtins::Fastly::baseURL));
  // 2. If parsedURL is failure, then throw a TypeError.
  if (!parsedURL) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_RESPONSE_REDIRECT_INVALID_URI);
    return false;
  }
  JS::RootedValue url_val(cx, JS::ObjectValue(*parsedURL));
  auto url_str = core::encode(cx, url_val);
  if (!url_str) {
    return false;
  }
  // 3. If status is not a redirect status, then throw a RangeError.
  // A redirect status is a status that is 301, 302, 303, 307, or 308.
  auto statusVal = args.get(1);
  uint16_t status;
  if (statusVal.isUndefined()) {
    status = 302;
  } else {
    if (!JS::ToUint16(cx, statusVal, &status)) {
      return false;
    }
  }
  if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_RESPONSE_REDIRECT_INVALID_STATUS);
    return false;
  }
  // 4. Let responseObject be the result of creating a Response object, given a new response,
  // "immutable", and this’s relevant Realm.
  auto response_handle_res = host_api::HttpResp::make();
  if (auto *err = response_handle_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto response_handle = response_handle_res.unwrap();
  if (!response_handle.is_valid()) {
    return false;
  }

  auto make_res = host_api::HttpBody::make();
  if (auto *err = make_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto body = make_res.unwrap();
  JS::RootedObject response_instance(cx, JS_NewObjectWithGivenProto(cx, &builtins::Response::class_,
                                                                    builtins::Response::proto_obj));
  if (!response_instance) {
    return false;
  }
  JS::RootedObject response(
      cx, create(cx, response_instance, response_handle, body, false, false, nullptr));
  if (!response) {
    return false;
  }

  // 5. Set responseObject’s response’s status to status.
  auto set_res = response_handle.set_status(status);
  if (auto *err = set_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  // To ensure that we really have the same status value as the host,
  // we always read it back here.
  auto get_res = response_handle.get_status();
  if (auto *err = get_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  status = get_res.unwrap();

  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::StatusMessage),
                      JS::StringValue(JS_GetEmptyString(cx)));
  // 6. Let value be parsedURL, serialized and isomorphic encoded.
  // 7. Append (`Location`, value) to responseObject’s response’s header list.
  JS::RootedObject headers(cx);
  JS::RootedObject headersInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_, builtins::Headers::proto_obj));
  if (!headersInstance)
    return false;

  headers = builtins::Headers::create(cx, headersInstance, builtins::Headers::Mode::ProxyToResponse,
                                      response);
  if (!headers) {
    return false;
  }
  if (!builtins::Headers::maybe_add(cx, headers, "location", url_str.begin())) {
    return false;
  }
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Redirected), JS::FalseValue());
  // 8. Return responseObject.

  args.rval().setObjectOrNull(response);
  return true;
}

namespace {
bool callbackCalled;
bool write_json_to_buf(const char16_t *str, uint32_t strlen, void *out) {
  callbackCalled = true;
  auto outstr = static_cast<std::u16string *>(out);
  outstr->append(str, strlen);

  return true;
}
} // namespace

bool Response::json(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "json", 1)) {
    return false;
  }
  JS::RootedValue data(cx, args.get(0));
  JS::RootedValue init_val(cx, args.get(1));
  JS::RootedObject replacer(cx);
  JS::RootedValue space(cx);

  std::u16string out;
  // 1. Let bytes the result of running serialize a JavaScript value to JSON bytes on data.
  callbackCalled = false;
  if (!JS::ToJSON(cx, data, replacer, space, &write_json_to_buf, &out)) {
    return false;
  }
  if (!callbackCalled) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_RESPONSE_JSON_INVALID_VALUE);
    return false;
  }
  // 2. Let body be the result of extracting bytes.

  // 3. Let responseObject be the result of creating a Response object, given a new response,
  // "response", and this’s relevant Realm.
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

    if (status == 204 || status == 205 || status == 304) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_RESPONSE_NULL_BODY_STATUS_WITH_BODY);
      return false;
    }

    if (!statusText_val.isUndefined() && !(statusText = JS::ToString(cx, statusText_val))) {
      return false;
    }

  } else if (!init_val.isNullOrUndefined()) {
    JS_ReportErrorLatin1(cx, "Response constructor: |init| parameter can't be converted to "
                             "a dictionary");
    return false;
  }

  auto response_handle_res = host_api::HttpResp::make();
  if (auto *err = response_handle_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto response_handle = response_handle_res.unwrap();
  if (!response_handle.is_valid()) {
    return false;
  }

  auto make_res = host_api::HttpBody::make();
  if (auto *err = make_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto body = make_res.unwrap();
  JS::RootedString string(cx, JS_NewUCStringCopyN(cx, out.c_str(), out.length()));
  auto stringChars = core::encode(cx, string);

  auto write_res =
      body.write_all(reinterpret_cast<uint8_t *>(stringChars.begin()), stringChars.len);
  if (auto *err = write_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  JS::RootedObject response_instance(cx, JS_NewObjectWithGivenProto(cx, &builtins::Response::class_,
                                                                    builtins::Response::proto_obj));
  if (!response_instance) {
    return false;
  }
  JS::RootedObject response(
      cx, create(cx, response_instance, response_handle, body, false, false, nullptr));
  if (!response) {
    return false;
  }

  // Set `this`’s `response`’s `status` to `init`["status"].
  auto set_res = response_handle.set_status(status);
  if (auto *err = set_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  // To ensure that we really have the same status value as the host,
  // we always read it back here.
  auto get_res = response_handle.get_status();
  if (auto *err = get_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  status = get_res.unwrap();

  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));

  // Set `this`’s `response`’s `status message` to `init`["statusText"].
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::StatusMessage),
                      JS::StringValue(statusText));

  // If `init`["headers"] `exists`, then `fill` `this`’s `headers` with
  // `init`["headers"].
  JS::RootedObject headers(cx);
  JS::RootedObject headersInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_, builtins::Headers::proto_obj));
  if (!headersInstance)
    return false;

  headers = builtins::Headers::create(cx, headersInstance, builtins::Headers::Mode::ProxyToResponse,
                                      response, headers_val);
  if (!headers) {
    return false;
  }
  // 4. Perform initialize a response given responseObject, init, and (body, "application/json").
  if (!builtins::Headers::maybe_add(cx, headers, "content-type", "application/json")) {
    return false;
  }
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Redirected), JS::FalseValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::HasBody), JS::TrueValue());
  RequestOrResponse::set_url(response, JS_GetEmptyStringValue(cx));

  // 5. Return responseObject.
  args.rval().setObjectOrNull(response);
  return true;
}

const JSFunctionSpec Response::static_methods[] = {
    JS_FN("redirect", redirect, 1, JSPROP_ENUMERATE),
    JS_FN("json", json, 1, JSPROP_ENUMERATE),
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
    JS_PSG("version", version_get, JSPROP_ENUMERATE),
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
  REQUEST_HANDLER_ONLY("The Response builtin");

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
    JS_ReportErrorLatin1(cx, "Response constructor: |init| parameter can't be converted to "
                             "a dictionary");
    return false;
  }

  // 1.  If `init`["status"] is not in the range 200 to 599, inclusive, then
  // `throw` a ``RangeError``.
  if (status < 200 || status > 599) {
    JS_ReportErrorLatin1(cx, "Response constructor: invalid status %u", status);
    return false;
  }

  // 2.  If `init`["statusText"] does not match the `reason-phrase` token
  // production, then `throw` a ``TypeError``. Skipped: the statusText can only
  // be consumed by the content creating it, so we're lenient about its format.

  // 3.  Set `this`’s `response` to a new `response`.
  // TODO(performance): consider not creating a host-side representation for responses
  // eagerly. Some applications create Response objects purely for internal use,
  // e.g. to represent cache entries. While that's perhaps not ideal to begin
  // with, it exists, so we should handle it in a good way, and not be
  // superfluously slow.
  // https://github.com/fastly/js-compute-runtime/issues/219
  // TODO(performance): enable creating Response objects during the init phase, and only
  // creating the host-side representation when processing requests.
  // https://github.com/fastly/js-compute-runtime/issues/220
  auto response_handle_res = host_api::HttpResp::make();
  if (auto *err = response_handle_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto make_res = host_api::HttpBody::make();
  if (auto *err = make_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto response_handle = response_handle_res.unwrap();
  auto body = make_res.unwrap();
  JS::RootedObject responseInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  JS::RootedObject response(
      cx, create(cx, responseInstance, response_handle, body, false, false, nullptr));
  if (!response) {
    return false;
  }

  RequestOrResponse::set_url(response, JS_GetEmptyStringValue(cx));

  // 4.  Set `this`’s `headers` to a `new` ``Headers`` object with `this`’s
  // `relevant Realm`,
  //     whose `header list` is `this`’s `response`’s `header list` and `guard`
  //     is "`response`".
  // (implicit)

  // 5.  Set `this`’s `response`’s `status` to `init`["status"].
  auto set_res = response_handle.set_status(status);
  if (auto *err = set_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  // To ensure that we really have the same status value as the host,
  // we always read it back here.
  auto get_res = response_handle.get_status();
  if (auto *err = get_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }
  status = get_res.unwrap();

  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));

  // 6.  Set `this`’s `response`’s `status message` to `init`["statusText"].
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::StatusMessage),
                      JS::StringValue(statusText));

  // 7.  If `init`["headers"] `exists`, then `fill` `this`’s `headers` with
  // `init`["headers"].
  JS::RootedObject headers(cx);
  JS::RootedObject headersInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::Headers::class_, builtins::Headers::proto_obj));
  if (!headersInstance)
    return false;

  headers = builtins::Headers::create(cx, headersInstance, builtins::Headers::Mode::ProxyToResponse,
                                      response, headers_val);
  if (!headers) {
    return false;
  }
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));
  // 8.  If `body` is non-null, then:
  if ((!body_val.isNullOrUndefined())) {
    //     1.  If `init`["status"] is a `null body status`, then `throw` a
    //     ``TypeError``.
    if (status == 204 || status == 205 || status == 304) {
      JS_ReportErrorLatin1(cx, "Response constructor: Response body is given "
                               "with a null body status.");
      return false;
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

JSObject *Response::create(JSContext *cx, JS::HandleObject response,
                           host_api::HttpResp response_handle, host_api::HttpBody body_handle,
                           bool is_upstream, bool is_grip, JS::UniqueChars backend) {
  // MOZ_ASSERT(cx);
  // MOZ_ASSERT(is_instance(response));
  // MOZ_ASSERT(response_handle);
  // MOZ_ASSERT(body_handle);
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Response),
                      JS::Int32Value(response_handle.handle));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Headers), JS::NullValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Body),
                      JS::Int32Value(body_handle.handle));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::BodyStream), JS::NullValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::HasBody), JS::FalseValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::BodyUsed), JS::FalseValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Redirected), JS::FalseValue());
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::IsUpstream),
                      JS::BooleanValue(is_upstream));
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::IsGripUpgrade),
                      JS::BooleanValue(is_grip));

  if (is_upstream) {
    auto res = response_handle.get_status();
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return nullptr;
    }

    auto status = res.unwrap();
    JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::Status), JS::Int32Value(status));
    set_status_message_from_code(cx, response, status);

    if (!(status == 204 || status == 205 || status == 304)) {
      JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::HasBody), JS::TrueValue());
    }
  }
  JS::SetReservedSlot(response, static_cast<uint32_t>(Slots::GripBackend),
                      JS::PrivateValue(std::move(backend.release())));
  return response;
}

} // namespace builtins
