#ifndef BUILTIN_REQUEST_RESPONSE
#define BUILTIN_REQUEST_RESPONSE

#include "headers.h"
#include "host_api.h"

namespace builtins {
namespace web {
namespace fetch {

namespace request_response {

bool install(api::Engine *engine);

}

class RequestOrResponse final {

public:
  enum class Slots {
    RequestOrResponse,
    BodyStream,
    BodyAllPromise,
    HasBody,
    BodyUsed,
    Headers,
    URL,
    Count,
  };

  static bool is_instance(JSObject *obj);
  static bool is_incoming(JSObject *obj);
  static host_api::HttpRequestResponseBase *handle(JSObject *obj);
  static host_api::HttpHeadersReadOnly *headers_handle(JSObject *obj);
  static bool has_body(JSObject *obj);
  static host_api::HttpIncomingBody *incoming_body_handle(JSObject *obj);
  static host_api::HttpOutgoingBody *outgoing_body_handle(JSObject *obj);
  static JSObject *body_stream(JSObject *obj);
  static JSObject *body_source(JSContext *cx, JS::HandleObject obj);
  static bool body_used(JSObject *obj);
  static bool mark_body_used(JSContext *cx, JS::HandleObject obj);
  static JS::Value url(JSObject *obj);
  static void set_url(JSObject *obj, JS::Value url);
  static bool body_unusable(JSContext *cx, JS::HandleObject body);
  static bool extract_body(JSContext *cx, JS::HandleObject self, JS::HandleValue body_val);

  /**
   * Returns the RequestOrResponse's Headers if it has been reified, nullptr if
   * not.
   */
  static JSObject *maybe_headers(JSObject *obj);

  /**
   * Returns a handle to a clone of the RequestOrResponse's Headers.
   *
   * The main purposes for this function are use in sending outgoing requests/responses and
   * in the constructor of request/response objects when a HeadersInit object is passed.
   *
   * The handle is guaranteed to be uniquely owned by the caller.
   */
  static unique_ptr<host_api::HttpHeaders> headers_handle_clone(JSContext *, HandleObject self,
                                                                host_api::HttpHeadersGuard guard);

  /**
   * Returns the RequestOrResponse's Headers, reifying it if necessary.
   */
  static JSObject *headers(JSContext *cx, JS::HandleObject obj);

  static bool append_body(JSContext *cx, JS::HandleObject self, JS::HandleObject source);

  using ParseBodyCB = bool(JSContext *cx, JS::HandleObject self, JS::UniqueChars buf, size_t len);

  enum class BodyReadResult {
    ArrayBuffer,
    JSON,
    Text,
  };

  template <BodyReadResult result_type>
  static bool parse_body(JSContext *cx, JS::HandleObject self, JS::UniqueChars buf, size_t len);

  static bool content_stream_read_then_handler(JSContext *cx, JS::HandleObject self,
                                               JS::HandleValue extra, JS::CallArgs args);
  static bool content_stream_read_catch_handler(JSContext *cx, JS::HandleObject self,
                                                JS::HandleValue extra, JS::CallArgs args);
  static bool consume_content_stream_for_bodyAll(JSContext *cx, JS::HandleObject self,
                                                 JS::HandleValue stream_val, JS::CallArgs args);
  template <RequestOrResponse::BodyReadResult result_type>
  static bool bodyAll(JSContext *cx, JS::CallArgs args, JS::HandleObject self);
  static bool body_source_cancel_algorithm(JSContext *cx, JS::CallArgs args,
                                           JS::HandleObject stream, JS::HandleObject owner,
                                           JS::HandleValue reason);
  static bool body_source_pull_algorithm(JSContext *cx, JS::CallArgs args, JS::HandleObject source,
                                         JS::HandleObject body_owner, JS::HandleObject controller);

  /**
   * Ensures that the given |body_owner|'s body is properly streamed, if it
   * requires streaming.
   *
   * If streaming is required, starts the process of reading from the
   * ReadableStream representing the body and sets the |requires_streaming| bool
   * to `true`.
   */
  static bool maybe_stream_body(JSContext *cx, JS::HandleObject body_owner,
                                bool *requires_streaming);

  static JSObject *create_body_stream(JSContext *cx, JS::HandleObject owner);

  static bool body_get(JSContext *cx, JS::CallArgs args, JS::HandleObject self,
                       bool create_if_undefined);
};

class Request final : public BuiltinImpl<Request> {
  static bool method_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool headers_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool url_get(JSContext *cx, unsigned argc, JS::Value *vp);

  template <RequestOrResponse::BodyReadResult result_type>
  static bool bodyAll(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool body_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool bodyUsed_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool clone(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "Request";

  enum class Slots {
    Request = static_cast<int>(RequestOrResponse::Slots::RequestOrResponse),
    BodyStream = static_cast<int>(RequestOrResponse::Slots::BodyStream),
    HasBody = static_cast<int>(RequestOrResponse::Slots::HasBody),
    BodyUsed = static_cast<int>(RequestOrResponse::Slots::BodyUsed),
    Headers = static_cast<int>(RequestOrResponse::Slots::Headers),
    URL = static_cast<int>(RequestOrResponse::Slots::URL),
    Method = static_cast<int>(RequestOrResponse::Slots::Count),
    ResponsePromise,
    PendingResponseHandle,
    Count,
  };

  static JSObject *response_promise(JSObject *obj);
  static JSString *method(JSContext *cx, JS::HandleObject obj);
  static host_api::HttpRequest *request_handle(JSObject *obj);
  static host_api::HttpOutgoingRequest *outgoing_handle(JSObject *obj);
  static host_api::HttpIncomingRequest *incoming_handle(JSObject *obj);

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);

  static JSObject *create(JSContext *cx, JS::HandleObject requestInstance);
  static JSObject *create(JSContext *cx, JS::HandleObject requestInstance, JS::HandleValue input,
                          JS::HandleValue init_val);

  static JSObject *create_instance(JSContext *cx);
};

class Response final : public BuiltinImpl<Response> {
  static bool waitUntil(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool ok_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool status_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool statusText_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool url_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool type_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool headers_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool redirected_get(JSContext *cx, unsigned argc, JS::Value *vp);

  template <RequestOrResponse::BodyReadResult result_type>
  static bool bodyAll(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool body_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool bodyUsed_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool redirect(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool json(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "Response";

  enum class Slots {
    Response = static_cast<int>(RequestOrResponse::Slots::RequestOrResponse),
    BodyStream = static_cast<int>(RequestOrResponse::Slots::BodyStream),
    HasBody = static_cast<int>(RequestOrResponse::Slots::HasBody),
    BodyUsed = static_cast<int>(RequestOrResponse::Slots::BodyUsed),
    Headers = static_cast<int>(RequestOrResponse::Slots::Headers),
    Status = static_cast<int>(RequestOrResponse::Slots::Count),
    StatusMessage,
    Redirected,
    Count,
  };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);

  static JSObject *create(JSContext *cx, JS::HandleObject response);
  static JSObject* create_incoming(JSContext * cx, HandleObject self,
                                                           host_api::HttpIncomingResponse* response);

  static host_api::HttpResponse *response_handle(JSObject *obj);
  static uint16_t status(JSObject *obj);
  static JSString *status_message(JSObject *obj);
  static void set_status_message_from_code(JSContext *cx, JSObject *obj, uint16_t code);
};

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

} // namespace fetch
} // namespace web
} // namespace builtins

#endif
