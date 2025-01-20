#ifndef FETCH_UTILS_H_
#define FETCH_UTILS_H_

#include "builtin.h"

namespace builtins::web::fetch {

// Extracts a valid byte range from the given `Range` header query string, following
// the steps defined for "blob" schemes in the Fetch specification:
// https://fetch.spec.whatwg.org/#scheme-fetch
//
// @param range_query  The raw `Range` header value (e.g. "bytes=0-499").
// @param full_len     The total size of the resource for which the range is requested.
//
// @return An optional tuple `(start, end)` representing the byte range. Returns
//         `std::nullopt` if the range is invalid or cannot be parsed.
std::optional<std::tuple<size_t, size_t>> extract_range(std::string_view range_query, size_t full_len);

}

#endif // FETCH_UTILS_H_
