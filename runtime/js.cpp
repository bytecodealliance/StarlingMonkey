#include <fstream>
#include <iostream>
#include <string>

#include "builtin.h"
#include "exports.h"
#include "extension-api.h"
#include "host_api.h"
#include "js/SourceText.h"
#include "wizer.h"
#ifdef MEM_STATS
#include <string>
#endif

bool WIZENED = false;
extern "C" void __wasm_call_ctors();

api::Engine *engine;
extern bool install_builtins(api::Engine *engine);

#ifdef DEBUG
static bool trap(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  engine->dump_value(args.get(0));
  MOZ_ASSERT(false, "trap function called");
  return false;
}
#endif

bool initialize(const char *filename) {
  auto engine = api::Engine();

  if (!install_builtins(&engine)) {
    return false;
  }

#ifdef DEBUG
  if (!JS_DefineFunction(engine.cx(), engine.global(), "trap", trap, 1, 0)) {
    return false;
  }
#endif

  RootedValue result(engine.cx());
  bool success = engine.eval_toplevel(filename, &result);
  success = success && engine.run_event_loop();

  if (JS_IsExceptionPending(engine.cx())) {
    engine.dump_pending_exception("pre-initializing");
    return false;
  }

  if (!success) {
    fflush(stdout);
    fprintf(stderr, "Internal error");
    return false;
  }

  js::ResetMathRandomSeed(engine.cx());

  return true;
}

extern "C" bool exports_wasi_cli_run_run() {
  __wasm_call_ctors();

  // auto args = bindings_list_string_t{};
  // wasi_cli_0_2_0_rc_2023_10_18_environment_get_arguments(&args);
  // auto [ptr, len] = args.ptr[1];
  // std::string filename(reinterpret_cast<const char *>(ptr), len);

  if (!initialize("filename")) {
    return false;
  }

  return true;
}

__attribute__((weak)) 
int main(int argc, const char *argv[]) {
  printf("Main starting\n");
  return 0;
}

void wizen() {
  std::string filename;
  std::getline(std::cin, filename);

  if (!initialize(filename.c_str())) {
    exit(1);
  }
  markWizeningAsFinished();

  WIZENED = true;
}

WIZER_INIT(wizen);
