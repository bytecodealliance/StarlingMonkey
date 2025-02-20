#include "sockets.h"

#include <../wasi-0.2.0/handles.h>

template <> struct HandleOps<host_api::TCPSocket> {
  using owned = wasi_sockets_tcp_own_tcp_socket_t;
  using borrowed = wasi_sockets_tcp_borrow_tcp_socket_t;
};

class TCPSocketHandle final : public WASIHandle<host_api::TCPSocket> {
  wasi_sockets_instance_network_own_network_t network_;
  PollableHandle pollable_handle_;
  wasi_io_streams_own_input_stream_t input_;
  wasi_io_streams_own_output_stream_t output_;

  friend host_api::TCPSocket;

public:
  explicit TCPSocketHandle(HandleOps<host_api::TCPSocket>::owned handle)
      : WASIHandle(handle), pollable_handle_(INVALID_POLLABLE_HANDLE) {
    network_ = wasi_sockets_instance_network_instance_network();
  }

  static TCPSocketHandle *cast(HandleState *handle) {
    return reinterpret_cast<TCPSocketHandle *>(handle);
  }

  wasi_sockets_tcp_borrow_network_t network() const {
    return wasi_sockets_tcp_borrow_network_t(network_.__handle);
  }
  PollableHandle pollable_handle() {
    if (pollable_handle_ == INVALID_POLLABLE_HANDLE) {
      pollable_handle_ = wasi_sockets_tcp_method_tcp_socket_subscribe(borrow()).__handle;
    }
    return pollable_handle_;
  }
};

namespace host_api {

TCPSocket::TCPSocket(std::unique_ptr<HandleState> state) {
  this->handle_state_ = std::move(state);
}

TCPSocket *TCPSocket::make(IPAddressFamily address_family) {
  wasi_sockets_tcp_create_socket_ip_address_family_t family =
      address_family == IPV4 ? WASI_SOCKETS_NETWORK_IP_ADDRESS_FAMILY_IPV4
                             : WASI_SOCKETS_NETWORK_IP_ADDRESS_FAMILY_IPV6;
  wasi_sockets_tcp_create_socket_own_tcp_socket_t ret;
  wasi_sockets_tcp_create_socket_error_code_t err;
  if (!wasi_sockets_tcp_create_socket_create_tcp_socket(family, &ret, &err)) {
    return nullptr;
  }
  return new TCPSocket(std::unique_ptr<HandleState>(new TCPSocketHandle(ret)));
}

bool TCPSocket::connect(AddressIPV4 address, Port port) {
  auto state = TCPSocketHandle::cast(handle_state_.get());
  auto handle = state->borrow();
  auto addr = wasi_sockets_network_ipv4_address_t(get<0>(address), get<1>(address), get<2>(address),
                                                  get<3>(address));
  wasi_sockets_tcp_ip_socket_address_t socket_address = {
      WASI_SOCKETS_NETWORK_IP_SOCKET_ADDRESS_IPV4, {{port, addr}}};
  wasi_sockets_tcp_error_code_t err;
  if (!wasi_sockets_tcp_method_tcp_socket_start_connect(handle, state->network(), &socket_address,
                                                        &err)) {
    // TODO: handle error
    return false;
  }

  wasi_sockets_tcp_tuple2_own_input_stream_own_output_stream_t streams;
  while (true) {
    if (!wasi_sockets_tcp_method_tcp_socket_finish_connect(handle, &streams, &err)) {
      if (err == WASI_SOCKETS_NETWORK_ERROR_CODE_WOULD_BLOCK) {
        block_on_pollable_handle(state->pollable_handle());
        continue;
      }
      // TODO: handle error
      return false;
    }
    state->input_ = streams.f0;
    state->output_ = streams.f1;
    break;
  }

  return true;
}
void TCPSocket::close() {
  auto state = TCPSocketHandle::cast(handle_state_.get());
  if (!state->valid()) {
    return;
  }
  wasi_sockets_tcp_error_code_t err;
  wasi_sockets_tcp_method_tcp_socket_shutdown(state->borrow(),
    WASI_SOCKETS_TCP_SHUTDOWN_TYPE_BOTH, &err);
  wasi_io_streams_output_stream_drop_own(state->output_);
  wasi_io_streams_input_stream_drop_own(state->input_);
  if (state->pollable_handle_ != INVALID_POLLABLE_HANDLE) {
    wasi_io_poll_pollable_drop_own(own_pollable_t{state->pollable_handle_});
  }
  wasi_sockets_tcp_tcp_socket_drop_own(state->take());
}

bool TCPSocket::send(HostString chunk) {
  auto state = TCPSocketHandle::cast(handle_state_.get());
  auto borrow = wasi_io_streams_borrow_output_stream(state->output_);
  bindings_list_u8_t list{reinterpret_cast<uint8_t *>(chunk.ptr.get()), chunk.len};
  uint64_t capacity = 0;
  wasi_io_streams_stream_error_t err;
  if (!wasi_io_streams_method_output_stream_check_write(borrow, &capacity, &err)) {
    // TODO: proper error handling.
  }
  // TODO: proper error handling.
  MOZ_ASSERT(chunk.len <= capacity);
  return wasi_io_streams_method_output_stream_write(borrow, &list, &err);
}

HostString TCPSocket::receive(uint32_t chunk_size) {
  auto state = TCPSocketHandle::cast(handle_state_.get());
  auto borrow = wasi_io_streams_borrow_input_stream(state->input_);
  bindings_list_u8_t ret{};
  wasi_io_streams_stream_error_t err{};
  mozilla::DebugOnly<bool> success;
  success = wasi_io_streams_method_input_stream_blocking_read(borrow, chunk_size, &ret, &err);
  MOZ_ASSERT(success, "Why you not handle errors");
  UniqueChars chars((char*)ret.ptr);
  return HostString(std::move(chars), ret.len);
}

} // namespace host_api
