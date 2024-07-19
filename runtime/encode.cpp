#include "encode.h"

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/Conversions.h"
#pragma clang diagnostic pop

namespace core {

using host_api::HostBytes;
using host_api::HostString;

HostString encode(JSContext *cx, JS::HandleString str) {
  HostString res;
  res.ptr = JS_EncodeStringToUTF8(cx, str);
  if (res.ptr) {
    // This shouldn't fail, since the encode operation ensured `str` is linear.
    JSLinearString *linear = JS_EnsureLinearString(cx, str);
    res.len = JS::GetDeflatedUTF8StringLength(linear);
  }

  return res;
}

HostString encode(JSContext *cx, JS::HandleValue val) {
  JS::RootedString str(cx, JS::ToString(cx, val));
  if (!str) {
    return HostString{};
  }

  return encode(cx, str);
}

HostString encode_byte_string(JSContext *cx, JS::HandleValue val) {
  JS::RootedString str(cx, JS::ToString(cx, val));
  if (!str) {
    return HostString{};
  }
  size_t length;
  if (!JS::StringHasLatin1Chars(str)) {
    bool foundBadChar = false;
    {
      JS::AutoCheckCannotGC nogc;
      const char16_t* chars = JS_GetTwoByteStringCharsAndLength(cx, nogc, str, &length);
      if (!chars) {
        foundBadChar = true;
      }
      else {
        for (size_t i = 0; i < length; i++) {
          if (chars[i] > 255) {
            foundBadChar = true;
            break;
          }
        }
      }
    }
    if (foundBadChar) {
      api::throw_error(cx, core::ByteStringEncodingError);
      return host_api::HostString{};
    }
  } else {
    length = JS::GetStringLength(str);
  }
  char *buf = static_cast<char *>(malloc(length));
  if (!JS_EncodeStringToBuffer(cx, str, buf, length))
    MOZ_ASSERT_UNREACHABLE();
  return HostString(JS::UniqueChars(buf), length);
}

jsurl::SpecString encode_spec_string(JSContext *cx, JS::HandleValue val) {
  jsurl::SpecString slice(nullptr, 0, 0);
  auto chars = encode(cx, val);
  if (chars.ptr) {
    slice.data = (uint8_t *)chars.ptr.release();
    slice.len = chars.len;
    slice.cap = chars.len;
  }
  return slice;
}

} // namespace core
