#include <algorithm>
#include <type_traits>

#include "host_api.h"

typedef uint8_t bindings_string_t_ptr_t;
typedef bindings_own_incoming_body_t http_body_t;
static_assert(sizeof(bindings_own_incoming_body_t) ==
              sizeof(bindings_own_outgoing_body_t));
typedef bindings_own_future_incoming_response_t own_pending_request_t;
typedef bindings_own_incoming_request_t own_request_t;
typedef bindings_own_incoming_response_t own_response_t;

namespace host_api {

HostString::HostString(const char *c_str) {
  len = strlen(c_str);
  ptr = JS::UniqueChars((char *)malloc(len + 1));
  std::memcpy(ptr.get(), c_str, len);
  ptr[len] = '\0';
}

namespace {

bindings_string_t string_view_to_world_string(std::string_view str) {
  return {
      .ptr = (bindings_string_t_ptr_t *)str.data(),
      .len = str.size(),
  };
}

bindings_list_u8_t string_view_to_world_bytes(std::string_view str) {
  return {
      .ptr = (uint8_t *)str.data(),
      .len = str.size(),
  };
}

} // namespace

// The host interface makes the assumption regularly that uint32_t is sufficient space to store a
// pointer.
static_assert(sizeof(uint32_t) == sizeof(void *));

// Ensure that the handle types stay in sync with bindings.h
static_assert(sizeof(HttpIncomingBody::Handle) == sizeof(http_body_t));
static_assert(sizeof(FutureHttpIncomingResponse::Handle) == sizeof(own_pending_request_t));
// static_assert(sizeof(HttpIncomingRequest::Handle) == sizeof(own_request_t));
// static_assert(sizeof(HttpIncomingResponse::Handle) == sizeof(own_response_t));

Result<std::optional<uint32_t>> AsyncHandle::select(std::vector<AsyncHandle> &handles,
                                                    uint32_t timeout_ms) {
  Result<std::optional<uint32_t>> res;

  static_assert(sizeof(AsyncHandle) == sizeof(bindings_borrow_pollable_t));

  auto count = handles.size();
  if (timeout_ms > 0) {
    // WASI clock resolution is in us.
    auto timeout = timeout_ms * 1000;
    auto timer = wasi_clocks_0_2_0_rc_2023_10_18_monotonic_clock_subscribe(
        timeout, false);
    count++;
    handles.push_back(AsyncHandle(timer));
  }
  auto handles_ptr =
      reinterpret_cast<bindings_borrow_pollable_t *>(handles.data());

  auto list = bindings_list_borrow_pollable_t{handles_ptr, count};
  bindings_list_u32_t result = {.ptr = nullptr,.len = 0};
  wasi_io_0_2_0_rc_2023_10_18_poll_poll_list(&list, &result);
  MOZ_ASSERT(result.len > 0);
  if (timeout_ms > 0 && result.ptr[0] == count - 1) {
    res.emplace(std::nullopt);
  } else {
    // TODO: remember all handles that are ready instead of just the first one.
    res.emplace(result.ptr[0]);
  }
  free(result.ptr);

  return res;
}

Result<HostBytes> Random::get_bytes(size_t num_bytes) {
  Result<HostBytes> res;

  bindings_list_u8_t* list = nullptr;
  wasi_random_0_2_0_rc_2023_10_18_random_get_random_bytes(num_bytes, list);
  auto ret = HostBytes {
      std::unique_ptr<uint8_t[]>{list->ptr},
      list->len,
  };
  res.emplace(std::move(ret));

  return res;
}

Result<uint32_t> Random::get_u32() {
  Result<uint32_t> res;

  res.emplace(wasi_random_0_2_0_rc_2023_10_18_random_get_random_u64());

  return res;
}

using std::optional;
using std::string_view;
using std::tuple;
using std::unique_ptr;
using std::vector;

HttpHeaders::HttpHeaders() {
  auto entries =
      bindings_list_tuple2_string_list_u8_t{
          nullptr, 0};
  this->handle = wasi_http_0_2_0_rc_2023_10_18_types_constructor_fields(&entries);
}

HttpHeaders::HttpHeaders(
    vector<tuple<HostString, vector<HostString>>> entries) {
}

HttpHeaders::HttpHeaders(const HttpHeaders &headers) {
}

Result<vector<tuple<HostString, HostString>>> HttpHeaders::entries() const {
  Result<vector<tuple<HostString, HostString>>> res;
  MOZ_ASSERT(valid());

  bindings_list_tuple2_string_list_u8_t entries;
  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields(handle);
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_entries(borrow, &entries);

  vector<tuple<HostString, HostString>> entries_vec;
  for (int i = 0; i < entries.len; i++) {
    auto key = entries.ptr[i].f0;
    auto value = entries.ptr[i].f1;
    entries_vec.emplace_back(tuple(HostString(key), HostString(value)));
  }
  // Free the outer list, but not the entries themselves.
  free(entries.ptr);
  res.emplace(std::move(entries_vec));

  return res;
}

Result<vector<HostString>> HttpHeaders::names() const {
  Result<vector<HostString>> res;
  MOZ_ASSERT(valid());

  bindings_list_tuple2_string_list_u8_t entries;
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_entries(wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields(handle),
                                        &entries);

  vector<HostString> names;
  for (int i = 0; i < entries.len; i++) {
    names.emplace_back(HostString(entries.ptr[i].f0));
  }
  // Free the outer list, but not the entries themselves.
  free(entries.ptr);
  res.emplace(std::move(names));

  return res;
}

Result<optional<vector<HostString>>> HttpHeaders::get(string_view name) const {
  Result<optional<vector<HostString>>> res;
  MOZ_ASSERT(valid());

  bindings_list_list_u8_t values;
  auto hdr = string_view_to_world_string(name);
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_get(wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields(handle), &hdr,
                                    &values);

  if (values.len > 0) {
    std::vector<HostString> names;
    for (int i = 0; i < values.len; i++) {
      auto value = values.ptr[i];
      names.emplace_back(HostString(value));
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
  auto value_bytes = string_view_to_world_bytes(value);
  auto fieldval = bindings_list_u8_t{value_bytes.ptr, value_bytes.len};

  bindings_list_list_u8_t host_values = {&fieldval, 1};

  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields(handle);
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_set(borrow, &hdr, &host_values);
  free(host_values.ptr);

  return Result<Void>();
}

Result<Void> HttpHeaders::append(string_view name, string_view value) {
  MOZ_ASSERT(valid());
  auto hdr = string_view_to_world_string(name);
  auto bytes = string_view_to_world_bytes(value);

  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields(handle);
  auto fieldval = bindings_list_u8_t{bytes.ptr, bytes.len};
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_append(borrow, &hdr, &fieldval);

  return Result<Void>();
}

Result<Void> HttpHeaders::remove(string_view name) {
  MOZ_ASSERT(valid());
  auto hdr = string_view_to_world_string(name);

  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_fields(handle);
  wasi_http_0_2_0_rc_2023_10_18_types_method_fields_delete(borrow, &hdr);

  return Result<Void>();
}

const optional<string_view> HttpRequestResponseBase::url() {
  ensure_url();
  if (_url) {
    return string_view(*_url);
  }
  return std::nullopt;
}

Result<tuple<bindings_borrow_output_stream_t, uint64_t>>
HttpOutgoingBody::ensure_stream() {
  MOZ_ASSERT(valid());
  typedef Result<tuple<bindings_borrow_output_stream_t, uint64_t>> Res;

  if (stream.__handle == invalid_stream.__handle) {
    auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_body(handle);
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_outgoing_body_write(borrow, &this->stream)) {
      return Res::err(154);
    }
  }

  MOZ_ASSERT(stream.__handle != invalid_stream.__handle);

  auto borrow = wasi_io_0_2_0_rc_2023_10_18_streams_borrow_output_stream(stream);
  uint64_t capacity = 0;
  wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t err;
  if (!wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_check_write(borrow, &capacity, &err)) {
    return Res::err(154);
  }
  return Res::ok(std::make_tuple(borrow, capacity));
}

Result<tuple<bindings_borrow_output_stream_t, uint64_t>>
HttpOutgoingBody::ensure_stream_with_capacity() {
  MOZ_ASSERT(valid());
  typedef Result<tuple<bindings_borrow_output_stream_t, uint64_t>> Res;
  auto res = ensure_stream();
  if (res.is_err()) {
    return res;
  }
  auto [borrow, capacity] = res.unwrap();

  if (capacity == 0) {
    wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t err;
    auto pollable = wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_subscribe(borrow);
    wasi_io_0_2_0_rc_2023_10_18_poll_poll_one(wasi_io_0_2_0_rc_2023_10_18_poll_borrow_pollable(pollable));
    if (!wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_check_write(borrow, &capacity,
                                                          &err)) {
      return Res::err(154);
    }
  }

  MOZ_ASSERT(capacity > 0);

  return Res::ok(std::make_tuple(borrow, capacity));
}

bool write_to_outgoing_body(bindings_borrow_output_stream_t borrow,
                            const uint8_t *ptr, size_t len) {
  // The write call doesn't mutate the buffer; the cast is just for the
  // generated bindings.
  bindings_list_u8_t list{const_cast<uint8_t *>(ptr), len};
  wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t err;
  // TODO: proper error handling.
  return wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_write(borrow, &list, &err);
}

Result<uint32_t> HttpOutgoingBody::write(const uint8_t *ptr, size_t len) {
  MOZ_ASSERT(valid());
  auto res = ensure_stream();
  if (res.is_err()) {
    // TODO: proper error handling for all 154 error codes.
      return Result<uint32_t>::err(154);
  }
  auto [borrow, capacity] = res.unwrap();

  len = std::min(len, size_t(capacity));
  if (!write_to_outgoing_body(borrow, ptr, len)) {
    return Result<uint32_t>::err(154);
  }

  return Result<uint32_t>::ok(len);
}

Result<Void> HttpOutgoingBody::write_all(const uint8_t *bytes, size_t len) {
  MOZ_ASSERT(valid());
  while (len > 0) {
    auto res = ensure_stream_with_capacity();
    if (res.is_err()) {
      // TODO: proper error handling for all 154 error codes.
      return Result<Void>::err(154);
    }
    auto [borrow, capacity] = res.unwrap();
    auto bytes_to_write = std::min(len, static_cast<size_t>(capacity));
    if (!write_to_outgoing_body(borrow, bytes, len)) {
      return Result<Void>::err(154);
    }

    bytes += bytes_to_write;
    len -= bytes_to_write;
  }

  return Result<Void>::ok();
}

Result<bindings_borrow_input_stream_t> HttpIncomingBody::ensure_stream() {
  typedef Result<bindings_borrow_input_stream_t> Res;
  MOZ_ASSERT(valid());

  if (stream.__handle == invalid_stream.__handle) {
    auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_body(handle);
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_body_stream(borrow, &this->stream)) {
      return Res::err(154);
    }
  }

  MOZ_ASSERT(stream.__handle != invalid_stream.__handle);

  return Res::ok(wasi_io_0_2_0_rc_2023_10_18_streams_borrow_input_stream(stream));
}

Result<Void> HttpOutgoingBody::append(HttpIncomingBody *other) {
  MOZ_ASSERT(valid());
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_body_append(this->handle, other.handle,
  // &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace();
  // }

  return res;
}

Result<Void> HttpOutgoingBody::close() {
  MOZ_ASSERT(valid());
  MOZ_ASSERT(!closed());

  wasi_http_0_2_0_rc_2023_10_18_types_static_outgoing_body_finish(handle, nullptr);
  handle = invalid;
  stream = invalid_stream;

  return Result<Void>();
}

bool HttpOutgoingBody::closed() { return _closed; }

AsyncHandle HttpOutgoingBody::async_handle() {
  MOZ_ASSERT(valid());
  auto res = ensure_stream();
  // TODO: proper error handling
  MOZ_ASSERT(!res.is_err());
  auto [borrow, _] = res.unwrap();
  auto handle = wasi_io_0_2_0_rc_2023_10_18_streams_method_output_stream_subscribe(borrow);
  return AsyncHandle{handle};
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

string_view http_method_from_host(wasi_http_0_2_0_rc_2023_10_18_types_method_t method) {
  if (method.tag != WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_METHOD_OTHER) {
    return string_view(http_method_names[method.tag]);
  }
  return string_view(reinterpret_cast<char *>(method.val.other.ptr), method.val.other.len);
}

HttpOutgoingRequest::HttpOutgoingRequest(string_view method_str,
                                         optional<HostString> url_str,
                                         HttpHeaders *headers) {
  bindings_string_t path_with_query;
  wasi_http_0_2_0_rc_2023_10_18_types_scheme_t scheme;
  bindings_string_t authority;

  bindings_string_t *maybe_path_with_query = nullptr;
  wasi_http_0_2_0_rc_2023_10_18_types_scheme_t *maybe_scheme = nullptr;
  bindings_string_t *maybe_authority = nullptr;

  if (url_str) {
    jsurl::SpecString val = url_str.value();
    jsurl::JSUrl* url = jsurl::new_jsurl(&val);
    jsurl::SpecSlice protocol = jsurl::protocol(url);
    if (std::memcmp(protocol.data, "http:", protocol.len) == 0) {
      scheme.tag = WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTP;
    } else if (std::memcmp(protocol.data, "https:", protocol.len) == 0) {
      scheme.tag = WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTPS;
    } else {
      scheme = wasi_http_0_2_0_rc_2023_10_18_types_scheme_t{WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_OTHER, {const_cast<uint8_t*>(protocol.data), protocol.len - 1}};
    }
    maybe_scheme = &scheme;

    jsurl::SpecSlice authority_slice = jsurl::authority(url);
    authority = {const_cast<uint8_t*>(authority_slice.data), authority_slice.len};
    maybe_authority = &authority;

    jsurl::SpecSlice path_with_query_slice = jsurl::path_with_query(url);
    path_with_query = {const_cast<uint8_t*>(path_with_query_slice.data), path_with_query_slice.len};
    maybe_path_with_query = &path_with_query;
  }

  wasi_http_0_2_0_rc_2023_10_18_types_method_t method = http_method_to_host(method_str);
  handle = wasi_http_0_2_0_rc_2023_10_18_types_constructor_outgoing_request(&method, maybe_path_with_query,
                                                        maybe_scheme, maybe_authority,
                                                        headers->borrow());
}

HttpHeaders *HttpOutgoingRequest::headers() {
  MOZ_ASSERT(valid());
  MOZ_ASSERT(headers_handle);
  return headers_handle;
}

Result<HttpOutgoingBody *> HttpOutgoingRequest::body() {
  typedef Result<HttpOutgoingBody *> Res;
  MOZ_ASSERT(valid());
  if (!this->body_handle) {
    bindings_own_outgoing_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_outgoing_request_write(
         wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_request(this->handle), &body)) {
      return Res::err(154);
    }
    this->body_handle = new HttpOutgoingBody(body);
  }
  return Res::ok(this->body_handle);
}

Result<FutureHttpIncomingResponse*> HttpOutgoingRequest::send() {
  MOZ_ASSERT(valid());
  bindings_own_future_incoming_response_t ret;
  wasi_http_0_2_0_rc_2023_10_18_outgoing_handler_error_t err;
  wasi_http_0_2_0_rc_2023_10_18_outgoing_handler_handle(handle, nullptr, &ret, &err);
  auto res = new FutureHttpIncomingResponse(ret);
  return Result<FutureHttpIncomingResponse*>::ok(res);
}

Result<tuple<HostString, bool>> HttpIncomingBody::read(uint32_t chunk_size, bool blocking) {
  typedef Result<tuple<HostString, bool>> Res;
  auto res = ensure_stream();
  // TODO: proper error handling
  MOZ_ASSERT(!res.is_err());
  auto stream = res.unwrap();

  auto ret = bindings_list_u8_t {};
  auto err = wasi_io_0_2_0_rc_2023_10_18_streams_stream_error_t {};
  bool success;
  if (blocking) {
    success = wasi_io_0_2_0_rc_2023_10_18_streams_method_input_stream_blocking_read(
        stream, chunk_size, &ret, &err);
  } else {
    success = wasi_io_0_2_0_rc_2023_10_18_streams_method_input_stream_read(
        stream, chunk_size, &ret, &err);
  }
  if (!success) {
    if (err.tag == WASI_IO_0_2_0_RC_2023_10_18_STREAMS_STREAM_ERROR_CLOSED) {
      return Res::ok(tuple(HostString(), true));
    }
    return Res::err(154);
  }
  return Res::ok(tuple(HostString(ret), false));
}

Result<Void> HttpIncomingBody::close() { return Result<Void>(); }

AsyncHandle HttpIncomingBody::async_handle() {
  MOZ_ASSERT(valid());
  auto res = ensure_stream();
  // TODO: proper error handling
  MOZ_ASSERT(!res.is_err());
  auto borrow = res.unwrap();
  auto handle = wasi_io_0_2_0_rc_2023_10_18_streams_method_input_stream_subscribe(borrow);
  return AsyncHandle{handle};
}

Result<optional<HttpIncomingResponse *>> FutureHttpIncomingResponse::poll() {
  typedef Result<optional<HttpIncomingResponse *>> Res;
  bindings_result_result_own_incoming_response_wasi_http_0_2_0_rc_2023_10_18_types_error_void_t
      res;
  if (!wasi_http_0_2_0_rc_2023_10_18_types_method_future_incoming_response_get(
      wasi_http_0_2_0_rc_2023_10_18_types_borrow_future_incoming_response(handle), &res)) {
    return Res::ok(std::nullopt);
  }

  MOZ_ASSERT(!res.is_err, "FutureHttpIncomingResponse::poll must not be called again after succeeding once");

  auto inner = res.val.ok;
  if (inner.is_err) {
    return Res::err(154);
  }

  auto handle = inner.val.ok;
  return Res::ok(new HttpIncomingResponse(handle));
}

AsyncHandle FutureHttpIncomingResponse::async_handle() {
  if (!pollable.valid()) {
    auto async_handle = wasi_http_0_2_0_rc_2023_10_18_types_method_future_incoming_response_subscribe(wasi_http_0_2_0_rc_2023_10_18_types_borrow_future_incoming_response(handle));
    pollable.handle.__handle = async_handle.__handle;
  }
  return pollable;
}

uint16_t HttpIncomingResponse::status() const {
  return wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_response_status(
      wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_response(handle));
}

HttpHeaders *HttpIncomingResponse::headers() {
  MOZ_ASSERT(valid());
  if (!headers_handle) {
    auto res = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_response_headers(
        wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_response(handle));
    headers_handle = new HttpHeaders(res);
  }

  return headers_handle;
}

Result<HttpIncomingBody *> HttpIncomingResponse::body() {
  MOZ_ASSERT(valid());
  if (!body_handle) {
    bindings_own_incoming_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_response_consume(
        wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_response(handle), &body)) {
      return Result<HttpIncomingBody *>::err(154);
    }
    body_handle = new HttpIncomingBody(body);
  }
  return Result<HttpIncomingBody *>::ok(body_handle);
}

HttpOutgoingResponse::HttpOutgoingResponse(uint16_t status,
                                           HttpHeaders *headers) : status(status) {
  handle = wasi_http_0_2_0_rc_2023_10_18_types_constructor_outgoing_response(status, headers->borrow());
}

HttpHeaders *HttpOutgoingResponse::headers() {
  MOZ_ASSERT(valid());
  MOZ_ASSERT(headers_handle);
  return headers_handle;
}

Result<HttpOutgoingBody *> HttpOutgoingResponse::body() {
  typedef Result<HttpOutgoingBody *> Res;
  MOZ_ASSERT(valid());
  if (!this->body_handle) {
    bindings_own_outgoing_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_outgoing_response_write(
         wasi_http_0_2_0_rc_2023_10_18_types_borrow_outgoing_response(this->handle), &body)) {
      return Res::err(154);
    }
    this->body_handle = new HttpOutgoingBody(body);
  }
  return Res::ok(this->body_handle);
}

Result<Void> HttpOutgoingResponse::send(ResponseOutparam *out_param) {
  auto result =
      bindings_result_own_outgoing_response_wasi_http_0_2_0_rc_2023_10_18_types_error_t{
          false, {this->handle}};
  wasi_http_0_2_0_rc_2023_10_18_types_static_response_outparam_set(*out_param,
                                                                   &result);
  return Result<Void>();
}

HostString* scheme_to_string(wasi_http_0_2_0_rc_2023_10_18_types_scheme_t scheme) {
  if (scheme.tag == WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTP) {
    return new HostString("http:");
  } else if (scheme.tag == WASI_HTTP_0_2_0_RC_2023_10_18_TYPES_SCHEME_HTTPS) {
    return new HostString("https:");
  } else {
    return new HostString(scheme.val.other);
  }
}

void HttpIncomingRequest::ensure_url() {
  // TODO: don't attempt to get the URL again and again in case it's not available.
  if (_url) {
    return;
  }
  auto borrow = wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_request(handle);

  wasi_http_0_2_0_rc_2023_10_18_types_scheme_t scheme;
  if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_scheme(
          borrow, &scheme)) {
    return;
  }

  bindings_string_t authority;
  if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_authority(
          borrow, &authority)) {
    return;
  }

  bindings_string_t path;
  if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_path_with_query(
          borrow, &path)) {
    return;
  }
  HostString *scheme_str = scheme_to_string(scheme);

  _url = new std::string(scheme_str->ptr.release(), scheme_str->len);
  _url->append(string_view(HostString(authority)));
  _url->append(string_view(HostString(path)));
}

const string_view HttpIncomingRequest::method() const {
  wasi_http_0_2_0_rc_2023_10_18_types_method_t method;
  wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_method(
      wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_request(handle), &method);
  return http_method_from_host(method);
}

HttpHeaders *HttpIncomingRequest::headers() {
  MOZ_ASSERT(valid());
  if (!headers_handle) {
    auto res = wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_headers(
        wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_request(handle));
    headers_handle = new HttpHeaders(res);
  }

  return headers_handle;
}

Result<HttpIncomingBody *> HttpIncomingRequest::body() {
  MOZ_ASSERT(valid());
  if (!body_handle) {
    bindings_own_incoming_body_t body;
    if (!wasi_http_0_2_0_rc_2023_10_18_types_method_incoming_request_consume(
            wasi_http_0_2_0_rc_2023_10_18_types_borrow_incoming_request(handle), &body)) {
      return Result<HttpIncomingBody *>::err(154);
    }
    body_handle = new HttpIncomingBody(body);
  }
  return Result<HttpIncomingBody *>::ok(body_handle);
}

} // namespace host_api
