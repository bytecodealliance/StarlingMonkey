#include "event_loop.h"
#include "builtins/builtin.h"
#include "builtins/web/fetch/request-response.h"
#include "builtins/web/streams/native-stream-source.h"
#include "host_api.h"

#include <iostream>
#include <list>
#include <vector>

static core::AsyncTask *timeout_task;
static int64_t timeout = 0;
static JS::PersistentRootedObjectVector *pending_async_tasks;

namespace core {

namespace {

bool process_pending_request(JSContext *cx, HandleObject request) {

  RootedObject response_promise(
      cx, builtins::web::fetch::Request::response_promise(request));


  auto pending = builtins::web::fetch::Request::pending_handle(request);

  auto res = pending->poll();
  if (auto *err = res.to_err()) {
    JS_ReportErrorUTF8(cx, "NetworkError when attempting to fetch resource.");
    return RejectPromiseWithPendingError(cx, response_promise);
  }


  auto maybe_response = res.unwrap();
  MOZ_ASSERT(maybe_response.has_value());
  auto response = maybe_response.value();
  RootedObject response_obj(cx, JS_NewObjectWithGivenProto(cx, &builtins::web::fetch::Response::class_,
                                                                builtins::web::fetch::Response::proto_obj));
  if (!response_obj) {
    return false;
  }

  response_obj = builtins::web::fetch::Response::create(cx, response_obj, response);
  if (!response_obj) {
    return false;
  }

  builtins::web::fetch::RequestOrResponse::set_url(
      response_obj, builtins::web::fetch::RequestOrResponse::url(request));
  RootedValue response_val(cx, ObjectValue(*response_obj));
  return ResolvePromise(cx, response_promise, response_val);
}

// TODO(TS): move this into the fetch or streams impl
bool error_stream_controller_with_pending_exception(
    JSContext *cx, HandleObject controller) {
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

bool process_body_read(JSContext *cx, HandleObject streamSource) {
  RootedObject owner(cx,
      builtins::web::streams::NativeStreamSource::owner(streamSource));
  RootedObject controller( cx,
      builtins::web::streams::NativeStreamSource::controller(streamSource));
  auto body =
      builtins::web::fetch::RequestOrResponse::incoming_body_handle(owner);

  auto read_res = body->read(HANDLE_READ_CHUNK_SIZE, true);
  if (auto *err = read_res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return error_stream_controller_with_pending_exception(cx, controller);
  }

  auto &chunk = read_res.unwrap();
  if (std::get<1>(chunk)) {
    RootedValue r(cx);
    return Call(cx, controller, "close", HandleValueArray::empty(), &r);
  }

  // We don't release control of chunk's data until after we've checked that
  // the array buffer allocation has been successful, as that ensures that the
  // return path frees chunk automatically when necessary.
  auto &bytes = std::get<0>(chunk);
  RootedObject buffer(cx, JS::NewArrayBufferWithContents(cx, bytes.len, bytes.ptr.get()));
  if (!buffer) {
    return error_stream_controller_with_pending_exception(cx, controller);
  }

  // At this point `buffer` has taken full ownership of the chunk's data.
  std::ignore = bytes.ptr.release();

  RootedObject byte_array(
      cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, bytes.len));
  if (!byte_array) {
    return false;
  }

  RootedValueArray<1> enqueue_args(cx);
  enqueue_args[0].setObject(*byte_array);
  RootedValue r(cx);
  if (!JS::Call(cx, controller, "enqueue", enqueue_args, &r)) {
    return error_stream_controller_with_pending_exception(cx, controller);
  }

  return true;
}

} // namespace

bool EventLoop::process_pending_async_tasks(JSContext *cx) {
  MOZ_ASSERT(has_pending_async_tasks());

  // Taking a timestamp once here is precise enough, even though we might spend
  // some time processing things before using it as part of deadline
  // calculations for async tasks.
  timespec current_time{};
  clock_gettime(CLOCK_MONOTONIC, &current_time);
  const int64_t now = current_time.tv_sec * 1000000000 + current_time.tv_nsec;

  int64_t delay = 0;
  if (timeout_task) {
    delay = timeout - now;

    // If a delay is already overdue, run it immediately and return.
    if (delay <= 0) {
      return timeout_task->run();
    }
  }

  size_t count = pending_async_tasks->length();
  std::vector<host_api::AsyncHandle> handles;

  for (size_t i = 0; i < count; i++) {
    HandleObject pending_obj = (*pending_async_tasks)[i];
    if (builtins::web::fetch::Request::is_instance(pending_obj)) {
      handles.push_back(
          builtins::web::fetch::Request::pending_handle(pending_obj)
              ->async_handle());
    } else {
      RootedObject owner(cx,
                             builtins::web::streams::NativeStreamSource::owner(pending_obj));
      handles.push_back(
          builtins::web::fetch::RequestOrResponse::incoming_body_handle(owner)->async_handle());
    }
  }

  // TODO: WASI's poll_oneoff is infallible, so once legacy CAE support isn't
  // required anymore this should be simplified.
  auto res = host_api::AsyncHandle::select(handles, delay);
  if (res.is_err()) {
    auto *err = res.to_err();
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto ret = res.unwrap();
  if (!ret.has_value()) {
    MOZ_ASSERT(timeout_task);
    return timeout_task->run();
  }

    // At this point we know that handles is not empty, and that ready_index is
    // valid, both because the delay wasn't reached. If handles was empty and
    // delay was zero, we would have errored out after the call to `select`. If
    // the delay was non-zero and handles was empty, the delay would expire
    // and we would exit through the path that runs the first timer.
    auto ready_index = ret.value();

  #if defined(DEBUG) && defined(CAE)
    auto ready_handle = handles[ready_index];
    auto is_ready = ready_handle.is_ready();
    MOZ_ASSERT(!is_ready.is_err());
    MOZ_ASSERT(is_ready.unwrap());
  #endif

  bool ok;
  HandleObject ready_obj = (*pending_async_tasks)[ready_index];
  if (builtins::web::fetch::Request::is_instance(ready_obj)) {
    ok = process_pending_request(cx, ready_obj);
  } else {
    ok = process_body_read(cx, ready_obj);
  }

  pending_async_tasks->erase(const_cast<JSObject **>(ready_obj.address()));
  return ok;
}

bool EventLoop::queue_async_task(HandleObject task) {
  return pending_async_tasks->append(task);
}

void EventLoop::set_timeout_task(AsyncTask *task, const int64_t timeout) {
  timeout_task = task;
  ::timeout = timeout;
}

void EventLoop::remove_timeout_task() {
  timeout_task = nullptr;
}

bool EventLoop::has_pending_async_tasks() {
  return !pending_async_tasks->empty() || timeout_task;
}

void EventLoop::init(JSContext *cx) {
  pending_async_tasks = new JS::PersistentRootedObjectVector(cx);
}

} // namespace core
