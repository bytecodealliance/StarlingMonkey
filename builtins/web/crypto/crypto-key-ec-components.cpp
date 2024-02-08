#include "crypto-key-ec-components.h"

CryptoKeyECComponents::CryptoKeyECComponents(std::string_view x, std::string_view y)
    : type(Type::Public), x(x), y(y) {}

std::unique_ptr<CryptoKeyECComponents> CryptoKeyECComponents::createPublic(std::string_view x,
                                                                           std::string_view y) {
  return std::make_unique<CryptoKeyECComponents>(x, y);
}

CryptoKeyECComponents::CryptoKeyECComponents(std::string_view x, std::string_view y,
                                             std::string_view d)
    : type(Type::Private), x(x), y(y), d(d) {}

std::unique_ptr<CryptoKeyECComponents>
CryptoKeyECComponents::createPrivate(std::string_view x, std::string_view y, std::string_view d) {
  return std::make_unique<CryptoKeyECComponents>(x, y, d);
}
