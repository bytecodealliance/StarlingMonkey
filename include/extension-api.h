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

  bool is_initializing();
  void set_init_finished();

  JSContext *cx();
  HandleObject global();
  bool eval(char *code, size_t len, const char* filename, MutableHandleValue result);
  bool run_event_loop(MutableHandleValue result);

  bool has_pending_async_tasks();
  void queue_async_task(AsyncTask *task);
  bool cancel_async_task(int32_t id);

  void abort(const char* reason);

  bool debug_logging_enabled();

  bool dump_value(JS::Value val, FILE *fp = stdout);
  void dump_pending_exception(const char* description = "");
  void dump_promise_rejection(HandleValue reason, HandleObject promise, FILE *fp);
};

class AsyncTask {
protected:
  PollableHandle handle_ = -1;

public:
  virtual ~AsyncTask() = default;

  virtual bool run(Engine* engine) = 0;
  virtual bool cancel(Engine* engine) = 0;
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
  static size_t select(std::vector<AsyncTask *> * handles);
};

} // namespace api

#endif //EXTENSION_API_H
