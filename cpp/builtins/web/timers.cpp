#include "timers.h"

#include "event_loop.h"

#include <ctime>
#include <iostream>
#include <list>
#include <vector>

namespace builtins::web::timers {

using TimerArgumentsVector = std::vector<JS::Heap<JS::Value>>;

class TimersTask final : public core::AsyncTask {
private:
  static TimersTask *instance;
  core::Engine *engine;

  explicit TimersTask(core::Engine* engine) : engine(engine) {}

public:
  static void init(core::Engine *engine) {
    MOZ_ASSERT(!instance);
    instance = new TimersTask(engine);
  }
  static TimersTask *get() { return instance; }

  void enqueue() override;
  bool run() override;
};

TimersTask *TimersTask::instance = nullptr;

#define NS_TO_MS(ns) ((ns) / 1000000)
#define S_TO_NS(s) ((s) * 1000000000)

class Timer {
public:
  /// Returns the monotonic clock's current time in nanoseconds.
  static int64_t now() {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return S_TO_NS(now.tv_sec) + now.tv_nsec;
  }

  uint32_t id;
  JS::Heap<JSObject *> callback;
  TimerArgumentsVector arguments;
  int64_t delay;
  int64_t deadline;
  bool repeat;

  Timer(const uint32_t id, const HandleObject callback, const int64_t delay,
        JS::HandleValueVector args, const bool repeat)
      : id(id), callback(callback), delay(delay), repeat(repeat) {
    const auto now = Timer::now();
    deadline = now + delay;

    arguments.reserve(args.length());
    for (auto &arg : args) {
      arguments.emplace_back(arg);
    }
  }

  void trace(JSTracer *trc) {
    TraceEdge(trc, &callback, "Timer callback");
    for (auto &arg : arguments) {
      TraceEdge(trc, &arg, "Timer callback arguments");
    }
  }
};

class ScheduledTimers {
private:
  static PersistentRooted<js::UniquePtr<ScheduledTimers>> instance;
  static core::Engine* engine;

  std::list<std::unique_ptr<Timer>> timers;
  int64_t first_deadline = 0;

public:
  static void init(core::Engine* engine) {
    instance.init(engine->cx(), js::MakeUnique<ScheduledTimers>());
    ScheduledTimers::engine = engine;
  }

private:
  [[nodiscard]] Timer *first() const {
    if (std::empty(timers)) {
      return nullptr;
    } else {
      return timers.front().get();
    }
  }

  // `repeat_first` must only be called if the `timers` list is not empty
  // The caller of repeat_first needs to check the `timers` list is not empty
  void repeat_first() {
    MOZ_ASSERT(!this->timers.empty());
    auto timer = std::move(this->timers.front());
    timers.pop_front();
    timer->deadline = Timer::now() + timer->delay;
    add_timer(std::move(timer));
  }

  void add_timer(std::unique_ptr<Timer> timer) {
    auto iter = timers.begin();

    for (; iter != timers.end(); iter++) {
      if ((*iter)->deadline > timer->deadline) {
        break;
      }
    }

    timers.insert(iter, std::move(timer));

    if (first()->deadline != first_deadline) {
      first_deadline = first()->deadline;
      engine->set_timeout_task(TimersTask::get(), first_deadline);
    }
  }

  void remove_timer(uint32_t id) {
    auto it = std::find_if(timers.begin(), timers.end(),
                           [id](auto &timer) { return timer->id == id; });
    if (it == timers.end()) {
      return;
    }

    timers.erase(it);

    if (std::empty(timers)) {
      MOZ_ASSERT(first_deadline != 0);
      first_deadline = 0;
      engine->remove_timeout_task();
    } else if (first()->deadline != first_deadline) {
      first_deadline = first()->deadline;
      engine->set_timeout_task(TimersTask::get(), first_deadline);
    }
  }

  bool run_first_timer(JSContext *cx) {
    RootedValue fun_val(cx);
    JS::RootedValueVector argv(cx);
    uint32_t id;
    {
      auto *timer = first();
      MOZ_ASSERT(timer);
#ifdef DEBUG
      const auto now = Timer::now();
      const auto diff = now - NS_TO_MS(timer->deadline);
#endif
      MOZ_ASSERT(diff >= 0);
      id = timer->id;
      RootedObject fun(cx, timer->callback);
      fun_val.setObject(*fun.get());
      if (!argv.initCapacity(timer->arguments.size())) {
        JS_ReportOutOfMemory(cx);
        return false;
      }

      for (auto &arg : timer->arguments) {
        argv.infallibleAppend(arg);
      }
    }

    RootedObject fun(cx, &fun_val.toObject());

    RootedValue rval(cx);
    if (!Call(cx, NullHandleValue, fun, argv, &rval)) {
      return false;
    }

    // Repeat / remove the first timer if it's still the one we just ran.
    if (auto *timer = first(); timer && timer->id == id) {
      if (timer->repeat) {
        repeat_first();
      } else {
        remove_timer(timer->id);
      }
    }

    return true;
  }

public:
  static uint32_t add(HandleObject callback, int64_t delay,
                      JS::HandleValueVector arguments, bool repeat) {
    static uint32_t next_timer_id = 1;

    auto id = next_timer_id++;
    instance->add_timer(
        std::make_unique<Timer>(id, callback, delay, arguments, repeat));
    return id;
  }

  static void remove(const uint32_t id) { instance->remove_timer(id); }

  static bool run(JSContext *cx) { return instance->run_first_timer(cx); }

  static bool empty() { return instance->timers.empty(); }

  static int64_t timeout() {
    return instance->first_deadline;
  }

  void trace(JSTracer *trc) const {
    for (auto &timer : timers) {
      timer->trace(trc);
    }
  }
};

PersistentRooted<js::UniquePtr<ScheduledTimers>> ScheduledTimers::instance;
core::Engine* ScheduledTimers::engine = nullptr;

void TimersTask::enqueue() {
  engine->set_timeout_task(this, ScheduledTimers::timeout());
}
bool TimersTask::run() { return ScheduledTimers::run(engine->cx()); }

/**
 * The `setTimeout` and `setInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval
 */
template <bool repeat>
bool setTimeout_or_interval(JSContext *cx, const unsigned argc, Value *vp) {
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

  const uint32_t id = ScheduledTimers::add(handler, delay, handler_args, repeat);

  args.rval().setInt32(id);
  return true;
}

/**
 * The `clearTimeout` and `clearInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-cleartimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-clearinterval
 */
template <bool interval>
bool clearTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  REQUEST_HANDLER_ONLY(interval ? "clearInterval" : "clearTimeout");
  const CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, interval ? "clearInterval" : "clearTimeout",
                           1)) {
    return false;
  }

  int32_t id = 0;
  if (!JS::ToInt32(cx, args[0], &id)) {
    return false;
  }

  ScheduledTimers::remove(static_cast<uint32_t>(id));

  args.rval().setUndefined();
  return true;
}

constexpr JSFunctionSpec methods[] = {
    JS_FN("setInterval", setTimeout_or_interval<true>, 1, JSPROP_ENUMERATE),
    JS_FN("setTimeout", setTimeout_or_interval<false>, 1, JSPROP_ENUMERATE),
    JS_FS_END};

bool install(core::Engine *engine) {
  ScheduledTimers::init(engine);
  TimersTask::init(engine);
  return JS_DefineFunctions(engine->cx(), engine->global(), methods);
}

} // namespace builtins::web::timers
