#include "extension-api.h"

#define NS_DEF(ns)                                                                                 \
  namespace ns {                                                                                   \
  extern bool install(api::Engine *engine);                                                        \
  }
#include "builtins.incl"
#undef NS_DEF

bool install_builtins(api::Engine *engine) {
#define NS_DEF(ns)                                                                                 \
  if (!ns::install(engine))                                                                        \
    return false;
#include "builtins.incl"
#undef NS_DEF

  return true;
}
