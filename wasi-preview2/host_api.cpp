#include "host_api.h"

#include <bindings.h>
#include <engine.h>
#include <algorithm>

using std::optional;
using std::string_view;
using std::tuple;
using std::unique_ptr;
using std::vector;

// The host interface makes the assumption regularly that uint32_t is sufficient space to store a
// pointer.
static_assert(sizeof(uint32_t) == sizeof(void *));

typedef wasi_http_0_2_0_rc_2023_10_18_types_list_u8_t http_list_u8_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_list_list_u8_t http_list_list_u8_t;

typedef wasi_http_0_2_0_rc_2023_10_18_types_own_incoming_request_t incoming_request_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_request_t borrow_incoming_request_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_own_incoming_response_t incoming_response_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_request_t borrow_outgoing_request_t;

typedef wasi_http_0_2_0_rc_2023_10_18_types_own_future_incoming_response_t future_incoming_response_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_borrow_future_incoming_response_t borrow_future_incoming_response_t;

typedef wasi_http_0_2_0_rc_2023_10_18_types_own_incoming_body_t incoming_body_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_own_outgoing_body_t outgoing_body_t;

typedef wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_body_t borrow_incoming_body_t;
typedef wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_body_t borrow_outgoing_body_t;

typedef wasi_io_0_2_0_rc_2023_10_18_poll_own_pollable_t own_pollable_t;
typedef wasi_io_0_2_0_rc_2023_10_18_poll_borrow_pollable_t borrow_pollable_t;
typedef wasi_io_0_2_0_rc_2023_10_18_poll_list_borrow_pollable_t list_borrow_pollable_t;

typedef wasi_io_0_2_0_rc_2023_10_18_poll_list_u32_t poll_list_u32_t;

typedef wasi_io_0_2_0_rc_2023_10_18_streams_own_input_stream_t own_input_stream_t;
typedef wasi_io_0_2_0_rc_2023_10_18_streams_borrow_input_stream_t borrow_input_stream_t;

typedef wasi_io_0_2_0_rc_2023_10_18_streams_own_output_stream_t own_output_stream_t;
typedef wasi_io_0_2_0_rc_2023_10_18_streams_borrow_output_stream_t borrow_output_stream_t;

typedef wasi_io_0_2_0_rc_2023_10_18_streams_list_u8_t streams_list_u8_t;

size_t core::AsyncTask::select(std::vector<core::AsyncTask*>* tasks) {
  auto count = tasks->size();
  vector<wasi_io_0_2_0_rc_2023_10_18_poll_borrow_pollable_t> handles;
  for (const auto task : *tasks) {
    handles.emplace_back(borrow_pollable_t{task->id()});
  }
  auto list = list_borrow_pollable_t{handles.data(), count};
  poll_list_u32_t result = {.ptr = nullptr,.len = 0};
  wasi_io_0_2_0_rc_2023_10_18_poll_poll_list(&list, &result);
  MOZ_ASSERT(result.len > 0);
  const auto ready_index = result.ptr[0];
  free(result.ptr);

  return ready_index;
}

namespace host_api {

HostString::HostString(const char *c_str) {
  len = strlen(c_str);
  ptr = JS::UniqueChars(static_cast<char*>(malloc(len + 1)));
  std::memcpy(ptr.get(), c_str, len);
  ptr[len] = '\0';
}

template<typename T>
HostString bindings_string_to_host_string(T str) {
  return {JS::UniqueChars(reinterpret_cast<char*>(str.ptr)), str.len};
}

template<typename T>
HostString bindings_bytes_to_host_string(T str) {
  return {JS::UniqueChars(reinterpret_cast<char*>(str.ptr)), str.len};
}

namespace {

bindings_string_t string_view_to_world_string(std::string_view str) {
  return {
      .ptr = (uint8_t*)str.data(),
      .len = str.size(),
  };
}

http_list_u8_t string_view_to_world_bytes(std::string_view str) {
  return {
      .ptr = (uint8_t *)str.data(),
      .len = str.size(),
  };
}

HostString scheme_to_string(const wasi_http_0_2_0_rc_2023_10_18_types_scheme_t scheme) {
  if (scheme.tag == WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTP) {
    return {"http:"};
  }
  if (scheme.tag == WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTPS) {
    return {"https:"};
  }
  return bindings_string_to_host_string(scheme.val.other);
}

} // namespace

Result<HostBytes> Random::get_bytes(size_t num_bytes) {
  Result<HostBytes> res;

  wasi_random_0_2_0_rc_2023_10_18_random_list_u8_t list{};
  wasi_random_0_2_0_rc_2023_10_18_random_get_random_bytes(num_bytes, &list);
  auto ret = HostBytes {
      std::unique_ptr<uint8_t[]>{list.ptr},
      list.len,
  };
  res.emplace(std::move(ret));

  return res;
}

Result<uint32_t> Random::get_u32() {
  return Result<uint32_t>::ok(wasi_random_0_2_0_rc_2023_10_18_random_get_random_u64());
}

uint64_t MonotonicClock::now() {
  return wasi_clocks_0_2_0_rc_2023_10_18_monotonic_clock_now();
}

uint64_t MonotonicClock::resolution() {
  return wasi_clocks_0_2_0_rc_2023_10_18_monotonic_clock_resolution();
}

int32_t MonotonicClock::subscribe(const uint64_t when, const bool absolute) {
  return wasi_clocks_0_2_0_rc_2023_10_18_monotonic_clock_subscribe(when, absolute).__handle;
}

void MonotonicClock::unsubscribe(const int32_t handle_id) {
  wasi_io_0_2_0_rc_2023_10_18_poll_pollable_drop_own(own_pollable_t{handle_id});
}

HttpHeaders::HttpHeaders() {
  auto entries = wasi_http_0_2_0_rc_2023_10_18_types_list_tuple2_string_list_u8_t{nullptr, 0};
  this->handle_state_ =
      new HandleState(wasi_http_0_2_0_rc_2023_10_18_types_constructor_fields(&entries).__handle);
}
HttpHeaders::HttpHeaders(Handle handle) {
  handle_state_ = new HandleState(handle);
}

HttpHeaders::HttpHeaders(const vector<tuple<string_view, vector<string_view>>>& entries) {
  for (const auto& [name, values] : entries) {
    for (const auto& value : values) {
      auto res = set(name, value);
      MOZ_RELEASE_ASSERT(!res.is_err());
    }
  }
}

HttpHeaders::HttpHeaders(const HttpHeaders &headers) {
  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({headers.handle_state_->handle});
  auto handle = wasi_http_0_2_0_rc_2023_10_18_types_method_fields_clone(borrow);
  handle_state_ = new HandleState(handle.__handle);
}

Result<vector<tuple<HostString, HostString>>> HttpHeaders::entries() const {
  Result<vector<tuple<HostString, HostString>>> res;
  MOZ_ASSERT(valid());

  wasi_http_0_2_0_rc_2023_10_18_types_list_tuple2_string_list_u8_t entries;
  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({handle_state_->handle});
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_entries(borrow, &entries);

  vector<tuple<HostString, HostString>> entries_vec;
  for (int i = 0; i < entries.len; i++) {
    auto key = entries.ptr[i].f0;
    auto value = entries.ptr[i].f1;
    entries_vec.emplace_back(bindings_string_to_host_string(key), bindings_bytes_to_host_string(value));
  }
  // Free the outer list, but not the entries themselves.
  free(entries.ptr);
  res.emplace(std::move(entries_vec));

  return res;
}

Result<vector<HostString>> HttpHeaders::names() const {
  Result<vector<HostString>> res;
  MOZ_ASSERT(valid());

  wasi_http_0_2_0_rc_2023_10_18_types_list_tuple2_string_list_u8_t entries;
  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({handle_state_->handle});
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_entries(borrow, &entries);

  vector<HostString> names;
  for (int i = 0; i < entries.len; i++) {
    names.emplace_back(bindings_string_to_host_string(entries.ptr[i].f0));
  }
  // Free the outer list, but not the entries themselves.
  free(entries.ptr);
  res.emplace(std::move(names));

  return res;
}

Result<optional<vector<HostString>>> HttpHeaders::get(string_view name) const {
  Result<optional<vector<HostString>>> res;
  MOZ_ASSERT(valid());

  http_list_list_u8_t values;
  auto hdr = string_view_to_world_string(name);
  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({handle_state_->handle});
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_get(borrow, &hdr, &values);

  if (values.len > 0) {
    std::vector<HostString> names;
    for (int i = 0; i < values.len; i++) {
      names.emplace_back(bindings_bytes_to_host_string(values.ptr[i]));
    }
    // Free the outer list, but not the values themselves.
    free(values.ptr);
    res.emplace(std::move(names));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<Void> HttpHeaders::set(string_view name, string_view value) {
  MOZ_ASSERT(valid());
  auto hdr = string_view_to_world_string(name);
  auto [ptr, len] = string_view_to_world_bytes(value);
  auto fieldval = http_list_u8_t{ptr, len};

  http_list_list_u8_t host_values = {&fieldval, 1};

  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({handle_state_->handle});
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_set(borrow, &hdr, &host_values);
  free(host_values.ptr);

  return {};
}

Result<Void> HttpHeaders::append(string_view name, string_view value) {
  MOZ_ASSERT(valid());
  auto hdr = string_view_to_world_string(name);
  auto [ptr, len] = string_view_to_world_bytes(value);

  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({handle_state_->handle});
  auto fieldval = http_list_u8_t{ptr, len};
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_append(borrow, &hdr, &fieldval);

  return {};
}

Result<Void> HttpHeaders::remove(string_view name) {
  MOZ_ASSERT(valid());
  auto hdr = string_view_to_world_string(name);

  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields({handle_state_->handle});
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_delete(borrow, &hdr);

  return {};
}

// TODO: convert to `Result`
string_view HttpRequestResponseBase::url() {
  if (_url) {
    return string_view(*_url);
  }

  auto borrow = borrow_incoming_request_t{handle_state_->handle};

  wasi_http_0_2_0_rc_2023_10_18_types_scheme_t scheme;
  bool success;
  success = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_scheme(borrow, &scheme);
  MOZ_RELEASE_ASSERT(success);

  bindings_string_t authority;
  success = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_authority(borrow, &authority);
  MOZ_RELEASE_ASSERT(success);

  bindings_string_t path;
  success = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_path_with_query(borrow, &path);
  MOZ_RELEASE_ASSERT(success);

  HostString scheme_str = scheme_to_string(scheme);
  _url = new std::string(scheme_str.ptr.release(), scheme_str.len);
  _url->append(string_view(bindings_string_to_host_string(authority)));
  _url->append(string_view(bindings_string_to_host_string(path)));

  return string_view(*_url);
}

bool write_to_outgoing_body(borrow_output_stream_t borrow, const uint8_t *ptr, const size_t len) {
  // The write call doesn't mutate the buffer; the cast is just for the
  // generated bindings.
  streams_list_u8_t list{const_cast<uint8_t *>(ptr), len};
  wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t err;
  // TODO: proper error handling.
  return wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_write(borrow, &list, &err);
}

class OutgoingBodyHandleState final : HandleState {
  Handle stream_handle_;
  core::PollableHandle pollable_handle_;

  friend HttpOutgoingBody;

public:
  explicit OutgoingBodyHandleState(const Handle handle)
        : HandleState(handle), pollable_handle_(core::INVALID_POLLABLE_HANDLE) {
    const borrow_outgoing_body_t borrow = {handle};
    own_output_stream_t stream{};
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_outgoing_body_write(borrow, &stream)) {
      MOZ_ASSERT_UNREACHABLE("Getting a body's stream should never fail");
    }
    stream_handle_ = stream.__handle;
  }
};

HttpOutgoingBody::HttpOutgoingBody(Handle handle) : Pollable() {
  handle_state_ = new OutgoingBodyHandleState(handle);
}
Result<uint64_t> HttpOutgoingBody::capacity() {
  if (!valid()) {
    // TODO: proper error handling for all 154 error codes.
    return Result<uint64_t>::err(154);
  }

  auto borrow = borrow_output_stream_t({static_cast<OutgoingBodyHandleState*>(handle_state_)->stream_handle_});
  uint64_t capacity = 0;
  wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t err;
  if (!wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_check_write(borrow, &capacity, &err)) {
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

  auto borrow = borrow_output_stream_t({static_cast<OutgoingBodyHandleState*>(handle_state_)->stream_handle_});
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

  auto borrow = borrow_output_stream_t({static_cast<OutgoingBodyHandleState*>(handle_state_)->stream_handle_});

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

class BodyAppendTask final : public core::AsyncTask {
  enum class State {
    BlockedOnBoth,
    BlockedOnIncoming,
    BlockedOnOutgoing,
    Ready,
    Done,
  };

  HttpIncomingBody* incoming_body_;
  HttpOutgoingBody *outgoing_body_;
  core::PollableHandle incoming_pollable_;
  core::PollableHandle outgoing_pollable_;
  State state_;

  void set_state(const State state) {
    MOZ_ASSERT(state_ != State::Done);
    state_ = state;
  }

public:
  explicit BodyAppendTask(HttpIncomingBody* incoming_body, HttpOutgoingBody* outgoing_body)
    : incoming_body_(incoming_body), outgoing_body_(outgoing_body) {
    auto res = incoming_body_->subscribe();
    MOZ_ASSERT(!res.is_err());
    incoming_pollable_ = res.unwrap();

    res = outgoing_body_->subscribe();
    MOZ_ASSERT(!res.is_err());
    outgoing_pollable_ = res.unwrap();

    state_ = State::BlockedOnBoth;
  }

  [[nodiscard]] bool run(core::Engine* engine) override {
    // If run is called while we're blocked on the incoming stream, that means that stream's
    // pollable has resolved, so the stream must be ready.
    if (state_ == State::BlockedOnBoth || state_ == State::BlockedOnIncoming) {
      auto res = incoming_body_->read(0);
      MOZ_ASSERT(!res.is_err());
      auto [bytes, done] = std::move(res.unwrap());
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
        // TODO: make this async by storing the remaining chunk in the task and marking it as being blocked on write
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

  [[nodiscard]] bool cancel(core::Engine* engine) override {
    MOZ_ASSERT_UNREACHABLE("BodyAppendTask's semantics don't allow for cancellation");
    return true;
  }

  bool ready() override {
    // TODO(TS): properly implement. This won't ever return `true` right now
    return state_ == State::Ready;
  }

  [[nodiscard]] int32_t id() override {
    if (state_ == State::BlockedOnBoth || state_ == State::BlockedOnIncoming) {
      return incoming_pollable_;
    }

    MOZ_ASSERT(state_ == State::BlockedOnOutgoing, "BodyAppendTask should only be queued if it's not known to be ready");
    return outgoing_pollable_;
  }

  void trace(JSTracer *trc) override {
    // Nothing to trace.
  }
};

Result<Void> HttpOutgoingBody::append(core::Engine* engine, HttpIncomingBody *other) {
  MOZ_ASSERT(valid());
  engine->queue_async_task(new BodyAppendTask(other, this));
  return {};
}

Result<Void> HttpOutgoingBody::close() {
  MOZ_ASSERT(valid());

  auto state = static_cast<OutgoingBodyHandleState *>(handle_state_);
  // A blocking flush is required here to ensure that all buffered contents are
  // actually written before finishing the body.
  borrow_output_stream_t borrow = {state->stream_handle_};
  wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t err;
  bool success =
      wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_blocking_flush(borrow, &err);
  MOZ_RELEASE_ASSERT(success);
  if (state->pollable_handle_ != core::INVALID_POLLABLE_HANDLE) {
    wasi_io_0_2_0_rc_2023_10_18_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  }
  wasi_io_0_2_0_rc_2023_10_18_streams_output_stream_drop_own({state->stream_handle_});
  wasi_http_0_2_0_rc_2023_10_18_types_static_outgoing_body_finish({state->handle}, nullptr);
  delete handle_state_;
  handle_state_ = nullptr;

  return {};
}
Result<core::PollableHandle> HttpOutgoingBody::subscribe() {
  auto state = static_cast<OutgoingBodyHandleState *>(handle_state_);
  if (state->pollable_handle_ == core::INVALID_POLLABLE_HANDLE) {
    auto borrow = borrow_output_stream_t({state->stream_handle_});
    state->pollable_handle_ =
        wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_subscribe(borrow).__handle;
  }
  return Result<core::PollableHandle>::ok(state->pollable_handle_);
}

void HttpOutgoingBody::unsubscribe() {
  auto state = static_cast<OutgoingBodyHandleState *>(handle_state_);
  if (state->pollable_handle_ == core::INVALID_POLLABLE_HANDLE) {
    return;
  }
  wasi_io_0_2_0_rc_2023_10_18_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  state->pollable_handle_ = core::INVALID_POLLABLE_HANDLE;
}

static const char *http_method_names[9] = {"GET",     "HEAD",   "POST",
                                           "PUT",     "DELETE", "CONNECT",
                                           "OPTIONS", "TRACE",  "PATCH"};

wasi_http_0_2_0_rc_2023_10_18_types_method_t http_method_to_host(string_view method_str) {

  if (method_str.empty()) {
    return wasi_http_0_2_0_rc_2023_10_18_types_method_t{WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_METHOD_GET};
  }

  auto method = method_str.begin();
  for (uint8_t i = 0; i < WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_METHOD_OTHER; i++) {
    auto name = http_method_names[i];
    if (strcasecmp(method, name) == 0) {
      return wasi_http_0_2_0_rc_2023_10_18_types_method_t{i};
    }
  }

  auto val = bindings_string_t{reinterpret_cast<uint8_t*>(const_cast<char*>(method)), method_str.length()};
  return wasi_http_0_2_0_rc_2023_10_18_types_method_t{WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_METHOD_OTHER, {val}};
}

HttpOutgoingRequest::HttpOutgoingRequest(string_view method_str, optional<HostString> url_str,
                                         HttpHeaders *headers) {
  bindings_string_t path_with_query;
  wasi_http_0_2_0_rc_2023_10_18_types_scheme_t scheme;
  bindings_string_t authority;

  bindings_string_t *maybe_path_with_query = nullptr;
  wasi_http_0_2_0_rc_2023_10_18_types_scheme_t *maybe_scheme = nullptr;
  bindings_string_t *maybe_authority = nullptr;

  if (url_str) {
    jsurl::SpecString val = url_str.value();
    jsurl::JSUrl *url = new_jsurl(&val);
    jsurl::SpecSlice protocol = jsurl::protocol(url);
    if (std::memcmp(protocol.data, "http:", protocol.len) == 0) {
      scheme.tag = WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTP;
    } else if (std::memcmp(protocol.data, "https:", protocol.len) == 0) {
      scheme.tag = WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTPS;
    } else {
      scheme.tag = WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_OTHER;
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

  wasi_http_0_2_0_rc_2023_10_18_types_method_t method = http_method_to_host(method_str);
  auto handle = wasi_http_0_2_0_rc_2023_10_18_types_constructor_outgoing_request(
                    &method, maybe_path_with_query, maybe_scheme, maybe_authority,
                    {headers->handle_state_->handle})
                    .__handle;
  handle_state_ = new HandleState(handle);
}

Result<string_view> HttpOutgoingRequest::method() {
  MOZ_ASSERT(valid());
  MOZ_ASSERT(headers_);
  return Result<string_view>::ok(method_);
}

Result<HttpHeaders*> HttpOutgoingRequest::headers() {
  MOZ_ASSERT(valid());
  MOZ_ASSERT(headers_);
  return Result<HttpHeaders*>::ok(headers_);
}

Result<HttpOutgoingBody *> HttpOutgoingRequest::body() {
  typedef Result<HttpOutgoingBody *> Res;
  MOZ_ASSERT(valid());
  if (!this->body_) {
    outgoing_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_outgoing_request_write(
         wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_request({handle_state_->handle}), &body)) {
      return Res::err(154);
    }
    this->body_ = new HttpOutgoingBody(body.__handle);
  }
  return Res::ok(body_);
}

Result<FutureHttpIncomingResponse*> HttpOutgoingRequest::send() {
  MOZ_ASSERT(valid());
  future_incoming_response_t ret;
  wasi_http_0_2_0_rc_2023_10_18_outgoing_handler_error_t err;
  wasi_http_0_2_0_rc_2023_10_18_outgoing_handler_handle({handle_state_->handle}, nullptr, &ret, &err);
  auto res = new FutureHttpIncomingResponse(ret.__handle);
  return Result<FutureHttpIncomingResponse*>::ok(res);
}

class IncomingBodyHandleState final : HandleState {
  Handle stream_handle_;
  core::PollableHandle pollable_handle_;

  friend HttpIncomingBody;

public:
  explicit IncomingBodyHandleState(const Handle handle)
          : HandleState(handle), pollable_handle_(core::INVALID_POLLABLE_HANDLE) {
    const borrow_incoming_body_t borrow = {handle};
    own_input_stream_t stream{};
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_body_stream(borrow, &stream)) {
      MOZ_ASSERT_UNREACHABLE("Getting a body's stream should never fail");
    }
    stream_handle_ = stream.__handle;
  }
};

HttpIncomingBody::HttpIncomingBody(const Handle handle) : Pollable() {
  handle_state_ = new IncomingBodyHandleState(handle);
}

Result<HttpIncomingBody::ReadResult> HttpIncomingBody::read(uint32_t chunk_size) {
  typedef Result<ReadResult> Res;

  auto ret = streams_list_u8_t {};
  auto err = wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t {};
  auto borrow = borrow_input_stream_t({static_cast<IncomingBodyHandleState*>(handle_state_)->stream_handle_});
  bool success = wasi_io_0_2_0_rc_2023_10_18_streams_method_input_stream_read(borrow, chunk_size, &ret, &err);
  if (!success) {
    if (err.tag == WASI_IO_0_2_0_RC_2023_10_18_STREAMS_STREAM_ERROR_CLOSED) {
      return Res::ok(ReadResult(true, nullptr, 0));
    }
    return Res::err(154);
  }
  return Res::ok(ReadResult(false, unique_ptr<uint8_t[]>(ret.ptr), ret.len));
}

// TODO: implement
Result<Void> HttpIncomingBody::close() { return {}; }

Result<core::PollableHandle> HttpIncomingBody::subscribe() {
  auto borrow = borrow_input_stream_t(
      {static_cast<IncomingBodyHandleState *>(handle_state_)->stream_handle_});
  auto pollable = wasi_io_0_2_0_rc_2023_10_18_streams_method_input_stream_subscribe(borrow);
  return Result<core::PollableHandle>::ok(pollable.__handle);
}
void HttpIncomingBody::unsubscribe() {
  auto state = static_cast<IncomingBodyHandleState *>(handle_state_);
  if (state->pollable_handle_ == core::INVALID_POLLABLE_HANDLE) {
    return;
  }
  wasi_io_0_2_0_rc_2023_10_18_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  state->pollable_handle_ = core::INVALID_POLLABLE_HANDLE;
}

FutureHttpIncomingResponse::FutureHttpIncomingResponse(Handle handle) {
  handle_state_ = new HandleState(handle);
}

Result<optional<HttpIncomingResponse *>> FutureHttpIncomingResponse::maybe_response() {
  typedef Result<optional<HttpIncomingResponse *>> Res;
  wasi_http_0_2_0_rc_2023_10_18_types_result_result_own_incoming_response_error_void_t res;
  auto borrow =
      wasi_http_0_2_0_rc_2023_10_18_types_borrow_future_incoming_response({handle_state_->handle});
  if (!wasi_http_0_2_0_rc_2023_10_18_types_method_future_incoming_response_get(borrow, &res)) {
    return Res::ok(std::nullopt);
  }

  MOZ_ASSERT(!res.is_err, "FutureHttpIncomingResponse::poll must not be called again after succeeding once");

  auto [is_err, val] = res.val.ok;
  if (is_err) {
    return Res::err(154);
  }

  return Res::ok(new HttpIncomingResponse(val.ok.__handle));
}

Result<core::PollableHandle> FutureHttpIncomingResponse::subscribe() {
  auto borrow =
      wasi_http_0_2_0_rc_2023_10_18_types_borrow_future_incoming_response({handle_state_->handle});
  auto pollable =
      wasi_http_0_2_0_rc_2023_10_18_types_method_future_incoming_response_subscribe(borrow);
  return Result<core::PollableHandle>::ok(pollable.__handle);
}
void FutureHttpIncomingResponse::unsubscribe() {
  // TODO: implement
}

Result<uint16_t> HttpIncomingResponse::status() {
  if (status_ == UNSET_STATUS) {
    if (!valid()) {
      return Result<uint16_t>::err(154);
    }
    auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_response_t({handle_state_->handle});
    status_ = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_response_status(borrow);
  }
  return Result<uint16_t>::ok(status_);
}

HttpIncomingResponse::HttpIncomingResponse(Handle handle) {
  handle_state_ = new HandleState(handle);
}

Result<HttpHeaders*> HttpIncomingResponse::headers() {
  if (!headers_) {
    if (!valid()) {
      return Result<HttpHeaders*>::err(154);
    }
    auto res = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_response_headers(
        wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_response({handle_state_->handle}));
    headers_ = new HttpHeaders(res.__handle);
  }

  return Result<HttpHeaders*>::ok(headers_);
}

Result<HttpIncomingBody *> HttpIncomingResponse::body() {
  if (!body_) {
    if (!valid()) {
      return Result<HttpIncomingBody*>::err(154);
    }
    incoming_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_response_consume(
        wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_response({handle_state_->handle}), &body)) {
      return Result<HttpIncomingBody *>::err(154);
    }
    body_ = new HttpIncomingBody(body.__handle);
  }
  return Result<HttpIncomingBody *>::ok(body_);
}

HttpOutgoingResponse::HttpOutgoingResponse(const uint16_t status, HttpHeaders *headers) {
  status_ = status;
  headers_ = headers;
  auto borrow =
      wasi_http_0_2_0_rc_2023_10_18_types_borrow_headers_t({headers_->handle_state_->handle});
  auto handle = wasi_http_0_2_0_rc_2023_10_18_types_constructor_outgoing_response(status, borrow);
  handle_state_ = new HandleState(handle.__handle);
}

Result<HttpHeaders *> HttpOutgoingResponse::headers() {
  if (!valid()) {
    return Result<HttpHeaders*>::err(154);
  }
  return Result<HttpHeaders*>::ok(headers_);
}

Result<HttpOutgoingBody *> HttpOutgoingResponse::body() {
  typedef Result<HttpOutgoingBody *> Res;
  MOZ_ASSERT(valid());
  if (!this->body_) {
    outgoing_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_outgoing_response_write(
            wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_response({handle_state_->handle}),
            &body)) {
      return Res::err(154);
    }
    this->body_ = new HttpOutgoingBody(body.__handle);
  }
  return Res::ok(this->body_);
}
Result<uint16_t> HttpOutgoingResponse::status() {
  return Result<uint16_t>::ok(status_);
}

Result<Void> HttpOutgoingResponse::send(ResponseOutparam out_param) {
  auto result = wasi_http_0_2_0_rc_2023_10_18_types_result_own_outgoing_response_error_t{false};
  wasi_http_0_2_0_rc_2023_10_18_types_static_response_outparam_set({out_param}, &result);
  // TODO: proper error handling
  return {};
}

HttpIncomingRequest::HttpIncomingRequest(Handle handle) {
  handle_state_ = new HandleState(handle);
}

Result<string_view> HttpIncomingRequest::method() {
  if (method_.empty()) {
    if (!valid()) {
      return Result<string_view>::err(154);
    }
  }
  wasi_http_0_2_0_rc_2023_10_18_types_method_t method;
  wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_method(
      borrow_incoming_request_t(handle_state_->handle), &method);
  if (method.tag != WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_METHOD_OTHER) {
    method_ = std::string(http_method_names[method.tag], strlen(http_method_names[method.tag]));
  } else {
    method_ = std::string(reinterpret_cast<char *>(method.val.other.ptr), method.val.other.len);
    bindings_string_free(&method.val.other);
  }
  return Result<string_view>::ok(method_);
}

Result<HttpHeaders *> HttpIncomingRequest::headers() {
  if (!headers_) {
    if (!valid()) {
      return Result<HttpHeaders *>::err(154);
    }
    borrow_incoming_request_t borrow(handle_state_->handle);
    auto res = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_headers(borrow);
    headers_ = new HttpHeaders(res.__handle);
  }

  return Result<HttpHeaders *>::ok(headers_);
}

Result<HttpIncomingBody *> HttpIncomingRequest::body() {
  if (!body_) {
    if (!valid()) {
      return Result<HttpIncomingBody *>::err(154);
    }
    incoming_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_consume(
            borrow_incoming_request_t(handle_state_->handle), &body)) {
      return Result<HttpIncomingBody *>::err(154);
    }
    body_ = new HttpIncomingBody(body.__handle);
  }
  return Result<HttpIncomingBody *>::ok(body_);
}

} // namespace host_api
