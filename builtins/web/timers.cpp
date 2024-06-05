#include "timers.h"

#include "event_loop.h"

#include <ctime>
#include <host_api.h>
#include <iostream>
#include <list>
#include <vector>

#define S_TO_NS(s) ((s) * 1000000000)
#define NS_TO_MS(ns) ((ns) / 1000000)

static api::Engine *ENGINE;

class TimerTask final : public api::AsyncTask {
  using TimerArgumentsVector = std::vector<JS::Heap<JS::Value>>;
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

    if (repeat_) {
      engine->queue_async_task(new TimerTask(delay_, true, callback, argv));
    }

    return cancel(engine);
  }

  [[nodiscard]] bool cancel(api::Engine *engine) override {
    host_api::MonotonicClock::unsubscribe(id());
    handle_ = -1;
    return true;
  }

  [[nodiscard]] uint64_t deadline(api::Engine *engine) override {
    return deadline_;
  }

  void trace(JSTracer *trc) override {
    TraceEdge(trc, &callback_, "Timer callback");
    for (auto &arg : arguments_) {
      TraceEdge(trc, &arg, "Timer callback arguments");
    }
  }
};

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
  args.rval().setInt32(timer->id());

  return true;
}

/**
 * The `clearTimeout` and `clearInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-cleartimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-clearinterval
 */
template <bool interval> bool clearTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  REQUEST_HANDLER_ONLY(interval ? "clearInterval" : "clearTimeout");
  const CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, interval ? "clearInterval" : "clearTimeout", 1)) {
    return false;
  }

  int32_t id = 0;
  if (!ToInt32(cx, args[0], &id)) {
    return false;
  }

  ENGINE->cancel_async_task(id);

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
