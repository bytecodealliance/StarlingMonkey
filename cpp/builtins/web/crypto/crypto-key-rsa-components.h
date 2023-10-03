#ifndef JS_COMPUTE_RUNTIME_CRYPTO_KEY_RSA_COMPONENTS_H
#define JS_COMPUTE_RUNTIME_CRYPTO_KEY_RSA_COMPONENTS_H
#include <string>
#include <vector>

class CryptoKeyRSAComponents final {
public:
  class PrimeInfo {
  public:
    std::string primeFactor;
    std::string factorCRTExponent;
    std::string factorCRTCoefficient;
    PrimeInfo(std::string_view primeFactor, std::string_view factorCRTExponent,
              std::string_view factorCRTCoefficient)
        : primeFactor{primeFactor}, factorCRTExponent{factorCRTExponent},
          factorCRTCoefficient{factorCRTCoefficient} {};
    PrimeInfo(std::string_view primeFactor, std::string_view factorCRTExponent)
        : primeFactor{primeFactor}, factorCRTExponent{factorCRTExponent} {};
    PrimeInfo(std::string_view primeFactor) : primeFactor{primeFactor} {};
  };
  enum class Type : uint8_t { Public, Private };
  const Type type;

  // Private and public keys.
  const std::string modulus;
  const std::string exponent;

  // Only private keys.
  const std::string privateExponent;
  const bool hasAdditionalPrivateKeyParameters = false;
  const std::optional<PrimeInfo> firstPrimeInfo;
  const std::optional<PrimeInfo> secondPrimeInfo;
  // When three or more primes have been used, the number of array elements
  // is be the number of primes used minus two.
  const std::vector<PrimeInfo> otherPrimeInfos;
  static std::unique_ptr<CryptoKeyRSAComponents> createPublic(std::string_view modulus,
                                                              std::string_view exponent);

  static std::unique_ptr<CryptoKeyRSAComponents> createPrivate(std::string_view modulus,
                                                               std::string_view exponent,
                                                               std::string_view privateExponent);

  static std::unique_ptr<CryptoKeyRSAComponents> createPrivateWithAdditionalData(
      std::string_view modulus, std::string_view exponent, std::string_view privateExponent,
      std::optional<PrimeInfo> firstPrimeInfo, std::optional<PrimeInfo> secondPrimeInfo,
      std::vector<PrimeInfo> otherPrimeInfos);

  CryptoKeyRSAComponents(std::string_view modulus, std::string_view exponent);

  CryptoKeyRSAComponents(std::string_view modulus, std::string_view exponent,
                         std::string_view privateExponent);

  CryptoKeyRSAComponents(std::string_view modulus, std::string_view exponent,
                         std::string_view privateExponent, std::optional<PrimeInfo> firstPrimeInfo,
                         std::optional<PrimeInfo> secondPrimeInfo,
                         std::vector<PrimeInfo> otherPrimeInfos);
};
#endif