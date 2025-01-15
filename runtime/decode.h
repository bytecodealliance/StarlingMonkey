#ifndef JS_COMPUTE_RUNTIME_DECODE_H
#define JS_COMPUTE_RUNTIME_DECODE_H

namespace core {

JSString* decode(JSContext *cx, std::string_view str);
JSString* decode_byte_string(JSContext* cx, std::string_view str);

} // namespace core

#endif
