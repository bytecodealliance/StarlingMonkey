#include "../dom-exception.h"
#include "crypto.h"
#include "host_api.h"
#include "subtle-crypto.h"
#include "uuid.h"

bool is_int_typed_array(JSObject *obj) {
  return JS_IsInt8Array(obj) || JS_IsUint8Array(obj) || JS_IsInt16Array(obj) ||
         JS_IsUint16Array(obj) || JS_IsInt32Array(obj) || JS_IsUint32Array(obj) ||
         JS_IsUint8ClampedArray(obj) || JS_IsBigInt64Array(obj) || JS_IsBigUint64Array(obj);
}

namespace builtins::web::crypto {

using dom_exception::DOMException;

#define MAX_BYTE_LENGTH 65536
/**
 * Implementation of
 * https://www.w3.org/TR/WebCryptoAPI/#Crypto-method-getRandomValues
 * TODO: investigate ways to automatically wipe the buffer passed in here when
 * it is GC'd. Content can roughly approximate that using finalizers for views
 * of the buffer, but it's far from ideal.
 */
bool Crypto::get_random_values(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  if (!args[0].isObject() || !is_int_typed_array(&args[0].toObject())) {
    DOMException::raise(cx, "crypto.getRandomValues: input must be an integer-typed TypedArray",
                        "TypeMismatchError");
    return false;
  }

  JS::RootedObject typed_array(cx, &args[0].toObject());
  size_t byte_length = JS_GetArrayBufferViewByteLength(typed_array);
  if (byte_length > MAX_BYTE_LENGTH) {
    std::string message = "crypto.getRandomValues: input byteLength must be at most ";
    message += std::to_string(MAX_BYTE_LENGTH);
    message += ", but is ";
    message += std::to_string(byte_length);
    DOMException::raise(cx, message, "QuotaExceededError");
    return false;
  }

  JS::AutoCheckCannotGC noGC(cx);
  bool is_shared = false;
  auto *buffer = static_cast<uint8_t *>(JS_GetArrayBufferViewData(typed_array, &is_shared, noGC));

  auto res = host_api::Random::get_bytes(byte_length);
  if (auto *err = res.to_err()) {
    noGC.reset();
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto bytes = std::move(res.unwrap());
  std::copy_n(bytes.begin(), byte_length, buffer);

  args.rval().setObject(*typed_array);
  return true;
}

bool Crypto::random_uuid(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  auto maybe_uuid = random_uuid_v4(cx);
  if (!maybe_uuid.has_value()) {
    return false;
  }

  auto uuid = maybe_uuid.value();
  MOZ_ASSERT(uuid.size() == 36);

  JS::RootedString str(cx, JS_NewStringCopyN(cx, uuid.data(), uuid.size()));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}
JS::PersistentRooted<JSObject *> Crypto::subtle;
JS::PersistentRooted<JSObject *> crypto;

bool Crypto::subtle_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "subtle get");
  if (self != crypto.get()) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "subtle get", "Crypto");
  }

  args.rval().setObject(*subtle);
  return true;
}

const JSFunctionSpec Crypto::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Crypto::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Crypto::methods[] = {
    JS_FN("getRandomValues", get_random_values, 1, JSPROP_ENUMERATE),
    JS_FN("randomUUID", random_uuid, 0, JSPROP_ENUMERATE), JS_FS_END};

const JSPropertySpec Crypto::properties[] = {
    JS_PSG("subtle", subtle_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Crypto", JSPROP_READONLY), JS_PS_END};

bool crypto_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  auto thisv = args.thisv();
  if (thisv != JS::UndefinedHandleValue && thisv != JS::ObjectValue(*global)) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "crypto get", "Window");
  }
  args.rval().setObject(*crypto);
  return true;
}

bool Crypto::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
    return false;
  }

  JS::RootedObject cryptoInstance(
      cx, JS_NewObjectWithGivenProto(cx, &Crypto::class_, Crypto::proto_obj));
  if (!cryptoInstance) {
    return false;
  }
  crypto.init(cx, cryptoInstance);

  JS::RootedObject subtleCrypto(
      cx, JS_NewObjectWithGivenProto(cx, &SubtleCrypto::class_, SubtleCrypto::proto_obj));
  subtle.init(cx, subtleCrypto);
  return JS_DefineProperty(cx, global, "crypto", crypto_get, nullptr, JSPROP_ENUMERATE);
}

bool install(api::Engine *engine) {
  if (!SubtleCrypto::init_class(engine->cx(), engine->global()))
    return false;
  if (!Crypto::init_class(engine->cx(), engine->global()))
    return false;
  if (!CryptoKey::init_class(engine->cx(), engine->global()))
    return false;
  return true;
}

} // namespace builtins::web::crypto
