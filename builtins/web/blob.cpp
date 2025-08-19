#include "blob.h"
#include "file.h"
#include "builtin.h"
#include "encode.h"
#include "rust-encoding.h"
#include "streams/buf-reader.h"
#include "streams/native-stream-source.h"

#include "js/UniquePtr.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/experimental/TypedData.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

namespace {

template <typename T> bool validate_type(T *chars, size_t strlen) {
  for (size_t i = 0; i < strlen; i++) {
    T c = chars[i];
    if (c < 0x20 || c > 0x7E) {
      return false;
    }
  }

  return true;
}

// 1. If type contains any characters outside the range U+0020 to U+007E, then set t to the empty string.
// 2. Convert every character in type to ASCII lowercase.
JSString *normalize_type(JSContext *cx, HandleValue value) {
  JS::RootedString value_str(cx);

  if (value.isObject() || value.isString()) {
    value_str = JS::ToString(cx, value);
    if (!value_str) {
      return nullptr;
    }
  } else if (value.isNull()) {
    return JS::ToString(cx, value);
  } else {
    return JS_GetEmptyString(cx);
  }

  auto *str = JS::StringToLinearString(cx, value_str);
  if (!str) {
    return nullptr;
  }

  std::string normalized;
  auto strlen = JS::GetLinearStringLength(str);

  if (strlen == 0U) {
    return JS_GetEmptyString(cx);
  }

  if (JS::LinearStringHasLatin1Chars(str)) {
    JS::AutoCheckCannotGC nogc(cx);
    const auto *chars = JS::GetLatin1LinearStringChars(nogc, str);
    if (!validate_type(chars, strlen)) {
      return JS_GetEmptyString(cx);
    }

    normalized = std::string(reinterpret_cast<const char *>(chars), strlen);
  } else {
    JS::AutoCheckCannotGC nogc(cx);
    const auto *chars = (JS::GetTwoByteLinearStringChars(nogc, str));
    if (!validate_type(chars, strlen)) {
      return JS_GetEmptyString(cx);
    }

    normalized.reserve(strlen);
    for (size_t i = 0; i < strlen; ++i) {
      normalized += static_cast<unsigned char>(chars[i]);
    }
  }

  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  return JS_NewStringCopyN(cx, normalized.c_str(), normalized.length());
}

// https://w3c.github.io/FileAPI/#convert-line-endings-to-native
std::string convert_line_endings_to_native(std::string_view s) {
  std::string native_line_ending = "\n";
#ifdef _WIN32
  native_line_ending = "\r\n";
#endif

  std::string result;
  result.reserve(s.size());

  size_t i = 0;
  while (i < s.size()) {
    switch (s[i]) {
    case '\r': {
      if (i + 1 < s.size() && s[i + 1] == '\n') {
        result.append(native_line_ending);
        i += 2;
      } else {
        result.append(native_line_ending);
        i += 1;
      }
      break;
    }
    case '\n': {
      result.append(native_line_ending);
      i += 1;
      break;
    }
    default: {
      result.push_back(s[i]);
      i += 1;
      break;
    }
    }
  }

  return result;
}

} // anonymous namespace



namespace builtins::web::blob {

using js::Vector;
using streams::BufReader;
using streams::NativeStreamSource;

#define DEFINE_BLOB_METHOD(name)                               \
bool Blob::name(JSContext *cx, unsigned argc, JS::Value *vp) { \
  METHOD_HEADER(0)                                             \
  return name(cx, self, args.rval());                          \
}

#define DEFINE_BLOB_METHOD_W_ARGS(name)                        \
bool Blob::name(JSContext *cx, unsigned argc, JS::Value *vp) { \
  METHOD_HEADER(0)                                             \
  return name(cx, self, args, args.rval());                    \
}

const JSFunctionSpec Blob::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Blob::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Blob::methods[] = {
    JS_FN("arrayBuffer", Blob::arrayBuffer, 0, JSPROP_ENUMERATE),
    JS_FN("bytes", Blob::bytes, 0, JSPROP_ENUMERATE),
    JS_FN("slice", Blob::slice, 0, JSPROP_ENUMERATE),
    JS_FN("stream", Blob::stream, 0, JSPROP_ENUMERATE),
    JS_FN("text", Blob::text, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec Blob::properties[] = {
    JS_PSG("size", size_get, JSPROP_ENUMERATE),
    JS_PSG("type", type_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Blob", JSPROP_READONLY),
    JS_PS_END,
};

JSObject *Blob::data_to_owned_array_buffer(JSContext *cx, HandleObject self) {
  auto *src = Blob::blob(self);
  auto size = src->length();

  auto buf = mozilla::MakeUnique<uint8_t[]>(size);
  if (!buf) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  std::copy_n(src->begin(), size, buf.get());

  auto *array_buffer = JS::NewArrayBufferWithContents(
      cx, size, buf.get(), JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory);
  if (!array_buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  // `array_buffer` now owns `buf`
  std::ignore = (buf.release());
  return array_buffer;
}

bool Blob::read_blob_slice(JSContext *cx, HandleObject self, std::span<uint8_t> buf,
                           size_t start, size_t *read, bool *done) {
  auto *src = Blob::blob(self);

  if (start >= src->length()) {
    *read = 0;
    *done = true;
    return true;
  }

  size_t available = src->length() - start;
  size_t to_read = std::min(buf.size(), available);

  std::copy_n(src->begin() + start, to_read, buf.data());
  *read = to_read;

 return true;
}

DEFINE_BLOB_METHOD(arrayBuffer)
DEFINE_BLOB_METHOD(bytes)
DEFINE_BLOB_METHOD(stream)
DEFINE_BLOB_METHOD(text)
DEFINE_BLOB_METHOD_W_ARGS(slice)

bool Blob::arrayBuffer(JSContext *cx, HandleObject self, MutableHandleValue rval) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise) {
    return false;
  }

  rval.setObject(*promise);

  auto *buffer = data_to_owned_array_buffer(cx, self);
  if (!buffer) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  JS::RootedValue result(cx);
  result.setObject(*buffer);
  JS::ResolvePromise(cx, promise, result);

  return true;
}

bool Blob::bytes(JSContext *cx, HandleObject self, MutableHandleValue rval) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise) {
    return false;
  }

  rval.setObject(*promise);

  JS::RootedObject buffer(cx, data_to_owned_array_buffer(cx, self));
  if (!buffer) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  auto len = JS::GetArrayBufferByteLength(buffer);
  JS::RootedObject byte_array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, len));
  if (!byte_array) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  JS::RootedValue result(cx);
  result.setObject(*byte_array);
  JS::ResolvePromise(cx, promise, result);

  return true;
}

bool Blob::slice(JSContext *cx, HandleObject self, const CallArgs &args, MutableHandleValue rval) {
  auto *src = Blob::blob(self);
  int64_t size = src->length();
  int64_t start = 0;
  int64_t end = size;

  JS::RootedString contentType(cx, JS_GetEmptyString(cx));

  if (args.hasDefined(0)) {
    HandleValue start_val = args.get(0);
    if (!JS::ToInt64(cx, start_val, &start)) {
      return false;
    }
  }

  if (args.hasDefined(1)) {
    HandleValue end_val = args.get(1);
    if (!JS::ToInt64(cx, end_val, &end)) {
      return false;
    }
  }

  if (args.hasDefined(2)) {
    HandleValue contentType_val = args.get(2);
    if ((contentType = normalize_type(cx, contentType_val)) == nullptr) {
      return false;
    }
  }

  // A negative value is treated as an offset from the end of the Blob toward the beginning.
  start = (start < 0) ? std::max((size + start), 0LL) : std::min(start, size);
  end = (end < 0) ? std::max((size + end), 0LL) : std::min(end, size);

  auto slice_len = std::max(end - start, 0LL);
  auto dst = (slice_len > 0) ? UniqueChars(js_pod_malloc<char>(slice_len)) : nullptr;

  if (dst) {
    std::copy(src->begin() + start, src->begin() + end, dst.get());
  }

  JS::RootedObject new_blob(cx, create(cx, std::move(dst), slice_len, contentType));
  if (!new_blob) {
    return false;
  }

  rval.setObject(*new_blob);
  return true;
}

bool Blob::stream(JSContext *cx, HandleObject self, MutableHandleValue rval) {
  RootedObject reader(cx, BufReader::create(cx, self, read_blob_slice));
  if (!reader) {
    return false;
  }

  RootedObject native_stream(cx, BufReader::stream(reader));
  RootedObject default_stream(cx, NativeStreamSource::stream(native_stream));

  rval.setObject(*default_stream);
  return true;
}

bool Blob::text(JSContext *cx, HandleObject self, MutableHandleValue rval) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise) {
    return false;
  }

  rval.setObject(*promise);

  auto *src = Blob::blob(self);

  const char* utf8_label = "UTF-8";
  const auto *encoding =
      jsencoding::encoding_for_label_no_replacement(reinterpret_cast<const uint8_t *>(utf8_label), 5);

  auto deleter = [&](jsencoding::Decoder *dec) { jsencoding::decoder_free(dec); };
  std::unique_ptr<jsencoding::Decoder, decltype(deleter)> decoder(
      jsencoding::encoding_new_decoder_with_bom_removal(encoding), deleter);

  MOZ_ASSERT(decoder);

  auto src_len = src->length();
  auto dst_len = jsencoding::decoder_max_utf16_buffer_length(decoder.get(), src_len);

  JS::UniqueTwoByteChars dst(static_cast<char16_t *>(js_pod_malloc<char16_t>(dst_len + 1)));
  if (!dst) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  bool had_replacements = false;
  auto *dst_data = reinterpret_cast<uint16_t *>(dst.get());

  jsencoding::decoder_decode_to_utf16(decoder.get(), src->begin(), &src_len, dst_data, &dst_len,
                                      true, &had_replacements);

  JS::RootedString str(cx, JS_NewUCString(cx, std::move(dst), dst_len));
  if (!str) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  JS::RootedValue result(cx);
  result.setString(str);
  JS::ResolvePromise(cx, promise, result);

  return true;
}

bool Blob::size_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "size get", "Blob");
  }

  auto size = Blob::blob_size(self);
  args.rval().setNumber(size);
  return true;
}

bool Blob::type_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "type get", "Blob");
  }

  auto *type = Blob::type(self);
  args.rval().setString(type);
  return true;
}

Blob::ByteBuffer *Blob::blob(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto *blob = static_cast<ByteBuffer *>(
      JS::GetReservedSlot(self, static_cast<size_t>(Blob::Slots::Data)).toPrivate());

  MOZ_ASSERT(blob);
  return blob;
}

size_t Blob::blob_size(JSObject *self) {
  return blob(self)->length();
}

JSString *Blob::type(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return JS::GetReservedSlot(self, static_cast<size_t>(Blob::Slots::Type)).toString();
}

Blob::LineEndings Blob::line_endings(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return static_cast<LineEndings>(
      JS::GetReservedSlot(self, static_cast<size_t>(Blob::Slots::Endings)).toInt32());
}

bool Blob::append_value(JSContext *cx, HandleObject self, HandleValue val) {
  auto *blob = Blob::blob(self);

  if (val.isObject()) {
    RootedObject obj(cx, &val.toObject());

    if (Blob::is_instance(obj)) {
      auto *src = Blob::blob(obj);
      return blob->append(src->begin(), src->end());
    } if (JS_IsArrayBufferViewObject(obj) || JS::IsArrayBufferObject(obj)) {
      auto span = value_to_buffer(cx, val, "Blob Parts");
      if (span.has_value()) {
        auto *src = span->data();
        auto len = span->size();
        return blob->append(src, src + len);
      }

      return true;
    }
  } else if (val.isString()) {
    auto chars = core::encode(cx, val);
    if (!chars) {
      return false;
    }

    if (line_endings(self) == LineEndings::Native) {
      auto converted = convert_line_endings_to_native(chars);
      auto *src = converted.data();
      auto len = converted.length();
      return blob->append(src, src + len);
    }

    return blob->append(chars.ptr.get(), chars.ptr.get() + chars.len);
  }

  // FALLBACK: if we ever get here convert, to string and call append again
  auto *str = JS::ToString(cx, val);
  if (!str) {
    return false;
  }

  RootedValue str_val(cx, JS::StringValue(str));
  return append_value(cx, self, str_val);
}

bool Blob::init_blob_parts(JSContext *cx, HandleObject self, HandleValue value) {
  JS::ForOfIterator it(cx);
  if (!it.init(value, JS::ForOfIterator::AllowNonIterable)) {
    return false;
  }

  bool is_iterable = value.isObject() && it.valueIsIterable();

  if (is_iterable) {
    // if the object is an iterable, walk over its elements...
    JS::Rooted<JS::Value> item(cx);
    while (true) {
      bool done = false;

      if (!it.next(&item, &done)) {
        return false;
      }
      if (done) {
        break;
      }

      if (!append_value(cx, self, item)) {
        return false;
      }
    }

    return true;
  }     // non-objects are not allowed for the blobParts
    return api::throw_error(cx, api::Errors::TypeError, "Blob.constructor", "blobParts", "be an object");
 
}

bool Blob::init_options(JSContext *cx, HandleObject self, HandleValue initv) {
  JS::RootedValue init_val(cx, initv);

  if (!init_val.isObject()) {
    return api::throw_error(cx, api::Errors::TypeError, "Blob.constructor", "options", "be an object");
  }

  // `options` is an object which may specify any of the properties:
  // - `type`: the MIME type of the data that will be stored into the blob,
  // - `endings`: how to interpret newline characters (\n) within the contents.
  JS::RootedObject opts(cx, init_val.toObjectOrNull());
  bool has_endings = false;
  bool has_type = false;

  if (!JS_HasProperty(cx, opts, "endings", &has_endings) ||
      !JS_HasProperty(cx, opts, "type", &has_type)) {
    return false;
  }

  if (!has_type && !has_endings) {
    // Use defaults
    return true;
  }

  if (has_endings) {
    JS::RootedValue endings_val(cx);
    bool is_transparent = false;
    bool is_native = false;
    if (!JS_GetProperty(cx, opts, "endings", &endings_val)) {
      return false;
    }

    auto *endings_str = JS::ToString(cx, endings_val);
    if (!JS_StringEqualsLiteral(cx, endings_str, "transparent", &is_transparent) ||
        !JS_StringEqualsLiteral(cx, endings_str, "native", &is_native)) {
      return false;
    }

    if (is_transparent || is_native) {
      auto endings = is_native ? LineEndings::Native : LineEndings::Transparent;
      SetReservedSlot(self, static_cast<uint32_t>(Slots::Endings), JS::Int32Value(endings));
    }
  }

  if (has_type) {
    JS::RootedValue type(cx);
    if (!JS_GetProperty(cx, opts, "type", &type)) {
      return false;
    }

    auto *type_str = normalize_type(cx, type);
    if (!type_str) {
      return false;
    }
    SetReservedSlot(self, static_cast<uint32_t>(Slots::Type), JS::StringValue(type_str));
  }

  return true;
}

JSObject *Blob::create(JSContext *cx, UniqueChars data, size_t data_len, HandleString type) {
  JSObject *self = JS_NewObjectWithGivenProto(cx, &class_, proto_obj);
  if (!self) {
    return nullptr;
  }

  auto blob = js::MakeUnique<ByteBuffer>();
  if (!blob) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  if (data != nullptr) {
    // Take the ownership of given data.
    blob->replaceRawBuffer(reinterpret_cast<uint8_t *>(data.release()), data_len);
  }

  SetReservedSlot(self, static_cast<uint32_t>(Slots::Data), JS::PrivateValue(blob.release()));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Type), JS::StringValue(type));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Endings), JS::Int32Value(LineEndings::Transparent));
  return self;
}

bool Blob::init(JSContext *cx, HandleObject self, HandleValue blobParts, HandleValue opts) {
  auto blob = js::MakeUnique<ByteBuffer>();
  if (blob == nullptr) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  SetReservedSlot(self, static_cast<uint32_t>(Slots::Type), JS_GetEmptyStringValue(cx));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Endings), JS::Int32Value(LineEndings::Transparent));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Data), JS::PrivateValue(blob.release()));

  // Walk the blob parts and append them to the blob's buffer.
  if (blobParts.isNull()) {
    return api::throw_error(cx, api::Errors::TypeError, "Blob.constructor", "blobParts", "be an object");
  }

  if (!blobParts.isUndefined() && !init_blob_parts(cx, self, blobParts)) {
    return false;
  }

  if (!opts.isNullOrUndefined() && !init_options(cx, self, opts)) {
    return false;
  }

  return true;
}

bool Blob::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("Blob", 0);

  RootedValue blobParts(cx, args.get(0));
  RootedValue opts(cx, args.get(1));
  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));

  if (!self) {
    return false;
  }

  if (!init(cx, self, blobParts, opts)) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

bool Blob::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

void Blob::finalize(JS::GCContext *gcx, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto *blob = Blob::blob(self);
  if (blob) {
    js_delete(blob);
  }
}

bool install(api::Engine *engine) {
  return Blob::init_class(engine->cx(), engine->global());
}

} // namespace builtins::web::blob


