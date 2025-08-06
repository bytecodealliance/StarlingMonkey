/**
 * NOT PART OF THE PUBLIC INTERFACE!
 *
 * Types for dealing with WASI handles in the wit-bindgen generated C bindings.
 */

#ifndef HANDLES_H
#define HANDLES_H

#include "host_api.h"
#include "bindings/bindings.h"

#include <exports.h>
#include <list>
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

typedef wasi_http_types_own_future_incoming_response_t future_incoming_response_t;
typedef wasi_http_types_borrow_future_incoming_response_t borrow_future_incoming_response_t;

typedef wasi_http_types_own_incoming_body_t incoming_body_t;
typedef wasi_http_types_own_outgoing_body_t outgoing_body_t;

using field_key = wasi_http_types_field_key_t;
using field_value = wasi_http_types_field_value_t;

typedef wasi_io_poll_own_pollable_t own_pollable_t;
typedef wasi_io_poll_borrow_pollable_t borrow_pollable_t;
typedef wasi_io_poll_list_borrow_pollable_t list_borrow_pollable_t;

#ifdef LOG_HANDLE_OPS
#define LOG_HANDLE_OP(...)                                                                         \
  fprintf(stderr, "%s", __PRETTY_FUNCTION__);                                                      \
  fprintf(stderr, __VA_ARGS__)
#else
#define LOG_HANDLE_OP(...)
#endif

/// The type of handles used by the host interface.
typedef int32_t Handle;
constexpr Handle POISONED_HANDLE = -1;

class host_api::HandleState {
protected:
  HandleState() = default;

public:
  virtual ~HandleState() = default;
  virtual bool valid() const = 0;
};

template <class T> struct HandleOps {};

template <class T> class WASIHandle : public host_api::HandleState {
#ifdef DEBUG
  static inline auto used_handles = std::set<Handle>();
#endif

protected:
  Handle handle_;
#ifdef DEBUG
  bool owned_;
#endif

public:
  using Borrowed = typename HandleOps<T>::borrowed;

  explicit WASIHandle(typename HandleOps<T>::owned handle) : handle_{handle.__handle} {
    LOG_HANDLE_OP("Creating owned handle %d\n", handle.__handle);
#ifdef DEBUG
    owned_ = true;
    MOZ_ASSERT(!used_handles.contains(handle.__handle));
    used_handles.insert(handle.__handle);
#endif
  }

  explicit WASIHandle(typename HandleOps<T>::borrowed handle) : handle_{handle.__handle} {
    LOG_HANDLE_OP("Creating borrowed handle %d\n", handle.__handle);
#ifdef DEBUG
    owned_ = false;
    MOZ_ASSERT(!used_handles.contains(handle.__handle));
    used_handles.insert(handle.__handle);
#endif
  }

  ~WASIHandle() override {
#ifdef DEBUG
    if (handle_ != POISONED_HANDLE) {
      LOG_HANDLE_OP("Deleting (owned? %d) handle %d\n", owned_, handle_);
      MOZ_ASSERT(used_handles.contains(handle_));
      used_handles.erase(handle_);
    }
#endif
  }

  static WASIHandle<T> *cast(HandleState *handle) {
    return reinterpret_cast<WASIHandle<T> *>(handle);
  }

  typename HandleOps<T>::borrowed borrow(HandleState *handle) { return cast(handle)->borrow(); }

  bool valid() const override {
    bool valid = handle_ != POISONED_HANDLE;
    MOZ_ASSERT_IF(valid, used_handles.contains(handle_));
    return valid;
  }

  typename HandleOps<T>::borrowed borrow() const {
    MOZ_ASSERT(valid());
    LOG_HANDLE_OP("borrowing handle %d\n", handle_);
    return {handle_};
  }

  typename HandleOps<T>::owned take() {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(owned_);
    LOG_HANDLE_OP("taking handle %d\n", handle_);
    typename HandleOps<T>::owned handle = {handle_};
#ifdef DEBUG
    used_handles.erase(handle_);
#endif
    handle_ = POISONED_HANDLE;
    return handle;
  }
};

template <class T> struct Borrow {
  static constexpr typename HandleOps<T>::borrowed invalid{std::numeric_limits<int32_t>::max()};
  typename HandleOps<T>::borrowed handle_{invalid};

  explicit Borrow(host_api::HandleState *handle) {
    handle_ = WASIHandle<T>::cast(handle)->borrow();
  }

  explicit Borrow(typename HandleOps<T>::borrowed handle) { handle_ = handle; }

  explicit Borrow(typename HandleOps<T>::owned handle) { handle_ = {handle.__handle}; }

  operator typename HandleOps<T>::borrowed() const { return handle_; }
};

template <> struct HandleOps<host_api::Pollable> {
  using owned = wasi_io_poll_own_pollable_t;
  using borrowed = wasi_io_poll_borrow_pollable_t;
};

template <> struct HandleOps<host_api::HttpHeaders> {
  using owned = wasi_http_types_own_headers_t;
  using borrowed = wasi_http_types_borrow_fields_t;
};

template <> struct HandleOps<host_api::HttpIncomingRequest> {
  using owned = wasi_http_types_own_incoming_request_t;
  using borrowed = wasi_http_types_borrow_incoming_request_t;
};

template <> struct HandleOps<host_api::HttpOutgoingRequest> {
  using owned = wasi_http_types_own_outgoing_request_t;
  using borrowed = wasi_http_types_borrow_outgoing_request_t;
};

template <> struct HandleOps<host_api::FutureHttpIncomingResponse> {
  using owned = wasi_http_types_own_future_incoming_response_t;
  using borrowed = wasi_http_types_borrow_future_incoming_response_t;
};

template <> struct HandleOps<host_api::HttpIncomingResponse> {
  using owned = wasi_http_types_own_incoming_response_t;
  using borrowed = wasi_http_types_borrow_incoming_response_t;
};

template <> struct HandleOps<host_api::HttpOutgoingResponse> {
  using owned = wasi_http_types_own_outgoing_response_t;
  using borrowed = wasi_http_types_borrow_outgoing_response_t;
};

template <> struct HandleOps<host_api::HttpIncomingBody> {
  using owned = wasi_http_types_own_incoming_body_t;
  using borrowed = wasi_http_types_borrow_incoming_body_t;
};

template <> struct HandleOps<host_api::HttpOutgoingBody> {
  using owned = wasi_http_types_own_outgoing_body_t;
  using borrowed = wasi_http_types_borrow_outgoing_body_t;
};

struct OutputStream {};
template <> struct HandleOps<OutputStream> {
  using owned = wasi_io_streams_own_output_stream_t;
  using borrowed = wasi_io_streams_borrow_output_stream_t;
};

struct InputStream {};
template <> struct HandleOps<InputStream> {
  using owned = wasi_io_streams_own_input_stream_t;
  using borrowed = wasi_io_streams_borrow_input_stream_t;
};

class IncomingBodyHandle final : public WASIHandle<host_api::HttpIncomingBody> {
  HandleOps<InputStream>::owned stream_handle_;
  PollableHandle pollable_handle_;

  friend host_api::HttpIncomingBody;
  friend host_api::HttpOutgoingBody;

public:
  explicit IncomingBodyHandle(HandleOps<host_api::HttpIncomingBody>::owned handle)
      : WASIHandle(handle), pollable_handle_(api::INVALID_POLLABLE_HANDLE) {
    HandleOps<InputStream>::owned stream{};
    if (!wasi_http_types_method_incoming_body_stream(borrow(), &stream)) {
      MOZ_ASSERT_UNREACHABLE("Getting a body's stream should never fail");
    }
    stream_handle_ = stream;
  }

  static IncomingBodyHandle *cast(HandleState *handle) {
    return reinterpret_cast<IncomingBodyHandle *>(handle);
  }
};

class OutgoingBodyHandle final : public WASIHandle<host_api::HttpOutgoingBody> {
  HandleOps<OutputStream>::owned stream_handle_;
  PollableHandle pollable_handle_;

  friend host_api::HttpOutgoingBody;

public:
  explicit OutgoingBodyHandle(HandleOps<host_api::HttpOutgoingBody>::owned handle)
      : WASIHandle(handle), pollable_handle_(api::INVALID_POLLABLE_HANDLE) {
    HandleOps<OutputStream>::owned stream{};
    if (!wasi_http_types_method_outgoing_body_write(borrow(), &stream)) {
      MOZ_ASSERT_UNREACHABLE("Getting a body's stream should never fail");
    }
    stream_handle_ = stream;
  }

  static OutgoingBodyHandle *cast(HandleState *handle) {
    return reinterpret_cast<OutgoingBodyHandle *>(handle);
  }
};

#endif
