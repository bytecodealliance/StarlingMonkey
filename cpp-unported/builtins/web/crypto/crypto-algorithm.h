#ifndef JS_COMPUTE_RUNTIME_CRYPTO_ALGORITHM_H
#define JS_COMPUTE_RUNTIME_CRYPTO_ALGORITHM_H
#include <span>

#include "builtin.h"
#include "crypto-key.h"
#include "json-web-key.h"
#include "openssl/evp.h"
#include <openssl/md5.h>

namespace builtins {

// We are defining all the algorithms from
// https://w3c.github.io/webcrypto/#issue-container-generatedID-15
enum class CryptoAlgorithmIdentifier : uint8_t {
  RSASSA_PKCS1_v1_5,
  RSA_PSS,
  RSA_OAEP,
  ECDSA,
  ECDH,
  AES_CTR,
  AES_CBC,
  AES_GCM,
  AES_KW,
  HMAC,
  MD5,
  SHA_1,
  SHA_256,
  SHA_384,
  SHA_512,
  HKDF,
  PBKDF2
};

enum class NamedCurve : uint8_t {
  P256,
  P384,
  P521,
};

const char *algorithmName(CryptoAlgorithmIdentifier algorithm);

/// The base class that all algorithm implementations should derive from.
class CryptoAlgorithm {
public:
  virtual ~CryptoAlgorithm() = default;

  virtual const char *name() const noexcept = 0;
  virtual CryptoAlgorithmIdentifier identifier() = 0;
};

using KeyData = std::variant<std::span<uint8_t>, JsonWebKey *>;

class CryptoAlgorithmImportKey : public CryptoAlgorithm {
public:
  virtual JSObject *importKey(JSContext *cx, CryptoKeyFormat format, JS::HandleValue key_data,
                              bool extractable, CryptoKeyUsages usages) = 0;
  virtual JSObject *importKey(JSContext *cx, CryptoKeyFormat format, KeyData key_data,
                              bool extractable, CryptoKeyUsages usages) = 0;
  static std::unique_ptr<CryptoAlgorithmImportKey> normalize(JSContext *cx, JS::HandleValue value);
};

class CryptoAlgorithmSignVerify : public CryptoAlgorithm {
public:
  virtual JSObject *sign(JSContext *cx, JS::HandleObject key, std::span<uint8_t> data) = 0;
  virtual JS::Result<bool> verify(JSContext *cx, JS::HandleObject key, std::span<uint8_t> signature,
                                  std::span<uint8_t> data) = 0;
  static std::unique_ptr<CryptoAlgorithmSignVerify> normalize(JSContext *cx, JS::HandleValue value);
};

class CryptoAlgorithmHMAC_Sign_Verify final : public CryptoAlgorithmSignVerify {
public:
  const char *name() const noexcept override { return "HMAC"; };
  CryptoAlgorithmHMAC_Sign_Verify(){};
  CryptoAlgorithmIdentifier identifier() final { return CryptoAlgorithmIdentifier::HMAC; };

  JSObject *sign(JSContext *cx, JS::HandleObject key, std::span<uint8_t> data) override;
  JS::Result<bool> verify(JSContext *cx, JS::HandleObject key, std::span<uint8_t> signature,
                          std::span<uint8_t> data) override;
  static JSObject *exportKey(JSContext *cx, CryptoKeyFormat format, JS::HandleObject key);
  JSObject *toObject(JSContext *cx);
};

class CryptoAlgorithmECDSA_Sign_Verify final : public CryptoAlgorithmSignVerify {
public:
  // The hash member describes the hash algorithm to use.
  CryptoAlgorithmIdentifier hashIdentifier;

  const char *name() const noexcept override { return "ECDSA"; };
  CryptoAlgorithmECDSA_Sign_Verify(CryptoAlgorithmIdentifier hashIdentifier)
      : hashIdentifier{hashIdentifier} {};

  // https://www.w3.org/TR/WebCryptoAPI/#EcdsaParams-dictionary
  // 23.3. EcdsaParams dictionary
  static std::unique_ptr<CryptoAlgorithmECDSA_Sign_Verify>
  fromParameters(JSContext *cx, JS::HandleObject parameters);
  CryptoAlgorithmIdentifier identifier() final { return CryptoAlgorithmIdentifier::ECDSA; };

  JSObject *sign(JSContext *cx, JS::HandleObject key, std::span<uint8_t> data) override;
  JS::Result<bool> verify(JSContext *cx, JS::HandleObject key, std::span<uint8_t> signature,
                          std::span<uint8_t> data) override;
  JSObject *toObject(JSContext *cx);
};

class CryptoAlgorithmECDSA_Import final : public CryptoAlgorithmImportKey {
public:
  // A named curve.
  NamedCurve namedCurve;

  const char *name() const noexcept override { return "ECDSA"; };
  CryptoAlgorithmECDSA_Import(NamedCurve namedCurve) : namedCurve{namedCurve} {};

  // https://www.w3.org/TR/WebCryptoAPI/#EcKeyImportParams-dictionary
  // 23.6 EcKeyImportParams dictionary
  static std::unique_ptr<CryptoAlgorithmECDSA_Import> fromParameters(JSContext *cx,
                                                                     JS::HandleObject parameters);

  CryptoAlgorithmIdentifier identifier() final { return CryptoAlgorithmIdentifier::ECDSA; };

  JSObject *importKey(JSContext *cx, CryptoKeyFormat format, JS::HandleValue, bool extractable,
                      CryptoKeyUsages usages) override;
  JSObject *importKey(JSContext *cx, CryptoKeyFormat format, KeyData, bool extractable,
                      CryptoKeyUsages usages) override;
  JSObject *toObject(JSContext *cx);
};

class CryptoAlgorithmRSASSA_PKCS1_v1_5_Sign_Verify final : public CryptoAlgorithmSignVerify {
public:
  const char *name() const noexcept override { return "RSASSA-PKCS1-v1_5"; };
  CryptoAlgorithmRSASSA_PKCS1_v1_5_Sign_Verify(){};
  CryptoAlgorithmIdentifier identifier() final {
    return CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5;
  };

  JSObject *sign(JSContext *cx, JS::HandleObject key, std::span<uint8_t> data) override;
  JS::Result<bool> verify(JSContext *cx, JS::HandleObject key, std::span<uint8_t> signature,
                          std::span<uint8_t> data) override;
  JSObject *toObject(JSContext *cx);
};

class CryptoAlgorithmRSASSA_PKCS1_v1_5_Import final : public CryptoAlgorithmImportKey {
public:
  // The hash member describes the hash algorithm to use.
  CryptoAlgorithmIdentifier hashIdentifier;

  const char *name() const noexcept override { return "RSASSA-PKCS1-v1_5"; };
  CryptoAlgorithmRSASSA_PKCS1_v1_5_Import(CryptoAlgorithmIdentifier hashIdentifier)
      : hashIdentifier{hashIdentifier} {};

  // https://w3c.github.io/webcrypto/#RsaHashedImportParams-dictionary
  // 20.7 RsaHashedImportParams dictionary
  static std::unique_ptr<CryptoAlgorithmRSASSA_PKCS1_v1_5_Import>
  fromParameters(JSContext *cx, JS::HandleObject parameters);

  CryptoAlgorithmIdentifier identifier() final {
    return CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5;
  };

  JSObject *importKey(JSContext *cx, CryptoKeyFormat format, JS::HandleValue, bool extractable,
                      CryptoKeyUsages usages) override;
  JSObject *importKey(JSContext *cx, CryptoKeyFormat format, KeyData, bool extractable,
                      CryptoKeyUsages usages) override;
  JSObject *toObject(JSContext *cx);
};

class CryptoAlgorithmHMAC_Import final : public CryptoAlgorithmImportKey {
public:
  // The hash member describes the hash algorithm to use.
  // Valid values are SHA_256, SHA_384, SHA_512.
  CryptoAlgorithmIdentifier hashIdentifier;
  // A Number representing the length in bits of the key.
  // If this is omitted the length of the key is equal to the length of the digest generated by the
  // hash algorithm defined in hashIdentifier.
  std::optional<size_t> length;

  const char *name() const noexcept override { return "HMAC"; };

  CryptoAlgorithmHMAC_Import(CryptoAlgorithmIdentifier hashIdentifier)
      : hashIdentifier{hashIdentifier} {};

  CryptoAlgorithmHMAC_Import(CryptoAlgorithmIdentifier hashIdentifier, size_t length)
      : hashIdentifier{hashIdentifier}, length{length} {};

  // https://w3c.github.io/webcrypto/#hmac-importparams
  // 29.3 HmacImportParams dictionary
  static std::unique_ptr<CryptoAlgorithmHMAC_Import> fromParameters(JSContext *cx,
                                                                    JS::HandleObject parameters);

  CryptoAlgorithmIdentifier identifier() final { return CryptoAlgorithmIdentifier::HMAC; };

  JSObject *importKey(JSContext *cx, CryptoKeyFormat format, JS::HandleValue, bool extractable,
                      CryptoKeyUsages usages) override;
  JSObject *importKey(JSContext *cx, CryptoKeyFormat format, KeyData, bool extractable,
                      CryptoKeyUsages usages) override;
  JSObject *toObject(JSContext *cx);
};

class CryptoAlgorithmDigest : public CryptoAlgorithm {
public:
  virtual JSObject *digest(JSContext *cx, std::span<uint8_t>) = 0;
  static std::unique_ptr<CryptoAlgorithmDigest> normalize(JSContext *cx, JS::HandleValue value);
};

class CryptoAlgorithmMD5 final : public CryptoAlgorithmDigest {
public:
  const char *name() const noexcept override { return "MD5"; };
  CryptoAlgorithmIdentifier identifier() override { return CryptoAlgorithmIdentifier::MD5; };
  JSObject *digest(JSContext *cx, std::span<uint8_t>) override;
};

class CryptoAlgorithmSHA1 final : public CryptoAlgorithmDigest {
public:
  const char *name() const noexcept override { return "SHA-1"; };
  CryptoAlgorithmIdentifier identifier() override { return CryptoAlgorithmIdentifier::SHA_1; };
  JSObject *digest(JSContext *cx, std::span<uint8_t>) override;
};

class CryptoAlgorithmSHA256 final : public CryptoAlgorithmDigest {
public:
  const char *name() const noexcept override { return "SHA-256"; };
  CryptoAlgorithmIdentifier identifier() override { return CryptoAlgorithmIdentifier::SHA_256; };
  JSObject *digest(JSContext *cx, std::span<uint8_t>) override;
};

class CryptoAlgorithmSHA384 final : public CryptoAlgorithmDigest {
public:
  const char *name() const noexcept override { return "SHA-384"; };
  CryptoAlgorithmIdentifier identifier() override { return CryptoAlgorithmIdentifier::SHA_384; };
  JSObject *digest(JSContext *cx, std::span<uint8_t>) override;
};

class CryptoAlgorithmSHA512 final : public CryptoAlgorithmDigest {
public:
  const char *name() const noexcept override { return "SHA-512"; };
  CryptoAlgorithmIdentifier identifier() override { return CryptoAlgorithmIdentifier::SHA_512; };
  JSObject *digest(JSContext *cx, std::span<uint8_t>) override;
};

} // namespace builtins
#endif