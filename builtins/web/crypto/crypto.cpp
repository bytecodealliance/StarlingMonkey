// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/experimental/TypedData.h" // used in "js/Conversions.h"
#pragma clang diagnostic pop

#include "builtins/shared/dom-exception.h"
#include "crypto.h"
#include "subtle-crypto.h"

bool is_int_typed_array(JSObject *obj) {
  return JS_IsInt8Array(obj) || JS_IsUint8Array(obj) || JS_IsInt16Array(obj) ||
         JS_IsUint16Array(obj) || JS_IsInt32Array(obj) || JS_IsUint32Array(obj) ||
         JS_IsUint8ClampedArray(obj) || JS_IsBigInt64Array(obj) || JS_IsBigUint64Array(obj);
}

namespace builtins {
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
  bool is_shared;
  auto *buffer = static_cast<uint8_t *>(JS_GetArrayBufferViewData(typed_array, &is_shared, noGC));

  auto res = host_api::Random::get_bytes(byte_length);
  if (auto *err = res.to_err()) {
    HANDLE_ERROR(cx, *err);
    return false;
  }

  auto bytes = std::move(res.unwrap());
  std::copy_n(bytes.begin(), byte_length, buffer);

  args.rval().setObject(*typed_array);
  return true;
}

/*
  FROM RFC 4122
  The formal definition of the UUID string representation is
  provided by the following ABNF [7]:

  UUID                   = time-low "-" time-mid "-"
                            time-high-and-version "-"
                            clock-seq-and-reserved
                            clock-seq-low "-" node
  time-low               = 4hexOctet
  time-mid               = 2hexOctet
  time-high-and-version  = 2hexOctet
  clock-seq-and-reserved = hexOctet
  clock-seq-low          = hexOctet
  node                   = 6hexOctet
  hexOctet               = hexDigit hexDigit
  hexDigit =
        "0" / "1" / "2" / "3" / "4" / "5" / "6" / "7" / "8" / "9" /
        "a" / "b" / "c" / "d" / "e" / "f" /
        "A" / "B" / "C" / "D" / "E" / "F"
*/
struct UUID {
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_high_and_version;
  uint8_t clock_seq_and_reserved;
  uint8_t clock_seq_low;
  uint8_t node[6];
};

/*
  FROM RFC 4122
  4.1.3.  Version

  The version number is in the most significant 4 bits of the time
  stamp (bits 4 through 7 of the time_hi_and_version field).

  The following table lists the currently-defined versions for this
  UUID variant.

  Msb0  Msb1  Msb2  Msb3   Version  Description

    0     0     0     1        1    The time-based version
                                    specified in this document.

    0     0     1     0        2    DCE Security version, with
                                    embedded POSIX UIDs.

    0     0     1     1        3    The name-based version
                                    specified in this document
                                    that uses MD5 hashing.

    0     1     0     0        4    The randomly or pseudo-
                                    randomly generated version
                                    specified in this document.

    0     1     0     1        5    The name-based version
                                    specified in this document
                                    that uses SHA-1 hashing.
*/
/*
  FROM RFC 4122
  The version 4 UUID is meant for generating UUIDs from truly-random or
  pseudo-random numbers.

  The algorithm is as follows:

  Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved to zero and one,
  respectively.

  Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field to the
  4-bit version number from Section 4.1.3.

  Set all the other bits to randomly (or pseudo-randomly) chosen values.
*/
bool Crypto::random_uuid(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  UUID id;

  {
    auto res = host_api::Random::get_bytes(sizeof(id));
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return false;
    }

    auto bytes = std::move(res.unwrap());
    std::copy_n(bytes.begin(), sizeof(id), reinterpret_cast<uint8_t *>(&id));
  }

  // Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved to zero and
  // one, respectively.
  id.clock_seq_and_reserved &= 0x3f;
  id.clock_seq_and_reserved |= 0x80;
  // Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field to the
  // 4-bit version number from Section 4.1.3.
  id.time_high_and_version &= 0x0fff;
  id.time_high_and_version |= 0x4000;

  const char *fmt = "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";
  char buf[80];
  std::snprintf(buf, sizeof(buf) - 1, fmt, id.time_low, (uint32_t)id.time_mid,
                (uint32_t)id.time_high_and_version, (uint32_t)id.clock_seq_and_reserved,
                (uint32_t)id.clock_seq_low, (uint32_t)id.node[0], (uint32_t)id.node[1],
                (uint32_t)id.node[2], (uint32_t)id.node[3], (uint32_t)id.node[4],
                (uint32_t)id.node[5]);
  JS::RootedString str(cx, JS_NewStringCopyN(cx, buf, 36));

  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}
JS::PersistentRooted<JSObject *> Crypto::subtle;
JS::PersistentRooted<JSObject *> crypto;

bool Crypto::subtle_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  if (self != crypto.get()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_INTERFACE,
                              "subtle get", "Crypto");
    return false;
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

bool Crypto::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_ILLEGAL_CTOR);
  return false;
}

bool crypto_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  auto thisv = args.thisv();
  if (thisv != JS::UndefinedHandleValue && thisv != JS::ObjectValue(*global)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_INTERFACE,
                              "crypto get", "Window");
    return false;
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
} // namespace builtins
