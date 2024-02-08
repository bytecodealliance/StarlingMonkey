#ifndef fastly_sys_h
#define fastly_sys_h

#include <optional>
#include <span>

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#pragma clang diagnostic pop

enum JSErrNum {
#define MSG_DEF(name, count, exception, format) name,
#include "./error-numbers.msg"
#undef MSG_DEF
  JSErrNum_Limit
};

struct JSErrorFormatString;

const JSErrorFormatString js_ErrorFormatString[JSErrNum_Limit] = {
#define MSG_DEF(name, count, exception, format) {#name, format, count, exception},
#include "./error-numbers.msg"
#undef MSG_DEF
};

#include "host_interface/host_api.h"

const JSErrorFormatString *GetErrorMessage(void *userRef, unsigned errorNumber);

JSObject *PromiseRejectedWithPendingError(JSContext *cx);
inline bool ReturnPromiseRejectedWithPendingError(JSContext *cx, const JS::CallArgs &args) {
  JSObject *promise = PromiseRejectedWithPendingError(cx);
  if (!promise) {
    return false;
  }

  args.rval().setObject(*promise);
  return true;
}

std::optional<std::span<uint8_t>> value_to_buffer(JSContext *cx, JS::HandleValue val,
                                                  const char *val_desc);

using InternalMethod = bool(JSContext *cx, JS::HandleObject receiver, JS::HandleValue extra,
                            JS::CallArgs args);

template <InternalMethod fun> bool internal_method(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  JS::RootedValue extra(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  return fun(cx, self, extra, args);
}

template <InternalMethod fun>
JSObject *create_internal_method(JSContext *cx, JS::HandleObject receiver,
                                 JS::HandleValue extra = JS::UndefinedHandleValue,
                                 unsigned int nargs = 0, const char *name = "") {
  JSFunction *method = js::NewFunctionWithReserved(cx, internal_method<fun>, 1, 0, name);
  if (!method)
    return nullptr;
  JS::RootedObject method_obj(cx, JS_GetFunctionObject(method));
  js::SetFunctionNativeReserved(method_obj, 0, JS::ObjectValue(*receiver));
  js::SetFunctionNativeReserved(method_obj, 1, extra);
  return method_obj;
}

bool hasWizeningFinished();
bool isWizening();
void markWizeningAsFinished();

class FastlyOptions {
private:
  bool experimental_high_resolution_time_methods_enabled = false;

public:
  FastlyOptions() = default;

  bool getExperimentalHighResolutionTimeMethodsEnabled() {
    return this->experimental_high_resolution_time_methods_enabled;
  };
  void setExperimentalHighResolutionTimeMethodsEnabled(bool set) {
    this->experimental_high_resolution_time_methods_enabled = set;
  };
};

bool define_fastly_sys(JSContext *cx, JS::HandleObject global, FastlyOptions options);

bool RejectPromiseWithPendingError(JSContext *cx, JS::HandleObject promise);

namespace GlobalProperties {
extern const uint8_t base64DecodeTable[128];
extern const uint8_t base64URLDecodeTable[128];
extern const char base64EncodeTable[65];
extern const char base64URLEncodeTable[65];

std::string forgivingBase64Encode(std::string_view data, const char *encodeTable);
JS::Result<std::string> forgivingBase64Decode(std::string_view data, const uint8_t *decodeTable);

JS::Result<std::string> convertJSValueToByteString(JSContext *cx, std::string v);
} // namespace GlobalProperties

bool debug_logging_enabled();
bool dump_value(JSContext *cx, JS::Value value, FILE *fp);
void dump_promise_rejection(JSContext *cx, JS::HandleValue reason, JS::HandleObject promise,
                            FILE *fp);
bool print_stack(JSContext *cx, FILE *fp);
bool print_stack(JSContext *cx, JS::HandleObject stack, FILE *fp);

bool reactor_main(host_api::Request req);

#endif // fastly_sys_h
