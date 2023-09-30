// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/experimental/TypedData.h"
#pragma clang diagnostic pop

#include "zlib.h"

#include "builtin.h"
#include "builtins/decompression-stream.h"
#include "builtins/transform-stream-default-controller.h"
#include "builtins/transform-stream.h"
#include "core/encode.h"
#include "js-compute-builtins.h"

namespace builtins {

namespace {

enum class Format {
  GZIP,
  Deflate,
  DeflateRaw,
};

// Using the same fixed encoding buffer size as Chromium, see
// https://chromium.googlesource.com/chromium/src/+/457f48d3d8635c8bca077232471228d75290cc29/third_party/blink/renderer/modules/compression/deflate_transformer.cc#29
constexpr size_t BUFFER_SIZE = 16384;

JSObject *transform(JSObject *self) {
  MOZ_ASSERT(DecompressionStream::is_instance(self));
  return &JS::GetReservedSlot(self, DecompressionStream::Slots::Transform).toObject();
}

z_stream *state(JSObject *self) {
  MOZ_ASSERT(DecompressionStream::is_instance(self));
  void *ptr = JS::GetReservedSlot(self, DecompressionStream::Slots::State).toPrivate();
  MOZ_ASSERT(ptr);
  return (z_stream *)ptr;
}

uint8_t *output_buffer(JSObject *self) {
  MOZ_ASSERT(DecompressionStream::is_instance(self));
  void *ptr = JS::GetReservedSlot(self, DecompressionStream::Slots::Buffer).toPrivate();
  MOZ_ASSERT(ptr);
  return (uint8_t *)ptr;
}

JS::PersistentRooted<JSObject *> transformAlgo;
JS::PersistentRooted<JSObject *> flushAlgo;

// Steps 1-5 of the transform algorithm, and 1-5 of the flush algorithm.
bool inflate_chunk(JSContext *cx, JS::HandleObject self, JS::HandleValue chunk, bool finished) {
  z_stream *zstream = state(self);

  if (!finished) {
    // 1.  If _chunk_ is not a `BufferSource` type, then throw a `TypeError`.
    // Step 2 of transform:
    auto data = value_to_buffer(cx, chunk, "DecompressionStream transform: chunks");
    if (!data.has_value()) {
      return false;
    }

    if (data->size() == 0) {
      return true;
    }

    // 2.  Let _buffer_ be the result of decompressing _chunk_ with _ds_'s format
    // and context. This just sets up step 2. The actual decompression happen in
    // the `do` loop below.
    zstream->avail_in = data->size();

    // `data` is a live view into `chunk`. That's ok here because it'll be fully
    // used in the `do` loop below before any content can execute again and
    // could potentially invalidate the pointer to `data`.
    zstream->next_in = data->data();
  } else {
    // Step 1 of flush:
    // 1.  Let _buffer_ be the result of decompressing an empty input with _ds_'s
    // format and
    //     context, with the finish flag.

    // Step 2 of flush:
    // 2.  If the end of the compressed input has not been reached, then throw a TypeError.
    if (zstream->avail_in != 0) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_DECOMPRESSING_ERROR);
      return false;
    }
    // This just sets up step 3. The actual decompression happens in the `do` loop
    // below.
    zstream->avail_in = 0;
    zstream->next_in = nullptr;
  }

  JS::RootedObject controller(cx, builtins::TransformStream::controller(transform(self)));

  // Steps 3-5 of transform are identical to steps 3-5 of flush, so numbers
  // below refer to the former for those. Also, the compression happens in
  // potentially smaller chunks in the `do` loop below, so the three steps are
  // reordered and somewhat intertwined with each other.

  uint8_t *buffer = output_buffer(self);

  // Call `inflate` in a loop, enqueuing compressed chunks until the input
  // buffer has been fully consumed. That is the case when `zstream->avail_out`
  // is non-zero, i.e. when the last chunk wasn't completely filled. See zlib
  // docs for details:
  // https://searchfox.org/mozilla-central/rev/87ecd21d3ca517f8d90e49b32bf042a754ed8f18/modules/zlib/src/zlib.h#319-324
  do {
    // 4.  Split _buffer_ into one or more non-empty pieces and convert them
    // into `Uint8Array`s.
    // 5.  For each `Uint8Array` _array_, enqueue _array_ in _cds_'s transform.
    // This loop does the actual decompression, one output-buffer sized chunk at a
    // time, and then creates and enqueues the Uint8Arrays immediately.
    zstream->avail_out = BUFFER_SIZE;
    zstream->next_out = buffer;
    int err = inflate(zstream, finished ? Z_FINISH : Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {

      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_DECOMPRESSING_ERROR);
      return false;
    }

    size_t bytes = BUFFER_SIZE - zstream->avail_out;
    if (bytes) {
      JS::RootedObject out_obj(cx, JS_NewUint8Array(cx, bytes));
      if (!out_obj) {
        return false;
      }

      {
        bool is_shared;
        JS::AutoCheckCannotGC nogc;
        uint8_t *out_buffer = JS_GetUint8ArrayData(out_obj, &is_shared, nogc);
        memcpy(out_buffer, buffer, bytes);
      }

      JS::RootedValue out_chunk(cx, JS::ObjectValue(*out_obj));
      if (!builtins::TransformStreamDefaultController::Enqueue(cx, controller, out_chunk)) {
        return false;
      }
    }

    // 3.  If _buffer_ is empty, return.
  } while (zstream->avail_out == 0);

  return true;
}

} // namespace

// https://wicg.github.io/compression/#decompress-and-enqueue-a-chunk
// All steps inlined into `inflate_chunk`.
bool DecompressionStream::transformAlgorithm(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(1, "Decompression stream transform algorithm")

  if (!inflate_chunk(cx, self, args[0], false)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

// https://wicg.github.io/compression/#decompress-flush-and-enqueue
// All steps inlined into `inflate_chunk`.
bool DecompressionStream::flushAlgorithm(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "Decompression stream flush algorithm")

  if (!inflate_chunk(cx, self, JS::UndefinedHandleValue, true)) {
    return false;
  }

  inflateEnd(state(self));
  JS_free(cx, output_buffer(self));

// These fields shouldn't ever be accessed again, but we should be able to
// assert that.
#ifdef DEBUG
  JS::SetReservedSlot(self, DecompressionStream::Slots::State, JS::PrivateValue(nullptr));
  JS::SetReservedSlot(self, DecompressionStream::Slots::Buffer, JS::PrivateValue(nullptr));
#endif

  args.rval().setUndefined();
  return true;
}

bool DecompressionStream::readable_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "get readable")
  args.rval().setObject(*builtins::TransformStream::readable(transform(self)));
  return true;
}

bool DecompressionStream::writable_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER_WITH_NAME(0, "get writable")
  args.rval().setObject(*builtins::TransformStream::writable(transform(self)));
  return true;
}

const JSFunctionSpec DecompressionStream::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec DecompressionStream::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec DecompressionStream::methods[] = {
    JS_FS_END,
};

const JSPropertySpec DecompressionStream::properties[] = {
    JS_PSG("readable", DecompressionStream::readable_get, JSPROP_ENUMERATE),
    JS_PSG("writable", DecompressionStream::writable_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "DecompressionStream", JSPROP_READONLY),
    JS_PS_END,
};

namespace {

// Steps 2-6 of `new DecompressionStream()`.
JSObject *create(JSContext *cx, JS::HandleObject stream, Format format) {
  JS::RootedValue stream_val(cx, JS::ObjectValue(*stream));

  // 2.  Set this's format to _format_.
  JS::SetReservedSlot(stream, DecompressionStream::Slots::Format, JS::Int32Value((int32_t)format));

  // 3.  Let _transformAlgorithm_ be an algorithm which takes a _chunk_ argument
  // and runs the
  //     `compress and enqueue a chunk algorithm with this and _chunk_.
  // 4.  Let _flushAlgorithm_ be an algorithm which takes no argument and runs
  // the
  //     `compress flush and enqueue` algorithm with this.
  // (implicit)

  // 5.  Set this's transform to a new `TransformStream`.
  // 6.  [Set up](https://streams.spec.whatwg.org/#transformstream-set-up)
  // this's transform with _transformAlgorithm_ set to _transformAlgorithm_ and
  // _flushAlgorithm_ set to _flushAlgorithm_.
  JS::RootedObject transform(cx, builtins::TransformStream::create(cx, 1, nullptr, 0, nullptr,
                                                                   stream_val, nullptr,
                                                                   transformAlgo, flushAlgo));
  if (!transform) {
    return nullptr;
  }

  builtins::TransformStream::set_used_as_mixin(transform);
  JS::SetReservedSlot(stream, DecompressionStream::Slots::Transform, JS::ObjectValue(*transform));

  // The remainder of the function deals with setting up the inflate state used
  // for decompressing chunks.

  z_stream *zstream = (z_stream *)JS_malloc(cx, sizeof(z_stream));
  if (!zstream) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  memset(zstream, 0, sizeof(z_stream));
  JS::SetReservedSlot(stream, DecompressionStream::Slots::State, JS::PrivateValue(zstream));

  uint8_t *buffer = (uint8_t *)JS_malloc(cx, BUFFER_SIZE);
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  JS::SetReservedSlot(stream, DecompressionStream::Slots::Buffer, JS::PrivateValue(buffer));

  // Using the same window bits as Chromium's Compression stream, see
  // https://chromium.googlesource.com/chromium/src/+/457f48d3d8635c8bca077232471228d75290cc29/third_party/blink/renderer/modules/compression/inflate_transformer.cc#31
  int window_bits = 15;
  if (format == Format::GZIP) {
    window_bits += 16;
  } else if (format == Format::DeflateRaw) {
    window_bits = -15;
  }

  int err = inflateInit2(zstream, window_bits);
  if (err != Z_OK) {
    JS_ReportErrorASCII(cx, "Error initializing decompression stream");
    return nullptr;
  }

  return stream;
}

} // namespace

/**
 * https://wicg.github.io/compression/#dom-compressionstream-compressionstream
 */
bool DecompressionStream::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  // 1.  If _format_ is unsupported in `CompressionStream`, then throw a
  // `TypeError`.
  CTOR_HEADER("DecompressionStream", 1);

  auto format_chars = core::encode(cx, args[0]);
  if (!format_chars) {
    return false;
  }

  enum Format format;
  if (!strcmp(format_chars.begin(), "deflate-raw")) {
    format = Format::DeflateRaw;
  } else if (!strcmp(format_chars.begin(), "deflate")) {
    format = Format::Deflate;
  } else if (!strcmp(format_chars.begin(), "gzip")) {
    format = Format::GZIP;
  } else {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_COMPRESSION_FORMAT,
                             format_chars.begin());
    return false;
  }

  JS::RootedObject decompressionStreamInstance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  // Steps 2-6.
  JS::RootedObject stream(cx, create(cx, decompressionStreamInstance, format));
  if (!stream) {
    return false;
  }

  args.rval().setObject(*stream);
  return true;
}

bool DecompressionStream::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
    return false;
  }

  JSFunction *transformFun = JS_NewFunction(cx, transformAlgorithm, 1, 0, "DS Transform");
  if (!transformFun)
    return false;
  transformAlgo.init(cx, JS_GetFunctionObject(transformFun));

  JSFunction *flushFun = JS_NewFunction(cx, flushAlgorithm, 1, 0, "DS Flush");
  if (!flushFun)
    return false;
  flushAlgo.init(cx, JS_GetFunctionObject(flushFun));

  return true;
}

} // namespace builtins
