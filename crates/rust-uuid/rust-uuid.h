#ifndef RUST_UUID_H_
#define RUST_UUID_H_

#include "rust-uuid-ffi.h"
#include <memory>

namespace jsuuid {

using UuidPtr = std::unique_ptr<char, decltype(&free_uuid)>;

// Create a UuidPtr
inline UuidPtr make_uuid() { return UuidPtr(new_uuid_v4(), &free_uuid); }

}  // namespace jsuuid

#endif // RUST_UUID_H_
