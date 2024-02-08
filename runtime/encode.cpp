#include "core/encode.h"

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/Conversions.h"
#include "rust-url/rust-url.h"
#pragma clang diagnostic pop

namespace core {

host_api::HostString encode(JSContext *cx, JS::HandleString str) {
  host_api::HostString res;
  res.ptr = JS_EncodeStringToUTF8(cx, str);
  if (res.ptr) {
    // This shouldn't fail, since the encode operation ensured `str` is linear.
    JSLinearString *linear = JS_EnsureLinearString(cx, str);
    res.len = JS::GetDeflatedUTF8StringLength(linear);
  }

  return res;
}

host_api::HostString encode(JSContext *cx, JS::HandleValue val) {
  JS::RootedString str(cx, JS::ToString(cx, val));
  if (!str) {
    return host_api::HostString{};
  }

  return encode(cx, str);
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
