#ifndef BUILTINS_WEB_BASE64_H
#define BUILTINS_WEB_BASE64_H

#include "../builtin.h"

namespace builtins {
namespace web {
namespace base64 {

bool add_to_global(JSContext *cx, JS::HandleObject global);

extern const uint8_t base64DecodeTable[128];
extern const uint8_t base64URLDecodeTable[128];
extern const char base64EncodeTable[65];
extern const char base64URLEncodeTable[65];

std::string forgivingBase64Encode(std::string_view data,
                                  const char *encodeTable);
JS::Result<std::string> forgivingBase64Decode(std::string_view data,
                                              const uint8_t *decodeTable);

JS::Result<std::string> convertJSValueToByteString(JSContext *cx,
                                                   std::string v);

} // namespace base64
} // namespace web
} // namespace builtins

#endif
