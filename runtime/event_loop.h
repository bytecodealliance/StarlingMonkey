#ifndef JS_COMPUTE_RUNTIME_EVENT_LOOP_H
#define JS_COMPUTE_RUNTIME_EVENT_LOOP_H

#include "bindings.h"
#include "engine.h"

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
   * Process any outstanding requests.
   */
  static bool process_pending_async_tasks(JSContext *cx);

  /**
   * Queue a new async task.
   */
  static bool queue_async_task(JS::HandleObject task);

  /**
   * Set a task to run after the specified timeout has elapsed.
   *
   * If a timeout task had already been set, it will be replaced.
   */
  static void set_timeout_task(AsyncTask *task, int64_t timeout);

  /**
   * Remove the currently set async task, if any.
   */
  static void remove_timeout_task();
};

} // namespace core

#endif
