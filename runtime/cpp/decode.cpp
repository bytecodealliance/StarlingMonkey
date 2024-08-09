#include "encode.h"

namespace core {

JSString* decode(JSContext* cx, string_view str) {
  JS::UTF8Chars ret_chars(str.data(), str.length());
  return JS_NewStringCopyUTF8N(cx, ret_chars);
}

} // namespace core
