#ifndef BUILTINS_WEB_FILE_H
#define BUILTINS_WEB_FILE_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace file {
class File: public BuiltinImpl<File> {
  static bool name_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool lastModified_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  enum Slots { Count };

  static constexpr const char *class_name = "File";
  static constexpr unsigned ctor_length = 2;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool is_instance(const JSObject *obj);
  static bool is_instance(const Value val);

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

bool install(api::Engine *engine);

} // namespace file
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_FILE_H
