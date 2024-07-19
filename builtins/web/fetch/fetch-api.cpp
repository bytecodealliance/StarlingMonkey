#include "fetch-api.h"
#include "event_loop.h"
#include "headers.h"
#include "request-response.h"

#include <encode.h>

#include <memory>

using builtins::web::fetch::Headers;

namespace builtins::web::fetch {

static api::Engine *ENGINE;

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
  host_api::HostString method = core::encode(cx, method_str);
  if (!method.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedValue url_val(cx, RequestOrResponse::url(request_obj));
  host_api::HostString url = core::encode(cx, url_val);
  if (!url.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  unique_ptr<host_api::HttpHeaders> headers =
      RequestOrResponse::headers_handle_clone(cx, request_obj);
  if (!headers) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  auto request = host_api::HttpOutgoingRequest::make(method, std::move(url), std::move(headers));
  MOZ_RELEASE_ASSERT(request);
  JS_SetReservedSlot(request_obj, static_cast<uint32_t>(Request::Slots::Request),
                     PrivateValue(request));

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise)
    return ReturnPromiseRejectedWithPendingError(cx, args);

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
      return ReturnPromiseRejectedWithPendingError(cx, args);
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
