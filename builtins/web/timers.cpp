#include "timers.h"

#include "event_loop.h"

#include <ctime>
#include <host_api.h>
#include <iostream>
#include <list>
#include <map>
#include <vector>

#define S_TO_NS(s) ((s) * 1000000000)
#define NS_TO_MS(ns) ((ns) / 1000000)

static api::Engine *ENGINE;

class TimerTask final : public api::AsyncTask {
  using TimerArgumentsVector = std::vector<JS::Heap<JS::Value>>;

  static std::map<int32_t, api::AsyncTask*> timer_ids_;
  static int32_t next_timer_id;

  int32_t timer_id_;
  int64_t delay_;
  int64_t deadline_;
  bool repeat_;

  Heap<JSObject *> callback_;
  TimerArgumentsVector arguments_;

public:
  explicit TimerTask(const int64_t delay_ns, const bool repeat, HandleObject callback,
                     JS::HandleValueVector args)
      : delay_(delay_ns), repeat_(repeat), callback_(callback) {
    deadline_ = host_api::MonotonicClock::now() + delay_ns;

    arguments_.reserve(args.length());
    for (auto &arg : args) {
      arguments_.emplace_back(arg);
    }

    handle_ = host_api::MonotonicClock::subscribe(deadline_, true);
    timer_id_ = next_timer_id++;
    timer_ids_.emplace(timer_id_, this);
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    JSContext *cx = engine->cx();

    const RootedObject callback(cx, callback_);
    JS::RootedValueVector argv(cx);
    if (!argv.initCapacity(arguments_.size())) {
      JS_ReportOutOfMemory(cx);
      return false;
    }

    for (auto &arg : arguments_) {
      argv.infallibleAppend(arg);
    }

    RootedValue rval(cx);
    if (!Call(cx, NullHandleValue, callback, argv, &rval)) {
      return false;
    }

    // The task might've been canceled during the callback.
    if (handle_ != INVALID_POLLABLE_HANDLE) {
      host_api::MonotonicClock::unsubscribe(handle_);
    }

    if (timer_ids_.contains(timer_id_)) {
      if (repeat_) {
        deadline_ = host_api::MonotonicClock::now() + delay_;
        handle_ = host_api::MonotonicClock::subscribe(deadline_, true);
        engine->queue_async_task(this);
      } else {
        timer_ids_.erase(timer_id_);
      }
    }

    return true;
  }

  [[nodiscard]] bool cancel(api::Engine *engine) override {
    if (!timer_ids_.contains(timer_id_)) {
      return false;
    }

    host_api::MonotonicClock::unsubscribe(id());
    handle_ = -1;
    return true;
  }

  [[nodiscard]] uint64_t deadline() override {
    return deadline_;
  }

  [[nodiscard]] int32_t timer_id() const {
    return timer_id_;
  }

  void trace(JSTracer *trc) override {
    TraceEdge(trc, &callback_, "Timer callback");
    for (auto &arg : arguments_) {
      TraceEdge(trc, &arg, "Timer callback arguments");
    }
  }

  static bool clear(int32_t timer_id) {
    if (!timer_ids_.contains(timer_id)) {
      return false;
    }

    ENGINE->cancel_async_task(timer_ids_[timer_id]);
    timer_ids_.erase(timer_id);
    return true;
  }
};

std::map<int32_t, api::AsyncTask*> TimerTask::timer_ids_ = {};
int32_t TimerTask::next_timer_id = 1;

namespace builtins::web::timers {

/**
 * The `setTimeout` and `setInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval
 */
template <bool repeat> bool setTimeout_or_interval(JSContext *cx, const unsigned argc, Value *vp) {
  REQUEST_HANDLER_ONLY(repeat ? "setInterval" : "setTimeout");
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, repeat ? "setInterval" : "setTimeout", 1)) {
    return false;
  }

  if (!(args[0].isObject() && JS::IsCallable(&args[0].toObject()))) {
    JS_ReportErrorASCII(cx, "First argument to %s must be a function",
                        repeat ? "setInterval" : "setTimeout");
    return false;
  }
  const RootedObject handler(cx, &args[0].toObject());

  int32_t delay_ms = 0;
  if (args.length() > 1 && !JS::ToInt32(cx, args.get(1), &delay_ms)) {
    return false;
  }
  if (delay_ms < 0) {
    delay_ms = 0;
  }

  // Convert delay from milliseconds to nanoseconds, as that's what Timers operate on.
  const int64_t delay = static_cast<int64_t>(delay_ms) * 1000000;

  JS::RootedValueVector handler_args(cx);
  if (args.length() > 2 && !handler_args.initCapacity(args.length() - 2)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  for (size_t i = 2; i < args.length(); i++) {
    handler_args.infallibleAppend(args[i]);
  }

  const auto timer = new TimerTask(delay, repeat, handler, handler_args);
  ENGINE->queue_async_task(timer);
  args.rval().setInt32(timer->timer_id());

  return true;
}

/**
 * The `clearTimeout` and `clearInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-cleartimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-clearinterval
 */
template <bool interval> bool clearTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  const CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, interval ? "clearInterval" : "clearTimeout", 1)) {
    return false;
  }

  int32_t id = 0;
  if (!ToInt32(cx, args[0], &id)) {
    return false;
  }

  TimerTask::clear(id);

  args.rval().setUndefined();
  return true;
}

constexpr JSFunctionSpec methods[] = {
    JS_FN("setInterval", setTimeout_or_interval<true>, 1, JSPROP_ENUMERATE),
    JS_FN("setTimeout", setTimeout_or_interval<false>, 1, JSPROP_ENUMERATE),
    JS_FN("clearInterval", clearTimeout_or_interval<true>, 1, JSPROP_ENUMERATE),
    JS_FN("clearTimeout", clearTimeout_or_interval<false>, 1, JSPROP_ENUMERATE), JS_FS_END};

bool install(api::Engine *engine) {
  ENGINE = engine;
  return JS_DefineFunctions(engine->cx(), engine->global(), methods);
}

} // namespace builtins::web::timers
