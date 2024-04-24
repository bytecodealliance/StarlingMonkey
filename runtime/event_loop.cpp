#include "event_loop.h"

#include "host_api.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include <iostream>
#include <vector>

struct TaskQueue {
  std::vector<api::AsyncTask *> tasks = {};

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

// TODO: implement compute limit
bool EventLoop::process_jobs(api::Engine *engine, double total_compute) {
  JSContext *cx = engine->cx();
  while (js::HasJobsPending(cx)) {
    js::RunJobs(cx);
    if (JS_IsExceptionPending(cx))
      return false;
  }
  return true;
}

// TODO: implement timeout limit
bool EventLoop::process_async_tasks(api::Engine *engine, double timeout) {
  if (has_pending_async_tasks()) {
    auto tasks = &queue.get().tasks;
    const auto index = api::AsyncTask::select(tasks);
    auto task = tasks->at(index);
    if (!task->run(engine)) {
      return false;
    }
    tasks->erase(tasks->begin() + index);
  }
  return true;
}

void EventLoop::init(JSContext *cx) { queue.init(cx); }

} // namespace core
