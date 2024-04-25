#ifndef EXTENSION_API_H
#define EXTENSION_API_H
#include <vector>

#include "builtin.h"

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include "jsapi.h"
#pragma clang diagnostic pop

using JS::RootedObject;
using JS::RootedString;
using JS::RootedValue;

using JS::HandleObject;
using JS::HandleValue;
using JS::HandleValueArray;
using JS::MutableHandleValue;

using JS::PersistentRooted;
using JS::PersistentRootedVector;

typedef int32_t PollableHandle;
constexpr PollableHandle INVALID_POLLABLE_HANDLE = -1;

namespace api {

class AsyncTask;

class Engine {

typedef int Lifetime;

#define LIFETIME_NONE -1;
#define LIFETIME_ALL -2;

public:
  Engine();
  JSContext *cx();
  HandleObject global();

  /**
   * Define a new builtin module
   * 
   * The enumerable properties of the builtin object are used to construct
   * a synthetic module namespace for the module.
   * 
   * The enumeration and getters are called only on the first import of
   * the builtin, so that lazy getters can be used to lazily initialize
   * builtins.
   * 
   * Once loaded, the instance is cached and reused as a singleton.
   */
  bool define_builtin_module(const char* id, HandleValue builtin);

  /**
   * Treat the top-level script as a module or classic JS script.
   *
   * By default, the engine treats the top-level script as a module.
   * Since not all content can be run as a module, this method allows
   * changing this default, and will impact all subsequent top-level
   * evaluations.
   */
  void enable_module_mode(bool enable);
  bool eval_toplevel(const char *path, MutableHandleValue result);

  /**
   * Run the JS task queue and wait on pending tasks until there
   * are no outstanding lifetimes to wait on.
   */
  bool run_event_loop();

  /**
   * Add an event loop lifetime to track
   */
  void incr_event_loop_lifetime();

  /**
   * Remove an event loop lifetime to track
   * The last decrementer marks the event loop as complete to finish
   */
  void decr_event_loop_lifetime();

  /**
   * Get the JS value associated with the top-level script execution -
   * the last expression for a script, or the module namespace for a module.
   */
  HandleValue script_value();

  bool has_pending_async_tasks();
  void queue_async_task(AsyncTask *task);
  bool cancel_async_task(int32_t id);

  void abort(const char *reason);

  bool debug_logging_enabled();

  bool dump_value(JS::Value val, FILE *fp = stdout);
  bool print_stack(FILE *fp);
  void dump_pending_exception(const char *description = "");
  void dump_promise_rejection(HandleValue reason, HandleObject promise, FILE *fp);
};

class AsyncTask {
protected:
  PollableHandle handle_ = -1;

public:
  virtual ~AsyncTask() = default;

  virtual bool run(Engine *engine) = 0;
  virtual bool cancel(Engine *engine) = 0;

  [[nodiscard]] virtual PollableHandle id() {
    MOZ_ASSERT(handle_ != INVALID_POLLABLE_HANDLE);
    return handle_;
  }

  virtual void trace(JSTracer *trc) = 0;

  /**
   * Poll for completion on the given async tasks
   * A list of ready task indices is returned
   */
  static std::vector<size_t> poll(std::vector<AsyncTask *> *handles);

  /**
   * Returns whether or not the given task is ready
   */
  static bool ready(AsyncTask *task);
};

} // namespace api

#endif // EXTENSION_API_H
