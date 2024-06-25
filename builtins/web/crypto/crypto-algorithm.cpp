#include "openssl/rsa.h"
#include "openssl/sha.h"
#include <fmt/format.h>
#include <iostream>
#include <openssl/ecdsa.h>
#include <optional>
#include <span>
#include <vector>

#include "../base64.h"
#include "../dom-exception.h"
#include "crypto-algorithm.h"

#include "crypto-key-ec-components.h"
#include "crypto-key-rsa-components.h"
#include "encode.h"

namespace builtins::web::crypto {

using dom_exception::DOMException;

// namespace {

int numBitsToBytes(int x) { return (x / 8) + (7 + (x % 8)) / 8; }

std::pair<mozilla::UniquePtr<uint8_t[], JS::FreePolicy>, size_t>
convertToBytesExpand(JSContext *cx, const BIGNUM *bignum, size_t minimumBufferSize) {
  int length = BN_num_bytes(bignum);

  size_t bufferSize = std::max<size_t>(length, minimumBufferSize);
  mozilla::UniquePtr<uint8_t[], JS::FreePolicy> bytes{
      static_cast<uint8_t *>(JS_malloc(cx, bufferSize))};

  size_t paddingLength = bufferSize - length;
  if (paddingLength > 0) {
    uint8_t padding = BN_is_negative(bignum) ? 0xFF : 0x00;
    std::fill_n(bytes.get(), paddingLength, padding);
  }
  BN_bn2bin(bignum, bytes.get() + paddingLength);
  return std::pair<mozilla::UniquePtr<uint8_t[], JS::FreePolicy>, size_t>(std::move(bytes),
                                                                          bufferSize);
}

const EVP_MD *createDigestAlgorithm(JSContext *cx, CryptoAlgorithmIdentifier hashIdentifier) {
  switch (hashIdentifier) {
  case CryptoAlgorithmIdentifier::MD5: {
    return EVP_md5();
  }
  case CryptoAlgorithmIdentifier::SHA_1: {
    return EVP_sha1();
  }
  case CryptoAlgorithmIdentifier::SHA_256: {
    return EVP_sha256();
  }
  case CryptoAlgorithmIdentifier::SHA_384: {
    return EVP_sha384();
  }
  case CryptoAlgorithmIdentifier::SHA_512: {
    return EVP_sha512();
  }
  default: {
    DOMException::raise(cx, "NotSupportedError", "NotSupportedError");
    return nullptr;
  }
  }
}

const EVP_MD *createDigestAlgorithm(JSContext *cx, JS::HandleObject key) {

  JS::RootedObject alg(cx, CryptoKey::get_algorithm(key));

  JS::RootedValue hash_val(cx);
  JS_GetProperty(cx, alg, "hash", &hash_val);
  if (!hash_val.isObject()) {
    DOMException::raise(cx, "NotSupportedError", "NotSupportedError");
    return nullptr;
  }
  JS::RootedObject hash(cx, &hash_val.toObject());
  JS::RootedValue name_val(cx);
  JS_GetProperty(cx, hash, "name", &name_val);
  auto name_chars = core::encode(cx, name_val);
  if (!name_chars) {
    DOMException::raise(cx, "NotSupportedError", "NotSupportedError");
    return nullptr;
  }

  std::string_view name = name_chars;
  if (name == "MD5") {
    return EVP_md5();
  } else if (name == "SHA-1") {
    return EVP_sha1();
  } else if (name == "SHA-224") {
    return EVP_sha224();
  } else if (name == "SHA-256") {
    return EVP_sha256();
  } else if (name == "SHA-384") {
    return EVP_sha384();
  } else if (name == "SHA-512") {
    return EVP_sha512();
  } else {
    DOMException::raise(cx, "NotSupportedError", "NotSupportedError");
    return nullptr;
  }
}
// This implements https://w3c.github.io/webcrypto/#sha-operations for all
// the SHA algorithms that we support.
std::optional<std::vector<uint8_t>> rawDigest(JSContext *cx, std::span<uint8_t> data,
                                              const EVP_MD *algorithm, size_t buffer_size) {
  unsigned int size;
  std::vector<uint8_t> buf(buffer_size, 0);
  if (!EVP_Digest(data.data(), data.size(), buf.data(), &size, algorithm, NULL)) {
    // 2. If performing the operation results in an error, then throw an OperationError.
    DOMException::raise(cx, "SubtleCrypto.digest: failed to create digest", "OperationError");
    return std::nullopt;
  }
  return {std::move(buf)};
};

// This implements https://w3c.github.io/webcrypto/#sha-operations for all
// the SHA algorithms that we support.
JSObject *digest(JSContext *cx, std::span<uint8_t> data, const EVP_MD *algorithm,
                 size_t buffer_size) {
  unsigned int size;
  mozilla::UniquePtr<uint8_t[], JS::FreePolicy> buf{
      static_cast<uint8_t *>(JS_malloc(cx, buffer_size))};
  if (!buf) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }
  if (!EVP_Digest(data.data(), data.size(), buf.get(), &size, algorithm, NULL)) {
    // 2. If performing the operation results in an error, then throw an OperationError.
    DOMException::raise(cx, "SubtleCrypto.digest: failed to create digest", "OperationError");
    return nullptr;
  }
  // 3. Return a new ArrayBuffer containing result.
  JS::RootedObject array_buffer(cx);
  array_buffer.set(JS::NewArrayBufferWithContents(
      cx, size, buf.get(), JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
  if (!array_buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  // `array_buffer` now owns `buf`
  static_cast<void>(buf.release());

  return array_buffer;
};

// https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.1
// 6.3.1.  Parameters for RSA Public Keys
std::unique_ptr<CryptoKeyRSAComponents> createRSAPublicKeyFromJWK(JSContext *cx, JsonWebKey *jwk) {
  if (!jwk->n.has_value() || !jwk->e.has_value()) {
    DOMException::raise(cx, "Data provided to an operation does not meet requirements",
                        "DataError");
    return nullptr;
  }
  auto modulusResult = base64::forgivingBase64Decode(jwk->n.value(), base64::base64URLDecodeTable);
  if (modulusResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'n' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto modulus = modulusResult.unwrap();

  // Per RFC 7518 Section 6.3.1.1: https://tools.ietf.org/html/rfc7518#section-6.3.1.1
  if (modulus.starts_with('0')) {
    modulus = modulus.erase(0, 1);
  }
  auto dataResult = base64::stringToJSByteString(cx, jwk->e.value());
  if (dataResult.isErr()) {
    DOMException::raise(cx, "Data provided to an operation does not meet requirements",
                        "DataError");
    return nullptr;
  }
  auto data = dataResult.unwrap();
  auto exponentResult = base64::forgivingBase64Decode(data, base64::base64URLDecodeTable);
  if (exponentResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'e' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto exponent = exponentResult.unwrap();

  // import public key
  auto publicKeyComponents = CryptoKeyRSAComponents::createPublic(modulus, exponent);
  return publicKeyComponents;
}

std::unique_ptr<CryptoKeyECComponents> createECPublicKeyFromJWK(JSContext *cx, JsonWebKey *jwk) {
  if (!jwk->x.has_value() || !jwk->y.has_value()) {
    DOMException::raise(cx, "Data provided to an operation does not meet requirements",
                        "DataError");
    return nullptr;
  }
  auto xResult = base64::forgivingBase64Decode(jwk->x.value(), base64::base64URLDecodeTable);
  if (xResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'x' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto x = xResult.unwrap();
  auto yResult = base64::forgivingBase64Decode(jwk->y.value(), base64::base64URLDecodeTable);
  if (yResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'y' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto y = yResult.unwrap();

  return CryptoKeyECComponents::createPublic(x, y);
}

std::unique_ptr<CryptoKeyECComponents> createECPrivateKeyFromJWK(JSContext *cx, JsonWebKey *jwk) {
  if (!jwk->x.has_value() || !jwk->y.has_value() || !jwk->d.has_value()) {
    DOMException::raise(cx, "Data provided to an operation does not meet requirements",
                        "DataError");
    return nullptr;
  }
  auto xResult = base64::forgivingBase64Decode(jwk->x.value(), base64::base64URLDecodeTable);
  if (xResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'x' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto x = xResult.unwrap();
  auto yResult = base64::forgivingBase64Decode(jwk->y.value(), base64::base64URLDecodeTable);
  if (yResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'y' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto y = yResult.unwrap();
  auto dResult = base64::forgivingBase64Decode(jwk->d.value(), base64::base64URLDecodeTable);
  if (dResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'd' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto d = dResult.unwrap();

  return CryptoKeyECComponents::createPrivate(x, y, d);
}
// https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2
// 6.3.2.  Parameters for RSA Private Keys
std::unique_ptr<CryptoKeyRSAComponents> createRSAPrivateKeyFromJWK(JSContext *cx, JsonWebKey *jwk) {
  // 2.10.1 If jwk does not meet the requirements of Section 6.3.2 of JSON Web Algorithms [JWA],
  // then throw a DataError. 2.10.2 Let privateKey represents the RSA private key identified by
  // interpreting jwk according to Section 6.3.2 of JSON Web Algorithms [JWA]. 2.10.3 If privateKey
  // is not a valid RSA private key according to [RFC3447], then throw a DataError.
  auto modulusResult = base64::forgivingBase64Decode(jwk->n.value(), base64::base64URLDecodeTable);
  if (modulusResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'n' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto modulus = modulusResult.unwrap();
  // Per RFC 7518 Section 6.3.1.1: https://tools.ietf.org/html/rfc7518#section-6.3.1.1
  if (modulus.starts_with('0')) {
    modulus = modulus.erase(0, 1);
  }
  auto dataResult = base64::stringToJSByteString(cx, jwk->e.value());
  if (dataResult.isErr()) {
    DOMException::raise(cx, "Data provided to an operation does not meet requirements",
                        "DataError");
  }
  auto data = dataResult.unwrap();
  auto exponentResult = base64::forgivingBase64Decode(data, base64::base64URLDecodeTable);
  if (exponentResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'e' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto exponent = exponentResult.unwrap();
  auto privateExponentResult =
      base64::forgivingBase64Decode(jwk->d.value(), base64::base64URLDecodeTable);
  if (privateExponentResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'd' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto privateExponent = privateExponentResult.unwrap();
  if (!jwk->p.has_value() && !jwk->q.has_value() && !jwk->dp.has_value() && !jwk->dp.has_value() &&
      !jwk->qi.has_value()) {
    auto privateKeyComponents =
        CryptoKeyRSAComponents::createPrivate(modulus, exponent, privateExponent);
    return privateKeyComponents;
  }

  if (!jwk->p.has_value() || !jwk->q.has_value() || !jwk->dp.has_value() || !jwk->dq.has_value() ||
      !jwk->qi.has_value()) {
    DOMException::raise(cx, "Data provided to an operation does not meet requirements",
                        "DataError");
    return nullptr;
  }

  auto firstPrimeFactorResult =
      base64::forgivingBase64Decode(jwk->p.value(), base64::base64URLDecodeTable);
  if (firstPrimeFactorResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'p' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto firstPrimeFactor = firstPrimeFactorResult.unwrap();
  auto firstFactorCRTExponentResult =
      base64::forgivingBase64Decode(jwk->dp.value(), base64::base64URLDecodeTable);
  if (firstFactorCRTExponentResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'dp' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto firstFactorCRTExponent = firstFactorCRTExponentResult.unwrap();
  auto secondPrimeFactorResult =
      base64::forgivingBase64Decode(jwk->q.value(), base64::base64URLDecodeTable);
  if (secondPrimeFactorResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'q' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto secondPrimeFactor = secondPrimeFactorResult.unwrap();
  auto secondFactorCRTExponentResult =
      base64::forgivingBase64Decode(jwk->dq.value(), base64::base64URLDecodeTable);
  if (secondFactorCRTExponentResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'dq' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto secondFactorCRTExponent = secondFactorCRTExponentResult.unwrap();
  auto secondFactorCRTCoefficientResult =
      base64::forgivingBase64Decode(jwk->qi.value(), base64::base64URLDecodeTable);
  if (secondFactorCRTCoefficientResult.isErr()) {
    DOMException::raise(
        cx, "The JWK member 'qi' could not be base64url decoded or contained padding", "DataError");
    return nullptr;
  }
  auto secondFactorCRTCoefficient = secondFactorCRTCoefficientResult.unwrap();

  CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo{firstPrimeFactor, firstFactorCRTExponent};

  CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo{secondPrimeFactor, secondFactorCRTExponent,
                                                    secondFactorCRTCoefficient};

  if (!jwk->oth.size()) {
    auto privateKeyComponents = CryptoKeyRSAComponents::createPrivateWithAdditionalData(
        modulus, exponent, privateExponent, firstPrimeInfo, secondPrimeInfo, {});
    return privateKeyComponents;
  }

  std::vector<CryptoKeyRSAComponents::PrimeInfo> otherPrimeInfos;
  for (const auto &value : jwk->oth) {
    auto primeFactorResult = base64::forgivingBase64Decode(value.r, base64::base64URLDecodeTable);
    if (primeFactorResult.isErr()) {
      return nullptr;
    }
    auto primeFactor = primeFactorResult.unwrap();
    auto factorCRTExponentResult =
        base64::forgivingBase64Decode(value.d, base64::base64URLDecodeTable);
    if (factorCRTExponentResult.isErr()) {
      return nullptr;
    }
    auto factorCRTExponent = factorCRTExponentResult.unwrap();
    auto factorCRTCoefficientResult =
        base64::forgivingBase64Decode(value.t, base64::base64URLDecodeTable);
    if (factorCRTCoefficientResult.isErr()) {
      return nullptr;
    }
    auto factorCRTCoefficient = factorCRTCoefficientResult.unwrap();

    otherPrimeInfos.emplace_back(primeFactor, factorCRTExponent, factorCRTCoefficient);
  }

  auto privateKeyComponents = CryptoKeyRSAComponents::createPrivateWithAdditionalData(
      modulus, exponent, privateExponent, firstPrimeInfo, secondPrimeInfo, otherPrimeInfos);
  return privateKeyComponents;
}

JS::Result<CryptoAlgorithmIdentifier> toHashIdentifier(JSContext *cx, JS::HandleValue value) {
  auto normalizedHashAlgorithm = CryptoAlgorithmDigest::normalize(cx, value);
  if (!normalizedHashAlgorithm) {
    return JS::Result<CryptoAlgorithmIdentifier>(JS::Error());
  }
  return normalizedHashAlgorithm->identifier();
}

std::optional<NamedCurve> toNamedCurve(std::string_view name) {
  if (name == "P-256") {
    return NamedCurve::P256;
  } else if (name == "P-384") {
    return NamedCurve::P384;
  } else if (name == "P-521") {
    return NamedCurve::P521;
  }

  return std::nullopt;
}

JS::Result<NamedCurve> toNamedCurve(JSContext *cx, JS::HandleValue value) {
  auto nameChars = core::encode(cx, value);
  auto name = toNamedCurve(nameChars);
  if (name.has_value()) {
    return name.value();
  } else {
    return JS::Result<NamedCurve>(JS::Error());
  }
}

JS::Result<size_t> curveSize(JSContext *cx, JS::HandleObject key) {

  JS::RootedObject alg(cx, CryptoKey::get_algorithm(key));

  JS::RootedValue namedCurve_val(cx);
  JS_GetProperty(cx, alg, "namedCurve", &namedCurve_val);
  auto namedCurve_chars = core::encode(cx, namedCurve_val);
  if (!namedCurve_chars) {
    return JS::Result<size_t>(JS::Error());
  }

  std::string_view namedCurve = namedCurve_chars;
  if (namedCurve == "P-256") {
    return 256;
  } else if (namedCurve == "P-384") {
    return 384;
  } else if (namedCurve == "P-521") {
    return 521;
  }

  MOZ_ASSERT_UNREACHABLE();
  return 0;
}

// This implements the first section of
// https://w3c.github.io/webcrypto/#algorithm-normalization-normalize-an-algorithm which is shared
// across all the diffent algorithms, but importantly does not implement the parts to do with the
// chosen `op` (operation) the `op` parts are handled in the specialized `normalize` functions on
// the concrete classes which derive from CryptoAlgorithm such as CryptoAlgorithmDigest.
JS::Result<CryptoAlgorithmIdentifier> normalizeIdentifier(JSContext *cx, JS::HandleValue value) {

  // The specification states:
  // --------
  //    If alg is an instance of a DOMString:
  //    Return the result of running the normalize an algorithm algorithm,
  //    with the alg set to a new Algorithm dictionary whose name attribute
  //    is alg, and with the op set to op.
  // --------
  // Instead of doing that, we operate on the string and not the dictionary.
  // If we see a dictionary (JSObject), we pull the name attribute out
  // and coerce it's value to a String.
  // The reason we chose this direct is because we only need this one field
  // from the provided dictionary, so we store the field on it's own and not
  // in a JSObject which would take up more memory.

  // 1. Let registeredAlgorithms be the associative container stored at the op key of
  // supportedAlgorithms.
  // 2. Let initialAlg be the result of converting the ECMAScript object represented by alg to the
  // IDL dictionary type Algorithm, as defined by [WebIDL].
  // 3. If an error occurred, return the error and terminate this algorithm.
  // 4. Let algName be the value of the name attribute of initialAlg.
  JS::Rooted<JSString *> algName(cx);
  if (value.isObject()) {
    JS::Rooted<JSObject *> params(cx, &value.toObject());
    JS::Rooted<JS::Value> name_val(cx);
    if (!JS_GetProperty(cx, params, "name", &name_val)) {
      return JS::Result<CryptoAlgorithmIdentifier>(JS::Error());
    }
    algName.set(JS::ToString(cx, name_val));
  } else {
    algName.set(JS::ToString(cx, value));
  }
  // If `algName` is falsey, it means the call to JS::ToString failed.
  // In that scenario, we should already have an exception, which is why we are not creating our own
  // one.
  if (!algName) {
    return JS::Result<CryptoAlgorithmIdentifier>(JS::Error());
  }

  // TODO: We convert from JSString to std::string quite a lot in the codebase, should we pull this
  // logic out into a new function?
  auto algorithmChars = core::encode(cx, algName);
  if (!algorithmChars) {
    return JS::Result<CryptoAlgorithmIdentifier>(JS::Error());
  }
  std::string algorithm(algorithmChars.begin(), algorithmChars.len);

  // 5. If registeredAlgorithms contains a key that is a case-insensitive string match for algName:
  // 5.1 Set algName to the value of the matching key.
  // 5.2 Let desiredType be the IDL dictionary type stored at algName in registeredAlgorithms.
  // Note: We do not implement 5.2 here, it is instead implemented in the specialized `normalize`
  // functions.
  std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  if (algorithm == "RSASSA-PKCS1-V1_5") {
    return CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5;
  } else if (algorithm == "RSA-PSS") {
    return CryptoAlgorithmIdentifier::RSA_PSS;
  } else if (algorithm == "RSA-OAEP") {
    return CryptoAlgorithmIdentifier::RSA_OAEP;
  } else if (algorithm == "ECDSA") {
    return CryptoAlgorithmIdentifier::ECDSA;
  } else if (algorithm == "ECDH") {
    return CryptoAlgorithmIdentifier::ECDH;
  } else if (algorithm == "AES-CTR") {
    return CryptoAlgorithmIdentifier::AES_CTR;
  } else if (algorithm == "AES-CBC") {
    return CryptoAlgorithmIdentifier::AES_CBC;
  } else if (algorithm == "AES-GCM") {
    return CryptoAlgorithmIdentifier::AES_GCM;
  } else if (algorithm == "AES-KW") {
    return CryptoAlgorithmIdentifier::AES_KW;
  } else if (algorithm == "HMAC") {
    return CryptoAlgorithmIdentifier::HMAC;
  } else if (algorithm == "MD5") {
    return CryptoAlgorithmIdentifier::MD5;
  } else if (algorithm == "SHA-1") {
    return CryptoAlgorithmIdentifier::SHA_1;
  } else if (algorithm == "SHA-256") {
    return CryptoAlgorithmIdentifier::SHA_256;
  } else if (algorithm == "SHA-384") {
    return CryptoAlgorithmIdentifier::SHA_384;
  } else if (algorithm == "SHA-512") {
    return CryptoAlgorithmIdentifier::SHA_512;
  } else if (algorithm == "HKDF") {
    return CryptoAlgorithmIdentifier::HKDF;
  } else if (algorithm == "PBKDF2") {
    return CryptoAlgorithmIdentifier::PBKDF2;
  } else {
    // Otherwise: Return a new NotSupportedError and terminate this algorithm.
    DOMException::raise(cx, "Algorithm: Unrecognized name", "NotSupportedError");
    return JS::Result<CryptoAlgorithmIdentifier>(JS::Error());
  }
}
// } // namespace

const char *curveName(NamedCurve curve) {
  switch (curve) {
  case NamedCurve::P256: {
    return "P-256";
  }
  case NamedCurve::P384: {
    return "P-384";
  }
  case NamedCurve::P521: {
    return "P-521";
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Unknown `CryptoAlgorithmIdentifier` value");
  }
  }
}

const char *algorithmName(CryptoAlgorithmIdentifier algorithm) {
  switch (algorithm) {
  case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5: {
    return "RSASSA-PKCS1-v1_5";
  }
  case CryptoAlgorithmIdentifier::RSA_PSS: {
    return "RSA-PSS";
  }
  case CryptoAlgorithmIdentifier::RSA_OAEP: {
    return "RSA-OAEP";
  }
  case CryptoAlgorithmIdentifier::ECDSA: {
    return "ECDSA";
  }
  case CryptoAlgorithmIdentifier::ECDH: {
    return "ECDH";
  }
  case CryptoAlgorithmIdentifier::AES_CTR: {
    return "AES-CTR";
  }
  case CryptoAlgorithmIdentifier::AES_CBC: {
    return "AES-CBC";
  }
  case CryptoAlgorithmIdentifier::AES_GCM: {
    return "AES-GCM";
  }
  case CryptoAlgorithmIdentifier::AES_KW: {
    return "AES-KW";
  }
  case CryptoAlgorithmIdentifier::HMAC: {
    return "HMAC";
  }
  case CryptoAlgorithmIdentifier::MD5: {
    return "MD5";
  }
  case CryptoAlgorithmIdentifier::SHA_1: {
    return "SHA-1";
  }
  case CryptoAlgorithmIdentifier::SHA_256: {
    return "SHA-256";
  }
  case CryptoAlgorithmIdentifier::SHA_384: {
    return "SHA-384";
  }
  case CryptoAlgorithmIdentifier::SHA_512: {
    return "SHA-512";
  }
  case CryptoAlgorithmIdentifier::HKDF: {
    return "HKDF";
  }
  case CryptoAlgorithmIdentifier::PBKDF2: {
    return "PBKDF2";
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Unknown `CryptoAlgorithmIdentifier` value");
  }
  }
}

// clang-format off
/// This table is from https://w3c.github.io/webcrypto/#h-note-15
// | Algorithm					|	encrypt	|	decrypt	|	sign	|	verify	|	digest	|	generateKey	|	deriveKey	|	deriveBits	|	importKey	|	exportKey	|	wrapKey	|	unwrapKey	|
// |	RSASSA-PKCS1-v1_5	|					|					|		✔		|		✔			|					|			✔				| 					|							|			✔			|			✔			|					|						|
// |	RSA-PSS         	|					|					|		✔		|		✔			|					|			✔				|						|							|			✔			|			✔			|					|						|
// |	RSA-OAEP					|		✔			|		✔			|				|					|					|			✔				|						|							|			✔			|			✔			|		✔			|		✔				|
// |	ECDSA						 	|					|					|		✔		|		✔			|					|			✔				| 					|							|			✔			|			✔			|					|						|
// |	ECDH							|					|					|				|					|					|			✔				|			✔			|			✔				|			✔			|			✔			|					|						|
// |	AES-CTR						|		✔			|		✔			|				|					|					|			✔				|						|							|			✔			|			✔			|		✔			|		✔				|
// |	AES-CBC						|		✔			|		✔			|				|					|					|			✔				|						|							|			✔			|			✔			|		✔			|		✔				|
// |	AES-GCM						|		✔			|		✔			|				|					|					|			✔				|						|							|			✔			|			✔			|		✔			|		✔				|
// |	AES-KW						|					|					|				|					|					|			✔				|						|							|			✔			|			✔			|		✔			|		✔				|
// |	HMAC							|					|					|		✔		|		✔			|					|			✔				| 					|							|			✔			|			✔			|					|						|
// |	SHA-1							|					|					|				|					|		✔			|							|						|							|						|						|					|						|
// |	SHA-256						|					|					|				|					|		✔			|							|						|							|						|						|					|						|
// |	SHA-384						|					|					|				|					|		✔			|							|						|							|						|						|					|						|
// |	SHA-512						|					|					|				|					|		✔			|							|						|							|						|						|					|						|
// |	HKDF							|					|					|				|					|					|							|			✔			|			✔				|			✔			|						|					|						|
// |	PBKDF2						|					|					|				|					|					|							|			✔			|			✔				|			✔			|						|					|						|
//clang-format on

std::unique_ptr<CryptoAlgorithmDigest> CryptoAlgorithmDigest::normalize(JSContext *cx,
                                                                        JS::HandleValue value) {
  // Do steps 1 through 5.1 of https://w3c.github.io/webcrypto/#algorithm-normalization-normalize-an-algorithm
  auto identifierResult = normalizeIdentifier(cx, value);
  if (identifierResult.isErr()) {
    // If we are here, this means either the identifier could not be coerced to a String or was not recognized
    // In both those scenarios an exception will have already been created, which is why we are not creating one here.
    return nullptr;
  }
  auto identifier = identifierResult.unwrap();

  // The table listed at https://w3c.github.io/webcrypto/#h-note-15 is what defines which algorithms support which operations
  // SHA-1, SHA-256, SHA-384, and SHA-512 are the only algorithms which support the digest operation
  // We also support MD5 as an extra implementor defined algorithm

  // Note: The specification states that none of the SHA algorithms take any parameters -- https://w3c.github.io/webcrypto/#sha-registration
  switch (identifier) {
  case CryptoAlgorithmIdentifier::MD5: {
    return std::make_unique<CryptoAlgorithmMD5>();
  }
  case CryptoAlgorithmIdentifier::SHA_1: {
    return std::make_unique<CryptoAlgorithmSHA1>();
  }
  case CryptoAlgorithmIdentifier::SHA_256: {
    return std::make_unique<CryptoAlgorithmSHA256>();
  }
  case CryptoAlgorithmIdentifier::SHA_384: {
    return std::make_unique<CryptoAlgorithmSHA384>();
  }
  case CryptoAlgorithmIdentifier::SHA_512: {
    return std::make_unique<CryptoAlgorithmSHA512>();
  }
  default: {
    DOMException::raise(cx, "Supplied algorithm does not support the digest operation", "NotSupportedError");
    return nullptr;
  }
  }
};

std::unique_ptr<CryptoAlgorithmSignVerify>
CryptoAlgorithmSignVerify::normalize(JSContext *cx, JS::HandleValue value) {
  // Do steps 1 through 5.1 of https://w3c.github.io/webcrypto/#algorithm-normalization-normalize-an-algorithm
  auto identifierResult = normalizeIdentifier(cx, value);
  if (identifierResult.isErr()) {
    // If we are here, this means either the identifier could not be coerced to a String or was not recognized
    // In both those scenarios an exception will have already been created, which is why we are not creating one here.
    return nullptr;
  }
  auto identifier = identifierResult.unwrap();
  JS::Rooted<JSObject *> params(cx);

  // The value can either be a JS String or a JS Object with a 'name' property which is the algorithm identifier.
  // Other properties within the object will be the parameters for the algorithm to use.
  if (value.isString()) {
    auto obj = JS_NewPlainObject(cx);
    params.set(obj);
    if (!JS_SetProperty(cx, params, "name", value)) {
      return nullptr;
    }
  } else if (value.isObject()) {
    params.set(&value.toObject());
  }

  // The table listed at https://w3c.github.io/webcrypto/#h-note-15 is what defines which algorithms support which operations
  // RSASSA-PKCS1-v1_5, RSA-PSS, ECDSA, HMAC, are the algorithms
  // which support the sign operation
  switch (identifier) {
  case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5: {
    return std::make_unique<CryptoAlgorithmRSASSA_PKCS1_v1_5_Sign_Verify>();
  }
  case CryptoAlgorithmIdentifier::HMAC: {
    return std::make_unique<CryptoAlgorithmHMAC_Sign_Verify>();
  }
  case CryptoAlgorithmIdentifier::ECDSA: {
    return CryptoAlgorithmECDSA_Sign_Verify::fromParameters(cx, params);
  }
  case CryptoAlgorithmIdentifier::RSA_PSS: {
    MOZ_ASSERT(false);
    DOMException::raise(cx, "Supplied algorithm is not yet supported", "NotSupportedError");
    return nullptr;
  }
  default: {
    return nullptr;
  }
  }
};


namespace {
  std::optional<std::pair<mozilla::UniquePtr<uint8_t[], JS::FreePolicy>, size_t>> hmacSignature(JSContext *cx,
  const EVP_MD* algorithm, const std::span<uint8_t>& keyData, const std::span<uint8_t> data) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
      return std::nullopt;
    }

    EVP_PKEY * hkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, keyData.data(), keyData.size());
    if (!hkey) {
      return std::nullopt;
    }

    if (1 != EVP_DigestSignInit(ctx, nullptr, algorithm, nullptr, hkey)) {
      return std::nullopt;
    }

    if (1 != EVP_DigestSignUpdate(ctx, data.data(), data.size())) {
      return std::nullopt;
    }

    size_t len = 0;
    if (1 != EVP_DigestSignFinal(ctx, nullptr, &len)) {
      return std::nullopt;
    }

    mozilla::UniquePtr<uint8_t[], JS::FreePolicy> cipherText{static_cast<uint8_t *>(JS_malloc(cx, len))};
    if (!cipherText) {
      JS_ReportOutOfMemory(cx);
      return std::nullopt;
    }

    if (1 != EVP_DigestSignFinal(ctx, cipherText.get(), &len)) {
      return std::nullopt;
    }
    return std::pair<mozilla::UniquePtr<uint8_t[], JS::FreePolicy>, size_t>(std::move(cipherText), len);
  }
}

JSObject *CryptoAlgorithmHMAC_Sign_Verify::sign(JSContext *cx, JS::HandleObject key, std::span<uint8_t> data) {
  MOZ_ASSERT(CryptoKey::is_instance(key));

  // 1. Let mac be the result of performing the MAC Generation operation described in Section 4 of [FIPS-198-1] using the key represented by [[handle]] internal slot of key, the hash function identified by the hash attribute of the [[algorithm]] internal slot of key and message as the input data text.
  const EVP_MD *algorithm = createDigestAlgorithm(cx, key);
  if (!algorithm) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  std::span<uint8_t> keyData = CryptoKey::hmacKeyData(key);
  auto result = hmacSignature(cx, algorithm, keyData, data);
  if (!result.has_value()) {
    DOMException::raise(cx, "SubtleCrypto.sign: failed to sign", "OperationError");
    return nullptr;
  }
  auto sig = std::move(result.value().first);
  auto size = std::move(result.value().second);

  // 2. Return a new ArrayBuffer object, associated with the relevant global object of this [HTML], and containing the bytes of mac.
  JS::RootedObject array_buffer(cx);
  array_buffer.set(JS::NewArrayBufferWithContents(cx, size, sig.get(), JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
  if (!array_buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

   // `array_buffer` now owns `sig`
  static_cast<void>(sig.release());

  return array_buffer;

};
JS::Result<bool> CryptoAlgorithmHMAC_Sign_Verify::verify(JSContext *cx, JS::HandleObject key, std::span<uint8_t> signature, std::span<uint8_t> data) {
  MOZ_ASSERT(CryptoKey::is_instance(key));
  // 1. Let mac be the result of performing the MAC Generation operation described in Section 4 of [FIPS-198-1] using the key represented by [[handle]] internal slot of key, the hash function identified by the hash attribute of the [[algorithm]] internal slot of key and message as the input data text.
  const EVP_MD *algorithm = createDigestAlgorithm(cx, key);
  if (!algorithm) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  std::span<uint8_t> keyData = CryptoKey::hmacKeyData(key);
  auto result = hmacSignature(cx, algorithm, keyData, data);
  if (!result.has_value()) {
    DOMException::raise(cx, "SubtleCrypto.verify: failed to verify", "OperationError");
    return JS::Result<bool>(JS::Error());
  }
  auto sig = std::move(result.value().first);
  auto size = std::move(result.value().second);


  // 2. Return true if mac is equal to signature and false otherwise.
  bool match = size == signature.size() && (CRYPTO_memcmp(sig.get(), signature.data(), size) == 0);
  return match;
};
JSObject *CryptoAlgorithmHMAC_Sign_Verify::toObject(JSContext *cx) {
  return nullptr;
};

JSObject *CryptoAlgorithmECDSA_Sign_Verify::sign(JSContext *cx, JS::HandleObject key, std::span<uint8_t> data) {
  MOZ_ASSERT(CryptoKey::is_instance(key));

  // 1. If the [[type]] internal slot of key is not "private", then throw an InvalidAccessError.
  if (CryptoKey::type(key) != CryptoKeyType::Private) {
    DOMException::raise(cx, "InvalidAccessError", "InvalidAccessError");
    return nullptr;
  }

  // 2. Let hashAlgorithm be the hash member of normalizedAlgorithm.
  const EVP_MD* algorithm = createDigestAlgorithm(cx, this->hashIdentifier);
  if (!algorithm) {
    DOMException::raise(cx, "SubtleCrypto.sign: failed to sign", "OperationError");
    return nullptr;
  }

  // 3. Let M be the result of performing the digest operation specified by hashAlgorithm using message.
  auto digestOption = rawDigest(cx, data, algorithm, EVP_MD_size(algorithm));
  if (!digestOption.has_value()) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  auto digest = digestOption.value();

  // 4. Let d be the ECDSA private key associated with key.
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-declarations"
  const EC_KEY * ecKey = EVP_PKEY_get0_EC_KEY(CryptoKey::key(key));
  #pragma clang diagnostic pop
  if (!ecKey) {
    DOMException::raise(cx, "SubtleCrypto.verify: failed to verify", "OperationError");
    return nullptr;
  }

  // 5. Let params be the EC domain parameters associated with key.
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-declarations"
  auto sig = ECDSA_do_sign(digest.data(), digest.size(), const_cast<EC_KEY*>(ecKey));
  #pragma clang diagnostic pop
  if (!sig) {
    DOMException::raise(cx, "SubtleCrypto.verify: failed to verify", "OperationError");
    return nullptr;
  }

  // 6. If the namedCurve attribute of the [[algorithm]] internal slot of key is "P-256", "P-384" or "P-521":
  //         Perform the ECDSA signing process, as specified in [RFC6090], Section 5.4, with M as the message, using params as the EC domain parameters, and with d as the private key.
  //         Let r and s be the pair of integers resulting from performing the ECDSA signing process.
  //         Let result be an empty byte sequence.
  //         Let n be the smallest integer such that n * 8 is greater than the logarithm to base 2 of the order of the base point of the elliptic curve identified by params.
  //         Convert r to an octet string of length n and append this sequence of bytes to result.
  //         Convert s to an octet string of length n and append this sequence of bytes to result.
  // Otherwise, the namedCurve attribute of the [[algorithm]] internal slot of key is a value specified in an applicable specification:
  //     Perform the ECDSA signature steps specified in that specification, passing in M, params and d and resulting in result.
  const BIGNUM* r;
  const BIGNUM* s;
  ECDSA_SIG_get0(sig, &r, &s);
  auto keySize = curveSize(cx, key);
  if (keySize.isErr()) {
    return nullptr;
  }

  size_t keySizeInBytes = numBitsToBytes(keySize.unwrap());

  auto rBytesAndSize = convertToBytesExpand(cx, r, keySizeInBytes);
  auto *rBytes = rBytesAndSize.first.get();
  auto rBytesSize = rBytesAndSize.second;

  auto sBytesAndSize = convertToBytesExpand(cx, s, keySizeInBytes);
  auto *sBytes = sBytesAndSize.first.get();
  auto sBytesSize = sBytesAndSize.second;

  auto resultSize = rBytesSize + sBytesSize;
  mozilla::UniquePtr<uint8_t[], JS::FreePolicy> result{
      static_cast<uint8_t *>(JS_malloc(cx, resultSize))};

  std::memcpy(result.get(), rBytes, rBytesSize);
  std::memcpy(result.get() + rBytesSize, sBytes, sBytesSize);

  // 7. Return the result of creating an ArrayBuffer containing result.
  JS::RootedObject buffer(cx, JS::NewArrayBufferWithContents(cx, resultSize, result.get(), JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
  if (!buffer) {
    // We can be here if the array buffer was too large -- if that was the case then a
    // JSMSG_BAD_ARRAY_LENGTH will have been created. Otherwise we're probably out of memory.
    if (!JS_IsExceptionPending(cx)) {
      js::ReportOutOfMemory(cx);
    }
    return nullptr;
  }

  // `signature` is now owned by `buffer`
  static_cast<void>(result.release());

  return buffer;
};
JS::Result<bool> CryptoAlgorithmECDSA_Sign_Verify::verify(JSContext *cx, JS::HandleObject key, std::span<uint8_t> signature, std::span<uint8_t> data) {
  MOZ_ASSERT(CryptoKey::is_instance(key));
  // 1. If the [[type]] internal slot of key is not "public", then throw an InvalidAccessError.
  if (CryptoKey::type(key) != CryptoKeyType::Public) {
    DOMException::raise(cx, "InvalidAccessError", "InvalidAccessError");
    return JS::Result<bool>(JS::Error());
  }

  // 2. Let hashAlgorithm be the hash member of normalizedAlgorithm.
  const EVP_MD* algorithm = createDigestAlgorithm(cx, this->hashIdentifier);
  if (!algorithm) {
    DOMException::raise(cx, "SubtleCrypto.verify: failed to verify", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  // 3. Let M be the result of performing the digest operation specified by hashAlgorithm using message.
  auto digestOption = rawDigest(cx, data, algorithm, EVP_MD_size(algorithm));
  if (!digestOption.has_value()) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  auto digest = digestOption.value();

  // 4. Let Q be the ECDSA public key associated with key.
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-declarations"
  const EC_KEY * ecKey = EVP_PKEY_get0_EC_KEY(CryptoKey::key(key));
  #pragma clang diagnostic pop
  if (!ecKey) {
    DOMException::raise(cx, "SubtleCrypto.verify: failed to verify", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  // 5. Let params be the EC domain parameters associated with key.
  // 6. If the namedCurve attribute of the [[algorithm]] internal slot of key is "P-256", "P-384" or "P-521":
      // Perform the ECDSA verifying process, as specified in RFC6090, Section 5.3, with M as the received message, signature as the received signature and using params as the EC domain parameters, and Q as the public key.
    // Otherwise, the namedCurve attribute of the [[algorithm]] internal slot of key is a value specified in an applicable specification:
      // Perform the ECDSA verification steps specified in that specification passing in M, signature, params and Q and resulting in an indication of whether or not the purported signature is valid.
  auto keySize = curveSize(cx, key);
  if (keySize.isErr()) {
    return JS::Result<bool>(JS::Error());
  }

  size_t keySizeInBytes = numBitsToBytes(keySize.unwrap());

  auto sig = ECDSA_SIG_new();
  auto r = BN_bin2bn(signature.data(), keySizeInBytes, nullptr);
  auto s = BN_bin2bn(signature.data() + keySizeInBytes, keySizeInBytes, nullptr);

  if (!ECDSA_SIG_set0(sig, r, s)) {
    DOMException::raise(cx, "SubtleCrypto.verify: failed to verify", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  // 7. Let result be a boolean with the value true if the signature is valid and the value false otherwise.
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-declarations"
  bool result = ECDSA_do_verify(digest.data(), digest.size(), sig, const_cast<EC_KEY*>(ecKey)) == 1;
  #pragma clang diagnostic pop

  // 8. Return result.
  return result;
};
JSObject *CryptoAlgorithmECDSA_Sign_Verify::toObject(JSContext *cx) {
  return nullptr;
};

JSObject *CryptoAlgorithmRSASSA_PKCS1_v1_5_Sign_Verify::sign(JSContext *cx, JS::HandleObject key,
                                                 std::span<uint8_t> data) {

  // 1. If the [[type]] internal slot of key is not "private", then throw an InvalidAccessError.
  if (CryptoKey::type(key) != CryptoKeyType::Private) {
    DOMException::raise(cx, "InvalidAccessError", "InvalidAccessError");
    return nullptr;
  }

  MOZ_ASSERT(CryptoKey::is_instance(key));
  if (CryptoKey::type(key) != CryptoKeyType::Private) {
    DOMException::raise(cx, "InvalidAccessError", "InvalidAccessError");
    return nullptr;
  }

  const EVP_MD *algorithm = createDigestAlgorithm(cx, key);
  if (!algorithm) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  auto digest = rawDigest(cx, data, algorithm, EVP_MD_size(algorithm));
  if (!digest.has_value()) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  // 2. Perform the signature generation operation defined in Section 8.2 of [RFC3447] with the
  //  key represented by the [[handle]] internal slot of key as the signer's private key and the
  //  contents of message as M and using the hash function specified in the hash attribute of the
  //  [[algorithm]] internal slot of key as the Hash option for the EMSA-PKCS1-v1_5 encoding
  //  method.
  // 3. If performing the operation results in an error, then throw an OperationError.
  auto ctx = EVP_PKEY_CTX_new(CryptoKey::key(key), nullptr);
  if (!ctx) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  if (EVP_PKEY_sign_init(ctx) <= 0) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  if (EVP_PKEY_CTX_set_signature_md(ctx, algorithm) <= 0) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  size_t signature_length;
  if (EVP_PKEY_sign(ctx, nullptr, &signature_length, digest->data(), digest->size()) <= 0) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  // 4. Let signature be the value S that results from performing the operation.
  mozilla::UniquePtr<uint8_t[], JS::FreePolicy> signature{static_cast<uint8_t *>(JS_malloc(cx, signature_length))};
  if (EVP_PKEY_sign(ctx, signature.get(), &signature_length, digest->data(), digest->size()) <= 0) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return nullptr;
  }

  // 5. Return a new ArrayBuffer associated with the relevant global object of this [HTML], and
  // containing the bytes of signature.
  JS::RootedObject buffer(cx, JS::NewArrayBufferWithContents(cx, signature_length, signature.get(), JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
  if (!buffer) {
    // We can be here if the array buffer was too large -- if that was the case then a
    // JSMSG_BAD_ARRAY_LENGTH will have been created. Otherwise we're probably out of memory.
    if (!JS_IsExceptionPending(cx)) {
      js::ReportOutOfMemory(cx);
    }
    return nullptr;
  }

  // `signature` is now owned by `buffer`
  static_cast<void>(signature.release());

  return buffer;
}

JS::Result<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5_Sign_Verify::verify(JSContext *cx, JS::HandleObject key,
                                                          std::span<uint8_t> signature,
                                                          std::span<uint8_t> data) {
  MOZ_ASSERT(CryptoKey::is_instance(key));

  if (CryptoKey::type(key) != CryptoKeyType::Public) {
    DOMException::raise(cx, "InvalidAccessError", "InvalidAccessError");
    return JS::Result<bool>(JS::Error());
  }
  const EVP_MD *algorithm = createDigestAlgorithm(cx, key);

  auto digestOption = rawDigest(cx, data, algorithm, EVP_MD_size(algorithm));
  if (!digestOption.has_value()) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  auto digest = digestOption.value();

  auto ctx = EVP_PKEY_CTX_new(CryptoKey::key(key), nullptr);
  if (!ctx) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  if (EVP_PKEY_verify_init(ctx) != 1) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) != 1) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  if (EVP_PKEY_CTX_set_signature_md(ctx, algorithm) != 1) {
    DOMException::raise(cx, "OperationError", "OperationError");
    return JS::Result<bool>(JS::Error());
  }

  return EVP_PKEY_verify(ctx, signature.data(), signature.size(), digest.data(), digest.size()) ==
         1;
}

std::unique_ptr<CryptoAlgorithmImportKey>
CryptoAlgorithmImportKey::normalize(JSContext *cx, JS::HandleValue value) {
  // Do steps 1 through 5.1 of https://w3c.github.io/webcrypto/#algorithm-normalization-normalize-an-algorithm
  auto identifierResult = normalizeIdentifier(cx, value);
  if (identifierResult.isErr()) {
    // If we are here, this means either the identifier could not be coerced to a String or was not recognized
    // In both those scenarios an exception will have already been created, which is why we are not creating one here.
    return nullptr;
  }
  auto identifier = identifierResult.unwrap();
  JS::RootedObject params(cx);

  // The value can either be a JS String or a JS Object with a 'name' property which is the algorithm identifier.
  // Other properties within the object will be the parameters for the algorithm to use.
  if (value.isString()) {
    auto obj = JS_NewPlainObject(cx);
    params.set(obj);
    JS_SetProperty(cx, params, "name", value);
  } else if (value.isObject()) {
    params.set(&value.toObject());
  }

  // The table listed at https://w3c.github.io/webcrypto/#h-note-15 is what defines which algorithms support which operations
  // RSASSA-PKCS1-v1_5, RSA-PSS, RSA-OAEP, ECDSA, ECDH, AES-CTR, AES-CBC, AES-GCM, AES-KW, HMAC, HKDF, PBKDF2 are the algorithms
  // which support the importKey operation
  switch (identifier) {
  case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5: {
    return CryptoAlgorithmRSASSA_PKCS1_v1_5_Import::fromParameters(cx, params);
  }
  case CryptoAlgorithmIdentifier::HMAC: {
    return CryptoAlgorithmHMAC_Import::fromParameters(cx, params);
  }
  case CryptoAlgorithmIdentifier::ECDSA: {
    return CryptoAlgorithmECDSA_Import::fromParameters(cx, params);
  }
  case CryptoAlgorithmIdentifier::RSA_PSS:
  case CryptoAlgorithmIdentifier::RSA_OAEP:
  case CryptoAlgorithmIdentifier::AES_CTR:
  case CryptoAlgorithmIdentifier::AES_CBC:
  case CryptoAlgorithmIdentifier::AES_GCM:
  case CryptoAlgorithmIdentifier::AES_KW:
  case CryptoAlgorithmIdentifier::ECDH:
  case CryptoAlgorithmIdentifier::HKDF:
  case CryptoAlgorithmIdentifier::PBKDF2: {
    DOMException::raise(cx, "Supplied algorithm is not yet supported", "NotSupportedError");
    return nullptr;
  }
  default: {
    DOMException::raise(cx, "Supplied algorithm does not support the importKey operation", "NotSupportedError");
    return nullptr;
  }
  }
};

std::unique_ptr<CryptoAlgorithmHMAC_Import> CryptoAlgorithmHMAC_Import::fromParameters(JSContext *cx, JS::HandleObject parameters) {
  JS::Rooted<JS::Value> hash_val(cx);
  if (!JS_GetProperty(cx, parameters, "hash", &hash_val)) {
    return nullptr;
  }
  auto hashIdentifier = toHashIdentifier(cx, hash_val);
  if (hashIdentifier.isErr()) {
    return nullptr;
  }
  bool found;
  unsigned long length;
  if (!JS_HasProperty(cx, parameters, "length", &found)) {
    return nullptr;
  }
  if (found) {
    JS::Rooted<JS::Value> length_val(cx);
    if (!JS_GetProperty(cx, parameters, "length", &length_val)) {
      return nullptr;
    }
    if (!length_val.isNumber()) {
      return nullptr;
    }
    length = length_val.toNumber();
    return std::make_unique<CryptoAlgorithmHMAC_Import>(hashIdentifier.unwrap(), length);
  } else {
    return std::make_unique<CryptoAlgorithmHMAC_Import>(hashIdentifier.unwrap());
  }
}

// https://w3c.github.io/webcrypto/#hmac-operations
JSObject *CryptoAlgorithmHMAC_Import::importKey(JSContext *cx, CryptoKeyFormat format,
                                                             KeyData keyData, bool extractable,
                                                             CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  JS::RootedObject result(cx);

  // 2. If usages contains an entry which is not "sign" or "verify", then throw a SyntaxError.
  if (!usages.canOnlySignOrVerify()) {
    DOMException::raise(cx, "HMAC keys only support 'sign' and 'verify' operations",
                        "SyntaxError");
    return nullptr;
  }

  std::unique_ptr<std::span<uint8_t>> data;
  // 3. Let hash be a new KeyAlgorithm.
  // 4.
  switch (format) {
  // 5. If format is "raw":
  case CryptoKeyFormat::Raw: {
    // 5.1 Let data be the octet string contained in keyData.
    data = std::make_unique<std::span<uint8_t>>(std::get<std::span<uint8_t>>(keyData));
    if (!data) {
      DOMException::raise(cx, "Supplied keyData must be either an ArrayBuffer, TypedArray, or DataView.", "DataError");
      return nullptr;
    }

    // 5.2 Set hash to equal the hash member of normalizedAlgorithm.
    // Step 5.2 is done in the call to CryptoKey::createHMAC
    break;
  }
  //6.  If format is "jwk":
  case CryptoKeyFormat::Jwk: {
    // 6.1 If keyData is a JsonWebKey dictionary:
      // 6.2 Let jwk equal keyData.
    // Otherwise:
      // 6.3 Throw a DataError.
    auto jwk = std::get<JsonWebKey *>(keyData);
    if (!jwk) {
      DOMException::raise(cx, "Supplied keyData is not a JSONWebKey", "DataError");
      return nullptr;
    }
    // 6.4 If the kty field of jwk is not "oct", then throw a DataError.
    // Step 6.4 has already been done in the other implementation of
    // CryptoAlgorithmHMAC_Import::importKey which is called before this one.
    // 6.5 If jwk does not meet the requirements of Section 6.4 of JSON Web Algorithms [JWA], then throw a DataError.

    // 6.6 Let data be the octet string obtained by decoding the k field of jwk.
    auto dataResult = base64::forgivingBase64Decode(
      jwk->k.value(), base64::base64URLDecodeTable);
    if (dataResult.isErr()) {
      DOMException::raise(cx,
                         "The JWK member 'k' could not be base64url decoded or contained padding", "DataError");
      return nullptr;
    }
    auto data_string = dataResult.unwrap();
    auto ddd = std::vector<uint8_t>(data_string.begin(), data_string.end());
    data = std::make_unique<std::span<uint8_t>>(
      ddd
    );
    // 6.7 Set the hash to equal the hash member of normalizedAlgorithm.
    CryptoAlgorithmIdentifier hash = this->hashIdentifier;
    switch(hash) {
      // 6.8 If the name attribute of hash is "SHA-1":
      case CryptoAlgorithmIdentifier::SHA_1: {
        // If the alg field of jwk is present and is not "HS1", then throw a DataError.
        if (jwk->alg.has_value() && jwk->alg.value() != "HS1") {
          DOMException::raise(cx, "Operation not permitted", "DataError");
          return nullptr;
        }
        break;
      }
      // 6.9 If the name attribute of hash is "SHA-256":
      case CryptoAlgorithmIdentifier::SHA_256: {
        // If the alg field of jwk is present and is not "HS256", then throw a DataError.
        if (jwk->alg.has_value() && jwk->alg.value() != "HS256") {
          DOMException::raise(cx, "Operation not permitted", "DataError");
          return nullptr;
        }
        break;
      }
      // 6.10 If the name attribute of hash is "SHA-384":
      case CryptoAlgorithmIdentifier::SHA_384: {
        // If the alg field of jwk is present and is not "HS384", then throw a DataError.
        if (jwk->alg.has_value() && jwk->alg.value() != "HS384") {
          DOMException::raise(cx, "Operation not permitted", "DataError");
          return nullptr;
        }
        break;
      }
      // 6.11 If the name attribute of hash is "SHA-512":
      case CryptoAlgorithmIdentifier::SHA_512: {
        // If the alg field of jwk is present and is not "HS512", then throw a DataError.
        if (jwk->alg.has_value() && jwk->alg.value() != "HS512") {
          DOMException::raise(cx, "Operation not permitted", "DataError");
          return nullptr;
        }
        break;
      }
      // 6.12 Otherwise, if the name attribute of hash is defined in another applicable specification:
      default: {
        // 6.13 Perform any key import steps defined by other applicable specifications, passing format, jwk and hash and obtaining hash.
        DOMException::raise(cx, "Operation not permitted", "DataError");
        return nullptr;
      }
    }
    // 6.14 If usages is non-empty and the use field of jwk is present and is not "sign", then throw a DataError.
    if (!usages.isEmpty() && jwk->use.has_value() && jwk->use != "sign") {
      DOMException::raise(cx, "Operation not permitted", "DataError");
      return nullptr;
    }
    // 6.15 If the key_ops field of jwk is present, and is invalid according to the requirements of JSON Web Key [JWK] or does not contain all of the specified usages values, then throw a DataError.
    if (jwk->key_ops.size() > 0) {
      auto ops = CryptoKeyUsages::from(jwk->key_ops);
      if (!ops.isSuperSetOf(usages)) {
        DOMException::raise(cx,
                            "The JWK 'key_ops' member was inconsistent with that specified by the "
                            "Web Crypto call. The JWK usage must be a superset of those requested", "DataError");
        return nullptr;
      }
    }
    // 6.16 If the ext field of jwk is present and has the value false and extractable is true, then throw a DataError.
    if (jwk->ext && !jwk->ext.value() && extractable) {
      DOMException::raise(cx, "Data provided to an operation does not meet requirements", "DataError");
      return nullptr;
    }
    break;
  }
  // 7. Otherwise: throw a NotSupportedError.
  default: {
    DOMException::raise(cx, "Supplied format is not supported", "NotSupportedError");
    return nullptr;
  }
  }


  // 8. Let length be equivalent to the length, in octets, of data, multiplied by 8.
  auto length = data->size() * 8;
  // 9. If length is zero then throw a DataError.
  if (length == 0) {
    DOMException::raise(cx, "Supplied keyData can not have a length of 0.", "DataError");
    return nullptr;
  }

  // 10. If the length member of normalizedAlgorithm is present:
  if (this->length.has_value()) {
    // 11. If the length member of normalizedAlgorithm is greater than length:
    if (this->length.value() > length) {
      // throw a DataError.
      DOMException::raise(cx, "The optional HMAC key length must be shorter than the key data, and by no more than 7 bits.", "DataError");
      return nullptr;
    // 12. If the length member of normalizedAlgorithm, is less than or equal to length minus eight:
    } else if (this->length.value() <= (length - 8)) {
      // throw a DataError.
      DOMException::raise(cx, "The optional HMAC key length must be shorter than the key data, and by no more than 7 bits.", "DataError");
      return nullptr;
    // 13. Otherwise:
    } else {
      // 14. Set length equal to the length member of normalizedAlgorithm.
      length = this->length.value();
    }
  } else {
    // This is because we need to capture the the length in the algorithm object which is created in CryptoAlgorithmHMAC_Import::toObject
    this->length = length;
  }
  // 15. Let key be a new {{CryptoKey}} object representing an HMAC key with the first length bits of data.
  // 16. Let algorithm be a new HmacKeyAlgorithm.
  // 17. Set the name attribute of algorithm to "HMAC".
  // 18. Set the length attribute of algorithm to length.
  // 19. Set the hash attribute of algorithm to hash.
  // 20. Set the [[algorithm]] internal slot of key to algorithm.
  // 21. Return key.
  return CryptoKey::createHMAC(cx, this, std::move(data), length, extractable, usages);
}

JSObject *CryptoAlgorithmHMAC_Import::importKey(JSContext *cx, CryptoKeyFormat format, JS::HandleValue key_data, bool extractable,
                      CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  // The only supported formats for HMAC are raw, and jwk.
  if (format != CryptoKeyFormat::Raw && format != CryptoKeyFormat::Jwk) {
    MOZ_ASSERT_UNREACHABLE("coding error");
    return nullptr;
  }

  KeyData data;
  if (format == CryptoKeyFormat::Jwk) {
    // This handles step 6.4: If the kty field of jwk is not a case-sensitive string match to "oct", then throw a DataError.
    auto jwk = JsonWebKey::parse(cx, key_data, "oct");
    if (!jwk) {
      return nullptr;
    }
    data = jwk.release();
  } else {
    std::optional<std::span<uint8_t>> buffer = value_to_buffer(cx, key_data, "");
    if (!buffer.has_value()) {
      // value_to_buffer would have already created a JS exception so we don't need to create one
      // ourselves.
      return nullptr;
    }
    data = buffer.value();
  }
  return this->importKey(cx, format, data, extractable, usages);
}
JSObject *CryptoAlgorithmHMAC_Import::toObject(JSContext *cx) {
  // Let algorithm be a new RsaHashedKeyAlgorithm dictionary.
  JS::RootedObject algorithm(cx, JS_NewPlainObject(cx));

  // Set the name attribute of algorithm to "HMAC"
  auto alg_name = JS_NewStringCopyZ(cx, this->name());
  if (!alg_name) {
    return nullptr;
  }
  JS::RootedValue name_val(cx, JS::StringValue(alg_name));
  if (!JS_SetProperty(cx, algorithm, "name", name_val)) {
    return nullptr;
  }

  // Set the hash attribute of algorithm to the hash member of normalizedAlgorithm.
  JS::RootedObject hash(cx, JS_NewObject(cx, nullptr));

  auto hash_name = JS_NewStringCopyZ(cx, algorithmName(this->hashIdentifier));
  if (!hash_name) {
    return nullptr;
  }
  JS::RootedValue hash_name_val(cx, JS::StringValue(hash_name));
  if (!JS_SetProperty(cx, hash, "name", hash_name_val)) {
    return nullptr;
  }
  JS::RootedValue hash_val(cx, JS::ObjectValue(*hash));
  if (!JS_SetProperty(cx, algorithm, "hash", hash_val)) {
    return nullptr;
  }

  // Set the length attribute of algorithm to the length member of normalizedAlgorithm.
  MOZ_ASSERT(this->length.has_value());
  JS::RootedValue length_val(cx, JS::NumberValue(this->length.value()));
  if (!JS_SetProperty(cx, algorithm, "length", length_val)) {
    return nullptr;
  }
  return algorithm;
}

JSObject *CryptoAlgorithmECDSA_Import::importKey(JSContext *cx, CryptoKeyFormat format,
                                                             KeyData keyData, bool extractable,
                                                             CryptoKeyUsages usages) {
  // 1. Let keyData be the key data to be imported.
  MOZ_ASSERT(cx);
  JS::RootedObject result(cx);

  std::unique_ptr<CryptoKeyECComponents> key = nullptr;
  switch (format) {
    // 2.  If format is "jwk":
    case CryptoKeyFormat::Jwk: {
      // 2.1. If keyData is a JsonWebKey dictionary:
        // Let jwk equal keyData.
      // Otherwise:
        // Throw a DataError.
      auto jwk = std::get<JsonWebKey *>(keyData);
      if (!jwk) {
        DOMException::raise(cx, "Supplied keyData is not a JSONWebKey", "DataError");
        return nullptr;
      }
      // 2.2. If the "d" field is present and usages contains a value which is not "sign",
      //  or, if the "d" field is not present and usages contains a value which is not
      // "verify" then throw a SyntaxError.
      if (
        (jwk->d.has_value() && !usages.isEmpty() && !usages.canOnlySign()) ||
        (!jwk->d.has_value() && !usages.isEmpty() && !usages.canOnlyVerify())
        ) {
          DOMException::raise(cx, "EC keys only support 'sign' and 'verify' operations", "SyntaxError");
          return nullptr;
      }

      // 2.3. If the "kty" field of jwk is not "EC", then throw a DataError.
      // NOTE: This has already been done in the parent function.

      // 2.4. If usages is non-empty and the "use" field of jwk is present and is not "sig", then throw a DataError.

      if (!usages.isEmpty() && jwk->use.has_value() && jwk->use.value() != "sig") {
        DOMException::raise(cx, "Operation not permitted", "DataError");
        return nullptr;
      }
      // 2.5. If the "key_ops" field of jwk is present, and is invalid according to the requirements of JSON Web Key, or it does not contain all of the specified usages values, then throw a DataError.
      if (jwk->key_ops.size() > 0) {
        auto ops = CryptoKeyUsages::from(jwk->key_ops);
        if (!ops.isSuperSetOf(usages)) {
          DOMException::raise(cx,
                              "The JWK 'key_ops' member was inconsistent with that specified by the "
                              "Web Crypto call. The JWK usage must be a superset of those requested", "DataError");
          return nullptr;
        }
      }
      // 2.6. If the "ext" field of jwk is present and has the value false and extractable is true, then throw a DataError.
      if (jwk->ext && !jwk->ext.value() && extractable) {
        DOMException::raise(cx, "Data provided to an operation does not meet requirements", "DataError");
        return nullptr;
      }
      // 2.7. Let namedCurve be a string whose value is equal to the "crv" field of jwk.
      auto namedCurve = toNamedCurve(jwk->crv.value());
      // 2.8. If namedCurve is not equal to the namedCurve member of normalizedAlgorithm, throw a DataError.
      if (!namedCurve.has_value() || namedCurve.value() != this->namedCurve) {
        DOMException::raise(cx, "The JWK's \"crv\" member specifies a different curve than requested", "DataError");
        return nullptr;
      }
      // 2.9.  If namedCurve is equal to "P-256", "P-384" or "P-521":
      // NOTE: At this point in time, namedCurve can _only_ be a valid value.
        // 2.9.1. Let algNamedCurve be a string whose initial value is undefined.
        // 2.9.2. If the "alg" field is not present:
        //     Let algNamedCurve be undefined.
        // If the "alg" field is equal to the string "ES256":
        //     Let algNamedCurve be the string "P-256".
        // If the "alg" field is equal to the string "ES384":
        //     Let algNamedCurve be the string "P-384".
        // If the "alg" field is equal to the string "ES512":
        //     Let algNamedCurve be the string "P-521".
        // otherwise:
        //     throw a DataError.
        // 2.9.3. If algNamedCurve is defined, and is not equal to namedCurve, throw a DataError.
        if (jwk->alg.has_value()) {
          auto algNamedCurve = toNamedCurve(jwk->crv.value());
          if (!algNamedCurve.has_value() || algNamedCurve.value() != this->namedCurve) {
            DOMException::raise(cx, "Oopsie", "DataError");
            return nullptr;
          }
        }
        // 2.9.4. If the "d" field is present:
        if (jwk->d.has_value()) {
          // 2.9.4.1. If jwk does not meet the requirements of Section 6.2.2 of JSON Web Algorithms, then throw a DataError.
          // https://datatracker.ietf.org/doc/html/rfc7518#section-6.2.2.1
          // The "d" (ECC private key) parameter contains the Elliptic Curve
          // private key value.  It is represented as the base64url encoding of
          // the octet string representation of the private key value, as defined
          // in Section 2.3.7 of SEC1 [SEC1].  The length of this octet string
          // MUST be ceiling(log-base-2(n)/8) octets (where n is the order of the
          // curve).
          auto dResult = base64::forgivingBase64Decode(
              jwk->d.value(), base64::base64URLDecodeTable);
          if (dResult.isErr()) {
            DOMException::raise(
                cx, "The JWK member 'd' could not be base64url decoded or contained padding", "DataError");
            return nullptr;
          }
          auto d = dResult.unwrap();
          switch (this->namedCurve) {
            case NamedCurve::P256: {
              if (d.length() != 32) {
                auto message = fmt::format("The JWK's \"d\" member defines an octet string of length {} bytes but should be 32", d.length());
                DOMException::raise(cx, message, "SyntaxError");
                return nullptr;
              }
              break;
            }
            case NamedCurve::P384: {
              if (d.length() != 48) {
                auto message = fmt::format("The JWK's \"d\" member defines an octet string of length {} bytes but should be 48", d.length());
                DOMException::raise(cx, message, "SyntaxError");
                return nullptr;
              }
              break;
            }
            case NamedCurve::P521: {
              if (d.length() != 66) {
                auto message = fmt::format("The JWK's \"d\" member defines an octet string of length {} bytes but should be 66", d.length());
                DOMException::raise(cx, message, "SyntaxError");
                return nullptr;
              }
              break;
            }
          }
          // 2.9.4.2. Let key be a new CryptoKey object that represents the Elliptic Curve private key identified by interpreting jwk according to Section 6.2.2 of JSON Web Algorithms.
          // 2.9.4.3. Set the [[type]] internal slot of Key to "private".
          key = createECPrivateKeyFromJWK(cx, jwk);
          if (!key) {
            return nullptr;
          }
        // Otherwise:
        } else {
          // 2.9.4.1. If jwk does not meet the requirements of Section 6.2.1 of JSON Web Algorithms, then throw a DataError.
          // 2.9.4.2. Let key be a new CryptoKey object that represents the Elliptic Curve public key identified by interpreting jwk according to Section 6.2.1 of JSON Web Algorithms.
          // 2.9.4.3. Set the [[type]] internal slot of Key to "public".
          key = createECPublicKeyFromJWK(cx, jwk);
          if (!key) {
            return nullptr;
          }
        }
      //  Otherwise:
        // 2.9.1. Perform any key import steps defined by other applicable specifications, passing format, jwk and obtaining key.
        // 2.9.2. If an error occured or there are no applicable specifications, throw a DataError.
        // NOTE: We do not implement the above 2 steps as we only support "P-256", "P-384", and "P-521".

      // 2.10. If the key value is not a valid point on the Elliptic Curve identified by the namedCurve member of normalizedAlgorithm throw a DataError.
      // 2.11. Let algorithm be a new instance of an EcKeyAlgorithm object.
      // 2.12. Set the name attribute of algorithm to "ECDSA".
      // 2.13. Set the namedCurve attribute of algorithm to namedCurve.
      // 2.14. Set the [[algorithm]] internal slot of key to algorithm.
      return CryptoKey::createECDSA(cx, this, std::move(key), extractable, usages);
      break;
    }
    case CryptoKeyFormat::Pkcs8:
    case CryptoKeyFormat::Raw:
    case CryptoKeyFormat::Spki: {
      MOZ_ASSERT(false);
      // TODO finish this
    }
  }
  return nullptr;
}

JSObject *CryptoAlgorithmECDSA_Import::importKey(JSContext *cx, CryptoKeyFormat format, JS::HandleValue key_data, bool extractable,
                      CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);

  KeyData data;
  switch (format) {
    case CryptoKeyFormat::Jwk: {
      // This handles step 2.3: If the "kty" field of jwk is not "EC", then throw a DataError.
      auto jwk = JsonWebKey::parse(cx, key_data, "EC");
      if (!jwk) {
        return nullptr;
      }
      data = jwk.release();
      break;
    }
    case CryptoKeyFormat::Pkcs8:
    case CryptoKeyFormat::Raw:
    case CryptoKeyFormat::Spki: {
      // TODO finish this
      DOMException::raise(cx, "Supplied format is not yet supported", "NotSupportedError");
      return nullptr;
    }
  }
  return this->importKey(cx, format, data, extractable, usages);

}
JSObject *CryptoAlgorithmECDSA_Import::toObject(JSContext *cx) {
  // Let algorithm be a new RsaHashedKeyAlgorithm dictionary.
  JS::RootedObject algorithm(cx, JS_NewPlainObject(cx));

  // Set the name attribute of algorithm to "RSASSA-PKCS1-v1_5"
  auto alg_name = JS_NewStringCopyZ(cx, this->name());
  if (!alg_name) {
    return nullptr;
  }
  JS::RootedValue name_val(cx, JS::StringValue(alg_name));
  if (!JS_SetProperty(cx, algorithm, "name", name_val)) {
    return nullptr;
  }

  // Set the hash attribute of algorithm to the hash member of normalizedAlgorithm.
  JS::RootedObject hash(cx, JS_NewObject(cx, nullptr));

  auto curve_name = JS_NewStringCopyZ(cx, curveName(this->namedCurve));
  if (!curve_name) {
    return nullptr;
  }
  JS::RootedValue curve_name_val(cx, JS::StringValue(curve_name));
  if (!JS_SetProperty(cx, algorithm, "namedCurve", curve_name_val)) {
    return nullptr;
  }
  return algorithm;
}

std::unique_ptr<CryptoAlgorithmECDSA_Sign_Verify> CryptoAlgorithmECDSA_Sign_Verify::fromParameters(JSContext *cx, JS::HandleObject parameters) {
  JS::Rooted<JS::Value> hash_val(cx);
  if (!JS_GetProperty(cx, parameters, "hash", &hash_val)) {
    return nullptr;
  }
  auto hashIdentifier = toHashIdentifier(cx, hash_val);
  if (hashIdentifier.isErr()) {
    return nullptr;
  }
  return std::make_unique<CryptoAlgorithmECDSA_Sign_Verify>(hashIdentifier.unwrap());
}

std::unique_ptr<CryptoAlgorithmECDSA_Import> CryptoAlgorithmECDSA_Import::fromParameters(JSContext *cx, JS::HandleObject parameters) {
  JS::Rooted<JS::Value> namedCurve_val(cx);
  if (!JS_GetProperty(cx, parameters, "namedCurve", &namedCurve_val)) {
    return nullptr;
  }

  // P-256
  // P-384
  // P-512
  auto namedCurve = toNamedCurve(cx, namedCurve_val);
  if (namedCurve.isErr()) {
    return nullptr;
  }
  return std::make_unique<CryptoAlgorithmECDSA_Import>(namedCurve.unwrap());
}

std::unique_ptr<CryptoAlgorithmRSASSA_PKCS1_v1_5_Import> CryptoAlgorithmRSASSA_PKCS1_v1_5_Import::fromParameters(JSContext *cx, JS::HandleObject parameters) {
  JS::Rooted<JS::Value> hash_val(cx);
  if (!JS_GetProperty(cx, parameters, "hash", &hash_val)) {
    return nullptr;
  }
  auto hashIdentifier = toHashIdentifier(cx, hash_val);
  if (hashIdentifier.isErr()) {
    return nullptr;
  }
  return std::make_unique<CryptoAlgorithmRSASSA_PKCS1_v1_5_Import>(hashIdentifier.unwrap());
}

// https://w3c.github.io/webcrypto/#rsassa-pkcs1-operations
JSObject *CryptoAlgorithmRSASSA_PKCS1_v1_5_Import::importKey(JSContext *cx, CryptoKeyFormat format,
                                                             KeyData keyData, bool extractable,
                                                             CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  JS::RootedObject result(cx);
  switch (format) {
  // 2. If format is "jwk":
  case CryptoKeyFormat::Jwk: {
    // 2.1. If keyData is a JsonWebKey dictionary:
      // Let jwk equal keyData.
    // Otherwise:
      // Throw a DataError.
    auto jwk = std::get<JsonWebKey *>(keyData);
    if (!jwk) {
      DOMException::raise(cx, "Supplied keyData is not a JSONWebKey", "DataError");
      return nullptr;
    }


    // 2.2 If the d field of jwk is present and usages contains an entry which
    // is not "sign", or, if the d field of jwk is not present and usages
    // contains an entry which is not "verify" then throw a SyntaxError.
    bool isUsagesAllowed = false;
    // public key
    if (jwk->d.has_value()) {
      isUsagesAllowed = usages.canOnlySign();
    } else {
      // private key
      isUsagesAllowed = usages.canOnlyVerify();
    }
    if (!isUsagesAllowed) {
      DOMException::raise(cx,
                          "The JWK 'key_ops' member was inconsistent with that specified by the "
                          "Web Crypto call. The JWK usage must be a superset of those requested", "DataError");
      return nullptr;
    }

    // 2.3 If the kty field of jwk is not a case-sensitive string match to "RSA", then throw a DataError.

    // Step 2.3 has already been done in the other implementation of
    // CryptoAlgorithmRSASSA_PKCS1_v1_5_Import::importKey which is called before this one.

    // 2.4. If usages is non-empty and the use field of jwk is present and is
    // not a case-sensitive string match to "sig", then throw a DataError.
    if (!usages.isEmpty() && jwk->use.has_value() && jwk->use.value() != "sig") {
      DOMException::raise(cx, "Operation not permitted", "DataError");
      return nullptr;
    }

    // 2.5. If the key_ops field of jwk is present, and is invalid according to
    // the requirements of JSON Web Key [JWK] or does not contain all of the
    // specified usages values, then throw a DataError.
    if (jwk->key_ops.size() > 0) {
      auto ops = CryptoKeyUsages::from(jwk->key_ops);
      if (!ops.isSuperSetOf(usages)) {
        DOMException::raise(cx,
                            "The JWK 'key_ops' member was inconsistent with that specified by the "
                            "Web Crypto call. The JWK usage must be a superset of those requested", "DataError");
        return nullptr;
      }
    }

    // 2.6 If the ext field of jwk is present and has the value false and
    // extractable is true, then throw a DataError.
    if (jwk->ext && !jwk->ext.value() && extractable) {
      DOMException::raise(cx, "Data provided to an operation does not meet requirements", "DataError");
      return nullptr;
    }

    // 2.7 Let hash be a be a string whose initial value is undefined.
    // 2.8 If the alg field of jwk is not present:
        // Let hash be undefined.
      // If the alg field is equal to the string "RS1":
        // Let hash be the string "SHA-1".
      // If the alg field is equal to the string "RS256":
        // Let hash be the string "SHA-256".
      // If the alg field is equal to the string "RS384":
        // Let hash be the string "SHA-384".
      // If the alg field is equal to the string "RS512":
        // Let hash be the string "SHA-512".
      // Otherwise:
        // Perform any key import steps defined by other applicable specifications, passing format, jwk and obtaining hash.
        // If an error occurred or there are no applicable specifications, throw a DataError.
    // 2.9 If hash is not undefined:
      // Let normalizedHash be the result of normalize an algorithm with alg set to hash and op set to digest.
      // If normalizedHash is not equal to the hash member of normalizedAlgorithm, throw a DataError.
    bool isMatched = false;
    switch (this->hashIdentifier) {
    case CryptoAlgorithmIdentifier::SHA_1: {
      isMatched = !jwk->alg.has_value() || jwk->alg == "RS1";
      break;
    }
    case CryptoAlgorithmIdentifier::SHA_256: {
      isMatched = !jwk->alg.has_value() || jwk->alg == "RS256";
      break;
    }
    case CryptoAlgorithmIdentifier::SHA_384: {
      isMatched = !jwk->alg.has_value() || jwk->alg == "RS384";
      break;
    }
    case CryptoAlgorithmIdentifier::SHA_512: {
      isMatched = !jwk->alg.has_value() || jwk->alg == "RS512";
      break;
    }
    default: {
      break;
    }
    }
    if (!isMatched) {
      DOMException::raise(cx,
                          "The JWK 'alg' member was inconsistent with that specified by the Web Crypto call",
                          "DataError");
      return nullptr;
    }

    std::unique_ptr<CryptoKeyRSAComponents> key = nullptr;
    // 2.10 If the d field of jwk is present:
    if (jwk->d.has_value()) {
      // 2.10.1 If jwk does not meet the requirements of Section 6.3.2 of JSON Web Algorithms [JWA], then throw a DataError.
      // 2.10.2 Let privateKey represents the RSA private key identified by interpreting jwk according to Section 6.3.2 of JSON Web Algorithms [JWA].
      // 2.10.3 If privateKey is not a valid RSA private key according to [RFC3447], then throw a DataError.
      key = createRSAPrivateKeyFromJWK(cx, jwk);
      if (!key) {
        return nullptr;
      }
    // 2.10 Otherwise:
    } else {
      // 2.10.1 If jwk does not meet the requirements of Section 6.3.1 of JSON Web Algorithms [JWA], then throw a DataError.
      // 2.10.2 Let publicKey represent the RSA public key identified by interpreting jwk according to Section 6.3.1 of JSON Web Algorithms [JWA].
      // 2.10.3 If publicKey can be determined to not be a valid RSA public key according to [RFC3447], then throw a DataError.
      key = createRSAPublicKeyFromJWK(cx, jwk);
      if (!key) {
        return nullptr;
      }
    }
    // 2.10.4 Let key be a new CryptoKey object that represents privateKey.
    // 2.10.5 Set the [[type]] internal slot of key to "private"
    // 3. Let algorithm be a new RsaHashedKeyAlgorithm dictionary.
    // 4. Set the name attribute of algorithm to "RSASSA-PKCS1-v1_5"
    // 5. Set the modulusLength attribute of algorithm to the length, in bits, of the RSA public modulus.
    // 6. Set the publicExponent attribute of algorithm to the BigInteger representation of the RSA public exponent.
    // 7. Set the hash attribute of algorithm to the hash member of normalizedAlgorithm.
    // 8. Set the [[algorithm]] internal slot of key to algorithm.
    // 9. Return key.
    return CryptoKey::createRSA(cx, this, std::move(key), extractable, usages);
  }
  case CryptoKeyFormat::Spki:
  case CryptoKeyFormat::Pkcs8: {
    // TODO: Add implementations for these
  }
  default: {
    DOMException::raise(cx, "Supplied format is not supported", "DataError");
    return nullptr;
  }
  }
}

JSObject *CryptoAlgorithmRSASSA_PKCS1_v1_5_Import::importKey(JSContext *cx,
                                                             CryptoKeyFormat format,
                                                             JS::HandleValue key_data,
                                                             bool extractable,
                                                             CryptoKeyUsages usages) {
  MOZ_ASSERT(cx);
  // The only support formats for RSASSA PKCS1 v1-5 are spki, pkcs8, and jwk.
  if (format == CryptoKeyFormat::Raw) {
    MOZ_ASSERT_UNREACHABLE("coding error");
    return nullptr;
  }

  KeyData data;
  if (format == CryptoKeyFormat::Jwk) {
    // This handles set 2.3: If the kty field of jwk is not a case-sensitive string match to "RSA", then throw a DataError.
    auto jwk = JsonWebKey::parse(cx, key_data, "RSA");
    if (!jwk) {
      return nullptr;
    }
    data = jwk.release();
  } else {
    std::optional<std::span<uint8_t>> buffer = value_to_buffer(cx, key_data, "");
    if (!buffer.has_value()) {
      // value_to_buffer would have already created a JS exception so we don't need to create one
      // ourselves.
      return nullptr;
    }
    data = buffer.value();
  }
  return this->importKey(cx, format, data, extractable, usages);
}

JSObject *CryptoAlgorithmRSASSA_PKCS1_v1_5_Import::toObject(JSContext *cx) {
  // Let algorithm be a new RsaHashedKeyAlgorithm dictionary.
  JS::RootedObject algorithm(cx, JS_NewPlainObject(cx));

  // Set the name attribute of algorithm to "RSASSA-PKCS1-v1_5"
  auto alg_name = JS_NewStringCopyZ(cx, this->name());
  if (!alg_name) {
    return nullptr;
  }
  JS::RootedValue name_val(cx, JS::StringValue(alg_name));
  if (!JS_SetProperty(cx, algorithm, "name", name_val)) {
    return nullptr;
  }

  // Set the hash attribute of algorithm to the hash member of normalizedAlgorithm.
  JS::RootedObject hash(cx, JS_NewObject(cx, nullptr));

  auto hash_name = JS_NewStringCopyZ(cx, algorithmName(this->hashIdentifier));
  if (!hash_name) {
    return nullptr;
  }
  JS::RootedValue hash_name_val(cx, JS::StringValue(hash_name));
  if (!JS_SetProperty(cx, hash, "name", hash_name_val)) {
    return nullptr;
  }
  JS::RootedValue hash_val(cx, JS::ObjectValue(*hash));
  if (!JS_SetProperty(cx, algorithm, "hash", hash_val)) {
    return nullptr;
  }
  return algorithm;
}

JSObject *CryptoAlgorithmMD5::digest(JSContext *cx, std::span<uint8_t> data) {
  return builtins::web::crypto::digest(cx, data, EVP_md5(), MD5_DIGEST_LENGTH);
}
JSObject *CryptoAlgorithmSHA1::digest(JSContext *cx, std::span<uint8_t> data) {
  return builtins::web::crypto::digest(cx, data, EVP_sha1(), SHA_DIGEST_LENGTH);
}
JSObject *CryptoAlgorithmSHA256::digest(JSContext *cx, std::span<uint8_t> data) {
  return builtins::web::crypto::digest(cx, data, EVP_sha256(), SHA256_DIGEST_LENGTH);
}
JSObject *CryptoAlgorithmSHA384::digest(JSContext *cx, std::span<uint8_t> data) {
  return builtins::web::crypto::digest(cx, data, EVP_sha384(), SHA384_DIGEST_LENGTH);
}
JSObject *CryptoAlgorithmSHA512::digest(JSContext *cx, std::span<uint8_t> data) {
  return builtins::web::crypto::digest(cx, data, EVP_sha512(), SHA512_DIGEST_LENGTH);
}

} // namespace builtins::web::crypto
