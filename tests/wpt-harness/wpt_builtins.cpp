#include "extension-api.h"

#include <url.h>
#include <worker-location.h>

using builtins::web::worker_location::WorkerLocation;

static bool baseURL_set(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.get(0).isNullOrUndefined()) {
    WorkerLocation::url.set(nullptr);
  } else if (!builtins::web::url::URL::is_instance(args.get(0))) {
    JS_ReportErrorUTF8(cx, "Invalid value assigned to baseURL, must be an instance of "
                           "URL, null, or undefined");
    return false;
  }

  WorkerLocation::url.set(&args.get(0).toObject());

  args.rval().setObjectOrNull(WorkerLocation::url.get());
  return true;
}

static bool baseURL_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setObjectOrNull(WorkerLocation::url.get());
  return true;
}


const JSPropertySpec properties[] = {
  JS_PSGS("baseURL", baseURL_get, baseURL_set, JSPROP_ENUMERATE),
JS_PS_END};

namespace wpt_support {

bool install(api::Engine* engine) {
  engine->enable_module_mode(false);
  if (!JS_DefineProperties(engine->cx(), engine->global(), properties)) {
    return false;
  }

  printf("installing wpt_builtins\n");
  return true;
}

} // namespace wpt_builtins
