#include "blob.h"
#include "encode.h"
#include "file.h"
#include "rust-url.h"
#include "sequence.hpp"
#include "url.h"
#include "worker-location.h"

#include "crypto/uuid.h"

#include "js/Array.h"
#include "js/AllocPolicy.h"
#include "js/GCHashTable.h"
#include "js/TypeDecls.h"



namespace builtins::web::url {

using blob::Blob;
using file::File;
using worker_location::WorkerLocation;

bool URLSearchParamsIterator::next(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  JS::RootedObject params_obj(cx, &JS::GetReservedSlot(self, Slots::Params).toObject());
  auto *const params = URLSearchParams::get_params(params_obj);
  size_t index = JS::GetReservedSlot(self, Slots::Index).toInt32();
  uint8_t type = JS::GetReservedSlot(self, Slots::Type).toInt32();

  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) {
    return false;
  }

  jsurl::JSSearchParam param{};
  jsurl::params_at(params, index, &param);

  if (param.done) {
    JS_DefineProperty(cx, result, "done", JS::TrueHandleValue, JSPROP_ENUMERATE);
    JS_DefineProperty(cx, result, "value", JS::UndefinedHandleValue, JSPROP_ENUMERATE);

    args.rval().setObject(*result);
    return true;
  }

  JS_DefineProperty(cx, result, "done", JS::FalseHandleValue, JSPROP_ENUMERATE);

  JS::RootedValue key_val(cx);
  JS::RootedValue val_val(cx);

  if (type != ITER_TYPE_VALUES) {
    auto chars = JS::UTF8Chars((char *)param.name.data, param.name.len);
    JS::RootedString str(cx, JS_NewStringCopyUTF8N(cx, chars));
    if (!str) {
      return false;
    }
    key_val = JS::StringValue(str);
  }

  if (type != ITER_TYPE_KEYS) {
    auto chars = JS::UTF8Chars((char *)param.value.data, param.value.len);
    JS::RootedString str(cx, JS_NewStringCopyUTF8N(cx, chars));
    if (!str) {
      return false;
    }
    val_val = JS::StringValue(str);
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

  JS::SetReservedSlot(self, Slots::Index, JS::Int32Value(index + 1));
  args.rval().setObject(*result);
  return true;
}

const JSFunctionSpec URLSearchParamsIterator::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec URLSearchParamsIterator::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec URLSearchParamsIterator::methods[] = {
    JS_FN("next", URLSearchParamsIterator::next, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec URLSearchParamsIterator::properties[] = {
    JS_PS_END,
};

bool URLSearchParamsIterator::init_class(JSContext *cx, JS::HandleObject global) {
  JS::RootedObject iterator_proto(cx, JS::GetRealmIteratorPrototype(cx));
  if (!iterator_proto) {
    return false;
  }

  if (!init_class_impl(cx, global, iterator_proto)) {
    return false;
  }

  // Delete both the `URLSearchParamsIterator` global property and the
  // `constructor` property on `URLSearchParamsIterator.prototype`. The latter
  // because Iterators don't have their own constructor on the prototype.
  return JS_DeleteProperty(cx, global, class_.name) &&
         JS_DeleteProperty(cx, proto_obj, "constructor");
}

JSObject *URLSearchParamsIterator::create(JSContext *cx, JS::HandleObject params, uint8_t type) {
  MOZ_RELEASE_ASSERT(type <= ITER_TYPE_VALUES);

  JS::RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self) {
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::Params, JS::ObjectValue(*params));
  JS::SetReservedSlot(self, Slots::Type, JS::Int32Value(type));
  JS::SetReservedSlot(self, Slots::Index, JS::Int32Value(0));

  return self;
}

const JSFunctionSpec URLSearchParams::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec URLSearchParams::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec URLSearchParams::methods[] = {
    JS_FN("append", URLSearchParams::append, 2, JSPROP_ENUMERATE),
    JS_FN("delete", URLSearchParams::delete_, 1, JSPROP_ENUMERATE),
    JS_FN("has", URLSearchParams::has, 1, JSPROP_ENUMERATE),
    JS_FN("get", URLSearchParams::get, 1, JSPROP_ENUMERATE),
    JS_FN("getAll", URLSearchParams::getAll, 1, JSPROP_ENUMERATE),
    JS_FN("set", URLSearchParams::set, 2, JSPROP_ENUMERATE),
    JS_FN("sort", URLSearchParams::sort, 0, JSPROP_ENUMERATE),
    JS_FN("toString", URLSearchParams::toString, 0, JSPROP_ENUMERATE),
    JS_FN("forEach", URLSearchParams::forEach, 0, JSPROP_ENUMERATE),
    JS_FN("entries", URLSearchParams::entries, 0, JSPROP_ENUMERATE),
    JS_FN("keys", URLSearchParams::keys, 0, JSPROP_ENUMERATE),
    JS_FN("values", URLSearchParams::values, 0, JSPROP_ENUMERATE),
    // [Symbol.iterator] added in init_class.
    JS_FS_END,
};

const JSPropertySpec URLSearchParams::properties[] = {
    JS_PSG("size", URLSearchParams::size_get, JSPROP_ENUMERATE),
    JS_PS_END,
};

jsurl::JSUrlSearchParams *URLSearchParams::get_params(JSObject *self) {
  return static_cast<jsurl::JSUrlSearchParams *>(
      JS::GetReservedSlot(self, Slots::Params).toPrivate());
}

namespace {
jsurl::SpecString append_impl_validate(JSContext *cx, JS::HandleValue key, const char *_) {
  return core::encode_spec_string(cx, key);
}
bool append_impl(JSContext *cx, JS::HandleObject self, jsurl::SpecString key, JS::HandleValue val,
                 const char *_) {
  auto *const params = URLSearchParams::get_params(self);

  auto value = core::encode_spec_string(cx, val);
  if (!value.data) {
    return false;
  }

  jsurl::params_append(params, key, value);
  return true;
}
} // namespace

jsurl::SpecSlice URLSearchParams::serialize(JSContext *cx, JS::HandleObject self) {
  return jsurl::params_to_string(get_params(self));
}

bool URLSearchParams::append(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)
  auto value = append_impl_validate(cx, args[0], "append");
  if (!append_impl(cx, self, value, args[1], "append")) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool URLSearchParams::delete_(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(1, "delete");

  auto *params =
      static_cast<jsurl::JSUrlSearchParams *>(JS::GetReservedSlot(self, Slots::Params).toPrivate());

  auto name = core::encode_spec_string(cx, args.get(0));
  if (!name.data) {
    return false;
  }

  if (args.hasDefined(1)) {
    auto value = core::encode_spec_string(cx, args.get(1));
    if (!value.data) {
      return false;
    }
    jsurl::params_delete_kv(params, &name, &value);
  } else {
    jsurl::params_delete(params, &name);
  }

  args.rval().setUndefined();
  return true;
}

bool URLSearchParams::has(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)
  auto *params =
      static_cast<jsurl::JSUrlSearchParams *>(JS::GetReservedSlot(self, Slots::Params).toPrivate());

  auto name = core::encode_spec_string(cx, args.get(0));
  if (!name.data) {
    return false;
  }

  if (args.hasDefined(1)) {
    auto value = core::encode_spec_string(cx, args.get(1));
    if (!value.data) {
      return false;
    }
    args.rval().setBoolean(jsurl::params_has_kv(params, &name, &value));
  } else {
    args.rval().setBoolean(jsurl::params_has(params, &name));
  }

  return true;
}

bool URLSearchParams::get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);
  const auto *params = static_cast<const jsurl::JSUrlSearchParams *>(
      JS::GetReservedSlot(self, Slots::Params).toPrivate());

  auto name = core::encode_spec_string(cx, args.get(0));
  if (!name.data) {
    return false;
  }

  const jsurl::SpecSlice slice = jsurl::params_get(params, &name);
  if (!slice.data) {
    args.rval().setNull();
    return true;
  }

  JS::RootedString str(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars((char *)slice.data, slice.len)));
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

bool URLSearchParams::getAll(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);

  const auto *params = static_cast<const jsurl::JSUrlSearchParams *>(
      JS::GetReservedSlot(self, Slots::Params).toPrivate());

  auto name = core::encode_spec_string(cx, args.get(0));
  if (!name.data) {
    return false;
  }

  const jsurl::CVec<jsurl::SpecSlice> values = jsurl::params_get_all(params, &name);

  JS::RootedObject result(cx, JS::NewArrayObject(cx, values.len));
  if (!result) {
    return false;
  }

  JS::RootedString str(cx);
  JS::RootedValue str_val(cx);
  for (size_t i = 0; i < values.len; i++) {
    const jsurl::SpecSlice value = values.ptr[i];
    str = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars((char *)value.data, value.len));
    if (!str) {
      return false;
    }

    str_val.setString(str);
    if (!JS_SetElement(cx, result, i, str_val)) {
      return false;
    }
  }

  args.rval().setObject(*result);
  return true;
}

bool URLSearchParams::set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2);

  auto *params =
      static_cast<jsurl::JSUrlSearchParams *>(JS::GetReservedSlot(self, Slots::Params).toPrivate());

  auto name = core::encode_spec_string(cx, args[0]);
  if (!name.data) {
    return false;
  }

  auto value = core::encode_spec_string(cx, args[1]);
  if (!value.data) {
    return false;
  }

  jsurl::params_set(params, name, value);
  return true;

  args.rval().setUndefined();
  return true;
}

bool URLSearchParams::size_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  auto *params =
      static_cast<jsurl::JSUrlSearchParams *>(JS::GetReservedSlot(self, Slots::Params).toPrivate());
  args.rval().setNumber(jsurl::params_size(params));
  return true;
}

bool URLSearchParams::sort(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  auto *params =
      static_cast<jsurl::JSUrlSearchParams *>(JS::GetReservedSlot(self, Slots::Params).toPrivate());
  jsurl::params_sort(params);
  args.rval().setUndefined();
  return true;
}

bool URLSearchParams::toString(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  jsurl::SpecSlice slice = serialize(cx, self);
  JS::RootedString str(
      cx, JS_NewStringCopyUTF8N(
              cx, JS::UTF8Chars(reinterpret_cast<const char *>(slice.data), slice.len)));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

BUILTIN_ITERATOR_METHODS(URLSearchParams)

bool URLSearchParams::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("URLSearchParams", 0);

  JS::RootedObject urlSearchParamsInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  JS::RootedObject self(cx, create(cx, urlSearchParamsInstance, args.get(0)));
  if (!self) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

bool URLSearchParams::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
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

JSObject *URLSearchParams::create(JSContext *cx, JS::HandleObject self,
                                  JS::HandleValue params_val) {
  auto *params = jsurl::new_params();
  JS::SetReservedSlot(self, Slots::Params, JS::PrivateValue(params));

  bool consumed = false;
  const char *alt_text = ", or a value that can be stringified";
  if (!core::maybe_consume_sequence_or_record<jsurl::SpecString, append_impl_validate, append_impl>(
          cx, params_val, self, &consumed, "URLSearchParams", alt_text)) {
    return nullptr;
  }

  if (!consumed) {
    auto init = core::encode_spec_string(cx, params_val);
    if (!init.data) {
      return nullptr;
    }

    jsurl::params_init(params, &init);
  }

  return self;
}

JSObject *URLSearchParams::create(JSContext *cx, JS::HandleObject self, jsurl::JSUrl *url) {

  jsurl::JSUrlSearchParams *params = jsurl::url_search_params(url);
  if (!params) {
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::Params, JS::PrivateValue(params));
  JS::SetReservedSlot(self, Slots::Url, JS::PrivateValue(url));

  return self;
}

#define ACCESSOR_GET(field)                                                                        \
  bool URL::field(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval) {             \
    const jsurl::JSUrl *url =                                                                      \
        static_cast<jsurl::JSUrl *>(JS::GetReservedSlot(self, URL::Slots::Url).toPrivate());       \
    const jsurl::SpecSlice slice = jsurl::field(url);                                              \
    JS::RootedString str(cx,                                                                       \
                         JS_NewStringCopyUTF8N(cx, JS::UTF8Chars((char *)slice.data, slice.len))); \
    if (!str) {                                                                                    \
      return false;                                                                                \
    }                                                                                              \
                                                                                                   \
    rval.setString(str);                                                                           \
    return true;                                                                                   \
  }                                                                                                \
                                                                                                   \
  bool URL::field##_get(JSContext *cx, unsigned argc, JS::Value *vp) {                             \
    METHOD_HEADER(0)                                                                               \
    return field(cx, self, args.rval());                                                           \
  }

#define ACCESSOR_SET(field)                                                                        \
  bool URL::field##_set(JSContext *cx, unsigned argc, JS::Value *vp) {                             \
    METHOD_HEADER(1)                                                                               \
    jsurl::JSUrl *url =                                                                            \
        static_cast<jsurl::JSUrl *>(JS::GetReservedSlot(self, URL::Slots::Url).toPrivate());       \
                                                                                                   \
    jsurl::SpecString str = core::encode_spec_string(cx, args.get(0));                             \
    if (!str.data) {                                                                               \
      return false;                                                                                \
    }                                                                                              \
                                                                                                   \
    jsurl::set_##field(url, &str);                                                                 \
                                                                                                   \
    args.rval().set(args.get(0));                                                                  \
    return true;                                                                                   \
  }

#define ACCESSOR(field)                                                                            \
  ACCESSOR_GET(field)                                                                              \
  ACCESSOR_SET(field)

ACCESSOR(hash)
ACCESSOR(host)
ACCESSOR(hostname)
ACCESSOR(href)
ACCESSOR(password)
ACCESSOR(pathname)
ACCESSOR(port)
ACCESSOR(protocol)
ACCESSOR(search)
ACCESSOR(username)

#undef ACCESSOR_GET
#undef ACCESSOR_SET
#undef ACCESSOR

const JSFunctionSpec URL::static_methods[] = {
    JS_FN("createObjectURL", createObjectURL, 1, JSPROP_ENUMERATE),
    JS_FN("revokeObjectURL", revokeObjectURL, 1, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec URL::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec URL::methods[] = {
    JS_FN("toString", toString, 0, JSPROP_ENUMERATE),
    JS_FN("toJSON", toJSON, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec URL::properties[] = {
    JS_PSGS("hash", URL::hash_get, URL::hash_set, JSPROP_ENUMERATE),
    JS_PSGS("host", URL::host_get, URL::host_set, JSPROP_ENUMERATE),
    JS_PSGS("hostname", URL::hostname_get, URL::hostname_set, JSPROP_ENUMERATE),
    JS_PSGS("href", URL::href_get, URL::href_set, JSPROP_ENUMERATE),
    JS_PSG("origin", URL::origin_get, JSPROP_ENUMERATE),
    JS_PSGS("password", URL::password_get, URL::password_set, JSPROP_ENUMERATE),
    JS_PSGS("pathname", URL::pathname_get, URL::pathname_set, JSPROP_ENUMERATE),
    JS_PSGS("port", URL::port_get, URL::port_set, JSPROP_ENUMERATE),
    JS_PSGS("protocol", URL::protocol_get, URL::protocol_set, JSPROP_ENUMERATE),
    JS_PSGS("search", URL::search_get, URL::search_set, JSPROP_ENUMERATE),
    JS_PSG("searchParams", URL::searchParams_get, JSPROP_ENUMERATE),
    JS_PSGS("username", URL::username_get, URL::username_set, JSPROP_ENUMERATE),
    JS_PS_END,
};

struct UrlKey {
  std::string key_;

  UrlKey() = default;
  UrlKey(std::string key) : key_(std::move(key)) {}

  void trace(JSTracer *trc) {}
};

struct UrlKeyHasher {
  using Lookup = const std::string&;

  static mozilla::HashNumber hash(Lookup lookup) {
    return mozilla::HashString(lookup.data());
  }

  static bool match(const UrlKey& key, Lookup lookup) {
    return key.key_ == lookup;
  }
};

static PersistentRooted<JS::GCHashMap<UrlKey, Heap<JSObject *>, UrlKeyHasher, js::SystemAllocPolicy>> URL_STORE;

bool URL::createObjectURL(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "createObjectURL", 1)) {
    return false;
  }

  HandleValue obj_val = args.get(0);
  if (!obj_val.isObject()) {
    return false;
  }

  RootedObject obj(cx, &obj_val.toObject());
  if (!obj) {
    return false;
  }

  if (!Blob::is_instance(obj) && !File::is_instance(obj)) {
    return false;
  }

  // To generate a new blob URL, run the following steps:
  // 1. Let result be the empty string.
  // 2. Append the string "blob:" to result.
  std::string result("blob:");

  // 3. Let settings be the current settings object
  // 4. Let origin be settings’s origin.
  // 5. Let serialized be the ASCII serialization of origin.
  // 6. If serialized is "null", set it to an implementation-defined value.
  RootedObject worker_location(cx, WorkerLocation::url.get());
  if (worker_location) {
    RootedValue origin(cx);
    if (!JS_GetProperty(cx, worker_location, "origin", &origin)) {
      return false;
    }

    auto origin_str = RootedString(cx, JS::ToString(cx, origin));
    if (!origin_str) {
      return false;
    }

    auto chars = core::encode(cx, origin_str);
    result.append(chars.ptr.get());
  }

  // 7. Append U+0024 SOLIDUS (/) to result.
  result.append("/");

  // 8. Generate a UUID [RFC4122] as a string and append it to result.
  auto maybe_uuid = crypto::random_uuid_v4(cx);
  if (!maybe_uuid.has_value()) {
    return false;
  }

  const auto& uuid = maybe_uuid.value();
  result.append(uuid);

  RootedString url(cx, JS_NewStringCopyN(cx, result.data(), result.size()));
  if (!url) {
    return false;
  }

  if (!URL_STORE.get().put(result, obj)) {
    return false;
  }

  args.rval().setString(url);
  return true;
}

bool URL::revokeObjectURL(JSContext *cx, unsigned argc, JS::Value *vp) {
  // Suppress false positive in Mozilla's HashTable implementation
  // about null dereference, the remove method will check if the entry exists.
  // NOLINTBEGIN
  CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "createObjectURL", 1)) {
    return false;
  }

  // The revokeObjectURL(url) static method must run these steps:
  // 1. Let urlRecord be the result of parsing url.
  auto chars = core::encode(cx, args.get(0));
  if (!chars.ptr) {
    return false;
  }

  // 2. If urlRecord’s scheme is not "blob", return.
  // 3. Let entry be urlRecord’s blob URL entry.
  std::string url_record(chars.ptr.get());
  if (!url_record.starts_with("blob:")) {
    return true;
  }

  // 4. If entry is null, then return.
  // 5. Let isAuthorized be the result of checking for same-partition blob URL usage with entry and the current settings object.
  // 6. If isAuthorized is false, then return.
  // 7. Remove an entry from the Blob URL Store for url.

  URL_STORE.get().remove(url_record);
  // NOLINTEND
  return true;
}

JSObject *URL::getObjectURL(std::string &url_str) {
  // To obtain a blob object given a blob URL entry blobUrlEntry:
  // 1. Let isAuthorized be true.
  // 2. If environment is not the string "navigation", then set isAuthorized to the result of checking for same-partition blob URL usage with blobUrlEntry and environment.
  // 3. If isAuthorized is false, then return failure.
  // 4. Return blobUrlEntry's object.
  auto url = URL_STORE.get().lookup(url_str);
  return url ? url->value() : nullptr;
}

const jsurl::JSUrl *URL::url(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<const jsurl::JSUrl *>(JS::GetReservedSlot(self, Url).toPrivate());
}

jsurl::JSUrl *URL::url_mut(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<jsurl::JSUrl *>(JS::GetReservedSlot(self, Url).toPrivate());
}

jsurl::SpecString URL::origin(JSContext *cx, JS::HandleObject self) {
  return jsurl::origin(url(self));
}

bool URL::origin(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval) {
  jsurl::SpecString slice = origin(cx, self);
  JS::RootedString str(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars((char *)slice.data, slice.len)));
  if (!str) {
    return false;
  }
  rval.setString(str);
  return true;
}

bool URL::origin_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return origin(cx, self, args.rval());
}

bool URL::searchParams_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  JS::Value params_val = JS::GetReservedSlot(self, Slots::Params);
  JS::RootedObject params(cx);
  if (params_val.isNullOrUndefined()) {
    JS::RootedObject url_search_params_instance(
        cx, JS_NewObjectWithGivenProto(cx, &URLSearchParams::class_, URLSearchParams::proto_obj));
    if (!url_search_params_instance) {
      return false;
    }

    params = URLSearchParams::create(cx, url_search_params_instance, url_mut(self));
    if (!params) {
      return false;
    }
    JS::SetReservedSlot(self, Slots::Params, JS::ObjectValue(*params));
  } else {
    params = &params_val.toObject();
  }

  args.rval().setObject(*params);
  return true;
}

bool URL::toString(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return href_get(cx, argc, vp);
}

bool URL::toJSON(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return href_get(cx, argc, vp);
}

bool URL::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("URL", 1);

  JS::RootedObject urlInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!urlInstance) {
    return false;
  }
  JS::RootedObject self(cx, create(cx, urlInstance, args.get(0), args.get(1)));
  if (!self) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

DEF_ERR(InvalidURLError, JSEXN_TYPEERR, "URL constructor: {0} is not a valid URL.", 1);

JSObject *URL::create(JSContext *cx, JS::HandleObject self, jsurl::SpecString url_str,
                      const jsurl::JSUrl *base) {
  jsurl::JSUrl *url = nullptr;
  if (base) {
    url = jsurl::new_jsurl_with_base(&url_str, base);
  } else {
    url = jsurl::new_jsurl(&url_str);
  }

  if (!url) {
    api::throw_error(cx, InvalidURLError, (char *)url_str.data);
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::Url, JS::PrivateValue(url));

  return self;
}

JSObject *URL::create(JSContext *cx, JS::HandleObject self, JS::HandleValue url_val,
                      const jsurl::JSUrl *base) {
  auto str = core::encode_spec_string(cx, url_val);
  if (!str.data) {
    return nullptr;
  }

  return create(cx, self, str, base);
}

JSObject *URL::create(JSContext *cx, JS::HandleObject self, JS::HandleValue url_val,
                      JS::HandleObject base_obj) {
  jsurl::JSUrl *base = nullptr;
  if (is_instance(base_obj)) {
    base = static_cast<jsurl::JSUrl *>(JS::GetReservedSlot(base_obj, Slots::Url).toPrivate());
  }

  return create(cx, self, url_val, base);
}

void URL::finalize(JS::GCContext *gcx, JSObject *self) {
  auto *url =
      static_cast<jsurl::JSUrl *>(JS::GetReservedSlot(self, Slots::Url).toPrivate());
  jsurl::free_jsurl(url);
}

JSObject *URL::create(JSContext *cx, JS::HandleObject self, JS::HandleValue url_val,
                      JS::HandleValue base_val) {
  if (is_instance(base_val)) {
    JS::RootedObject base_obj(cx, &base_val.toObject());
    return create(cx, self, url_val, base_obj);
  }

  jsurl::JSUrl *base = nullptr;

  if (!base_val.isUndefined()) {
    auto str = core::encode_spec_string(cx, base_val);
    if (!str.data) {
      return nullptr;
    }

    base = jsurl::new_jsurl(&str);
    if (!base) {
      api::throw_error(cx, InvalidURLError, (char *)str.data);
      return nullptr;
    }
  }

  return create(cx, self, url_val, base);
}

bool URL::init_class(JSContext *cx, JS::HandleObject global) {
  URL_STORE.init(cx);
  return URL::init_class_impl(cx, global);
}

bool install(api::Engine *engine) {
  if (!URL::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!URLSearchParams::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!URLSearchParamsIterator::init_class(engine->cx(), engine->global())) {
    return false;
  }
  return true;
}

} // namespace builtins::web::url
