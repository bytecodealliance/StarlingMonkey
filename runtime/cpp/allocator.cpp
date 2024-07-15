
#include "allocator.h"

JSContext *CONTEXT = nullptr;

extern "C" {

__attribute__((weak, export_name("cabi_realloc"))) void *cabi_realloc(void *ptr, size_t orig_size,
                                                                size_t _align, size_t new_size) {
  if (new_size == orig_size) {
    return ptr;
  }
  return JS_realloc(CONTEXT, ptr, orig_size, new_size);
}

void cabi_free(void *ptr) { JS_free(CONTEXT, ptr); }
}
