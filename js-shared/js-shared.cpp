#include "../wasi-0.2.0-rc-2023-12-05/bindings/bindings.h"
#include "libjs.h"

int main(int argc, const char *argv[]) {
  return 0;
}

extern "C" bool JS_Initialize(const char* src) {
  const std::string filename(src);

  if (!initialize(filename)) {
    return false;
  }

  return true;
}

extern "C" bool exports_wasi_cli_0_2_0_rc_2023_12_05_run_run() {
  bindings_list_string_t ret;
  wasi_cli_0_2_0_rc_2023_12_05_environment_get_arguments(&ret);
  if (ret.len < 2) {
    printf("no arguments\n");
    return false;
  }

  std::string filename((char*)ret.ptr[1].ptr, ret.ptr[1].len);
  printf("let's run %.*s\n", (int)ret.ptr[1].len, (char*)ret.ptr[1].ptr);
  return initialize(filename);
}
