#include "bindings.h"
#include "stdio.h"
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

#include "builtins/web/web_builtins.h"
#include "engine.h"
#include "event_loop.h"
#include "host_api.h"
// #include "wizer.h"

// bool WIZENED = false;
// void wizen() { WIZENED = true; }
// WIZER_INIT(wizen);
extern "C" void __wasm_call_ctors();

int main() { return 0; }

core::Engine *engine;

bool exports_wasi_cli_run_run(void) {
  __wasm_call_ctors();

  auto args = bindings_list_string_t{};
  wasi_cli_environment_get_arguments(&args);
  auto namebuf = args.ptr[1];
  std::string filename(reinterpret_cast<const char *>(namebuf.ptr), namebuf.len);
  FILE *fd = fopen(filename.c_str(), "r");

  core::Engine engine = core::Engine();
  if (!builtins::web::add_to_global(engine.cx(), engine.global())) {
    return false;
  }

  js::ResetMathRandomSeed(engine.cx());

  char *code = NULL;
  size_t len = 0;
  if (getdelim(&code, &len, EOF, fd) < 0) {
    return false;
  }

  RootedValue result(engine.cx());
  if (!engine.eval(code, strlen(code), &result)) {
    fflush(stdout);
    if (JS_IsExceptionPending(engine.cx())) {
      engine.dump_pending_exception("Error evaluating code: ");
    }
    return false;
  }

  if (!engine.run_event_loop(&result)) {
    fflush(stdout);
    fprintf(stderr, "Error running event loop: ");
    engine.dump_value(result, stderr);
    return false;
  }
  if (JS_IsExceptionPending(engine.cx())) {
    engine.dump_pending_exception("Error evaluating code: ");
  }

  printf("Done\n");

  return true;
}

void exports_wasi_http_incoming_handler_handle(
    bindings_own_incoming_request_t request,
    bindings_own_response_outparam_t response_out) {
  std::cout << "Incoming request" << std::endl;
}
