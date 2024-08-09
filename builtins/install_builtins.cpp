#include "extension-api.h"

#define NS_DEF(ns)                              \
  namespace ns {                                \
  extern bool install(api::Engine *engine);     \
  }
#define RS_DEF(install_fn)                      \
extern "C" bool install_fn(api::Engine *engine);
#include "builtins.incl"
#undef RS_DEF
#undef NS_DEF


bool install_builtins(api::Engine *engine) {
#define NS_DEF(ns)                              \
  if (!ns::install(engine))                     \
    return false;
#define RS_DEF(install_fn)                      \
  if (!install_fn(engine))                      \
    return false;
#include "builtins.incl"
#undef RS_DEF
#undef NS_DEF

  return true;
}
