#include "builtins/shared/text-decoder.h"
#include "builtin.h"
#include "core/encode.h"
#include "js-compute-builtins.h"
#include "rust-encoding/rust-encoding.h"

namespace builtins {

bool TextDecoder::decode(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  auto source_value = args.get(0);
  std::optional<std::span<uint8_t>> src;

  if (source_value.isUndefined()) {
    src = std::span<uint8_t, 0>();
  } else {
    src = value_to_buffer(cx, source_value, "TextDecoder#decode: input");
  }
  if (!src.has_value()) {
    return false;
  }

  bool stream = false;
  if (args.hasDefined(1)) {
    auto options_value = args.get(1);
    if (!options_value.isObject()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEXT_DECODER_DECODE_OPTIONS_NOT_DICTIONARY);
      return false;
    }
    JS::RootedObject options(cx, &options_value.toObject());
    JS::RootedValue stream_value(cx);
    if (!JS_GetProperty(cx, options, "stream", &stream_value)) {
      return false;
    }
    stream = JS::ToBoolean(stream_value);
  }

  auto fatal =
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Fatal)).toBoolean();
  auto ignoreBOM =
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::IgnoreBOM)).toBoolean();
  auto decoder = reinterpret_cast<jsencoding::Decoder *>(
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Decoder)).toPrivate());
  MOZ_ASSERT(decoder);

  uint32_t result;
  size_t srcLen = src->size();
  size_t destLen = jsencoding::decoder_max_utf16_buffer_length(decoder, srcLen);
  std::unique_ptr<uint16_t[]> dest(new uint16_t[destLen + 1]);
  if (!dest) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  if (fatal) {
    result = jsencoding::decoder_decode_to_utf16_without_replacement(decoder, src->data(), &srcLen,
                                                                     dest.get(), &destLen, !stream);
    if (result != 0) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TEXT_DECODER_DECODING_FAILED);
      return false;
    }
  } else {
    bool hadReplacements;
    result = jsencoding::decoder_decode_to_utf16(decoder, src->data(), &srcLen, dest.get(),
                                                 &destLen, !stream, &hadReplacements);
  }
  MOZ_ASSERT(result == 0);

  auto encoding = reinterpret_cast<jsencoding::Encoding *>(
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Encoding)).toPrivate());
  MOZ_ASSERT(encoding);
  // If the internal streaming flag of the decoder object is not set,
  // then reset the encoding algorithm state to the default values
  if (!stream) {
    if (ignoreBOM) {
      jsencoding::encoding_new_decoder_without_bom_handling_into(encoding, decoder);
    } else {
      jsencoding::encoding_new_decoder_with_bom_removal_into(encoding, decoder);
    }
  }

  JS::RootedString str(cx,
                       JS_NewUCStringCopyN(cx, reinterpret_cast<char16_t *>(dest.get()), destLen));
  if (!str) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TEXT_DECODER_DECODING_FAILED);
    return false;
  }

  args.rval().setString(str);
  return true;
}

bool TextDecoder::encoding_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  JS::RootedObject result(cx);
  if (!JS_GetPrototype(cx, self, &result)) {
    return false;
  }
  if (result != TextDecoder::proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_INTERFACE,
                              "encoding get", "TextDecoder");
    return false;
  }

  auto encoding = reinterpret_cast<jsencoding::Encoding *>(
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Encoding)).toPrivate());
  MOZ_ASSERT(encoding);

  std::unique_ptr<uint8_t[]> name(new uint8_t[jsencoding::ENCODING_NAME_MAX_LENGTH]);
  if (!name) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  size_t length = jsencoding::encoding_name(encoding, name.get());
  // encoding_rs/jsencoding returns the name uppercase but we need to have it lowercased
  for (size_t i = 0; i < length; i++) {
    name[i] = std::tolower(name[i]);
  }
  JS::RootedString str(cx, JS_NewStringCopyN(cx, reinterpret_cast<char *>(name.get()), length));
  if (!str) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  args.rval().setString(str);
  return true;
}

bool TextDecoder::fatal_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  JS::RootedObject result(cx);
  if (!JS_GetPrototype(cx, self, &result)) {
    return false;
  }
  if (result != TextDecoder::proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_INTERFACE,
                              "fatal get", "TextDecoder");
    return false;
  }

  auto fatal =
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Fatal)).toBoolean();

  args.rval().setBoolean(fatal);
  return true;
}

bool TextDecoder::ignoreBOM_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  JS::RootedObject result(cx);
  if (!JS_GetPrototype(cx, self, &result)) {
    return false;
  }
  if (result != TextDecoder::proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_INTERFACE,
                              "ignoreBOM get", "TextDecoder");
    return false;
  }

  auto ignoreBOM =
      JS::GetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::IgnoreBOM)).toBoolean();

  args.rval().setBoolean(ignoreBOM);
  return true;
}

const JSFunctionSpec TextDecoder::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec TextDecoder::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec TextDecoder::methods[] = {
    JS_FN("decode", decode, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec TextDecoder::properties[] = {
    JS_PSG("encoding", encoding_get, JSPROP_ENUMERATE),
    JS_PSG("fatal", fatal_get, JSPROP_ENUMERATE),
    JS_PSG("ignoreBOM", ignoreBOM_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "TextDecoder", JSPROP_READONLY),
    JS_PS_END,
};

// constructor(optional DOMString label = "utf-8", optional TextDecoderOptions options = {});
bool TextDecoder::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("TextDecoder", 0);
  // 1. Let encoding be the result of getting an encoding from label.
  auto label_value = args.get(0);
  // https://encoding.spec.whatwg.org/#concept-encoding-get
  // To get an encoding from a string label, run these steps:
  // 1. Remove any leading and trailing ASCII whitespace from label.
  // 2. If label is an ASCII case-insensitive match for any of the labels listed in the table
  // below, then return the corresponding encoding; otherwise return failure. JS-Compute-Runtime:
  jsencoding::Encoding *encoding;
  if (label_value.isUndefined()) {
    encoding = const_cast<jsencoding::Encoding *>(jsencoding::encoding_for_label_no_replacement(
        reinterpret_cast<uint8_t *>(const_cast<char *>("UTF-8")), 5));
  } else {
    auto label_chars = core::encode(cx, label_value);
    if (!label_chars) {
      return false;
    }
    encoding = const_cast<jsencoding::Encoding *>(jsencoding::encoding_for_label_no_replacement(
        reinterpret_cast<uint8_t *>(label_chars.begin()), label_chars.len));
  }
  if (!encoding) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TEXT_DECODER_INVALID_ENCODING);
    return false;
  }
  bool fatal = false;
  bool ignoreBOM = false;
  if (args.hasDefined(1)) {
    auto options_val = args.get(1);
    if (options_val.isObject()) {
      JS::RootedObject options(cx, &options_val.toObject());
      JS::RootedValue fatal_value(cx);
      if (!JS_GetProperty(cx, options, "fatal", &fatal_value)) {
        return false;
      }
      fatal = JS::ToBoolean(fatal_value);
      JS::RootedValue ignoreBOM_value(cx);
      if (!JS_GetProperty(cx, options, "ignoreBOM", &ignoreBOM_value)) {
        return false;
      }
      ignoreBOM = JS::ToBoolean(ignoreBOM_value);
    } else if (!options_val.isNull()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEXT_DECODER_OPTIONS_NOT_DICTIONARY);
      return false;
    }
  }
  JS::RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  jsencoding::Decoder *decoder;
  if (ignoreBOM) {
    decoder = jsencoding::encoding_new_decoder_without_bom_handling(encoding);
  } else {
    decoder = jsencoding::encoding_new_decoder_with_bom_removal(encoding);
  }
  JS::SetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Decoder),
                      JS::PrivateValue(decoder));
  JS::SetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Encoding),
                      JS::PrivateValue(encoding));
  JS::SetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::Fatal),
                      JS::BooleanValue(fatal));
  JS::SetReservedSlot(self, static_cast<uint32_t>(TextDecoder::Slots::IgnoreBOM),
                      JS::BooleanValue(ignoreBOM));

  args.rval().setObject(*self);
  return true;
}

bool TextDecoder::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

} // namespace builtins
