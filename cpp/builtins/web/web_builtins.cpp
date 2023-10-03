#include "web_builtins.h"
#include "base64.h"
#include "console.h"
#include "crypto/crypto.h"
#include "fetch/fetch-api.h"
#include "streams/streams.h"
#include "timers.h"
#include "text-codec/text-codec.h"
#include "url.h"
#include "worker-location.h"

bool builtins::web::add_to_global(JSContext *cx, JS::HandleObject global) {
  return Console::add_to_global(cx, global) &&
         base64::add_to_global(cx, global) &&
         crypto::add_to_global(cx, global) &&
         fetch::add_to_global(cx, global) &&
         streams::add_to_global(cx, global) &&
         timers::add_to_global(cx, global) &&
         text_codec::add_to_global(cx, global) &&
         url::add_to_global(cx, global) &&
         worker_location::add_to_global(cx, global);
}
