#include <algorithm>
#include <type_traits>

#include "builtins/cache-override.h"
#include "core/allocator.h"
#include "host_interface/component/fastly_world.h"
#include "host_interface/fastly.h"
#include "host_interface/host_api.h"
#include "js-compute-builtins.h"

namespace host_api {

namespace {

fastly_world_list_u8_t span_to_list_u8(std::span<uint8_t> span) {
  return {
      .ptr = const_cast<uint8_t *>(span.data()),
      .len = span.size(),
  };
}

fastly_world_string_t string_view_to_world_string(std::string_view str) {
  return {
      .ptr = const_cast<char *>(str.data()),
      .len = str.size(),
  };
}

HostString make_host_string(fastly_world_string_t str) {
  return HostString{JS::UniqueChars{str.ptr}, str.len};
}

HostBytes make_host_bytes(fastly_world_list_u8_t str) {
  return HostBytes{std::unique_ptr<uint8_t[]>{str.ptr}, str.len};
}

Response make_response(fastly_compute_at_edge_http_types_response_t &resp) {
  return Response{HttpResp{resp.f0}, HttpBody{resp.f1}};
}

} // namespace

// The host interface makes the assumption regularly that uint32_t is sufficient space to store a
// pointer.
static_assert(sizeof(uint32_t) == sizeof(void *));

// Ensure that the handle types stay in sync with fastly-world.h
static_assert(std::is_same_v<AsyncHandle::Handle, fastly_compute_at_edge_async_io_handle_t>);
static_assert(std::is_same_v<HttpBody::Handle, fastly_compute_at_edge_http_types_body_handle_t>);
static_assert(std::is_same_v<HttpPendingReq::Handle,
                             fastly_compute_at_edge_http_types_pending_request_handle_t>);
static_assert(std::is_same_v<HttpReq::Handle, fastly_compute_at_edge_http_types_request_handle_t>);
static_assert(
    std::is_same_v<HttpResp::Handle, fastly_compute_at_edge_http_types_response_handle_t>);
static_assert(std::is_same_v<LogEndpoint::Handle, fastly_compute_at_edge_log_handle_t>);
static_assert(std::is_same_v<Dict::Handle, fastly_compute_at_edge_dictionary_handle_t>);
static_assert(std::is_same_v<ObjectStore::Handle, fastly_compute_at_edge_object_store_handle_t>);
static_assert(std::is_same_v<HttpVersion, fastly_compute_at_edge_http_types_http_version_t>);
static_assert(std::is_same_v<typeof(CacheOverrideTag::value),
                             fastly_compute_at_edge_http_req_cache_override_tag_t>);
static_assert(
    std::is_same_v<typeof(TlsVersion::value), fastly_compute_at_edge_http_types_tls_version_t>);
static_assert(std::is_same_v<CacheHandle::Handle, fastly_compute_at_edge_cache_handle_t>);

static_assert(
    std::is_same_v<typeof(CacheState::state), fastly_compute_at_edge_cache_lookup_state_t>);

Result<bool> AsyncHandle::is_ready() const {
  Result<bool> res;

  fastly_compute_at_edge_types_error_t err;
  bool ret;
  if (!fastly_compute_at_edge_async_io_is_ready(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<std::optional<uint32_t>> AsyncHandle::select(const std::vector<AsyncHandle> &handles,
                                                    uint32_t timeout_ms) {
  Result<std::optional<uint32_t>> res;

  static_assert(sizeof(AsyncHandle) == sizeof(fastly_compute_at_edge_async_io_handle_t));
  fastly_world_list_fastly_compute_at_edge_async_io_handle_t hs{
      .ptr = reinterpret_cast<fastly_compute_at_edge_async_io_handle_t *>(
          const_cast<AsyncHandle *>(handles.data())),
      .len = handles.size()};
  fastly_world_option_u32_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_async_io_select(&hs, timeout_ms, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(ret.val);
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<HttpBody> HttpBody::make() {
  Result<HttpBody> res;

  fastly_compute_at_edge_http_types_body_handle_t handle;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_body_new(&handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(handle);
  }

  return res;
}

Result<HostString> HttpBody::read(uint32_t chunk_size) const {
  Result<HostString> res;

  fastly_world_list_u8_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_body_read(this->handle, chunk_size, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(JS::UniqueChars(reinterpret_cast<char *>(ret.ptr)), ret.len);
  }

  return res;
}

Result<uint32_t> HttpBody::write(const uint8_t *ptr, size_t len) const {
  Result<uint32_t> res;

  // The write call doesn't mutate the buffer; the cast is just for the generated fastly api.
  fastly_world_list_u8_t chunk{const_cast<uint8_t *>(ptr), len};

  fastly_compute_at_edge_types_error_t err;
  uint32_t written;
  if (!fastly_compute_at_edge_http_body_write(
          this->handle, &chunk, FASTLY_COMPUTE_AT_EDGE_HTTP_BODY_WRITE_END_BACK, &written, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(written);
  }

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

  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_body_append(this->handle, other.handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<Void> HttpBody::close() {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_body_close(this->handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

AsyncHandle HttpBody::async_handle() const { return AsyncHandle{this->handle}; }

namespace {

template <auto header_names_get>
Result<std::vector<HostString>> generic_get_header_names(auto handle) {
  Result<std::vector<HostString>> res;

  fastly_world_list_string_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!header_names_get(handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    std::vector<HostString> names;

    for (int i = 0; i < ret.len; i++) {
      names.emplace_back(make_host_string(ret.ptr[i]));
    }

    // Free the vector of string pointers, but leave the individual strings alone.
    cabi_free(ret.ptr);

    res.emplace(std::move(names));
  }

  return res;
}

template <auto header_values_get>
Result<std::optional<std::vector<HostString>>> generic_get_header_values(auto handle,
                                                                         std::string_view name) {
  Result<std::optional<std::vector<HostString>>> res;

  fastly_world_string_t hdr = string_view_to_world_string(name);
  fastly_world_option_list_string_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!header_values_get(handle, &hdr, &ret, &err)) {
    res.emplace_err(err);
  } else {

    if (ret.is_some) {
      std::vector<HostString> names;

      for (int i = 0; i < ret.val.len; i++) {
        names.emplace_back(make_host_string(ret.val.ptr[i]));
      }

      // Free the vector of string pointers, but leave the individual strings alone.
      cabi_free(ret.val.ptr);

      res.emplace(std::move(names));
    } else {
      res.emplace(std::nullopt);
    }
  }

  return res;
}

template <auto header_op>
Result<Void> generic_header_op(auto handle, std::string_view name, std::string_view value) {
  Result<Void> res;

  fastly_world_string_t hdr = string_view_to_world_string(name);
  fastly_world_string_t val = string_view_to_world_string(value);
  fastly_compute_at_edge_types_error_t err;
  if (!header_op(handle, &hdr, &val, &err)) {
    res.emplace_err(err);
  }

  return res;
}

template <auto remove_header>
Result<Void> generic_header_remove(auto handle, std::string_view name) {
  Result<Void> res;

  fastly_world_string_t hdr = string_view_to_world_string(name);
  fastly_compute_at_edge_types_error_t err;
  if (!remove_header(handle, &hdr, &err)) {
    res.emplace_err(err);
  }

  return res;
}

} // namespace

Result<std::optional<Response>> HttpPendingReq::poll() {
  Result<std::optional<Response>> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_option_fastly_compute_at_edge_http_req_response_t ret;
  if (!fastly_compute_at_edge_http_req_pending_req_poll(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(make_response(ret.val));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<Response> HttpPendingReq::wait() {
  Result<Response> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_response_t ret;
  if (!fastly_compute_at_edge_http_req_pending_req_wait(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_response(ret));
  }

  return res;
}

AsyncHandle HttpPendingReq::async_handle() const { return AsyncHandle{this->handle}; }

void CacheOverrideTag::set_pass() {
  this->value |= FASTLY_COMPUTE_AT_EDGE_HTTP_REQ_CACHE_OVERRIDE_TAG_PASS;
}

void CacheOverrideTag::set_ttl() {
  this->value |= FASTLY_COMPUTE_AT_EDGE_HTTP_REQ_CACHE_OVERRIDE_TAG_TTL;
}

void CacheOverrideTag::set_stale_while_revalidate() {
  this->value |= FASTLY_COMPUTE_AT_EDGE_HTTP_REQ_CACHE_OVERRIDE_TAG_STALE_WHILE_REVALIDATE;
}

void CacheOverrideTag::set_pci() {
  this->value |= FASTLY_COMPUTE_AT_EDGE_HTTP_REQ_CACHE_OVERRIDE_TAG_PCI;
}

TlsVersion::TlsVersion(uint8_t raw) : value{raw} {
  switch (raw) {
  case FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS1:
  case FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS11:
  case FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS12:
  case FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS13:
    break;

  default:
    MOZ_ASSERT(false, "Making a TlsValue from an invalid raw value");
  }
}

TlsVersion TlsVersion::version_1() {
  return TlsVersion{FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS1};
}

TlsVersion TlsVersion::version_1_1() {
  return TlsVersion{FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS11};
}

TlsVersion TlsVersion::version_1_2() {
  return TlsVersion{FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS12};
}

TlsVersion TlsVersion::version_1_3() {
  return TlsVersion{FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_TLS_VERSION_TLS13};
}

Result<HttpReq> HttpReq::make() {
  Result<HttpReq> res;

  fastly_compute_at_edge_http_types_request_handle_t handle;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_new(&handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(handle);
  }

  return res;
}

Result<Void> HttpReq::redirect_to_grip_proxy(std::string_view backend) {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t backend_str = string_view_to_world_string(backend);
  if (!fastly_compute_at_edge_http_req_redirect_to_grip_proxy(&backend_str, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<Void> HttpReq::auto_decompress_gzip() {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_content_encodings_t encodings_to_decompress = 0;
  encodings_to_decompress |= FASTLY_COMPUTE_AT_EDGE_HTTP_TYPES_CONTENT_ENCODINGS_GZIP;
  if (!fastly_compute_at_edge_http_req_auto_decompress_response_set(
          this->handle, encodings_to_decompress, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<Void> HttpReq::register_dynamic_backend(std::string_view name, std::string_view target,
                                               const BackendConfig &config) {
  Result<Void> res;

  fastly_compute_at_edge_http_types_dynamic_backend_config_t backend_config;
  memset(&backend_config, 0, sizeof(backend_config));

  if (auto &val = config.host_override) {
    backend_config.host_override.is_some = true;
    backend_config.host_override.val = string_view_to_world_string(*val);
  }

  if (auto &val = config.connect_timeout) {
    backend_config.connect_timeout.is_some = true;
    backend_config.connect_timeout.val = *val;
  }

  if (auto &val = config.first_byte_timeout) {
    backend_config.first_byte_timeout.is_some = true;
    backend_config.first_byte_timeout.val = *val;
  }

  if (auto &val = config.between_bytes_timeout) {
    backend_config.between_bytes_timeout.is_some = true;
    backend_config.between_bytes_timeout.val = *val;
  }

  if (auto &val = config.use_ssl) {
    backend_config.use_ssl.is_some = true;
    backend_config.use_ssl.val = *val;
  }

  if (auto &val = config.dont_pool) {
    backend_config.dont_pool.is_some = true;
    backend_config.dont_pool.val = *val;
  }

  if (auto &val = config.ssl_min_version) {
    backend_config.ssl_min_version.is_some = true;
    backend_config.ssl_min_version.val = val->value;
  }

  if (auto &val = config.ssl_max_version) {
    backend_config.ssl_max_version.is_some = true;
    backend_config.ssl_max_version.val = val->value;
  }

  if (auto &val = config.cert_hostname) {
    backend_config.cert_hostname.is_some = true;
    backend_config.cert_hostname.val = string_view_to_world_string(*val);
  }

  if (auto &val = config.ca_cert) {
    backend_config.ca_cert.is_some = true;
    backend_config.ca_cert.val = string_view_to_world_string(*val);
  }

  if (auto &val = config.ciphers) {
    backend_config.ciphers.is_some = true;
    backend_config.ciphers.val = string_view_to_world_string(*val);
  }

  if (auto &val = config.sni_hostname) {
    backend_config.sni_hostname.is_some = true;
    backend_config.sni_hostname.val = string_view_to_world_string(*val);
  }

  auto name_str = string_view_to_world_string(name);
  auto target_str = string_view_to_world_string(target);
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_register_dynamic_backend(&name_str, &target_str,
                                                                &backend_config, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<Response> HttpReq::send(HttpBody body, std::string_view backend) {
  Result<Response> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_response_t ret;
  fastly_world_string_t backend_str = string_view_to_world_string(backend);
  if (!fastly_compute_at_edge_http_req_send(this->handle, body.handle, &backend_str, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_response(ret));
  }

  return res;
}

Result<HttpPendingReq> HttpReq::send_async(HttpBody body, std::string_view backend) {
  Result<HttpPendingReq> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_pending_request_handle_t ret;
  fastly_world_string_t backend_str = string_view_to_world_string(backend);
  if (!fastly_compute_at_edge_http_req_send_async(this->handle, body.handle, &backend_str, &ret,
                                                  &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<HttpPendingReq> HttpReq::send_async_streaming(HttpBody body, std::string_view backend) {
  Result<HttpPendingReq> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_pending_request_handle_t ret;
  fastly_world_string_t backend_str = string_view_to_world_string(backend);
  if (!fastly_compute_at_edge_http_req_send_async_streaming(this->handle, body.handle, &backend_str,
                                                            &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<Void> HttpReq::set_method(std::string_view method) {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t str = string_view_to_world_string(method);
  if (!fastly_compute_at_edge_http_req_method_set(this->handle, &str, &err)) {
    res.emplace_err(err);
  }

  return res;
}

Result<HostString> HttpReq::get_method() const {
  Result<HostString> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t ret;
  if (!fastly_compute_at_edge_http_req_method_get(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_string(ret));
  }

  return res;
}

Result<Void> HttpReq::set_uri(std::string_view str) {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t uri = string_view_to_world_string(str);
  if (!fastly_compute_at_edge_http_req_uri_set(this->handle, &uri, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<HostString> HttpReq::get_uri() const {
  Result<HostString> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t uri;
  if (!fastly_compute_at_edge_http_req_uri_get(this->handle, &uri, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_string(uri));
  }

  return res;
}

Result<Void> HttpReq::cache_override(CacheOverrideTag tag, std::optional<uint32_t> opt_ttl,
                                     std::optional<uint32_t> opt_swr,
                                     std::optional<std::string_view> opt_sk) {
  Result<Void> res;

  uint32_t *ttl = nullptr;
  if (opt_ttl.has_value()) {
    ttl = &opt_ttl.value();
  }

  uint32_t *swr = nullptr;
  if (opt_swr.has_value()) {
    swr = &opt_swr.value();
  }

  fastly_world_string_t sk{nullptr, 0};
  if (opt_sk.has_value()) {
    sk = string_view_to_world_string(opt_sk.value());
  }

  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_cache_override_set(this->handle, tag.value, ttl, swr, &sk,
                                                          &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<HostBytes> HttpReq::downstream_client_ip_addr() {
  Result<HostBytes> res;

  fastly_world_list_u8_t octets;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_downstream_client_ip_addr(&octets, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_bytes(octets));
  }

  return res;
}

// http-req-downstream-tls-cipher-openssl-name: func() -> result<string, error>
Result<HostString> HttpReq::http_req_downstream_tls_cipher_openssl_name() {
  Result<HostString> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t ret;
  if (!fastly_compute_at_edge_http_req_downstream_tls_cipher_openssl_name(&ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_string(ret));
  }

  return res;
}

// http-req-downstream-tls-protocol: func() -> result<string, error>
Result<HostString> HttpReq::http_req_downstream_tls_protocol() {
  Result<HostString> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_world_string_t ret;
  if (!fastly_compute_at_edge_http_req_downstream_tls_protocol(&ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_string(ret));
  }

  return res;
}

// http-req-downstream-tls-client-hello: func() -> result<list<u8>, error>
Result<HostBytes> HttpReq::http_req_downstream_tls_client_hello() {
  Result<HostBytes> res;

  fastly_world_list_u8_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_downstream_tls_client_hello(&ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_bytes(ret));
  }

  return res;
}

// http-req-downstream-tls-raw-client-certificate: func() -> result<list<u8>, error>
Result<HostBytes> HttpReq::http_req_downstream_tls_raw_client_certificate() {
  Result<HostBytes> res;

  fastly_world_list_u8_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_downstream_tls_raw_client_certificate(&ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_bytes(ret));
  }

  return res;
}

// http-req-downstream-tls-ja3-md5: func() -> result<list<u8>, error>
Result<HostBytes> HttpReq::http_req_downstream_tls_ja3_md5() {
  Result<HostBytes> res;

  fastly_world_list_u8_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_req_downstream_tls_ja3_md5(&ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_bytes(ret));
  }

  return res;
}

bool HttpReq::is_valid() const { return this->handle != HttpReq::invalid; }

Result<fastly_compute_at_edge_http_types_http_version_t> HttpReq::get_version() const {
  Result<fastly_compute_at_edge_http_types_http_version_t> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_http_version_t ret;
  if (!fastly_compute_at_edge_http_req_version_get(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<std::vector<HostString>> HttpReq::get_header_names() {
  return generic_get_header_names<fastly_compute_at_edge_http_req_header_names_get>(this->handle);
}

Result<std::optional<std::vector<HostString>>> HttpReq::get_header_values(std::string_view name) {
  return generic_get_header_values<fastly_compute_at_edge_http_req_header_values_get>(this->handle,
                                                                                      name);
}

Result<Void> HttpReq::insert_header(std::string_view name, std::string_view value) {
  return generic_header_op<fastly_compute_at_edge_http_req_header_insert>(this->handle, name,
                                                                          value);
}

Result<Void> HttpReq::append_header(std::string_view name, std::string_view value) {
  return generic_header_op<fastly_compute_at_edge_http_req_header_append>(this->handle, name,
                                                                          value);
}

Result<Void> HttpReq::remove_header(std::string_view name) {
  return generic_header_remove<fastly_compute_at_edge_http_req_header_remove>(this->handle, name);
}

Result<HttpResp> HttpResp::make() {
  Result<HttpResp> res;

  fastly_compute_at_edge_http_types_response_handle_t handle;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_resp_new(&handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(handle);
  }

  return res;
}

Result<uint16_t> HttpResp::get_status() const {
  Result<uint16_t> res;

  fastly_compute_at_edge_http_types_http_status_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_resp_status_get(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<Void> HttpResp::set_status(uint16_t status) {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_resp_status_set(this->handle, status, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<Void> HttpResp::send_downstream(HttpBody body, bool streaming) {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_http_resp_send_downstream(this->handle, body.handle, streaming,
                                                        &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

bool HttpResp::is_valid() const { return this->handle != HttpResp::invalid; }

Result<fastly_compute_at_edge_http_types_http_version_t> HttpResp::get_version() const {
  Result<fastly_compute_at_edge_http_types_http_version_t> res;

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_http_version_t ret;
  if (!fastly_compute_at_edge_http_resp_version_get(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<std::vector<HostString>> HttpResp::get_header_names() {
  return generic_get_header_names<fastly_compute_at_edge_http_resp_header_names_get>(this->handle);
}

Result<std::optional<std::vector<HostString>>> HttpResp::get_header_values(std::string_view name) {
  return generic_get_header_values<fastly_compute_at_edge_http_resp_header_values_get>(this->handle,
                                                                                       name);
}

Result<Void> HttpResp::insert_header(std::string_view name, std::string_view value) {
  return generic_header_op<fastly_compute_at_edge_http_resp_header_insert>(this->handle, name,
                                                                           value);
}

Result<Void> HttpResp::append_header(std::string_view name, std::string_view value) {
  return generic_header_op<fastly_compute_at_edge_http_resp_header_append>(this->handle, name,
                                                                           value);
}

Result<Void> HttpResp::remove_header(std::string_view name) {
  return generic_header_remove<fastly_compute_at_edge_http_resp_header_remove>(this->handle, name);
}

Result<HostString> GeoIp::lookup(std::span<uint8_t> bytes) {
  Result<HostString> res;

  fastly_world_list_u8_t octets_list{const_cast<uint8_t *>(bytes.data()), bytes.size()};
  fastly_world_string_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_geo_lookup(&octets_list, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(make_host_string(ret));
  }

  return res;
}

Result<LogEndpoint> LogEndpoint::get(std::string_view name) {
  Result<LogEndpoint> res;

  auto name_str = string_view_to_world_string(name);
  fastly_compute_at_edge_log_handle_t handle;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_log_endpoint_get(&name_str, &handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(LogEndpoint{handle});
  }

  return res;
}

Result<Void> LogEndpoint::write(std::string_view msg) {
  Result<Void> res;

  auto msg_str = string_view_to_world_string(msg);
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_log_write(this->handle, &msg_str, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<Dict> Dict::open(std::string_view name) {
  Result<Dict> res;

  auto name_str = string_view_to_world_string(name);
  fastly_compute_at_edge_dictionary_handle_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_dictionary_open(&name_str, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<std::optional<HostString>> Dict::get(std::string_view name) {
  Result<std::optional<HostString>> res;

  auto name_str = string_view_to_world_string(name);
  fastly_world_option_string_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_dictionary_get(this->handle, &name_str, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(make_host_string(ret.val));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<ObjectStore> ObjectStore::open(std::string_view name) {
  Result<ObjectStore> res;

  auto name_str = string_view_to_world_string(name);
  fastly_compute_at_edge_object_store_handle_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_object_store_open(&name_str, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<std::optional<HttpBody>> ObjectStore::lookup(std::string_view name) {
  Result<std::optional<HttpBody>> res;

  auto name_str = string_view_to_world_string(name);
  fastly_world_option_fastly_compute_at_edge_object_store_body_handle_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_object_store_lookup(this->handle, &name_str, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(ret.val);
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<Void> ObjectStore::insert(std::string_view name, HttpBody body) {
  Result<Void> res;

  auto name_str = string_view_to_world_string(name);
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_object_store_insert(this->handle, &name_str, body.handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

static_assert(std::is_same_v<Secret::Handle, fastly_compute_at_edge_secret_store_secret_handle_t>);
static_assert(
    std::is_same_v<SecretStore::Handle, fastly_compute_at_edge_secret_store_store_handle_t>);

Result<std::optional<HostString>> Secret::plaintext() const {
  Result<std::optional<HostString>> res;

  fastly_world_option_string_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_secret_store_plaintext(this->handle, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(make_host_string(ret.val));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<SecretStore> SecretStore::open(std::string_view name) {
  Result<SecretStore> res;

  auto name_str = string_view_to_world_string(name);
  fastly_compute_at_edge_secret_store_store_handle_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_secret_store_open(&name_str, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(ret);
  }

  return res;
}

Result<std::optional<Secret>> SecretStore::get(std::string_view name) {
  Result<std::optional<Secret>> res;

  auto name_str = string_view_to_world_string(name);
  fastly_world_option_fastly_compute_at_edge_secret_store_secret_handle_t ret;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_secret_store_get(this->handle, &name_str, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(ret.val);
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

Result<HostBytes> Random::get_bytes(size_t num_bytes) {
  Result<HostBytes> res;

  auto ret = HostBytes::with_capacity(num_bytes);
  auto err =
      fastly::random_get(reinterpret_cast<uint32_t>(static_cast<void *>(ret.begin())), num_bytes);
  if (err != 0) {
    res.emplace_err(err);
  } else {
    res.emplace(std::move(ret));
  }

  return res;
}

Result<uint32_t> Random::get_u32() {
  Result<uint32_t> res;

  uint32_t storage;
  auto err = fastly::random_get(reinterpret_cast<uint32_t>(static_cast<void *>(&storage)),
                                sizeof(storage));
  if (err != 0) {
    res.emplace_err(err);
  } else {
    res.emplace(storage);
  }

  return res;
}

bool CacheState::is_found() const {
  return this->state & FASTLY_COMPUTE_AT_EDGE_CACHE_LOOKUP_STATE_FOUND;
}

bool CacheState::is_usable() const {
  return this->state & FASTLY_COMPUTE_AT_EDGE_CACHE_LOOKUP_STATE_USABLE;
}

bool CacheState::is_stale() const {
  return this->state & FASTLY_COMPUTE_AT_EDGE_CACHE_LOOKUP_STATE_STALE;
}

bool CacheState::must_insert_or_update() const {
  return this->state & FASTLY_COMPUTE_AT_EDGE_CACHE_LOOKUP_STATE_MUST_INSERT_OR_UPDATE;
}

Result<CacheHandle> CacheHandle::lookup(std::string_view key, const CacheLookupOptions &opts) {
  Result<CacheHandle> res;

  auto key_str = string_view_to_world_string(key);

  fastly_compute_at_edge_cache_lookup_options_t os;
  memset(&os, 0, sizeof(os));

  if (opts.request_headers.is_valid()) {
    os.request_headers.is_some = true;
    os.request_headers.val = opts.request_headers.handle;
  }

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_cache_handle_t handle;
  if (!fastly_compute_at_edge_cache_lookup(&key_str, &os, &handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(handle);
  }

  return res;
}

Result<CacheHandle> CacheHandle::transaction_lookup(std::string_view key,
                                                    const CacheLookupOptions &opts) {
  Result<CacheHandle> res;

  auto key_str = string_view_to_world_string(key);

  fastly_compute_at_edge_cache_lookup_options_t os;
  memset(&os, 0, sizeof(os));

  if (opts.request_headers.is_valid()) {
    os.request_headers.is_some = true;
    os.request_headers.val = opts.request_headers.handle;
  }

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_cache_handle_t handle;
  if (!fastly_compute_at_edge_cache_transaction_lookup(&key_str, &os, &handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(handle);
  }

  return res;
}

namespace {

void init_write_options(fastly_compute_at_edge_cache_write_options_t &options,
                        const CacheWriteOptions &opts) {
  memset(&options, 0, sizeof(options));

  options.max_age_ns = opts.max_age_ns;
  options.request_headers = opts.req.handle;

  if (opts.vary_rule.empty()) {
    options.vary_rule.ptr = 0;
    options.vary_rule.len = 0;
  } else {
    options.vary_rule = string_view_to_world_string(opts.vary_rule);
  }

  options.initial_age_ns = opts.initial_age_ns;
  options.stale_while_revalidate_ns = opts.stale_while_revalidate_ns;

  if (opts.surrogate_keys.empty()) {
    options.surrogate_keys.ptr = 0;
    options.surrogate_keys.len = 0;
  } else {
    options.surrogate_keys = string_view_to_world_string(opts.surrogate_keys);
  }

  options.length = opts.length;

  if (opts.metadata.empty()) {
    options.user_metadata.ptr = 0;
    options.user_metadata.len = 0;
  } else {
    options.user_metadata = span_to_list_u8(opts.metadata);
  }

  options.sensitive_data = opts.sensitive;
}

} // namespace

Result<HttpBody> CacheHandle::insert(std::string_view key, const CacheWriteOptions &opts) {
  Result<HttpBody> res;

  fastly_compute_at_edge_cache_write_options_t options;
  init_write_options(options, opts);

  fastly_compute_at_edge_types_error_t err;
  fastly_compute_at_edge_http_types_body_handle_t ret;
  auto host_key = string_view_to_world_string(key);
  if (!fastly_compute_at_edge_cache_insert(&host_key, &options, &ret, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(HttpBody{ret});
  }

  return res;
}

Result<std::tuple<HttpBody, CacheHandle>>
CacheHandle::insert_and_stream_back(const CacheWriteOptions &opts) {
  Result<std::tuple<HttpBody, CacheHandle>> res;

  fastly_compute_at_edge_cache_write_options_t options;
  init_write_options(options, opts);

  fastly_compute_at_edge_types_error_t err;
  fastly_world_tuple2_fastly_compute_at_edge_cache_body_handle_fastly_compute_at_edge_cache_handle_t
      ret;
  if (!fastly_compute_at_edge_cache_transaction_insert_and_stream_back(this->handle, &options, &ret,
                                                                       &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(HttpBody{ret.f0}, CacheHandle{ret.f1});
  }

  return res;
}

Result<Void> CacheHandle::transaction_cancel() {
  Result<Void> res;

  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_cache_transaction_cancel(this->handle, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace();
  }

  return res;
}

Result<HttpBody> CacheHandle::get_body(const CacheGetBodyOptions &opts) {
  Result<HttpBody> res;

  fastly_compute_at_edge_cache_get_body_options_t options{
      .start = opts.start,
      .end = opts.end,
  };
  fastly_compute_at_edge_http_types_body_handle_t body;
  fastly_compute_at_edge_types_error_t err;
  if (!fastly_compute_at_edge_cache_get_body(this->handle, &options, &body, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(HttpBody{body});
  }

  return res;
}

Result<CacheState> CacheHandle::get_state() {
  Result<CacheState> res;

  fastly_compute_at_edge_types_error_t err;
  alignas(4) fastly_compute_at_edge_cache_lookup_state_t state;
  if (!fastly_compute_at_edge_cache_get_state(this->handle, &state, &err)) {
    res.emplace_err(err);
  } else {
    res.emplace(CacheState{state});
  }

  return res;
}

Result<std::optional<HostString>> Fastly::purge_surrogate_key(std::string_view key) {
  Result<std::optional<HostString>> res;

  auto host_key = string_view_to_world_string(key);
  fastly_world_option_string_t ret;
  fastly_compute_at_edge_types_error_t err;
  // TODO: we don't currently define any meaningful options in fastly.wit
  fastly_compute_at_edge_purge_options_mask_t purge_options = 0;
  if (!fastly_compute_at_edge_purge_surrogate_key(&host_key, purge_options, &ret, &err)) {
    res.emplace_err(err);
  } else if (ret.is_some) {
    res.emplace(make_host_string(ret.val));
  } else {
    res.emplace(std::nullopt);
  }

  return res;
}

} // namespace host_api
