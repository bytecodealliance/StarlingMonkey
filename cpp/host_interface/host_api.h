#ifndef JS_RUNTIME_HOST_API_H
#define JS_RUNTIME_HOST_API_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "bindings.h"
#include <engine.h>
#include "js/TypeDecls.h"
#include "rust-url/rust-url.h"

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
  APIError &emplace_err(APIError err) & {
    return this->result.template emplace<Error>(err).value;
  }

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
  size_t len;

  HostString() = default;
  HostString(std::nullptr_t) : HostString() {}
  HostString(const char *c_str);
  HostString(JS::UniqueChars ptr, size_t len) : ptr{std::move(ptr)}, len{len} {}
  HostString(bindings_list_u8_t list) : ptr{reinterpret_cast<char*>(list.ptr)}, len{list.len} {}
  HostString(bindings_string_t list) : ptr{reinterpret_cast<char*>(list.ptr)}, len{list.len} {}

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

  const_iterator begin() const { return this->ptr.get(); }
  const_iterator end() const { return this->begin() + this->len; }

  /// Conversion to a bool, testing for an empty pointer.
  operator bool() const { return this->ptr != nullptr; }

  /// Comparison against nullptr
  bool operator==(std::nullptr_t) { return this->ptr == nullptr; }

  /// Comparison against nullptr
  bool operator!=(std::nullptr_t) { return this->ptr != nullptr; }

  /// Conversion to a `bindings_string_t`.
  operator bindings_string_t() const {
    return bindings_string_t { reinterpret_cast<uint8_t*>(this->ptr.get()), this->len };
  }

  /// Conversion to a `bindings_list_u8_t`.
  operator bindings_list_u8_t() const {
    return bindings_list_u8_t { reinterpret_cast<uint8_t *>(this->ptr.get()), this->len };
  }

  /// Conversion to a `string_view`.
  operator string_view() const { return string_view(this->ptr.get(), this->len); }

  /// Conversion to a `jsurl::SpecString`.
  operator jsurl::SpecString() {
    return jsurl::SpecString(reinterpret_cast<uint8_t *>(this->ptr.release()), this->len, this->len);
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
  operator std::span<uint8_t>() const {
    return std::span<uint8_t>(this->ptr.get(), this->len);
  }
};

/// Common methods for async handles.
class AsyncHandle {
public:
#ifdef CAE
  using Handle = uint32_t;
  static constexpr Handle invalid = UINT32_MAX - 1;
#else
  using Handle = bindings_own_pollable_t;
  static constexpr Handle invalid = bindings_own_pollable_t { -1 };
#endif // CAE

  Handle handle = invalid;

  explicit AsyncHandle() : handle{invalid} {}
  explicit AsyncHandle(const Handle handle) : handle{handle} {}

  [[nodiscard]] bool valid() const { return this->handle.__handle != invalid.__handle; }

#ifdef CAE
  // WASI pollables don't have a generic way to query readiness, and this is only used for debug asserts.
  /// Check to see if this handle is ready.
  Result<bool> is_ready() const;
#endif

  static Result<optional<uint32_t>> select(vector<AsyncHandle> &handles, int64_t timeout_ns);
};

/// A convenience wrapper for the host calls involving incoming http bodies.
class HttpIncomingBody final {
#ifdef CAE
public:
  using Handle = uint32_t;
  static constexpr Handle invalid = UINT32_MAX - 1;
#else
private:
  /// Ensures that this body's stream is initialized and returns a borrowed handle to it.
  Result<bindings_borrow_input_stream_t> ensure_stream();

public:
  using Handle = bindings_own_incoming_body_t;
  static constexpr Handle invalid = Handle { -1 };

  using StreamHandle = bindings_own_input_stream_t;
  static constexpr StreamHandle invalid_stream = StreamHandle { -1 };
  StreamHandle stream = invalid_stream;
#endif // CAE

  /// The handle to use when making host calls.
  Handle handle = invalid;

  HttpIncomingBody() = delete;
  explicit HttpIncomingBody(Handle handle) : handle{handle} {}

  /// Returns true when this body handle is valid.
  bool valid() const { return this->handle.__handle != invalid.__handle; }

  /// Read a chunk of up to `chunk_size` bytes from this handle.
  ///
  /// If the `blocking` flag is set, will block until at least one byte has been read.
  /// Otherwise, might return an empty string.
  // TODO: check why this doesn't return HostBytes.
  Result<tuple<HostString, bool>> read(uint32_t chunk_size, bool blocking);

  /// Close this handle, and reset internal state to invalid.
  Result<Void> close();

  AsyncHandle async_handle();
};

/// A convenience wrapper for the host calls involving outgoing http bodies.
class HttpOutgoingBody final {
  /// Ensures that this body's stream is initialized.
  /// Returns the stream handle and its capacity, which might be 0.
  Result<tuple<bindings_borrow_output_stream_t, uint64_t>>
  ensure_stream();
  /// Ensures that this body's stream is initialized.
  /// Returns the stream handle and its capacity, blocking until the capacity is > 0.
  Result<tuple<bindings_borrow_output_stream_t, uint64_t>>
  ensure_stream_with_capacity();

  bool _closed = false;

public:
#ifdef CAE
  using Handle = uint32_t;
  static constexpr Handle invalid = UINT32_MAX - 1;
#else
  using Handle = bindings_own_outgoing_body_t;
  static constexpr Handle invalid = Handle{-1};
  using StreamHandle = bindings_own_output_stream_t;
  static constexpr StreamHandle invalid_stream = StreamHandle{-1};
#endif // CAE


  /// The handle to use when making host calls.
  Handle handle = invalid;
#ifndef CAE
  StreamHandle stream = invalid_stream;
#endif

  HttpOutgoingBody() = delete;
  explicit HttpOutgoingBody(Handle handle) : handle{handle} {}

  /// Returns true when this body handle is valid.
  bool valid() const { return this->handle.__handle != invalid.__handle; }

  /// Get the body's stream's current capacity.
  Result<uint64_t> capacity();

  /// Write a chunk to this handle.
  Result<uint32_t> write(const uint8_t *bytes, size_t len);

  /// Writes the given number of bytes from the given buffer to the given handle.
  ///
  /// The host doesn't necessarily write all bytes in any particular call to
  /// `write`, so to ensure all bytes are written, we call it in a loop.
  /// TODO: turn into an async task that writes chunks of the passed buffer until done.
  Result<Void> write_all(const uint8_t *bytes, size_t len);

  /// Append an HttpIncomingBody to this one.
  Result<Void> append(core::Engine* engine, HttpIncomingBody* other);

  /// Close this handle, and reset internal state to invalid.
  Result<Void> close();
  bool closed();

  AsyncHandle async_handle();
};

class HttpBodyPipe {
private:
  HttpIncomingBody* incoming;
  HttpOutgoingBody* outgoing;

public:
  HttpBodyPipe(HttpIncomingBody *incoming, HttpOutgoingBody * outgoing)
      : incoming(incoming), outgoing(outgoing) {}

  Result<uint8_t> pump();
  bool ready();
  bool done();
};

class HttpIncomingResponse;
class FutureHttpIncomingResponse final {
public:
#ifdef CAE
  using Handle = uint32_t;
  static constexpr Handle invalid = UINT32_MAX - 1;
#else
  using Handle = bindings_own_future_incoming_response_t;
  static constexpr Handle invalid = Handle { -1 };
  AsyncHandle pollable;
#endif // CAE

  Handle handle = invalid;

  FutureHttpIncomingResponse() = delete;
  explicit FutureHttpIncomingResponse(const Handle handle) : handle(handle) { }

  /// Poll for the response to this request.
  Result<optional<HttpIncomingResponse*>> poll();

  /// Block until the response is ready.
  Result<HttpIncomingResponse*> wait();

  /// Fetch the AsyncHandle for this pending request.
  AsyncHandle async_handle();
};

class HttpHeaders final {
  using Handle = bindings_own_fields_t;
  static constexpr Handle invalid = Handle{-1};

private:
  Handle handle = invalid;

public:
  HttpHeaders();
  HttpHeaders(Handle handle) : handle(handle) {}
  HttpHeaders(const vector<tuple<HostString, vector<HostString>>>& entries);
  HttpHeaders(const HttpHeaders &headers);

  bool valid() const { return this->handle.__handle != invalid.__handle; }

  bindings_borrow_fields_t borrow() const {
    return bindings_borrow_fields_t{handle.__handle};
  }

  Result<vector<tuple<HostString, HostString>>> entries() const;
  Result<vector<HostString>> names() const;

  Result<optional<vector<HostString>>> get(string_view name) const;
  Result<Void> set(string_view name, string_view value);
  Result<Void> append(string_view name, string_view value);
  Result<Void> remove(string_view name);
};

class HttpRequestResponseBase {
protected:
  HttpHeaders* headers_handle = nullptr;
  std::string* _url = nullptr;
  virtual void ensure_url() {};

 public:
  virtual HttpHeaders *headers() = 0;

  optional<string_view> url();

  virtual bool is_incoming() = 0;
  virtual bool is_request() = 0;
  virtual bool valid() = 0;
};

class HttpIncomingBodyOwner {
protected:
  HttpIncomingBody* body_handle = nullptr;

public:
  virtual Result<HttpIncomingBody *> body() = 0;
  bool has_body() { return body_handle != nullptr; }
};

class HttpOutgoingBodyOwner {
protected:
  HttpOutgoingBody* body_handle = nullptr;

public:
  virtual Result<HttpOutgoingBody *> body() = 0;
  bool has_body() { return body_handle != nullptr; }
};

class HttpRequest : public HttpRequestResponseBase {};

class HttpIncomingRequest : public HttpRequest,
                            public HttpIncomingBodyOwner {
  using Handle = bindings_own_incoming_request_t;
  static constexpr Handle invalid = Handle{-1};

private:
  Handle handle = invalid;

protected:
  virtual void ensure_url() override;

public:
  HttpIncomingRequest() = delete;
  explicit HttpIncomingRequest(Handle handle) : handle(handle) {}

  string_view method() const;

  bool is_incoming() override { return true; }
  bool is_request() override { return true; }
  bool valid() override { return handle.__handle != invalid.__handle; }
  HttpHeaders *headers() override;
  virtual Result<HttpIncomingBody*> body() override;
};

class HttpOutgoingRequest : public HttpRequest,
                            public HttpOutgoingBodyOwner {
private:
  using Handle = bindings_own_outgoing_request_t;
  static constexpr Handle invalid = Handle{-1};

  Handle handle = invalid;

public:
  HttpOutgoingRequest() = delete;
  HttpOutgoingRequest(string_view method, optional<HostString> url, HttpHeaders *headers);

  bool is_incoming() override { return false; }
  bool is_request() override { return true; }
  bool valid() override { return handle.__handle != invalid.__handle; }
  HttpHeaders *headers() override;
  virtual Result<HttpOutgoingBody *> body() override;

  Result<FutureHttpIncomingResponse*> send();
};

class HttpResponse : public HttpRequestResponseBase {};

class HttpIncomingResponse final : public HttpResponse,
                                   public HttpIncomingBodyOwner {
private:
  using Handle = bindings_own_incoming_response_t;
  static constexpr Handle invalid = Handle{-1};

  Handle handle = invalid;

public:
  uint16_t status() const;

  HttpIncomingResponse() = delete;
  explicit HttpIncomingResponse(Handle handle) : handle(handle) {}

  bool is_incoming() override { return true; }
  bool is_request() override { return false; }
  bool valid() override { return handle.__handle != invalid.__handle; }
  HttpHeaders *headers() override;
  virtual Result<HttpIncomingBody *> body() override;
};

class HttpOutgoingResponse final : public HttpResponse,
                                   public HttpOutgoingBodyOwner {
private:
  using Handle = bindings_own_outgoing_response_t;
  static constexpr Handle invalid = Handle{-1};

  Handle handle = invalid;

public:
  const uint16_t status;

  using ResponseOutparam = bindings_own_response_outparam_t;

  HttpOutgoingResponse() = delete;
  HttpOutgoingResponse(uint16_t status, HttpHeaders *headers);

  bool is_incoming() override { return false; }
  bool is_request() override { return false; }
  bool valid() override { return handle.__handle != invalid.__handle; }
  HttpHeaders *headers() override;
  virtual Result<HttpOutgoingBody *> body() override;

  Result<Void> send(ResponseOutparam* out_param);
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

  static int32_t subscribe(uint64_t when, bool absolute);
  static void unsubscribe(int32_t handle_id);
};

} // namespace host_api

#endif
