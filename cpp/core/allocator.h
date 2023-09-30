#ifndef JS_COMPUTE_RUNTIME_ALLOCATOR_H
#define JS_COMPUTE_RUNTIME_ALLOCATOR_H

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "js/TypeDecls.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/Utility.h"
#include "jsapi.h"
#pragma clang diagnostic pop

struct JSContext;

/// We need a handle to the JSContext in order to use JS_realloc in the
/// implementation of cabi_realloc. Unfortunately way that we can do this now is
/// to keep the context pointer in a global that can be used there. This global
/// is initialized in js-compute-runtime.cpp.
extern JSContext *CONTEXT;

extern "C" {

/// A strong symbol to override the cabi_realloc defined by wit-bindgen. This
/// version of cabi_realloc uses JS_malloc under the hood.
void *cabi_realloc(void *ptr, size_t orig_size, size_t align, size_t new_size);

/// A more ergonomic version of cabi_realloc for fresh allocations.
inline void *cabi_malloc(size_t bytes, size_t align) {
  return cabi_realloc(NULL, 0, align, bytes);
}

/// Not required by wit-bindgen generated code, but a usefully named version of
/// JS_free that can help with identifying where memory allocated by the c-abi.
void cabi_free(void *ptr);
}

/// A string allocated by the host interface. Holds ownership of the data.
struct HostString final {
  JS::UniqueChars ptr;
  size_t len;

  HostString() = default;
  HostString(std::nullptr_t) : HostString() {}
  HostString(JS::UniqueChars ptr, size_t len) : ptr{std::move(ptr)}, len{len} {}

  HostString(const HostString &other) = delete;
  HostString &operator=(const HostString &other) = delete;

  HostString(HostString &&other) : ptr{std::move(other.ptr)}, len{other.len} {}
  HostString &operator=(HostString &&other) {
    this->ptr.reset(other.ptr.release());
    this->len = other.len;
    return *this;
  }

  using iterator = char *;
  using const_iterator = const char *;

  size_t size() const { return this->len; }

  iterator begin() { return this->ptr.get(); }
  iterator end() { return this->begin() + this->len; }

  const_iterator begin() const { return this->ptr.get(); }
  const_iterator end() const { return this->begin() + this->len; }

  /// Conversion to a bool, testing for an empty pointer.
  operator bool() const { return this->ptr != nullptr; }

  /// Comparison against nullptr
  bool operator==(std::nullptr_t) { return this->ptr == nullptr; }

  /// Comparison against nullptr
  bool operator!=(std::nullptr_t) { return this->ptr != nullptr; }

  /// Conversion to a `std::string_view`.
  operator std::string_view() const {
    return std::string_view(this->ptr.get(), this->len);
  }
};

struct HostBytes final {
  std::unique_ptr<uint8_t[]> ptr;
  size_t len;

  HostBytes() = default;
  HostBytes(std::nullptr_t) : HostBytes() {}
  HostBytes(std::unique_ptr<uint8_t[]> ptr, size_t len)
      : ptr{std::move(ptr)}, len{len} {}

  HostBytes(const HostBytes &other) = delete;
  HostBytes &operator=(const HostBytes &other) = delete;

  HostBytes(HostBytes &&other) : ptr{std::move(other.ptr)}, len{other.len} {}
  HostBytes &operator=(HostBytes &&other) {
    this->ptr.reset(other.ptr.release());
    this->len = other.len;
    return *this;
  }

  /// Allocate a zeroed HostBytes with the given number of bytes.
  static HostBytes with_capacity(size_t len) {
    HostBytes ret;
    ret.ptr = std::make_unique<uint8_t[]>(len);
    ret.len = len;
    return ret;
  }

  using iterator = uint8_t *;
  using const_iterator = const uint8_t *;

  size_t size() const { return this->len; }

  iterator begin() { return this->ptr.get(); }
  iterator end() { return this->begin() + this->len; }

  const_iterator begin() const { return this->ptr.get(); }
  const_iterator end() const { return this->begin() + this->len; }

  /// Conversion to a bool, testing for an empty pointer.
  operator bool() const { return this->ptr != nullptr; }

  /// Comparison against nullptr
  bool operator==(std::nullptr_t) { return this->ptr == nullptr; }

  /// Comparison against nullptr
  bool operator!=(std::nullptr_t) { return this->ptr != nullptr; }

  /// Converstion to a `std::span<uint8_t>`.
  operator std::span<uint8_t>() const {
    return std::span{this->ptr.get(), this->len};
  }
};

#endif
