#include "bindings/bindings.h"
#include "handles.h"

namespace host_api {

Result<Void> HttpOutgoingBody::append(api::Engine *engine, HttpIncomingBody *other,
                                      api::TaskCompletionCallback callback,
                                      HandleObject callback_receiver) {
  auto *state = static_cast<OutgoingBodyHandle *>(this->handle_state_.get());

  auto incoming_body_handle = IncomingBodyHandle::cast(other->handle_state_.get());
  auto incoming_stream = incoming_body_handle->stream_handle_;
  incoming_body_handle->stream_handle_ = {-1};
  wasi_http_types_own_io_error_t io_error;
  uint64_t *len = nullptr;
  if (other->content_length_) {
    len = &other->content_length_.value();
  }
  if (!wasi_http_types_method_outgoing_body_append(state->borrow(), incoming_stream, len, &io_error)) {
    return Result<Void>::err(154);
  }
  callback(engine->cx(), callback_receiver);
  return {};
}

}
