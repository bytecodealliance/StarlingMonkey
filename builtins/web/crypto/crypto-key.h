#ifndef BUILTINS_WEB_CRYPTO_CRYPTO_KEY_H
#define BUILTINS_WEB_CRYPTO_CRYPTO_KEY_H

#include "builtin.h"

#include "crypto-key-ec-components.h"
#include "crypto-key-rsa-components.h"
#include "crypto-raii.h"
#include "openssl/evp.h"



namespace builtins::web::crypto {
enum class CryptoAlgorithmIdentifier : uint8_t;
class CryptoAlgorithmRSASSA_PKCS1_v1_5_Import;
class CryptoAlgorithmHMAC_Import;
class CryptoAlgorithmECDSA_Import;
class CryptoAlgorithmEd25519_Import;
enum class CryptoKeyType : uint8_t { Public, Private, Secret };

enum class CryptoKeyFormat : uint8_t { Raw, Spki, Pkcs8, Jwk };

class CryptoKeyUsages {
private:
  uint16_t mask = 0;

public:
  static constexpr const uint16_t encrypt_flag = 1 << 0;
  static constexpr const uint16_t decrypt_flag = 1 << 1;
  static constexpr const uint16_t sign_flag = 1 << 2;
  static constexpr const uint16_t verify_flag = 1 << 3;
  static constexpr const uint16_t derive_key_flag = 1 << 4;
  static constexpr const uint16_t derive_bits_flag = 1 << 5;
  static constexpr const uint16_t wrap_key_flag = 1 << 6;
  static constexpr const uint16_t unwrap_key_flag = 1 << 7;
  static constexpr const uint16_t encapsulate_key_flag = 1 << 8;
  static constexpr const uint16_t encapsulate_bits_flag = 1 << 9;
  static constexpr const uint16_t decapsulate_key_flag = 1 << 10;
  static constexpr const uint16_t decapsulate_bits_flag = 1 << 11;

  CryptoKeyUsages() = default;
  CryptoKeyUsages(uint16_t mask);
  CryptoKeyUsages(bool encrypt, bool decrypt, bool sign, bool verify, bool derive_key,
                  bool derive_bits, bool wrap_key, bool unwrap_key);
  static CryptoKeyUsages from(const std::vector<std::string>& key_usages);
  static JS::Result<CryptoKeyUsages> from(JSContext *cx, JS::HandleValue key_usages);

  [[nodiscard]] uint16_t toInt() const { return this->mask; };

  [[nodiscard]] bool isEmpty() const { return this->mask == 0; };
  bool isSuperSetOf(CryptoKeyUsages &other) const { return (this->mask & other.mask) != 0; };

  [[nodiscard]] bool canEncrypt() const { return (this->mask & encrypt_flag) != 0; };
  [[nodiscard]] bool canDecrypt() const { return (this->mask & decrypt_flag) != 0; };
  [[nodiscard]] bool canSign() const { return (this->mask & sign_flag) != 0; };
  [[nodiscard]] bool canVerify() const { return (this->mask & verify_flag) != 0; };
  [[nodiscard]] bool canDeriveKey() const { return (this->mask & derive_key_flag) != 0; };
  [[nodiscard]] bool canDeriveBits() const { return (this->mask & derive_bits_flag) != 0; };
  [[nodiscard]] bool canWrapKey() const { return (this->mask & wrap_key_flag) != 0; };
  [[nodiscard]] bool canUnwrapKey() const { return (this->mask & unwrap_key_flag) != 0; };
  [[nodiscard]] bool canEncapsulateKey() const { return (this->mask & encapsulate_key_flag) != 0; };
  [[nodiscard]] bool canEncapsulateBits() const { return (this->mask & encapsulate_bits_flag) != 0; };
  [[nodiscard]] bool canDecapsulateKey() const { return (this->mask & decapsulate_key_flag) != 0; };
  [[nodiscard]] bool canDecapsulateBits() const { return (this->mask & decapsulate_bits_flag) != 0; };

  [[nodiscard]] bool canOnlyEncrypt() const { return this->mask == encrypt_flag; };
  [[nodiscard]] bool canOnlyDecrypt() const { return this->mask == decrypt_flag; };
  [[nodiscard]] bool canOnlySign() const { return this->mask == sign_flag; };
  [[nodiscard]] bool canOnlyVerify() const { return this->mask == verify_flag; };
  [[nodiscard]] bool canOnlySignOrVerify() const {
    return this->mask == sign_flag || this->mask == verify_flag ||
           this->mask == (sign_flag | verify_flag);
  };
  [[nodiscard]] bool canOnlyDeriveKey() const { return this->mask == derive_key_flag; };
  [[nodiscard]] bool canOnlyDeriveBits() const { return this->mask == derive_bits_flag; };
  [[nodiscard]] bool canOnlyDeriveBitsOrKey() const {
    return this->mask == derive_bits_flag || this->mask == derive_key_flag ||
           this->mask == (derive_bits_flag | derive_key_flag);
  };
  [[nodiscard]] bool canOnlyWrapKey() const { return this->mask == wrap_key_flag; };
  [[nodiscard]] bool canOnlyUnwrapKey() const { return this->mask == unwrap_key_flag; };
};

class CryptoKey : public BuiltinNoConstructor<CryptoKey> {
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

  enum class Slots : uint8_t {
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

  static bool init_class(JSContext *cx, JS::HandleObject global);

  static JSObject *createHMAC(JSContext *cx, CryptoAlgorithmHMAC_Import *algorithm,
                              std::unique_ptr<std::vector<uint8_t>> data, unsigned long length,
                              bool extractable, CryptoKeyUsages usages);
  // Shared constructor for `secret`-type CryptoKey objects whose key material is a raw
  // byte string (e.g. HMAC and PBKDF2 keys). `algorithm_object` is the already-materialized
  // KeyAlgorithm object to store in the [[algorithm]] internal slot.
  static JSObject *createSymmetric(JSContext *cx, JS::HandleObject algorithm_object,
                                   std::unique_ptr<std::vector<uint8_t>> data, bool extractable,
                                   CryptoKeyUsages usages);
  static JSObject *createRSA(JSContext *cx, CryptoAlgorithmRSASSA_PKCS1_v1_5_Import *algorithm,
                             std::unique_ptr<CryptoKeyRSAComponents> keyData, bool extractable,
                             CryptoKeyUsages usages);
  static JSObject *createECDSA(JSContext *cx, CryptoAlgorithmECDSA_Import *algorithm,
                               std::unique_ptr<CryptoKeyECComponents> keyData, bool extractable,
                               CryptoKeyUsages usages);
  static JSObject *createEd25519(JSContext *cx, CryptoAlgorithmEd25519_Import *algorithm,
                                 EvpPkeyPtr pkey, CryptoKeyType keyType, bool extractable,
                                 CryptoKeyUsages usages);
  static CryptoKeyType type(JSObject *self);
  static JSObject *get_algorithm(JS::HandleObject self);
  static EVP_PKEY *key(JSObject *self);
  static std::span<uint8_t> keyData(JSObject *self);
  static bool canEncrypt(JS::HandleObject self);
  static bool canDecrypt(JS::HandleObject self);
  static bool canSign(JS::HandleObject self);
  static bool canVerify(JS::HandleObject self);
  static bool canDeriveBits(JS::HandleObject self);
  static JS::Result<bool> is_algorithm(JSContext *cx, JS::HandleObject self,
                                       CryptoAlgorithmIdentifier algorithm);
};

} // namespace builtins::web::crypto


#endif
