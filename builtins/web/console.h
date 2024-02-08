#ifndef JS_COMPUTE_RUNTIME_BUILTIN_CONSOLE_H
#define JS_COMPUTE_RUNTIME_BUILTIN_CONSOLE_H

#include "builtin.h"

namespace builtins {

class Console : public BuiltinNoConstructor<Console> {
private:
public:
  static constexpr const char *class_name = "Console";
  enum LogType {
    Log,
    Info,
    Debug,
    Warn,
    Error,
  };
  enum Slots { Count };
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool create(JSContext *cx, JS::HandleObject global);
};

} // namespace builtins

void builtin_impl_console_log(builtins::Console::LogType log_ty, const char *msg);

#endif
