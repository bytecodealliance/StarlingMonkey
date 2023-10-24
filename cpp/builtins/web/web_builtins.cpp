#include "web_builtins.h"
#include "builtins/builtin.h"

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

bool builtins::web::add_to_global(JSContext *cx, JS::HandleObject global) {
  return Console::add_to_global(cx, global) &&
         base64::add_to_global(cx, global) &&
         crypto::add_to_global(cx, global) &&
         fetch::add_to_global(cx, global) &&
         performance::add_to_global(cx, global) &&
         queue_microtask::add_to_global(cx, global) &&
        //  streams::add_to_global(cx, global) &&
         timers::add_to_global(cx, global) &&
         text_codec::add_to_global(cx, global) &&
         url::add_to_global(cx, global) &&
         worker_location::add_to_global(cx, global) &&
         JS_DefineProperties(cx, global, properties);
}
