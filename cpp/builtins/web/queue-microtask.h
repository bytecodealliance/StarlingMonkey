#ifndef JS_COMPUTE_RUNTIME_QUEUE_MICROTASK_H
#define JS_COMPUTE_RUNTIME_QUEUE_MICROTASK_H

#include "builtins/builtin.h"

namespace builtins {
namespace web {
namespace queue_microtask {

bool add_to_global(JSContext *cx, JS::HandleObject global);

} // namespace queue_microtask
} // namespace web
} // namespace builtins

#endif
