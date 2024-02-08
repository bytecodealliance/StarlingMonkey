#ifndef BUILTINS_HEADERS_H
#define BUILTINS_HEADERS_H

#include "builtin.h"

namespace builtins {

class Headers final : public BuiltinImpl<Headers> {
  static bool get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool has(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool append(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool delete_(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool forEach(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool entries(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool keys(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool values(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  enum class Mode : int32_t {
    Standalone,
    ProxyToRequest,
    ProxyToResponse,
  };

  static constexpr const char *class_name = "Headers";

  enum class Slots {
    BackingMap,
    Handle,
    Mode,
    HasLazyValues,
    Count,
  };

  static bool delazify(JSContext *cx, JS::HandleObject headers);

  /**
   * Adds the given header name/value to `self`'s list of headers iff `self`
   * doesn't already contain a header with that name.
   *
   * Assumes that both the name and value are valid and normalized.
   * TODO(performance): fully skip normalization.
   * https://github.com/fastly/js-compute-runtime/issues/221
   */
  static bool maybe_add(JSContext *cx, JS::HandleObject self, const char *name, const char *value);

  // Appends a non-normalized value for a non-normalized header name to both
  // the JS side Map and, in non-standalone mode, the host.
  //
  // Verifies and normalizes the name and value.
  static bool append_header_value(JSContext *cx, JS::HandleObject self, JS::HandleValue name,
                                  JS::HandleValue value, const char *fun_name);

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, JS::HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp);

  static JSObject *create(JSContext *cx, JS::HandleObject headers, enum Mode mode,
                          JS::HandleObject owner, JS::HandleObject init_headers);
  static JSObject *create(JSContext *cx, JS::HandleObject headers, enum Mode mode,
                          JS::HandleObject owner, JS::HandleValue initv);
  static JSObject *create(JSContext *cx, JS::HandleObject self, enum Mode mode,
                          JS::HandleObject owner);
};

} // namespace builtins

#endif
