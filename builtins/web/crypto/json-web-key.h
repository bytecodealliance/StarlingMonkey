#ifndef BUILTINS_WEB_CRYPTO_JSON_WEB_KEY_H
#define BUILTINS_WEB_CRYPTO_JSON_WEB_KEY_H
#include "jsapi.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>



namespace builtins::web::crypto {

// https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.7
// 6.3.2.7.  "oth" (Other Primes Info) Parameter
class RsaOtherPrimesInfo {
public:
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.7.1
  // 6.3.2.7.1.  "r" (Prime Factor)
  // The "r" (prime factor) parameter within an "oth" array member
  // represents the value of a subsequent prime factor.  It is represented
  // as a Base64urlUInt-encoded value.
  std::string r;

  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.7.2
  // 6.3.2.7.2.  "d" (Factor CRT Exponent)
  // The "d" (factor CRT exponent) parameter within an "oth" array member
  // represents the CRT exponent of the corresponding prime factor.  It is
  // represented as a Base64urlUInt-encoded value.
  std::string d;

  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.7.3
  // 6.3.2.7.3.  "t" (Factor CRT Coefficient)
  // The "t" (factor CRT coefficient) parameter within an "oth" array
  // member represents the CRT coefficient of the corresponding prime
  // factor.  It is represented as a Base64urlUInt-encoded value.
  std::string t;
  RsaOtherPrimesInfo(std::string r, std::string d, std::string t) : r{std::move(r)}, d{std::move(d)}, t{std::move(t)} {}
};

// https://datatracker.ietf.org/doc/html/rfc7517#section-4
class JsonWebKey {
public:
  // https://datatracker.ietf.org/doc/html/rfc7517#section-4.1
  // 4.1.  "kty" (Key Type) Parameter
  std::string kty;
  // https://datatracker.ietf.org/doc/html/rfc7517#section-4.2
  // 4.2.  "use" (Public Key Use) Parameter
  std::optional<std::string> use;

  // https://datatracker.ietf.org/doc/html/rfc7517#section-4.3
  // 4.3.  "key_ops" (Key Operations) Parameter
  std::vector<std::string> key_ops;
  // https://datatracker.ietf.org/doc/html/rfc7517#section-4.4
  // 4.4.  "alg" (Algorithm) Parameter
  std::optional<std::string> alg;

  // https://w3c.github.io/webcrypto/#iana-section-jwk
  std::optional<bool> ext;

  // 6.2.  Parameters for Elliptic Curve Keys
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.2.1.1
  // 6.2.1.1.  "crv" (Curve) Parameter
  // The "crv" (curve) parameter identifies the cryptographic curve used
  // with the key.
  std::optional<std::string> crv;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.2.1.2
  // 6.2.1.2.  "x" (X Coordinate) Parameter
  // The "x" (x coordinate) parameter contains the x coordinate for the
  // Elliptic Curve point.  It is represented as the base64url encoding of
  // the octet string representation of the coordinate, as defined in
  // Section 2.3.5 of SEC1 [SEC1]
  std::optional<std::string> x;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.2.1.3
  // 6.2.1.3.  "y" (Y Coordinate) Parameter
  // The "y" (y coordinate) parameter contains the y coordinate for the
  // Elliptic Curve point.  It is represented as the base64url encoding of
  // the octet string representation of the coordinate, as defined in
  // Section 2.3.5 of SEC1 [SEC1].
  std::optional<std::string> y;

  // 6.2.2.  Parameters for Elliptic Curve Private Keys
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.2.2.1
  // The "d" (ECC private key) parameter contains the Elliptic Curve
  // private key value.  It is represented as the base64url encoding of
  // the octet string representation of the private key value, as defined
  // in Section 2.3.7 of SEC1 [SEC1].  The length of this octet string
  // MUST be ceiling(log-base-2(n)/8) octets (where n is the order of the
  // curve).
  //
  // 6.3.2.  Parameters for RSA Private Keys
  // The "d" (private exponent) parameter contains the private exponent
  // value for the RSA private key.  It is represented as a Base64urlUInt-
  // encoded value.
  std::optional<std::string> d;

  // 6.3.  Parameters for RSA Keys
  // 6.3.1.  Parameters for RSA Public Keys
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.1.1
  // 6.3.1.1.  "n" (Modulus) Parameter
  // The "n" (modulus) parameter contains the modulus value for the RSA
  //  public key.  It is represented as a Base64urlUInt-encoded value.
  std::optional<std::string> n;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.1.2
  // 6.3.1.2.  "e" (Exponent) Parameter
  // The "e" (exponent) parameter contains the exponent value for the RSA
  // public key.  It is represented as a Base64urlUInt-encoded value.
  std::optional<std::string> e;

  // 6.3.2.  Parameters for RSA Private Keys
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.2
  // The "p" (first prime factor) parameter contains the first prime
  // factor.  It is represented as a Base64urlUInt-encoded value.
  std::optional<std::string> p;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.3
  // The "q" (second prime factor) parameter contains the second prime
  // factor.  It is represented as a Base64urlUInt-encoded value.
  std::optional<std::string> q;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.4
  // The "dp" (first factor CRT exponent) parameter contains the Chinese
  // Remainder Theorem (CRT) exponent of the first factor.  It is
  // represented as a Base64urlUInt-encoded value.
  std::optional<std::string> dp;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.5
  // The "dq" (second factor CRT exponent) parameter contains the CRT
  // exponent of the second factor.  It is represented as a Base64urlUInt-
  // encoded value.
  std::optional<std::string> dq;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.6
  // The "qi" (first CRT coefficient) parameter contains the CRT
  // coefficient of the second factor.  It is represented as a
  // Base64urlUInt-encoded value.
  std::optional<std::string> qi;
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.2.7
  // The "oth" (other primes info) parameter contains an array of
  // information about any third and subsequent primes, should they exist.
  // When only two primes have been used (the normal case), this parameter
  // MUST be omitted.  When three or more primes have been used, the
  // number of array elements MUST be the number of primes used minus two.
  std::vector<RsaOtherPrimesInfo> oth;

  // 6.4.  Parameters for Symmetric Keys
  // https://datatracker.ietf.org/doc/html/rfc7518#section-6.4.1
  // 6.4.1.  "k" (Key Value) Parameter
  // The "k" (key value) parameter contains the value of the symmetric (or
  // other single-valued) key.  It is represented as the base64url
  // encoding of the octet sequence containing the key value.
  std::optional<std::string> k;

  JsonWebKey(std::string kty, std::vector<std::string> key_ops, std::optional<bool> ext,
             std::optional<std::string> n, std::optional<std::string> e)
      : kty{std::move(kty)}, key_ops{std::move(key_ops)}, ext{ext}, n{std::move(n)}, e{std::move(e)} {}

  JsonWebKey RSAPublicKey(std::string kty, std::vector<std::string> key_ops,
                          std::optional<bool> ext, std::optional<std::string> n,
                          std::optional<std::string> e) {
    return {std::move(kty), std::move(key_ops), ext, std::move(n), std::move(e)};
  }

  JsonWebKey(std::string kty, std::vector<std::string> key_ops, std::optional<bool> ext,
             std::optional<std::string> n, std::optional<std::string> e,
             std::optional<std::string> d)
      : kty{std::move(kty)}, key_ops{std::move(key_ops)}, ext{ext}, d{std::move(d)}, n{std::move(n)}, e{std::move(e)} {}

  JsonWebKey RSAPrivateKey(std::string kty, std::vector<std::string> key_ops,
                           std::optional<bool> ext, std::optional<std::string> n,
                           std::optional<std::string> e, std::optional<std::string> d) {
    return {std::move(kty), std::move(key_ops), ext, std::move(n), std::move(e), std::move(d)};
  }
  JsonWebKey(std::string kty, std::vector<std::string> key_ops, std::optional<bool> ext,
             std::optional<std::string> n, std::optional<std::string> e,
             std::optional<std::string> d, std::optional<std::string> p,
             std::optional<std::string> q, std::optional<std::string> dp,
             std::optional<std::string> dq, std::optional<std::string> qi)
      : kty{std::move(kty)}, key_ops{std::move(key_ops)}, ext{ext}, d{std::move(d)}, n{std::move(n)}, e{std::move(e)}, p{std::move(p)}, q{std::move(q)}, dp{std::move(dp)}, dq{std::move(dq)}, qi{std::move(qi)} {
  }

  JsonWebKey RSAPrivateKeyWithAdditionalPrimes(
      std::string kty, std::vector<std::string> key_ops, std::optional<bool> ext,
      std::optional<std::string> n, std::optional<std::string> e, std::optional<std::string> d,
      std::optional<std::string> p, std::optional<std::string> q, std::optional<std::string> dp,
      std::optional<std::string> dq, std::optional<std::string> qi) {
    return {std::move(kty), std::move(key_ops), ext, std::move(n), std::move(e), std::move(d), std::move(p), std::move(q), std::move(dp), std::move(dq), std::move(qi)};
  }

  JsonWebKey(std::string kty, std::optional<std::string> use, std::vector<std::string> key_ops,
             std::optional<std::string> alg, std::optional<bool> ext,
             std::optional<std::string> crv, std::optional<std::string> x,
             std::optional<std::string> y, std::optional<std::string> n,
             std::optional<std::string> e, std::optional<std::string> d,
             std::optional<std::string> p, std::optional<std::string> q,
             std::optional<std::string> dp, std::optional<std::string> dq,
             std::optional<std::string> qi, std::vector<RsaOtherPrimesInfo> oth,
             std::optional<std::string> k)
      : kty{std::move(kty)}, use{std::move(use)}, key_ops{std::move(key_ops)}, alg{std::move(alg)}, ext{ext}, crv{std::move(crv)}, x{std::move(x)}, y{std::move(y)}, d{std::move(d)}, n{std::move(n)},
        e{std::move(e)}, p{std::move(p)}, q{std::move(q)}, dp{std::move(dp)}, dq{std::move(dq)}, qi{std::move(qi)}, oth{std::move(oth)}, k{std::move(k)} {}

  static std::unique_ptr<JsonWebKey> parse(JSContext *cx, JS::HandleValue value,
                                           std::string_view required_kty_value);
};
} // namespace builtins::web::crypto


#endif
