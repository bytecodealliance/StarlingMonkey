#ifndef BUILTINS_WEB_CONSOLE_H
#define BUILTINS_WEB_CONSOLE_H

#include "../builtin.h"

namespace builtins {
namespace web {

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

  static bool install(core::Engine* engine);
};
} // namespace web

} // namespace builtins

#endif
