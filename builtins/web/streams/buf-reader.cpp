#include "buf-reader.h"
#include "extension-api.h"
#include "native-stream-source.h"

#include "js/Stream.h"
#include "mozilla/Assertions.h"

namespace {

constexpr size_t CHUNK_SIZE = 8192;

} // namespace

namespace builtins {
namespace web {
namespace streams {

class StreamTask final : public api::AsyncTask {
  Heap<JSObject *> reader_;

public:
  explicit StreamTask(const HandleObject reader) : reader_(reader) {
    handle_ = IMMEDIATE_TASK_HANDLE;
  }

  [[nodiscard]] bool run(api::Engine *engine) override {
    JSContext *cx = engine->cx();

    RootedObject user(cx, BufReader::user(reader_));
    RootedObject source(cx, BufReader::stream(reader_));
    RootedObject stream(cx, NativeStreamSource::stream(source));

    size_t readsz = 0;
    bool done = false;

    RootedObject buffer(cx, JS::NewArrayBuffer(cx, CHUNK_SIZE));
    if (!buffer) {
      return false;
    }

    RootedValue buffer_val(cx, JS::ObjectValue(*buffer));
    auto span = value_to_buffer(cx, buffer_val, "BufReader: buffer");
    if (!span) {
      return false;
    }

    auto read = BufReader::read_fn(reader_);
    auto start = BufReader::position(reader_);
    auto buf = span.value();

    if (!read(cx, user, buf, start, &readsz, &done)) {
      return false;
    }

    MOZ_ASSERT(readsz <= buf.size());

    if (done) {
      if (!JS::ReadableStreamClose(cx, stream)) {
        return false;
      }

      return cancel(engine);
    }

    RootedObject bytes_buffer(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, readsz));
    if (!bytes_buffer) {
      return false;
    }

    RootedValue enqueue_val(cx);
    enqueue_val.setObject(*bytes_buffer);
    if (!JS::ReadableStreamEnqueue(cx, stream, enqueue_val)) {
      return false;
    }

    BufReader::set_position(reader_, start + readsz);
    return cancel(engine);
  }

  [[nodiscard]] bool cancel(api::Engine *engine) override {
    handle_ = INVALID_POLLABLE_HANDLE;
    return true;
  }

  void trace(JSTracer *trc) override { TraceEdge(trc, &reader_, "Reader for BufReader StreamTask"); }
};

const JSFunctionSpec BufReader::static_methods[] = {JS_FS_END};
const JSPropertySpec BufReader::static_properties[] = {JS_PS_END};
const JSFunctionSpec BufReader::methods[] = {JS_FS_END};
const JSPropertySpec BufReader::properties[] = {JS_PS_END};

bool cancel(JSContext *cx, JS::CallArgs args, HandleObject stream, HandleObject owner, HandleValue reason) {
  args.rval().setUndefined();
  return true;
}

bool pull(JSContext *cx, JS::CallArgs args, HandleObject source, HandleObject owner, HandleObject controller) {
  api::Engine::get(cx)->queue_async_task(new StreamTask(owner));
  args.rval().setUndefined();
  return true;
}

JSObject *BufReader::user(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, Slots::User).toObject();
}

JSObject *BufReader::stream(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, Slots::Stream).toObject();
}

BufReader::ReadFn* BufReader::read_fn(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return reinterpret_cast<ReadFn *>(JS::GetReservedSlot(self, Slots::Read).toPrivate());
}

size_t BufReader::position(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return reinterpret_cast<size_t>(JS::GetReservedSlot(self, Slots::Position).toPrivate());
}

void BufReader::set_position(JSObject *self, size_t pos) {
  MOZ_ASSERT(is_instance(self));
  JS::SetReservedSlot(self, Slots::Position, JS::PrivateValue(reinterpret_cast<void *>(pos)));
}

JSObject *BufReader::create(JSContext *cx, JS::HandleObject user, BufReader::ReadFn *read) {
  JS::RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self) {
    return nullptr;
  }

  auto size = JS::UndefinedHandleValue;
  RootedObject stream(cx, NativeStreamSource::create(cx, self, size, pull, cancel));
  if (!stream) {
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::User, JS::ObjectValue(*user));
  JS::SetReservedSlot(self, Slots::Stream, JS::ObjectValue(*stream));
  JS::SetReservedSlot(self, Slots::Read, JS::PrivateValue(reinterpret_cast<void *>(read)));
  JS::SetReservedSlot(self, Slots::Position, JS::PrivateValue(reinterpret_cast<void *>(0)));

  return self;
}

} // namespace streams
} // namespace web
} // namespace builtins
