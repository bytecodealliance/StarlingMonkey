#ifndef WEB_BUILTINS_ABORT_CONTROLLER_H
#define WEB_BUILTINS_ABORT_CONTROLLER_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace abort {

class AbortController : public BuiltinImpl<AbortController> {
  static bool signal_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool abort(JSContext *cx, unsigned argc, JS::Value *vp);

  static JSObject *create(JSContext *cx);

public:
  static constexpr const char *class_name = "AbortController";
  static constexpr unsigned ctor_length = 0;

  enum Slots { Signal = 0, Count };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

} // namespace abort
} // namespace web
} // namespace builtins



#endif // WEB_BUILTINS_ABORT_CONTROLLER_H
