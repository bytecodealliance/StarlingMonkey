#include "host_api.h"
#include "bindings/bindings.h"

#include <set>
#include <algorithm>
#include <exports.h>
#ifdef DEBUG
#include <set>
#endif

using host_api::HostString;
using std::optional;
using std::string_view;
using std::tuple;
using std::unique_ptr;
using std::vector;

// The host interface makes the assumption regularly that uint32_t is sufficient space to store a
// pointer.
static_assert(sizeof(uint32_t) == sizeof(void *));

typedef wasi_http_0_2_0_types_own_incoming_request_t incoming_request_t;
typedef wasi_http_0_2_0_types_borrow_incoming_request_t borrow_incoming_request_t;
typedef wasi_http_0_2_0_types_own_incoming_response_t incoming_response_t;
typedef wasi_http_0_2_0_types_borrow_outgoing_request_t borrow_outgoing_request_t;
typedef wasi_http_0_2_0_types_borrow_outgoing_response_t borrow_outgoing_response_t;

typedef wasi_http_0_2_0_types_own_future_incoming_response_t future_incoming_response_t;
typedef wasi_http_0_2_0_types_borrow_future_incoming_response_t borrow_future_incoming_response_t;

typedef wasi_http_0_2_0_types_own_incoming_body_t incoming_body_t;
typedef wasi_http_0_2_0_types_own_outgoing_body_t outgoing_body_t;

using field_key = wasi_http_0_2_0_types_field_key_t;
using field_value = wasi_http_0_2_0_types_field_value_t;

typedef wasi_http_0_2_0_types_borrow_incoming_body_t borrow_incoming_body_t;
typedef wasi_http_0_2_0_types_borrow_outgoing_body_t borrow_outgoing_body_t;

typedef wasi_io_0_2_0_poll_own_pollable_t own_pollable_t;
typedef wasi_io_0_2_0_poll_borrow_pollable_t borrow_pollable_t;
typedef wasi_io_0_2_0_poll_list_borrow_pollable_t list_borrow_pollable_t;

typedef wasi_io_0_2_0_streams_own_input_stream_t own_input_stream_t;
typedef wasi_io_0_2_0_streams_borrow_input_stream_t borrow_input_stream_t;

typedef wasi_io_0_2_0_streams_own_output_stream_t own_output_stream_t;

/// The type of handles used by the host interface.
typedef int32_t Handle;
constexpr Handle UNINITIALIZED_HANDLE = -1;

class host_api::HandleState {
  Handle handle_ = UNINITIALIZED_HANDLE;

#ifdef DEBUG
  uint8_t handle_ns_;
  bool poisoned_ = false;

  static std::set<int32_t> used_handles;
#endif

  static int32_t to_namespaced(Handle handle, uint8_t handle_ns) {
    return (handle_ns << 24) | handle;
  }

public:
  HandleState() = delete;
  HandleState(Handle handle, uint8_t handle_ns) : handle_{handle} {
#ifdef DEBUG
    handle_ns_ = handle_ns;
    // DBG("Adding handle %d,%d\n", handle_ns_, handle);
    // TODO: remove this, and replace all this with better things
    MOZ_ASSERT(handle < 500000);
    auto ns_handle = to_namespaced(handle, handle_ns);
    MOZ_ASSERT(handle > UNINITIALIZED_HANDLE);
    MOZ_ASSERT(!used_handles.contains(ns_handle));
    used_handles.insert(ns_handle);
#endif
  }
  virtual ~HandleState() {
    // TODO: support dropping handles for all handle types. Probably using a template class.
    // DBG("Removing handle %d,%d\n", handle_ns_, handle_);
#ifdef DEBUG
    if (!poisoned_) {
      auto ns_handle = to_namespaced(handle_, handle_ns_);
      MOZ_ASSERT(used_handles.contains(ns_handle));
      used_handles.erase(ns_handle);
    }
#endif
  }

  void assert_valid() const {
#ifdef DEBUG
    MOZ_ASSERT(initialized(), "Handle not initialized");
    if (poisoned()) {
      fprintf(stderr, "Handle %d,%d is poisened", handle_ns_, handle_); fflush(stderr);
      MOZ_ASSERT(false);
    }
#endif
  }

  Handle get() const {
    // DBG("Getting handle %d,%d\n", handle_ns_, handle_);
    assert_valid();
    return handle_;
  }

  Handle take() {
    auto handle = get();
    // DBG("Consuming handle %d,%d\n", handle_ns_, handle);
#ifdef DEBUG
    auto ns_handle = to_namespaced(handle, handle_ns_);
    used_handles.erase(ns_handle);
    poisoned_ = true;
#endif
    return handle;
  }

  bool valid() const {
#ifdef DEBUG
    if (posoned_) return false;
#endif
    return handle_ != UNINITIALIZED_HANDLE;
  }
  bool initialized() const { return handle_ != UNINITIALIZED_HANDLE;}
#ifdef DEBUG
  bool poisoned() const { return poisoned_; }
#endif
};

#ifdef DEBUG
std::set<int32_t> host_api::HandleState::used_handles = std::set<int32_t>();
#endif

namespace {

// TODO: merge HandleOps into a WASIHandleState subclass of HandleState, and use that everywhere.
/// This is the type contract for using the Own and Borrow templates.
template <typename T> struct HandleOps {};

/// A convenience wrapper for constructing a borrow. As we only create borrows of things we already
/// own, this wrapper will never explicitly drop borrows.
template <typename T> class Borrow final {
  static constexpr const typename HandleOps<T>::borrow invalid{std::numeric_limits<int32_t>::max()};
  HandleOps<T>::borrow handle{Borrow::invalid};

public:
  Borrow() = default;

  // Construct a borrow from an owned handle.
  Borrow(HandleOps<T>::own handle) : handle{HandleOps<T>::borrow_owned(handle)} {}

  // Construct a borrow from a raw `Handle` value.
  Borrow(Handle handle) : Borrow{typename HandleOps<T>::own{handle}} {
    MOZ_ASSERT(valid());
  }

  // Convenience wrapper for constructing a borrow of a HandleState.
  Borrow(host_api::HandleState *state) : Borrow{typename HandleOps<T>::own{state->get()}} {}

  bool valid() const { return this->handle.__handle != Borrow::invalid.__handle; }

  operator bool() const { return this->valid(); }

  operator typename HandleOps<T>::borrow() const { return this->handle; }
};

template <> struct HandleOps<host_api::HttpHeaders> {
  using own = wasi_http_0_2_0_types_own_fields_t;
  using borrow = wasi_http_0_2_0_types_borrow_fields_t;

  static constexpr const auto borrow_owned = wasi_http_0_2_0_types_borrow_fields;
};

struct OutputStream {};

template <> struct HandleOps<OutputStream> {
  using own = wasi_io_0_2_0_streams_own_output_stream_t;
  using borrow = wasi_io_0_2_0_streams_borrow_output_stream_t;

  static constexpr const auto borrow_owned = wasi_io_0_2_0_streams_borrow_output_stream;
};

struct Pollable {};

template <> struct HandleOps<Pollable> {
  using own = wasi_io_0_2_0_poll_own_pollable_t;
  using borrow = wasi_io_0_2_0_poll_borrow_pollable_t;

  static constexpr const auto borrow_owned = wasi_io_0_2_0_poll_borrow_pollable;
};

} // namespace

size_t api::AsyncTask::select(std::vector<api::AsyncTask *> *tasks) {
  auto count = tasks->size();
  vector<Borrow<Pollable>> handles;
  for (const auto task : *tasks) {
    handles.emplace_back(task->id());
  }
  auto list = list_borrow_pollable_t{
      reinterpret_cast<HandleOps<Pollable>::borrow *>(handles.data()), count};
  wasi_io_0_2_0_poll_list_u32_t result{nullptr, 0};
  wasi_io_0_2_0_poll_poll(&list, &result);
  MOZ_ASSERT(result.len > 0);
  const auto ready_index = result.ptr[0];
  free(result.ptr);

  return ready_index;
}

namespace host_api {

HostString::HostString(const char *c_str) {
  len = strlen(c_str);
  ptr = JS::UniqueChars(static_cast<char *>(malloc(len + 1)));
  std::memcpy(ptr.get(), c_str, len);
  ptr[len] = '\0';
}

namespace {

template <typename T> HostString to_host_string(T str) {
  return {JS::UniqueChars(reinterpret_cast<char *>(str.ptr)), str.len};
}

auto bindings_string_to_host_string = to_host_string<bindings_string_t>;

template <typename T> T from_string_view(std::string_view str) {
  return T{
      .ptr = (uint8_t *)str.data(),
      .len = str.size(),
  };
}

auto string_view_to_world_string = from_string_view<bindings_string_t>;

HostString scheme_to_string(const wasi_http_0_2_0_types_scheme_t scheme) {
  if (scheme.tag == WASI_HTTP_0_2_0_TYPES_SCHEME_HTTP) {
    return {"http:"};
  }
  if (scheme.tag == WASI_HTTP_0_2_0_TYPES_SCHEME_HTTPS) {
    return {"https:"};
  }
  return to_host_string(scheme.val.other);
}

} // namespace

Result<HostBytes> Random::get_bytes(size_t num_bytes) {
  Result<HostBytes> res;

  wasi_random_0_2_0_random_list_u8_t list{};
  wasi_random_0_2_0_random_get_random_bytes(num_bytes, &list);
  auto ret = HostBytes{
      std::unique_ptr<uint8_t[]>{list.ptr},
      list.len,
  };
  res.emplace(std::move(ret));

  return res;
}

Result<uint32_t> Random::get_u32() {
  return Result<uint32_t>::ok(wasi_random_0_2_0_random_get_random_u64());
}

uint64_t MonotonicClock::now() { return wasi_clocks_0_2_0_monotonic_clock_now(); }

uint64_t MonotonicClock::resolution() { return wasi_clocks_0_2_0_monotonic_clock_resolution(); }

int32_t MonotonicClock::subscribe(const uint64_t when, const bool absolute) {
  if (absolute) {
    return wasi_clocks_0_2_0_monotonic_clock_subscribe_instant(when).__handle;
  } else {
    return wasi_clocks_0_2_0_monotonic_clock_subscribe_duration(when).__handle;
  }
}

void MonotonicClock::unsubscribe(const int32_t handle_id) {
  wasi_io_0_2_0_poll_pollable_drop_own(own_pollable_t{handle_id});
}

HttpHeaders::HttpHeaders(std::unique_ptr<HandleState> state) : HttpHeadersReadOnly(std::move(state)) {}

static Resource::HandleNS HEADERS_HANDLE_NS = Resource::next_handle_ns("HttpHeaders");
HttpHeaders::HttpHeaders() : HttpHeadersReadOnly(std::make_unique<HandleState>(wasi_http_0_2_0_types_constructor_fields().__handle, HEADERS_HANDLE_NS)) {
}

Result<HttpHeaders*> HttpHeaders::FromEntries(vector<tuple<HostString, HostString>>& entries) {
  std::vector<wasi_http_0_2_0_types_tuple2_field_key_field_value_t> pairs;
  pairs.reserve(entries.size());

  for (const auto &[name, value] : entries) {
    pairs.emplace_back(from_string_view<field_key>(name), from_string_view<field_value>(value));
  }

  wasi_http_0_2_0_types_list_tuple2_field_key_field_value_t tuples{pairs.data(), entries.size()};

  wasi_http_0_2_0_types_own_fields_t ret;
  wasi_http_0_2_0_types_header_error_t err;
  if (!wasi_http_0_2_0_types_static_fields_from_list(&tuples, &ret, &err)) {
    // TODO: handle `err`
    return Result<HttpHeaders*>::err(154);
  }

  return Result<HttpHeaders*>::ok(new HttpHeaders(std::make_unique<HandleState>(ret.__handle,
  HEADERS_HANDLE_NS)));
}

HttpHeaders::HttpHeaders(const HttpHeadersReadOnly &headers) : HttpHeadersReadOnly(nullptr) {
  Borrow<HttpHeaders> borrow(headers.handle_state_.get());
  auto handle = wasi_http_0_2_0_types_method_fields_clone(borrow);
  this->handle_state_ = std::unique_ptr<HandleState>(new HandleState(handle.__handle, HEADERS_HANDLE_NS));
}

HttpHeaders *HttpHeadersReadOnly::clone() {
  return new HttpHeaders(*this);
}

Result<vector<tuple<HostString, HostString>>> HttpHeadersReadOnly::entries() const {
  Result<vector<tuple<HostString, HostString>>> res;
  handle_state_->assert_valid();

  wasi_http_0_2_0_types_list_tuple2_field_key_field_value_t entries;
  Borrow<HttpHeaders> borrow(this->handle_state_.get());
  wasi_http_0_2_0_types_method_fields_entries(borrow, &entries);

  vector<tuple<HostString, HostString>> entries_vec;
  for (int i = 0; i < entries.len; i++) {
    auto key = entries.ptr[i].f0;
    auto value = entries.ptr[i].f1;
    entries_vec.emplace_back(to_host_string(key), to_host_string(value));
  }
  // Free the outer list, but not the entries themselves.
  free(entries.ptr);
  res.emplace(std::move(entries_vec));

  return res;
}

Result<vector<HostString>> HttpHeadersReadOnly::names() const {
  Result<vector<HostString>> res;
  handle_state_->assert_valid();

  wasi_http_0_2_0_types_list_tuple2_field_key_field_value_t entries;
  Borrow<HttpHeaders> borrow(this->handle_state_.get());
  wasi_http_0_2_0_types_method_fields_entries(borrow, &entries);

  vector<HostString> names;
  for (int i = 0; i < entries.len; i++) {
    names.emplace_back(bindings_string_to_host_string(entries.ptr[i].f0));
  }
  // Free the outer list, but not the entries themselves.
  free(entries.ptr);
  res.emplace(std::move(names));

  return res;
}

Result<optional<vector<HostString>>> HttpHeadersReadOnly::get(string_view name) const {
  Result<optional<vector<HostString>>> res;
  handle_state_->assert_valid();

  wasi_http_0_2_0_types_list_field_value_t values;
  auto hdr = string_view_to_world_string(name);
  Borrow<HttpHeaders> borrow(this->handle_state_.get());
  wasi_http_0_2_0_types_method_fields_get(borrow, &hdr, &values);

  if (values.len > 0) {
    std::vector<HostString> names;
    for (int i = 0; i < values.len; i++) {
      names.emplace_back(to_host_string<field_value>(values.ptr[i]));
    }
    // Free the outer list, but not the values themselves.
    free(values.ptr);
    res.emplace(std::move(names));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<bool> HttpHeadersReadOnly::has(string_view name) const {
  handle_state_->assert_valid();

  auto hdr = string_view_to_world_string(name);
  Borrow<HttpHeaders> borrow(this->handle_state_.get());
  return Result<bool>::ok(wasi_http_0_2_0_types_method_fields_has(borrow, &hdr));
}

Result<Void> HttpHeaders::set(string_view name, string_view value) {
  handle_state_->assert_valid();
  auto hdr = from_string_view<field_key>(name);
  auto val = from_string_view<field_value>(value);
  wasi_http_0_2_0_types_list_field_value_t host_values{&val, 1};
  Borrow<HttpHeaders> borrow(this->handle_state_.get());

  wasi_http_0_2_0_types_header_error_t err;
  wasi_http_0_2_0_types_method_fields_set(borrow, &hdr, &host_values, &err);

  // TODO: handle `err`

  return {};
}

Result<Void> HttpHeaders::append(string_view name, string_view value) {
  handle_state_->assert_valid();
  auto hdr = from_string_view<field_key>(name);
  auto val = from_string_view<field_value>(value);
  Borrow<HttpHeaders> borrow(this->handle_state_.get());

  // TODO: properly handle `err`
  wasi_http_0_2_0_types_header_error_t err;
  if (!wasi_http_0_2_0_types_method_fields_append(borrow, &hdr, &val, &err)) {
    switch (err.tag) {
    case WASI_HTTP_0_2_0_TYPES_HEADER_ERROR_INVALID_SYNTAX:
    case WASI_HTTP_0_2_0_TYPES_HEADER_ERROR_FORBIDDEN:
      return Result<Void>::err(154);
    case WASI_HTTP_0_2_0_TYPES_HEADER_ERROR_IMMUTABLE:
      fprintf(stderr, "Headers %d should not be immutable", this->handle_state_->get());
      MOZ_ASSERT_UNREACHABLE();
    }
  }

  return {};
}

Result<Void> HttpHeaders::remove(string_view name) {
  handle_state_->assert_valid();
  auto hdr = string_view_to_world_string(name);
  Borrow<HttpHeaders> borrow(this->handle_state_.get());

  wasi_http_0_2_0_types_header_error_t err;
  wasi_http_0_2_0_types_method_fields_delete(borrow, &hdr, &err);

  // TODO: handle `err`

  return {};
}

// TODO: convert to `Result`
string_view HttpRequestResponseBase::url() {
  if (_url) {
    return string_view(*_url);
  }

  auto borrow = borrow_incoming_request_t{handle_state_->get()};

  wasi_http_0_2_0_types_scheme_t scheme;
  bool success;
  success = wasi_http_0_2_0_types_method_incoming_request_scheme(borrow, &scheme);
  MOZ_RELEASE_ASSERT(success);

  bindings_string_t authority;
  success = wasi_http_0_2_0_types_method_incoming_request_authority(borrow, &authority);
  MOZ_RELEASE_ASSERT(success);

  bindings_string_t path;
  success = wasi_http_0_2_0_types_method_incoming_request_path_with_query(borrow, &path);
  MOZ_RELEASE_ASSERT(success);

  HostString scheme_str = scheme_to_string(scheme);
  _url = new std::string(scheme_str.ptr.release(), scheme_str.len);
  _url->append(string_view(bindings_string_to_host_string(authority)));
  _url->append(string_view(bindings_string_to_host_string(path)));

  return string_view(*_url);
}

bool write_to_outgoing_body(Borrow<OutputStream> borrow, const uint8_t *ptr, const size_t len) {
  // The write call doesn't mutate the buffer; the cast is just for the
  // generated bindings.
  wasi_io_0_2_0_streams_list_u8_t list{const_cast<uint8_t *>(ptr), len};
  wasi_io_0_2_0_streams_stream_error_t err;
  // TODO: proper error handling.
  return wasi_io_0_2_0_streams_method_output_stream_write(borrow, &list, &err);
}

static Resource::HandleNS OUT_BODY_HANDLE_NS = Resource::next_handle_ns("OutgoingBody");
class OutgoingBodyHandleState final : public HandleState {
  Handle stream_handle_;
  PollableHandle pollable_handle_;

  friend HttpOutgoingBody;

public:
  explicit OutgoingBodyHandleState(const Handle handle)
      : HandleState(handle, OUT_BODY_HANDLE_NS), pollable_handle_(INVALID_POLLABLE_HANDLE) {
    const borrow_outgoing_body_t borrow = {handle};
    own_output_stream_t stream{};
    if (!wasi_http_0_2_0_types_method_outgoing_body_write(borrow, &stream)) {
      MOZ_ASSERT_UNREACHABLE("Getting a body's stream should never fail");
    }
    stream_handle_ = stream.__handle;
  }
};

HttpOutgoingBody::HttpOutgoingBody(std::unique_ptr<HandleState> state) : Pollable() {
  handle_state_ = std::move(state);
}
Result<uint64_t> HttpOutgoingBody::capacity() {
  if (!valid()) {
    // TODO: proper error handling for all 154 error codes.
    return Result<uint64_t>::err(154);
  }

  auto *state = static_cast<OutgoingBodyHandleState *>(this->handle_state_.get());
  Borrow<OutputStream> borrow(state->stream_handle_);
  uint64_t capacity = 0;
  wasi_io_0_2_0_streams_stream_error_t err;
  if (!wasi_io_0_2_0_streams_method_output_stream_check_write(borrow, &capacity, &err)) {
    return Result<uint64_t>::err(154);
  }
  return Result<uint64_t>::ok(capacity);
}

Result<uint32_t> HttpOutgoingBody::write(const uint8_t *bytes, size_t len) {
  auto res = capacity();
  if (res.is_err()) {
    // TODO: proper error handling for all 154 error codes.
    return Result<uint32_t>::err(154);
  }
  auto capacity = res.unwrap();
  auto bytes_to_write = std::min(len, static_cast<size_t>(capacity));

  auto *state = static_cast<OutgoingBodyHandleState *>(this->handle_state_.get());
  Borrow<OutputStream> borrow(state->stream_handle_);
  if (!write_to_outgoing_body(borrow, bytes, bytes_to_write)) {
    return Result<uint32_t>::err(154);
  }

  return Result<uint32_t>::ok(bytes_to_write);
}

Result<Void> HttpOutgoingBody::write_all(const uint8_t *bytes, size_t len) {
  if (!valid()) {
    // TODO: proper error handling for all 154 error codes.
    return Result<Void>::err({});
  }

  auto *state = static_cast<OutgoingBodyHandleState *>(handle_state_.get());
  Borrow<OutputStream> borrow(state->stream_handle_);

  while (len > 0) {
    auto capacity_res = capacity();
    if (capacity_res.is_err()) {
      // TODO: proper error handling for all 154 error codes.
      return Result<Void>::err(154);
    }
    auto capacity = capacity_res.unwrap();
    auto bytes_to_write = std::min(len, static_cast<size_t>(capacity));
    if (!write_to_outgoing_body(borrow, bytes, len)) {
      return Result<Void>::err(154);
    }

    bytes += bytes_to_write;
    len -= bytes_to_write;
  }

  return {};
}

class BodyAppendTask final : public api::AsyncTask {
  enum class State {
    BlockedOnBoth,
    BlockedOnIncoming,
    BlockedOnOutgoing,
    Ready,
    Done,
  };

  HttpIncomingBody *incoming_body_;
  HttpOutgoingBody *outgoing_body_;
  PollableHandle incoming_pollable_;
  PollableHandle outgoing_pollable_;
  State state_;

  void set_state(const State state) {
    MOZ_ASSERT(state_ != State::Done);
    state_ = state;
  }

public:
  explicit BodyAppendTask(HttpIncomingBody *incoming_body, HttpOutgoingBody *outgoing_body)
      : incoming_body_(incoming_body), outgoing_body_(outgoing_body) {
    auto res = incoming_body_->subscribe();
    MOZ_ASSERT(!res.is_err());
    incoming_pollable_ = res.unwrap();

    res = outgoing_body_->subscribe();
    MOZ_ASSERT(!res.is_err());
    outgoing_pollable_ = res.unwrap();

    state_ = State::BlockedOnBoth;
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    // If run is called while we're blocked on the incoming stream, that means that stream's
    // pollable has resolved, so the stream must be ready.
    if (state_ == State::BlockedOnBoth || state_ == State::BlockedOnIncoming) {
      auto res = incoming_body_->read(0);
      MOZ_ASSERT(!res.is_err());
      auto [done, _] = std::move(res.unwrap());
      if (done) {
        set_state(State::Done);
        return true;
      }
      set_state(State::BlockedOnOutgoing);
    }

    uint64_t capacity = 0;
    if (state_ == State::BlockedOnOutgoing) {
      auto res = outgoing_body_->capacity();
      if (res.is_err()) {
        return false;
      }
      capacity = res.unwrap();
      if (capacity > 0) {
        set_state(State::Ready);
      } else {
        engine->queue_async_task(this);
        return true;
      }
    }

    MOZ_ASSERT(state_ == State::Ready);

    // TODO: reuse a buffer for this loop
    do {
      auto res = incoming_body_->read(capacity);
      if (res.is_err()) {
        // TODO: proper error handling.
        return false;
      }
      auto [done, bytes] = std::move(res.unwrap());
      if (bytes.len == 0 && !done) {
        set_state(State::BlockedOnIncoming);
        engine->queue_async_task(this);
        return true;
      }

      auto offset = 0;
      while (bytes.len - offset > 0) {
        // TODO: remove double checking of write-readiness
        // TODO: make this async by storing the remaining chunk in the task and marking it as
        // being blocked on write
        auto write_res = outgoing_body_->write(bytes.ptr.get() + offset, bytes.len - offset);
        if (write_res.is_err()) {
          // TODO: proper error handling.
          return false;
        }
        offset += write_res.unwrap();
      }

      if (done) {
        set_state(State::Done);
        return true;
      }

      auto capacity_res = outgoing_body_->capacity();
      if (capacity_res.is_err()) {
        // TODO: proper error handling.
        return false;
      }
      capacity = capacity_res.unwrap();
    } while (capacity > 0);

    set_state(State::BlockedOnOutgoing);
    engine->queue_async_task(this);
    return true;
  }

  [[nodiscard]] bool cancel(api::Engine *engine) override {
    MOZ_ASSERT_UNREACHABLE("BodyAppendTask's semantics don't allow for cancellation");
    return true;
  }

  [[nodiscard]] int32_t id() override {
    if (state_ == State::BlockedOnBoth || state_ == State::BlockedOnIncoming) {
      return incoming_pollable_;
    }

    MOZ_ASSERT(state_ == State::BlockedOnOutgoing,
               "BodyAppendTask should only be queued if it's not known to be ready");
    return outgoing_pollable_;
  }

  void trace(JSTracer *trc) override {
    // Nothing to trace.
  }
};

Result<Void> HttpOutgoingBody::append(api::Engine *engine, HttpIncomingBody *other) {
  handle_state_->assert_valid();
  engine->queue_async_task(new BodyAppendTask(other, this));
  return {};
}

Result<Void> HttpOutgoingBody::close() {
  handle_state_->assert_valid();

  auto state = static_cast<OutgoingBodyHandleState *>(handle_state_.get());
  // A blocking flush is required here to ensure that all buffered contents are
  // actually written before finishing the body.
  Borrow<OutputStream> borrow{state->stream_handle_};

  {
    wasi_io_0_2_0_streams_stream_error_t err;
    bool success = wasi_io_0_2_0_streams_method_output_stream_blocking_flush(borrow, &err);
    MOZ_RELEASE_ASSERT(success);
    // TODO: handle `err`
  }

  if (state->pollable_handle_ != INVALID_POLLABLE_HANDLE) {
    wasi_io_0_2_0_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  }
  wasi_io_0_2_0_streams_output_stream_drop_own({state->stream_handle_});

  {
    wasi_http_0_2_0_types_error_code_t err;
    wasi_http_0_2_0_types_static_outgoing_body_finish({state->take()}, nullptr, &err);
    // TODO: handle `err`
  }

  return {};
}
Result<PollableHandle> HttpOutgoingBody::subscribe() {
  auto state = static_cast<OutgoingBodyHandleState *>(handle_state_.get());
  if (state->pollable_handle_ == INVALID_POLLABLE_HANDLE) {
    Borrow<OutputStream> borrow(state->stream_handle_);
    state->pollable_handle_ = wasi_io_0_2_0_streams_method_output_stream_subscribe(borrow).__handle;
  }
  return Result<PollableHandle>::ok(state->pollable_handle_);
}

void HttpOutgoingBody::unsubscribe() {
  auto state = static_cast<OutgoingBodyHandleState *>(handle_state_.get());
  if (state->pollable_handle_ == INVALID_POLLABLE_HANDLE) {
    return;
  }
  wasi_io_0_2_0_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  state->pollable_handle_ = INVALID_POLLABLE_HANDLE;
}

static const char *http_method_names[9] = {"GET",     "HEAD",    "POST",  "PUT",  "DELETE",
                                           "CONNECT", "OPTIONS", "TRACE", "PATCH"};

wasi_http_0_2_0_types_method_t http_method_to_host(string_view method_str) {

  if (method_str.empty()) {
    return wasi_http_0_2_0_types_method_t{WASI_HTTP_0_2_0_TYPES_METHOD_GET};
  }

  auto method = method_str.begin();
  for (uint8_t i = 0; i < WASI_HTTP_0_2_0_TYPES_METHOD_OTHER; i++) {
    auto name = http_method_names[i];
    if (strcasecmp(method, name) == 0) {
      return wasi_http_0_2_0_types_method_t{i};
    }
  }

  auto val = bindings_string_t{reinterpret_cast<uint8_t *>(const_cast<char *>(method)),
                               method_str.length()};
  return wasi_http_0_2_0_types_method_t{WASI_HTTP_0_2_0_TYPES_METHOD_OTHER, {val}};
}

HttpOutgoingRequest::HttpOutgoingRequest(std::unique_ptr<HandleState> state) { this->handle_state_ = std::move(state); }

static Resource::HandleNS OUT_REQUEST_HANDLE_NS = Resource::next_handle_ns("HttpOutgoingRequest");
HttpOutgoingRequest *HttpOutgoingRequest::make(string_view method_str, optional<HostString> url_str,
                                               std::unique_ptr<HttpHeadersReadOnly> headers) {
  bindings_string_t path_with_query;
  wasi_http_0_2_0_types_scheme_t scheme;
  bindings_string_t authority;

  bindings_string_t *maybe_path_with_query = nullptr;
  wasi_http_0_2_0_types_scheme_t *maybe_scheme = nullptr;
  bindings_string_t *maybe_authority = nullptr;

  if (url_str) {
    jsurl::SpecString val = url_str.value();
    jsurl::JSUrl *url = new_jsurl(&val);
    jsurl::SpecSlice protocol = jsurl::protocol(url);
    if (std::memcmp(protocol.data, "http:", protocol.len) == 0) {
      scheme.tag = WASI_HTTP_0_2_0_TYPES_SCHEME_HTTP;
    } else if (std::memcmp(protocol.data, "https:", protocol.len) == 0) {
      scheme.tag = WASI_HTTP_0_2_0_TYPES_SCHEME_HTTPS;
    } else {
      scheme.tag = WASI_HTTP_0_2_0_TYPES_SCHEME_OTHER;
      scheme.val = {const_cast<uint8_t *>(protocol.data), protocol.len - 1};
    }
    maybe_scheme = &scheme;

    jsurl::SpecSlice authority_slice = jsurl::authority(url);
    authority = {const_cast<uint8_t *>(authority_slice.data), authority_slice.len};
    maybe_authority = &authority;

    jsurl::SpecSlice path_with_query_slice = jsurl::path_with_query(url);
    path_with_query = {const_cast<uint8_t *>(path_with_query_slice.data),
                       path_with_query_slice.len};
    maybe_path_with_query = &path_with_query;
  }

  Handle headers_handle = headers->handle_state_->take();
  auto handle =
      wasi_http_0_2_0_types_constructor_outgoing_request({headers_handle});
  {
    auto borrow = wasi_http_0_2_0_types_borrow_outgoing_request(handle);

    // TODO: error handling on result
    auto method = http_method_to_host(method_str);
    wasi_http_0_2_0_types_method_outgoing_request_set_method(borrow, &method);

    // TODO: error handling on result
    wasi_http_0_2_0_types_method_outgoing_request_set_scheme(borrow, maybe_scheme);

    // TODO: error handling on result
    wasi_http_0_2_0_types_method_outgoing_request_set_authority(borrow, maybe_authority);

    // TODO: error handling on result
    wasi_http_0_2_0_types_method_outgoing_request_set_path_with_query(borrow,
                                                                      maybe_path_with_query);
  }

  auto *state = new HandleState(handle.__handle, OUT_REQUEST_HANDLE_NS);
  auto *resp = new HttpOutgoingRequest(std::unique_ptr<HandleState>(state));

  return resp;
}

Result<string_view> HttpOutgoingRequest::method() {
  handle_state_->assert_valid();
  return Result<string_view>::ok(method_);
}

Result<HttpHeadersReadOnly *> HttpOutgoingRequest::headers() {
  if (!headers_) {
    if (!valid()) {
      return Result<HttpHeadersReadOnly *>::err(154);
    }
    borrow_outgoing_request_t borrow(handle_state_->get());
    auto res = wasi_http_0_2_0_types_method_outgoing_request_headers(borrow);
    headers_ = new HttpHeadersReadOnly(std::make_unique<HandleState>(res.__handle, HEADERS_HANDLE_NS));
  }

  return Result<HttpHeadersReadOnly *>::ok(headers_);
}

Result<HttpOutgoingBody *> HttpOutgoingRequest::body() {
  typedef Result<HttpOutgoingBody *> Res;
  if (!this->body_) {
    handle_state_->assert_valid();
    outgoing_body_t body;
    if (!wasi_http_0_2_0_types_method_outgoing_request_body(
            wasi_http_0_2_0_types_borrow_outgoing_request({handle_state_->get()}), &body)) {
      return Res::err(154);
    }
    body_ = new HttpOutgoingBody(std::unique_ptr<HandleState>(new OutgoingBodyHandleState(body.__handle)));
  }
  return Res::ok(body_);
}

static Resource::HandleNS FUTURE_INCOMING_RESPONSE_HANDLE_NS = Resource::next_handle_ns("FutureHttpIncomingResponse");
Result<FutureHttpIncomingResponse *> HttpOutgoingRequest::send() {
  typedef Result<FutureHttpIncomingResponse *> Res;
  handle_state_->assert_valid();
  future_incoming_response_t ret;
  wasi_http_0_2_0_outgoing_handler_error_code_t err;
  if (!wasi_http_0_2_0_outgoing_handler_handle({handle_state_->take()}, nullptr, &ret, &err)) {
    return Res::err(154);
  }
  auto res = new FutureHttpIncomingResponse(std::make_unique<HandleState>(ret.__handle, FUTURE_INCOMING_RESPONSE_HANDLE_NS));
  return Result<FutureHttpIncomingResponse *>::ok(res);
}

static Resource::HandleNS IN_BODY_HANDLE_NS = Resource::next_handle_ns("IncomingBody");
class IncomingBodyHandleState final : public HandleState {
  Handle stream_handle_;
  PollableHandle pollable_handle_;

  friend HttpIncomingBody;

public:
  explicit IncomingBodyHandleState(const Handle handle)
      : HandleState(handle, IN_BODY_HANDLE_NS), pollable_handle_(INVALID_POLLABLE_HANDLE) {
    const borrow_incoming_body_t borrow = {handle};
    own_input_stream_t stream{};
    if (!wasi_http_0_2_0_types_method_incoming_body_stream(borrow, &stream)) {
      MOZ_ASSERT_UNREACHABLE("Getting a body's stream should never fail");
    }
    stream_handle_ = stream.__handle;
  }
};

void block_on_pollable_handle(PollableHandle handle) {
  wasi_io_0_2_0_poll_method_pollable_block({handle});
}

HttpIncomingBody::HttpIncomingBody(std::unique_ptr<HandleState> state) : Pollable() { handle_state_ = std::move(state); }

Resource::~Resource() {
  if (handle_state_ != nullptr) {
    delete handle_state_.release();
  }
}

static Resource::HandleNS HANDLE_NS = 0;
Resource::HandleNS Resource::next_handle_ns(const char* ns_name) {
  // DBG("Creating handle namespace %d with name %s\n", HANDLE_NS, ns_name);
  return HANDLE_NS++;
}

bool Resource::valid() const {
  return this->handle_state_ != nullptr && this->handle_state_->valid();
}

Result<HttpIncomingBody::ReadResult> HttpIncomingBody::read(uint32_t chunk_size) {
  typedef Result<ReadResult> Res;

  wasi_io_0_2_0_streams_list_u8_t ret{};
  wasi_io_0_2_0_streams_stream_error_t err{};
  auto borrow = borrow_input_stream_t(
      {static_cast<IncomingBodyHandleState *>(handle_state_.get())->stream_handle_});
  bool success = wasi_io_0_2_0_streams_method_input_stream_read(borrow, chunk_size, &ret, &err);
  if (!success) {
    if (err.tag == WASI_IO_0_2_0_STREAMS_STREAM_ERROR_CLOSED) {
      return Res::ok(ReadResult(true, nullptr, 0));
    }
    return Res::err(154);
  }
  return Res::ok(ReadResult(false, unique_ptr<uint8_t[]>(ret.ptr), ret.len));
}

// TODO: implement
Result<Void> HttpIncomingBody::close() { return {}; }

Result<PollableHandle> HttpIncomingBody::subscribe() {
  auto state = static_cast<IncomingBodyHandleState *>(handle_state_.get());
  auto borrow = borrow_input_stream_t({state->stream_handle_});
  auto pollable = wasi_io_0_2_0_streams_method_input_stream_subscribe(borrow);
  return Result<PollableHandle>::ok(pollable.__handle);
}
void HttpIncomingBody::unsubscribe() {
  auto state = static_cast<IncomingBodyHandleState *>(handle_state_.get());
  if (state->pollable_handle_ == INVALID_POLLABLE_HANDLE) {
    return;
  }
  wasi_io_0_2_0_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  state->pollable_handle_ = INVALID_POLLABLE_HANDLE;
}

FutureHttpIncomingResponse::FutureHttpIncomingResponse(std::unique_ptr<HandleState> state) {
  handle_state_ = std::move(state);
}

static Resource::HandleNS IN_RESPONSE_HANDLE_NS = Resource::next_handle_ns("HttpIncomingResponse");

Result<optional<HttpIncomingResponse *>> FutureHttpIncomingResponse::maybe_response() {
  typedef Result<optional<HttpIncomingResponse *>> Res;
  wasi_http_0_2_0_types_result_result_own_incoming_response_error_code_void_t res;
  auto borrow = wasi_http_0_2_0_types_borrow_future_incoming_response({handle_state_->get()});
  if (!wasi_http_0_2_0_types_method_future_incoming_response_get(borrow, &res)) {
    return Res::ok(std::nullopt);
  }

  MOZ_ASSERT(!res.is_err,
             "FutureHttpIncomingResponse::poll must not be called again after succeeding once");

  auto [is_err, val] = res.val.ok;
  if (is_err) {
    return Res::err(154);
  }

  return Res::ok(new HttpIncomingResponse(std::make_unique<HandleState>(val.ok.__handle, IN_RESPONSE_HANDLE_NS)));
}

Result<PollableHandle> FutureHttpIncomingResponse::subscribe() {
  auto borrow = wasi_http_0_2_0_types_borrow_future_incoming_response({handle_state_->get()});
  auto pollable = wasi_http_0_2_0_types_method_future_incoming_response_subscribe(borrow);
  return Result<PollableHandle>::ok(pollable.__handle);
}
void FutureHttpIncomingResponse::unsubscribe() {
  // TODO: implement
}

HttpHeadersReadOnly::HttpHeadersReadOnly(std::unique_ptr<HandleState> state) {
  handle_state_ = std::move(state);
}

Result<uint16_t> HttpIncomingResponse::status() {
  if (status_ == UNSET_STATUS) {
    if (!valid()) {
      return Result<uint16_t>::err(154);
    }
    auto borrow = wasi_http_0_2_0_types_borrow_incoming_response_t({handle_state_->get()});
    status_ = wasi_http_0_2_0_types_method_incoming_response_status(borrow);
  }
  return Result<uint16_t>::ok(status_);
}

HttpIncomingResponse::HttpIncomingResponse(std::unique_ptr<HandleState> state) {
  handle_state_ = std::move(state);
}

Result<HttpHeadersReadOnly *> HttpIncomingResponse::headers() {
  if (!headers_) {
    if (!valid()) {
      return Result<HttpHeadersReadOnly *>::err(154);
    }
    auto res = wasi_http_0_2_0_types_method_incoming_response_headers(
        wasi_http_0_2_0_types_borrow_incoming_response({handle_state_->get()}));
    headers_ = new HttpHeadersReadOnly(std::make_unique<HandleState>(res.__handle, HEADERS_HANDLE_NS));
  }

  return Result<HttpHeadersReadOnly *>::ok(headers_);
}

Result<HttpIncomingBody *> HttpIncomingResponse::body() {
  if (!body_) {
    if (!valid()) {
      return Result<HttpIncomingBody *>::err(154);
    }
    incoming_body_t body;
    if (!wasi_http_0_2_0_types_method_incoming_response_consume(
            wasi_http_0_2_0_types_borrow_incoming_response({handle_state_->get()}), &body)) {
      return Result<HttpIncomingBody *>::err(154);
    }
    body_ = new HttpIncomingBody(std::unique_ptr<HandleState>(new IncomingBodyHandleState(body.__handle)));
  }
  return Result<HttpIncomingBody *>::ok(body_);
}

HttpOutgoingResponse::HttpOutgoingResponse(std::unique_ptr<HandleState> state) { this->handle_state_ = std::move(state); }

static Resource::HandleNS OUT_RESPONSE_HANDLE_NS = Resource::next_handle_ns("HttpOutgoingResponse");
HttpOutgoingResponse *HttpOutgoingResponse::make(const uint16_t status, unique_ptr<HttpHeaders> headers) {
  wasi_http_0_2_0_types_own_headers_t owned{headers->handle_state_->take()};
  auto handle = wasi_http_0_2_0_types_constructor_outgoing_response(owned);
  auto borrow = wasi_http_0_2_0_types_borrow_outgoing_response(handle);

  auto *state = new HandleState(handle.__handle, OUT_RESPONSE_HANDLE_NS);
  auto *resp = new HttpOutgoingResponse(std::unique_ptr<HandleState>(state));

  // Set the status
  if (status != 200) {
    // The DOM implementation is expected to have validated the status code already.
    MOZ_RELEASE_ASSERT(wasi_http_0_2_0_types_method_outgoing_response_set_status_code(borrow, status));
  }

  resp->status_ = status;
  return resp;
}

Result<HttpHeadersReadOnly *> HttpOutgoingResponse::headers() {
  if (!headers_) {
    if (!valid()) {
      return Result<HttpHeadersReadOnly *>::err(154);
    }
    borrow_outgoing_response_t borrow(handle_state_->get());
    auto res = wasi_http_0_2_0_types_method_outgoing_response_headers(borrow);
    headers_ = new HttpHeadersReadOnly(std::make_unique<HandleState>(res.__handle, HEADERS_HANDLE_NS));
  }

  return Result<HttpHeadersReadOnly *>::ok(headers_);
}

Result<HttpOutgoingBody *> HttpOutgoingResponse::body() {
  typedef Result<HttpOutgoingBody *> Res;
  if (!this->body_) {
    handle_state_->assert_valid();
    outgoing_body_t body;
    if (!wasi_http_0_2_0_types_method_outgoing_response_body(
            wasi_http_0_2_0_types_borrow_outgoing_response({handle_state_->get()}), &body)) {
      return Res::err(154);
    }
    body_ = new HttpOutgoingBody(std::unique_ptr<HandleState>(new OutgoingBodyHandleState(body.__handle)));
  }
  return Res::ok(this->body_);
}
Result<uint16_t> HttpOutgoingResponse::status() { return Result<uint16_t>::ok(status_); }

HttpIncomingRequest::HttpIncomingRequest(std::unique_ptr<HandleState> state) {
  handle_state_ = std::move(state);
}

Result<string_view> HttpIncomingRequest::method() {
  if (method_.empty()) {
    if (!valid()) {
      return Result<string_view>::err(154);
    }
  }
  wasi_http_0_2_0_types_method_t method;
  wasi_http_0_2_0_types_method_incoming_request_method(
      borrow_incoming_request_t(handle_state_->get()), &method);
  if (method.tag != WASI_HTTP_0_2_0_TYPES_METHOD_OTHER) {
    method_ = std::string(http_method_names[method.tag], strlen(http_method_names[method.tag]));
  } else {
    method_ = std::string(reinterpret_cast<char *>(method.val.other.ptr), method.val.other.len);
    bindings_string_free(&method.val.other);
  }
  return Result<string_view>::ok(method_);
}

Result<HttpHeadersReadOnly *> HttpIncomingRequest::headers() {
  if (!headers_) {
    if (!valid()) {
      return Result<HttpHeadersReadOnly *>::err(154);
    }
    borrow_incoming_request_t borrow(handle_state_->get());
    auto res = wasi_http_0_2_0_types_method_incoming_request_headers(borrow);
    headers_ = new HttpHeadersReadOnly(std::make_unique<HandleState>(res.__handle, HEADERS_HANDLE_NS));
  }

  return Result<HttpHeadersReadOnly *>::ok(headers_);
}

Result<HttpIncomingBody *> HttpIncomingRequest::body() {
  if (!body_) {
    if (!valid()) {
      return Result<HttpIncomingBody *>::err(154);
    }
    incoming_body_t body;
    if (!wasi_http_0_2_0_types_method_incoming_request_consume(
            borrow_incoming_request_t(handle_state_->get()), &body)) {
      return Result<HttpIncomingBody *>::err(154);
    }
    body_ = new HttpIncomingBody(std::unique_ptr<HandleState>(new IncomingBodyHandleState(body.__handle)));
  }
  return Result<HttpIncomingBody *>::ok(body_);
}

} // namespace host_api

static host_api::HttpIncomingRequest::RequestHandler REQUEST_HANDLER = nullptr;
static host_api::HandleState *RESPONSE_OUT = nullptr;

void host_api::HttpIncomingRequest::set_handler(RequestHandler handler) {
  MOZ_ASSERT(!REQUEST_HANDLER);
  REQUEST_HANDLER = handler;
}

host_api::Result<host_api::Void> host_api::HttpOutgoingResponse::send() {
  wasi_http_0_2_0_types_result_own_outgoing_response_error_code_t result;

  result.is_err = false;
  result.val.ok = {this->handle_state_->take()};

  wasi_http_0_2_0_types_static_response_outparam_set({RESPONSE_OUT->take()}, &result);

  return {};
}

static host_api::Resource::HandleNS RESPONSE_OUTPARAM_HANDLE_NS =
  host_api::Resource::next_handle_ns("ResponseOutParam");
static host_api::Resource::HandleNS IN_REQUEST_HANDLE_NS = host_api::Resource::next_handle_ns("HttpIncomingRequest");
void exports_wasi_http_incoming_handler(exports_wasi_http_incoming_request request_handle,
                                        exports_wasi_http_response_outparam response_out) {
  RESPONSE_OUT = new host_api::HandleState(response_out.__handle, RESPONSE_OUTPARAM_HANDLE_NS);
  auto *request = new host_api::HttpIncomingRequest(std::make_unique<host_api::HandleState>(request_handle.__handle, IN_REQUEST_HANDLE_NS));
  auto res = REQUEST_HANDLER(request);
  MOZ_RELEASE_ASSERT(res);
  MOZ_RELEASE_ASSERT(!RESPONSE_OUT->valid());
}
