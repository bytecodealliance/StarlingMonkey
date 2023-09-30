// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/experimental/TypedData.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#pragma clang diagnostic pop

#include "builtins/json-web-key.h"
#include "core/encode.h"
#include "js-compute-builtins.h"

namespace builtins {

namespace {
JS::Result<std::optional<std::string>>
extractStringPropertyFromObject(JSContext *cx, JS::HandleObject object, std::string_view property) {
  bool has_property;
  if (!JS_HasProperty(cx, object, property.data(), &has_property)) {
    return JS::Result<std::optional<std::string>>(JS::Error());
  }
  if (!has_property) {
    return std::optional<std::string>(std::nullopt);
  }
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, object, property.data(), &value)) {
    return JS::Result<std::optional<std::string>>(JS::Error());
  }
  // Convert into a String following https://tc39.es/ecma262/#sec-tostring
  auto chars = core::encode(cx, value);
  if (!chars) {
    return JS::Result<std::optional<std::string>>(JS::Error());
  }
  return std::optional<std::string>(std::string(chars.begin(), chars.len));
}
} // namespace

std::unique_ptr<JsonWebKey> JsonWebKey::parse(JSContext *cx, JS::HandleValue value,
                                              std::string_view required_kty_value) {
  if (!value.isObject()) {
    JS_ReportErrorLatin1(cx, "The provided value is not of type JsonWebKey");
    return nullptr;
  }
  JS::RootedObject object(cx, &value.toObject());

  //   DOMString kty;
  auto kty_result = extractStringPropertyFromObject(cx, object, "kty");
  if (kty_result.isErr()) {
    return nullptr;
  }
  auto kty_option = kty_result.unwrap();
  if (!kty_option.has_value()) {
    JS_ReportErrorASCII(cx, "The required JWK member 'kty' was missing");
    return nullptr;
  }
  auto kty = kty_option.value();
  if (kty != required_kty_value) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SUBTLE_CRYPTO_INVALID_JWK_KTY_VALUE, required_kty_value.data());
    return nullptr;
  }

  //   DOMString use;
  auto use_result = extractStringPropertyFromObject(cx, object, "use");
  if (use_result.isErr()) {
    return nullptr;
  }
  auto use = use_result.unwrap();

  //   DOMString alg;
  auto alg_result = extractStringPropertyFromObject(cx, object, "alg");
  if (alg_result.isErr()) {
    return nullptr;
  }
  auto alg = alg_result.unwrap();

  //   DOMString crv;
  auto crv_result = extractStringPropertyFromObject(cx, object, "crv");
  if (crv_result.isErr()) {
    return nullptr;
  }
  auto crv = crv_result.unwrap();

  //   DOMString x;
  auto x_result = extractStringPropertyFromObject(cx, object, "x");
  if (x_result.isErr()) {
    return nullptr;
  }
  auto x = x_result.unwrap();

  //   DOMString y;
  auto y_result = extractStringPropertyFromObject(cx, object, "y");
  if (y_result.isErr()) {
    return nullptr;
  }
  auto y = y_result.unwrap();

  //   DOMString d;
  auto d_result = extractStringPropertyFromObject(cx, object, "d");
  if (d_result.isErr()) {
    return nullptr;
  }
  auto d = d_result.unwrap();

  //   DOMString n;
  auto n_result = extractStringPropertyFromObject(cx, object, "n");
  if (n_result.isErr()) {
    return nullptr;
  }
  auto n = n_result.unwrap();

  //   DOMString e;
  auto e_result = extractStringPropertyFromObject(cx, object, "e");
  if (e_result.isErr()) {
    return nullptr;
  }
  auto e = e_result.unwrap();

  //   DOMString p;
  auto p_result = extractStringPropertyFromObject(cx, object, "p");
  if (p_result.isErr()) {
    return nullptr;
  }
  auto p = p_result.unwrap();

  //   DOMString q;
  auto q_result = extractStringPropertyFromObject(cx, object, "q");
  if (q_result.isErr()) {
    return nullptr;
  }
  auto q = q_result.unwrap();

  //   DOMString dp;
  auto dp_result = extractStringPropertyFromObject(cx, object, "dp");
  if (dp_result.isErr()) {
    return nullptr;
  }
  auto dp = dp_result.unwrap();

  //   DOMString dq;
  auto dq_result = extractStringPropertyFromObject(cx, object, "dq");
  if (dq_result.isErr()) {
    return nullptr;
  }
  auto dq = dq_result.unwrap();

  //   DOMString qi;
  auto qi_result = extractStringPropertyFromObject(cx, object, "qi");
  if (qi_result.isErr()) {
    return nullptr;
  }
  auto qi = qi_result.unwrap();

  //   DOMString k;
  auto k_result = extractStringPropertyFromObject(cx, object, "k");
  if (k_result.isErr()) {
    return nullptr;
  }
  auto k = k_result.unwrap();

  //   boolean ext;
  std::optional<bool> ext = std::nullopt;
  {
    bool has_ext;
    if (!JS_HasProperty(cx, object, "ext", &has_ext)) {
      return nullptr;
    }
    if (has_ext) {
      JS::RootedValue ext_val(cx);
      if (!JS_GetProperty(cx, object, "ext", &ext_val)) {
        return nullptr;
      }
      ext = JS::ToBoolean(ext_val);
    }
  }
  //   sequence<DOMString> key_ops;
  std::vector<std::string> key_ops;
  {
    bool has_key_ops;
    if (!JS_HasProperty(cx, object, "key_ops", &has_key_ops)) {
      return nullptr;
    }
    if (has_key_ops) {
      JS::RootedValue key_ops_val(cx);
      if (!JS_GetProperty(cx, object, "key_ops", &key_ops_val)) {
        return nullptr;
      }
      bool key_ops_is_array;
      if (!JS::IsArrayObject(cx, key_ops_val, &key_ops_is_array)) {
        return nullptr;
      }
      if (!key_ops_is_array) {
        // TODO: Check if key_ops_val is iterable via Symbol.iterator and if so, convert to a JS
        // Array
        JS_ReportErrorASCII(cx, "Failed to read the 'key_ops' property from 'JsonWebKey': The "
                                "provided value cannot be converted to a sequence");
        return nullptr;
      }
      uint32_t length;
      JS::RootedObject key_ops_array(cx, &key_ops_val.toObject());
      if (!JS::GetArrayLength(cx, key_ops_array, &length)) {
        return nullptr;
      }
      if (length != 0) {
        // Initialize to the provided array's length.
        // TODO: move to mozilla::Vector so we can catch if this reserve exceeds our memory limits
        key_ops.reserve(length);
        JS::RootedValue op_val(cx);
        for (uint32_t i = 0; i < length; ++i) {
          // We should check for and handle interrupts so we can allow GC to happen whilst we are
          // iterating.
          // TODO: Go through entire codebase and add JS_CheckForInterrupt into code-paths which
          // iterate or are recursive such as the structuredClone function if
          // (!JS_CheckForInterrupt(cx)) {
          //   return nullptr;
          // }

          if (!JS_GetElement(cx, key_ops_array, i, &op_val)) {
            return nullptr;
          }

          auto op_chars = core::encode(cx, op_val);
          if (!op_chars) {
            return nullptr;
          }

          std::string op(op_chars.begin(), op_chars.len);

          if (op != "encrypt" && op != "decrypt" && op != "sign" && op != "verify" &&
              op != "deriveKey" && op != "deriveBits" && op != "wrapKey" && op != "unwrapKey") {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_SUBTLE_CRYPTO_INVALID_KEY_USAGES_VALUE);
            return nullptr;
          }

          // No duplicates allowed
          if (std::find(key_ops.begin(), key_ops.end(), op) != key_ops.end()) {
            JS_ReportErrorASCII(
                cx, "The 'key_ops' member of the JWK dictionary contains duplicate usages");
            return nullptr;
          }

          key_ops.push_back(std::move(op));
        }
      }
    }
  }

  std::vector<RsaOtherPrimesInfo> oth;
  {
    bool has_oth;
    if (!JS_HasProperty(cx, object, "oth", &has_oth)) {
      return nullptr;
    }
    if (has_oth) {
      JS::RootedValue oth_val(cx);
      if (!JS_GetProperty(cx, object, "oth", &oth_val)) {
        return nullptr;
      }
      bool oth_is_array;
      if (!JS::IsArrayObject(cx, oth_val, &oth_is_array)) {
        return nullptr;
      }
      if (!oth_is_array) {
        // TODO: Check if oth_val is iterable via Symbol.iterator and if so, convert to a JS Array
        JS_ReportErrorASCII(cx, "Failed to read the 'oth' property from 'JsonWebKey': The provided "
                                "value cannot be converted to a sequence");
        return nullptr;
      }
      uint32_t length;
      JS::RootedObject oth_array(cx, &oth_val.toObject());
      if (!JS::GetArrayLength(cx, oth_array, &length)) {
        return nullptr;
      }
      if (length != 0) {
        // Initialize to the provided array's length.
        // TODO: move to mozilla::Vector so we can catch if this reserve exceeds our memory limits
        oth.reserve(length);
        JS::RootedValue info_val(cx);
        for (uint32_t i = 0; i < length; ++i) {
          // We should check for and handle interrupts so we can allow GC to happen whilst we are
          // iterating.
          // TODO: Go through entire codebase and add JS_CheckForInterrupt into code-paths which
          // iterate or are recursive such as the structuredClone function if
          // (!JS_CheckForInterrupt(cx)) {
          //   return nullptr;
          // }

          if (!JS_GetElement(cx, oth_array, i, &info_val)) {
            return nullptr;
          }

          if (!info_val.isObject()) {
            JS_ReportErrorASCII(cx, "Failed to read the 'oth' property from 'JsonWebKey': The "
                                    "provided value is not of type 'RsaOtherPrimesInfo'");
            return nullptr;
          }
          JS::RootedObject info_obj(cx, &info_val.toObject());

          JS::RootedValue r_val(cx);
          if (!JS_GetProperty(cx, info_obj, "r", &r_val)) {
            return nullptr;
          }
          auto r_chars = core::encode(cx, info_val);
          if (!r_chars) {
            JS_ReportErrorASCII(cx, "Failed to read the 'oth' property from 'JsonWebKey': The "
                                    "provided value is not of type 'RsaOtherPrimesInfo'");
            return nullptr;
          }
          std::string r(r_chars.begin(), r_chars.len);
          JS::RootedValue d_val(cx);
          if (!JS_GetProperty(cx, info_obj, "d", &d_val)) {
            return nullptr;
          }
          auto d_chars = core::encode(cx, info_val);
          if (!d_chars) {
            JS_ReportErrorASCII(cx, "Failed to read the 'oth' property from 'JsonWebKey': The "
                                    "provided value is not of type 'RsaOtherPrimesInfo'");
            return nullptr;
          }
          std::string d(d_chars.begin(), d_chars.len);

          JS::RootedValue t_val(cx);
          if (!JS_GetProperty(cx, info_obj, "t", &t_val)) {
            return nullptr;
          }
          auto t_chars = core::encode(cx, info_val);
          if (!t_chars) {
            JS_ReportErrorASCII(cx, "Failed to read the 'oth' property from 'JsonWebKey': The "
                                    "provided value is not of type 'RsaOtherPrimesInfo'");
            return nullptr;
          }
          std::string t(t_chars.begin(), t_chars.len);

          oth.emplace_back(r, d, t);
        }
      }
    }
  }
  return std::make_unique<JsonWebKey>(kty, use, key_ops, alg, ext, crv, x, y, n, e, d, p, q, dp, dq,
                                      qi, oth, k);
}
} // namespace builtins
