#include "fetch_event.h"
#include "../url.h"
#include "../worker-location.h"
#include "encode.h"
#include "exports.h"
#include "request-response.h"

#include "bindings.h"
#include <iostream>
#include <memory>

using namespace std::literals::string_view_literals;

namespace builtins::web::fetch::fetch_event {

namespace {

api::Engine *ENGINE;

PersistentRooted<JSObject *> INSTANCE;
JS::PersistentRootedObjectVector *FETCH_HANDLERS;

host_api::HttpOutgoingResponse::ResponseOutparam RESPONSE_OUT;
host_api::HttpOutgoingBody *STREAMING_BODY;

void inc_pending_promise_count(JSObject *self) {
  MOZ_ASSERT(FetchEvent::is_instance(self));
  auto count =
      JS::GetReservedSlot(self, static_cast<uint32_t>(FetchEvent::Slots::PendingPromiseCount))
          .toInt32();
  count++;
  MOZ_ASSERT(count > 0);
  JS::SetReservedSlot(self, static_cast<uint32_t>(FetchEvent::Slots::PendingPromiseCount),
                      JS::Int32Value(count));
}

void dec_pending_promise_count(JSObject *self) {
  MOZ_ASSERT(FetchEvent::is_instance(self));
  auto count =
      JS::GetReservedSlot(self, static_cast<uint32_t>(FetchEvent::Slots::PendingPromiseCount))
          .toInt32();
  MOZ_ASSERT(count > 0);
  count--;
  JS::SetReservedSlot(self, static_cast<uint32_t>(FetchEvent::Slots::PendingPromiseCount),
                      JS::Int32Value(count));
}

bool add_pending_promise(JSContext *cx, JS::HandleObject self, JS::HandleObject promise) {
  MOZ_ASSERT(FetchEvent::is_instance(self));
  MOZ_ASSERT(JS::IsPromiseObject(promise));

  JS::RootedObject handler(cx);
  handler = &JS::GetReservedSlot(
                 self, static_cast<uint32_t>(FetchEvent::Slots::DecPendingPromiseCountFunc))
                 .toObject();
  if (!JS::AddPromiseReactions(cx, promise, handler, handler))
    return false;

  inc_pending_promise_count(self);
  return true;
}

} // namespace

JSObject *FetchEvent::prepare_downstream_request(JSContext *cx) {
  JS::RootedObject requestInstance(
      cx, JS_NewObjectWithGivenProto(cx, &Request::class_, Request::proto_obj));
  if (!requestInstance)
    return nullptr;
  return Request::create(cx, requestInstance, nullptr);
}

bool FetchEvent::init_incoming_request(JSContext *cx, JS::HandleObject self,
                                       host_api::HttpIncomingRequest *req) {
  JS::RootedObject request(
      cx, &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Request)).toObject());

  MOZ_ASSERT(!Request::request_handle(request));

  JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::Request),
                      JS::PrivateValue(req));

  // Set the method.
  auto res = req->method();
  MOZ_ASSERT(!res.is_err(), "TODO: proper error handling");
  auto method_str = res.unwrap();
  bool is_get = method_str == "GET";
  bool is_head = !is_get && method_str == "HEAD";

  if (!is_get) {
    JS::RootedString method(cx, JS_NewStringCopyN(cx, method_str.cbegin(), method_str.length()));
    if (!method) {
      return false;
    }

    JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::Method),
                        JS::StringValue(method));
  }

  // Set whether we have a body depending on the method.
  // TODO: verify if that's right. I.e. whether we should treat all requests
  // that are not GET or HEAD as having a body, which might just be 0-length.
  // It's not entirely clear what else we even could do here though.
  if (!is_get && !is_head) {
    JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::HasBody), JS::TrueValue());
  }

  auto uri_str = req->url();
  JS::RootedString url(cx, JS_NewStringCopyN(cx, uri_str.data(), uri_str.size()));
  if (!url) {
    return false;
  }
  JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::URL), JS::StringValue(url));

  // Set the URL for `globalThis.location` to the client request's URL.
  JS::RootedObject url_instance(
      cx, JS_NewObjectWithGivenProto(cx, &url::URL::class_, url::URL::proto_obj));
  if (!url_instance) {
    return false;
  }

  auto uri_bytes = new uint8_t[uri_str.size() + 1];
  std::copy(uri_str.begin(), uri_str.end(), uri_bytes);
  jsurl::SpecString spec(uri_bytes, uri_str.size(), uri_str.size());

  worker_location::WorkerLocation::url = url::URL::create(cx, url_instance, spec);
  if (!worker_location::WorkerLocation::url) {
    return false;
  }

  return true;
}

bool FetchEvent::request_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  args.rval().set(JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Request)));
  return true;
}

namespace {

bool send_response(host_api::HttpOutgoingResponse *response, JS::HandleObject self,
                   FetchEvent::State new_state) {
  MOZ_ASSERT(FetchEvent::state(self) == FetchEvent::State::unhandled ||
             FetchEvent::state(self) == FetchEvent::State::waitToRespond);
  auto result = response->send(RESPONSE_OUT);
  FetchEvent::set_state(self, new_state);

  if (auto *err = result.to_err()) {
    HANDLE_ERROR(ENGINE->cx(), *err);
    return false;
  }

  return true;
}

bool start_response(JSContext *cx, JS::HandleObject response_obj, bool streaming) {
  auto generic_response = Response::response_handle(response_obj);
  host_api::HttpOutgoingResponse* response;

  if (generic_response->is_incoming()) {
    auto incoming_response = static_cast<host_api::HttpIncomingResponse *>(generic_response);
    auto status = incoming_response->status();
    MOZ_RELEASE_ASSERT(!status.is_err(), "Incoming response must have a status code");
    auto headers = new host_api::HttpHeaders(*incoming_response->headers().unwrap());
    response = host_api::HttpOutgoingResponse::make(status.unwrap(), headers);
    auto *source_body = incoming_response->body().unwrap();
    auto *dest_body = response->body().unwrap();

    auto res = dest_body->append(ENGINE, source_body);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
    MOZ_RELEASE_ASSERT(RequestOrResponse::mark_body_used(cx, response_obj));
  } else {
    response = static_cast<host_api::HttpOutgoingResponse *>(generic_response);
  }

  if (streaming && response->has_body()) {
    STREAMING_BODY = response->body().unwrap();
  }

  return send_response(response, FetchEvent::instance(),
                       streaming ? FetchEvent::State::responseStreaming
                                 : FetchEvent::State::responseDone);
}

// Steps in this function refer to the spec at
// https://w3c.github.io/ServiceWorker/#fetch-event-respondwith
bool response_promise_then_handler(JSContext *cx, JS::HandleObject event, JS::HandleValue extra,
                                   JS::CallArgs args) {
  // Step 10.1
  // Note: the `then` handler is only invoked after all Promise resolution has
  // happened. (Even if there were multiple Promises to unwrap first.) That
  // means that at this point we're guaranteed to have the final value instead
  // of a Promise wrapping it, so either the value is a Response, or we have to
  // bail.
  if (!Response::is_instance(args.get(0))) {
    JS_ReportErrorUTF8(cx, "FetchEvent#respondWith must be called with a Response "
                           "object or a Promise resolving to a Response object as "
                           "the first argument");
    JS::RootedObject rejection(cx, PromiseRejectedWithPendingError(cx));
    if (!rejection)
      return false;
    args.rval().setObject(*rejection);
    return FetchEvent::respondWithError(cx, event);
  }

  // Step 10.2 (very roughly: the way we handle responses and their bodies is
  // very different.)
  JS::RootedObject response_obj(cx, &args[0].toObject());

  // Ensure that all headers are stored client-side, so we retain access to them
  // after sending the response off.
  // TODO(TS): restore proper headers handling
  // if (Response::is_upstream(response_obj)) {
  //   JS::RootedObject headers(cx);
  //   headers =
  //       RequestOrResponse::headers<Headers::Mode::ProxyToResponse>(cx, response_obj);
  //   if (!Headers::delazify(cx, headers))
  //     return false;
  // }

  bool streaming = false;
  if (!RequestOrResponse::maybe_stream_body(cx, response_obj, &streaming)) {
    return false;
  }

  return start_response(cx, response_obj, streaming);
}

// Steps in this function refer to the spec at
// https://w3c.github.io/ServiceWorker/#fetch-event-respondwith
bool response_promise_catch_handler(JSContext *cx, JS::HandleObject event,
                                    JS::HandleValue promise_val, JS::CallArgs args) {
  JS::RootedObject promise(cx, &promise_val.toObject());

  fprintf(stderr, "Error while running request handler: ");
  ENGINE->dump_promise_rejection(args.get(0), promise, stderr);

  // TODO: verify that this is the right behavior.
  // Steps 9.1-2
  return FetchEvent::respondWithError(cx, event);
}

} // namespace

// Steps in this function refer to the spec at
// https://w3c.github.io/ServiceWorker/#fetch-event-respondwith
bool FetchEvent::respondWith(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  // Coercion of argument `r` to a Promise<Response>
  JS::RootedObject response_promise(cx, JS::CallOriginalPromiseResolve(cx, args.get(0)));
  if (!response_promise)
    return false;

  // Step 2
  if (!is_dispatching(self)) {
    JS_ReportErrorUTF8(cx, "FetchEvent#respondWith must be called synchronously from "
                           "within a FetchEvent handler");
    return false;
  }

  // Step 3
  if (state(self) != State::unhandled) {
    JS_ReportErrorUTF8(cx, "FetchEvent#respondWith can't be called twice on the same event");
    return false;
  }

  // Step 4
  add_pending_promise(cx, self, response_promise);

  // Steps 5-7 (very roughly)
  set_state(self, State::waitToRespond);

  // Step 9 (continued in `response_promise_catch_handler` above)
  JS::RootedObject catch_handler(cx);
  JS::RootedValue extra(cx, JS::ObjectValue(*response_promise));
  catch_handler = create_internal_method<response_promise_catch_handler>(cx, self, extra);
  if (!catch_handler)
    return false;

  // Step 10 (continued in `response_promise_then_handler` above)
  JS::RootedObject then_handler(cx);
  then_handler = create_internal_method<response_promise_then_handler>(cx, self);
  if (!then_handler)
    return false;

  if (!JS::AddPromiseReactions(cx, response_promise, then_handler, catch_handler))
    return false;

  args.rval().setUndefined();
  return true;
}

bool FetchEvent::respondWithError(JSContext *cx, JS::HandleObject self) {
  MOZ_RELEASE_ASSERT(state(self) == State::unhandled || state(self) == State::waitToRespond);

  auto headers = std::make_unique<host_api::HttpHeaders>();
  auto *response = host_api::HttpOutgoingResponse::make(500, headers.get());

  auto body_res = response->body();
  if (auto *err = body_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  return send_response(response, self, FetchEvent::State::respondedWithError);
}

namespace {

// Step 5 of https://w3c.github.io/ServiceWorker/#wait-until-method
bool dec_pending_promise_count(JSContext *cx, JS::HandleObject event, JS::HandleValue extra,
                               JS::CallArgs args) {
  // Step 5.1
  dec_pending_promise_count(event);

  // Note: step 5.2 not relevant to our implementation.
  return true;
}

} // namespace

// Steps in this function refer to the spec at
// https://w3c.github.io/ServiceWorker/#wait-until-method
bool FetchEvent::waitUntil(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  JS::RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, args.get(0)));
  if (!promise)
    return false;

  // Step 2
  if (!is_active(self)) {
    JS_ReportErrorUTF8(cx, "FetchEvent#waitUntil called on inactive event");
    return false;
  }

  // Steps 3-4
  add_pending_promise(cx, self, promise);

  // Note: step 5 implemented in dec_pending_promise_count

  args.rval().setUndefined();
  return true;
}

const JSFunctionSpec FetchEvent::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec FetchEvent::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec FetchEvent::methods[] = {
    JS_FN("respondWith", respondWith, 1, JSPROP_ENUMERATE),
    JS_FN("waitUntil", waitUntil, 1, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec FetchEvent::properties[] = {
    JS_PSG("request", request_get, JSPROP_ENUMERATE),
    JS_PS_END,
};

JSObject *FetchEvent::create(JSContext *cx) {
  JS::RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self)
    return nullptr;

  JS::RootedObject request(cx, prepare_downstream_request(cx));
  if (!request)
    return nullptr;

  JS::RootedObject dec_count_handler(cx,
                                     create_internal_method<dec_pending_promise_count>(cx, self));
  if (!dec_count_handler)
    return nullptr;

  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::Request), JS::ObjectValue(*request));
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::Dispatch), JS::FalseValue());
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::State),
                      JS::Int32Value((int)State::unhandled));
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::PendingPromiseCount), JS::Int32Value(0));
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::DecPendingPromiseCountFunc),
                      JS::ObjectValue(*dec_count_handler));

  INSTANCE.init(cx, self);
  self = INSTANCE;
  return self;
}

JS::HandleObject FetchEvent::instance() {
  MOZ_ASSERT(INSTANCE);
  MOZ_ASSERT(is_instance(INSTANCE));
  return INSTANCE;
}

bool FetchEvent::is_active(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  // Note: we also treat the FetchEvent as active if it's in `responseStreaming`
  // state because that requires us to extend the service's lifetime as well. In
  // the spec this is achieved using individual promise counts for the body read
  // operations.
  return JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Dispatch)).toBoolean() ||
         state(self) == State::responseStreaming ||
         JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::PendingPromiseCount)).toInt32() > 0;
}

bool FetchEvent::is_dispatching(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Dispatch)).toBoolean();
}

void FetchEvent::start_dispatching(JSObject *self) {
  MOZ_ASSERT(!is_dispatching(self));
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::Dispatch), JS::TrueValue());
}

void FetchEvent::stop_dispatching(JSObject *self) {
  MOZ_ASSERT(is_dispatching(self));
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::Dispatch), JS::FalseValue());
}

FetchEvent::State FetchEvent::state(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<State>(
      JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::State)).toInt32());
}

void FetchEvent::set_state(JSObject *self, State new_state) {
  MOZ_ASSERT(is_instance(self));
  MOZ_ASSERT((uint8_t)new_state > (uint8_t)state(self));
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::State),
                      JS::Int32Value(static_cast<int32_t>(new_state)));
}

bool FetchEvent::response_started(JSObject *self) {
  auto current_state = state(self);
  return current_state != State::unhandled && current_state != State::waitToRespond;
}

static bool addEventListener(JSContext *cx, unsigned argc, Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "addEventListener", 2)) {
    return false;
  }

  auto event_chars = core::encode(cx, args[0]);
  if (!event_chars) {
    return false;
  }

  if (strncmp(event_chars.begin(), "fetch", event_chars.len)) {
    fprintf(stderr,
            "Error: addEventListener only supports the event 'fetch' right now, "
            "but got event '%s'\n",
            event_chars.begin());
    exit(1);
  }

  RootedValue val(cx, args[1]);
  if (!val.isObject() || !JS_ObjectIsFunction(&val.toObject())) {
    fprintf(stderr, "Error: addEventListener: Argument 2 is not a function.\n");
    exit(1);
  }

  return FETCH_HANDLERS->append(&val.toObject());
}

static void dispatch_fetch_event(HandleObject event, double *total_compute) {
  MOZ_ASSERT(FetchEvent::is_instance(event));
  // auto pre_handler = system_clock::now();

  RootedValue result(ENGINE->cx());
  RootedValue event_val(ENGINE->cx(), JS::ObjectValue(*event));
  HandleValueArray argsv = HandleValueArray(event_val);
  RootedValue handler(ENGINE->cx());
  RootedValue rval(ENGINE->cx());

  FetchEvent::start_dispatching(event);

  for (size_t i = 0; i < FETCH_HANDLERS->length(); i++) {
    handler.setObject(*(*FETCH_HANDLERS)[i]);
    if (!JS_CallFunctionValue(ENGINE->cx(), ENGINE->global(), handler, argsv, &rval)) {
      ENGINE->dump_pending_exception("dispatching FetchEvent\n");
      break;
    }
    if (FetchEvent::state(event) != FetchEvent::State::unhandled) {
      break;
    }
  }

  FetchEvent::stop_dispatching(event);

  // double diff =
  //     duration_cast<microseconds>(system_clock::now() - pre_handler).count();
  // *total_compute += diff;
  // LOG("Request handler took %fms\n", diff / 1000);
}

bool install(api::Engine *engine) {
  ENGINE = engine;
  FETCH_HANDLERS = new JS::PersistentRootedObjectVector(engine->cx());

  if (!JS_DefineFunction(engine->cx(), engine->global(), "addEventListener", addEventListener, 2,
                         0)) {
    MOZ_RELEASE_ASSERT(false);
  }

  if (!FetchEvent::init_class(engine->cx(), engine->global()))
    return false;

  if (!FetchEvent::create(engine->cx())) {
    MOZ_RELEASE_ASSERT(false);
  }

  // TODO(TS): restore validation
  // if (FETCH_HANDLERS->length() == 0) {
  //   RootedValue val(engine->cx());
  //   if (!JS_GetProperty(engine->cx(), engine->global(), "onfetch", &val) || !val.isObject()
  //       || !JS_ObjectIsFunction(&val.toObject())) {
  //     // The error message only mentions `addEventListener`, even though we also
  //     // support an `onfetch` top-level function as an alternative. We're
  //     // treating the latter as undocumented functionality for the time being.
  //     fprintf(
  //         stderr,
  //         "Error: no `fetch` event handler registered during initialization. "
  //         "Make sure to call `addEventListener('fetch', your_handler)`.\n");
  //     exit(1);
  //   }
  //   if (!FETCH_HANDLERS->append(&val.toObject())) {
  //     engine->abort("Adding onfetch as a fetch event handler");
  //   }
  // }

  return true;
}

} // namespace builtins::web::fetch::fetch_event

// #define S_TO_NS(s) ((s) * 1000000000)
// static int64_t now_ns() {
//   timespec now{};
//   clock_gettime(CLOCK_MONOTONIC, &now);
//   return S_TO_NS(now.tv_sec) + now.tv_nsec;
// }
using namespace builtins::web::fetch::fetch_event;
// TODO: change this to fully work in terms of host_api.
void exports_wasi_http_incoming_handler(exports_wasi_http_incoming_request request_handle,
                                        exports_wasi_http_response_outparam response_out) {

  // auto begin = now_ns();
  // auto id1 = host_api::MonotonicClock::subscribe(begin + 1, true);
  // auto id2 = host_api::MonotonicClock::subscribe(begin + 1000000*1000, true);
  // bindings_borrow_pollable_t handles[2] = {bindings_borrow_pollable_t{id2},
  // bindings_borrow_pollable_t{id1}}; auto list = bindings_list_borrow_pollable_t{handles, 2};
  // bindings_list_u32_t res = {.ptr = nullptr,.len = 0};
  // wasi_io_0_2_0_rc_2023_10_18_poll_poll_list(&list, &res);
  // fprintf(stderr, "first ready after first poll: %d. diff: %lld\n", handles[res.ptr[0]].__handle,
  // (now_ns() - begin) / 1000);
  //
  // wasi_io_0_2_0_rc_2023_10_18_poll_pollable_drop_own(bindings_own_pollable_t{id1});
  //
  // bindings_borrow_pollable_t handles2[1] = {bindings_borrow_pollable_t{id2}};
  // list = bindings_list_borrow_pollable_t{handles2, 1};
  // wasi_io_0_2_0_rc_2023_10_18_poll_poll_list(&list, &res);
  // fprintf(stderr, "first ready after second poll: %d. diff: %lld\n",
  // handles2[res.ptr[0]].__handle, (now_ns() - begin) / 1000);
  //
  // return;

  RESPONSE_OUT = response_out.__handle;

  auto *request = new host_api::HttpIncomingRequest(request_handle.__handle);
  HandleObject fetch_event = FetchEvent::instance();
  MOZ_ASSERT(FetchEvent::is_instance(fetch_event));
  if (!FetchEvent::init_incoming_request(ENGINE->cx(), fetch_event, request)) {
    ENGINE->dump_pending_exception("initialization of FetchEvent");
    return;
  }

  double total_compute = 0;

  dispatch_fetch_event(fetch_event, &total_compute);

  RootedValue result(ENGINE->cx());
  if (!ENGINE->run_event_loop(&result)) {
    fflush(stdout);
    fprintf(stderr, "Error running event loop: ");
    ENGINE->dump_value(result, stderr);
    return;
  }

  if (JS_IsExceptionPending(ENGINE->cx())) {
    ENGINE->dump_pending_exception("Error evaluating code: ");
  }

  if (ENGINE->debug_logging_enabled() && ENGINE->has_pending_async_tasks()) {
    fprintf(stderr, "Event loop terminated with async tasks pending. "
                    "Use FetchEvent#waitUntil to extend the component's "
                    "lifetime if needed.\n");
  }

  if (!FetchEvent::response_started(fetch_event)) {
    FetchEvent::respondWithError(ENGINE->cx(), fetch_event);
    return;
  }

  if (STREAMING_BODY && STREAMING_BODY->valid()) {
    STREAMING_BODY->close();
  }
}
