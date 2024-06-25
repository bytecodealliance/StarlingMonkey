#ifndef BUILTINS_WEB_BASE64_H
#define BUILTINS_WEB_BASE64_H

#include "extension-api.h"

namespace builtins {
namespace web {
namespace base64 {

bool install(api::Engine *engine);

extern const uint8_t base64DecodeTable[128];
extern const uint8_t base64URLDecodeTable[128];
extern const char base64EncodeTable[65];
extern const char base64URLEncodeTable[65];

std::string forgivingBase64Encode(std::string_view data, const char *encodeTable);
JS::Result<std::string> forgivingBase64Decode(std::string_view data, const uint8_t *decodeTable);

JS::Result<std::string> valueToJSByteString(JSContext *cx, HandleValue v);
JS::Result<std::string> stringToJSByteString(JSContext *cx, std::string v);

} // namespace base64
} // namespace web
} // namespace builtins

#endif
