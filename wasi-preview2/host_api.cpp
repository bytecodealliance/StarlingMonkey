#include <algorithm>
#include <type_traits>

#include "allocator.h"
#include "builtins/web/fetch/request-response.h"
#include "bindings.h"

typedef uint8_t bindings_string_t_ptr_t;
typedef bindings_own_incoming_body_t http_body_t;
static_assert(sizeof(bindings_own_incoming_body_t) ==
              sizeof(bindings_own_outgoing_body_t));
typedef bindings_own_future_incoming_response_t own_pending_request_t;
typedef bindings_own_incoming_request_t own_request_t;
typedef bindings_own_incoming_response_t own_response_t;

namespace host_api {

namespace {

bindings_string_t string_view_to_world_string(std::string_view str) {
  return {
      .ptr = (bindings_string_t_ptr_t *)str.data(),
      .len = str.size(),
  };
}

HostString make_host_string(bindings_string_t str) {
  return HostString{JS::UniqueChars{(char*)str.ptr}, str.len};
}

// TODO: verify that this is correct.
HostString make_host_string(bindings_list_u8_t str) {
  return HostString{JS::UniqueChars{(char*)str.ptr}, str.len};
}

// Response make_response(fastly_compute_at_edge_http_types_response_t &resp) {
//   return Response{HttpResp{resp.f0}, HttpBody{resp.f1}};
// }

} // namespace

// The host interface makes the assumption regularly that uint32_t is sufficient space to store a
// pointer.
static_assert(sizeof(uint32_t) == sizeof(void *));

// Ensure that the handle types stay in sync with bindings.h
static_assert(sizeof(HttpBody::Handle) == sizeof(http_body_t));
static_assert(sizeof(HttpPendingReq::Handle) == sizeof(own_pending_request_t));
static_assert(sizeof(HttpReq::Handle) == sizeof(own_request_t));
static_assert(sizeof(HttpResp::Handle) == sizeof(own_response_t));

Result<std::optional<uint32_t>> AsyncHandle::select(std::vector<AsyncHandle> &handles,
                                                    uint32_t timeout_ms) {
  Result<std::optional<uint32_t>> res;

  static_assert(sizeof(AsyncHandle) == sizeof(bindings_borrow_pollable_t));

  auto count = handles.size();
  if (timeout_ms > 0) {
    // WASI clock resolution is in us.
    auto timeout = timeout_ms * 1000;
    auto timer = wasi_clocks_monotonic_clock_subscribe(timeout, false);
    count++;
    handles.push_back(static_cast<AsyncHandle>(timer.__handle));
  }
  auto handles_ptr =
      reinterpret_cast<bindings_borrow_pollable_t *>(handles.data());

  auto list = bindings_list_borrow_pollable_t{handles_ptr, count};
  bindings_list_u32_t result = {.ptr = nullptr,.len = 0};
  wasi_io_poll_poll_list(&list, &result);
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
  wasi_random_random_get_random_bytes(num_bytes, list);
  auto ret = HostBytes {
      std::unique_ptr<uint8_t[]>{list->ptr},
      list->len,
  };
  res.emplace(std::move(ret));

  return res;
}

Result<uint32_t> Random::get_u32() {
  Result<uint32_t> res;

  res.emplace(wasi_random_random_get_random_u64());

  return res;
}

Result<HttpBody> HttpBody::make(HttpResp response) {
  Result<HttpBody> res;

  bindings_own_outgoing_body_t body;
  if (!wasi_http_types_method_outgoing_response_write(
      bindings_borrow_outgoing_response_t{response.handle}, &body)) {
    // TODO: define proper error codes.
    res.emplace_err(0);
  } else {
    res.emplace(body.__handle);
  }

  return res;
}

Result<HttpBody> HttpBody::make(HttpReq request) {
  Result<HttpBody> res;

  bindings_own_outgoing_body_t body;
  if (!wasi_http_types_method_outgoing_request_write(
      bindings_borrow_outgoing_request_t{request.handle}, &body)) {
    // TODO: define proper error codes.
    res.emplace_err(0);
  } else {
    res.emplace(body.__handle);
  }

  return res;
}

Result<HostString> HttpBody::read(uint32_t chunk_size) const {
  Result<HostString> res;

  // bindings_list_u8_t ret;
  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_body_read(this->handle, chunk_size, &ret, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(JS::UniqueChars(reinterpret_cast<char *>(ret.ptr)), ret.len);
  // }

  return res;
}

Result<uint32_t> HttpBody::write(const uint8_t *ptr, size_t len) const {
  Result<uint32_t> res;

  // The write call doesn't mutate the buffer; the cast is just for the generated fastly api.
  // bindings_list_u8_t chunk{const_cast<uint8_t *>(ptr), len};

  // fastly_compute_at_edge_types_error_t err;
  // uint32_t written;
  // if (!fastly_compute_at_edge_http_body_write(
  //         this->handle, &chunk, FASTLY_COMPUTE_AT_EDGE_HTTP_BODY_WRITE_END_BACK, &written, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(written);
  // }

  return res;
}

Result<Void> HttpBody::write_all(const uint8_t *ptr, size_t len) const {
  while (len > 0) {
    auto write_res = this->write(ptr, len);
    if (auto *err = write_res.to_err()) {
      return Result<Void>::err(*err);
    }

    auto written = write_res.unwrap();
    ptr += written;
    len -= std::min(len, static_cast<size_t>(written));
  }

  return Result<Void>::ok();
}

Result<Void> HttpBody::append(HttpBody other) const {
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_body_append(this->handle, other.handle, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace();
  // }

  return res;
}

Result<Void> HttpBody::close() {
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_body_close(this->handle, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace();
  // }

  return res;
}

AsyncHandle HttpBody::async_handle() const { return AsyncHandle{this->handle}; }

namespace {

Result<std::vector<HostString>>
headers_get_names(bindings_own_headers_t headers) {
  Result<std::vector<HostString>> res;

  bindings_list_tuple2_string_list_u8_t *entries = nullptr;
  wasi_http_types_method_fields_entries(bindings_borrow_fields_t {headers.__handle}, entries);

  std::vector<HostString> names;
  for (int i = 0; i < entries->len; i++) {
    names.emplace_back(make_host_string(entries->ptr[i].f0));
  }
  // Free the outer list, but not the entries themselves.
  free(entries->ptr);
  res.emplace(std::move(names));

  return res;
}

Result<std::optional<std::vector<HostString>>>
headers_get_values(bindings_own_headers_t headers,
                          std::string_view name) {
  Result<std::optional<std::vector<HostString>>> res;

  bindings_string_t hdr = string_view_to_world_string(name);

  bindings_list_list_u8_t *values = nullptr;
  wasi_http_types_method_fields_get(bindings_borrow_fields_t{headers.__handle},
                                    &hdr, values);

  if (values->len > 0) {
    std::vector<HostString> names;
    for (int i = 0; i < values->len; i++) {
      names.emplace_back(make_host_string(values->ptr[i]));
    }
    // Free the outer list, but not the values themselves.
    free(values->ptr);
    res.emplace(std::move(names));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

template <auto header_op>
Result<Void> generic_header_op(auto handle, std::string_view name, std::string_view value) {
  Result<Void> res;

  // bindings_string_t hdr = string_view_to_world_string(name);
  // bindings_string_t val = string_view_to_world_string(value);
  // fastly_compute_at_edge_types_error_t err;
  // if (!header_op(handle, &hdr, &val, &err)) {
  //   res.emplace_err(err);
  // }

  return res;
}

template <auto remove_header>
Result<Void> generic_header_remove(auto handle, std::string_view name) {
  Result<Void> res;

  // bindings_string_t hdr = string_view_to_world_string(name);
  // fastly_compute_at_edge_types_error_t err;
  // if (!remove_header(handle, &hdr, &err)) {
  //   res.emplace_err(err);
  // }

  return res;
}

} // namespace

Result<std::optional<Response>> HttpPendingReq::poll() {
  Result<std::optional<Response>> res;

  // fastly_compute_at_edge_types_error_t err;
  // fastly_world_option_fastly_compute_at_edge_http_req_response_t ret;
  // if (!fastly_compute_at_edge_http_req_pending_req_poll(this->handle, &ret, &err)) {
  //   res.emplace_err(err);
  // } else if (ret.is_some) {
  //   res.emplace(make_response(ret.val));
  // } else {
  //   res.emplace(std::nullopt);
  // }

  return res;
}

Result<Response> HttpPendingReq::wait() {
  Result<Response> res;

  // fastly_compute_at_edge_types_error_t err;
  // fastly_compute_at_edge_http_types_response_t ret;
  // if (!fastly_compute_at_edge_http_req_pending_req_wait(this->handle, &ret, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(make_response(ret));
  // }

  return res;
}

AsyncHandle HttpPendingReq::async_handle() const { return AsyncHandle{this->handle}; }

Result<HttpReq> HttpReq::make() {
  Result<HttpReq> res;

  // own_request_t handle;
  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_req_new(&handle, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(handle);
  // }

  return res;
}

Result<Response> HttpReq::send(HttpBody body, std::string_view backend) {
  Result<Response> res;

  // fastly_compute_at_edge_types_error_t err;
  // fastly_compute_at_edge_http_types_response_t ret;
  // bindings_string_t backend_str = string_view_to_world_string(backend);
  // if (!fastly_compute_at_edge_http_req_send(this->handle, body.handle, &backend_str, &ret, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(make_response(ret));
  // }

  return res;
}

Result<HttpPendingReq> HttpReq::send_async(HttpBody body, std::string_view backend) {
  Result<HttpPendingReq> res;

  // fastly_compute_at_edge_types_error_t err;
  // own_pending_request_t ret;
  // bindings_string_t backend_str = string_view_to_world_string(backend);
  // if (!fastly_compute_at_edge_http_req_send_async(this->handle, body.handle,
  // &backend_str, &ret,
  //                                                 &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(ret);
  // }

  return res;
}

Result<HttpPendingReq> HttpReq::send_async_streaming(HttpBody body, std::string_view backend) {
  Result<HttpPendingReq> res;

  // fastly_compute_at_edge_types_error_t err;
  // own_pending_request_t ret;
  // bindings_string_t backend_str = string_view_to_world_string(backend);
  // if (!fastly_compute_at_edge_http_req_send_async_streaming(this->handle,
  // body.handle, &backend_str,
  //                                                           &ret, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(ret);
  // }

  return res;
}

Result<Void> HttpReq::set_method(std::string_view method) {
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // bindings_string_t str = string_view_to_world_string(method);
  // if (!fastly_compute_at_edge_http_req_method_set(this->handle, &str, &err)) {
  //   res.emplace_err(err);
  // }

  return res;
}

Result<HostString> HttpReq::get_method() const {
  Result<HostString> res;

  // fastly_compute_at_edge_types_error_t err;
  // bindings_string_t ret;
  // if (!fastly_compute_at_edge_http_req_method_get(this->handle, &ret, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(make_host_string(ret));
  // }

  return res;
}

Result<Void> HttpReq::set_uri(std::string_view str) {
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // bindings_string_t uri = string_view_to_world_string(str);
  // if (!fastly_compute_at_edge_http_req_uri_set(this->handle, &uri, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace();
  // }

  return res;
}

Result<HostString> HttpReq::get_uri() const {
  Result<HostString> res;

  // fastly_compute_at_edge_types_error_t err;
  // bindings_string_t uri;
  // if (!fastly_compute_at_edge_http_req_uri_get(this->handle, &uri, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(make_host_string(uri));
  // }

  return res;
}

bool HttpReq::is_valid() const { return this->handle != HttpReq::invalid; }

Result<std::vector<HostString>> HttpReq::get_header_names() {
  bindings_own_headers_t headers =
      wasi_http_types_method_incoming_request_headers(
          bindings_borrow_incoming_request_t{(int32_t)handle});
  return headers_get_names(headers);
}

Result<std::optional<std::vector<HostString>>>
HttpReq::get_header_values(std::string_view name) {
  bindings_own_headers_t headers =
      wasi_http_types_method_incoming_request_headers(
          bindings_borrow_incoming_request_t{(int32_t)handle});
  return headers_get_values(headers, name);
}

Result<Void> HttpReq::insert_header(std::string_view name,
                                    std::string_view value) {
// TODO: properly support both insert and append.
  return generic_header_op<wasi_http_types_method_incoming_request_headers>(
      this->handle, name, value);
}

Result<Void> HttpReq::append_header(std::string_view name,
                                    std::string_view value) {
// TODO: properly support both insert and append.
  return generic_header_op<wasi_http_types_method_incoming_request_headers>(
      this->handle, name, value);
}

Result<Void> HttpReq::remove_header(std::string_view name) {
  return generic_header_remove<wasi_http_types_method_incoming_request_headers>(
      this->handle, name);
}

Result<HttpResp> HttpResp::make() {
  Result<HttpResp> res;

  // own_response_t handle;
  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_resp_new(&handle, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(handle);
  // }

  return res;
}

Result<uint16_t> HttpResp::get_status() const {
  Result<uint16_t> res;

  // fastly_compute_at_edge_http_types_http_status_t ret;
  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_resp_status_get(this->handle, &ret, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace(ret);
  // }

  return res;
}

Result<Void> HttpResp::set_status(uint16_t status) {
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_resp_status_set(this->handle, status,
  //                                                  &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace();
  // }

  return res;
}

Result<Void> HttpResp::send_downstream(HttpBody body, bool streaming) {
  Result<Void> res;

  // fastly_compute_at_edge_types_error_t err;
  // if (!fastly_compute_at_edge_http_resp_send_downstream(
  //         this->handle, body.handle, streaming, &err)) {
  //   res.emplace_err(err);
  // } else {
  //   res.emplace();
  // }

  return res;
}

bool HttpResp::is_valid() const { return this->handle != HttpResp::invalid; }

Result<std::vector<HostString>> HttpResp::get_header_names() {
  bindings_own_headers_t headers =
      wasi_http_types_method_incoming_response_headers(
          bindings_borrow_incoming_response_t{(int32_t)handle});
  return headers_get_names(headers);
}

Result<std::optional<std::vector<HostString>>>
HttpResp::get_header_values(std::string_view name) {
  bindings_own_headers_t headers =
      wasi_http_types_method_incoming_response_headers(
          bindings_borrow_incoming_response_t{(int32_t)handle});
  return headers_get_values(headers, name);
}

Result<Void> HttpResp::insert_header(std::string_view name,
                                     std::string_view value) {
  // TODO: properly support both insert and append.
  return generic_header_op<wasi_http_types_method_incoming_response_headers>(
      this->handle, name, value);
}

Result<Void> HttpResp::append_header(std::string_view name,
                                     std::string_view value) {
  // TODO: properly support both insert and append.
  return generic_header_op<wasi_http_types_method_incoming_response_headers>(
      this->handle, name, value);
}

Result<Void> HttpResp::remove_header(std::string_view name) {
  return generic_header_remove<wasi_http_types_method_incoming_response_headers>(
      this->handle, name);
}
} // namespace host_api
