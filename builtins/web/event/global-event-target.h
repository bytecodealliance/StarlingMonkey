#ifndef BUILTINS_WEB_GLOBAL_EVENT_TARGET_H_
#define BUILTINS_WEB_GLOBAL_EVENT_TARGET_H_

#include "builtin.h"

namespace builtins {
namespace web {
namespace event {

JSObject *global_event_target();

bool global_event_target_init(JSContext *cx, HandleObject global);

} // namespace event
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_GLOBAL_EVENT_TARGET_H_
