#include "builtins/shared/text-encoder.h"
#include "core/encode.h"
#include "js-compute-builtins.h"
#include <iostream>
#include <tuple>

#include "js/ArrayBuffer.h"
#include "mozilla/Span.h"
// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/experimental/TypedData.h"
#pragma clang diagnostic pop

namespace builtins {

bool TextEncoder::encode(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  // Default to empty string if no input is given.
  if (args.get(0).isUndefined()) {
    JS::RootedObject byte_array(cx, JS_NewUint8Array(cx, 0));
    if (!byte_array) {
      return false;
    }

    args.rval().setObject(*byte_array);
    return true;
  }

  auto chars = core::encode(cx, args[0]);
  JS::RootedObject buffer(cx, JS::NewArrayBufferWithContents(cx, chars.len, chars.begin()));
  if (!buffer) {
    return false;
  }

  // `buffer` now owns `chars`
  static_cast<void>(chars.ptr.release());

  JS::RootedObject byte_array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, chars.len));
  if (!byte_array) {
    return false;
  }

  args.rval().setObject(*byte_array);
  return true;
}

bool TextEncoder::encodeInto(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2);

  auto source = JS::ToString(cx, args.get(0));
  if (!source) {
    return false;
  }
  auto destination_value = args.get(1);

  if (!destination_value.isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEXT_ENCODER_ENCODEINTO_INVALID_ARRAY);
    return false;
  }
  JS::RootedObject destination(cx, &destination_value.toObject());
  if (!JS_IsArrayBufferViewObject(destination)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEXT_ENCODER_ENCODEINTO_INVALID_ARRAY);
    return false;
  }
  if (JS::IsLargeArrayBufferView(destination)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEXT_ENCODER_ENCODEINTO_INVALID_ARRAY);
    return false;
  }

  uint8_t *data;
  bool is_shared;
  size_t len = 0;
  if (!JS_GetObjectAsUint8Array(destination, &len, &is_shared, &data)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEXT_ENCODER_ENCODEINTO_INVALID_ARRAY);
    return false;
  }
  auto span = AsWritableChars(mozilla::Span(data, len));
  auto maybe = JS_EncodeStringToUTF8BufferPartial(cx, source, span);
  if (!maybe) {
    return false;
  }
  size_t read;
  size_t written;
  std::tie(read, written) = *maybe;

  MOZ_ASSERT(written <= len);

  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return false;
  }
  JS::RootedValue read_value(cx, JS::NumberValue(read));
  JS::RootedValue written_value(cx, JS::NumberValue(written));
  if (!JS_SetProperty(cx, obj, "read", read_value)) {
    return false;
  }
  if (!JS_SetProperty(cx, obj, "written", written_value)) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

bool TextEncoder::encoding_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  JS::RootedObject result(cx);
  if (!JS_GetPrototype(cx, self, &result)) {
    return false;
  }
  if (result != TextEncoder::proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_INTERFACE,
                              "encoding get", "TextEncoder");
    return false;
  }

  JS::RootedString str(cx, JS_NewStringCopyN(cx, "utf-8", 5));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

const JSFunctionSpec TextEncoder::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec TextEncoder::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec TextEncoder::methods[] = {
    JS_FN("encode", encode, 0, JSPROP_ENUMERATE),
    JS_FN("encodeInto", encodeInto, 2, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec TextEncoder::properties[] = {
    JS_PSG("encoding", encoding_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "TextEncoder", JSPROP_READONLY),
    JS_PS_END,
};

bool TextEncoder::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("TextEncoder", 0);

  JS::RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

bool TextEncoder::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

} // namespace builtins
