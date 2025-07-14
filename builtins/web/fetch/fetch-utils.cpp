#include "fetch-utils.h"
#include "request-response.h"

#include "js/Stream.h"
#include "mozilla/ResultVariant.h"

#include <charconv>
#include <fmt/format.h>
#include <ranges>
#include <string>
#include <string_view>

namespace builtins::web::fetch {

std::string MimeType::to_string() {
  std::string result = essence;

  for (const auto &[key, value] : params) {
    result += fmt::format(";{}={}", key, value);
  }

  return result;
}

std::string_view trim(std::string_view input) {
  auto trim_size = input.find_first_not_of(" \t");
  if (trim_size == std::string_view::npos) {
    return {};
  }

  input.remove_prefix(trim_size);

  trim_size = input.find_last_not_of(" \t");
  input.remove_suffix(input.size() - trim_size - 1);

  return input;
}

std::optional<MimeType> parse_mime_type(std::string_view str) {
  auto input = trim(str);

  if (input.empty()) {
    return std::nullopt;
  }

  std::string essence;
  std::string params;

  if (auto pos = input.find(';'); pos != std::string::npos) {
    essence = trim(input.substr(0, pos));
    params = trim(input.substr(pos + 1));
  } else {
    essence = trim(input);
  }

  if (essence.empty() || essence.find('/') == std::string::npos) {
    return std::nullopt;
  }

  MimeType mime;
  mime.essence = essence;

  if (params.empty()) {
    return mime;
  }

  auto as_string = [](std::string_view v) -> std::string { return {v.data(), v.size()}; };

  for (const auto view : std::views::split(params, ';')) {
    auto param = std::string_view(view.begin(), view.end());

    if (auto pos = param.find('='); pos != std::string::npos) {
      auto key = trim(param.substr(0, pos));
      auto value = trim(param.substr(pos + 1));

      if (!key.empty()) {
        mime.params.push_back({as_string(key), as_string(value)});
      }
    }
  }

  return mime;
}

// https://fetch.spec.whatwg.org/#concept-body-mime-type
mozilla::Result<MimeType, InvalidMimeType> extract_mime_type(std::string_view query) {
  // 1. Let charset be null.
  // 2. Let essence be null.
  // 3. Let mimeType be null.
  std::string essence;
  std::string charset;
  MimeType mime;

  bool found = false;

  // 4. Let values be the result of getting, decoding, and splitting `Content-Type` from headers.
  // 5. If values is null, then return failure.
  // 6. For each value of values:
  for (const auto value : std::views::split(query, ',')) {
    // 1. Let temporaryMimeType be the result of parsing value.
    // 2. If temporaryMimeType is failure or its essence is "*/*", then continue.
    auto value_str = std::string_view(value.begin(), value.end());
    auto maybe_mime = parse_mime_type(value_str);
    if (!maybe_mime || maybe_mime.value().essence == "*/*") {
      continue;
    }

    // 3. Set mimeType to temporaryMimeType.
    mime = maybe_mime.value();
    found = true;

    // 4. If mimeType's essence is not essence, then:
    if (mime.essence != essence) {
      // 1. Set charset to null.
      charset.clear();
      // 2. If mimeTypes parameters["charset"] exists, then set charset to mimeType's
      // parameters["charset"].
      auto it = std::find_if(mime.params.begin(), mime.params.end(),
                             [&](const auto &kv) { return std::get<0>(kv) == "charset"; });

      if (it != mime.params.end()) {
        charset = std::get<1>(*it);
      }

      // 3. Set essence to mimeTypes essence.
      essence = mime.essence;

    } else {
      // 5. Otherwise, if mimeTypes parameters["charset"] does not exist, and charset is non-null,
      //    set mimeType's parameters["charset"] to charset.
      auto it = std::find_if(mime.params.begin(), mime.params.end(),
                             [&](const auto &kv) { return std::get<0>(kv) == "charset"; });

      if (it == mime.params.end() && !charset.empty()) {
        mime.params.push_back({"charset", charset});
      }
    }
  }

  // 7. If mimeType is null, then return failure.
  // 8. Return mimeType.
  return found ? mime : mozilla::Result<MimeType, InvalidMimeType>(InvalidMimeType{});
}

std::optional<std::tuple<size_t, size_t>> extract_range(std::string_view range_query,
                                                        size_t full_len) {
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

// https://fetch.spec.whatwg.org/#abort-fetch
bool abort_fetch(JSContext *cx, HandleObject promise, HandleObject request, HandleObject response, HandleValue error) {
  // 1. Reject promise with error.
  // This is a no-op if promise has already fulfilled.
  JS_SetPendingException(cx, error);

  if (!RejectPromiseWithPendingError(cx, promise)) {
    return false;
  }

  // 2. If request's body is non-null and is readable, then cancel request's body with error.
  if (request && RequestOrResponse::has_body(request)) {
    RootedObject body(cx, RequestOrResponse::body_stream(request));
    MOZ_ASSERT(body);

    if (IsReadableStream(body) && !ReadableStreamCancel(cx, body, error)) {
      return false;
    }
  }

  // 3. If responseObject is null, then return.
  if (!response) {
    return true;
  }

  // 4. Let response be responseObject's response.
  // (Implicit)

  // 5. If response's body is non-null and is readable, then error response's body with error.
  // TODO: implement this
  if (response && RequestOrResponse::has_body(response)) {
    RootedObject body(cx, RequestOrResponse::body_stream(response));
    MOZ_ASSERT(body);

    if (IsReadableStream(body) && !ReadableStreamError(cx, body, error)) {
      return false;
    }
  }
  return true;
}

} // namespace builtins::web::fetch
