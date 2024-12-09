#ifndef BUILTINS_WEB_FILE_H
#define BUILTINS_WEB_FILE_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace file {

class File: public BuiltinImpl<File> {
  static bool arrayBuffer(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool bytes(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool slice(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool stream(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool text(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool size_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool type_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool name_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool lastModified_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool init_last_modified(JSContext *cx, HandleObject self, HandleValue initv);

public:
  enum Slots { Blob, Name, LastModified, Count };

  static constexpr const char *class_name = "File";
  static constexpr unsigned ctor_length = 2;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSObject *blob(JSObject *self);

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

bool install(api::Engine *engine);

} // namespace file
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_FILE_H
