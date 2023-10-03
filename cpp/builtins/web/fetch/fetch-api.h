#ifndef BUILTINS_WEB_FETCH_API_H
#define BUILTINS_WEB_FETCH_API_H

#include "builtins/builtin.h"

namespace builtins {
namespace web {
namespace fetch {
bool add_to_global(JSContext *cx, JS::HandleObject global);
}
} // namespace web
} // namespace builtins

#endif
