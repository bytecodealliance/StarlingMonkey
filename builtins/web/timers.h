#ifndef BUILTINS_WEB_TIMERS_H
#define BUILTINS_WEB_TIMERS_H

#include "extension-api.h"

namespace builtins::web::timers {

bool set_timeout(JSContext *cx, HandleObject handler, JS::HandleValueVector args, int32_t delay_ms,
                 int32_t *timer_id);

bool set_interval(JSContext *cx, HandleObject handler, JS::HandleValueVector args, int32_t delay_ms,
                  int32_t *timer_id);

void clear_timeout_or_interval(int32_t timer_id);

bool install(api::Engine *engine);

} // namespace builtins::web::timers

#endif
