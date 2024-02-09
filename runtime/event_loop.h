#ifndef JS_COMPUTE_RUNTIME_EVENT_LOOP_H
#define JS_COMPUTE_RUNTIME_EVENT_LOOP_H

#include "extension-api.h"

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "jsapi.h"
#pragma clang diagnostic pop

namespace core {

class EventLoop {
public:
  /**
   * Initialize the event loop
   */
  static void init(JSContext *cx);

  /**
   * Check if there are any pending tasks (io requests or timers) to process.
   */
  static bool has_pending_async_tasks();

  /**
   * Run the event loop until all async tasks have completed.
   *
   * Concretely, that means running a loop, whose body does two things:
   * 1. Run all micro-tasks, i.e. pending Promise reactions
   * 2. Run the next ready async task
   *
   * The loop terminates once both of these steps are null-ops.
   */
  static bool run_event_loop(api::Engine *engine, double total_compute, MutableHandleValue result);

  /**
   * Queue a new async task.
   */
  static void queue_async_task(api::AsyncTask *task);

  /**
   * Remove a queued async task.
   */
  static bool cancel_async_task(api::Engine *engine, int32_t id);
};

} // namespace core

#endif
