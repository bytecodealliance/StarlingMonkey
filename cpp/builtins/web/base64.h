#ifndef BUILTINS_WEB_BASE64_H
#define BUILTINS_WEB_BASE64_H

#include "../builtin.h"

namespace builtins {
namespace web {
namespace base64 {
bool add_to_global(JSContext *cx, JS::HandleObject global);
}
} // namespace web
} // namespace builtins

#endif
