#ifndef JS_COMPUTE_RUNTIME_CRYPTO_KEY_H
#define JS_COMPUTE_RUNTIME_CRYPTO_KEY_H

#include "builtin.h"

#include "crypto-key-ec-components.h"
#include "crypto-key-rsa-components.h"
#include "openssl/evp.h"

namespace builtins {
enum class CryptoAlgorithmIdentifier : uint8_t;
class CryptoAlgorithmRSASSA_PKCS1_v1_5_Import;
class CryptoAlgorithmHMAC_Import;
class CryptoAlgorithmECDSA_Import;
enum class CryptoKeyType : uint8_t { Public, Private, Secret };

enum class CryptoKeyFormat : uint8_t { Raw, Spki, Pkcs8, Jwk };

class CryptoKeyUsages {
private:
  uint8_t mask = 0;

public:
  static constexpr const uint8_t encrypt_flag = 1 << 0;
  static constexpr const uint8_t decrypt_flag = 1 << 1;
  static constexpr const uint8_t sign_flag = 1 << 2;
  static constexpr const uint8_t verify_flag = 1 << 3;
  static constexpr const uint8_t derive_key_flag = 1 << 4;
  static constexpr const uint8_t derive_bits_flag = 1 << 5;
  static constexpr const uint8_t wrap_key_flag = 1 << 6;
  static constexpr const uint8_t unwrap_key_flag = 1 << 7;

  CryptoKeyUsages() = default;
  CryptoKeyUsages(uint8_t mask);
  CryptoKeyUsages(bool encrypt, bool decrypt, bool sign, bool verify, bool derive_key,
                  bool derive_bits, bool wrap_key, bool unwrap_key);
  static CryptoKeyUsages from(std::vector<std::string> key_usages);
  static JS::Result<CryptoKeyUsages> from(JSContext *cx, JS::HandleValue key_usages,
                                          std::string_view error_message);

  uint8_t toInt() { return this->mask; };

  bool isEmpty() { return this->mask == 0; };
  bool isSuperSetOf(CryptoKeyUsages &other) { return this->mask & other.mask; };

  bool canEncrypt() { return this->mask & encrypt_flag; };
  bool canDecrypt() { return this->mask & decrypt_flag; };
  bool canSign() { return this->mask & sign_flag; };
  bool canVerify() { return this->mask & verify_flag; };
  bool canDeriveKey() { return this->mask & derive_key_flag; };
  bool canDeriveBits() { return this->mask & derive_bits_flag; };
  bool canWrapKey() { return this->mask & wrap_key_flag; };
  bool canUnwrapKey() { return this->mask & unwrap_key_flag; };

  bool canOnlyEncrypt() { return this->mask == encrypt_flag; };
  bool canOnlyDecrypt() { return this->mask == decrypt_flag; };
  bool canOnlySign() { return this->mask == sign_flag; };
  bool canOnlyVerify() { return this->mask == verify_flag; };
  bool canOnlySignOrVerify() { return this->mask & (sign_flag | verify_flag); };
  bool canOnlyDeriveKey() { return this->mask == derive_key_flag; };
  bool canOnlyDeriveBits() { return this->mask == derive_bits_flag; };
  bool canOnlyWrapKey() { return this->mask == wrap_key_flag; };
  bool canOnlyUnwrapKey() { return this->mask == unwrap_key_flag; };
};

class CryptoKey : public BuiltinImpl<CryptoKey> {
public:
  static const int ctor_length = 0;
  static constexpr const char *class_name = "CryptoKey";

  // https://w3c.github.io/webcrypto/#dom-cryptokey-algorithm
  // Returns the cached ECMAScript object associated with the [[algorithm]] internal slot.
  static bool algorithm_get(JSContext *cx, unsigned argc, JS::Value *vp);

  // https://w3c.github.io/webcrypto/#dom-cryptokey-extractable
  // Reflects the [[extractable]] internal slot, which indicates whether or not the raw keying
  // material may be exported by the application.
  static bool extractable_get(JSContext *cx, unsigned argc, JS::Value *vp);

  // https://w3c.github.io/webcrypto/#dom-cryptokey-type
  // Reflects the [[type]] internal slot, which contains the type of the underlying key.
  static bool type_get(JSContext *cx, unsigned argc, JS::Value *vp);

  // https://w3c.github.io/webcrypto/#dom-cryptokey-usages
  // Returns the cached ECMAScript object associated with the [[usages]] internal slot, which
  // indicates which cryptographic operations are permissible to be used with this key.
  static bool usages_get(JSContext *cx, unsigned argc, JS::Value *vp);

  enum Slots {
    // https://w3c.github.io/webcrypto/#ref-for-dfn-CryptoKey-slot-algorithm-1
    // The contents of the [[algorithm]] internal slot shall be, or be derived from, a KeyAlgorithm.
    // We store a JS::ObjectValue within this slot which contains a JS Object representation of the
    // algorithm.
    Algorithm,
    // The type of the underlying key.
    // We store a JS::Int32Value representation of the CryptoKeyType variant in this slot
    Type,
    // Indicates whether or not the raw keying material may be exported by the application
    // We store a JS::BooleanValue in this slot
    Extractable,
    // Indicates which cryptographic operations are permissible to be used with this key
    // We store a JS::Int32Value representation of a CryptoKeyUsages. in this slot
    Usages,
    // Returns the cached ECMAScript object associated with the [[usages]] internal slot,
    // which indicates which cryptographic operations are permissible to be used with this key.
    UsagesArray,
    // We store a JS::PrivateValue in this slot, it will contain either the raw key data.
    // It will either be an `EVP_PKEY *` or an `uint8_t *`.
    // `uint8_t *` is used only for HMAC keys, `EVP_PKEY *` is used for all the other key types.
    Key,
    KeyData,
    KeyDataLength,
    Count
  };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool init_class(JSContext *cx, JS::HandleObject global);

  static JSObject *createHMAC(JSContext *cx, CryptoAlgorithmHMAC_Import *algorithm,
                              std::unique_ptr<std::span<uint8_t>> data, unsigned long length,
                              bool extractable, CryptoKeyUsages usages);
  static JSObject *createRSA(JSContext *cx, CryptoAlgorithmRSASSA_PKCS1_v1_5_Import *algorithm,
                             std::unique_ptr<CryptoKeyRSAComponents> keyData, bool extractable,
                             CryptoKeyUsages usages);
  static JSObject *createECDSA(JSContext *cx, CryptoAlgorithmECDSA_Import *algorithm,
                               std::unique_ptr<CryptoKeyECComponents> keyData, bool extractable,
                               CryptoKeyUsages usages);
  static CryptoKeyType type(JSObject *self);
  static JSObject *get_algorithm(JS::HandleObject self);
  static EVP_PKEY *key(JSObject *self);
  static std::span<uint8_t> hmacKeyData(JSObject *self);
  static bool canSign(JS::HandleObject self);
  static bool canVerify(JS::HandleObject self);
  static JS::Result<bool> is_algorithm(JSContext *cx, JS::HandleObject self,
                                       CryptoAlgorithmIdentifier algorithm);
};

} // namespace builtins
#endif