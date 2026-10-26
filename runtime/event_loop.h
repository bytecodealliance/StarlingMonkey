#ifndef JS_COMPUTE_RUNTIME_EVENT_LOOP_H
#define JS_COMPUTE_RUNTIME_EVENT_LOOP_H

#include "extension-api.h"
#include "jsapi.h"

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

  /**
   * Run the FinalizationRegistry cleanups that collections have queued, and the microtasks they
   * queue, until none are left. Used before a Wizer snapshot is taken, so that the snapshot holds
   * an empty queue. A cleanup that triggers a collection that queues more is run in a further
   * round, up to MAX_CLEANUP_DRAIN_ROUNDS; anything still queued then stays queued, and runs in
   * the first event loop after the snapshot is resumed. Returns false if an exception is pending.
   */
  static bool drain_finalization_registry_cleanups(api::Engine *engine);

  static constexpr unsigned MAX_CLEANUP_DRAIN_ROUNDS = 16;

  static void incr_event_loop_interest();
  static void decr_event_loop_interest();

  /**
   * Select on the next async tasks
   */
  static bool process_async_tasks(api::Engine *engine, double timeout);

  /**
   * Queue a new async task.
   */
  static void queue_async_task(const RefPtr<api::AsyncTask>& task);

  /**
   * Remove a queued async task.
   */
  static bool cancel_async_task(api::Engine *engine, const RefPtr<api::AsyncTask>& task);
};

} // namespace core

#endif
