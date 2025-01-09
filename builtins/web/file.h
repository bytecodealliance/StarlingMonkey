#ifndef BUILTINS_WEB_FILE_H
#define BUILTINS_WEB_FILE_H

#include "blob.h"
#include "builtin.h"

namespace builtins {
namespace web {
namespace file {
class File : public BuiltinImpl<File> {
  static bool name_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool lastModified_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr int ParentSlots = blob::Blob::Slots::Count;
  enum Slots { Name = ParentSlots, LastModified, Count };

  static constexpr const char *class_name = "File";
  static constexpr unsigned ctor_length = 2;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSObject *create(JSContext *cx, HandleValue fileBits, HandleValue fileName, HandleValue opts);
  static bool init(JSContext *cx, HandleObject self, HandleValue fileBits, HandleValue fileName, HandleValue opts);
  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

bool install(api::Engine *engine);

} // namespace file
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_FILE_H
