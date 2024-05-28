#include "fetch-api.h"
#include "event_loop.h"
#include "headers.h"
#include "request-response.h"

#include <encode.h>

#include <memory>

namespace builtins::web::fetch {

static api::Engine *ENGINE;

class ResponseFutureTask final : public api::AsyncTask {
  Heap<JSObject *> request_;
  host_api::FutureHttpIncomingResponse *future_;

public:
  explicit ResponseFutureTask(const HandleObject request,
                              host_api::FutureHttpIncomingResponse *future)
      : request_(request), future_(future) {
    auto res = future->subscribe();
    MOZ_ASSERT(!res.is_err(), "Subscribing to a future should never fail");
    handle_ = res.unwrap();
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    // MOZ_ASSERT(ready());
    JSContext *cx = engine->cx();

    const RootedObject request(cx, request_);
    RootedObject response_promise(cx, Request::response_promise(request));

    auto res = future_->maybe_response();
    if (res.is_err()) {
      JS_ReportErrorUTF8(cx, "NetworkError when attempting to fetch resource.");
      return RejectPromiseWithPendingError(cx, response_promise);
    }

    auto maybe_response = res.unwrap();
    MOZ_ASSERT(maybe_response.has_value());
    auto response = maybe_response.value();
    RootedObject response_obj(
        cx, JS_NewObjectWithGivenProto(cx, &Response::class_, Response::proto_obj));
    if (!response_obj) {
      return false;
    }

    response_obj = Response::create_incoming(cx, response_obj, response);
    if (!response_obj) {
      return false;
    }

    RequestOrResponse::set_url(response_obj, RequestOrResponse::url(request));
    RootedValue response_val(cx, ObjectValue(*response_obj));
    if (!ResolvePromise(cx, response_promise, response_val)) {
      return false;
    }

    return cancel(engine);
  }

  [[nodiscard]] bool cancel(api::Engine *engine) override {
    // TODO(TS): implement
    handle_ = -1;
    return true;
  }

  void trace(JSTracer *trc) override { TraceEdge(trc, &request_, "Request for response future"); }
};

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

  RootedObject request_obj(
      cx, JS_NewObjectWithGivenProto(cx, &Request::class_, Request::proto_obj));
  if (!request_obj) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  if (!Request::create(cx, request_obj, args[0], args.get(1))) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedString method_str(cx, Request::method(cx, request_obj));
  if (!method_str) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  host_api::HostString method = core::encode(cx, method_str);
  if (!method.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedValue url_val(cx, RequestOrResponse::url(request_obj));
  host_api::HostString url = core::encode(cx, url_val);
  if (!url.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  unique_ptr<host_api::HttpHeaders> headers;
  RootedObject headers_obj(cx, RequestOrResponse::maybe_headers(request_obj));
  if (headers_obj) {
    headers = Headers::handle_clone(cx, headers_obj);
  } else {
    headers = std::make_unique<host_api::HttpHeaders>();
  }

  if (!headers) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  auto request = host_api::HttpOutgoingRequest::make(method, std::move(url),
                                                     std::move(headers));
  MOZ_RELEASE_ASSERT(request);
  JS_SetReservedSlot(request_obj, static_cast<uint32_t>(Request::Slots::Request),
                     PrivateValue(request));

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise)
    return ReturnPromiseRejectedWithPendingError(cx, args);

  bool streaming = false;
  if (!RequestOrResponse::maybe_stream_body(cx, request_obj, &streaming)) {
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
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    pending_handle = res.unwrap();
  }

  // If the request body is streamed, we need to wait for streaming to complete
  // before marking the request as pending.
  if (!streaming) {
    ENGINE->queue_async_task(new ResponseFutureTask(request_obj, pending_handle));
  }

  JS::SetReservedSlot(request_obj, static_cast<uint32_t>(Request::Slots::ResponsePromise),
                      ObjectValue(*response_promise));

  args.rval().setObject(*response_promise);
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
