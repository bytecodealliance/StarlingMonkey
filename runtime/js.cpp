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

bool initialize(char *script_src, size_t len, const char *filename) {
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
  if (!engine.eval(script_src, len, filename, &result)) {
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

  js::ResetMathRandomSeed(engine.cx());

  return true;
}

// using namespace std::literals;
bool initialize(const std::string &script_path) {
  std::ifstream is(script_path);
  if (!is.is_open()) {
    std::cerr << "Error reading file " << script_path << std::endl;
    return false;
  }

  std::string code;
  std::getline(is, code, '\0');
  auto len = code.size();
  is.seekg(0);
  is.read(&code[0], len);
  return initialize(&code[0], len, script_path.c_str());
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

int main(int argc, const char *argv[]) {
  printf("Main starting\n");
  return 0;
}

void wizen() {
  std::string filename;
  std::getline(std::cin, filename);

  if (!initialize(filename)) {
    exit(1);
  }
  markWizeningAsFinished();

  WIZENED = true;
}

WIZER_INIT(wizen);
