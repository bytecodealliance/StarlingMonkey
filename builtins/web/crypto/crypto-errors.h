#ifndef CRYPTO_ERRORS_H
#define CRYPTO_ERRORS_H

namespace CryptoErrors {
DEF_ERR(InvalidJwk, JSEXN_TYPEERR, "crypto.subtle.importkey: The JWK member '{0}' was not '{1}'", 2)
DEF_ERR(InvalidKeyFormat, JSEXN_SYNTAXERR, "crypto.subtle.importkey: Provided format parameter is "
                                          "not supported. Supported formats are: "
                                          "'spki', 'pkcs8', 'jwk', and 'raw'", 0)
DEF_ERR(InvalidHmacKeyUsage, JSEXN_SYNTAXERR, "HMAC keys only support 'sign' and 'verify' operations", 0)
};     // namespace FetchErrors

#endif // CRYPTO_ERRORS_H
