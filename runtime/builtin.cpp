#include "builtin.h"

const JSErrorFormatString *GetErrorMessageBuiltin(void *userRef, unsigned errorNumber) {
  if (errorNumber > 0 && errorNumber < JSBuiltinErrNum_Limit) {
    return &js_ErrorFormatStringBuiltin[errorNumber];
  }

  return nullptr;
}

std::optional<std::span<uint8_t>> value_to_buffer(JSContext *cx, JS::HandleValue val,
                                                  const char *val_desc) {
  if (!val.isObject() ||
      !(JS_IsArrayBufferViewObject(&val.toObject()) || JS::IsArrayBufferObject(&val.toObject()))) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessageBuiltin, nullptr, JSMSG_INVALID_BUFFER_ARG,
                             val_desc, val.type());
    return std::nullopt;
  }

  JS::RootedObject input(cx, &val.toObject());
  uint8_t *data;
  bool is_shared;
  size_t len = 0;

  if (JS_IsArrayBufferViewObject(input)) {
    js::GetArrayBufferViewLengthAndData(input, &len, &is_shared, &data);
  } else {
    JS::GetArrayBufferLengthAndData(input, &len, &is_shared, &data);
  }

  return std::span(data, len);
}
