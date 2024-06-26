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


public:
  Engine();
  JSContext *cx();
  HandleObject global();

  /// Initialize the engine with the given filename
  bool initialize(const char * filename);

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
  bool define_builtin_module(const char *id, HandleValue builtin);

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
  bool eval_toplevel(JS::SourceText<mozilla::Utf8Unit> &source, const char *path,
                     MutableHandleValue result);

  bool is_preinitializing();
  bool toplevel_evaluated();

  /**
   * Run the async event loop as long as there's interest registered in keeping it running.
   *
   * Each turn of the event loop consists of three steps:
   * 1. Run reactions to all promises that have been resolves/rejected.
   * 2. Check if there's any interest registered in continuing to wait for async tasks, and
   *    terminate the loop if not.
   * 3. Wait for the next async tasks and execute their reactions
   *
   * Interest or loss of interest in keeping the event loop running can be signaled using the
   * `Engine::incr_event_loop_interest` and `Engine::decr_event_loop_interest` methods.
   *
   * Every call to incr_event_loop_interest must be followed by an eventual call to
   * decr_event_loop_interest, for the event loop to complete. Otherwise, if no async tasks remain
   * pending while there's still interest in the event loop, an error will be reported.
   */
  bool run_event_loop();

  /**
   * Add an event loop interest to track
   */
  void incr_event_loop_interest();

  /**
   * Remove an event loop interest to track
   * The last decrementer marks the event loop as complete to finish
   */
  void decr_event_loop_interest();

  /**
   * Get the JS value associated with the top-level script execution -
   * the last expression for a script, or the module namespace for a module.
   */
  HandleValue script_value();

  bool has_pending_async_tasks();
  void queue_async_task(AsyncTask *task);
  bool cancel_async_task(AsyncTask *task);

  void abort(const char *reason);

  bool debug_logging_enabled();

  bool dump_value(JS::Value val, FILE *fp = stdout);
  bool print_stack(FILE *fp);
  void dump_pending_exception(const char *description = "");
  void dump_promise_rejection(HandleValue reason, HandleObject promise, FILE *fp);
};


typedef bool (*TaskCompletionCallback)(JSContext* cx, HandleObject receiver);

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

  [[nodiscard]] virtual uint64_t deadline() {
    return 0;
  }

  virtual void trace(JSTracer *trc) = 0;

  /**
   * Select for the next available ready task, providing the oldest ready first.
   */
  static size_t select(std::vector<AsyncTask *> &handles);
};

} // namespace api

#endif // EXTENSION_API_H
