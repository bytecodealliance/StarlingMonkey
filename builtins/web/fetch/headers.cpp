#include "headers.h"
// #include "request-response.h"
#include "encode.h"
#include "decode.h"
#include "sequence.hpp"

#include "js/Conversions.h"

namespace builtins::web::fetch {
namespace {

using Handle = host_api::HttpHeaders;

#define HEADERS_ITERATION_METHOD(argc)                                                             \
  METHOD_HEADER(argc)                                                                              \
  JS::RootedObject entries(cx, get_entries(cx, self));                                             \
  if (!entries) {                                                                                  \
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
  bool name_changed;                                                                               \
  auto name_chars = normalize_header_name(cx, name, &name_changed, fun_name);                      \
  if (!name_chars) {                                                                               \
    return false;                                                                                  \
  }

#define NORMALIZE_VALUE(value, fun_name)                                                           \
  bool value_changed;                                                                              \
  auto value_chars = normalize_header_value(cx, value, &value_changed, fun_name);                  \
  if (!value_chars.ptr) {                                                                          \
    return false;                                                                                  \
  }

Handle *get_handle(JSObject *self) {
  MOZ_ASSERT(Headers::is_instance(self));
  auto handle =
      JS::GetReservedSlot(self, static_cast<uint32_t>(Headers::Slots::Handle)).toPrivate();
  return static_cast<Handle *>(handle);
}

/**
 * Validates and normalizes the given header name, by
 * - checking for invalid characters
 * - converting to lower-case
 *
 * See
 * https://searchfox.org/mozilla-central/rev/9f76a47f4aa935b49754c5608a1c8e72ee358c46/netwerk/protocol/http/nsHttp.cpp#172-215
 * For details on validation.
 */
host_api::HostString normalize_header_name(JSContext *cx, HandleValue name_val, bool* named_changed,
                                           const char *fun_name) {
  *named_changed = false;
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

  char *name_chars = name.begin();
  for (size_t i = 0; i < name.len; i++) {
    const unsigned char ch = name_chars[i];
    if (ch > 127 || !VALID_NAME_CHARS[ch]) {
      JS_ReportErrorUTF8(cx, "%s: Invalid header name '%s'", fun_name, name_chars);
      return nullptr;
    }

    if (ch >= 'A' && ch <= 'Z') {
      *named_changed = true;
      name_chars[i] = ch - 'A' + 'a';
    }
  }

  return name;
}

/**
 * Validates and normalizes the given header value, by
 * - stripping leading and trailing whitespace
 * - checking for interior line breaks and `\0`
 *
 * See
 * https://searchfox.org/mozilla-central/rev/9f76a47f4aa935b49754c5608a1c8e72ee358c46/netwerk/protocol/http/nsHttp.cpp#247-260
 * For details on validation.
 */
host_api::HostString normalize_header_value(JSContext *cx, HandleValue value_val,
                                            bool* value_changed, const char *fun_name) {
  *value_changed = false;
  JS::RootedString value_str(cx, JS::ToString(cx, value_val));
  if (!value_str) {
    return nullptr;
  }

  auto value = core::encode(cx, value_str);
  if (!value.ptr) {
    return nullptr;
  }

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

  if (start != 0 || end != value.len) {
    *value_changed = true;
  }

  for (size_t i = start; i < end; i++) {
    unsigned char ch = value_chars[i];
    if (ch == '\r' || ch == '\n' || ch == '\0') {
      JS_ReportErrorUTF8(cx, "%s: Invalid header value '%s'", fun_name, value_chars);
      return nullptr;
    }
  }

  return value;
}

JS::PersistentRooted<JSString *> comma;

bool retrieve_value_for_header_from_handle(JSContext *cx, JS::HandleObject self,
                                           const host_api::HostString &name,
                                           MutableHandleValue value) {
  auto handle = get_handle(self);
  auto ret = handle->get(name);

  if (auto *err = ret.to_err()) {
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
    val_str = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(str.ptr.get(), str.len));
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
    while ((currentPosition = cookiesString.find_first_of(',', currentPosition)) !=
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

} // namespace

void switch_to_content_only_mode(JSObject* self) {
  SetReservedSlot(self, static_cast<size_t>(Headers::Slots::Mode),
                  JS::Int32Value(static_cast<int32_t>(Headers::Mode::ContentOnly)));
}

bool Headers::append_header_value(JSContext *cx, JS::HandleObject self, JS::HandleValue name,
                                  JS::HandleValue value, const char *fun_name) {
  NORMALIZE_NAME(name, fun_name)
  NORMALIZE_VALUE(value, fun_name)

  auto handle = get_handle(self);
  if (handle) {
    std::string_view name = name_chars;
    if (name == "set-cookie") {
      for (auto value : splitCookiesString(value_chars)) {
        auto res = handle->append(name_chars, value);
        if (auto *err = res.to_err()) {
          HANDLE_ERROR(cx, *err);
          return false;
        }
      }
    } else {
      auto res = handle->append(name_chars, value_chars);
      if (auto *err = res.to_err()) {
        HANDLE_ERROR(cx, *err);
        return false;
      }
    }
  }

  return true;
}

JSObject *Headers::create(JSContext *cx, JS::HandleObject self, Handle *handle,
                          JS::HandleValue initv) {
  // TODO: check if initv is a Headers instance and clone its handle if so.
  JS::RootedObject headers(cx, create(cx, self, handle));
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

bool redecode_str_if_changed(JSContext* cx, HandleValue str_val, host_api::HostString& chars,
                             bool changed, MutableHandleValue rval) {
  if (!changed) {
    rval.set(str_val);
    return true;
  }

  RootedString str(cx, core::decode(cx, chars));
  if (!str) {
    return false;
  }

  rval.setString(str);
  return true;
}

bool Headers::get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  NORMALIZE_NAME(args[0], "Headers.get")

  Mode mode = Headers::mode(self);
  if (mode == Mode::HostOnly) {
    return retrieve_value_for_header_from_handle(cx, self, name_chars, args.rval());
  }

  RootedObject entries(cx, get_entries(cx, self));
  if (!entries) {
    return false;
  }

  RootedValue name_val(cx);
  if (!redecode_str_if_changed(cx, args[0], name_chars, name_changed, &name_val)) {
    return false;
  }
  return MapGet(cx, entries, name_val, args.rval());
}

bool Headers::set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  NORMALIZE_NAME(args[0], "Headers.set")
  NORMALIZE_VALUE(args[1], "Headers.set")

  Mode mode = Headers::mode(self);
  if (mode == Mode::HostOnly) {
    auto handle = get_handle(self);
    MOZ_ASSERT(handle);
    auto res = handle->set(name_chars, value_chars);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  } else if (mode == Mode::CachedInContent) {
    switch_to_content_only_mode(self);
  }

  if (mode == Mode::ContentOnly) {
    RootedObject entries(cx, get_entries(cx, self));
    if (!entries) {
      return false;
    }

    RootedValue name_val(cx);
    if (!redecode_str_if_changed(cx, args[0], name_chars, name_changed, &name_val)) {
      return false;
    }

    RootedValue value_val(cx);
    if (!redecode_str_if_changed(cx, args[0], value_chars, value_changed, &value_val)) {
      return false;
    }

    if (!MapSet(cx, entries, name_val, value_val)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::has(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  NORMALIZE_NAME(args[0], "Headers.has")

  Mode mode = Headers::mode(self);
  if (mode == Mode::HostOnly) {
    auto handle = get_handle(self);
    MOZ_ASSERT(handle);
    auto res = handle->has(name_chars);
    MOZ_ASSERT(!res.is_err());
    args.rval().setBoolean(res.unwrap());
  } else {
    RootedObject entries(cx, get_entries(cx, self));
    if (!entries) {
      return false;
    }
    bool has;
    if (!MapHas(cx, entries, args[0], &has)) {
      return false;
    }
    args.rval().setBoolean(has);
  }

  return true;
}

bool Headers::append(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  if (!append_header_value(cx, self, args[0], args[1], "Headers.append")) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool Headers::maybe_add(JSContext *cx, JS::HandleObject self, const char *name, const char *value) {
  MOZ_ASSERT(mode(self) == Mode::HostOnly);
  auto handle = get_handle(self);
  auto has = handle->has(name);
  MOZ_ASSERT(!has.is_err());
  if (has.unwrap()) {
    return true;
  }

  auto res = handle->append(name, value);
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  return true;
}

bool Headers::delete_(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(1, "delete")
  NORMALIZE_NAME(args[0], "Headers.delete")
  Mode mode = Headers::mode(self);
  if (mode == Mode::HostOnly) {
    auto handle = get_handle(self);
    MOZ_ASSERT(handle);
    std::string_view name = name_chars;
    auto res = handle->remove(name);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }
  } else if (mode == Mode::CachedInContent) {
    switch_to_content_only_mode(self);
  }

  if (mode == Mode::ContentOnly) {
    RootedObject entries(cx, get_entries(cx, self));
    if (!entries) {
      return false;
    }

    bool had;
    return MapDelete(cx, entries, args[0], &had);
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
  if (!JS::MapEntries(cx, entries, &iterable))
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

bool Headers::entries(JSContext *cx, unsigned argc, Value *vp) {
  HEADERS_ITERATION_METHOD(0)
  return MapEntries(cx, entries, args.rval());
}

bool Headers::keys(JSContext *cx, unsigned argc, Value *vp) {
  HEADERS_ITERATION_METHOD(0)
  return MapKeys(cx, entries, args.rval());
}

bool Headers::values(JSContext *cx, unsigned argc, Value *vp) {
  HEADERS_ITERATION_METHOD(0)
  return MapValues(cx, entries, args.rval());
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
  JS::RootedObject headers(cx, create(cx, headersInstance, nullptr, args.get(0)));
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

  auto comma_str = JS_NewStringCopyN(cx, ", ", 2);
  if (!comma_str) {
    return false;
  }
  comma.init(cx, comma_str);

  JS::RootedValue entries(cx);
  if (!JS_GetProperty(cx, proto_obj, "entries", &entries))
    return false;

  JS::SymbolCode code = JS::SymbolCode::iterator;
  JS::RootedId iteratorId(cx, JS::GetWellKnownSymbolKey(cx, code));
  return JS_DefinePropertyById(cx, proto_obj, iteratorId, entries, 0);
}

JSObject *Headers::create(JSContext *cx, HandleObject self, host_api::HttpHeadersReadOnly *handle) {
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Handle), PrivateValue(handle));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Mode),
                  JS::Int32Value(static_cast<int32_t>(Mode::HostOnly)));
  return self;
}
JSObject *Headers::get_entries(JSContext *cx, HandleObject self) {
  MOZ_ASSERT(is_instance(self));
  if (mode(self) != Mode::HostOnly) {
    return &GetReservedSlot(self, static_cast<size_t>(Slots::Entries)).toObject();
  }

  host_api::HttpHeadersReadOnly *handle = get_handle(self);
  MOZ_ASSERT(handle);
  auto res = handle->entries();
  if (res.is_err()) {
    HANDLE_ERROR(cx, *res.to_err());
    return nullptr;
  }

  RootedObject map(cx, JS::NewMapObject(cx));
  if (!map) {
    return nullptr;
  }

  RootedString key(cx);
  RootedValue key_val(cx);
  RootedString value(cx);
  RootedValue value_val(cx);
  for (auto& entry : std::move(res.unwrap())) {
    key = core::decode(cx, std::get<0>(entry));
    if (!key) {
      return nullptr;
    }
    value = core::decode(cx, std::get<1>(entry));
    if (!value) {
      return nullptr;
    }
    key_val.setString(key);
    value_val.setString(value);
    if (!MapSet(cx, map, key_val, value_val)) {
      return nullptr;
    }
  }

  SetReservedSlot(self, static_cast<size_t>(Slots::Entries), ObjectValue(*map));
  SetReservedSlot(self, static_cast<size_t>(Slots::Mode),
                  JS::Int32Value(static_cast<int32_t>(Mode::CachedInContent)));
  return map;
}

} // namespace builtins::web::fetch
