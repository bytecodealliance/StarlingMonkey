#ifndef DEBUGGER_H
#define DEBUGGER_H
#include "extension-api.h"

namespace content_debugger {
  /**
   * Establish a debug connection via a TCP socket and start debugging using a debugger script
   * received via the new connection.
   * Only active in builds with the `ENABLE_JS_DEBUGGER` CMake option set.
   *
   * @param content_already_initialized Whether the content script has already run
   *
   * For more detail on using the Debugger API, see
   * https://firefox-source-docs.mozilla.org/js/Debugger/index.html
   *
   * @return the global object that serves as the debugger
   */
  void maybe_init_debugger(api::Engine *engine, bool content_already_initialized);

  /**
   * Get the path to a replacement script to evaluate instead of the content script.
   * Always returns `false` in builds without the `ENABLE_JS_DEBUGGER` CMake option set.
   *
   * @return the path to the replacement script, if any
   */
  mozilla::Maybe<std::string_view> replacement_script_path();

  bool dbg_print(JSContext *cx, unsigned argc, Value *vp);
} // namespace content_debugger

#endif // DEBUGGER_H
