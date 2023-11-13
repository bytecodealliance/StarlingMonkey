#ifndef JS_RT_CORE_ENGINE_H
#define JS_RT_CORE_ENGINE_H

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

namespace core {

class AsyncTask;

class Engine {
public:
  Engine();
  JSContext *cx();
  HandleObject global();
  bool eval(char *code, size_t len, const char* filename, MutableHandleValue result);
  bool run_event_loop(MutableHandleValue result);

  bool has_pending_async_tasks();
  void enqueue_async_task(AsyncTask *task);
  void set_timeout_task(AsyncTask *task, int64_t timeout);
  void remove_timeout_task();

  void abort(const char* reason);

  bool debug_logging_enabled();

  bool dump_value(JS::Value val, FILE *fp = stdout);
  void dump_pending_exception(const char* description = "");
  void dump_promise_rejection(HandleValue reason, HandleObject promise, FILE *fp);

private:
  double total_compute;
};

class AsyncTask {
public:
  virtual ~AsyncTask() = default;

  virtual void enqueue() = 0;
  virtual bool run() = 0;
};

} // namespace core

#endif
