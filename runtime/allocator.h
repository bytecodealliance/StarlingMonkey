#ifndef JS_COMPUTE_RUNTIME_ALLOCATOR_H
#define JS_COMPUTE_RUNTIME_ALLOCATOR_H

#include <cstddef>

struct JSContext;

/// We need a handle to the JSContext in order to use JS_realloc in the implementation of
/// cabi_realloc. Unfortunately way that we can do this now is to keep the context pointer in a
/// global that can be used there. This global is initialized in js-compute-runtime.cpp.
extern JSContext *CONTEXT;

extern "C" {

/// A strong symbol to override the cabi_realloc defined by wit-bindgen. This version of
/// cabi_realloc uses JS_malloc under the hood.
void *cabi_realloc(void *ptr, size_t orig_size, size_t align, size_t new_size);

/// A more ergonomic version of cabi_realloc for fresh allocations.
inline void *cabi_malloc(size_t bytes, size_t align) { return cabi_realloc(NULL, 0, align, bytes); }

/// Not required by wit-bindgen generated code, but a usefully named version of JS_free that can
/// help with identifying where memory allocated by the c-abi.
void cabi_free(void *ptr);
}

#endif
