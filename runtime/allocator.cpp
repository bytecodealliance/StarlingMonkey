
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "jsapi.h"
#pragma clang diagnostic pop

JSContext *CONTEXT = nullptr;

extern "C" {

__attribute__((export_name("cabi_realloc"))) void *cabi_realloc(void *ptr, size_t orig_size,
                                                                size_t align, size_t new_size) {
  return JS_realloc(CONTEXT, ptr, orig_size, new_size);
}

void cabi_free(void *ptr) { JS_free(CONTEXT, ptr); }
}
