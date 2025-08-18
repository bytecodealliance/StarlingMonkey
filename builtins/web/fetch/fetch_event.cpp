#include "fetch_event.h"
#include "encode.h"
#include "request-response.h"

#include "../dom-exception.h"
#include "../event/event-target.h"
#include "../event/global-event-target.h"
#include "../performance.h"
#include "../url.h"
#include "../worker-location.h"

#include <allocator.h>
#include <debugger.h>
#include <js/SourceText.h>

#include <iostream>
#include <memory>
#include <optional>

using builtins::web::event::Event;
using builtins::web::event::EventTarget;
using builtins::web::event::global_event_target;

using EventFlag = Event::EventFlag;

namespace builtins::web::fetch::fetch_event {

namespace {

api::Engine *ENGINE;
JSString *fetch_type_atom;

JS::PersistentRootedObject INSTANCE;
host_api::HttpOutgoingBody *STREAMING_BODY;

constexpr const std::string_view DEFAULT_NO_HANDLER_ERROR_MSG = "ERROR: no fetch-event handler triggered, was one registered?";

void inc_pending_promise_count(JSObject *self) {
  MOZ_ASSERT(FetchEvent::is_instance(self));
  auto count = JS::GetReservedSlot(self, FetchEvent::Slots::PendingPromiseCount).toInt32();
  count++;
  MOZ_ASSERT(count > 0);
  if (count == 1) {
    ENGINE->incr_event_loop_interest();
  }

  JS::SetReservedSlot(self, FetchEvent::Slots::PendingPromiseCount, JS::Int32Value(count));
}

void dec_pending_promise_count(JSObject *self) {
  MOZ_ASSERT(FetchEvent::is_instance(self));
  auto count = JS::GetReservedSlot(self, FetchEvent::Slots::PendingPromiseCount).toInt32();
  MOZ_ASSERT(count > 0);
  count--;
  if (count == 0) {
    ENGINE->decr_event_loop_interest();
  }
  JS::SetReservedSlot(self, FetchEvent::Slots::PendingPromiseCount, JS::Int32Value(count));
}

// Step 5 of https://w3c.github.io/ServiceWorker/#wait-until-method
bool dec_pending_promise_count(JSContext *cx, JS::HandleObject event, JS::HandleValue extra,
                               JS::CallArgs args) {
  // Step 5.1
  dec_pending_promise_count(event);

  // Note: step 5.2 not relevant to our implementation.
  return true;
}

/// Wrapper for `dec_pending_promise_count` that also logs the rejection reason.
///
/// Without this logging, it's very hard to even tell that something went wrong,
/// because the rejection is just silently ignored: the promise rejection tracker
/// doesn't ever see it, because adding it to `waitUntil` marks it as handled.
bool handle_wait_until_rejection(JSContext *cx, JS::HandleObject event, JS::HandleValue promiseVal,
                                 JS::CallArgs args) {
  fprintf(stderr, "Warning: Promise passed to FetchEvent#waitUntil was rejected with error. "
                  "Pending tasks after that error might not run. Error details:\n");
  RootedObject promise(cx, &promiseVal.toObject());
  ENGINE->dump_promise_rejection(args.get(0), promise, stderr);
  return dec_pending_promise_count(cx, event, promiseVal, args);
}

bool add_pending_promise(JSContext *cx, JS::HandleObject self, JS::HandleObject promise, bool for_waitUntil) {
  MOZ_ASSERT(FetchEvent::is_instance(self));
  MOZ_ASSERT(JS::IsPromiseObject(promise));

  JS::RootedObject resolve_handler(cx);
  resolve_handler = &GetReservedSlot(self,
    static_cast<uint32_t>(FetchEvent::Slots::DecPendingPromiseCountFunc)).toObject();

  JS::RootedObject reject_handler(cx);
  if (for_waitUntil) {
    RootedValue promiseVal(cx, JS::ObjectValue(*promise));
    reject_handler = create_internal_method<handle_wait_until_rejection>(cx, self, promiseVal);
  } else {
    reject_handler = resolve_handler;
  }
  if (reject_handler == nullptr) {
    return false;
}
  if (!JS::AddPromiseReactions(cx, promise, resolve_handler, reject_handler)) {
    return false;
}

  inc_pending_promise_count(self);
  return true;
}

} // namespace

JSObject *FetchEvent::prepare_downstream_request(JSContext *cx) {
  JS::RootedObject request(cx, Request::create(cx));
  if (request == nullptr) {
    return nullptr;
}
  Request::init_slots(request);
  return request;
}

bool FetchEvent::init_incoming_request(JSContext *cx, JS::HandleObject self,
                                       host_api::HttpIncomingRequest *req) {
  builtins::web::performance::Performance::timeOrigin.emplace(
      std::chrono::high_resolution_clock::now());
  JS::RootedObject request(
      cx, &JS::GetReservedSlot(self, static_cast<uint32_t>(Slots::Request)).toObject());

  MOZ_ASSERT(!RequestOrResponse::maybe_handle(request));
  JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::Request),
                      JS::PrivateValue(req));

  // Set the method.
  auto res = req->method();
  MOZ_ASSERT(!res.is_err(), "TODO: proper error handling");
  auto method_str = res.unwrap();
  bool is_get = method_str == "GET";
  bool is_head = !is_get && method_str == "HEAD";

  if (!is_get) {
    JS::RootedString method(cx, JS_NewStringCopyN(cx, &*method_str.cbegin(), method_str.length()));
    if (method == nullptr) {
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
  if (url == nullptr) {
    return false;
  }
  JS::SetReservedSlot(request, static_cast<uint32_t>(Request::Slots::URL), JS::StringValue(url));

  // Set the URL for `globalThis.location` to the client request's URL.
  JS::RootedObject url_instance(
      cx, JS_NewObjectWithGivenProto(cx, &url::URL::class_, url::URL::proto_obj));
  if (url_instance == nullptr) {
    return false;
  }

  auto *uri_bytes = new uint8_t[uri_str.size() + 1];
  std::copy(uri_str.begin(), uri_str.end(), uri_bytes);
  jsurl::SpecString spec(uri_bytes, uri_str.size(), uri_str.size());

  worker_location::WorkerLocation::url = url::URL::create(cx, url_instance, spec);
  return worker_location::WorkerLocation::url != nullptr;
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
  auto result = response->send();
  FetchEvent::set_state(self, new_state);

  if (const auto *err = result.to_err()) {
    HANDLE_ERROR(ENGINE->cx(), *err);
    return false;
  }

  return true;
}

bool start_response(JSContext *cx, JS::HandleObject response_obj) {
  auto status = Response::status(response_obj);
  auto headers = RequestOrResponse::headers_handle_clone(cx, response_obj);
  if (!headers) {
    return false;
  }

  host_api::HttpOutgoingResponse* response =
    host_api::HttpOutgoingResponse::make(status, std::move(headers));

  auto *existing_handle = Response::maybe_response_handle(response_obj);
  if (existing_handle != nullptr) {
    MOZ_ASSERT(existing_handle->is_incoming());
  } else {
    SetReservedSlot(response_obj, static_cast<uint32_t>(Response::Slots::Response),
                    PrivateValue(response));
  }

  bool streaming = false;
  if (!RequestOrResponse::maybe_stream_body(cx, response_obj, response, &streaming)) {
    return false;
  }

  if (streaming) {
    STREAMING_BODY = response->body().unwrap();
    FetchEvent::increase_interest();
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
    api::throw_error(cx, FetchErrors::InvalidRespondWithArg);
    JS::RootedObject rejection(cx, PromiseRejectedWithPendingError(cx));
    if (rejection == nullptr) {
      return false;
}
    args.rval().setObject(*rejection);
    return FetchEvent::respondWithError(cx, event);
  }

  // Step 10.2 (very roughly: the way we handle responses and their bodies is
  // very different.)
  JS::RootedObject response_obj(cx, &args[0].toObject());
  return start_response(cx, response_obj);
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
  if (response_promise == nullptr) {
    return false;
}

  // Step 2
  if (!Event::has_flag(self, EventFlag::Dispatch)) {
    return dom_exception::DOMException::raise(
        cx, "FetchEvent#respondWith must be called synchronously from within a FetchEvent handler",
        "InvalidStateError");
  }

  // Step 3
  if (state(self) != State::unhandled) {
    return dom_exception::DOMException::raise(
        cx, "FetchEvent#respondWith can't be called twice on the same event", "InvalidStateError");
  }

  // Step 4
  add_pending_promise(cx, self, response_promise, false);

  // Steps 5-7 (very roughly)
  set_state(self, State::waitToRespond);

  // Step 9 (continued in `response_promise_catch_handler` above)
  JS::RootedObject catch_handler(cx);
  JS::RootedValue extra(cx, JS::ObjectValue(*response_promise));
  catch_handler = create_internal_method<response_promise_catch_handler>(cx, self, extra);
  if (catch_handler == nullptr) {
    return false;
}

  // Step 10 (continued in `response_promise_then_handler` above)
  JS::RootedObject then_handler(cx);
  then_handler = create_internal_method<response_promise_then_handler>(cx, self);
  if (then_handler == nullptr) {
    return false;
}

  if (!JS::AddPromiseReactions(cx, response_promise, then_handler, catch_handler)) {
    return false;
}

  args.rval().setUndefined();
  return true;
}

  bool FetchEvent::respondWithError(JSContext *cx, JS::HandleObject self, std::optional<std::string_view> body_text) {
  MOZ_RELEASE_ASSERT(state(self) == State::unhandled || state(self) == State::waitToRespond);

  auto headers = std::make_unique<host_api::HttpHeaders>();
  if (body_text) {
    auto header_set_res = headers->set("content-type", "text/plain");
    if (auto *err = header_set_res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  }

  auto *response = host_api::HttpOutgoingResponse::make(500, std::move(headers));

  auto body_res = response->body();
  if (const auto *err = body_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  if (body_text) {
    auto body = std::move(body_res.unwrap());
    body->write(reinterpret_cast<const uint8_t*>(body_text->data()), body_text->length());
  }

  return send_response(response, self, FetchEvent::State::respondedWithError);
}

// Steps in this function refer to the spec at
// https://w3c.github.io/ServiceWorker/#wait-until-method
bool FetchEvent::waitUntil(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  JS::RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, args.get(0)));
  if (promise == nullptr) {
    return false;
}

  // Step 2
  if (!is_active(self)) {
    return dom_exception::DOMException::raise(
        cx, "waitUntil called on a FetchEvent that isn't active anymore", "InvalidStateError");
  }

  // Steps 3-4
  add_pending_promise(cx, self, promise, true);

  // Note: step 5 implemented in dec_pending_promise_count

  args.rval().setUndefined();
  return true;
}

void FetchEvent::increase_interest() { inc_pending_promise_count(INSTANCE); }

void FetchEvent::decrease_interest() { dec_pending_promise_count(INSTANCE); }

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
  if (self == nullptr) {
    return nullptr;
  }

  JS::RootedValue type(cx, JS::StringValue(fetch_type_atom));
  JS::RootedValue init(cx);
  if (!Event::init(cx, self, type, init)) {
    return nullptr;
  }

  JS::RootedObject request(cx, prepare_downstream_request(cx));
  if (request == nullptr) {
    return nullptr;
  }

  JS::RootedObject dec_count_handler(cx, create_internal_method<dec_pending_promise_count>(cx, self));
  if (dec_count_handler == nullptr) {
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::Request, JS::ObjectValue(*request));
  JS::SetReservedSlot(self, Slots::CurrentState, JS::Int32Value((int)State::unhandled));
  JS::SetReservedSlot(self, Slots::PendingPromiseCount, JS::Int32Value(0));
  JS::SetReservedSlot(self, Slots::DecPendingPromiseCountFunc, JS::ObjectValue(*dec_count_handler));

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
  return Event::has_flag(self, EventFlag::Dispatch) || state(self) == State::responseStreaming ||
         JS::GetReservedSlot(self, Slots::PendingPromiseCount).toInt32() > 0;
}

FetchEvent::State FetchEvent::state(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<FetchEvent::State>(JS::GetReservedSlot(self, Slots::CurrentState).toInt32());
}

void FetchEvent::set_state(JSObject *self, FetchEvent::State new_state) {
  MOZ_ASSERT(is_instance(self));
  auto current_state = state(self);
  MOZ_ASSERT((uint8_t)new_state > (uint8_t)current_state);
  JS::SetReservedSlot(self, Slots::CurrentState, JS::Int32Value(static_cast<int32_t>(new_state)));

  if (current_state == State::responseStreaming &&
    (new_state == State::responseDone || new_state == State::respondedWithError)) {
    if ((STREAMING_BODY != nullptr) && STREAMING_BODY->valid()) {
      STREAMING_BODY->close();
    }
    decrease_interest();
  }
}

bool FetchEvent::response_started(JSObject *self) {
  auto current_state = state(self);
  return current_state != State::unhandled && current_state != State::waitToRespond;
}

static void dispatch_fetch_event(HandleObject event, double *total_compute) {
  MOZ_ASSERT(FetchEvent::is_instance(event));

  RootedValue event_val(ENGINE->cx(), JS::ObjectValue(*event));
  RootedValue rval(ENGINE->cx());
  RootedObject event_target(ENGINE->cx(), global_event_target());
  MOZ_RELEASE_ASSERT(event_target);

  EventTarget::dispatch_event(ENGINE->cx(), event_target, event_val, &rval);
}

bool handle_incoming_request(host_api::HttpIncomingRequest *request) {
#ifdef DEBUG
  fprintf(stderr, "Warning: Using a DEBUG build. Expect things to be SLOW.\n");
#endif
  MOZ_ASSERT(ENGINE->state() == api::EngineState::Initialized);

  HandleObject fetch_event = FetchEvent::instance();
  MOZ_ASSERT(FetchEvent::is_instance(fetch_event));

  if (!FetchEvent::init_incoming_request(ENGINE->cx(), fetch_event, request)) {
    ENGINE->dump_pending_exception("initialization of FetchEvent");
    return false;
  }

  double total_compute = 0;

  content_debugger::maybe_init_debugger(ENGINE, true);
  dispatch_fetch_event(fetch_event, &total_compute);

  bool success = ENGINE->run_event_loop();

  if (JS_IsExceptionPending(ENGINE->cx())) {
    ENGINE->dump_pending_exception("evaluating incoming request");
  }

  if (!success) {
    fprintf(stderr, "Warning: JS event loop terminated without completing the request.\n");
  }

  if (ENGINE->debug_logging_enabled() && ENGINE->has_pending_async_tasks()) {
    fprintf(stderr, "Event loop terminated with async tasks pending. "
                    "Use FetchEvent#waitUntil to extend the component's "
                    "lifetime if needed.\n");
  }

  if (!FetchEvent::response_started(fetch_event)) {
    // If at this point no fetch event handler has run, we can
    // send a specific error indicating that there is likely no handler registered
    FetchEvent::respondWithError(ENGINE->cx(), fetch_event, DEFAULT_NO_HANDLER_ERROR_MSG);
    return true;
  }

  if ((STREAMING_BODY != nullptr) && STREAMING_BODY->valid()) {
    STREAMING_BODY->close();
  }

  if (ENGINE->has_unhandled_promise_rejections()) {
    fprintf(stderr, "Warning: Unhandled promise rejections detected after handling incoming request.\n");
    ENGINE->report_unhandled_promise_rejections();
  }

  return true;
}

bool FetchEvent::init_class(JSContext *cx, JS::HandleObject global) {
  Event::register_subclass(&class_);
  return init_class_impl(cx, global, Event::proto_obj) && JS_DeleteProperty(cx, global, class_.name);
}

bool install(api::Engine *engine) {
  ENGINE = engine;

  if ((fetch_type_atom = JS_AtomizeAndPinString(engine->cx(), "fetch")) == nullptr) {
    return false;
  }

  if (!FetchEvent::init_class(engine->cx(), engine->global())) {
    return false;
  }

  if (FetchEvent::create(engine->cx()) == nullptr) {
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

  host_api::HttpIncomingRequest::set_handler(handle_incoming_request);
  return true;
}

} // namespace builtins::web::fetch::fetch_event
