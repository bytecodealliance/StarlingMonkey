#ifndef BUILTINS_WEB_BLOB_H
#define BUILTINS_WEB_BLOB_H

#include "builtin.h"
#include "extension-api.h"
#include "js/AllocPolicy.h"
#include "js/Vector.h"



namespace builtins::web::blob {

class Blob : public BuiltinImpl<Blob, FinalizableClassPolicy> {
  static bool arrayBuffer(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool bytes(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool slice(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool stream(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool text(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool size_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool type_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "Blob";

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr unsigned ctor_length = 0;
  enum Slots : uint8_t { Data, Type, Endings, Readers, Count };
  enum LineEndings : uint8_t { Transparent, Native };

  using ByteBuffer = js::Vector<uint8_t, 0, js::SystemAllocPolicy>;

  static bool arrayBuffer(JSContext *cx, HandleObject self, MutableHandleValue rval);
  static bool bytes(JSContext *cx, HandleObject self, MutableHandleValue rval);
  static bool stream(JSContext *cx, HandleObject self, MutableHandleValue rval);
  static bool text(JSContext *cx, HandleObject self, MutableHandleValue rval);
  static bool slice(JSContext *cx, HandleObject self, const CallArgs &args, MutableHandleValue rval);

  static ByteBuffer *blob(JSObject *self);
  static size_t blob_size(JSObject *self);
  static JSString *type(JSObject *self);
  static LineEndings line_endings(JSObject *self);

  static bool append_value(JSContext *cx, HandleObject self, HandleValue val);
  static bool init_blob_parts(JSContext *cx, HandleObject self, HandleValue iterable);
  static bool init_options(JSContext *cx, HandleObject self, HandleValue opts);
  static bool init(JSContext *cx, HandleObject self, HandleValue blobParts, HandleValue opts);

  static JSObject *data_to_owned_array_buffer(JSContext *cx, HandleObject self);
  static bool read_blob_slice(JSContext *cx, HandleObject self, std::span<uint8_t>,
                              size_t start, size_t *read, bool *done);

  static JSObject *create(JSContext *cx, UniqueChars data, size_t data_len, HandleString type);

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
  static void finalize(JS::GCContext *gcx, JSObject *self);
};

bool install(api::Engine *engine);

} // namespace builtins::web::blob



#endif // BUILTINS_WEB_URL_H
