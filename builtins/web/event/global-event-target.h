#ifndef BUILTINS_WEB_GLOBAL_EVENT_TARGET_H_
#define BUILTINS_WEB_GLOBAL_EVENT_TARGET_H_

#include "builtin.h"



namespace builtins::web::event {

JSObject *global_event_target();

bool global_event_target_init(JSContext *cx, HandleObject global);

} // namespace builtins::web::event



#endif // BUILTINS_WEB_GLOBAL_EVENT_TARGET_H_
