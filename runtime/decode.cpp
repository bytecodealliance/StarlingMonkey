#include "encode.h"

namespace core {

JSString* decode(JSContext* cx, host_api::HostString& str) {
  JS::UTF8Chars ret_chars(str.ptr.get(), str.len);
  return JS_NewStringCopyUTF8N(cx, ret_chars);
}

} // namespace core
