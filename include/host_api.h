#ifndef JS_RUNTIME_HOST_API_H
#define JS_RUNTIME_HOST_API_H

#include <cstdint>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "../crates/rust-url/rust-url.h"
#include "extension-api.h"
#include "js/TypeDecls.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/Utility.h"
#include "jsapi.h"
#pragma clang diagnostic pop

using std::optional;
using std::string_view;
using std::tuple;
using std::unique_ptr;
using std::vector;

namespace host_api {
class HttpOutgoingRequest;
class HttpOutgoingResponse;
class HttpIncomingRequest;

/// A type to signal that a result produces no value.
struct Void final {};

/// The type of errors returned from the host.
using APIError = uint8_t;

bool error_is_generic(APIError e);
bool error_is_invalid_argument(APIError e);
bool error_is_optional_none(APIError e);
bool error_is_bad_handle(APIError e);

/// Generate an error in the JSContext.
void handle_api_error(JSContext *cx, APIError err, int line, const char *func);

/// Wrap up a call to handle_api_error with the current line and function.
#define HANDLE_ERROR(cx, err) ::host_api::handle_api_error(cx, err, __LINE__, __func__)

template <typename T> class Result final {
  /// A private wrapper to distinguish `fastly_compute_at_edge_types_error_t` in the private
  /// variant.
  struct Error {
    APIError value;

    explicit Error(APIError value) : value{value} {}
  };

  std::variant<T, Error> result;

public:
  Result() = default;

  /// Explicitly construct an error.
  static Result err(APIError err) {
    Result res;
    res.emplace_err(err);
    return res;
  }

  /// Explicitly construct a successful result.
  template <typename... Args> static Result ok(Args &&...args) {
    Result res;
    res.emplace(std::forward<Args>(args)...);
    return res;
  }

  /// Construct an error in-place.
  APIError &emplace_err(APIError err) & { return this->result.template emplace<Error>(err).value; }

  /// Construct a value of T in-place.
  template <typename... Args> T &emplace(Args &&...args) {
    return this->result.template emplace<T>(std::forward<Args>(args)...);
  }

  /// True when the result contains an error.
  bool is_err() const { return std::holds_alternative<Error>(this->result); }

  /// Return a pointer to the error value of this result, if the call failed.
  const APIError *to_err() const {
    return reinterpret_cast<const APIError *>(std::get_if<Error>(&this->result));
  }

  /// Assume the call was successful, and return a reference to the result.
  T &unwrap() {
    MOZ_ASSERT(!is_err());
    return std::get<T>(this->result);
  }
};

/// A string allocated by the host interface. Holds ownership of the data.
struct HostString final {
  JS::UniqueChars ptr;
  size_t len = 0;

  HostString() = default;
  HostString(std::nullptr_t) : HostString() {}
  HostString(const char *c_str);
  HostString(JS::UniqueChars ptr, size_t len) : ptr{std::move(ptr)}, len{len} {}

  HostString(const HostString &other) = delete;
  HostString &operator=(const HostString &other) = delete;

  HostString(HostString &&other) : ptr{std::move(other.ptr)}, len{other.len} {}
  HostString &operator=(HostString &&other) {
    this->ptr.reset(other.ptr.release());
    this->len = other.len;
    return *this;
  }

  using iterator = char *;
  using const_iterator = const char *;

  size_t size() const { return this->len; }

  iterator begin() { return this->ptr.get(); }
  iterator end() { return this->begin() + this->len; }

  static HostString from_copy(string_view str) {
    JS::UniqueChars ptr(
        static_cast<char *>(std::memcpy(malloc(str.size()), str.data(), str.size())));
    return HostString(std::move(ptr), str.size());
  }

  const_iterator begin() const { return this->ptr.get(); }
  const_iterator end() const { return this->begin() + this->len; }

  /// Conversion to a bool, testing for an empty pointer.
  operator bool() const { return this->ptr != nullptr; }

  /// Comparison against nullptr
  bool operator==(std::nullptr_t) { return this->ptr == nullptr; }

  /// Comparison against nullptr
  bool operator!=(std::nullptr_t) { return this->ptr != nullptr; }

  /// Conversion to a `string_view`.
  operator string_view() const { return string_view(this->ptr.get(), this->len); }

  /// Conversion to a `jsurl::SpecString`.
  operator jsurl::SpecString() {
    return jsurl::SpecString(reinterpret_cast<uint8_t *>(this->ptr.release()), this->len,
                             this->len);
  }

  /// Conversion to a `jsurl::SpecString`.
  operator const jsurl::SpecString() const {
    return jsurl::SpecString(reinterpret_cast<uint8_t *>(this->ptr.get()), this->len, this->len);
  }
};

struct HostBytes final {
  unique_ptr<uint8_t[]> ptr;
  size_t len;

  HostBytes() = default;
  HostBytes(std::nullptr_t) : HostBytes() {}
  HostBytes(unique_ptr<uint8_t[]> ptr, size_t len) : ptr{std::move(ptr)}, len{len} {}

  HostBytes(const HostBytes &other) = delete;
  HostBytes &operator=(const HostBytes &other) = delete;

  HostBytes(HostBytes &&other) : ptr{std::move(other.ptr)}, len{other.len} {}
  HostBytes &operator=(HostBytes &&other) {
    this->ptr.reset(other.ptr.release());
    this->len = other.len;
    return *this;
  }

  /// Allocate a zeroed HostBytes with the given number of bytes.
  static HostBytes with_capacity(size_t len) {
    HostBytes ret;
    ret.ptr = std::make_unique<uint8_t[]>(len);
    ret.len = len;
    return ret;
  }

  using iterator = uint8_t *;
  using const_iterator = const uint8_t *;

  size_t size() const { return this->len; }

  iterator begin() { return this->ptr.get(); }
  iterator end() { return this->begin() + this->len; }

  const_iterator begin() const { return this->ptr.get(); }
  const_iterator end() const { return this->begin() + this->len; }

  /// Conversion to a bool, testing for an empty pointer.
  operator bool() const { return this->ptr != nullptr; }

  /// Comparison against nullptr
  bool operator==(std::nullptr_t) { return this->ptr == nullptr; }

  /// Comparison against nullptr
  bool operator!=(std::nullptr_t) { return this->ptr != nullptr; }

  /// Conversion to a `std::span<uint8_t>`.
  operator std::span<uint8_t>() const { return std::span<uint8_t>(this->ptr.get(), this->len); }
};

/// An opaque class to be used in classes representing host resources.
///
/// Some host resources have different requirements for their client-side representation
/// depending on the host API. To accommodate this, we introduce an opaque class to use for
/// all of them, which the API-specific implementation can define as needed.
class HandleState;

class Resource {
protected:
  std::unique_ptr<HandleState> handle_state_ = nullptr;

public:
  virtual ~Resource();

  typedef uint8_t HandleNS;
  static HandleNS next_handle_ns(const char *ns_name);

  /// Returns true if this resource handle has been initialized and is still valid.
  bool valid() const;
};

class Pollable : public Resource {
protected:
  Pollable() = default;

public:
  ~Pollable() override = default;

  virtual Result<PollableHandle> subscribe() = 0;
  virtual void unsubscribe() = 0;
};

void block_on_pollable_handle(PollableHandle handle);

class HttpIncomingBody final : public Pollable {
public:
  HttpIncomingBody() = delete;
  explicit HttpIncomingBody(std::unique_ptr<HandleState> handle);

  class ReadResult final {
  public:
    bool done = false;
    HostBytes bytes;
    ReadResult() = default;
    ReadResult(const bool done, unique_ptr<uint8_t[]> ptr, size_t len)
        : done{done}, bytes(std::move(ptr), len) {}
  };
  /// Read a chunk of up to `chunk_size` bytes from this handle.
  ///
  /// Might return an empty string if no data is available.
  Result<ReadResult> read(uint32_t chunk_size);

  /// Close this handle, and reset internal state to invalid.
  Result<Void> close();

  Result<PollableHandle> subscribe() override;
  void unsubscribe() override;
};

/// A convenience wrapper for the host calls involving outgoing http bodies.
class HttpOutgoingBody final : public Pollable {
public:
  HttpOutgoingBody() = delete;
  explicit HttpOutgoingBody(std::unique_ptr<HandleState> handle);

  /// Get the body's stream's current capacity.
  Result<uint64_t> capacity();

  /// Write a chunk to this handle.
  ///
  /// Doesn't necessarily write the entire chunk, and doesn't take ownership of `bytes`.
  ///
  /// @return the number of bytes written.
  Result<uint32_t> write(const uint8_t *bytes, size_t len);

  /// Writes the given number of bytes from the given buffer to the given handle.
  ///
  /// The host doesn't necessarily write all bytes in any particular call to
  /// `write`, so to ensure all bytes are written, we call it in a loop.
  /// TODO: turn into an async task that writes chunks of the passed buffer until done.
  Result<Void> write_all(const uint8_t *bytes, size_t len);

  /// Append an HttpIncomingBody to this one.
  Result<Void> append(api::Engine *engine, HttpIncomingBody *other,
                      api::TaskCompletionCallback callback, HandleObject callback_receiver);

  /// Close this handle, and reset internal state to invalid.
  Result<Void> close();

  Result<PollableHandle> subscribe() override;
  void unsubscribe() override;
};

class HttpBodyPipe {
  HttpIncomingBody *incoming;
  HttpOutgoingBody *outgoing;

public:
  HttpBodyPipe(HttpIncomingBody *incoming, HttpOutgoingBody *outgoing)
      : incoming(incoming), outgoing(outgoing) {}

  Result<uint8_t> pump();

  bool done();
};

class HttpIncomingResponse;
class HttpHeaders;

class FutureHttpIncomingResponse final : public Pollable {
public:
  FutureHttpIncomingResponse() = delete;
  explicit FutureHttpIncomingResponse(std::unique_ptr<HandleState> handle);

  /// Returns the response if it is ready, or `nullopt` if it is not.
  Result<optional<HttpIncomingResponse *>> maybe_response();

  Result<PollableHandle> subscribe() override;
  void unsubscribe() override;
};

class HttpHeadersReadOnly : public Resource {
  friend HttpIncomingResponse;
  friend HttpIncomingRequest;
  friend HttpOutgoingResponse;
  friend HttpOutgoingRequest;
  friend HttpHeaders;

protected:
  // It's never valid to create an HttpHeadersReadOnly without a handle,
  // but a subclass can create a handle and then assign it.
  explicit HttpHeadersReadOnly();

public:
  explicit HttpHeadersReadOnly(std::unique_ptr<HandleState> handle);
  HttpHeadersReadOnly(const HttpHeadersReadOnly &headers) = delete;

  HttpHeaders *clone();

  virtual bool is_writable() { return false; };
  virtual HttpHeaders *as_writable() {
    MOZ_ASSERT_UNREACHABLE();
    return nullptr;
  };

  Result<vector<tuple<HostString, HostString>>> entries() const;
  Result<vector<HostString>> names() const;
  Result<optional<vector<HostString>>> get(string_view name) const;
  Result<bool> has(string_view name) const;
};

class HttpHeaders final : public HttpHeadersReadOnly {
  friend HttpIncomingResponse;
  friend HttpIncomingRequest;
  friend HttpOutgoingResponse;
  friend HttpOutgoingRequest;

  HttpHeaders(std::unique_ptr<HandleState> handle);

public:
  HttpHeaders();
  HttpHeaders(const HttpHeadersReadOnly &headers);

  static Result<HttpHeaders *> FromEntries(vector<tuple<HostString, HostString>> &entries);

  bool is_writable() override { return true; };
  HttpHeaders *as_writable() override { return this; };

  Result<Void> set(string_view name, string_view value);
  Result<Void> append(string_view name, string_view value);
  Result<Void> remove(string_view name);

  static std::vector<string_view> get_forbidden_request_headers();
  static std::vector<string_view> get_forbidden_response_headers();
};

class HttpRequestResponseBase : public Resource {
protected:
  HttpHeadersReadOnly *headers_ = nullptr;
  std::string *_url = nullptr;

public:
  ~HttpRequestResponseBase() override = default;

  virtual Result<HttpHeadersReadOnly *> headers() = 0;
  virtual string_view url();

  virtual bool is_incoming() = 0;
  bool is_outgoing() { return !is_incoming(); }

  virtual bool is_request() = 0;
  bool is_response() { return !is_request(); }
};

class HttpIncomingBodyOwner {
protected:
  HttpIncomingBody *body_ = nullptr;

public:
  virtual ~HttpIncomingBodyOwner() = default;

  virtual Result<HttpIncomingBody *> body() = 0;
  bool has_body() const { return body_ != nullptr; }
};

class HttpOutgoingBodyOwner {
protected:
  HttpOutgoingBody *body_ = nullptr;

public:
  virtual ~HttpOutgoingBodyOwner() = default;

  virtual Result<HttpOutgoingBody *> body() = 0;
  bool has_body() { return body_ != nullptr; }
};

class HttpRequest : public HttpRequestResponseBase {
protected:
  std::string method_ = std::string();

public:
  [[nodiscard]] virtual Result<string_view> method() = 0;
};

class HttpIncomingRequest final : public HttpRequest, public HttpIncomingBodyOwner {
public:
  using RequestHandler = bool (*)(HttpIncomingRequest *request);

  HttpIncomingRequest() = delete;
  explicit HttpIncomingRequest(std::unique_ptr<HandleState> handle);

  bool is_incoming() override { return true; }
  bool is_request() override { return true; }

  [[nodiscard]] Result<string_view> method() override;
  Result<HttpHeadersReadOnly *> headers() override;
  Result<HttpIncomingBody *> body() override;

  static void set_handler(RequestHandler handler);
};

class HttpOutgoingRequest final : public HttpRequest, public HttpOutgoingBodyOwner {
  HttpOutgoingRequest(std::unique_ptr<HandleState> handle);

public:
  HttpOutgoingRequest() = delete;

  static HttpOutgoingRequest *make(string_view method_str, optional<HostString> url_str,
                                   std::unique_ptr<HttpHeadersReadOnly> headers);

  bool is_incoming() override { return false; }
  bool is_request() override { return true; }

  [[nodiscard]] Result<string_view> method() override;
  Result<HttpHeadersReadOnly *> headers() override;
  Result<HttpOutgoingBody *> body() override;

  Result<FutureHttpIncomingResponse *> send();
};

class HttpResponse : public HttpRequestResponseBase {
protected:
  static constexpr uint16_t UNSET_STATUS = UINT16_MAX;
  uint16_t status_ = UNSET_STATUS;

public:
  [[nodiscard]] virtual Result<uint16_t> status() = 0;
};

class HttpIncomingResponse final : public HttpResponse, public HttpIncomingBodyOwner {
public:
  HttpIncomingResponse() = delete;
  explicit HttpIncomingResponse(std::unique_ptr<HandleState> handle);

  bool is_incoming() override { return true; }
  bool is_request() override { return false; }

  Result<HttpHeadersReadOnly *> headers() override;
  Result<HttpIncomingBody *> body() override;
  [[nodiscard]] Result<uint16_t> status() override;
};

class HttpOutgoingResponse final : public HttpResponse, public HttpOutgoingBodyOwner {
  HttpOutgoingResponse(std::unique_ptr<HandleState> handle);

public:
  HttpOutgoingResponse() = delete;

  static HttpOutgoingResponse *make(const uint16_t status, unique_ptr<HttpHeaders> headers);

  bool is_incoming() override { return false; }
  bool is_request() override { return false; }

  Result<HttpHeadersReadOnly *> headers() override;
  Result<HttpOutgoingBody *> body() override;
  [[nodiscard]] Result<uint16_t> status() override;

  Result<Void> send();
};

class Random final {
public:
  static Result<HostBytes> get_bytes(size_t num_bytes);

  static Result<uint32_t> get_u32();
};

class MonotonicClock final {
public:
  MonotonicClock() = delete;

  static uint64_t now();
  static uint64_t resolution();

  static PollableHandle subscribe(uint64_t when, bool absolute);
  static void unsubscribe(PollableHandle handle_id);
};

} // namespace host_api

#endif
