#include "fetch-api.h"
#include "builtin.h"
#include "encode.h"
#include "event_loop.h"
#include "extension-api.h"
#include "headers.h"
#include "mozilla/Assertions.h"
#include "request-response.h"

#include "../blob.h"
#include "../url.h"

#include "js/String.h"
#include <charconv>
#include <fmt/format.h>
#include <memory>

namespace builtins::web::fetch {

using blob::Blob;
using fetch::Headers;
using host_api::HostString;
using url::URL;

static api::Engine *ENGINE;

enum class FetchScheme {
  About,
  Blob,
  Data,
  File,
  Http,
  Https,
};

std::optional<FetchScheme> scheme_from_url(const std::string_view &url) {
  if (url.starts_with("about:")) {
    return FetchScheme::About;
  } else if (url.starts_with("blob:")) {
    return FetchScheme::Blob;
  } else if (url.starts_with("data:")) {
    return FetchScheme::Data;
  } else if (url.starts_with("file:")) {
    return FetchScheme::File;
  } else if (url.starts_with("http")) {
    return FetchScheme::Http;
  } else if (url.starts_with("https")) {
    return FetchScheme::Https;
  } else {
    return std::nullopt;
  }
}

bool fetch_https(JSContext *cx, HandleObject request_obj, HostString method, HostString url,
                 MutableHandleValue rval) {
  unique_ptr<host_api::HttpHeaders> headers =
      RequestOrResponse::headers_handle_clone(cx, request_obj);
  if (!headers) {
    return false;
  }

  auto request = host_api::HttpOutgoingRequest::make(method, std::move(url), std::move(headers));
  MOZ_RELEASE_ASSERT(request);
  JS_SetReservedSlot(request_obj, static_cast<uint32_t>(Request::Slots::Request),
                     PrivateValue(request));

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise)
    return false;

  bool streaming = false;
  if (!RequestOrResponse::maybe_stream_body(cx, request_obj, request, &streaming)) {
    return false;
  }
  if (streaming) {
    // Ensure that the body handle is stored before making the request handle invalid by sending it.
    request->body();
  }

  host_api::FutureHttpIncomingResponse *pending_handle;
  {
    auto res = request->send();
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }

    pending_handle = res.unwrap();
  }

  // If the request body is streamed, we need to wait for streaming to complete
  // before marking the request as pending.
  if (!streaming) {
    ENGINE->queue_async_task(new ResponseFutureTask(request_obj, pending_handle));
  }

  SetReservedSlot(request_obj, static_cast<uint32_t>(Request::Slots::ResponsePromise),
                  ObjectValue(*response_promise));
  SetReservedSlot(request_obj, static_cast<uint32_t>(Request::Slots::PendingResponseHandle),
                  PrivateValue(pending_handle));

  rval.setObject(*response_promise);
  return true;
}

/// https://fetch.spec.whatwg.org/#scheme-fetch
bool fetch_blob(JSContext *cx, HandleObject request_obj, HostString method, HostString url,
                MutableHandleValue rval) {

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise) {
    return false;
  }

  rval.setObject(*response_promise);

  // 1. Let blobURLEntry be request's current URL's blob URL entry.
  RootedString blob_url(cx);

  // 2. If request's method is not `GET` or blobURLEntry is null, then return a network error.
  if (std::memcmp(method.ptr.get(), "GET", method.len) != 0) {
    return api::throw_error(cx, FetchErrors::FetchNetworkError);
  }

  // 3. Let requestEnvironment be the result of determining the environment given request.
  // 4. Let isTopLevelNavigation be true if request's destination is "document"; otherwise, false.
  // 5. If isTopLevelNavigation is false and requestEnvironment is null, then return a network error.
  // 6. Let navigationOrEnvironment be the string "navigation" if isTopLevelNavigation is true;
  //  otherwise, requestEnvironment.
  //  N/A
  // 7. Let blob be the result of obtaining a blob object given blobURLEntry and
  // navigationOrEnvironment.
  std::string url_key(url.ptr.get());
  RootedObject blob(cx, URL::getObjectURL(url_key));

  // 8. If blob is not a Blob object, then return a network error.
  if (!blob || !Blob::is_instance(blob)) {
    return api::throw_error(cx, FetchErrors::FetchNetworkError);
  }

  // 9. Let response be a new response.
  RootedObject response_obj(cx, Response::create(cx));
  if (!response_obj) {
    return false;
  }

  // 10. Let fullLength be blob's size.
  auto full_len = Blob::blob_size(blob);
  // 11. Let serializedFullLength be fullLength, serialized and isomorphic encoded.
  // 12. Let type be blob's type.
  RootedString type(cx, Blob::type(blob));

  JS::RootedObject req_headers(cx, RequestOrResponse::headers(cx, request_obj));
  if (!req_headers) {
    return false;
  }

  auto maybe_range_index = Headers::lookup(cx, req_headers, "Range");

  // 13. If request's header list does not contain `Range`:
  if (!maybe_range_index.has_value()) {
    // 1. Let bodyWithType be the result of safely extracting blob.
    RootedValue body_val(cx, JS::ObjectValue(*blob));
    RootedValue init_val(cx);
    if (!Response::initialize(cx, response_obj, body_val, init_val)) {
      return false;
    }

    // 2. Set response's status message to `OK`.
    Response::set_status_message_from_code(cx, response_obj, 200);

    // 3. Set response's body to bodyWithType's body.
    // 4. Set response's header list to (`Content-Length`, serializedFullLength), (`Content-Type`, type).
    // 3 and 4 done at the end.
  // 14. Otherwise:
  } else {
    // 1. Set response's range-requested flag.
    // 2. Let rangeHeader be the result of getting `Range` from request's header list.
    // 3. Let rangeValue be the result of parsing a single range header value given rangeHeader and true.
    // 4. If rangeValue is failure, then return a network error.
    auto *headers = Headers::get_index(cx, req_headers, maybe_range_index.value());
    MOZ_ASSERT(headers);

    auto &[key, val] = *headers;
    std::string_view range_value(val);

    if (!range_value.starts_with("bytes=")) {
      return api::throw_error(cx, FetchErrors::FetchNetworkError);
    }

    range_value.remove_prefix(6); // bytes=
    auto dash_pos = range_value.find('-');

    if (dash_pos == std::string_view::npos) {
      return api::throw_error(cx, FetchErrors::FetchNetworkError);
    }

    auto start_str = range_value.substr(0, dash_pos);
    auto end_str = range_value.substr(dash_pos + 1);

    auto to_size = [](std::string_view s) -> std::optional<size_t> {
      size_t v;
      auto [ptr, ec] = std::from_chars(s.begin(), s.end(), v);
      return ec == std::errc() ? std::optional<size_t>(v) : std::nullopt;
    };

    //   5. Let (rangeStart, rangeEnd) be rangeValue.
    auto maybe_start_range = to_size(start_str);
    auto maybe_end_range = to_size(end_str);

    size_t start_range = 0;
    size_t end_range = 0;

    // 6. If rangeStart is null:
    if (!maybe_start_range.has_value()) {
      // If both start_range and end_range are not provided, it's an error.
      if (!maybe_end_range.has_value()) {
        return api::throw_error(cx, FetchErrors::FetchNetworkError);
      }

      // 1. Set rangeStart to fullLength - rangeEnd.
      // 2. Set rangeEnd to rangeStart + rangeEnd - 1.
      start_range = full_len - maybe_end_range.value();
      end_range = start_range + maybe_end_range.value() - 1;
      // 7. Otherwise:
    } else {
      // 1. If rangeStart is greater than or equal to fullLength, then return a network error.
      if (maybe_start_range.value() > full_len) {
        return api::throw_error(cx, FetchErrors::FetchNetworkError);
      }
      // 2. If rangeEnd is null or rangeEnd is greater than or equal to fullLength, then set
      // rangeEnd to fullLength - 1.
      start_range = maybe_start_range.value();
      end_range = std::min(maybe_end_range.value_or(full_len - 1), full_len - 1);
    }

    // 8. Let slicedBlob be the result of invoking slice blob given blob, rangeStart, rangeEnd + 1, and type.
    // 9. Let slicedBodyWithType be the result of safely extracting slicedBlob.
    // 10. Set response's body to slicedBodyWithType's body.
    // 11. Let serializedSlicedLength be slicedBlob's size, serialized and isomorphic encoded.

    /// TODO: fix blob API so that we can pass start,end values directly
    JS::Value vp[4];
    vp[2].setInt32(start_range);
    vp[3].setInt32(end_range + 1);
    auto args = CallArgsFromVp(2, vp);

    RootedValue sliced_blob_val(cx);
    if (!Blob::slice(cx, blob, args, &sliced_blob_val)) {
      return false;
    }

    RootedValue init_val(cx);
    if (!Response::initialize(cx, response_obj, sliced_blob_val, init_val)) {
      return false;
    }

    //   12. Let contentRange be the result of invoking build a content range given rangeStart,
    //   rangeEnd, and fullLength.
    //   13. Set response's status to 206.
    //   14. Set response's status message to `Partial Content`.
    Response::set_status(response_obj, 206);
    Response::set_status_message_from_code(cx, response_obj, 206);
    //   15. Set response's header list to  (`Content-Length`, serializedSlicedLength),
    //   (`Content-Type`, type), (`Content-Range`, contentRange).

    JS::RootedObject resp_headers(cx, RequestOrResponse::headers(cx, response_obj));
    if (!resp_headers) {
      return false;
    }

    auto content_range_str = fmt::format("bytes {}-{}/{}", start_range, end_range, full_len);
    if (!Headers::set_valid_if_undefined(cx, resp_headers, "Content-Range", content_range_str)) {
      return false;
    }

    // overwrite full_len with sliced blob length so it's written to headers
    full_len = Blob::blob_size(&sliced_blob_val.toObject());
  }

  JS::RootedObject resp_headers(cx, RequestOrResponse::headers(cx, response_obj));
  if (!resp_headers) {
    return false;
  }

  auto full_len_str = std::to_string(full_len);
  if (!Headers::set_valid_if_undefined(cx, resp_headers, "Content-Length", full_len_str)) {
    return false;
  }

  auto chars = core::encode(cx, type);
  if (!chars) {
    return false;
  }

  auto type_str = JS::GetStringLength(type) ? chars.ptr.get() : "";
  if (!Headers::set_valid_if_undefined(cx, resp_headers, "Content-Type", type_str)) {
    return false;
  }

  // Blob response type is "basic"
  JS::SetReservedSlot(response_obj, static_cast<uint32_t>(Response::Slots::Type),
                      JS::Int32Value(Response::Type::Basic));

  // 15. Return response.
  JS::RootedValue result(cx);
  result.setObject(*response_obj);
  JS::ResolvePromise(cx, response_promise, result);

  return true;
}

// TODO: throw in all Request methods/getters that rely on host calls once a
// request has been sent. The host won't let us act on them anymore anyway.
/**
 * The `fetch` global function
 * https://fetch.spec.whatwg.org/#fetch-method
 */
bool fetch(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  REQUEST_HANDLER_ONLY("fetch")

  if (!args.requireAtLeast(cx, "fetch", 1)) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedObject request_obj(cx, Request::create(cx));
  if (!request_obj) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  if (!Request::initialize(cx, request_obj, args[0], args.get(1),
                           Headers::HeadersGuard::Immutable)) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedString method_str(cx, Request::method(request_obj));
  HostString method = core::encode(cx, method_str);
  if (!method.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedValue url_val(cx, RequestOrResponse::url(request_obj));
  HostString url = core::encode(cx, url_val);
  if (!url.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  switch (scheme_from_url(url).value_or(FetchScheme::Https)) {
  case FetchScheme::Blob: {
    auto res = fetch_blob(cx, request_obj, std::move(method), std::move(url), args.rval());
    if (!res) {
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }
    break;
  }
  case FetchScheme::Http:
  case FetchScheme::Https:
  default: {
    auto res = fetch_https(cx, request_obj, std::move(method), std::move(url), args.rval());
    if (!res) {
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }
    break;
  }
  }

  return true;
}

const JSFunctionSpec methods[] = {JS_FN("fetch", fetch, 2, JSPROP_ENUMERATE), JS_FS_END};

bool install(api::Engine *engine) {
  ENGINE = engine;

  if (!JS_DefineFunctions(engine->cx(), engine->global(), methods))
    return false;
  if (!request_response::install(engine)) {
    return false;
  }
  if (!Headers::init_class(engine->cx(), engine->global()))
    return false;
  return true;
}

} // namespace builtins::web::fetch
