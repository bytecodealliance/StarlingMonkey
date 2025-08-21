#ifndef WEB_CRYPTO_OPENSSL_RAII_H_
#define WEB_CRYPTO_OPENSSL_RAII_H_

#include <memory>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>

namespace builtins::web::crypto {

// RAII deleters for OpenSSL resources
namespace detail {
  struct BignumDeleter {
    void operator()(BIGNUM *bn) const {
      if (bn) { BN_free(bn); }
    }
  };

  struct EvpPkeyDeleter {
    void operator()(EVP_PKEY *pkey) const {
      if (pkey) { EVP_PKEY_free(pkey); }
    }
  };

  struct EvpPkeyCtxDeleter {
    void operator()(EVP_PKEY_CTX *ctx) const {
      if (ctx) { EVP_PKEY_CTX_free(ctx); }
    }
  };

  struct ParamBldDeleter {
    void operator()(OSSL_PARAM_BLD *bld) const {
      if (bld) { OSSL_PARAM_BLD_free(bld); }
    }
  };

  struct ParamDeleter {
    void operator()(OSSL_PARAM *params) const {
      if (params) { OSSL_PARAM_free(params); }
    }
  };

  struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX *ctx) const {
      if (ctx) { EVP_MD_CTX_free(ctx); }
    }
  };

  struct EcdsaSigDeleter {
    void operator()(ECDSA_SIG *sig) const {
      if (sig) { ECDSA_SIG_free(sig); }
    }
  };

  struct EcGroupDeleter {
    void operator()(EC_GROUP *group) const {
      if (group) { EC_GROUP_free(group); }
    }
  };

  struct EcPointDeleter {
    void operator()(EC_POINT *point) const {
      if (point) { EC_POINT_free(point); }
    }
  };
}

using BignumPtr = std::unique_ptr<BIGNUM, detail::BignumDeleter>;
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, detail::EvpPkeyDeleter>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, detail::EvpPkeyCtxDeleter>;
using ParamBldPtr = std::unique_ptr<OSSL_PARAM_BLD, detail::ParamBldDeleter>;
using ParamPtr = std::unique_ptr<OSSL_PARAM, detail::ParamDeleter>;
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, detail::EvpMdCtxDeleter>;
using EcdsaSigPtr = std::unique_ptr<ECDSA_SIG, detail::EcdsaSigDeleter>;
using EcGroupPtr = std::unique_ptr<EC_GROUP, detail::EcGroupDeleter>;
using EcPointPtr = std::unique_ptr<EC_POINT, detail::EcPointDeleter>;

// Transfer ownership helpers
template<typename T, typename D>
T* release_ptr(std::unique_ptr<T, D>& ptr) {
  return ptr.release();
}

template<typename T, typename D>
T* get_ptr(const std::unique_ptr<T, D>& ptr) {
  return ptr.get();
}

}


#endif // WEB_CRYPTO_OPENSSL_RAII_H_
