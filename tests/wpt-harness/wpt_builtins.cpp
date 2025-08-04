#include "extension-api.h"

#include <encode.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <url.h>
#include <worker-location.h>

using builtins::web::worker_location::WorkerLocation;

static bool baseURL_set(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.get(0).isNullOrUndefined()) {
    WorkerLocation::url.set(nullptr);
  } else if (!builtins::web::url::URL::is_instance(args.get(0))) {
    return api::throw_error(cx, api::Errors::TypeError, "baseURL setter", "value",
      "be a URL object, null, or undefined");
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

/**
 * Evaluate the given script in the global scope, without creating a new lexical scope.
 * This is roughly equivalent to how `<script>` tags work in HTML, and hence how
 * the WPT harness needs to load `META` scripts: otherwise, `let` and `const` bindings
 * aren't visible to importing code, and the tests break.
 */
static bool evalScript(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  auto script = core::encode(cx, args.get(0));
  if (!script) {
    return false;
  }
  JS::SourceText<mozilla::Utf8Unit> source;
  MOZ_RELEASE_ASSERT(source.init(cx, std::move(script.ptr), script.len));
  JS::CompileOptions options(cx);
  options.setNonSyntacticScope(true);
  return Evaluate(cx, options, source, args.rval());
}


const JSPropertySpec properties[] = {
  JS_PSGS("wpt_baseURL", baseURL_get, baseURL_set, JSPROP_ENUMERATE),
JS_PS_END};

namespace wpt_support {

bool install(api::Engine* engine) {
  if (!engine->wpt_mode()) {
    return true;
  }

  if (!JS_DefineFunction(engine->cx(), engine->global(), "evalScript", evalScript, 1, 0)) {
    return false;
  }
  if (!JS_DefineProperties(engine->cx(), engine->global(), properties)) {
    return false;
  }

  printf("installing wpt_builtins\n");
  return true;
}

} // namespace wpt_builtins
