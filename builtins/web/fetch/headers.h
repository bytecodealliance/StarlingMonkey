#ifndef BUILTINS_HEADERS_H
#define BUILTINS_HEADERS_H

#include "builtin.h"
#include "host_api.h"

namespace builtins {
namespace web {
namespace fetch {

class Headers final : public BuiltinImpl<Headers> {
  static bool append(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool delete_(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool entries(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool forEach(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool getSetCookie(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool has(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool keys(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool set(JSContext *cx, unsigned argc, JS::Value *vp);
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
    HostOnly,        // Headers are stored in the host.
    CachedInContent, // Host holds canonical headers, content a cached copy.
    ContentOnly,     // Headers are stored in a Map held by the `Entries` slot.
    Uninitialized,   // Headers have not been initialized.
  };

  // Headers internal data structure is a list of key-value pairs, ready to go on the wire as
  // owned host strings.
  using HeadersList = std::vector<std::tuple<host_api::HostString, host_api::HostString>>;
  // A sort list is maintained of ordered indicies of the the sorted lowercase keys of main headers
  // list, with each index of HeadersList always being present in this list once and only once.
  // All lookups are done as indices in this list, which then map to indices in HeadersList.
  // When this list is empty, that means the sort list is not valid and needs to be computed. For
  // example, it is cleared after an insertion. It is recomputed lazily for every lookup.
  using HeadersSortList = std::vector<size_t>;

  enum class Slots {
    Handle,
    HeadersList,
    HeadersSortList,
    Mode,
    Guard,
    Count,
  };

  enum class HeadersGuard {
    None,
    Request,
    Response,
    Immutable,
  };

  /// Adds the valid given header name/value to `self`'s list of headers iff `self`
  /// doesn't already contain a header with that name.
  static bool set_valid_if_undefined(JSContext *cx, JS::HandleObject self, string_view name,
                                     string_view value);

  /// Validates and normalizes the name and value.
  static host_api::HostString validate_header_name(JSContext *cx, HandleValue name_val, bool *err,
                                                   const char *fun_name);
  /// Appends a value for a header name.
  static bool append_valid_header(JSContext *cx, JS::HandleObject self,
                                  host_api::HostString valid_key, JS::HandleValue value,
                                  const char *fun_name);

  /// Lookup the given header key, returning the sorted header index.
  /// This index is guaranteed to be valid, so long as mutations are not made.
  static std::optional<size_t> lookup(JSContext *cx, JS::HandleObject self, string_view key);

  /// Get the header entry for a given index, ensuring that HeadersSortList is recomputed if
  /// necessary in the process.
  static std::tuple<host_api::HostString, host_api::HostString> *
  get_index(JSContext *cx, JS::HandleObject self, size_t idx);

  static Mode mode(JSObject *self) {
    MOZ_ASSERT(Headers::is_instance(self));
    Value modeVal = JS::GetReservedSlot(self, static_cast<size_t>(Slots::Mode));
    if (modeVal.isUndefined()) {
      return Mode::Uninitialized;
    }
    return static_cast<Mode>(modeVal.toInt32());
  }

  static HeadersGuard guard(JSObject *self) {
    MOZ_ASSERT(Headers::is_instance(self));
    Value modeVal = JS::GetReservedSlot(self, static_cast<size_t>(Slots::Guard));
    return static_cast<HeadersGuard>(modeVal.toInt32());
  }

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr unsigned ctor_length = 1;

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);

  static JSObject *create(JSContext *cx, HeadersGuard guard);
  static JSObject *create(JSContext *cx, HandleValue init_headers, HeadersGuard guard);
  static JSObject *create(JSContext *cx, host_api::HttpHeadersReadOnly *handle, HeadersGuard guard);

  static bool init_entries(JSContext *cx, HandleObject self, HandleValue init_headers);

  /// Returns the headers list of entries, constructing it if necessary.
  /// Depending on the `Mode` the instance is in, this can be a cache or the canonical store for
  /// the headers.
  static HeadersList *get_list(JSContext *cx, HandleObject self);

  /**
   * Returns a cloned handle representing the contents of this Headers object.
   *
   * The main purposes for this function are use in sending outgoing requests/responses and
   * in the constructor of request/response objects when a HeadersInit object is passed.
   *
   * The handle is guaranteed to be uniquely owned by the caller.
   */
  static unique_ptr<host_api::HttpHeaders> handle_clone(JSContext *, HandleObject self);
};

class HeadersIterator final : public BuiltinNoConstructor<HeadersIterator> {
  static bool next(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "Headers Iterator";

  enum Slots {
    Type,
    Cursor,
    Headers,
    Count,
  };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool init_class(JSContext *cx, HandleObject global);

  static JSObject *create(JSContext *cx, JS::HandleObject headers, uint8_t iter_type);
};

} // namespace fetch
} // namespace web
} // namespace builtins

#endif
