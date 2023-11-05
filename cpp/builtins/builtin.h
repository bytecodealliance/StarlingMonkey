#ifndef JS_COMPUTE_RUNTIME_BUILTIN_H
#define JS_COMPUTE_RUNTIME_BUILTIN_H

#include "engine.h"

#include <optional>
#include <span>
#include <tuple>

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/experimental/TypedData.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "rust-url/rust-url.h"
#pragma clang diagnostic pop

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::UniqueChars;

using JS::ObjectOrNullValue;
using JS::ObjectValue;
using JS::PrivateValue;
using JS::Value;

using JS::RootedObject;
using JS::RootedString;
using JS::RootedValue;

using JS::HandleObject;
using JS::HandleString;
using JS::HandleValue;
using JS::HandleValueArray;
using JS::MutableHandleValue;

using JS::PersistentRooted;

std::optional<std::span<uint8_t>>
value_to_buffer(JSContext *cx, JS::HandleValue val, const char *val_desc);
enum JSErrNum {
#define MSG_DEF(name, count, exception, format) name,
#include "./error-numbers.msg"
#undef MSG_DEF
  JSErrNum_Limit
};

bool hasWizeningFinished();
bool isWizening();
void markWizeningAsFinished();

const JSErrorFormatString js_ErrorFormatString[JSErrNum_Limit] = {
#define MSG_DEF(name, count, exception, format)                                \
  {#name, format, count, exception},
#include "./error-numbers.msg"
#undef MSG_DEF
};

const JSErrorFormatString *GetErrorMessage(void *userRef, unsigned errorNumber);

#define DBG(...)                                                               \
  printf("%s#%d: ", __func__, __LINE__);                                       \
  printf(__VA_ARGS__);                                                         \
  fflush(stdout);

#define MULTI_VALUE_HOSTCALL(op, accum)                                        \
  uint32_t cursor = 0;                                                         \
  int64_t ending_cursor = 0;                                                   \
  size_t nwritten;                                                             \
                                                                               \
  while (true) {                                                               \
    op                                                                         \
                                                                               \
        if (nwritten == 0) {                                                   \
      break;                                                                   \
    }                                                                          \
                                                                               \
    accum                                                                      \
                                                                               \
        if (ending_cursor < 0) {                                               \
      break;                                                                   \
    }                                                                          \
                                                                               \
    cursor = (uint32_t)ending_cursor;                                          \
  }

// Define this to make most methods print their name to stderr when invoked.
// #define TRACE_METHOD_CALLS

#ifdef TRACE_METHOD_CALLS
#define TRACE_METHOD(name) DBG("%s\n", name)
#else
#define TRACE_METHOD(name)
#endif

#define METHOD_HEADER_WITH_NAME(required_argc, name)                           \
  TRACE_METHOD(name)                                                           \
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);                            \
  if (!check_receiver(cx, args.thisv(), name))                                 \
    return false;                                                              \
  JS::RootedObject self(cx, &args.thisv().toObject());                         \
  if (!args.requireAtLeast(cx, name, required_argc))                           \
    return false;

// This macro:
// - Declares a `JS::CallArgs args` which contains the arguments provided to the
// method
// - Checks the receiver (`this`) is an instance of the class containing the
// called method
// - Declares a `JS::RootedObject self` which contains the receiver (`this`)
// - Checks that the number of arguments provided to the member is at least the
// number provided to the macro.
#define METHOD_HEADER(required_argc)                                           \
  METHOD_HEADER_WITH_NAME(required_argc, __func__)

#define CTOR_HEADER(name, required_argc)                                       \
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);                            \
  if (!ThrowIfNotConstructing(cx, args, name)) {                               \
    return false;                                                              \
  }                                                                            \
  if (!args.requireAtLeast(cx, name " constructor", required_argc)) {          \
    return false;                                                              \
  }

#define REQUEST_HANDLER_ONLY(name)                                             \
  if (isWizening()) {                                                          \
    JS_ReportErrorUTF8(cx,                                                     \
                       "%s can only be used during request handling, "         \
                       "not during initialization",                            \
                       name);                                                  \
    return false;                                                              \
  }

#define INIT_ONLY(name)                                                        \
  if (hasWizeningFinished()) {                                                 \
    JS_ReportErrorUTF8(cx,                                                     \
                       "%s can only be used during initialization, "           \
                       "not during request handling",                          \
                       name);                                                  \
    return false;                                                              \
  }

inline bool ThrowIfNotConstructing(JSContext *cx, const JS::CallArgs &args,
                                   const char *builtinName) {
  if (args.isConstructing()) {
    return true;
  }

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_BUILTIN_CTOR_NO_NEW, builtinName);
  return false;
}
namespace builtins {

template <typename Impl> class BuiltinImpl {
private:
  static constexpr const JSClassOps class_ops{};
  static constexpr const uint32_t class_flags = 0;

public:
  static constexpr JSClass class_{
      Impl::class_name,
      JSCLASS_HAS_RESERVED_SLOTS(static_cast<uint32_t>(Impl::Slots::Count)) |
          class_flags,
      &class_ops,
  };

  static inline JS::Result<std::tuple<JS::CallArgs, JS::Rooted<JSObject *> *>>
  MethodHeaderWithName(int required_argc, JSContext *cx, unsigned argc,
                       JS::Value *vp, const char *name) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (!check_receiver(cx, args.thisv(), name)) {
      return JS::Result<std::tuple<JS::CallArgs, JS::Rooted<JSObject *> *>>(
          JS::Error());
    }
    JS::Rooted<JSObject *> self(cx, &args.thisv().toObject());
    if (!args.requireAtLeast(cx, name, required_argc)) {
      return JS::Result<std::tuple<JS::CallArgs, JS::Rooted<JSObject *> *>>(
          JS::Error());
    }

    return JS::Result<std::tuple<JS::CallArgs, JS::Rooted<JSObject *> *>>(
        std::make_tuple(args, &self));
  }

  static JS::PersistentRooted<JSObject *> proto_obj;

  static bool is_instance(JSObject *obj) {
    return obj != nullptr && JS::GetClass(obj) == &class_;
  }

  static bool is_instance(JS::Value val) {
    return val.isObject() && is_instance(&val.toObject());
  }

  static bool check_receiver(JSContext *cx, JS::HandleValue receiver,
                             const char *method_name) {
    if (!Impl::is_instance(receiver)) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_INCOMPATIBLE_INSTANCE, method_name,
                                Impl::class_.name);
      return false;
    }

    return true;
  }

  static bool init_class_impl(JSContext *cx, JS::HandleObject global,
                              JS::HandleObject parent_proto = nullptr) {
    proto_obj.init(cx, JS_InitClass(cx, global, &class_, parent_proto,
                                    Impl::class_name, Impl::constructor,
                                    Impl::ctor_length, Impl::properties,
                                    Impl::methods, Impl::static_properties,
                                    Impl::static_methods));

    return proto_obj != nullptr;
  }
};

template <typename Impl>
JS::PersistentRooted<JSObject *> BuiltinImpl<Impl>::proto_obj{};

template <typename Impl> class BuiltinNoConstructor : public BuiltinImpl<Impl> {
public:
  static const int ctor_length = 1;

  static bool constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
    JS_ReportErrorUTF8(cx, "%s can't be instantiated directly",
                       Impl::class_name);
    return false;
  }

  static bool init_class(JSContext *cx, JS::HandleObject global) {
    return BuiltinImpl<Impl>::init_class_impl(cx, global) &&
           JS_DeleteProperty(cx, global, BuiltinImpl<Impl>::class_.name);
  }
};

} // namespace builtins

bool RejectPromiseWithPendingError(JSContext *cx, JS::HandleObject promise);
JSObject *PromiseRejectedWithPendingError(JSContext *cx);
inline bool ReturnPromiseRejectedWithPendingError(JSContext *cx,
                                                  const JS::CallArgs &args) {
  JSObject *promise = PromiseRejectedWithPendingError(cx);
  if (!promise) {
    return false;
  }

  args.rval().setObject(*promise);
  return true;
}
using InternalMethod = bool(JSContext *cx, JS::HandleObject receiver,
                            JS::HandleValue extra, JS::CallArgs args);

template <InternalMethod fun>
bool internal_method(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  JS::RootedObject self(
      cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  JS::RootedValue extra(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  return fun(cx, self, extra, args);
}

template <InternalMethod fun>
JSObject *
create_internal_method(JSContext *cx, JS::HandleObject receiver,
                       JS::HandleValue extra = JS::UndefinedHandleValue,
                       unsigned int nargs = 0, const char *name = "") {
  JSFunction *method =
      js::NewFunctionWithReserved(cx, internal_method<fun>, 1, 0, name);
  if (!method)
    return nullptr;
  JS::RootedObject method_obj(cx, JS_GetFunctionObject(method));
  js::SetFunctionNativeReserved(method_obj, 0, JS::ObjectValue(*receiver));
  js::SetFunctionNativeReserved(method_obj, 1, extra);
  return method_obj;
}

template <InternalMethod fun>
bool enqueue_internal_method(JSContext *cx, HandleObject receiver,
                             HandleValue extra = JS::UndefinedHandleValue,
                             unsigned int nargs = 0, const char *name = "") {
  RootedObject method(
      cx, create_internal_method<fun>(cx, receiver, extra, nargs, name));
  if (!method) {
    return false;
  }

  RootedObject promise(
      cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  if (!promise) {
    return false;
  }

  return JS::AddPromiseReactions(cx, promise, method, nullptr);
}

#endif
