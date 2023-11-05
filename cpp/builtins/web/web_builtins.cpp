#include "web_builtins.h"

#include "base64.h"
#include "console.h"
#include "crypto/crypto.h"
#include "fetch/fetch-api.h"
#include "performance.h"
#include "queue-microtask.h"
// #include "streams/streams.h"
#include "text-codec/text-codec.h"
#include "timers.h"
#include "url.h"
#include "worker-location.h"

bool self_get(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setObject(*JS::CurrentGlobalOrNull(cx));
  return true;
}

bool self_set(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "globalThis.self setter", 1)) {
    return false;
  }

  RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  if (args.thisv() != ObjectValue(*global)) {
    JS_ReportErrorLatin1(
        cx, "globalThis.self setter can only be called on the global object");
    return false;
  }

  if (!JS_DefineProperty(cx, global, "self", args[0], JSPROP_ENUMERATE)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

const JSPropertySpec properties[] = {
    JS_PSGS("self", self_get, self_set, JSPROP_ENUMERATE), JS_PS_END};

bool builtins::web::install(core::Engine *engine) {
  return Console::install(engine) &&
         base64::install(engine) &&
         crypto::install(engine) &&
         fetch::install(engine) &&
         performance::install(engine) &&
         queue_microtask::install(engine) &&
        //  streams::install(engine) &&
         timers::install(engine) &&
         text_codec::install(engine) &&
         url::install(engine) &&
         worker_location::install(engine) &&
         JS_DefineProperties(engine->cx(), engine->global(), properties);
}
