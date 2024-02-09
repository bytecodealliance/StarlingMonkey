#include "host_api.h"

namespace host_api {

/* Returns false if an exception is set on `cx` and the caller should
   immediately return to propagate the exception. */
void handle_api_error(JSContext *cx, uint8_t err, int line, const char *func) {
  JS_ReportErrorUTF8(cx, "%s: An error occurred while using the host API.\n", func);
}

} // namespace host_api
