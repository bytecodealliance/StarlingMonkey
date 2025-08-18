#include "crypto-key.h"
#include "crypto-algorithm.h"
#include "crypto-raii.h"
#include "encode.h"

#include "../dom-exception.h"

#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <utility>



namespace builtins::web::crypto {

CryptoKeyUsages::CryptoKeyUsages(uint8_t mask) : mask(mask) { };
CryptoKeyUsages::CryptoKeyUsages(bool encrypt, bool decrypt, bool sign, bool verify,
                                 bool derive_key, bool derive_bits, bool wrap_key,
                                 bool unwrap_key)  {
  
  if (encrypt) {
    this->mask |= encrypt_flag;
  }
  if (decrypt) {
    this->mask |= decrypt_flag;
  }
  if (sign) {
    this->mask |= sign_flag;
  }
  if (verify) {
    this->mask |= verify_flag;
  }
  if (derive_key) {
    this->mask |= derive_key_flag;
  }
  if (derive_bits) {
    this->mask |= derive_bits_flag;
  }
  if (wrap_key) {
    this->mask |= wrap_key_flag;
  }
  if (unwrap_key) {
    this->mask |= unwrap_key_flag;
  }
};

CryptoKeyUsages CryptoKeyUsages::from(const std::vector<std::string>& key_usages) {
  uint8_t mask = 0;
  for (const auto &usage : key_usages) {
    if (usage == "encrypt") {
      mask |= encrypt_flag;
    } else if (usage == "decrypt") {
      mask |= decrypt_flag;
    } else if (usage == "sign") {
      mask |= sign_flag;
    } else if (usage == "verify") {
      mask |= verify_flag;
    } else if (usage == "deriveKey") {
      mask |= derive_key_flag;
    } else if (usage == "deriveBits") {
      mask |= derive_bits_flag;
    } else if (usage == "wrapKey") {
      mask |= wrap_key_flag;
    } else if (usage == "unwrapKey") {
      mask |= unwrap_key_flag;
    }
  }
  return {mask};
}

JS::Result<CryptoKeyUsages> CryptoKeyUsages::from(JSContext *cx, JS::HandleValue key_usages) {
  bool key_usages_is_array = false;
  if (!JS::IsArrayObject(cx, key_usages, &key_usages_is_array)) {
    return JS::Result<CryptoKeyUsages>(JS::Error());
  }

  if (!key_usages_is_array) {
    // TODO: This should check if the JS::HandleValue is iterable and if so, should convert it into
    // a JS Array
    api::throw_error(cx, api::Errors::TypeError, "crypto.subtle.importKey", "keyUsages",
                     "be a sequence");
    return JS::Result<CryptoKeyUsages>(JS::Error());
  }

  JS::RootedObject array(cx, &key_usages.toObject());
  uint32_t key_usages_length = 0;
  if (!JS::GetArrayLength(cx, array, &key_usages_length)) {
    return JS::Result<CryptoKeyUsages>(JS::Error());
  }
  uint8_t mask = 0;
  for (uint32_t index = 0; index < key_usages_length; index++) {
    JS::RootedValue val(cx);
    if (!JS_GetElement(cx, array, index, &val)) {
      return JS::Result<CryptoKeyUsages>(JS::Error());
    }

    auto utf8chars = core::encode(cx, val);
    if (!utf8chars) {
      return JS::Result<CryptoKeyUsages>(JS::Error());
    }

    std::string_view usage = utf8chars;

    if (usage == "encrypt") {
      mask |= encrypt_flag;
    } else if (usage == "decrypt") {
      mask |= decrypt_flag;
    } else if (usage == "sign") {
      mask |= sign_flag;
    } else if (usage == "verify") {
      mask |= verify_flag;
    } else if (usage == "deriveKey") {
      mask |= derive_key_flag;
    } else if (usage == "deriveBits") {
      mask |= derive_bits_flag;
    } else if (usage == "wrapKey") {
      mask |= wrap_key_flag;
    } else if (usage == "unwrapKey") {
      mask |= unwrap_key_flag;
    } else {
      api::throw_error(
          cx, api::Errors::TypeError, "crypto.subtle.importKey",
          "each value in the 'keyUsages' list",
          "be one of 'encrypt', 'decrypt', 'sign', 'verify', 'deriveKey', 'deriveBits', "
          "'wrapKey', or 'unwrapKey'");
      return JS::Result<CryptoKeyUsages>(JS::Error());
    }
  }
  return CryptoKeyUsages(mask);
}

bool CryptoKey::algorithm_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj.get()) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "algorithm get", "CryptoKey");
  }

  auto *algorithm = &JS::GetReservedSlot(self, Slots::Algorithm).toObject();
  JS::RootedObject result(cx, algorithm);
  if (result == nullptr) {
    return false;
  }
  args.rval().setObject(*result);

  return true;
}

bool CryptoKey::extractable_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj.get()) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "extractable get", "CryptoKey");
  }

  auto extractable = JS::GetReservedSlot(self, Slots::Extractable).toBoolean();
  args.rval().setBoolean(extractable);

  return true;
}

bool CryptoKey::type_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)

  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj.get()) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "type get", "CryptoKey");
  }

  auto type = static_cast<CryptoKeyType>(JS::GetReservedSlot(self, Slots::Type).toInt32());

  // We store the type internally as a CryptoKeyType variant and need to
  // convert it into it's JSString representation.
  switch (type) {
  case CryptoKeyType::Private: {
    auto *str = JS_AtomizeString(cx, "private");
    if (str == nullptr) {
      return false;
    }
    args.rval().setString(str);
    return true;
  }
  case CryptoKeyType::Public: {
    auto *str = JS_AtomizeString(cx, "public");
    if (str == nullptr) {
      return false;
    }
    args.rval().setString(str);
    return true;
  }
  case CryptoKeyType::Secret: {
    auto *str = JS_AtomizeString(cx, "secret");
    if (str == nullptr) {
      return false;
    }
    args.rval().setString(str);
    return true;
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Unknown `CryptoKeyType` value");
    return false;
  }
  };
}

bool CryptoKey::usages_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj.get()) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "usages get", "CryptoKey");
  }

  // If the JS Array has already been created previously, return it.
  auto cached_usage = JS::GetReservedSlot(self, Slots::UsagesArray);
  if (cached_usage.isObject()) {
    args.rval().setObject(cached_usage.toObject());
    return true;
  }
  // Else, grab the CryptoKeyUsages value from Slots::Usages and convert
  // it into a JS Array and store the result in Slots::UsagesArray.
  auto usages = JS::GetReservedSlot(self, Slots::Usages).toInt32();
  MOZ_ASSERT(std::in_range<std::uint8_t>(usages));
  auto usage = CryptoKeyUsages(static_cast<uint8_t>(usages));
  // The result is ordered alphabetically.
  JS::RootedValueVector result(cx);
  JS::RootedString str(cx);
  auto append = [&](const char *name) -> bool {
    if (!(str = JS_AtomizeString(cx, name))) {
      return false;
    }
    if (!result.append(JS::StringValue(str))) {
      js::ReportOutOfMemory(cx);
      return false;
    }
    return true;
  };

  if (usage.canDecrypt()) {
    if (!append("decrypt")) {
      return false;
    }
  }
  if (usage.canDeriveBits()) {
    if (!append("deriveBits")) {
      return false;
    }
  }
  if (usage.canDeriveKey()) {
    if (!append("deriveKey")) {
      return false;
    }
  }
  if (usage.canEncrypt()) {
    if (!append("encrypt")) {
      return false;
    }
  }
  if (usage.canSign()) {
    if (!append("sign")) {
      return false;
    }
  }
  if (usage.canUnwrapKey()) {
    if (!append("unwrapKey")) {
      return false;
    }
  }
  if (usage.canVerify()) {
    if (!append("verify")) {
      return false;
    }
  }
  if (usage.canWrapKey()) {
    if (!append("wrapKey")) {
      return false;
    }
  }

  JS::Rooted<JSObject *> array(cx, JS::NewArrayObject(cx, result));
  if (array == nullptr) {
    return false;
  }
  cached_usage.setObject(*array);
  JS::SetReservedSlot(self, Slots::UsagesArray, cached_usage);

  args.rval().setObject(*array);
  return true;
}

const JSFunctionSpec CryptoKey::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec CryptoKey::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec CryptoKey::methods[] = {JS_FS_END};

const JSPropertySpec CryptoKey::properties[] = {
    JS_PSG("type", CryptoKey::type_get, JSPROP_ENUMERATE),
    JS_PSG("extractable", CryptoKey::extractable_get, JSPROP_ENUMERATE),
    JS_PSG("algorithm", CryptoKey::algorithm_get, JSPROP_ENUMERATE),
    JS_PSG("usages", CryptoKey::usages_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "CryptoKey", JSPROP_READONLY),
    JS_PS_END};

bool CryptoKey::init_class(JSContext *cx, JS::HandleObject global) {
  return BuiltinImpl<CryptoKey>::init_class_impl(cx, global);
}

namespace {

int bn_len(BIGNUM *a) { return BN_num_bytes(a) * 8; }

int curve_identifier(NamedCurve curve) {
  switch (curve) {
  case NamedCurve::P256:
    return NID_X9_62_prime256v1;
  case NamedCurve::P384:
    return NID_secp384r1;
  case NamedCurve::P521:
    return NID_secp521r1;
  }

  MOZ_ASSERT_UNREACHABLE();
  return 0;
}

BignumPtr make_bignum(std::string_view bytes) {
  if (bytes.empty()) {
    return nullptr;
  }

  auto *bn = BN_bin2bn(reinterpret_cast<const unsigned char *>(bytes.data()),
                      static_cast<int>(bytes.length()), nullptr);
  return BignumPtr(bn);
}

BignumPtr make_bignum_with_padding(std::string_view bytes, size_t expected_length) {
  if (bytes.length() != expected_length) {
    return nullptr;
  }

  return make_bignum(bytes);
}

const char *get_curve_name(int curve_nid) {
  switch (curve_nid) {
  case NID_X9_62_prime256v1:
    return "prime256v1";
  case NID_secp384r1:
    return "secp384r1";
  case NID_secp521r1:
    return "secp521r1";
  default:
    return nullptr;
  }
}

int get_curve_degree_bytes(int curve_nid) {
  switch (curve_nid) {
  case NID_X9_62_prime256v1:
    return 32;
  case NID_secp384r1:
    return 48;
  case NID_secp521r1:
    return 66;
  default:
    return -1;
  }
}

bool validate_key(JSContext *cx, const EvpPkeyPtr &pkey, bool has_private) {
  if (!pkey) {
    return false;
  }

  auto check_ctx = EvpPkeyCtxPtr(EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (!check_ctx) {
    return false;
  }

  int valid = has_private ? EVP_PKEY_pairwise_check(check_ctx.get())
                          : EVP_PKEY_public_check(check_ctx.get());

  if (valid != 1) {
    const auto *reason = ERR_reason_error_string(ERR_get_error());
    dom_exception::DOMException::raise(cx, "KeyValidation", reason);
    return false;
  }

  return true;
}

EvpPkeyPtr create_ec_key_from_parts(JSContext *cx, CryptoAlgorithmECDSA_Import *algorithm,
                                    const std::string_view &x_coord,
                                    const std::string_view &y_coord,
                                    const std::string_view &private_key = {}) {
  if (x_coord.empty() || y_coord.empty()) {
    return nullptr;
  }

  auto has_private_key = !private_key.empty();
  auto curve_nid = curve_identifier(algorithm->namedCurve);
  const auto *curve_name = get_curve_name(curve_nid);
  MOZ_ASSERT(curve_name);

  auto group = EcGroupPtr(EC_GROUP_new_by_curve_name(curve_nid));
  if (!group) {
    return nullptr;
  }

  auto degree_bytes = get_curve_degree_bytes(curve_nid);
  auto x_bn = make_bignum_with_padding(x_coord, degree_bytes);
  if (!x_bn) {
    return nullptr;
  }

  auto y_bn = make_bignum_with_padding(y_coord, degree_bytes);
  if (!y_bn) {
    return nullptr;
  }

  auto point = EcPointPtr(EC_POINT_new(group.get()));
  if (!point) {
    return nullptr;
  }

  if (EC_POINT_set_affine_coordinates(group.get(), point.get(), x_bn.get(), y_bn.get(), nullptr) == 0) {
    return nullptr;
  }

  auto form = EC_GROUP_get_point_conversion_form(group.get());
  unsigned char *pub_key = nullptr;
  auto pub_key_len = EC_POINT_point2buf(group.get(), point.get(), form, &pub_key, nullptr);
  if (pub_key_len == 0 || (pub_key == nullptr)) {
    return nullptr;
  }

  auto bld = ParamBldPtr(OSSL_PARAM_BLD_new());
  if (!bld) {
    return nullptr;
  }

  if ((OSSL_PARAM_BLD_push_utf8_string(bld.get(), OSSL_PKEY_PARAM_GROUP_NAME, curve_name, 0) == 0) ||
      (OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_EC_PUB_X, x_bn.get()) == 0) ||
      (OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_EC_PUB_Y, y_bn.get()) == 0) ||
      (OSSL_PARAM_BLD_push_octet_string(bld.get(), OSSL_PKEY_PARAM_PUB_KEY, pub_key, pub_key_len) == 0)) {
    return nullptr;
  }

  BignumPtr d_bn = nullptr; // bignum must outlive the OSSL_PARAM_BLD
  if (has_private_key) {
    d_bn = make_bignum_with_padding(private_key, degree_bytes);
    if (!d_bn) {
      return nullptr;
    }

    if (OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_PRIV_KEY, d_bn.get()) == 0) {
      return nullptr;
    }
  }

  auto params = ParamPtr(OSSL_PARAM_BLD_to_param(bld.get()));
  if (!params) {
    return nullptr;
  }

  auto ctx = EvpPkeyCtxPtr(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
  if (!ctx) {
    return nullptr;
  }

  if (EVP_PKEY_fromdata_init(ctx.get()) <= 0) {
    return nullptr;
  }

  EVP_PKEY *pkey_raw = nullptr;
  int key_type = private_key.empty() ? EVP_PKEY_PUBLIC_KEY : EVP_PKEY_KEYPAIR;

  int result = EVP_PKEY_fromdata(ctx.get(), &pkey_raw, key_type, params.get());
  if (result <= 0 || (pkey_raw == nullptr)) {
    return nullptr;
  }

  auto pkey = EvpPkeyPtr(pkey_raw);
  if (!pkey || !validate_key(cx, pkey, has_private_key)) {
    return nullptr;
  }

  return pkey;
}

EvpPkeyPtr create_rsa_key_from_parts(
    JSContext *cx, const std::string_view &modulus, const std::string_view &public_exponent,
    const std::string_view &private_exponent = {}, const std::string_view &prime1 = {},
    const std::string_view &prime2 = {}, const std::string_view &exponent1 = {},
    const std::string_view &exponent2 = {}, const std::string_view &coefficient = {}) {

  auto param_bld = ParamBldPtr(OSSL_PARAM_BLD_new());
  if (!param_bld) {
    return nullptr;
  }

  auto n_bn = make_bignum(modulus);
  if (!n_bn || (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_N, n_bn.get()) == 0)) {
    return nullptr;
  }

  auto e_bn = make_bignum(public_exponent);
  if (!e_bn || (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_E, e_bn.get()) == 0)) {
    return nullptr;
  }

  // bignum must outlive the OSSL_PARAM_BLD
  BignumPtr d_bn = nullptr;
  BignumPtr p_bn = nullptr;
  BignumPtr q_bn = nullptr;
  BignumPtr dmp1_bn = nullptr;
  BignumPtr dmq1_bn = nullptr;
  BignumPtr iqmp_bn = nullptr;

  bool is_private = !private_exponent.empty();
  if (is_private) {
    d_bn = make_bignum(private_exponent);
    if (!d_bn || (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_D, d_bn.get()) == 0)) {
      return nullptr;
    }

    p_bn = make_bignum(prime1);
    if (!p_bn ||
        (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_FACTOR1, p_bn.get()) == 0)) {
      return nullptr;
    }

    q_bn = make_bignum(prime2);
    if (!q_bn ||
        (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_FACTOR2, q_bn.get()) == 0)) {
      return nullptr;
    }

    dmp1_bn = make_bignum(exponent1);
    if (!dmp1_bn ||
        (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_EXPONENT1, dmp1_bn.get()) == 0)) {
      return nullptr;
    }

    dmq1_bn = make_bignum(exponent2);
    if (!dmq1_bn ||
        (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_EXPONENT2, dmq1_bn.get()) == 0)) {
      return nullptr;
    }

    iqmp_bn = make_bignum(coefficient);
    if (!iqmp_bn ||
        (OSSL_PARAM_BLD_push_BN(param_bld.get(), OSSL_PKEY_PARAM_RSA_COEFFICIENT1, iqmp_bn.get()) == 0)) {
      return nullptr;
    }
  }

  auto params = ParamPtr(OSSL_PARAM_BLD_to_param(param_bld.get()));
  if (!params) {
    return nullptr;
  }

  auto ctx = EvpPkeyCtxPtr(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
  if (!ctx) {
    return nullptr;
  }

  if (EVP_PKEY_fromdata_init(ctx.get()) <= 0) {
    return nullptr;
  }

  EVP_PKEY *pkey_raw = nullptr;
  int key_type = is_private ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY;
  if (EVP_PKEY_fromdata(ctx.get(), &pkey_raw, key_type, params.get()) <= 0) {
    return nullptr;
  }

  auto pkey = EvpPkeyPtr(pkey_raw);
  if (!pkey || !validate_key(cx, pkey, is_private)) {
    return nullptr;
  }

  return pkey;
}

} // namespace

JSObject *CryptoKey::createHMAC(JSContext *cx, CryptoAlgorithmHMAC_Import *algorithm,
                                std::unique_ptr<std::span<uint8_t>> data, unsigned long length,
                                bool extractable, CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(algorithm);
  JS::RootedObject instance(
      cx, JS_NewObjectWithGivenProto(cx, &CryptoKey::class_, CryptoKey::proto_obj));
  if (instance == nullptr) {
    return nullptr;
  }

  JS::RootedObject alg(cx, algorithm->toObject(cx));
  if (alg == nullptr) {
    return nullptr;
  }

  JS::SetReservedSlot(instance, Slots::Algorithm, JS::ObjectValue(*alg));
  JS::SetReservedSlot(instance, Slots::Type, JS::Int32Value(static_cast<uint8_t>(CryptoKeyType::Secret)));
  JS::SetReservedSlot(instance, Slots::Extractable, JS::BooleanValue(extractable));
  JS::SetReservedSlot(instance, Slots::Usages, JS::Int32Value(usages.toInt()));
  JS::SetReservedSlot(instance, Slots::KeyDataLength, JS::Int32Value(data->size()));
  JS::SetReservedSlot(instance, Slots::KeyData, JS::PrivateValue(data.release()->data()));
  return instance;
}

JSObject *CryptoKey::createECDSA(JSContext *cx, CryptoAlgorithmECDSA_Import *algorithm,
                                 std::unique_ptr<CryptoKeyECComponents> keyData, bool extractable,
                                 CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(algorithm);

  CryptoKeyType keyType;
  switch (keyData->type) {
  case CryptoKeyECComponents::Type::Public: {
    keyType = CryptoKeyType::Public;
    break;
  }
  case CryptoKeyECComponents::Type::Private: {
    keyType = CryptoKeyType::Private;
    break;
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Unknown `CryptoKeyECComponents::Type` value");
    return nullptr;
  }
  }

  // When creating a private key, we require the d information.
  if (keyType == CryptoKeyType::Private && keyData->d.empty()) {
    return nullptr;
  }

  // For both public and private keys, we need the public x and y.
  if (keyData->x.empty() || keyData->y.empty()) {
    return nullptr;
  }

  std::string_view private_key_data =
      keyType == CryptoKeyType::Private ? keyData->d : std::string_view{};

  auto pkey = create_ec_key_from_parts(cx, algorithm, keyData->x, keyData->y, private_key_data);

  if (!pkey) {
    return nullptr;
  }

  JS::RootedObject instance(
      cx, JS_NewObjectWithGivenProto(cx, &CryptoKey::class_, CryptoKey::proto_obj));
  if (instance == nullptr) {
    return nullptr;
  }

  JS::RootedObject alg(cx, algorithm->toObject(cx));
  if (alg == nullptr) {
    return nullptr;
  }

  JS::SetReservedSlot(instance, Slots::Algorithm, JS::ObjectValue(*alg));
  JS::SetReservedSlot(instance, Slots::Type, JS::Int32Value(static_cast<uint8_t>(keyType)));
  JS::SetReservedSlot(instance, Slots::Extractable, JS::BooleanValue(extractable));
  JS::SetReservedSlot(instance, Slots::Usages, JS::Int32Value(usages.toInt()));
  JS::SetReservedSlot(instance, Slots::Key, JS::PrivateValue(pkey.release()));
  return instance;
}

JSObject *CryptoKey::createRSA(JSContext *cx, CryptoAlgorithmRSASSA_PKCS1_v1_5_Import *algorithm,
                               std::unique_ptr<CryptoKeyRSAComponents> keyData, bool extractable,
                               CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(algorithm);

  CryptoKeyType keyType;
  switch (keyData->type) {
  case CryptoKeyRSAComponents::Type::Public: {
    keyType = CryptoKeyType::Public;
    break;
  }
  case CryptoKeyRSAComponents::Type::Private: {
    keyType = CryptoKeyType::Private;
    break;
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Unknown `CryptoKeyRSAComponents::Type` value");
    return nullptr;
  }
  }

  // When creating a private key, we require the p and q prime information.
  const auto is_private = keyType == CryptoKeyType::Private;
  if (is_private && !keyData->hasAdditionalPrivateKeyParameters) {
    return nullptr;
  }

  // But we don't currently support creating keys with any additional prime information.
  if (!keyData->otherPrimeInfos.empty()) {
    return nullptr;
  }

  // For both public and private keys, we need the public modulus and exponent.
  if (keyData->modulus.empty() || keyData->exponent.empty()) {
    return nullptr;
  }

  // For private keys, we require the private exponent, as well as p and q prime information.
  if (is_private) {
    if (keyData->privateExponent.empty() || keyData->firstPrimeInfo->primeFactor.empty() ||
        keyData->secondPrimeInfo->primeFactor.empty()) {
      return nullptr;
    }
  }

  const auto get_param = [=](std::string_view value) {
    return is_private ? value : std::string_view{};
  };
  const auto get_param2 = [=](auto &&info, auto &&member) {
    return (is_private && info) ? member : std::string_view{};
  };

  auto private_exponent = get_param(keyData->privateExponent);
  auto prime1 = get_param2(keyData->firstPrimeInfo, keyData->firstPrimeInfo->primeFactor);
  auto prime2 = get_param2(keyData->secondPrimeInfo, keyData->secondPrimeInfo->primeFactor);
  auto exponent1 = get_param2(keyData->firstPrimeInfo, keyData->firstPrimeInfo->factorCRTExponent);
  auto exponent2 = get_param2(keyData->secondPrimeInfo, keyData->secondPrimeInfo->factorCRTExponent);
  auto coeff = get_param2(keyData->secondPrimeInfo, keyData->secondPrimeInfo->factorCRTCoefficient);

  auto pkey = create_rsa_key_from_parts(cx, keyData->modulus, keyData->exponent, private_exponent,
                                        prime1, prime2, exponent1, exponent2, coeff);
  if (!pkey) {
    return nullptr;
  }

  auto n_copy = make_bignum(keyData->modulus);
  if (!n_copy) {
    return nullptr;
  }

  JS::RootedObject instance(
      cx, JS_NewObjectWithGivenProto(cx, &CryptoKey::class_, CryptoKey::proto_obj));
  if (instance == nullptr) {
    return nullptr;
  }

  JS::RootedObject alg(cx, algorithm->toObject(cx));
  if (alg == nullptr) {
    return nullptr;
  }

  // Set the modulusLength attribute of algorithm to the length, in bits, of the RSA public modulus.
  JS::RootedValue modulusLength(cx, JS::NumberValue(bn_len(n_copy.get())));
  if (!JS_SetProperty(cx, alg, "modulusLength", modulusLength)) {
    return nullptr;
  }

  auto p = mozilla::MakeUnique<uint8_t[]>(keyData->exponent.size());
  auto exp = keyData->exponent;
  std::copy(exp.begin(), exp.end(), p.get());

  JS::RootedObject buffer(
      cx, JS::NewArrayBufferWithContents(cx, keyData->exponent.size(), p.get(),
                                         JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));

  // `buffer` takes ownership of `p` if the call to NewArrayBufferWithContents was successful
  // if the call was not successful, we need to free `p` before exiting from the function.
  if (buffer == nullptr) {
    // We can be here if the array buffer was too large -- if that was the case then a
    // JSMSG_BAD_ARRAY_LENGTH will have been created. Otherwise we're probably out of memory.
    if (!JS_IsExceptionPending(cx)) {
      js::ReportOutOfMemory(cx);
    }
    return nullptr;
  }

  // At this point, `buffer` owns the memory managed by `p`.
  static_cast<void>(p.release());

  // Set the publicExponent attribute of algorithm to the BigInteger representation of the RSA
  // public exponent.
  JS::RootedObject byte_array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, keyData->exponent.size()));
  JS::RootedValue publicExponent(cx, JS::ObjectValue(*byte_array));
  if (!JS_SetProperty(cx, alg, "publicExponent", publicExponent)) {
    return nullptr;
  }

  JS::SetReservedSlot(instance, Slots::Algorithm, JS::ObjectValue(*alg));
  JS::SetReservedSlot(instance, Slots::Type, JS::Int32Value(static_cast<uint8_t>(keyType)));
  JS::SetReservedSlot(instance, Slots::Extractable, JS::BooleanValue(extractable));
  JS::SetReservedSlot(instance, Slots::Usages, JS::Int32Value(usages.toInt()));
  JS::SetReservedSlot(instance, Slots::Key, JS::PrivateValue(pkey.release()));
  return instance;
}

CryptoKeyType CryptoKey::type(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<CryptoKeyType>(JS::GetReservedSlot(self, Slots::Type).toInt32());
}

JSObject *CryptoKey::get_algorithm(JS::HandleObject self) {
  MOZ_ASSERT(is_instance(self));
  auto *algorithm = JS::GetReservedSlot(self, Slots::Algorithm).toObjectOrNull();
  return algorithm;
}

EVP_PKEY *CryptoKey::key(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<EVP_PKEY *>(JS::GetReservedSlot(self, Slots::Key).toPrivate());
}

std::span<uint8_t> CryptoKey::hmacKeyData(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return {
      static_cast<uint8_t *>(JS::GetReservedSlot(self, Slots::KeyData).toPrivate()),
      static_cast<size_t>(JS::GetReservedSlot(self, Slots::KeyDataLength).toInt32())};
}

JS::Result<bool> CryptoKey::is_algorithm(JSContext *cx, JS::HandleObject self,
                                         CryptoAlgorithmIdentifier algorithm) {
  MOZ_ASSERT(CryptoKey::is_instance(self));
  JS::RootedObject self_algorithm(cx, JS::GetReservedSlot(self, Slots::Algorithm).toObjectOrNull());
  MOZ_ASSERT(self_algorithm != nullptr);
  JS::Rooted<JS::Value> name_val(cx);
  if (!JS_GetProperty(cx, self_algorithm, "name", &name_val)) {
    return JS::Result<bool>(JS::Error());
  }
  JS::Rooted<JSString *> str(cx, JS::ToString(cx, name_val));
  if (str == nullptr) {
    return JS::Result<bool>(JS::Error());
  }
  // TODO: should chars be used?
  auto chars = core::encode(cx, str);
  if (!chars) {
    return JS::Result<bool>(JS::Error());
  }
  bool match = false;
  if (!JS_StringEqualsAscii(cx, JS::ToString(cx, name_val), algorithmName(algorithm), &match)) {
    return JS::Result<bool>(JS::Error());
  }
  return match;
}

bool CryptoKey::canSign(JS::HandleObject self) {
  MOZ_ASSERT(is_instance(self));
  auto usages = JS::GetReservedSlot(self, Slots::Usages).toInt32();
  MOZ_ASSERT(std::in_range<std::uint8_t>(usages));
  auto usage = CryptoKeyUsages(static_cast<uint8_t>(usages));
  return usage.canSign();
}

bool CryptoKey::canVerify(JS::HandleObject self) {
  MOZ_ASSERT(is_instance(self));
  auto usages = JS::GetReservedSlot(self, Slots::Usages).toInt32();
  MOZ_ASSERT(std::in_range<std::uint8_t>(usages));
  auto usage = CryptoKeyUsages(static_cast<uint8_t>(usages));
  return usage.canVerify();
}

} // namespace builtins::web::crypto


