#include "event_loop.h"

#include "host_api.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include <iostream>
#include <vector>

struct TaskQueue {
  std::vector<api::AsyncTask *> tasks = {};
  int lifetime_cnt = 0;

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

void EventLoop::incr_event_loop_lifetime() {
  queue.get().lifetime_cnt++;
}

void EventLoop::decr_event_loop_lifetime() {
  MOZ_ASSERT(queue.get().lifetime_cnt > 0);
  queue.get().lifetime_cnt--;
}

bool lifetime_complete() {
  return queue.get().lifetime_cnt == 0;
}

bool EventLoop::run_event_loop(api::Engine *engine, double total_compute) {
  JSContext *cx = engine->cx();

  while (true) {
    while (js::HasJobsPending(cx)) {
      js::RunJobs(cx);
      if (JS_IsExceptionPending(cx))
        return false;
      if (lifetime_complete())
        return true;
    }

    auto tasks = &queue.get().tasks;

    // Unresolved lifetime error -
    // if there are no async tasks, and the lifetime was not complete
    // then we cannot complete the lifetime
    if (tasks->size() == 0) {
      return false;
    }

    const auto ready = api::AsyncTask::poll(tasks);
    for (auto index : ready) {
      auto task = tasks->at(index);
      if (!task->run(engine)) {
        return false;
      }
      tasks->erase(tasks->begin() + index);
      if (lifetime_complete())
        return true;
    }
  }

  return true;
}

void EventLoop::init(JSContext *cx) { queue.init(cx); }

} // namespace core
