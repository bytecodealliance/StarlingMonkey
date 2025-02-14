#ifndef BUILTINS_WEB_FORM_DATA_ENCODER_
#define BUILTINS_WEB_FORM_DATA_ENCODER_

#include "builtin.h"

namespace builtins {
namespace web {
namespace form_data {

class OutOfMemory {};
class MultipartFormDataImpl;

class MultipartFormData : public FinalizableBuiltinImpl<MultipartFormData> {
  static MultipartFormDataImpl *as_impl(JSObject *self);

  static bool read(JSContext *cx, HandleObject self, std::span<uint8_t> buf,
                   size_t start, size_t *read, bool *done);

public:
  static constexpr const char *class_name = "MultipartFormData";
  static constexpr unsigned ctor_length = 0;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  enum Slots { Form, Inner, Count };

  static JSObject *form_data(JSObject *self);
  static std::string boundary(JSObject *self);

  static mozilla::Result<size_t, OutOfMemory> query_length(JSContext *cx, HandleObject self);
  static JSObject *encode_stream(JSContext *cx, HandleObject self);
  static JSObject *create(JSContext *cx, HandleObject form_data);

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
  static void finalize(JS::GCContext *gcx, JSObject *self);
};

} // namespace form_data_encoder
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_FORM_DATA_ENCODER_
