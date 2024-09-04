#ifndef BUILTINS_WEB_STRUCTURED_CLONE_H
#define BUILTINS_WEB_STRUCTURED_CLONE_H

#include "builtin.h"
#include "js/StructuredClone.h"
#include "url.h"

namespace builtins {
namespace web {
namespace structured_clone {

bool install(api::Engine *engine);

} // namespace structured_clone
} // namespace web
} // namespace builtins

#endif
