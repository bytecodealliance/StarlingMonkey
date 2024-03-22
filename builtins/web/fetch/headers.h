#ifndef BUILTINS_HEADERS_H
#define BUILTINS_HEADERS_H

#include "builtin.h"
#include "host_api.h"

namespace builtins {
namespace web {
namespace fetch {

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
  static constexpr const char *class_name = "Headers";

  /// Usually, headers are stored on the host only, including when they're created in-content.
  /// However, iterating over headers (as keys, values, or entries) would be extremely slow if
  /// we retrieved all of them from the host for each iteration step.
  /// So, when we start iterating, we retrieve them once and store them in a Map as a cache.
  /// If, while iterating, a header is added, deleted, or replaced, we start treating the Map as
  /// the canonical store for headers, and discard the underlying resource handle entirely.
  enum class Mode {
    HostOnly, // Headers are stored in the host.
    CachedInContent, // Host holds canonical headers, content a cached copy.
    ContentOnly, // Headers are stored in a Map held by the `Entries` slot.
  };

  enum class Slots {
    Handle,
    Entries, // Map holding headers if they are available in-content.
    Mode,
    Count,
  };

  /**
   * Adds the given header name/value to `self`'s list of headers iff `self`
   * doesn't already contain a header with that name.
   */
  static bool maybe_add(JSContext *cx, JS::HandleObject self, const char *name, const char *value);

  /// Appends a value for a header name.
  //
  /// Validates and normalizes the name and value.
  static bool append_header_value(JSContext *cx, JS::HandleObject self, JS::HandleValue name,
                                  JS::HandleValue value, const char *fun_name);

  static Mode mode(JSObject* self) const {
    MOZ_ASSERT(Headers::is_instance(self));
    return static_cast<Mode>(JS::GetReservedSlot(self, static_cast<size_t>(Slots::Mode)).toInt32());
  }

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);

  static JSObject *create(JSContext *cx, HandleObject self, host_api::HttpHeaders *handle,
                          HandleValue init_headers);
  static JSObject *create(JSContext *cx, HandleObject self, host_api::HttpHeadersReadOnly *handle);

  /// Returns a Map object containing the headers.
  ///
  /// Depending on the `Mode` the instance is in, this can be a cache or the canonical store for
  /// the headers.
  static JSObject* get_entries(JSContext *cx, HandleObject self);
};

} // namespace fetch
} // namespace web
} // namespace builtins

#endif
