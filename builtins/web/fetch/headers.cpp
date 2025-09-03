#include "headers.h"
#include "builtin.h"
#include "decode.h"
#include "encode.h"
#include "fetch-errors.h"
#include "sequence.hpp"

#include "js/Conversions.h"
#include <numeric>

namespace builtins::web::fetch {
namespace {

const std::array<char, 128> VALID_NAME_CHARS = {
    0, 0, 0, 0, 0, 0, 0, 0, //   0
    0, 0, 0, 0, 0, 0, 0, 0, //   8
    0, 0, 0, 0, 0, 0, 0, 0, //  16
    0, 0, 0, 0, 0, 0, 0, 0, //  24

    0, 1, 0, 1, 1, 1, 1, 1, //  32
    0, 0, 1, 1, 0, 1, 1, 0, //  40
    1, 1, 1, 1, 1, 1, 1, 1, //  48
    1, 1, 0, 0, 0, 0, 0, 0, //  56

    0, 1, 1, 1, 1, 1, 1, 1, //  64
    1, 1, 1, 1, 1, 1, 1, 1, //  72
    1, 1, 1, 1, 1, 1, 1, 1, //  80
    1, 1, 1, 0, 0, 0, 1, 1, //  88

    1, 1, 1, 1, 1, 1, 1, 1, //  96
    1, 1, 1, 1, 1, 1, 1, 1, // 104
    1, 1, 1, 1, 1, 1, 1, 1, // 112
    1, 1, 1, 0, 1, 0, 1, 0  // 120
};

host_api::HostString set_cookie_str;

host_api::HttpHeadersReadOnly *get_handle(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  auto *handle =
      JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Handle)).toPrivate();
  return static_cast<host_api::HttpHeadersReadOnly *>(handle);
}

/**
 * Validates and normalizes the given header value, by
 * - stripping leading and trailing whitespace
 * - checking for interior line breaks and `\0`
 *
 * Trim normalization is performed in-place.
 * Returns true if the header value is valid.
 *
 * See
 * https://searchfox.org/mozilla-central/rev/9f76a47f4aa935b49754c5608a1c8e72ee358c46/netwerk/protocol/http/nsHttp.cpp#247-260
 * For details on validation.
 */
bool normalize_header_value(host_api::HostString &value) {
  auto *value_chars = value.begin();

  size_t start = 0;
  size_t end = value.len;

  while (start < end) {
    unsigned char ch = value_chars[start];
    if (ch == '\t' || ch == ' ' || ch == '\r' || ch == '\n') {
      start++;
    } else {
      break;
    }
  }

  while (end > start) {
    unsigned char ch = value_chars[end - 1];
    if (ch == '\t' || ch == ' ' || ch == '\r' || ch == '\n') {
      end--;
    } else {
      break;
    }
  }

  for (size_t i = start; i < end; i++) {
    unsigned char ch = value_chars[i];
    if (ch == '\r' || ch == '\n' || ch == '\0') {
      return false;
    }
  }

  if (start != 0 || end != value.len) {
    memmove(value_chars, value_chars + start, end - start);
    value.len = end - start;
  }

  return true;
}

host_api::HostString normalize_and_validate_header_value(JSContext *cx, HandleValue value_val,
                                                         const char *fun_name) {
  host_api::HostString value = core::encode_byte_string(cx, value_val);
  if (!value.ptr) {
    return value;
  }
  bool valid = normalize_header_value(value);
  if (!valid) {
    // need to coerce to utf8 to report the error value
    JS::RootedString str(cx, JS::ToString(cx, value_val));
    if (!str) {
      return host_api::HostString{};
    }
    auto maybe_utf8 = core::encode(cx, str);
    if (maybe_utf8) {
      api::throw_error(cx, FetchErrors::InvalidHeaderValue, fun_name, maybe_utf8.begin());
    }
    return host_api::HostString{};
  }
  return value;
}

const std::vector<const char *> *forbidden_request_headers;
const std::vector<const char *> *forbidden_response_headers;

enum class Ordering : uint8_t { Less, Equal, Greater };

inline char header_lowercase(const char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

inline Ordering header_compare(const std::string_view a, const std::string_view b) {
  auto it_a = a.begin();
  auto it_b = b.begin();
  while (it_a != a.end() && it_b != b.end()) {
    char ca = header_lowercase(*it_a);
    char cb = header_lowercase(*it_b);
    if (ca < cb) {
      return Ordering::Less;
    }
    if (ca > cb) {
      return Ordering::Greater;
    }
    ++it_a;
    ++it_b;
  }
  if (it_a == a.end()) {
    return it_b == b.end() ? Ordering::Equal : Ordering::Less;
  }
  return Ordering::Greater;
}

struct HeaderCompare {
  bool operator()(const std::string_view a, const std::string_view b) {
    return header_compare(a, b) == Ordering::Less;
  }
};

class HeadersSortListCompare {
  const Headers::HeadersList *headers_;

public:
  HeadersSortListCompare(const Headers::HeadersList *headers) : headers_(headers) {}

  bool operator()(size_t a, size_t b) {
    const auto *header_a = &std::get<0>(headers_->at(a));
    const auto *header_b = &std::get<0>(headers_->at(b));
    return header_compare(*header_a, *header_b) == Ordering::Less;
  }
};

class HeadersSortListLookupCompare {
  const Headers::HeadersList *headers_;

public:
  HeadersSortListLookupCompare(const Headers::HeadersList *headers) : headers_(headers) {}

  bool operator()(size_t a, string_view b) {
    const auto *header_a = &std::get<0>(headers_->at(a));
    return header_compare(*header_a, b) == Ordering::Less;
  }
};

JS::PersistentRooted<JSString *> comma;

bool retrieve_value_for_header_from_handle(JSContext *cx, JS::HandleObject self,
                                           const host_api::HostString &name,
                                           MutableHandleValue value) {
  auto *handle = get_handle(self);
  auto ret = handle->get(name);

  if (const auto *err = ret.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto &values = ret.unwrap();
  if (!values.has_value()) {
    value.setNull();
    return true;
  }

  RootedString res_str(cx);
  RootedString val_str(cx);
  for (auto &str : values.value()) {
    val_str = core::decode_byte_string(cx, str);
    if (!val_str) {
      return false;
    }

    if (!res_str) {
      res_str = val_str;
    } else {
      res_str = JS_ConcatStrings(cx, res_str, comma);
      if (!res_str) {
        return false;
      }
      res_str = JS_ConcatStrings(cx, res_str, val_str);
      if (!res_str) {
        return false;
      }
    }
  }

  value.setString(res_str);
  return true;
}

// for getSetCookie
bool retrieve_values_for_header_from_handle(JSContext *cx, JS::HandleObject self,
                                            const host_api::HostString &name,
                                            JS::MutableHandleObject out_arr) {
  auto *handle = get_handle(self);
  auto ret = handle->get(name);

  if (const auto *err = ret.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto &values = ret.unwrap();
  if (!values.has_value()) {
    return true;
  }

  RootedString val_str(cx);
  size_t i = 0;
  for (auto &str : values.value()) {
    val_str = core::decode_byte_string(cx, str);
    if (!val_str) {
      return false;
    }
    if (!JS_SetElement(cx, out_arr, i, val_str)) {
      return false;
    }
    i++;
  }

  return true;
}

// Get the combined comma-separated value for a given header
bool retrieve_value_for_header_from_list(JSContext *cx, JS::HandleObject self, size_t *index,
                                         JS::MutableHandleValue value, bool is_iterator) {
  MOZ_ASSERT(Headers::is_instance(self));
  Headers::HeadersList *headers_list = Headers::headers_list(self);
  auto *const entry = Headers::get_index(cx, self, *index);
  const host_api::HostString *key = &std::get<0>(*entry);
  const host_api::HostString *val = &std::get<1>(*entry);
  // check if we need to join with the next value if it is the same key, comma-separated
  RootedString str(cx, core::decode_byte_string(cx, *val));
  if (!str) {
    return false;
  }
  // iterator doesn't join set-cookie, only get
  if (is_iterator && header_compare(*key, set_cookie_str) == Ordering::Equal) {
    value.setString(str);
    return true;
  }
  size_t len = headers_list->size();
  while (*index + 1 < len) {
    auto *const entry = Headers::get_index(cx, self, *index + 1);
    const host_api::HostString *next_key = &std::get<0>(*entry);
    if (header_compare(*next_key, *key) != Ordering::Equal) {
      break;
    }
    str = JS_ConcatStrings(cx, str, comma);
    if (!str) {
      return false;
    }
    val = &std::get<1>(*entry);
    RootedString next_str(cx, core::decode_byte_string(cx, *val));
    if (!next_str) {
      return false;
    }
    str = JS_ConcatStrings(cx, str, next_str);
    if (!str) {
      return false;
    }
    *index = *index + 1;
  }
  value.setString(str);
  return true;
}

// Get the array of values for a given header (set-cookie, this is only for set-cookie)
bool retrieve_values_for_header_from_list(JSContext *cx, JS::HandleObject self, size_t index,
                                          JS::MutableHandleObject out_arr) {
  MOZ_ASSERT(Headers::is_instance(self));
  Headers::HeadersList *headers_list = Headers::headers_list(self);
  const host_api::HostString *key = &std::get<0>(*Headers::get_index(cx, self, index));
  const host_api::HostString *val = &std::get<1>(*Headers::get_index(cx, self, index));
  // check if we need to join with the next value if it is the same key
  RootedString str(cx, core::decode_byte_string(cx, *val));
  if (!str) {
    return false;
  }
  size_t i = 0;
  size_t len = headers_list->size();
  if (!JS_SetElement(cx, out_arr, i, str)) {
    return false;
  }
  while (++i < len - index) {
    const host_api::HostString *next_key = &std::get<0>(*Headers::get_index(cx, self, index + i));
    val = &std::get<1>(*Headers::get_index(cx, self, index + i));
    if (header_compare(*next_key, *key) != Ordering::Equal) {
      break;
    }
    str = core::decode_byte_string(cx, *val);
    if (!str) {
      return false;
    }
    if (!JS_SetElement(cx, out_arr, i, str)) {
      return false;
    }
  }
  return true;
}

// Walk through the repeated values for a given header, updating the index
void skip_values_for_header_from_list(JSContext *cx, JS::HandleObject self, size_t *index,
                                      bool is_iterator) {
  MOZ_ASSERT(Headers::is_instance(self));
  Headers::HeadersList *headers_list = Headers::headers_list(self);
  const host_api::HostString *key = &std::get<0>(*Headers::get_index(cx, self, *index));
  size_t len = headers_list->size();
  while (*index + 1 < len) {
    const host_api::HostString *next_key = &std::get<0>(*Headers::get_index(cx, self, *index + 1));
    // iterator doesn't join set-cookie
    if (is_iterator && header_compare(*key, set_cookie_str) == Ordering::Equal) {
      break;
    }
    if (header_compare(*next_key, *key) != Ordering::Equal) {
      break;
    }
    *index = *index + 1;
  }
}

bool validate_guard(JSContext *cx, HandleObject self, string_view header_name, const char *fun_name,
                    bool *is_valid) {
  MOZ_ASSERT(Headers::is_instance(self));
  *is_valid = false;

  Headers::HeadersGuard guard = Headers::guard(self);
  switch (guard) {
  case Headers::HeadersGuard::None:
    *is_valid = true;
    return true;
  case Headers::HeadersGuard::Immutable:
    return api::throw_error(cx, FetchErrors::HeadersImmutable, fun_name);
  case Headers::HeadersGuard::Request:
    for (const auto *forbidden_header_name : *forbidden_request_headers) {
      if (header_compare(header_name, forbidden_header_name) == Ordering::Equal) {
        *is_valid = false;
        return true;
      }
    }
    *is_valid = true;
    return true;
  case Headers::HeadersGuard::Response:
    for (const auto *forbidden_header_name : *forbidden_response_headers) {
      if (header_compare(header_name, forbidden_header_name) == Ordering::Equal) {
        *is_valid = false;
        return true;
      }
    }
    *is_valid = true;
    return true;
  default:
    MOZ_ASSERT_UNREACHABLE();
  }
}

// Update the sort list
void ensure_updated_sort_list(const Headers::HeadersList *headers_list,
                              std::vector<size_t> *headers_sort_list) {
  MOZ_ASSERT(headers_list);
  MOZ_ASSERT(headers_sort_list);
  // Empty length means we need to recompute.
  if (headers_sort_list->empty()) {
    headers_sort_list->resize(headers_list->size());
    std::iota(headers_sort_list->begin(), headers_sort_list->end(), 0);
    std::sort(headers_sort_list->begin(), headers_sort_list->end(),
              HeadersSortListCompare(headers_list));
  }

  MOZ_ASSERT(headers_sort_list->size() == headers_list->size());
}

// Clear the sort list, marking it as mutated so it will be recomputed on the next lookup.
void mark_for_sort(JS::HandleObject self) {
  MOZ_ASSERT(Headers::is_instance(self));
  std::vector<size_t> *headers_sort_list = Headers::headers_sort_list(self);
  headers_sort_list->clear();
}

bool append_valid_normalized_header(JSContext *cx, HandleObject self, string_view header_name,
                                    string_view header_val) {
  Headers::Mode mode = Headers::mode(self);
  if (mode == Headers::Mode::HostOnly) {
    auto *handle = get_handle(self)->as_writable();
    MOZ_ASSERT(handle);
    auto res = handle->append(header_name, header_val);
    if (const auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  } else {
    MOZ_ASSERT(mode == Headers::Mode::ContentOnly);

    Headers::HeadersList *list = Headers::headers_list(self);

    list->emplace_back(host_api::HostString(header_name), host_api::HostString(header_val));
    // add the new index to the sort list for sorting
    mark_for_sort(self);
  }

  return true;
}

bool switch_mode(JSContext *cx, HandleObject self, const Headers::Mode mode) {
  auto current_mode = Headers::mode(self);
  if (mode == current_mode) {
    return true;
  }

  if (current_mode == Headers::Mode::Uninitialized) {
    MOZ_ASSERT(mode == Headers::Mode::ContentOnly);
    MOZ_ASSERT(JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersList))
                   .toPrivate() == nullptr);
    MOZ_ASSERT(JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersSortList))
                   .toPrivate() == nullptr);

    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersList),
                    PrivateValue(js_new<Headers::HeadersList>()));
    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersSortList),
                    PrivateValue(js_new<std::vector<size_t>>()));
    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Mode),
                    JS::Int32Value(static_cast<int32_t>(Headers::Mode::ContentOnly)));

    return true;
  }

  if (current_mode == Headers::Mode::ContentOnly) {
    MOZ_ASSERT(mode == Headers::Mode::CachedInContent,
               "Switching from ContentOnly to HostOnly is wasteful and not implemented");

    Headers::HeadersList *list = Headers::headers_list(self);

    auto handle = host_api::HttpHeaders::FromEntries(*list);
    if (handle.is_err()) {
      return api::throw_error(cx, FetchErrors::HeadersCloningFailed);
    }
    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Handle), PrivateValue(handle.unwrap()));
  }

  if (current_mode == Headers::Mode::HostOnly) {
    MOZ_ASSERT(mode == Headers::Mode::CachedInContent);
    MOZ_ASSERT(JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersList))
                   .toPrivate() == nullptr);
    MOZ_ASSERT(JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersSortList))
                   .toPrivate() == nullptr);

    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersList),
                    PrivateValue(js_new<Headers::HeadersList>()));
    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersSortList),
                    PrivateValue(js_new<std::vector<size_t>>()));
    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Mode),
                    JS::Int32Value(static_cast<int32_t>(Headers::Mode::ContentOnly)));

    auto *handle = get_handle(self);
    MOZ_ASSERT(handle);

    auto res = handle->entries();
    if (res.is_err()) {
      HANDLE_ERROR(cx, *res.to_err());
      return false;
    }

    Headers::HeadersList *list = Headers::headers_list(self);
    for (auto &entry : std::move(res.unwrap())) {
      list->emplace_back(std::move(std::get<0>(entry)), std::move(std::get<1>(entry)));
    }
  }

  if (mode == Headers::Mode::ContentOnly) {
    MOZ_ASSERT(current_mode == Headers::Mode::CachedInContent);
    auto *handle = get_handle(self);
    delete handle;
    SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Handle), PrivateValue(nullptr));
  }

  SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Mode),
                  JS::Int32Value(static_cast<int32_t>(mode)));
  return true;
}

bool prepare_for_entries_modification(JSContext *cx, JS::HandleObject self) {
  auto mode = Headers::mode(self);
  if (mode == Headers::Mode::HostOnly) {
    auto *handle = get_handle(self);
    if (!handle->is_writable()) {
      auto *new_handle = handle->clone();
      if (!new_handle) {
        return api::throw_error(cx, FetchErrors::HeadersCloningFailed);
      }
      delete handle;
      SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Handle), PrivateValue(new_handle));
    }
  } else if (mode == Headers::Mode::CachedInContent || mode == Headers::Mode::Uninitialized) {
    if (!switch_mode(cx, self, Headers::Mode::ContentOnly)) {
      return false;
    }
  }
  // bump the generation integer
  uint32_t gen = JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Gen)).toInt32();
  if (gen != INT32_MAX) {
    JS::SetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Gen), JS::Int32Value(gen + 1));
  }
  return true;
}

} // namespace

Headers::HeadersList *Headers::headers_list(JSObject *self) {
  auto *list = static_cast<Headers::HeadersList *>(
      JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersList)).toPrivate());
  MOZ_ASSERT(list);
  return list;
}

Headers::HeadersSortList *Headers::headers_sort_list(JSObject *self) {
  auto *list = static_cast<Headers::HeadersSortList *>(
      JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::HeadersSortList)).toPrivate());
  MOZ_ASSERT(list);
  return list;
}

Headers::Mode Headers::mode(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  Value modeVal = JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::Mode));
  if (modeVal.isUndefined()) {
    return Headers::Mode::Uninitialized;
  }
  return static_cast<Mode>(modeVal.toInt32());
}

Headers::HeadersGuard Headers::guard(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  Value modeVal = JS::GetReservedSlot(self, static_cast<size_t>(Headers::Slots::Guard));
  return static_cast<Headers::HeadersGuard>(modeVal.toInt32());
}

/**
 * Validates the given header name, by checking for invalid characters
 *
 * See
 * https://searchfox.org/mozilla-central/rev/9f76a47f4aa935b49754c5608a1c8e72ee358c46/netwerk/protocol/http/nsHttp.cpp#172-215
 * For details on validation.
 */
host_api::HostString Headers::validate_header_name(JSContext *cx, HandleValue name_val,
                                                   const char *fun_name) {
  JS::RootedString name_str(cx, JS::ToString(cx, name_val));
  if (!name_str) {
    return host_api::HostString{};
  }

  host_api::HostString name = core::encode(cx, name_str);
  if (!name) {
    return host_api::HostString{};
  }

  if (name.len == 0) {
    api::throw_error(cx, FetchErrors::EmptyHeaderName, fun_name);
    return host_api::HostString{};
  }

  char *name_chars = name.begin();
  for (size_t i = 0; i < name.len; i++) {
    const unsigned char ch = name_chars[i];
    if (ch > 127 || (VALID_NAME_CHARS.at(ch) == 0)) {
      api::throw_error(cx, FetchErrors::InvalidHeaderName, fun_name, name_chars);
      return host_api::HostString{};
    }
  }

  return name;
}

JSObject *Headers::create(JSContext *cx, HeadersGuard guard) {
  JSObject *self = JS_NewObjectWithGivenProto(cx, &class_, proto_obj);
  if (!self) {
    return nullptr;
  }

  SetReservedSlot(self, static_cast<uint32_t>(Slots::Guard),
                  JS::Int32Value(static_cast<int32_t>(guard)));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Mode),
                  JS::Int32Value(static_cast<int32_t>(Mode::Uninitialized)));

  SetReservedSlot(self, static_cast<uint32_t>(Slots::HeadersList), PrivateValue(nullptr));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::HeadersSortList), PrivateValue(nullptr));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Gen), JS::Int32Value(0));
  return self;
}

JSObject *Headers::create(JSContext *cx, host_api::HttpHeadersReadOnly *handle,
                          HeadersGuard guard) {
  RootedObject self(cx, create(cx, guard));
  if (!self) {
    return nullptr;
  }

  MOZ_ASSERT(Headers::mode(self) == Headers::Mode::Uninitialized);
  SetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Mode),
                  JS::Int32Value(static_cast<int32_t>(Headers::Mode::HostOnly)));
  SetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Handle), PrivateValue(handle));
  return self;
}

JSObject *Headers::create(JSContext *cx, HandleValue init_headers, HeadersGuard guard) {
  RootedObject self(cx, create(cx, guard));
  if (!self) {
    return nullptr;
  }
  if (!init_entries(cx, self, init_headers)) {
    return nullptr;
  }
  MOZ_ASSERT(mode(self) == Headers::Mode::ContentOnly ||
             mode(self) == Headers::Mode::Uninitialized);
  return self;
}

bool Headers::init_entries(JSContext *cx, HandleObject self, HandleValue initv) {
  // TODO: check if initv is a Headers instance and clone its handle if so.
  // TODO: But note: forbidden headers have to be applied correctly.
  bool consumed = false;
  if (!core::maybe_consume_sequence_or_record<host_api::HostString, validate_header_name,
                                              append_valid_header>(cx, initv, self, &consumed,
                                                                   "Headers")) {
    return false;
  }

  if (!consumed) {
    api::throw_error(cx, api::Errors::InvalidSequence, "Headers", "");
    return false;
  }

  return true;
}

uint32_t Headers::get_generation(JSObject *self) {
  return JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Gen)).toInt32();
}

bool Headers::get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  auto name_chars = validate_header_name(cx, args[0], "Headers.get");
  if (!name_chars) {
    return false;
  }

  Mode mode = Headers::mode(self);
  if (mode == Headers::Mode::Uninitialized) {
    args.rval().setNull();
    return true;
  }

  if (mode == Mode::HostOnly) {
    return retrieve_value_for_header_from_handle(cx, self, name_chars, args.rval());
  }

  auto idx = Headers::lookup(cx, self, name_chars);
  if (!idx) {
    args.rval().setNull();
    return true;
  }

  if (!retrieve_value_for_header_from_list(cx, self, &idx.value(), args.rval(), false)) {
    return false;
  }

  return true;
}

bool Headers::getSetCookie(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  JS::RootedObject out_arr(cx, JS::NewArrayObject(cx, 0));
  args.rval().setObject(*out_arr);

  Mode mode = Headers::mode(self);
  if (mode == Headers::Mode::Uninitialized) {
    return true;
}

  if (mode == Mode::HostOnly) {
    if (!retrieve_values_for_header_from_handle(cx, self, set_cookie_str, &out_arr)) {
      return false;
}
  } else {
    auto idx = Headers::lookup(cx, self, set_cookie_str);
    if (idx && !retrieve_values_for_header_from_list(cx, self, idx.value(), &out_arr)) {
      return false;
}
  }

  return true;
}

bool Headers::set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  auto name_chars = validate_header_name(cx, args[0], "Headers.set");
  if (!name_chars) {
    return false;
  }

  auto value_chars = normalize_and_validate_header_value(cx, args[1], "headers.set");
  if (!value_chars.ptr) {
    return false;
  }

  bool is_valid = false;
  if (!validate_guard(cx, self, name_chars, "Headers.append", &is_valid)) {
    return false;
  }

  if (!is_valid) {
    args.rval().setUndefined();
    return true;
  }

  if (!prepare_for_entries_modification(cx, self)) {
    return false;
  }

  Mode mode = Headers::mode(self);
  if (mode == Mode::HostOnly) {
    auto *handle = get_handle(self)->as_writable();
    MOZ_ASSERT(handle);
    auto res = handle->set(name_chars, value_chars);
    if (const auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  } else {
    MOZ_ASSERT(mode == Mode::ContentOnly);

    auto idx = Headers::lookup(cx, self, name_chars);
    if (!idx) {
      args.rval().setUndefined();
      return append_valid_normalized_header(cx, self, std::move(name_chars), std::move(value_chars));
    }

    size_t index = idx.value();
    // The lookup above will guarantee that sort_list is up to date.
    std::vector<size_t> *headers_sort_list = Headers::headers_sort_list(self);
    HeadersList *headers_list = Headers::headers_list(self);

    // Update the first entry in place to the new value
    host_api::HostString *header_val =
        &std::get<1>(headers_list->at(headers_sort_list->at(index)));

    // Swap in the new value respecting the disposal semantics
    header_val->ptr.swap(value_chars.ptr);
    header_val->len = value_chars.len;

    // Delete all subsequent entries for this header excluding the first,
    // as a variation of Headers::delete.
    size_t len = headers_list->size();
    size_t delete_cnt = 0;

    while (true) {
      size_t next_index = index + delete_cnt + 1;
      if (next_index >= len) {
        break;
      }

      size_t sorted_pos = headers_sort_list->at(next_index);
      if (sorted_pos < delete_cnt) {
        break;
      }

      size_t actual_pos = sorted_pos - delete_cnt;
      const auto& header_name = std::get<0>(headers_list->at(actual_pos));

      if (header_compare(header_name, name_chars) != Ordering::Equal) {
        break;
      }

      headers_list->erase(headers_list->begin() + actual_pos);
      delete_cnt++;
    }

    // Reset the sort list if we performed additional deletions.
    if (delete_cnt > 0) {
      headers_sort_list->clear();
    }
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::has(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  auto name_chars = validate_header_name(cx, args[0], "Headers.has");
  if (!name_chars) {
    return false;
  }

  Mode mode = Headers::mode(self);
  if (mode == Mode::Uninitialized) {
    args.rval().setBoolean(false);
    return true;
  }

  if (mode == Mode::HostOnly) {
    auto *handle = get_handle(self);
    MOZ_ASSERT(handle);
    auto res = handle->has(name_chars);
    MOZ_ASSERT(!res.is_err());
    args.rval().setBoolean(res.unwrap());
  } else {
    args.rval().setBoolean(Headers::lookup(cx, self, name_chars).has_value());
  }

  return true;
}

bool Headers::append(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  auto name_chars = validate_header_name(cx, args[0], "Headers.append");
  if (!name_chars) {
    return false;
  }

  auto value_chars = normalize_and_validate_header_value(cx, args[1], "Headers.append");
  if (!value_chars) {
    return false;
  }

  bool is_valid = false;
  if (!validate_guard(cx, self, name_chars, "Headers.append", &is_valid)) {
    return false;
  }

  if (!is_valid) {
    args.rval().setUndefined();
    return true;
  }

  // name casing must come from existing name match if there is one.
  auto idx = Headers::lookup(cx, self, name_chars);

  if (!prepare_for_entries_modification(cx, self)) {
    return false;
  }

  if (!idx) {
    args.rval().setUndefined();
    return append_valid_normalized_header(cx, self, std::move(name_chars), std::move(value_chars));
  }

  // set-cookie doesn't combine
  if (header_compare(name_chars, set_cookie_str) == Ordering::Equal) {
    return append_valid_normalized_header(cx, self, std::get<0>(*Headers::get_index(cx, self, idx.value())), std::move(value_chars));
  }

  // walk to the last name if multiple to do the combining into
  size_t index = idx.value();
  skip_values_for_header_from_list(cx, self, &index, false);
  host_api::HostString *header_val = &std::get<1>(*Headers::get_index(cx, self, index));
  size_t combined_len = header_val->len + value_chars.len + 2;
  auto combined = JS::UniqueChars(static_cast<char *>(js_malloc(combined_len)));
  memcpy(combined.get(), header_val->ptr.get(), header_val->len);
  memcpy(combined.get() + header_val->len, ", ", 2);
  memcpy(combined.get() + header_val->len + 2, value_chars.ptr.get(), value_chars.len);
  header_val->ptr.swap(combined);
  header_val->len = combined_len;

  args.rval().setUndefined();
  return true;
}


bool Headers::set_valid_if_undefined(JSContext *cx, HandleObject self, string_view name,
                                     string_view value) {
  if (!prepare_for_entries_modification(cx, self)) {
    return false;
}

  if (mode(self) == Mode::HostOnly) {
    auto *handle = get_handle(self)->as_writable();
    auto has = handle->has(name);
    MOZ_ASSERT(!has.is_err());
    if (has.unwrap()) {
      return true;
    }

    auto res = handle->append(name, value);
    if (const auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
    return true;
  }

  MOZ_ASSERT(mode(self) == Mode::ContentOnly);
  if (Headers::lookup(cx, self, name)) {
    return true;
  }

  return append_valid_normalized_header(cx, self, name, value);
}

bool Headers::delete_(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(1, "delete")

  auto name_chars = validate_header_name(cx, args[0], "Headers.delete");
  if (!name_chars) {
    return false;
  }

  bool is_valid = false;
  if (!validate_guard(cx, self, name_chars, "Headers.delete", &is_valid)) {
    return false;
  }

  if (!is_valid) {
    args.rval().setUndefined();
    return true;
  }

  if (!prepare_for_entries_modification(cx, self)) {
    return false;
  }

  Mode mode = Headers::mode(self);
  if (mode == Mode::HostOnly) {
    auto *handle = get_handle(self)->as_writable();
    MOZ_ASSERT(handle);
    std::string_view name = name_chars;
    auto res = handle->remove(name);
    if (const auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
    args.rval().setUndefined();
    return true;
  }

  MOZ_ASSERT(mode == Mode::ContentOnly);

  auto idx = Headers::lookup(cx, self, name_chars);
  if (!idx) {
    args.rval().setUndefined();
    return true;
  }

  size_t index = idx.value();
  // The lookup above will guarantee that sort_list is up to date.
  std::vector<size_t> *headers_sort_list = Headers::headers_sort_list(self);
  HeadersList *headers_list = Headers::headers_list(self);

  // Delete all case-insensitively equal names.
  // The ordering guarantee for sort_list is that equal names will come later in headers_list
  // so that we can continue to use sort list during the delete operation, only recomputing it
  // after.
  size_t delete_cnt = 0;
  size_t len = headers_sort_list->size();

  while (true) {
    size_t current_index = index + delete_cnt;

    if (current_index >= len) {
      break;
    }

    size_t sorted_pos = headers_sort_list->at(current_index);
    if (sorted_pos < delete_cnt) {
      break;
    }

    size_t actual_pos = sorted_pos - delete_cnt;
    const auto& header_name = std::get<0>(headers_list->at(actual_pos));

    if (header_compare(header_name, name_chars) != Ordering::Equal) {
      break;
    }

    headers_list->erase(headers_list->begin() + actual_pos);
    delete_cnt++;
  }

  headers_sort_list->clear();

  args.rval().setUndefined();
  return true;
}

bool Headers::append_valid_header(JSContext *cx, JS::HandleObject self,
                                  host_api::HostString valid_key, JS::HandleValue value,
                                  const char *fun_name) {
  bool is_valid = false;
  if (!validate_guard(cx, self, valid_key, "Headers constructor", &is_valid)) {
    return false;
  }

  if (!is_valid) {
    return true;
  }

  auto value_chars = normalize_and_validate_header_value(cx, value, fun_name);
  if (!value_chars.ptr) {
    return false;
  }

  if (!prepare_for_entries_modification(cx, self)) {
    return false;
  }

  // name casing must come from existing name match if there is one.
  auto idx = Headers::lookup(cx, self, valid_key);

  if (idx) {
    return append_valid_normalized_header(
        cx, self, std::get<0>(*Headers::get_index(cx, self, idx.value())), value_chars);
  }

  return append_valid_normalized_header(cx, self, valid_key, value_chars);
}

const JSFunctionSpec Headers::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Headers::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Headers::methods[] = {
    JS_FN("append", Headers::append, 2, JSPROP_ENUMERATE),
    JS_FN("delete", Headers::delete_, 1, JSPROP_ENUMERATE),
    JS_FN("entries", Headers::entries, 0, JSPROP_ENUMERATE),
    JS_FN("forEach", Headers::forEach, 1, JSPROP_ENUMERATE),
    JS_FN("get", Headers::get, 1, JSPROP_ENUMERATE),
    JS_FN("getSetCookie", Headers::getSetCookie, 0, JSPROP_ENUMERATE),
    JS_FN("has", Headers::has, 1, JSPROP_ENUMERATE),
    JS_FN("keys", Headers::keys, 0, JSPROP_ENUMERATE),
    JS_FN("set", Headers::set, 2, JSPROP_ENUMERATE),
    JS_FN("values", Headers::values, 0, JSPROP_ENUMERATE),
    // [Symbol.iterator] added in init_class.
    JS_FS_END,
};

const JSPropertySpec Headers::properties[] = {
    JS_PS_END,
};

bool Headers::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("Headers", 0);
  HandleValue headersInit = args.get(0);
  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Guard),
                  JS::Int32Value(static_cast<int32_t>(HeadersGuard::None)));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::HeadersList), PrivateValue(nullptr));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::HeadersSortList), PrivateValue(nullptr));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Gen), JS::Int32Value(0));

  // walk the headers list writing in the ordered normalized case headers (distinct from the wire)
  if (!init_entries(cx, self, headersInit)) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

void Headers::finalize(JS::GCContext *gcx, JSObject *self) {
  auto *list = static_cast<HeadersList *>(
      JS::GetReservedSlot(self, static_cast<size_t>(Slots::HeadersList)).toPrivate());
  if (list != nullptr) {
    list->clear();
    js_delete(list);
  }
  auto *sort_list = static_cast<HeadersSortList *>(
      JS::GetReservedSlot(self, static_cast<size_t>(Slots::HeadersSortList)).toPrivate());
  if (sort_list != nullptr) {
    sort_list->clear();
    js_delete(sort_list);
  }
}

bool Headers::init_class(JSContext *cx, JS::HandleObject global) {
  // get the host forbidden headers for guard checks
  forbidden_request_headers = &host_api::HttpHeaders::get_forbidden_request_headers();
  forbidden_response_headers = &host_api::HttpHeaders::get_forbidden_response_headers();

  // sort the forbidden headers with the lowercase-invariant comparator
  MOZ_RELEASE_ASSERT(std::is_sorted(forbidden_request_headers->begin(),
                                    forbidden_request_headers->end(), HeaderCompare()),
                     "Forbidden request headers must be sorted");
  MOZ_RELEASE_ASSERT(std::is_sorted(forbidden_response_headers->begin(),
                                    forbidden_response_headers->end(), HeaderCompare()),
                     "Forbidden response headers must be sorted");

  if (!init_class_impl(cx, global)) {
    return false;
  }

  set_cookie_str = host_api::HostString("set-cookie");

  auto *comma_str = JS_NewStringCopyN(cx, ", ", 2);
  if (!comma_str) {
    return false;
  }
  comma.init(cx, comma_str);

  if (!HeadersIterator::init_class(cx, global)) {
    return false;
  }

  JS::RootedValue entries(cx);
  if (!JS_GetProperty(cx, proto_obj, "entries", &entries)) {
    return false;
  }

  JS::SymbolCode code = JS::SymbolCode::iterator;
  JS::RootedId iteratorId(cx, JS::GetWellKnownSymbolKey(cx, code));
  return JS_DefinePropertyById(cx, proto_obj, iteratorId, entries, 0);
}

Headers::HeadersList *Headers::get_list(JSContext *cx, HandleObject self) {
  MOZ_ASSERT(is_instance(self));
  if (mode(self) == Mode::Uninitialized && !switch_mode(cx, self, Mode::ContentOnly)) {
    return nullptr;
  }
  if (mode(self) == Mode::HostOnly && !switch_mode(cx, self, Mode::CachedInContent)) {
    return nullptr;
  }
  return headers_list(self);
}

unique_ptr<host_api::HttpHeaders> Headers::handle_clone(JSContext *cx, HandleObject self) {
  auto mode = Headers::mode(self);

  // If this instance uninitialized, return an empty handle without initializing this instance.
  if (mode == Mode::Uninitialized) {
    return std::make_unique<host_api::HttpHeaders>();
  }

  if (mode == Mode::ContentOnly && !switch_mode(cx, self, Mode::CachedInContent)) {
    // Switch to Mode::CachedInContent to ensure that the latest data is available on the handle,
    // but without discarding the existing entries, in case content reads them later.
    return nullptr;
  }

  auto handle = unique_ptr<host_api::HttpHeaders>(get_handle(self)->clone());
  if (!handle) {
    api::throw_error(cx, FetchErrors::HeadersCloningFailed);
    return nullptr;
  }
  return handle;
}

BUILTIN_ITERATOR_METHODS(Headers)

// Headers Iterator
JSObject *HeadersIterator::create(JSContext *cx, HandleObject headers, uint8_t type) {
  JSObject *self = JS_NewObjectWithGivenProto(cx, &class_, proto_obj);
  if (!self) {
    return nullptr;
  }
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Type),
                  JS::Int32Value(static_cast<int32_t>(type)));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Cursor), JS::Int32Value(0));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Headers), JS::ObjectValue(*headers));
  return self;
}

const JSFunctionSpec HeadersIterator::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec HeadersIterator::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec HeadersIterator::methods[] = {
    JS_FN("next", HeadersIterator::next, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec HeadersIterator::properties[] = {
    JS_PS_END,
};

bool HeadersIterator::init_class(JSContext *cx, JS::HandleObject global) {
  JS::RootedObject iterator_proto(cx, JS::GetRealmIteratorPrototype(cx));
  if (!iterator_proto) {
    return false;
  }

  if (!init_class_impl(cx, global, iterator_proto)) {
    return false;
  }

  // Delete both the `HeadersIterator` global property and the
  // `constructor` property on `HeadersIterator.prototype`. The latter
  // because Iterators don't have their own constructor on the prototype.
  return JS_DeleteProperty(cx, global, class_.name) &&
         JS_DeleteProperty(cx, proto_obj, "constructor");
}

std::tuple<host_api::HostString, host_api::HostString> *
Headers::get_index(JSContext *cx, JS::HandleObject self, size_t index) {
  MOZ_ASSERT(is_instance(self));
  std::vector<size_t> *headers_sort_list = Headers::headers_sort_list(self);
  HeadersList *headers_list = Headers::get_list(cx, self);

  ensure_updated_sort_list(headers_list, headers_sort_list);
  MOZ_RELEASE_ASSERT(index < headers_sort_list->size());

  return &headers_list->at(headers_sort_list->at(index));
}

std::optional<size_t> Headers::lookup(JSContext *cx, HandleObject self, string_view key) {
  MOZ_ASSERT(is_instance(self));
  const HeadersList *headers_list = Headers::get_list(cx, self);
  std::vector<size_t> *headers_sort_list = Headers::headers_sort_list(self);

  ensure_updated_sort_list(headers_list, headers_sort_list);

  // Now we know its sorted, we can binary search.
  auto it = std::lower_bound(headers_sort_list->begin(), headers_sort_list->end(), key,
                             HeadersSortListLookupCompare(headers_list));
  if (it == headers_sort_list->end() ||
      header_compare(std::get<0>(headers_list->at(*it)), key) != Ordering::Equal) {
    return std::nullopt;
  }
  return it - headers_sort_list->begin();
}

bool HeadersIterator::next(JSContext *cx, unsigned argc, Value *vp) {
  METHOD_HEADER(0)
  JS::RootedObject headers(cx, &JS::GetReservedSlot(self, Slots::Headers).toObject());

  Headers::HeadersList *list = Headers::get_list(cx, headers);

  size_t index = JS::GetReservedSlot(self, Slots::Cursor).toInt32();
  size_t len = list->size();
  auto type = static_cast<uint8_t>(JS::GetReservedSlot(self, Slots::Type).toInt32());

  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) {
    return false;
  }

  if (index >= len) {
    JS_DefineProperty(cx, result, "done", JS::TrueHandleValue, JSPROP_ENUMERATE);
    JS_DefineProperty(cx, result, "value", JS::UndefinedHandleValue, JSPROP_ENUMERATE);

    args.rval().setObject(*result);
    return true;
  }

  JS_DefineProperty(cx, result, "done", JS::FalseHandleValue, JSPROP_ENUMERATE);

  JS::RootedValue key_val(cx);
  JS::RootedValue val_val(cx);

  if (type != ITER_TYPE_VALUES) {
    const host_api::HostString *key = &std::get<0>(*Headers::get_index(cx, headers, index));
    size_t len = key->len;
    auto chars = JS::UniqueLatin1Chars(static_cast<JS::Latin1Char *>(js_malloc(len)));
    for (int i = 0; i < len; ++i) {
      const unsigned char ch = key->ptr[i];
      // headers should already be validated by here
      MOZ_ASSERT(ch <= 127 && VALID_NAME_CHARS.at(ch));
      // we store header keys with casing, so getter itself lowercases
      if (ch >= 'A' && ch <= 'Z') {
        chars[i] = ch - 'A' + 'a';
      } else {
        chars[i] = ch;
      }
    }
    key_val = JS::StringValue(JS_NewLatin1String(cx, std::move(chars), len));
  }

  if (type != ITER_TYPE_KEYS) {
    if (!retrieve_value_for_header_from_list(cx, headers, &index, &val_val, true)) {
      return false;
    }
  } else {
    skip_values_for_header_from_list(cx, headers, &index, true);
  }

  JS::RootedValue result_val(cx);

  switch (type) {
  case ITER_TYPE_ENTRIES: {
    JS::RootedObject pair(cx, JS::NewArrayObject(cx, 2));
    if (!pair) {
      return false;
    }
    JS_DefineElement(cx, pair, 0, key_val, JSPROP_ENUMERATE);
    JS_DefineElement(cx, pair, 1, val_val, JSPROP_ENUMERATE);
    result_val = JS::ObjectValue(*pair);
    break;
  }
  case ITER_TYPE_KEYS: {
    result_val = key_val;
    break;
  }
  case ITER_TYPE_VALUES: {
    result_val = val_val;
    break;
  }
  default:
    MOZ_RELEASE_ASSERT(false, "Invalid iter type");
  }

  JS_DefineProperty(cx, result, "value", result_val, JSPROP_ENUMERATE);

  JS::SetReservedSlot(self, Slots::Cursor, JS::Int32Value(index + 1));
  args.rval().setObject(*result);
  return true;
}

} // namespace builtins::web::fetch
