#ifndef BUILTINS_WEB_BLOB_H
#define BUILTINS_WEB_BLOB_H

#include "builtin.h"
#include "extension-api.h"
#include "js/AllocPolicy.h"
#include "js/TypeDecls.h"

namespace builtins {
namespace web {
namespace blob {

class BlobReader {
  std::span<const uint8_t> data_;
  std::size_t position_;

public:
  explicit BlobReader(std::span<const uint8_t> data)
    : data_(data), position_(0) {}

  std::size_t remaining() const {
    return data_.size() - position_;
  }

  std::span<const uint8_t> read(std::size_t size) {
    size = std::min(size, remaining());
    auto result = data_.subspan(position_, size);

    position_ += size;
    return result;
  }
};

class Blob : public FinalizableBuiltinImpl<Blob> {
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

  static constexpr unsigned ctor_length = 1;
  enum Slots { Data, Type, Endings, Readers, Count };
  enum Endings { Transparent, Native };

  using ByteBuffer = std::vector<uint8_t>;
  using ReadersMap = js::HashMap<JSObject*, BlobReader, js::PointerHasher<JSObject*>, js::MallocAllocPolicy>;

  static ReadersMap *readers(JSObject *self);
  static ByteBuffer *blob(JSObject *self);
  static size_t blob_size(JSObject *self);
  static JSString *type(JSObject *self);
  static bool append_value(JSContext *cx, HandleObject self, HandleValue val);
  static bool init_blob_parts(JSContext *cx, HandleObject self, HandleValue iterable);
  static bool init_options(JSContext *cx, HandleObject self, HandleValue opts);

  static bool stream_cancel(JSContext *cx, JS::CallArgs args, JS::HandleObject stream,
                            JS::HandleObject owner, JS::HandleValue reason);
  static bool stream_pull(JSContext *cx, JS::CallArgs args, JS::HandleObject source,
                          JS::HandleObject body_owner, JS::HandleObject controller);

  static JSObject *data_to_owned_array_buffer(JSContext *cx, HandleObject self);
  static JSObject *data_to_owned_array_buffer(JSContext* cx, HandleObject self, size_t offset, size_t size, size_t* bytes_read);
  static JSObject *create(JSContext *cx, std::unique_ptr<ByteBuffer> data, JSString *type);

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
  static void finalize(JS::GCContext *gcx, JSObject *obj);
};

bool install(api::Engine *engine);

} // namespace blob
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_URL_H
