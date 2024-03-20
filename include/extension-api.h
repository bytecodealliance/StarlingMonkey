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

  bool run_event_loop(MutableHandleValue result);

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
  virtual bool ready() = 0;

  [[nodiscard]] virtual PollableHandle id() {
    MOZ_ASSERT(handle_ != INVALID_POLLABLE_HANDLE);
    return handle_;
  }

  virtual void trace(JSTracer *trc) = 0;

  /// Returns the first ready `AsyncTask`.
  ///
  /// TODO: as an optimization, return a vector containing the ready head of the queue.
  /// Note that that works iff the very first entry in the queue is ready, and then only
  /// for the dense head of the queue, without gaps. This is because during processing
  /// of the ready tasks, other tasks might become ready that should be processed first.
  static size_t select(std::vector<AsyncTask *> *handles);
};

} // namespace api

#endif // EXTENSION_API_H
