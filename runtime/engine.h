#ifndef WINTER_RT_CORE_ENGINE_H
#define WINTER_RT_CORE_ENGINE_H

#include "bindings.h"
#include "jsapi.h"

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
class Engine {
public:
  Engine();
  JSContext *cx();
  HandleObject global();
  bool eval(char *code, size_t len, MutableHandleValue result);
  bool run_event_loop(MutableHandleValue result);
  bool dump_value(JS::Value val, FILE *fp = stdout);
  void dump_pending_exception(const char* description = "");

private:
  double total_compute;
};
} // namespace core

bool dump_value(JSContext* cx, JS::Value val, FILE *fp);

void dump_promise_rejection(JSContext *cx, JS::HandleValue reason,
                            JS::HandleObject promise, FILE *fp);
bool print_stack(JSContext *cx, FILE *fp);
bool print_stack(JSContext *cx, JS::HandleObject stack, FILE *fp);

#endif
