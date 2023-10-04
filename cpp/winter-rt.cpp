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
// #include "wizer.h"

// bool WIZENED = false;
// void wizen() { WIZENED = true; }
// WIZER_INIT(wizen);
extern "C" void __wasm_call_ctors();

int main() { return 0; }

core::Engine *engine;

bool exports_wasi_cli_run_run(void) {
  __wasm_call_ctors();
  core::Engine engine = core::Engine();
  if (!builtins::web::add_to_global(engine.cx(), engine.global())) {
    return false;
  }

  js::ResetMathRandomSeed(engine.cx());

  char *code = NULL;
  size_t len = 0;
  if (getdelim(&code, &len, EOF, stdin) < 0) {
    return false;
  }

  RootedValue result(engine.cx());
  if (!engine.eval(code, strlen(code), &result)) {
    engine.dump_value(result, stderr);
    return false;
  }

  if (!engine.run_event_loop(&result)) {
    fprintf(stderr, "Error running event loop: ");
    engine.dump_value(result, stderr);
    return false;
  }
  DBG("2\n");


  //   auto stdout = wasi_cli_stdout_get_stdout();
  //   wasi_io_streams_write_error_t result;
  //   auto list = bindings_list_u8_t{(uint8_t *)(&"Hello, stream!\n"), 14};
  //   wasi_io_streams_write(stdout, &list, &result);
  return true;
}

void exports_wasi_http_incoming_handler_handle(
    wasi_http_incoming_handler_incoming_request_t request,
    wasi_http_incoming_handler_response_outparam_t response_out) {
      std::cout << "Incoming request" << std::endl;
}
