#include "event_loop.h"

#include "host_api.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/GCVector.h"
#include "js/Utility.h"

#include <iostream>
#include <print>
#include <vector>

struct TaskQueue {
  std::vector<RefPtr<api::AsyncTask>> tasks;
  int interest_cnt = 0;
  bool event_loop_running = false;

  void trace(JSTracer *trc) const {
    for (const auto &task : tasks) {
      task->trace(trc);
    }
  }
};

static PersistentRooted<TaskQueue> queue;

// SpiderMonkey hands the embedding one cleanup function per FinalizationRegistry that has
// records of dead targets, and leaves calling it to the embedding (HTML's
// HostEnqueueFinalizationRegistryCleanupJob). The functions are queued here and run by the event
// loop, as a task after the microtask checkpoint.
using CleanupFunctions = JS::GCVector<JSFunction *, 0, js::SystemAllocPolicy>;
static PersistentRooted<CleanupFunctions> finalization_registry_cleanups;

// Called during GC, so this must not trigger another collection: it only appends to the queue.
// An append can only fail on allocation failure, which is fatal, as in SpiderMonkey's shell.
static void queue_finalization_registry_cleanup(JSFunction *do_cleanup,
                                                JSObject * /*incumbent_global*/,
                                                void * /*data*/) {
  js::AutoEnterOOMUnsafeRegion oom_unsafe;
  if (!finalization_registry_cleanups.get().append(do_cleanup)) {
    oom_unsafe.crash("queue_finalization_registry_cleanup");
  }
}

// Calls every queued cleanup function and returns whether any ran. An exception thrown by a
// callback is reported and cleared, and does not fail the event loop, as in a browser.
static bool run_finalization_registry_cleanups(api::Engine *engine) {
  if (finalization_registry_cleanups.get().empty()) {
    return false;
  }
  JSContext *cx = engine->cx();
  JS::Rooted<CleanupFunctions> cleanups(cx);
  std::swap(cleanups.get(), finalization_registry_cleanups.get());
  JS::RootedFunction cleanup(cx);
  JS::RootedValue rval(cx);
  for (JSFunction *fn : cleanups) {
    cleanup = fn;
    JS::RootedObject cleanup_obj(cx, JS_GetFunctionObject(cleanup));
    JS::ExposeObjectToActiveJS(cleanup_obj);
    JSAutoRealm ar(cx, cleanup_obj);
    if (!JS_CallFunction(cx, nullptr, cleanup, JS::HandleValueArray::empty(), &rval)) {
      api::Engine::dump_pending_exception("running a FinalizationRegistry callback");
      JS_ClearPendingException(cx);
    }
  }
  return true;
}

namespace core {

void EventLoop::queue_async_task(const RefPtr<api::AsyncTask>& task) {
  MOZ_ASSERT(task);
  queue.get().tasks.emplace_back(task);
}

bool EventLoop::cancel_async_task(api::Engine *engine, const RefPtr<api::AsyncTask>& task) {
  auto *const tasks = &queue.get().tasks;
  for (auto it = tasks->begin(); it != tasks->end(); ++it) {
    if (*it == task) {
      tasks->erase(it);
      task->cancel(engine);
      return true;
    }
  }
  return false;
}

bool EventLoop::has_pending_async_tasks() { return !queue.get().tasks.empty(); }

void EventLoop::incr_event_loop_interest() { queue.get().interest_cnt++; }

void EventLoop::decr_event_loop_interest() {
  MOZ_ASSERT(queue.get().interest_cnt > 0);
  queue.get().interest_cnt--;
}

inline bool interest_complete() { return queue.get().interest_cnt == 0; }

inline void exit_event_loop() { queue.get().event_loop_running = false; }

bool EventLoop::run_event_loop(api::Engine *engine, double total_compute) {
  if (queue.get().event_loop_running) {
    std::print(stderr, "cannot run event loop as it is already running");
    return false;
  }
  queue.get().event_loop_running = true;
  JSContext *cx = engine->cx();

  while (true) {
    // Run a microtask checkpoint
    js::RunJobs(cx);

    if (JS_IsExceptionPending(cx)) {
      exit_event_loop();
      return false;
    }

    // Once the microtasks are done, run the FinalizationRegistry cleanups that a collection
    // queued, then go round again for the microtasks they queued.
    if (run_finalization_registry_cleanups(engine)) {
      continue;
    }

    // if there is no interest in the event loop at all, just run one tick
    if (interest_complete()) {
      exit_event_loop();
      return true;
    }

    auto *const tasks = &queue.get().tasks;
    size_t tasks_size = tasks->size();
    if (tasks_size == 0) {
      exit_event_loop();
      MOZ_ASSERT(!interest_complete());
      return false;
    }

    // Select the next task to run according to event-loop semantics of oldest-first.
    size_t task_idx = api::AsyncTask::select(*tasks);

    auto task = tasks->at(task_idx);
    tasks->erase(tasks->begin() + task_idx);
    bool success = task->run(engine);
    if (!success) {
      exit_event_loop();
      return false;
    }
  }
}

bool EventLoop::drain_finalization_registry_cleanups(api::Engine *engine) {
  JSContext *cx = engine->cx();
  for (unsigned round = 0; round < MAX_CLEANUP_DRAIN_ROUNDS; round++) {
    if (!run_finalization_registry_cleanups(engine)) {
      return true;
    }
    js::RunJobs(cx);
    if (JS_IsExceptionPending(cx)) {
      return false;
    }
  }
  return true;
}

void EventLoop::init(JSContext *cx) {
  queue.init(cx);
  finalization_registry_cleanups.init(cx);
  JS::SetHostCleanupFinalizationRegistryCallback(cx, queue_finalization_registry_cleanup, nullptr);
}

} // namespace core
