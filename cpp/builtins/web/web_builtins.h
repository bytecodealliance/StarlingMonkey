#ifndef BUILTINS_WEB_H
#define BUILTINS_WEB_H

#include "../builtin.h"

namespace builtins {
namespace web {

bool add_to_global(JSContext *cx, JS::HandleObject global);

} // namespace web

} // namespace builtins

#endif
