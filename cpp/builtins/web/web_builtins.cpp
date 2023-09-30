#include "web_builtins.h"
#include "base64.h"
#include "console.h"
#include "timers.h"

bool builtins::web::add_to_global(JSContext *cx, JS::HandleObject global) {
  return Console::add_to_global(cx, global) &&
         base64::add_to_global(cx, global) && timers::add_to_global(cx, global);
}
