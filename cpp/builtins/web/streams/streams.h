#ifndef BUILTINS_WEB_STREAMS_H
#define BUILTINS_WEB_STREAMS_H

#include "builtins/builtin.h"

namespace builtins {
namespace web {
namespace streams {
bool add_to_global(JSContext *cx, JS::HandleObject global);
} // namespace streams
} // namespace web
} // namespace builtins

#endif
