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

  while (true) {
    while (js::HasJobsPending(cx)) {
      js::RunJobs(cx);
      if (JS_IsExceptionPending(cx)) {
        exit_event_loop();
        return false;
      }
      if (interest_complete()) {
        exit_event_loop();
        return true;
      }
    }

    const auto tasks = &queue.get().tasks;

    if (tasks->size() == 0) {
      fprintf(stderr, "event loop error - both task and job queues are empty, but expected "
                      "operations did not resolve");
      exit_event_loop();
      return false;
    }

    auto ready = api::AsyncTask::poll(tasks);
    const auto ready_len = ready.size();
    // ensure ready list is monotonic
    if (!std::is_sorted(ready.begin(), ready.end())) {
      std::sort(ready.begin(), ready.end());
    }
    for (auto i = 0; i < ready_len; i++) {
      // index is offset by previous erasures
      auto index = ready.at(i) - i;
      auto task = tasks->at(index);
      bool success = task->run(engine);
      tasks->erase(tasks->begin() + index);
      if (!success) {
        exit_event_loop();
        return false;
      }
      js::RunJobs(cx);
      if (JS_IsExceptionPending(cx)) {
        exit_event_loop();
        return false;
      }
      if (interest_complete()) {
        exit_event_loop();
        return true;
      }
    }
  }
}

void EventLoop::init(JSContext *cx) { queue.init(cx); }

} // namespace core
