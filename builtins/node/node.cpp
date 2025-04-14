#include "extension-api.h"

namespace builtins::node {

bool install(api::Engine* engine) {
  // Create the process object in the global scope
  JS::RootedObject process(engine->cx(), JS_NewPlainObject(engine->cx()));
  if (!process) {
    return false;
  }

  // Add process to global
  if (!JS_DefineProperty(engine->cx(), engine->global(), "process", process,
                        JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

} // namespace builtins::node 