#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#ifdef MEM_STATS
#include <string>
#endif

#include <wasi/libc-environ.h>

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/CompilationAndEvaluation.h"
#include "js/ContextOptions.h"
#include "js/Initialization.h"
#include "js/SourceText.h"
#include "jsapi.h"

#pragma clang diagnostic pop

#include "builtins/fetch-event.h"
#include "core/allocator.h"
#include "core/encode.h"
#include "core/event_loop.h"
#include "host_interface/host_api.h"
#include "js-compute-builtins.h"
#include "third_party/wizer.h"
#ifdef MEM_STATS
#include "memory-reporting.h"
#endif

#include "builtins/shared/performance.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::system_clock;

using JS::Value;

using JS::RootedObject;
using JS::RootedString;
using JS::RootedValue;

using JS::HandleObject;
using JS::HandleValue;
using JS::HandleValueArray;
using JS::MutableHandleValue;

using JS::PersistentRooted;
using JS::PersistentRootedVector;

#ifdef MEM_STATS
size_t size_of_cb(const void *ptr) { return ptr ? sizeof(ptr) : 0; }

static bool dump_mem_stats(JSContext *cx) {
  SimpleJSRuntimeStats rtStats(&size_of_cb);
  if (!JS::CollectRuntimeStats(cx, &rtStats, nullptr, false))
    return false;
  std::string rtPath = "rt";
  size_t rtTotal;
  ReportJSRuntimeExplicitTreeStats(rtStats, rtPath, nullptr, false, &rtTotal);

  printf("compartment counts: %zu sys, %zu usr\n", JS::SystemCompartmentCount(cx),
         JS::UserCompartmentCount(cx));
  printf("GC heap total: %zu\n",
         size_t(JS_GetGCParameter(cx, JSGC_TOTAL_CHUNKS)) * js::gc::ChunkSize);
  printf("GC heap unused: %zu\n",
         size_t(JS_GetGCParameter(cx, JSGC_UNUSED_CHUNKS)) * js::gc::ChunkSize);

  return true;
}
#endif // MEM_STATS

/* The class of the global object. */
static JSClass global_class = {"global", JSCLASS_GLOBAL_FLAGS, &JS::DefaultGlobalClassOps};

JS::PersistentRootedObject GLOBAL;
JS::PersistentRootedObject unhandledRejectedPromises;

static JS::PersistentRootedObjectVector *FETCH_HANDLERS;

void gc_callback(JSContext *cx, JSGCStatus status, JS::GCReason reason, void *data) {
  if (debug_logging_enabled())
    printf("gc for reason %s, %s\n", JS::ExplainGCReason(reason), status ? "end" : "start");
}

static void rejection_tracker(JSContext *cx, bool mutedErrors, JS::HandleObject promise,
                              JS::PromiseRejectionHandlingState state, void *data) {
  RootedValue promiseVal(cx, JS::ObjectValue(*promise));

  switch (state) {
  case JS::PromiseRejectionHandlingState::Unhandled: {
    if (!JS::SetAdd(cx, unhandledRejectedPromises, promiseVal)) {
      // Note: we unconditionally print these, since they almost always indicate
      // serious bugs.
      fprintf(stderr, "Adding an unhandled rejected promise to the promise "
                      "rejection tracker failed");
    }
    return;
  }
  case JS::PromiseRejectionHandlingState::Handled: {
    bool deleted = false;
    if (!JS::SetDelete(cx, unhandledRejectedPromises, promiseVal, &deleted)) {
      // Note: we unconditionally print these, since they almost always indicate
      // serious bugs.
      fprintf(stderr, "Removing an handled rejected promise from the promise "
                      "rejection tracker failed");
    }
  }
  }
}

bool init_js() {
  JS_Init();

  JSContext *cx = JS_NewContext(JS::DefaultHeapMaxBytes);
  if (!cx) {
    return false;
  }
  if (!js::UseInternalJobQueues(cx) || !JS::InitSelfHostedCode(cx)) {
    return false;
  }

  bool ENABLE_PBL = std::string(std::getenv("ENABLE_PBL")) == "1";
  if (ENABLE_PBL) {
    JS_SetGlobalJitCompilerOption(cx, JSJitCompilerOption::JSJITCOMPILER_PORTABLE_BASELINE_ENABLE,
                                  1);
    JS_SetGlobalJitCompilerOption(
        cx, JSJitCompilerOption::JSJITCOMPILER_PORTABLE_BASELINE_WARMUP_THRESHOLD, 0);
  }

  // TODO: check if we should set a different creation zone.
  JS::RealmOptions options;
  options.creationOptions().setStreamsEnabled(true).setWeakRefsEnabled(
      JS::WeakRefSpecifier::EnabledWithoutCleanupSome);

  JS::DisableIncrementalGC(cx);
  // JS_SetGCParameter(cx, JSGC_MAX_EMPTY_CHUNK_COUNT, 1);

  RootedObject global(
      cx, JS_NewGlobalObject(cx, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
  if (!global) {
    return false;
  }

  JSAutoRealm ar(cx, global);
  if (!JS::InitRealmStandardClasses(cx)) {
    return false;
  }

  JS::SetPromiseRejectionTrackerCallback(cx, rejection_tracker);

  CONTEXT = cx;
  GLOBAL.init(cx, global);
  unhandledRejectedPromises.init(cx, JS::NewSetObject(cx));
  if (!unhandledRejectedPromises) {
    return false;
  }

  builtins::Performance::timeOrigin.emplace(std::chrono::high_resolution_clock::now());

  return true;
}

static bool report_unhandled_promise_rejections(JSContext *cx) {
  RootedValue iterable(cx);
  if (!JS::SetValues(cx, unhandledRejectedPromises, &iterable))
    return false;

  JS::ForOfIterator it(cx);
  if (!it.init(iterable))
    return false;

  RootedValue promise_val(cx);
  RootedObject promise(cx);
  while (true) {
    bool done;
    if (!it.next(&promise_val, &done))
      return false;

    if (done)
      break;

    promise = &promise_val.toObject();
    // Note: we unconditionally print these, since they almost always indicate
    // serious bugs.
    fprintf(stderr, "Promise rejected but never handled: ");
    RootedValue result(cx, JS::GetPromiseResult(promise));
    dump_promise_rejection(cx, result, promise, stderr);
  }

  return true;
}

static void DumpPendingException(JSContext *cx, const char *description) {
  JS::ExceptionStack exception(cx);
  if (!JS::GetPendingExceptionStack(cx, &exception)) {
    fprintf(stderr,
            "Error: exception pending after %s, but got another error "
            "when trying to retrieve it. Aborting.\n",
            description);
  } else {
    fprintf(stderr, "Exception while %s: ", description);
    dump_value(cx, exception.exception(), stderr);
    print_stack(cx, exception.stack(), stderr);
  }
}

static void abort(JSContext *cx, const char *description) {
  // Note: we unconditionally print messages here, since they almost always
  // indicate serious bugs.
  if (JS_IsExceptionPending(cx)) {
    DumpPendingException(cx, description);
  } else {
    fprintf(stderr,
            "Error while %s, but no exception is pending. "
            "Aborting, since that doesn't seem recoverable at all.\n",
            description);
  }

  if (JS::SetSize(cx, unhandledRejectedPromises) > 0) {
    fprintf(stderr, "Additionally, some promises were rejected, but the "
                    "rejection never handled:\n");
    report_unhandled_promise_rejections(cx);
  }

  // Respond with status `500` if no response was ever sent.
  HandleObject fetch_event = builtins::FetchEvent::instance();
  if (hasWizeningFinished() && !builtins::FetchEvent::response_started(fetch_event)) {
    builtins::FetchEvent::respondWithError(cx, fetch_event);
  }

  fflush(stderr);
  exit(1);
}

bool eval_stdin(JSContext *cx, MutableHandleValue result) {
  char *code = NULL;
  size_t len = 0;
  if (getdelim(&code, &len, EOF, stdin) < 0) {
    return false;
  }

  JS::CompileOptions opts(cx);

  // This ensures that we're eagerly loading the sript, and not lazily generating bytecode for
  // functions.
  // https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/js/public/CompileOptions.h#571-574
  opts.setForceFullParse();

  // TODO: investigate passing a filename to Wizer and using that here to
  // improve diagnostics.
  // TODO: furthermore, investigate whether Wizer by now allows us to pass an
  // actual path and open that, instead of having to redirect `stdin` for a
  // subprocess of `js-compute-runtime`.
  opts.setFileAndLine("<stdin>", 1);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, code, strlen(code), JS::SourceOwnership::TakeOwnership)) {
    return false;
  }

  JS::RootedScript script(cx);
  {
    // Disabling GGC during compilation seems to slightly reduce the number of
    // pages touched post-deploy.
    // (Whereas disabling it during execution below meaningfully increases it,
    // which is why this is scoped to just compilation.)
    JS::AutoDisableGenerationalGC noGGC(cx);
    script = JS::Compile(cx, opts, srcBuf);
    if (!script)
      return false;
  }

  // TODO(performance): verify that it's better to perform a shrinking GC here, as manual
  // testing indicates. Running a shrinking GC here causes *fewer* 4kb pages to
  // be written to when processing a request, at least for one fairly large
  // input script.
  //
  // A hypothesis for why this is the case could be that the objects allocated
  // by parsing the script (but not evaluating it) tend to be read-only, so
  // optimizing them for compactness makes sense and doesn't fragment writes
  // later on.
  // https://github.com/fastly/js-compute-runtime/issues/222
  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);

  // Execute the top-level script.
  if (!JS_ExecuteScript(cx, script, result))
    return false;

  // Ensure that any pending promise reactions are run before taking the
  // snapshot.
  while (js::HasJobsPending(cx)) {
    js::RunJobs(cx);

    if (JS_IsExceptionPending(cx))
      abort(cx, "running Promise reactions");
  }

  // Report any promise rejections that weren't handled before snapshotting.
  // TODO: decide whether we should abort in this case, instead of just
  // reporting.
  if (JS::SetSize(cx, unhandledRejectedPromises) > 0) {
    report_unhandled_promise_rejections(cx);
  }

  // TODO(performance): check if it makes sense to increase the empty chunk count *before*
  // running GC like this. The working theory is that otherwise the engine might
  // mark chunk pages as free that then later the allocator doesn't turn into
  // chunks without further fragmentation. But that might be wrong.
  // https://github.com/fastly/js-compute-runtime/issues/223
  // JS_SetGCParameter(cx, JSGC_MAX_EMPTY_CHUNK_COUNT, 10);

  // TODO(performance): verify that it's better to *not* perform a shrinking GC here, as
  // manual testing indicates. Running a shrinking GC here causes *more* 4kb
  // pages to be written to when processing a request, at least for one fairly
  // large input script.
  //
  // A hypothesis for why this is the case could be that most writes are to
  // object kinds that are initially allocated in the same vicinity, but that
  // the shrinking GC causes them to be intermingled with other objects. I.e.,
  // writes become more fragmented due to the shrinking GC.
  // https://github.com/fastly/js-compute-runtime/issues/224
  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, JS::GCOptions::Normal, JS::GCReason::API);

  // Ignore the first GC, but then print all others, because ideally GCs
  // should be rare, and developers should know about them.
  // TODO: consider exposing a way to parameterize this, and/or specifying a
  // dedicated log target for telemetry messages like this.
  JS_SetGCCallback(cx, gc_callback, nullptr);

  return true;
}

static bool addEventListener(JSContext *cx, unsigned argc, Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "addEventListener", 2)) {
    return false;
  }

  auto event_chars = core::encode(cx, args[0]);
  if (!event_chars) {
    return false;
  }

  if (strncmp(event_chars.begin(), "fetch", event_chars.len)) {
    fprintf(stderr,
            "Error: addEventListener only supports the event 'fetch' right now, "
            "but got event '%s'\n",
            event_chars.begin());
    exit(1);
  }

  RootedValue val(cx, args[1]);
  if (!val.isObject() || !JS_ObjectIsFunction(&val.toObject())) {
    fprintf(stderr, "Error: addEventListener: Argument 2 is not a function.\n");
    exit(1);
  }

  return FETCH_HANDLERS->append(&val.toObject());
}

void init() {
  assert(isWizening());

  if (!init_js())
    exit(1);

  JSContext *cx = CONTEXT;
  RootedObject global(cx, GLOBAL);
  JSAutoRealm ar(cx, global);
  FETCH_HANDLERS = new JS::PersistentRootedObjectVector(cx);

  bool ENABLE_EXPERIMENTAL_HIGH_RESOLUTION_TIME_METHODS =
      std::string(std::getenv("ENABLE_EXPERIMENTAL_HIGH_RESOLUTION_TIME_METHODS")) == "1";
  FastlyOptions options;
  options.setExperimentalHighResolutionTimeMethodsEnabled(
      ENABLE_EXPERIMENTAL_HIGH_RESOLUTION_TIME_METHODS);

  define_fastly_sys(cx, global, options);
  if (!JS_DefineFunction(cx, global, "addEventListener", addEventListener, 2, 0))
    exit(1);

  RootedValue result(cx);
  if (!eval_stdin(cx, &result))
    abort(cx, "evaluating JS");

  if (!builtins::FetchEvent::create(cx))
    exit(1);

  if (FETCH_HANDLERS->length() == 0) {
    RootedValue val(cx);
    if (!JS_GetProperty(cx, global, "onfetch", &val) || !val.isObject() ||
        !JS_ObjectIsFunction(&val.toObject())) {
      // The error message only mentions `addEventListener`, even though we also
      // support an `onfetch` top-level function as an alternative. We're
      // treating the latter as undocumented functionality for the time being.
      fprintf(stderr, "Error: no `fetch` event handler registered during initialization. "
                      "Make sure to call `addEventListener('fetch', your_handler)`.\n");
      exit(1);
    }
    if (!FETCH_HANDLERS->append(&val.toObject()))
      abort(cx, "Adding onfetch as a fetch event handler");
  }

  fflush(stdout);
  fflush(stderr);

  // Define this to print a simple memory usage report.
#ifdef MEM_STATS
  dump_mem_stats(cx);
#endif

  markWizeningAsFinished();
}

WIZER_INIT(init);

static void dispatch_fetch_event(JSContext *cx, HandleObject event, double *total_compute) {
  auto pre_handler = system_clock::now();

  builtins::FetchEvent::start_dispatching(event);

  RootedValue result(cx);
  RootedValue event_val(cx, JS::ObjectValue(*event));
  HandleValueArray argsv = HandleValueArray(event_val);
  RootedValue handler(cx);
  RootedValue rval(cx);

  for (size_t i = 0; i < FETCH_HANDLERS->length(); i++) {
    handler.setObject(*(*FETCH_HANDLERS)[i]);
    if (!JS_CallFunctionValue(cx, GLOBAL, handler, argsv, &rval)) {
      DumpPendingException(cx, "dispatching FetchEvent\n");
      break;
    }
    if (builtins::FetchEvent::state(event) != builtins::FetchEvent::State::unhandled) {
      break;
    }
  }

  builtins::FetchEvent::stop_dispatching(event);

  double diff = duration_cast<microseconds>(system_clock::now() - pre_handler).count();
  *total_compute += diff;
  if (debug_logging_enabled())
    printf("Request handler took %fms\n", diff / 1000);
}

static void process_pending_jobs(JSContext *cx, double *total_compute) {
  auto pre_reactions = system_clock::now();
  if (debug_logging_enabled()) {
    printf("Running promise reactions\n");
    fflush(stdout);
  }

  while (js::HasJobsPending(cx)) {
    js::RunJobs(cx);

    if (JS_IsExceptionPending(cx))
      abort(cx, "running Promise reactions");
  }

  double diff = duration_cast<microseconds>(system_clock::now() - pre_reactions).count();
  *total_compute += diff;
  if (debug_logging_enabled())
    printf("Running promise reactions took %fms\n", diff / 1000);
}

static void wait_for_backends(JSContext *cx, double *total_compute) {
  if (!core::EventLoop::has_pending_async_tasks())
    return;

  auto pre_requests = system_clock::now();
  if (debug_logging_enabled()) {
    printf("Waiting for backends ...\n");
    fflush(stdout);
  }

  if (!core::EventLoop::process_pending_async_tasks(cx))
    abort(cx, "processing network requests");

  double diff = duration_cast<microseconds>(system_clock::now() - pre_requests).count();
  if (debug_logging_enabled())
    printf("Done, waited for %fms\n", diff / 1000);
}

bool reactor_main(host_api::Request req) {
  assert(hasWizeningFinished());

  builtins::Performance::timeOrigin.emplace(std::chrono::high_resolution_clock::now());

  double total_compute = 0;
  auto start = system_clock::now();

  __wasilibc_initialize_environ();

  if (debug_logging_enabled()) {
    printf("Running JS handleRequest function for C@E service version %s\n",
           getenv("FASTLY_SERVICE_VERSION"));
    fflush(stdout);
  }

  JSContext *cx = CONTEXT;
  JSAutoRealm ar(cx, GLOBAL);
  js::ResetMathRandomSeed(cx);

  HandleObject fetch_event = builtins::FetchEvent::instance();
  builtins::FetchEvent::init_request(cx, fetch_event, req.req, req.body);

  dispatch_fetch_event(cx, fetch_event, &total_compute);

  // Loop until no more resolved promises or backend requests are pending.
  if (debug_logging_enabled()) {
    printf("Start processing async jobs ...\n");
    fflush(stdout);
  }

  do {
    // First, drain the promise reactions queue.
    process_pending_jobs(cx, &total_compute);

    // Then, check if the fetch event is still active, i.e. had pending promises
    // added to it using `respondWith` or `waitUntil`.
    if (!builtins::FetchEvent::is_active(fetch_event))
      break;

    // Process async tasks.
    wait_for_backends(cx, &total_compute);
  } while (js::HasJobsPending(cx) || core::EventLoop::has_pending_async_tasks());

  if (debug_logging_enabled() && core::EventLoop::has_pending_async_tasks()) {
    fprintf(stderr, "Service terminated with async tasks pending. "
                    "Use FetchEvent#waitUntil to extend the service's lifetime "
                    "if needed.\n");
  }

  if (JS::SetSize(cx, unhandledRejectedPromises) > 0) {
    report_unhandled_promise_rejections(cx);

    // Respond with status `500` if any promise rejections were left unhandled
    // and no response was ever sent.
    if (!builtins::FetchEvent::response_started(fetch_event)) {
      builtins::FetchEvent::respondWithError(cx, fetch_event);
    }
  }

  // Respond with status `500` if an exception is pending
  // and no response was ever sent.
  if (JS_IsExceptionPending(cx)) {
    if (!builtins::FetchEvent::response_started(fetch_event)) {
      builtins::FetchEvent::respondWithError(cx, fetch_event);
    }
  }

  auto end = system_clock::now();
  double diff = duration_cast<microseconds>(end - start).count();
  if (debug_logging_enabled()) {
    printf("Done. Total request processing time: %fms. Total compute time: %fms\n", diff / 1000,
           total_compute / 1000);
  }
  return true;
}
