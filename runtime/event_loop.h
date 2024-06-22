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
   * Run the event loop until all interests are complete.
   * See run_event_loop in extension-api.h for the complete description.
   */
  static bool run_event_loop(api::Engine *engine, double total_compute);

  static void incr_event_loop_interest();
  static void decr_event_loop_interest();

  /**
   * Select on the next async tasks
   */
  static bool process_async_tasks(api::Engine *engine, double timeout);

  /**
   * Queue a new async task.
   */
  static void queue_async_task(api::AsyncTask *task);

  /**
   * Remove a queued async task.
   */
  static bool remove_async_task(api::Engine *engine, api::AsyncTask *task);
};

} // namespace core

#endif
