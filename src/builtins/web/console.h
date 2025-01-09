#ifndef BUILTINS_WEB_CONSOLE_H
#define BUILTINS_WEB_CONSOLE_H

#include "extension-api.h"

namespace builtins::web::console {

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
};

bool install(api::Engine *engine);

} // namespace builtins::web::console

#endif
