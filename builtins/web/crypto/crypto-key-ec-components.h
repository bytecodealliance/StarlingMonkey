#ifndef JS_COMPUTE_RUNTIME_CRYPTO_KEY_EC_COMPONENTS_H
#define JS_COMPUTE_RUNTIME_CRYPTO_KEY_EC_COMPONENTS_H
#include <string>
#include <vector>

class CryptoKeyECComponents final {
public:
  enum class Type : uint8_t { Public, Private };
  const Type type;

  // Private and public keys.
  const std::string x;
  const std::string y;

  // Only private keys.
  const std::string d;
  static std::unique_ptr<CryptoKeyECComponents> createPublic(std::string_view x,
                                                             std::string_view y);

  static std::unique_ptr<CryptoKeyECComponents>
  createPrivate(std::string_view x, std::string_view y, std::string_view d);

  CryptoKeyECComponents(std::string_view x, std::string_view y);

  CryptoKeyECComponents(std::string_view x, std::string_view y, std::string_view d);
};
#endif