#ifndef JS_COMPUTE_RUNTIME_BUILTIN_DOM_EXCEPTION_H
#define JS_COMPUTE_RUNTIME_BUILTIN_DOM_EXCEPTION_H

#include "builtin.h"

namespace builtins {

class DOMException : public BuiltinImpl<DOMException> {
private:
public:
  static constexpr const char *class_name = "DOMException";
  enum Slots { Name, Message, Code, Count };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool code_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool message_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool name_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static const unsigned ctor_length = 0;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
  static JSObject *create(JSContext *cx, std::string_view message, std::string_view name);
  static void raise(JSContext *cx, std::string_view message, std::string_view name);
};

} // namespace builtins

#endif
