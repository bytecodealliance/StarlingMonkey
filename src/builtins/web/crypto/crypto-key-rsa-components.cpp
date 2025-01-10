#include "crypto-key-rsa-components.h"

CryptoKeyRSAComponents::CryptoKeyRSAComponents(std::string_view modulus, std::string_view exponent)
    : type(Type::Public), modulus(modulus), exponent(exponent) {}

std::unique_ptr<CryptoKeyRSAComponents>
CryptoKeyRSAComponents::createPublic(std::string_view modulus, std::string_view exponent) {
  return std::make_unique<CryptoKeyRSAComponents>(modulus, exponent);
}

CryptoKeyRSAComponents::CryptoKeyRSAComponents(std::string_view modulus, std::string_view exponent,
                                               std::string_view privateExponent)
    : type(Type::Private), modulus(modulus), exponent(exponent), privateExponent(privateExponent),
      hasAdditionalPrivateKeyParameters(false) {}

std::unique_ptr<CryptoKeyRSAComponents>
CryptoKeyRSAComponents::createPrivate(std::string_view modulus, std::string_view exponent,
                                      std::string_view privateExponent) {
  return std::make_unique<CryptoKeyRSAComponents>(modulus, exponent, privateExponent);
}

CryptoKeyRSAComponents::CryptoKeyRSAComponents(std::string_view modulus, std::string_view exponent,
                                               std::string_view privateExponent,
                                               std::optional<PrimeInfo> firstPrimeInfo,
                                               std::optional<PrimeInfo> secondPrimeInfo,
                                               std::vector<PrimeInfo> otherPrimeInfos)
    : type(Type::Private), modulus(modulus), exponent(exponent), privateExponent(privateExponent),
      hasAdditionalPrivateKeyParameters(true), firstPrimeInfo(firstPrimeInfo),
      secondPrimeInfo(secondPrimeInfo), otherPrimeInfos(otherPrimeInfos) {}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSAComponents::createPrivateWithAdditionalData(
    std::string_view modulus, std::string_view exponent, std::string_view privateExponent,
    std::optional<PrimeInfo> firstPrimeInfo, std::optional<PrimeInfo> secondPrimeInfo,
    std::vector<PrimeInfo> otherPrimeInfos) {
  return std::make_unique<CryptoKeyRSAComponents>(modulus, exponent, privateExponent,
                                                  firstPrimeInfo, secondPrimeInfo, otherPrimeInfos);
}
