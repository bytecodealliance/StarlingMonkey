#include "fetch-utils.h"

#include <string>
#include <charconv>

namespace builtins::web::fetch {

std::optional<std::tuple<size_t, size_t>> extract_range(std::string_view range_query, size_t full_len) {
  if (!range_query.starts_with("bytes=")) {
    return std::nullopt;
  }

  range_query.remove_prefix(6); // bytes=
  auto dash_pos = range_query.find('-');

  if (dash_pos == std::string_view::npos) {
    return std::nullopt;
  }

  auto start_str = range_query.substr(0, dash_pos);
  auto end_str = range_query.substr(dash_pos + 1);

  auto to_size = [](std::string_view s) -> std::optional<size_t> {
    size_t v;
    auto [ptr, ec] = std::from_chars(s.begin(), s.end(), v);
    return ec == std::errc() ? std::optional<size_t>(v) : std::nullopt;
  };

  //   5. Let (rangeStart, rangeEnd) be rangeValue.
  auto maybe_start_range = to_size(start_str);
  auto maybe_end_range = to_size(end_str);

  size_t start_range = 0;
  size_t end_range = 0;

  // 6. If rangeStart is null:
  if (!maybe_start_range.has_value()) {
    // If both start_range and end_range are not provided, it's an error.
    if (!maybe_end_range.has_value()) {
      return std::nullopt;
    }

    // 1. Set rangeStart to fullLength - rangeEnd.
    // 2. Set rangeEnd to rangeStart + rangeEnd - 1.
    start_range = full_len - maybe_end_range.value();
    end_range = start_range + maybe_end_range.value() - 1;
    // 7. Otherwise:
  } else {
    // 1. If rangeStart is greater than or equal to fullLength, then return a network error.
    if (maybe_start_range.value() > full_len) {
      return std::nullopt;
    }
    // 2. If rangeEnd is null or rangeEnd is greater than or equal to fullLength, then set
    // rangeEnd to fullLength - 1.
    start_range = maybe_start_range.value();
    end_range = std::min(maybe_end_range.value_or(full_len - 1), full_len - 1);
  }

  return std::optional<std::tuple<size_t, size_t>>{{start_range, end_range}};
}

}
