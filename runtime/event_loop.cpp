#include "event_loop.h"

#include "host_api.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include <iostream>
#include <vector>

struct TaskQueue {
  std::vector<api::AsyncTask *> tasks = {};
  int interest_cnt = 0;
  bool event_loop_running = false;

  void trace(JSTracer *trc) const {
    for (const auto task : tasks) {
      task->trace(trc);
    }
  }
};

static PersistentRooted<TaskQueue> queue;

namespace core {

void EventLoop::queue_async_task(api::AsyncTask *task) { queue.get().tasks.emplace_back(task); }

bool EventLoop::cancel_async_task(api::Engine *engine, const int32_t id) {
  const auto tasks = &queue.get().tasks;
  for (auto it = tasks->begin(); it != tasks->end(); ++it) {
    const auto task = *it;
    if (task->id() == id) {
      task->cancel(engine);
      tasks->erase(it);
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
    fprintf(stderr, "cannot run event loop as it is already running");
    return false;
  }
  queue.get().event_loop_running = true;
  JSContext *cx = engine->cx();

  // Run a microtask checkpoint
  js::RunJobs(cx);

  while (true) {
    if (JS_IsExceptionPending(cx)) {
      exit_event_loop();
      return false;
    }

    const auto tasks = &queue.get().tasks;
    size_t tasks_size = tasks->size();

    if (tasks_size == 0) {
      exit_event_loop();
      if (interest_complete()) {
        return true;
      }
      fprintf(stderr, "event loop error - both task and job queues are empty, but expected "
                      "operations did not resolve");
      return false;
    }

    size_t task_idx;

    // Select the next task to run according to event-loop semantics of oldest-first.
    if (interest_complete()) {
      // Perform a non-blocking select in the case of there being no event loop interest
      // (we are thus only performing a "single tick", but must still progress work that is ready)
      std::optional<size_t> maybe_task_idx = api::AsyncTask::ready(tasks);
      if (!maybe_task_idx.has_value()) {
        exit_event_loop();
        return true;
      }
      task_idx = maybe_task_idx.value();
    } else {
      task_idx = api::AsyncTask::select(tasks);
    }

    auto task = tasks->at(task_idx);
    bool success = task->run(engine);
    tasks->erase(tasks->begin() + task_idx);
    if (!success) {
      exit_event_loop();
      return false;
    }

    // Run a single microtask checkpoint after each async task processing
    // to complete "one tick"
    js::RunJobs(cx);

    if (interest_complete()) {
      return true;
    }
  }
}

void EventLoop::init(JSContext *cx) { queue.init(cx); }

} // namespace core
