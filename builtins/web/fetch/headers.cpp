#include "builtins/headers.h"
#include "builtins/request-response.h"
#include "core/encode.h"
#include "core/sequence.hpp"
#include "host_interface/host_api.h"
#include "js-compute-builtins.h"

#include "js/Conversions.h"

namespace builtins {

namespace {

#define HEADERS_ITERATION_METHOD(argc)                                                             \
  METHOD_HEADER(argc)                                                                              \
  JS::RootedObject backing_map(cx, get_backing_map(self));                                         \
  if (!ensure_all_header_values_from_handle(cx, self, backing_map)) {                              \
    return false;                                                                                  \
  }

const char VALID_NAME_CHARS[128] = {
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

#define NORMALIZE_NAME(name, fun_name)                                                             \
  JS::RootedValue normalized_name(cx, name);                                                       \
  auto name_chars = normalize_header_name(cx, &normalized_name, fun_name);                         \
  if (!name_chars) {                                                                               \
    return false;                                                                                  \
  }

#define NORMALIZE_VALUE(value, fun_name)                                                           \
  JS::RootedValue normalized_value(cx, value);                                                     \
  auto value_chars = normalize_header_value(cx, &normalized_value, fun_name);                      \
  if (!value_chars) {                                                                              \
    return false;                                                                                  \
  }

JSObject *get_backing_map(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  return &JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::BackingMap)).toObject();
}

Headers::Mode get_mode(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  return static_cast<Headers::Mode>(
      JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Mode)).toInt32());
}

bool lazy_values(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  return JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::HasLazyValues))
      .toBoolean();
}

uint32_t get_handle(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  return static_cast<uint32_t>(
      JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Handle)).toInt32());
}

/**
 * Validates and normalizes the given header name, by
 * - checking for invalid characters
 * - converting to lower-case
 *
 * See
 * https://searchfox.org/mozilla-central/rev/9f76a47f4aa935b49754c5608a1c8e72ee358c46/netwerk/protocol/http/nsHttp.cpp#172-215
 * For details on validation.
 *
 * Mutates `name_val` in place, and returns the name as UniqueChars.
 * This is done because most uses of header names require handling of both the
 * JSString and the char* version, so they'd otherwise have to recreate one of
 * the two.
 */
host_api::HostString normalize_header_name(JSContext *cx, JS::MutableHandleValue name_val,
                                           const char *fun_name) {
  JS::RootedString name_str(cx, JS::ToString(cx, name_val));
  if (!name_str) {
    return nullptr;
  }

  auto name = core::encode(cx, name_str);
  if (!name) {
    return nullptr;
  }

  if (name.len == 0) {
    JS_ReportErrorASCII(cx, "%s: Header name can't be empty", fun_name);
    return nullptr;
  }

  bool changed = false;

  char *name_chars = name.begin();
  for (size_t i = 0; i < name.len; i++) {
    unsigned char ch = name_chars[i];
    if (ch > 127 || !VALID_NAME_CHARS[ch]) {
      JS_ReportErrorUTF8(cx, "%s: Invalid header name '%s'", fun_name, name_chars);
      return nullptr;
    }

    if (ch >= 'A' && ch <= 'Z') {
      name_chars[i] = ch - 'A' + 'a';
      changed = true;
    }
  }

  if (changed) {
    name_str = JS_NewStringCopyN(cx, name_chars, name.len);
    if (!name_str) {
      return nullptr;
    }
  }

  name_val.setString(name_str);
  return name;
}

host_api::HostString normalize_header_value(JSContext *cx, JS::MutableHandleValue value_val,
                                            const char *fun_name) {
  JS::RootedString value_str(cx, JS::ToString(cx, value_val));
  if (!value_str) {
    return nullptr;
  }

  auto value = core::encode(cx, value_str);
  if (!value) {
    return nullptr;
  }

  auto *value_chars = value.begin();
  size_t start = 0;
  size_t end = value.len;

  // We follow Gecko's interpretation of what's a valid header value. After
  // stripping leading and trailing whitespace, all interior line breaks and
  // `\0` are considered invalid. See
  // https://searchfox.org/mozilla-central/rev/9f76a47f4aa935b49754c5608a1c8e72ee358c46/netwerk/protocol/http/nsHttp.cpp#247-260
  // for details.
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
      JS_ReportErrorUTF8(cx, "%s: Invalid header value '%s'", fun_name, value_chars);
      return nullptr;
    }
  }

  if (start != 0 || end != value.len) {
    value_str = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(value_chars + start, end - start));
    if (!value_str) {
      return nullptr;
    }
  }

  value_val.setString(value_str);

  return value;
}

JS::PersistentRooted<JSString *> comma;

// Append an already normalized value for an already normalized header name
// to the JS side map, but not the host.
//
// Returns the resulting combined value in `normalized_value`.
bool append_header_value_to_map(JSContext *cx, JS::HandleObject self,
                                JS::HandleValue normalized_name,
                                JS::MutableHandleValue normalized_value) {
  JS::RootedValue existing(cx);
  JS::RootedObject map(cx, get_backing_map(self));
  if (!JS::MapGet(cx, map, normalized_name, &existing))
    return false;

  // Existing value must only be null if we're in the process if applying
  // header values from a handle.
  if (!existing.isNullOrUndefined()) {
    if (!comma.get()) {
      comma.init(cx, JS_NewStringCopyN(cx, ", ", 2));
      if (!comma) {
        return false;
      }
    }

    JS::RootedString str(cx, existing.toString());
    str = JS_ConcatStrings(cx, str, comma);
    if (!str) {
      return false;
    }

    JS::RootedString val_str(cx, normalized_value.toString());
    str = JS_ConcatStrings(cx, str, val_str);
    if (!str) {
      return false;
    }

    normalized_value.setString(str);
  }

  return JS::MapSet(cx, map, normalized_name, normalized_value);
}

bool get_header_names_from_handle(JSContext *cx, uint32_t handle, Headers::Mode mode,
                                  JS::HandleObject backing_map) {

  auto names = mode == Headers::Mode::ProxyToRequest
                   ? host_api::HttpReq{handle}.get_header_names()
                   : host_api::HttpResp{handle}.get_header_names();
  if (auto *err = names.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  JS::RootedString name(cx);
  JS::RootedValue name_val(cx);
  for (auto &str : names.unwrap()) {
    // TODO: can `name` take ownership of the buffer here instead?
    name = JS_NewStringCopyN(cx, str.ptr.get(), str.len);
    if (!name) {
      return false;
    }

    name_val.setString(name);
    JS::MapSet(cx, backing_map, name_val, JS::NullHandleValue);
  }

  return true;
}

bool retrieve_value_for_header_from_handle(JSContext *cx, JS::HandleObject self,
                                           JS::HandleValue name, JS::MutableHandleValue value) {
  auto mode = get_mode(self);
  MOZ_ASSERT(mode != Headers::Mode::Standalone);
  uint32_t handle = get_handle(self);

  JS::RootedString name_str(cx, name.toString());
  auto name_chars = core::encode(cx, name_str);

  auto ret = mode == Headers::Mode::ProxyToRequest
                 ? host_api::HttpReq{handle}.get_header_values(name_chars)
                 : host_api::HttpResp{handle}.get_header_values(name_chars);

  if (auto *err = ret.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto &values = ret.unwrap();
  if (!values.has_value()) {
    return true;
  }

  JS::RootedString val_str(cx);
  for (auto &str : values.value()) {
    val_str = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(str.ptr.get(), str.len));
    if (!val_str) {
      return false;
    }

    value.setString(val_str);
    if (!append_header_value_to_map(cx, self, name, value)) {
      return false;
    }
  }

  return true;
}

/**
 * Ensures that a value for the given header is available to client code.
 *
 * The calling code must ensure that a header with the given name exists, but
 * might not yet have been retrieved from the host, i.e., it might be a "lazy"
 * value.
 *
 * The value is returned via the `values` outparam, but *only* if the Headers
 * object has lazy values at all. This is to avoid the map lookup in those cases
 * where none is necessary in this function, and the consumer wouldn't use the
 * value anyway.
 */
bool ensure_value_for_header(JSContext *cx, JS::HandleObject self, JS::HandleValue normalized_name,
                             JS::MutableHandleValue values) {
  if (!lazy_values(self))
    return true;

  JS::RootedObject map(cx, get_backing_map(self));
  if (!JS::MapGet(cx, map, normalized_name, values))
    return false;

  // Value isn't lazy, just return it.
  if (!values.isNull())
    return true;

  return retrieve_value_for_header_from_handle(cx, self, normalized_name, values);
}

bool get_header_value_for_name(JSContext *cx, JS::HandleObject self, JS::HandleValue name,
                               JS::MutableHandleValue rval, const char *fun_name) {
  NORMALIZE_NAME(name, fun_name)

  if (!ensure_value_for_header(cx, self, normalized_name, rval)) {
    return false;
  }

  if (rval.isString()) {
    return true;
  }

  JS::RootedObject map(cx, get_backing_map(self));
  if (!JS::MapGet(cx, map, normalized_name, rval)) {
    return false;
  }

  // Return `null` for non-existent headers.
  if (rval.isUndefined()) {
    rval.setNull();
  }

  return true;
}

std::string_view special_chars = "=,;";

std::vector<std::string_view> splitCookiesString(std::string_view cookiesString) {
  std::vector<std::string_view> cookiesStrings;
  std::size_t currentPosition = 0; // Current position in the string
  std::size_t start;               // Start position of the current cookie
  std::size_t lastComma;           // Position of the last comma found
  std::size_t nextStart;           // Position of the start of the next cookie

  // Iterate over the string and split it into cookies.
  while (currentPosition < cookiesString.length()) {
    start = currentPosition;

    // Iterate until we find a comma that might be used as a separator.
    while ((currentPosition = cookiesString.find_first_of(",", currentPosition)) !=
           std::string_view::npos) {
      // ',' is a cookie separator only if we later have '=', before having ';' or ','
      lastComma = currentPosition;
      nextStart = ++currentPosition;

      // Check if the next sequence of characters is a non-special character followed by an equals
      // sign.
      currentPosition = cookiesString.find_first_of(special_chars, currentPosition);

      // If the current character is an equals sign, we have found a cookie separator.
      if (currentPosition != std::string_view::npos && cookiesString.at(currentPosition) == '=') {
        // currentPosition is inside the next cookie, so back up and return it.
        currentPosition = nextStart;
        cookiesStrings.push_back(cookiesString.substr(start, lastComma - start));
        start = currentPosition;
      } else {
        // The cookie contains ';' or ',' as part of the value
        // so we need to keep accumulating characters
        currentPosition = lastComma + 1;
      }
    }

    // If we reach the end of the string without finding a separator, add the last cookie to the
    // vector.
    if (currentPosition >= cookiesString.length()) {
      cookiesStrings.push_back(cookiesString.substr(start, cookiesString.length() - start));
    }
  }
  return cookiesStrings;
}

bool ensure_all_header_values_from_handle(JSContext *cx, JS::HandleObject self,
                                          JS::HandleObject backing_map) {
  if (!lazy_values(self))
    return true;

  JS::RootedValue iterable(cx);
  if (!JS::MapKeys(cx, backing_map, &iterable))
    return false;

  JS::ForOfIterator it(cx);
  if (!it.init(iterable))
    return false;

  JS::RootedValue name(cx);
  JS::RootedValue v(cx);
  while (true) {
    bool done;
    if (!it.next(&name, &done))
      return false;

    if (done)
      break;

    if (!ensure_value_for_header(cx, self, name, &v))
      return false;
  }

  JS_SetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::HasLazyValues),
                     JS::BooleanValue(false));

  return true;
}

} // namespace

bool Headers::append_header_value(JSContext *cx, JS::HandleObject self, JS::HandleValue name,
                                  JS::HandleValue value, const char *fun_name) {
  NORMALIZE_NAME(name, fun_name)
  NORMALIZE_VALUE(value, fun_name)

  // Ensure that any host-side values have been applied JS-side.
  JS::RootedValue v(cx);
  if (!ensure_value_for_header(cx, self, normalized_name, &v)) {
    return false;
  }

  auto mode = get_mode(self);
  if (mode != Headers::Mode::Standalone) {
    auto handle = get_handle(self);
    std::string_view name = name_chars;
    std::string_view value = value_chars;
    if (name == "set-cookie") {
      for (auto value : splitCookiesString(value)) {
        auto res = mode == Headers::Mode::ProxyToRequest
                       ? host_api::HttpReq{handle}.append_header(name, value)
                       : host_api::HttpResp{handle}.append_header(name, value);
        if (auto *err = res.to_err()) {
          HANDLE_ERROR(cx, *err);
          return false;
        }
      }
    } else {
      auto res = mode == Headers::Mode::ProxyToRequest
                     ? host_api::HttpReq{handle}.append_header(name, value)
                     : host_api::HttpResp{handle}.append_header(name, value);
      if (auto *err = res.to_err()) {
        HANDLE_ERROR(cx, *err);
        return false;
      }
    }
  }

  return append_header_value_to_map(cx, self, normalized_name, &normalized_value);
}

bool Headers::delazify(JSContext *cx, JS::HandleObject headers) {
  JS::RootedObject backing_map(cx, get_backing_map(headers));
  return ensure_all_header_values_from_handle(cx, headers, backing_map);
}

JSObject *Headers::create(JSContext *cx, JS::HandleObject self, Headers::Mode mode,
                          JS::HandleObject owner, JS::HandleObject init_headers) {
  JS::RootedObject headers(cx, create(cx, self, mode, owner));
  if (!headers) {
    return nullptr;
  }

  if (!init_headers) {
    return headers;
  }

  if (!Headers::delazify(cx, init_headers)) {
    return nullptr;
  }

  JS::RootedObject headers_map(cx, get_backing_map(headers));
  JS::RootedObject init_map(cx, get_backing_map(init_headers));

  JS::RootedValue iterable(cx);
  if (!JS::MapEntries(cx, init_map, &iterable)) {
    return nullptr;
  }

  JS::ForOfIterator it(cx);
  if (!it.init(iterable)) {
    return nullptr;
  }

  JS::RootedObject entry(cx);
  JS::RootedValue entry_val(cx);
  JS::RootedValue name_val(cx);
  JS::RootedValue value_val(cx);
  while (true) {
    bool done;
    if (!it.next(&entry_val, &done)) {
      return nullptr;
    }

    if (done) {
      break;
    }

    entry = &entry_val.toObject();
    JS_GetElement(cx, entry, 0, &name_val);
    JS_GetElement(cx, entry, 1, &value_val);

    if (!Headers::append_header_value(cx, headers, name_val, value_val, "Headers constructor")) {
      return nullptr;
    }
  }

  return headers;
}

JSObject *Headers::create(JSContext *cx, JS::HandleObject self, Headers::Mode mode,
                          JS::HandleObject owner, JS::HandleValue initv) {
  JS::RootedObject headers(cx, create(cx, self, mode, owner));
  if (!headers)
    return nullptr;

  bool consumed = false;
  if (!core::maybe_consume_sequence_or_record<Headers::append_header_value>(cx, initv, headers,
                                                                            &consumed, "Headers")) {
    return nullptr;
  }

  if (!consumed) {
    core::report_sequence_or_record_arg_error(cx, "Headers", "");
    return nullptr;
  }

  return headers;
}

bool Headers::get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  NORMALIZE_NAME(args[0], "Headers.get")

  return get_header_value_for_name(cx, self, normalized_name, args.rval(), "Headers.get");
}

bool Headers::set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  NORMALIZE_NAME(args[0], "Headers.set")
  NORMALIZE_VALUE(args[1], "Headers.set")

  auto mode = get_mode(self);
  if (mode != Mode::Standalone) {
    auto handle = get_handle(self);
    std::string_view name = name_chars;
    std::string_view val = value_chars;
    auto res = mode == Mode::ProxyToRequest ? host_api::HttpReq{handle}.insert_header(name, val)
                                            : host_api::HttpResp{handle}.insert_header(name, val);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  }

  JS::RootedObject map(cx, get_backing_map(self));
  if (!JS::MapSet(cx, map, normalized_name, normalized_value)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::has(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  NORMALIZE_NAME(args[0], "Headers.has")
  bool has;
  JS::RootedObject map(cx, get_backing_map(self));
  if (!JS::MapHas(cx, map, normalized_name, &has)) {
    return false;
  }

  args.rval().setBoolean(has);
  return true;
}

bool Headers::append(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  if (!Headers::append_header_value(cx, self, args[0], args[1], "Headers.append")) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::maybe_add(JSContext *cx, JS::HandleObject self, const char *name, const char *value) {
  MOZ_ASSERT(Headers::is_instance(self));
  JS::RootedString name_str(cx, JS_NewStringCopyN(cx, name, strlen(name)));
  if (!name_str) {
    return false;
  }
  JS::RootedValue name_val(cx, JS::StringValue(name_str));

  JS::RootedObject map(cx, get_backing_map(self));
  bool has;
  if (!JS::MapHas(cx, map, name_val, &has)) {
    return false;
  }
  if (has) {
    return true;
  }

  JS::RootedString value_str(cx, JS_NewStringCopyN(cx, value, strlen(value)));
  if (!value_str) {
    return false;
  }
  JS::RootedValue value_val(cx, JS::StringValue(value_str));

  return Headers::append_header_value(cx, self, name_val, value_val, "internal_maybe_add");
}

bool Headers::delete_(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(1, "delete")

  NORMALIZE_NAME(args[0], "Headers.delete")

  bool has;
  JS::RootedObject map(cx, get_backing_map(self));
  if (!JS::MapDelete(cx, map, normalized_name, &has)) {
    return false;
  }

  // If no header with the given name exists, `delete` is a no-op.
  if (!has) {
    args.rval().setUndefined();
    return true;
  }

  auto mode = get_mode(self);
  if (mode != Headers::Mode::Standalone) {
    auto handle = get_handle(self);
    std::string_view name = name_chars;
    auto res = mode == Mode::ProxyToRequest ? host_api::HttpReq{handle}.remove_header(name)
                                            : host_api::HttpResp{handle}.remove_header(name);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::forEach(JSContext *cx, unsigned argc, JS::Value *vp) {
  HEADERS_ITERATION_METHOD(1)

  if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject())) {
    JS_ReportErrorASCII(cx, "Failed to execute 'forEach' on 'Headers': "
                            "parameter 1 is not of type 'Function'");
    return false;
  }

  JS::RootedValueArray<3> newArgs(cx);
  newArgs[2].setObject(*self);

  JS::RootedValue rval(cx);

  JS::RootedValue iterable(cx);
  if (!JS::MapEntries(cx, backing_map, &iterable))
    return false;

  JS::ForOfIterator it(cx);
  if (!it.init(iterable))
    return false;

  JS::RootedValue entry_val(cx);
  JS::RootedObject entry(cx);
  while (true) {
    bool done;
    if (!it.next(&entry_val, &done))
      return false;

    if (done)
      break;

    entry = &entry_val.toObject();
    JS_GetElement(cx, entry, 1, newArgs[0]);
    JS_GetElement(cx, entry, 0, newArgs[1]);

    if (!JS::Call(cx, args.thisv(), args[0], newArgs, &rval))
      return false;
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::entries(JSContext *cx, unsigned argc, JS::Value *vp) {
  HEADERS_ITERATION_METHOD(0)
  return JS::MapEntries(cx, backing_map, args.rval());
}

bool Headers::keys(JSContext *cx, unsigned argc, JS::Value *vp) {
  HEADERS_ITERATION_METHOD(0)
  return JS::MapKeys(cx, backing_map, args.rval());
}

bool Headers::values(JSContext *cx, unsigned argc, JS::Value *vp) {
  HEADERS_ITERATION_METHOD(0)
  return JS::MapValues(cx, backing_map, args.rval());
}

const JSFunctionSpec Headers::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Headers::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Headers::methods[] = {
    JS_FN("get", Headers::get, 1, JSPROP_ENUMERATE),
    JS_FN("has", Headers::has, 1, JSPROP_ENUMERATE),
    JS_FN("set", Headers::set, 2, JSPROP_ENUMERATE),
    JS_FN("append", Headers::append, 2, JSPROP_ENUMERATE),
    JS_FN("delete", Headers::delete_, 1, JSPROP_ENUMERATE),
    JS_FN("forEach", Headers::forEach, 1, JSPROP_ENUMERATE),
    JS_FN("entries", Headers::entries, 0, JSPROP_ENUMERATE),
    JS_FN("keys", Headers::keys, 0, JSPROP_ENUMERATE),
    JS_FN("values", Headers::values, 0, JSPROP_ENUMERATE),
    // [Symbol.iterator] added in init_class.
    JS_FS_END,
};

const JSPropertySpec Headers::properties[] = {
    JS_PS_END,
};

bool Headers::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("Headers", 0);
  JS::RootedObject headersInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  JS::RootedObject headers(cx, create(cx, headersInstance, Mode::Standalone, nullptr, args.get(0)));
  if (!headers) {
    return false;
  }

  args.rval().setObject(*headers);
  return true;
}

bool Headers::init_class(JSContext *cx, JS::HandleObject global) {
  bool ok = init_class_impl(cx, global);
  if (!ok)
    return false;

  JS::RootedValue entries(cx);
  if (!JS_GetProperty(cx, proto_obj, "entries", &entries))
    return false;

  JS::SymbolCode code = JS::SymbolCode::iterator;
  JS::RootedId iteratorId(cx, JS::GetWellKnownSymbolKey(cx, code));
  return JS_DefinePropertyById(cx, proto_obj, iteratorId, entries, 0);
}

JSObject *Headers::create(JSContext *cx, JS::HandleObject self, Headers::Mode mode,
                          JS::HandleObject owner) {
  JS_SetReservedSlot(self, static_cast<uint32_t>(Slots::Mode),
                     JS::Int32Value(static_cast<int32_t>(mode)));
  uint32_t handle = UINT32_MAX - 1;
  if (mode != Headers::Mode::Standalone) {
    handle = RequestOrResponse::handle(owner);
  }
  JS_SetReservedSlot(self, static_cast<uint32_t>(Slots::Handle),
                     JS::Int32Value(static_cast<int32_t>(handle)));

  JS::RootedObject backing_map(cx, JS::NewMapObject(cx));
  if (!backing_map) {
    return nullptr;
  }
  JS::SetReservedSlot(self, static_cast<uint32_t>(Slots::BackingMap),
                      JS::ObjectValue(*backing_map));

  bool lazy = false;
  if ((mode == Mode::ProxyToRequest && Request::is_downstream(owner)) ||
      (mode == Mode::ProxyToResponse && Response::is_upstream(owner))) {
    lazy = true;
    if (!get_header_names_from_handle(cx, handle, mode, backing_map)) {
      return nullptr;
    }
  }

  JS_SetReservedSlot(self, static_cast<uint32_t>(Slots::HasLazyValues), JS::BooleanValue(lazy));

  return self;
}

} // namespace builtins
