#ifndef BUILTINS_WEB_TIMERS_H
#define BUILTINS_WEB_TIMERS_H

#include "../builtin.h"

namespace builtins {
namespace web {
namespace timers {
bool add_to_global(JSContext *cx, JS::HandleObject global);
}
} // namespace web
} // namespace builtins

#endif
