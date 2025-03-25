#include "process_env.h"
#include "extension-api.h"
#include "host_api.h"

namespace builtins::node::process_env {

bool install(api::Engine* engine) {
  // Create process.env object
  JS::RootedObject env(engine->cx(), JS_NewPlainObject(engine->cx()));
  if (!env) {
    return false;
  }

  // Get environment variables from WASI
  auto env_vars = host_api::environment_get_environment();

  // Add each environment variable to the env object
  for (const auto& [key, value] : env_vars) {
    JS::RootedString key_str(engine->cx(), JS_NewStringCopyN(engine->cx(), 
      key.ptr.get(), key.len));
    if (!key_str) {
      return false;
    }

    JS::RootedString value_str(engine->cx(), JS_NewStringCopyN(engine->cx(),
      value.ptr.get(), value.len));
    if (!value_str) {
      return false;
    }

    // Convert key string to UTF8
    JS::UniqueChars key_utf8(JS_EncodeStringToUTF8(engine->cx(), key_str));
    if (!key_utf8) {
      return false;
    }

    if (!JS_DefineProperty(engine->cx(), env, key_utf8.get(), value_str,
                          JSPROP_ENUMERATE)) {
      return false;
    }
  }

  // Get the global object
  JS::RootedObject global(engine->cx(), engine->global());

  // Check if process object exists, create if not
  JS::RootedValue processVal(engine->cx());
  if (!JS_GetProperty(engine->cx(), global, "process", &processVal)) {
    return false;
  }

  JS::RootedObject processObj(engine->cx());
  if (processVal.isUndefined()) {
    processObj = JS_NewPlainObject(engine->cx());
    if (!processObj) {
      return false;
    }
    if (!JS_DefineProperty(engine->cx(), global, "process", processObj,
                          JSPROP_ENUMERATE)) {
      return false;
    }
  } else {
    processObj = &processVal.toObject();
  }

  // Add env object to process
  if (!JS_DefineProperty(engine->cx(), processObj, "env", env,
                        JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

} // namespace builtins::node::process_env
