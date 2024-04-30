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

  bool polled = false;
  std::vector<size_t> ready_list;
  size_t ready_idx;
  size_t erasures_since_last_poll = 0;

  while (true) {
    // Run a microtask checkpoint
    js::RunJobs(cx);

    if (JS_IsExceptionPending(cx)) {
      exit_event_loop();
      return false;
    }
    if (interest_complete()) {
      exit_event_loop();
      return true;
    }

    const auto tasks = &queue.get().tasks;
    size_t tasks_size = tasks->size();
    if (tasks_size == 0) {
      exit_event_loop();
      // if there is no interest in the event loop at all, we always run one tick
      if (interest_complete()) {
        return true;
      } else {
        fprintf(stderr, "event loop error - both task and job queues are empty, but expected "
                        "operations did not resolve");
        return false;
      }
    }

    // Select the next task to run according to event-loop semantics of oldest-first.
    // ready_list and last_idx are set if this has been run previously, where the previous list is
    // used as an optimization when the next ready item is adjacent, to avoid a new unnecessary poll
    // host call when possible.
    if (polled && ready_list.size() > ready_idx + 1 &&
        ready_list.at(ready_idx + 1) == ready_list.at(ready_idx) + 1) {
      ready_idx++;
    } else {
      ready_list = api::AsyncTask::poll(tasks);
      polled = true;
      erasures_since_last_poll = 0;
      MOZ_ASSERT(ready_list.size() > 0);
      ready_idx = 0;
    }

    size_t task_idx = ready_list.at(ready_idx);
    // index is offset by task processing erasures
    auto task = tasks->at(task_idx - erasures_since_last_poll);
    bool success = task->run(engine);
    tasks->erase(tasks->begin() + task_idx);
    erasures_since_last_poll++;
    if (!success) {
      exit_event_loop();
      return false;
    }
  }
}

void EventLoop::init(JSContext *cx) { queue.init(cx); }

} // namespace core
