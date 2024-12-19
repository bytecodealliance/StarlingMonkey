#ifndef BUILTINS_WEB_CRYPTO_UUID_H
#define BUILTINS_WEB_CRYPTO_UUID_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace crypto {

// FROM RFC 4122
// The version 4 UUID is meant for generating UUIDs from truly-random or
// pseudo-random numbers.
//
// The algorithm is as follows:
//
// Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved to zero and one,
// respectively.
//
// Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field to the
// 4-bit version number from Section 4.1.3.
//
// Set all the other bits to randomly (or pseudo-randomly) chosen values.
std::optional<std::string> random_uuid_v4(JSContext *cx);

} // namespace crypto
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_CRYPTO_UUID_H
