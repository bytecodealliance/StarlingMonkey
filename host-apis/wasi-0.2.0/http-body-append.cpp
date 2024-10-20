#include "handles.h"

namespace host_api {

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

  api::TaskCompletionCallback cb_;
  Heap<JSObject *> cb_receiver_;
  State state_;

  void set_state(JSContext *cx, const State state) {
    MOZ_ASSERT(state_ != State::Done);
    state_ = state;
    if (state == State::Done && cb_) {
      RootedObject receiver(cx, cb_receiver_);
      cb_(cx, receiver);
      cb_ = nullptr;
      cb_receiver_ = nullptr;
    }
  }

public:
  explicit BodyAppendTask(api::Engine *engine, HttpIncomingBody *incoming_body,
                          HttpOutgoingBody *outgoing_body,
                          api::TaskCompletionCallback completion_callback,
                          HandleObject callback_receiver)
      : incoming_body_(incoming_body), outgoing_body_(outgoing_body), cb_(completion_callback),
        cb_receiver_(callback_receiver), state_(State::BlockedOnBoth) {
    auto res = incoming_body_->subscribe();
    MOZ_ASSERT(!res.is_err());
    incoming_pollable_ = res.unwrap();

    res = outgoing_body_->subscribe();
    MOZ_ASSERT(!res.is_err());
    outgoing_pollable_ = res.unwrap();
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    // If run is called while we're blocked on the incoming stream, that means that stream's
    // pollable has resolved, so the stream must be ready.
    if (state_ == State::BlockedOnBoth || state_ == State::BlockedOnIncoming) {
      auto res = incoming_body_->read(0);
      MOZ_ASSERT(!res.is_err());
      auto [done, _] = std::move(res.unwrap());
      if (done) {
        set_state(engine->cx(), State::Done);
        return true;
      }
      set_state(engine->cx(), State::BlockedOnOutgoing);
    }

    MOZ_ASSERT(state_ == State::BlockedOnOutgoing);
    auto res = outgoing_body_->capacity();
    if (res.is_err()) {
      return false;
    }
    uint64_t capacity = res.unwrap();
    if (capacity > 0) {
      set_state(engine->cx(), State::Ready);
    } else {
      engine->queue_async_task(this);
      return true;
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
        set_state(engine->cx(), State::BlockedOnIncoming);
        engine->queue_async_task(this);
        return true;
      }

      if (bytes.len > 0) {
        outgoing_body_->write(bytes.ptr.get(), bytes.len);
      }

      if (done) {
        set_state(engine->cx(), State::Done);
        return true;
      }

      auto capacity_res = outgoing_body_->capacity();
      if (capacity_res.is_err()) {
        // TODO: proper error handling.
        return false;
      }
      capacity = capacity_res.unwrap();
    } while (capacity > 0);

    set_state(engine->cx(), State::BlockedOnOutgoing);
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
    JS::TraceEdge(trc, &cb_receiver_, "BodyAppendTask completion callback receiver");
  }
};

Result<Void> HttpOutgoingBody::append(api::Engine *engine, HttpIncomingBody *other,
                                      api::TaskCompletionCallback callback,
                                      HandleObject callback_receiver) {
  engine->queue_async_task(new BodyAppendTask(engine, other, this, callback, callback_receiver));
  return {};
}

}
