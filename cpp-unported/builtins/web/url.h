
#include "builtin.h"
#include "rust-url/rust-url.h"

namespace builtins {

class URLSearchParamsIterator : public BuiltinNoConstructor<URLSearchParamsIterator> {
public:
  static constexpr const char *class_name = "URLSearchParamsIterator";

  enum Slots { Params, Type, Index, Count };

  static bool next(JSContext *cx, unsigned argc, JS::Value *vp);

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 0;

  static JSObject *create(JSContext *cx, JS::HandleObject params, uint8_t type);
  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
};

class URLSearchParams : public BuiltinNoConstructor<URLSearchParams> {
  static bool append(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool has(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool getAll(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool sort(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool forEach(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool delete_(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool toString(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool entries(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool keys(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool values(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "URLSearchParams";

  enum Slots { Url, Params, Count };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static jsurl::SpecSlice serialize(JSContext *cx, JS::HandleObject self);
  static jsurl::JSUrlSearchParams *get_params(JSObject *self);

  static JSObject *create(JSContext *cx, JS::HandleObject self, jsurl::JSUrl *url);
  static JSObject *create(JSContext *cx, JS::HandleObject self,
                          JS::HandleValue params_val = JS::UndefinedHandleValue);

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
};

class URL : public BuiltinImpl<URL> {
  static bool hash_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool host_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool hostname_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool href_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool password_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool pathname_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool port_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool protocol_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool search_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool username_set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool hash_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool host_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool hostname_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool href_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool password_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool pathname_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool port_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool protocol_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool search_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool username_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool origin_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool searchParams_get(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool toString(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool toJSON(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "URL";

  enum Slots { Url, Params, Count };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static jsurl::SpecString origin(JSContext *cx, JS::HandleObject self);
  static bool origin(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool hash(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool host(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool hostname(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool href(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool password(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool pathname(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool port(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool protocol(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool search(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);
  static bool username(JSContext *cx, JS::HandleObject self, JS::MutableHandleValue rval);

  static JSObject *create(JSContext *cx, JS::HandleObject self, jsurl::SpecString url_str,
                          const jsurl::JSUrl *base = nullptr);

  static JSObject *create(JSContext *cx, JS::HandleObject self, JS::HandleValue url_val,
                          const jsurl::JSUrl *base = nullptr);

  static JSObject *create(JSContext *cx, JS::HandleObject self, JS::HandleValue url_val,
                          JS::HandleObject base_obj);

  static JSObject *create(JSContext *cx, JS::HandleObject self, JS::HandleValue url_val,
                          JS::HandleValue base_val);

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);
};

} // namespace builtins
