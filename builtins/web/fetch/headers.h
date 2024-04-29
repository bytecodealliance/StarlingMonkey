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

  /// Headers instances can be in one of three modes:
  /// - `HostOnly`: Headers are stored in the host only.
  /// - `CachedInContent`: Host holds canonical headers, content a cached copy.
  /// - `ContentOnly`: Headers are stored in a Map held by the `Entries` slot.
  ///
  /// For Headers instances created in-content, the mode is determined by the `HeadersInit`
  /// argument:
  /// - If `HeadersInit` is a `Headers` instance, the mode is inherited from that instance,
  ///   as is the underlying data.
  /// - If `HeadersInit` is empty or a sequence of header name/value pairs, the mode is
  ///   `ContentOnly`.
  ///
  /// The mode of Headers instances created via the `headers` accessor on `Request` and `Response`
  /// instances is determined by how those instances themselves were created:
  /// - If a `Request` or `Response` instance represents an incoming request or response, the mode
  ///   will initially be `HostOnly`.
  /// - If a `Request` or `Response` instance represents an outgoing request or response, the mode
  ///   of the `Headers` instance depends on the `HeadersInit` argument passed to the `Request` or
  ///   `Response` constructor (see above).
  ///
  /// A `Headers` instance can transition from `HostOnly` to `CachedInContent` or `ContentOnly`
  /// mode:
  /// Iterating over headers (as keys, values, or entries) would be extremely slow if we retrieved
  /// all of them from the host for each iteration step.
  /// Instead, when iterating over the headers of a `HostOnly` mode `Headers` instance, the instance
  /// is transitioned to `CachedInContent` mode, and the entries are stored in a Map in the
  /// `Entries` slot.
  ///
  /// If a header is added, deleted, or replaced on an instance in `CachedInContent` mode, the
  /// instance transitions to `ContentOnly` mode, and the underlying resource handle is discarded.
  enum class Mode {
    HostOnly, // Headers are stored in the host.
    CachedInContent, // Host holds canonical headers, content a cached copy.
    ContentOnly, // Headers are stored in a Map held by the `Entries` slot.
    Uninitialized, // Headers have not been initialized.
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
  static bool set_if_undefined(JSContext *cx, JS::HandleObject self, const char *name, const char *value);

  /// Appends a value for a header name.
  //
  /// Validates and normalizes the name and value.
  static bool append_header_value(JSContext *cx, JS::HandleObject self, JS::HandleValue name,
                                  JS::HandleValue value, const char *fun_name);

  static Mode mode(JSObject* self) {
    MOZ_ASSERT(Headers::is_instance(self));
    Value modeVal = JS::GetReservedSlot(self, static_cast<size_t>(Slots::Mode));
    if (modeVal.isUndefined()) {
      return Mode::Uninitialized;
    }
    return static_cast<Mode>(modeVal.toInt32());
  }

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);

  static JSObject *create(JSContext *cx, HandleValue init_headers);
  static JSObject *create(JSContext *cx, HandleObject self, HandleValue init_headers);
  static JSObject *create(JSContext *cx, host_api::HttpHeadersReadOnly *handle);

  /// Returns a Map object containing the headers.
  ///
  /// Depending on the `Mode` the instance is in, this can be a cache or the canonical store for
  /// the headers.
  static JSObject* get_entries(JSContext *cx, HandleObject self);

  /**
   * Returns a cloned handle representing the contents of this Headers object.
   *
   * The main purposes for this function are use in sending outgoing requests/responses and
   * in the constructor of request/response objects when a HeadersInit object is passed.
   *
   * The handle is guaranteed to be uniquely owned by the caller.
   */
  static unique_ptr<host_api::HttpHeaders> handle_clone(JSContext*, HandleObject self);
};

} // namespace fetch
} // namespace web
} // namespace builtins

#endif
