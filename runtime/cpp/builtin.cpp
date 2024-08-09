#include "builtin.h"

static const JSErrorFormatString *GetErrorMessageFromRef(void *userRef, unsigned errorNumber) {
  auto error = static_cast<JSErrorFormatString *>(userRef);

  JS::ConstUTF8CharsZ(error->format, strlen(error->format));
  return error;
}

bool api::throw_error(JSContext* cx, const JSErrorFormatString &error,
                      const char* arg1, const char* arg2, const char* arg3, const char* arg4) {
  const char** args = nullptr;
  const char* list[4] = { arg1, arg2, arg3, arg4 };
  if (arg1) {
    args = list;
  }

  JS_ReportErrorNumberUTF8Array(cx, GetErrorMessageFromRef,
    const_cast<JSErrorFormatString*>(&error), 0, args);
  return false;
}

std::optional<std::span<uint8_t>> value_to_buffer(JSContext *cx, JS::HandleValue val,
                                                  const char *val_desc) {
  if (!val.isObject() ||
      !(JS_IsArrayBufferViewObject(&val.toObject()) || JS::IsArrayBufferObject(&val.toObject()))) {
    api::throw_error(cx, api::Errors::InvalidBuffer, val_desc);
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

bool RejectPromiseWithPendingError(JSContext *cx, HandleObject promise) {
  RootedValue exn(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &exn)) {
    return false;
  }
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx, promise, exn);
}

JSObject *PromiseRejectedWithPendingError(JSContext *cx) {
  RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !RejectPromiseWithPendingError(cx, promise)) {
    return nullptr;
  }
  return promise;
}

enum class Mode { PreWizening, PostWizening };

Mode execution_mode = Mode::PreWizening;

bool hasWizeningFinished() { return execution_mode == Mode::PostWizening; }

bool isWizening() { return execution_mode == Mode::PreWizening; }

void markWizeningAsFinished() { execution_mode = Mode::PostWizening; }
