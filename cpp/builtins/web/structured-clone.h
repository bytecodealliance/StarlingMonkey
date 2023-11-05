#ifndef JS_COMPUTE_RUNTIME_STRUCTURED_CLONE_H
#define JS_COMPUTE_RUNTIME_STRUCTURED_CLONE_H

#include "builtins/builtin.h"
#include "js/StructuredClone.h"
#include "url.h"

namespace builtins {
namespace web {
namespace structured_clone {

bool install(core::Engine* engine);

} // namespace structured_clone
} // namespace web
} // namespace builtins

#endif
