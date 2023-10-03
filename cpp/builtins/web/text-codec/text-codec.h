#ifndef BUILTINS_WEB_TEXT_CODEC_H
#define BUILTINS_WEB_TEXT_CODEC_H

#include "builtins/builtin.h"

namespace builtins {
namespace web {
namespace text_codec {
bool add_to_global(JSContext *cx, JS::HandleObject global);
}
} // namespace web
} // namespace builtins

#endif
