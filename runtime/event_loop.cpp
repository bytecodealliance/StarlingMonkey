#include "event_loop.h"

#include "host_api.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include <iostream>
#include <vector>

struct TaskQueue {
  std::vector<api::AsyncTask*> tasks = {};

  void trace(JSTracer *trc) const {
    for (const auto task : tasks) {
      task->trace(trc);
    }
  }
};

static PersistentRooted<TaskQueue> queue;

namespace core {

void EventLoop::queue_async_task(api::AsyncTask * task) {
  queue.get().tasks.emplace_back(task);
}

bool EventLoop::cancel_async_task(api::Engine * engine, const int32_t id) {
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

bool EventLoop::has_pending_async_tasks() {
  return !queue.get().tasks.empty();
}

bool EventLoop::run_event_loop(api::Engine * engine, double total_compute, MutableHandleValue result) {
  // Loop until no more resolved promises or backend requests are pending.
  // LOG("Start processing async jobs ...\n");

  JSContext* cx = engine->cx();

  do {
    // First, drain the promise reactions queue.
    while (js::HasJobsPending(cx)) {
      js::RunJobs(cx);

      if (JS_IsExceptionPending(cx))
        engine->abort("running Promise reactions");
    }

    // TODO: add general mechanism for extending the event loop duration.
    // Then, check if the fetch event is still active, i.e. had pending promises
    // added to it using `respondWith` or `waitUntil`.
    // if (!builtins::FetchEvent::is_active(fetch_event))
    //   break;

    // Process async tasks.
    if (has_pending_async_tasks()) {
      auto tasks = &queue.get().tasks;
      const auto index = api::AsyncTask::select(tasks);
      auto task = tasks->at(index);
      if (!task->run(engine)) {
        return false;
      }
      tasks->erase(tasks->begin() + index);
    }
  } while (js::HasJobsPending(engine->cx()) || has_pending_async_tasks());

  return true;
}

void EventLoop::init(JSContext *cx) {
  queue.init(cx);
}

} // namespace core
