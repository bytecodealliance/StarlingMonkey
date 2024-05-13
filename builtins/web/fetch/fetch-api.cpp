#include "fetch-api.h"
#include "event_loop.h"
#include "headers.h"
#include "request-response.h"

namespace builtins::web::fetch {

static api::Engine *ENGINE;

class ResponseFutureTask final : public api::AsyncTask {
  Heap<JSObject *> request_;
  host_api::FutureHttpIncomingResponse *future_;

public:
  explicit ResponseFutureTask(const HandleObject request,
                              host_api::FutureHttpIncomingResponse *future)
      : request_(request), future_(future) {
    fprintf(stderr, "ResponseFutureTask::ResponseFutureTask\n");
    ensure_no_response(ENGINE->cx(), request, future);
    auto res = future->subscribe();
    MOZ_ASSERT(!res.is_err(), "Subscribing to a future should never fail");
    this->handle_ = res.unwrap();

    fprintf(stderr, "ResponseFutureTask::ResponseFutureTask END!\n");
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    fprintf(stderr, "ResponseFutureTask::run\n");
    // MOZ_ASSERT(ready());
    JSContext *cx = engine->cx();

    const RootedObject request(cx, request_);
    RootedObject response_promise(cx, Request::response_promise(request));

    fprintf(stderr, "ResponseFutureTask::run: maybe_response\n");
    auto res = future_->maybe_response();
    if (res.is_err()) {
      JS_ReportErrorUTF8(cx, "NetworkError when attempting to fetch resource.");
      return RejectPromiseWithPendingError(cx, response_promise);
    }

    fprintf(stderr, "ResponseFutureTask::run: maybe_response\n");
    auto maybe_response = res.unwrap();
    MOZ_ASSERT(maybe_response.has_value());
    auto response = maybe_response.value();
    RootedObject response_obj(
        cx, JS_NewObjectWithGivenProto(cx, &Response::class_, Response::proto_obj));
    if (!response_obj) {
      return false;
    }

    response_obj = Response::create(cx, response_obj, response);
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
    fprintf(stderr, "ResponseFutureTask::cancel\n");
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

  RootedObject requestInstance(
      cx, JS_NewObjectWithGivenProto(cx, &Request::class_, Request::proto_obj));
  if (!requestInstance)
    return false;

  RootedObject request(cx, Request::create(cx, requestInstance, args[0], args.get(1)));
  if (!request) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise)
    return ReturnPromiseRejectedWithPendingError(cx, args);

  bool streaming = false;
  if (!RequestOrResponse::maybe_stream_body(cx, request, &streaming)) {
    return false;
  }

  host_api::FutureHttpIncomingResponse *pending_handle;
  {
    auto request_handle = Request::outgoing_handle(request);
    fprintf(stderr, "fetch: sending request\n");
    auto res = request_handle->send();
    fprintf(stderr, "fetch: send initiated\n");

    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    pending_handle = res.unwrap();
  }

    JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::ResponsePromise),
                      JS::ObjectValue(*response_promise));

  // If the request body is streamed, we need to wait for streaming to complete
  // before marking the request as pending.
  if (!streaming) {
    fprintf(stderr, "fetch: scheduling response future task\n");

    if (ensure_no_response(cx, request, pending_handle)) {
      fprintf(stderr, "fetch: maybe_response has value!\n");
    } else {
      fprintf(stderr, "fetch: maybe_response has no value! Future queued.\n");
      
      ENGINE->queue_async_task(new ResponseFutureTask(request, pending_handle));
    }
  }

  fprintf(stderr, "fetch: returning response promise\n");
  args.rval().setObject(*response_promise);
  return true;
}

bool ensure_no_response(JSContext *cx, JS::HandleObject req,
                        host_api::FutureHttpIncomingResponse *future) {

    const RootedObject request(cx, req);
    RootedObject response_promise(cx, Request::response_promise(request));

    fprintf(stderr, "ensure_no_response: maybe_response\n");
    auto res = future->maybe_response();
    if (res.is_err()) {
      JS_ReportErrorUTF8(cx, "NetworkError when attempting to fetch resource.");
      return RejectPromiseWithPendingError(cx, response_promise);
    }

    fprintf(stderr, "ensure_no_response: maybe_response is not error \n");
    auto maybe_response = res.unwrap();
    if (maybe_response.has_value()) {
      fprintf(stderr, "ensure_no_response: maybe_response has value!\n");
      auto response = maybe_response.value();
      RootedObject response_obj(
          cx, JS_NewObjectWithGivenProto(cx, &Response::class_, Response::proto_obj));
      if (!response_obj) {
        return false;
      }

      response_obj = Response::create(cx, response_obj, response);
      if (!response_obj) {
        return false;
      }

      RequestOrResponse::set_url(response_obj, RequestOrResponse::url(request));
      RootedValue response_val(cx, ObjectValue(*response_obj));
      if (!ResolvePromise(cx, response_promise, response_val)) {
        return false;
      } else {
        return true;
      }
    } else {
      fprintf(stderr, "ensure_no_response: maybe_response has no value\n");
      return false;
    }
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
