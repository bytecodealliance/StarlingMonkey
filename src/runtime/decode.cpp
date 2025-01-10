#include "encode.h"

namespace core {

JSString *decode(JSContext *cx, string_view str) {
  JS::UTF8Chars ret_chars(str.data(), str.length());
  return JS_NewStringCopyUTF8N(cx, ret_chars);
}

JSString *decode_byte_string(JSContext *cx, string_view str) {
  JS::UniqueLatin1Chars chars(
      static_cast<JS::Latin1Char *>(std::memcpy(malloc(str.size()), str.data(), str.size())));
  return JS_NewLatin1String(cx, std::move(chars), str.length());
}

} // namespace core
