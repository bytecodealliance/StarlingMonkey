#ifndef JS_COMPUTE_RUNTIME_STRUCTURED_CLONE_H
#define JS_COMPUTE_RUNTIME_STRUCTURED_CLONE_H

#include "builtins/builtin.h"
#include "js/StructuredClone.h"
#include "url.h"

namespace builtins {
namespace web {
namespace structured_clone {

bool add_to_global(JSContext *cx, JS::HandleObject global);

} // namespace structured_clone
} // namespace web
} // namespace builtins

#endif
