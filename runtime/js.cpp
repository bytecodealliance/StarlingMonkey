#include <fstream>
#include <iostream>
#include <string>

#include "builtin.h"
#include "exports.h"
#include "extension-api.h"
#include "host_api.h"
#include "wizer.h"
#ifdef MEM_STATS
#include <string>
#endif

extern "C" void __wasm_call_ctors();

api::Engine *engine;

api::Engine* initialize(std::vector<std::string_view> args) {
  auto config = std::make_unique<api::EngineConfig>();
  config->apply_env()->apply_args(args);
  return new api::Engine(std::move(config));
}

static api::Engine *ENGINE = nullptr;

__attribute__((weak))
int main(int argc, const char *argv[]) {
  MOZ_ASSERT_UNREACHABLE("main() should not be called");
}

void wizen() {
  std::string args;
  std::getline(std::cin, args);
  auto config = std::make_unique<api::EngineConfig>();
  config->apply_env()->apply_args(args);
  config->pre_initialize = true;
  ENGINE = new api::Engine(std::move(config));
  ENGINE->finish_pre_initialization();
}

WIZER_INIT(wizen);

extern "C" void __wasm_call_ctors();

/**
 * The main entry function for the runtime.
 *
 * The runtime will be initialized with a configuration derived in the following way:
 * 1. If a command line is provided, it will be parsed and used.
 * 2. Otherwise, the env var `STARLINGMONKEY_CONFIG` will be split into a command line and used.
 * 3. Otherwise, a default configuration is used. In particular, the runtime will attempt to
 *    load the file `./index.js` and run it as the top-level module script.
 */
extern "C" bool exports_wasi_cli_run_run() {
  __wasm_call_ctors();

  auto arg_strings = host_api::environment_get_arguments();
  std::vector<std::string_view> args;
  for (auto& arg : arg_strings) args.push_back(arg);
  auto config = std::make_unique<api::EngineConfig>();
  config->apply_env()->apply_args(args);

  ENGINE = new api::Engine(std::move(config));
  return true;
}

/**
 * Initialize the runtime with the configuration provided via an environment variable.
 *
 * This initializer checks for the environment variable `STARLINGMONKEY_CONFIG` and parses
 * it as a command line arguments string. The variable not being set is treated as an empty
 * command line.
 */
extern "C" bool init_from_environment() {
  __wasm_call_ctors();

  auto config = std::make_unique<api::EngineConfig>();
  config->apply_env();

  ENGINE = new api::Engine(std::move(config));
  return true;
}
