#ifndef JS_COMPUTE_RUNTIME_EVENT_LOOP_H
#define JS_COMPUTE_RUNTIME_EVENT_LOOP_H

#include "host_interface/host_api.h"

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
   * Register a timer.
   */
  static uint32_t add_timer(JS::HandleObject callback, uint32_t delay,
                            JS::HandleValueVector arguments, bool repeat);

  /**
   * Remove an active timer
   */
  static void remove_timer(uint32_t id);
};

} // namespace core

#endif
