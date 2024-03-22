#ifndef JS_COMPUTE_RUNTIME_DECODE_H
#define JS_COMPUTE_RUNTIME_DECODE_H

#include "host_api.h"

namespace core {

JSString* decode(JSContext *cx, host_api::HostString& str);

} // namespace core

#endif
