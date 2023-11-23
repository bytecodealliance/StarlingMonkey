#include "event_loop.h"
#include "fetch-api.h"
#include "headers.h"
#include "request-response.h"

namespace builtins::web::fetch {

static core::Engine* ENGINE;

class ResponseFutureTask final : public core::AsyncTask {
  Heap<JSObject *> request_;
  host_api::FutureHttpIncomingResponse* pending_handle_;

public:
  explicit ResponseFutureTask(const HandleObject request, host_api::FutureHttpIncomingResponse* pending_handle)
    : request_(request), pending_handle_(pending_handle) {
    handle_id_ = pending_handle->async_handle().handle.__handle;
  }

  [[nodiscard]] bool run(core::Engine* engine) override {
    // MOZ_ASSERT(ready());
    JSContext* cx = engine->cx();

    const RootedObject request(cx, request_);
    RootedObject response_promise(cx, Request::response_promise(request));


    auto res = pending_handle_->poll();
    if (res.is_err()) {
      JS_ReportErrorUTF8(cx, "NetworkError when attempting to fetch resource.");
      return RejectPromiseWithPendingError(cx, response_promise);
    }

    auto maybe_response = res.unwrap();
    MOZ_ASSERT(maybe_response.has_value());
    auto response = maybe_response.value();
    RootedObject response_obj(cx, JS_NewObjectWithGivenProto(cx, &Response::class_,
                                                                  Response::proto_obj));
    if (!response_obj) {
      return false;
    }

    response_obj = Response::create(cx, response_obj, response);
    if (!response_obj) {
      return false;
    }

    RequestOrResponse::set_url(
        response_obj, RequestOrResponse::url(request));
    RootedValue response_val(cx, ObjectValue(*response_obj));
    if (!ResolvePromise(cx, response_promise, response_val)) {
      return false;
    }

    return cancel(engine);
  }

  [[nodiscard]] bool cancel(core::Engine* engine) override {
    // TODO(TS): implement
    handle_id_ = -1;
    return true;
  }

  bool ready() override {
    // TODO(TS): implement
    return true;
  }

  void trace(JSTracer *trc) override {
    TraceEdge(trc, &request_, "Request for response future");
  }
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

  RootedObject requestInstance(
      cx, JS_NewObjectWithGivenProto(cx, &Request::class_,
                                     Request::proto_obj));
  if (!requestInstance)
    return false;

  RootedObject request(
      cx, Request::create(cx, requestInstance, args[0], args.get(1)));
  if (!request) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

#ifdef CAE
  RootedString backend(cx, Request::backend(request));
  if (!backend) {
    if (builtins::Fastly::allowDynamicBackends) {
      JS::RootedObject dynamicBackend(cx,
                                      builtins::Backend::create(cx, request));
      if (!dynamicBackend) {
        return false;
      }
      backend.set(builtins::Backend::name(cx, dynamicBackend));
    } else {
      backend = builtins::Fastly::defaultBackend;
      if (!backend) {
        auto handle = Request::request_handle(request);

        auto res = handle.get_uri();
        if (auto *err = res.to_err()) {
          HANDLE_ERROR(cx, *err);
        } else {
          JS_ReportErrorLatin1(
              cx,
              "No backend specified for request with url %s. "
              "Must provide a `backend` property on the `init` object "
              "passed to either `new Request()` or `fetch`",
              res.unwrap().begin());
        }
        return ReturnPromiseRejectedWithPendingError(cx, args);
      }
    }
  }

  host_api::HostString backend_chars = core::encode(cx, backend);
  if (!backend_chars.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  if (!Request::apply_cache_override(cx, request)) {
    return false;
  }

  if (!Request::apply_auto_decompress_gzip(cx, request)) {
    return false;
  }
#endif // CAE

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise)
    return ReturnPromiseRejectedWithPendingError(cx, args);

  bool streaming = false;
  if (!RequestOrResponse::maybe_stream_body(cx, request, &streaming)) {
    return false;
  }

  host_api::FutureHttpIncomingResponse* pending_handle;
  {
    auto request_handle = Request::outgoing_handle(request);
    auto res = request_handle->send();

    if (auto *err = res.to_err()) {
#ifdef CAE
      if (host_api::error_is_generic(*err) ||
          host_api::error_is_invalid_argument(*err)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_REQUEST_BACKEND_DOES_NOT_EXIST,
                                  backend_chars.ptr.get());
      } else {
        HANDLE_ERROR(cx, *err);
      }
#else
      HANDLE_ERROR(cx, *err);
#endif // CAE
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    pending_handle = res.unwrap();
  }

  // If the request body is streamed, we need to wait for streaming to complete
  // before marking the request as pending.
  if (!streaming) {
    ENGINE->queue_async_task(new ResponseFutureTask(request, pending_handle));
  }

  JS::SetReservedSlot(
      request, static_cast<uint32_t>(Request::Slots::ResponsePromise),
      JS::ObjectValue(*response_promise));

  args.rval().setObject(*response_promise);
  return true;
}

const JSFunctionSpec methods[] = {JS_FN("fetch", fetch, 2, JSPROP_ENUMERATE),
                                  JS_FS_END};

bool install(core::Engine* engine) {
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
