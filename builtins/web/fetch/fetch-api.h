#ifndef BUILTINS_WEB_FETCH_API_H
#define BUILTINS_WEB_FETCH_API_H

#include "builtin.h"
#include "request-response.h"

namespace builtins {
namespace web {
namespace fetch {

bool install(api::Engine *engine);
bool ensure_no_response(JSContext *cx, HandleObject request,
                        host_api::FutureHttpIncomingResponse *future);

} // namespace fetch
} // namespace web
} // namespace builtins

#endif
