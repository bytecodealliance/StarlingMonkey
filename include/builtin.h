#ifndef JS_COMPUTE_RUNTIME_BUILTIN_H
#define JS_COMPUTE_RUNTIME_BUILTIN_H

#include "extension-api.h"

#include <optional>
#include <span>
#include <tuple>

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "jsapi.h"
#include "jsfriendapi.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "js/experimental/TypedData.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/Promise.h"
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
using JS::RootedValueArray;

using JS::HandleObject;
using JS::HandleString;
using JS::HandleValue;
using JS::HandleValueArray;
using JS::Heap;
using JS::MutableHandleValue;
using JS::NullHandleValue;
using JS::UndefinedHandleValue;

using JS::PersistentRooted;

std::optional<std::span<uint8_t>> value_to_buffer(JSContext *cx, HandleValue val,
                                                  const char *val_desc);

#define DEF_ERR(name, exception, format, count) \
static constexpr JSErrorFormatString name = { #name, format, count, exception };

namespace api {
#include "errors.h"
}

bool hasWizeningFinished();
bool isWizening();
void markWizeningAsFinished();

#define DBG(...)                                                                                   \
  printf("%s#%d: ", __func__, __LINE__);                                                           \
  printf(__VA_ARGS__);                                                                             \
  fflush(stdout);

// Define this to make most methods print their name to stderr when invoked.
// #define TRACE_METHOD_CALLS

#ifdef TRACE_METHOD_CALLS
#define TRACE_METHOD(name) DBG("%s\n", name)
#else
#define TRACE_METHOD(name)
#endif

#define METHOD_HEADER_WITH_NAME(required_argc, name)                                               \
  TRACE_METHOD(name)                                                                               \
  CallArgs args = CallArgsFromVp(argc, vp);                                                        \
  if (!check_receiver(cx, args.thisv(), name))                                                     \
    return false;                                                                                  \
  RootedObject self(cx, &args.thisv().toObject());                                                 \
  if (!args.requireAtLeast(cx, name, required_argc))                                               \
    return false;

// This macro:
// - Declares a `CallArgs args` which contains the arguments provided to the
// method
// - Checks the receiver (`this`) is an instance of the class containing the
// called method
// - Declares a `RootedObject self` which contains the receiver (`this`)
// - Checks that the number of arguments provided to the member is at least the
// number provided to the macro.
#define METHOD_HEADER(required_argc) METHOD_HEADER_WITH_NAME(required_argc, __func__)

#define CTOR_HEADER(name, required_argc)                                                           \
  CallArgs args = CallArgsFromVp(argc, vp);                                                        \
  if (!ThrowIfNotConstructing(cx, args, name)) {                                                   \
    return false;                                                                                  \
  }                                                                                                \
  if (!args.requireAtLeast(cx, name " constructor", required_argc)) {                              \
    return false;                                                                                  \
  }

#define ITER_TYPE_ENTRIES 0
#define ITER_TYPE_KEYS 1
#define ITER_TYPE_VALUES 2

#define BUILTIN_ITERATOR_METHOD(class_name, method, type)                                          \
  bool class_name::method(JSContext *cx, unsigned argc, JS::Value *vp) {                           \
    METHOD_HEADER(0)                                                                               \
    JS::RootedObject iter(cx, class_name##Iterator::create(cx, self, type));                       \
    if (!iter)                                                                                     \
      return false;                                                                                \
    args.rval().setObject(*iter);                                                                  \
    return true;                                                                                   \
  }

// defines entries(), keys(), values(), and forEach(), assuming class_name##Iterator
#define BUILTIN_ITERATOR_METHODS(class_name)                                                       \
  BUILTIN_ITERATOR_METHOD(class_name, entries, ITER_TYPE_ENTRIES)                                  \
  BUILTIN_ITERATOR_METHOD(class_name, keys, ITER_TYPE_KEYS)                                        \
  BUILTIN_ITERATOR_METHOD(class_name, values, ITER_TYPE_VALUES)                                    \
                                                                                                   \
bool class_name::forEach(JSContext *cx, unsigned argc, JS::Value *vp) {                            \
  METHOD_HEADER(1)                                                                                 \
  if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject())) {                               \
    return api::throw_error(cx, api::Errors::ForEachCallback, #class_name);                        \
  }                                                                                                \
  JS::RootedValueArray<3> newArgs(cx);                                                             \
  newArgs[2].setObject(*self);                                                                     \
  JS::RootedValue rval(cx);                                                                        \
  JS::RootedObject iter(cx, class_name##Iterator::create(cx, self, ITER_TYPE_ENTRIES));            \
  if (!iter)                                                                                       \
    return false;                                                                                  \
  JS::RootedValue iterable(cx, ObjectValue(*iter));                                                \
  JS::ForOfIterator it(cx);                                                                        \
  if (!it.init(iterable))                                                                          \
    return false;                                                                                  \
                                                                                                   \
  JS::RootedValue entry_val(cx);                                                                   \
  JS::RootedObject entry(cx);                                                                      \
  while (true) {                                                                                   \
    bool done;                                                                                     \
    if (!it.next(&entry_val, &done))                                                               \
      return false;                                                                                \
    if (done)                                                                                      \
      break;                                                                                       \
                                                                                                   \
    entry = &entry_val.toObject();                                                                 \
    JS_GetElement(cx, entry, 1, newArgs[0]);                                                       \
    JS_GetElement(cx, entry, 0, newArgs[1]);                                                       \
    if (!JS::Call(cx, args.thisv(), args[0], newArgs, &rval))                                      \
      return false;                                                                                \
  }                                                                                                \
  return true;                                                                                     \
}

#define REQUEST_HANDLER_ONLY(name)                                                                 \
  if (isWizening()) {                                                                              \
    return api::throw_error(cx, api::Errors::RequestHandlerOnly, name);                            \
  }

#define INIT_ONLY(name)                                                                            \
  if (hasWizeningFinished()) {                                                                     \
    return api::throw_error(cx, api::Errors::InitializationOnly, name);                            \
  }

inline bool ThrowIfNotConstructing(JSContext *cx, const CallArgs &args, const char *builtinName) {
  if (args.isConstructing()) {
    return true;
  }

  return api::throw_error(cx, api::Errors::CtorCalledWithoutNew, builtinName);
}
namespace builtins {

template <typename Impl> class BuiltinImpl {
  static constexpr JSClassOps class_ops{};
  static constexpr uint32_t class_flags = 0;

public:
  static constexpr JSClass class_{
      Impl::class_name,
      JSCLASS_HAS_RESERVED_SLOTS(static_cast<uint32_t>(Impl::Slots::Count)) | class_flags,
      &class_ops,
  };

  static JS::Result<std::tuple<CallArgs, RootedObject *>>
  MethodHeaderWithName(const int required_argc, JSContext *cx, const unsigned argc, Value *vp,
                       const char *name) {
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!check_receiver(cx, args.thisv(), name)) {
      return JS::Result<std::tuple<CallArgs, RootedObject *>>(JS::Error());
    }
    RootedObject self(cx, &args.thisv().toObject());
    if (!args.requireAtLeast(cx, name, required_argc)) {
      return JS::Result<std::tuple<CallArgs, RootedObject *>>(JS::Error());
    }

    return {std::make_tuple(args, &self)};
  }

  static PersistentRooted<JSObject *> proto_obj;

  static bool is_instance(const JSObject *obj) {
    return obj != nullptr && JS::GetClass(obj) == &class_;
  }

  static bool is_instance(const Value val) {
    return val.isObject() && is_instance(&val.toObject());
  }

  static bool check_receiver(JSContext *cx, HandleValue receiver, const char *method_name) {
    if (!Impl::is_instance(receiver)) {
      return api::throw_error(cx, api::Errors::WrongReceiver, method_name, Impl::class_name);
    }

    return true;
  }

  static bool init_class_impl(JSContext *cx, const HandleObject global,
                              const HandleObject parent_proto = nullptr) {
    proto_obj.init(cx, JS_InitClass(cx, global, &class_, parent_proto, Impl::class_name,
                                    Impl::constructor, Impl::ctor_length, Impl::properties,
                                    Impl::methods, Impl::static_properties, Impl::static_methods));

    return proto_obj != nullptr;
  }
};

template <typename Impl> PersistentRooted<JSObject *> BuiltinImpl<Impl>::proto_obj{};

template <typename Impl> class BuiltinNoConstructor : public BuiltinImpl<Impl> {
public:
  static constexpr int ctor_length = 1;

  static bool constructor(JSContext *cx, [[maybe_unused]] unsigned argc,
                          [[maybe_unused]] Value *vp) {
    return api::throw_error(cx, api::Errors::NoCtorBuiltin, Impl::class_name);
  }

  static bool init_class(JSContext *cx, HandleObject global) {
    return BuiltinImpl<Impl>::init_class_impl(cx, global) &&
           JS_DeleteProperty(cx, global, BuiltinImpl<Impl>::class_.name);
  }
};

} // namespace builtins

bool RejectPromiseWithPendingError(JSContext *cx, HandleObject promise);
JSObject *PromiseRejectedWithPendingError(JSContext *cx);
inline bool ReturnPromiseRejectedWithPendingError(JSContext *cx, const CallArgs &args) {
  JSObject *promise = PromiseRejectedWithPendingError(cx);
  if (!promise) {
    return false;
  }

  args.rval().setObject(*promise);
  return true;
}
using InternalMethod = bool(JSContext *cx, HandleObject receiver, HandleValue extra, CallArgs args);

template <InternalMethod fun> bool internal_method(JSContext *cx, const unsigned argc, Value *vp) {
  const CallArgs args = CallArgsFromVp(argc, vp);
  const RootedObject self(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  const RootedValue extra(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  return fun(cx, self, extra, args);
}

template <InternalMethod fun>
JSObject *create_internal_method(JSContext *cx, const HandleObject receiver,
                                 const HandleValue extra = UndefinedHandleValue,
                                 unsigned int nargs = 0, const char *name = "") {
  JSFunction *method = js::NewFunctionWithReserved(cx, internal_method<fun>, nargs, 0, name);
  if (!method)
    return nullptr;
  RootedObject method_obj(cx, JS_GetFunctionObject(method));
  js::SetFunctionNativeReserved(method_obj, 0, ObjectValue(*receiver));
  js::SetFunctionNativeReserved(method_obj, 1, extra);
  return method_obj;
}

template <InternalMethod fun>
bool enqueue_internal_method(JSContext *cx, const HandleObject receiver,
                             const HandleValue extra = UndefinedHandleValue,
                             const unsigned int nargs = 0, const char *name = "") {
  const RootedObject method(cx, create_internal_method<fun>(cx, receiver, extra, nargs, name));
  if (!method) {
    return false;
  }

  const RootedObject promise(cx, CallOriginalPromiseResolve(cx, UndefinedHandleValue));
  if (!promise) {
    return false;
  }

  return AddPromiseReactions(cx, promise, method, nullptr);
}

#endif
