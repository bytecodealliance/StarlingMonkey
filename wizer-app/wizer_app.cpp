#include "libjs.h"
#include "wizer.h"

#include <cstdio>
#include <iostream>
#include <string>

bool WIZENED = false;
extern "C" void __wasm_call_ctors();

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

  WIZENED = true;
}

WIZER_INIT(wizen);
