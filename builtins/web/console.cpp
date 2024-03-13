#include "console.h"
#include "encode.h"
#include <chrono>
#include <map>

#include <js/Array.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include <js/experimental/TypedData.h>
#include <js/PropertyAndElement.h>
#pragma clang diagnostic pop

namespace {
using FpMilliseconds = std::chrono::duration<float, std::chrono::milliseconds::period>;
auto count_map = std::map<std::string, size_t>{};
auto timer_map = std::map<std::string, std::chrono::steady_clock::time_point>{};
} // namespace
JS::Result<mozilla::Ok> ToSource(JSContext *cx, std::string &sourceOut, JS::HandleValue val,
                                 JS::MutableHandleObjectVector visitedObjects);

/**
 * Turn a handle of a Promise into a string which represents the promise.
 * - If the promise is pending this will return "Promise { <pending> }"
 * - If the promise is rejected this will return "Promise { <rejected>
 * (rejected-value)}" where rejected-value would be the ToSource representation
 * of the rejected value.
 * - If the promise is resolved this will return "Promise { resolved-value}"
 *  where resolved-value would be the ToSource representation of the resolved
 * value.
 */
JS::Result<mozilla::Ok> PromiseToSource(JSContext *cx, std::string &sourceOut, JS::HandleObject obj,
                                        JS::MutableHandleObjectVector visitedObjects) {
  sourceOut += "Promise { ";
  JS::PromiseState state = JS::GetPromiseState(obj);
  switch (state) {
  case JS::PromiseState::Pending: {
    sourceOut += "<pending> }";
    break;
  }
  case JS::PromiseState::Fulfilled: {
    JS::RootedValue value(cx, JS::GetPromiseResult(obj));
    MOZ_TRY(ToSource(cx, sourceOut, value, visitedObjects));
    sourceOut += " }";
    break;
  }
  case JS::PromiseState::Rejected: {
    sourceOut += "<rejected> ";
    JS::RootedValue value(cx, JS::GetPromiseResult(obj));
    MOZ_TRY(ToSource(cx, sourceOut, value, visitedObjects));
    sourceOut += " }";
    break;
  }
  }
  return mozilla::Ok();
}

/**
 * Turn a handle of a Map into a string which represents the map.
 * Each key and value within the map will be converted into it's ToSource
 * representation.
 */
JS::Result<mozilla::Ok> MapToSource(JSContext *cx, std::string &sourceOut, JS::HandleObject obj,
                                    JS::MutableHandleObjectVector visitedObjects) {
  sourceOut += "Map(";
  uint32_t size = JS::MapSize(cx, obj);
  sourceOut += std::to_string(size);
  sourceOut += ") { ";
  JS::Rooted<JS::Value> iterable(cx);
  if (!JS::MapEntries(cx, obj, &iterable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }
  JS::ForOfIterator it(cx);
  if (!it.init(iterable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }

  JS::RootedObject entry(cx);
  JS::RootedValue entry_val(cx);
  JS::RootedValue name_val(cx);
  JS::RootedValue value_val(cx);
  bool firstValue = true;
  while (true) {
    bool done;
    if (!it.next(&entry_val, &done)) {
      return JS::Result<mozilla::Ok>(JS::Error());
    }

    if (done) {
      break;
    }
    if (firstValue) {
      firstValue = false;
    } else {
      sourceOut += ", ";
    }

    entry = &entry_val.toObject();
    JS_GetElement(cx, entry, 0, &name_val);
    JS_GetElement(cx, entry, 1, &value_val);
    MOZ_TRY(ToSource(cx, sourceOut, name_val, visitedObjects));
    sourceOut += " => ";
    MOZ_TRY(ToSource(cx, sourceOut, value_val, visitedObjects));
  }
  sourceOut += " }";
  return mozilla::Ok();
}

/**
 * Turn a handle of a Set into a string which represents the set.
 * Each value within the set will be converted into it's ToSource
 * representation.
 */
JS::Result<mozilla::Ok> SetToSource(JSContext *cx, std::string &sourceOut, JS::HandleObject obj,
                                    JS::MutableHandleObjectVector visitedObjects) {
  sourceOut += "Set(";
  uint32_t size = JS::SetSize(cx, obj);
  sourceOut += std::to_string(size);
  sourceOut += ") { ";
  JS::Rooted<JS::Value> iterable(cx);
  if (!JS::SetValues(cx, obj, &iterable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }
  JS::ForOfIterator it(cx);
  if (!it.init(iterable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }

  JS::RootedValue entry_val(cx);
  bool firstValue = true;
  while (true) {
    bool done;
    if (!it.next(&entry_val, &done)) {
      return JS::Result<mozilla::Ok>(JS::Error());
    }

    if (done) {
      break;
    }
    if (firstValue) {
      firstValue = false;
    } else {
      sourceOut += ", ";
    }
    MOZ_TRY(ToSource(cx, sourceOut, entry_val, visitedObjects));
  }
  sourceOut += " }";
  return mozilla::Ok();
}

JS::Result<mozilla::Ok> ArrayToSource(JSContext *cx, std::string &sourceOut, JS::HandleObject obj,
                                      JS::MutableHandleObjectVector visitedObjects) {
  sourceOut += "[";
  uint32_t len;
  if (!JS::GetArrayLength(cx, obj, &len)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }

  for (int i = 0; i < len; i++) {
    JS::RootedValue entry_val(cx);
    JS_GetElement(cx, obj, i, &entry_val);
    if (i > 0) {
      sourceOut += ", ";
    }
    MOZ_TRY(ToSource(cx, sourceOut, entry_val, visitedObjects));
  }
  sourceOut += "]";
  return mozilla::Ok();
}

/*
 * Logs all enumerable properties, except non-own function and symbol properties
 * Includes handling for getters and setters
 */
JS::Result<mozilla::Ok> ObjectToSource(JSContext *cx, std::string &sourceOut, JS::HandleObject obj,
                                       JS::MutableHandleObjectVector visitedObjects) {
  JS::RootedIdVector ids(cx);

  if (!js::GetPropertyKeys(cx, obj, 0, &ids)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }

  JS::RootedValue value(cx);
  size_t length = ids.length();

  sourceOut += "{";
  bool firstValue = true;
  for (size_t i = 0; i < length; ++i) {
    const auto &id = ids[i];

    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
    JS_GetOwnPropertyDescriptorById(cx, obj, id, &desc);

    bool getter_setter = !desc.isNothing() && (desc->hasGetter() || desc->hasSetter());

    // retrive the value if not a getter or setter
    if (!getter_setter) {
      if (!JS_GetPropertyById(cx, obj, id, &value)) {
        return JS::Result<mozilla::Ok>(JS::Error());
      }
    }

    // Skip logging non-own function or getter and setter keys
    if (getter_setter || (value.isObject() && JS_ObjectIsFunction(&value.toObject()))) {
      bool own_prop;
      if (!JS_HasOwnPropertyById(cx, obj, id, &own_prop)) {
        return JS::Result<mozilla::Ok>(JS::Error());
      }
      if (!own_prop) {
        continue;
      }
    }

    if (firstValue) {
      firstValue = false;
      sourceOut += " ";
    } else {
      sourceOut += ", ";
    }

    // Key
    if (id.isSymbol()) {
      JS::RootedValue v(cx, SymbolValue(id.toSymbol()));
      MOZ_TRY(ToSource(cx, sourceOut, v, visitedObjects));
    } else {
      JS::RootedValue v(cx, js::IdToValue(id));
      auto msg = core::encode(cx, v);
      if (!msg) {
        return JS::Result<mozilla::Ok>(JS::Error());
      }
      sourceOut += std::string(msg.begin(), msg.len);
    }

    sourceOut += ": ";

    // Getters and Setters
    if (getter_setter) {
      sourceOut += "[Getter]";
    } else {
      MOZ_TRY(ToSource(cx, sourceOut, value, visitedObjects));
    }
  }

  if (!firstValue) {
    sourceOut += " ";
  }
  sourceOut += "}";

  return mozilla::Ok();
}

mozilla::Maybe<std::string> get_class_name(JSContext *cx, JS::HandleObject obj) {
  mozilla::Maybe<std::string> result = {};
  JS::RootedValue constructorVal(cx);
  if (JS_GetProperty(cx, obj, "constructor", &constructorVal) && constructorVal.isObject()) {
    JS::RootedValue name(cx);
    JS::RootedObject constructorObj(cx, &constructorVal.toObject());
    if (JS_GetProperty(cx, constructorObj, "name", &name) && name.isString()) {
      auto msg = core::encode(cx, name);
      if (!msg) {
        return result;
      }
      std::string name_str(msg.begin(), msg.len);
      result.emplace(name_str);
    }
  }
  return result;
}

/**
 * Turn a handle of any value into a string which represents it.
 */
JS::Result<mozilla::Ok> ToSource(JSContext *cx, std::string &sourceOut, JS::HandleValue val,
                                 JS::MutableHandleObjectVector visitedObjects) {

  auto type = val.type();
  switch (type) {
  case JS::ValueType::Undefined: {
    sourceOut += "undefined";
    return mozilla::Ok();
  }
  case JS::ValueType::Null: {
    sourceOut += "null";
    return mozilla::Ok();
  }
  case JS::ValueType::Object: {
    JS::RootedObject obj(cx, &val.toObject());

    if (JS_ObjectIsFunction(obj)) {
      sourceOut += "[Function";
      std::string source;
      auto id = JS_GetFunctionId(JS_ValueToFunction(cx, val));
      if (id) {
        sourceOut += " ";
        JS::RootedString name(cx, id);
        auto msg = core::encode(cx, name);
        if (!msg) {
          return JS::Result<mozilla::Ok>(JS::Error());
        }
        sourceOut += std::string(msg.begin(), msg.len);
      }
      sourceOut += "]";
      return mozilla::Ok();
    }

    if (JS_IsTypedArrayObject(obj)) {
      // Show the typed array type
      mozilla::Maybe<std::string> name_str = get_class_name(cx, obj);
      if (!name_str.isNothing()) {
        sourceOut += *name_str;
        sourceOut += " ";
      }
      MOZ_TRY(ArrayToSource(cx, sourceOut, obj, visitedObjects));
      return mozilla::Ok();
    }

    for (const auto &curObject : visitedObjects) {
      if (obj.get() == curObject) {
        sourceOut += "<Circular>";
        return mozilla::Ok();
      }
    }

    if (!visitedObjects.emplaceBack(obj)) {
      return JS::Result<mozilla::Ok>(JS::Error());
    }

    js::ESClass cls;
    if (!JS::GetBuiltinClass(cx, obj, &cls)) {
      return JS::Result<mozilla::Ok>(JS::Error());
    }

    switch (cls) {
    case js::ESClass::Date:
    case js::ESClass::Error:
    case js::ESClass::RegExp: {
      JS::RootedString source(cx, JS_ValueToSource(cx, val));
      auto msg = core::encode(cx, source);
      if (!msg) {
        return JS::Result<mozilla::Ok>(JS::Error());
      }
      sourceOut += std::string(msg.begin(), msg.len);
      return mozilla::Ok();
    }
    case js::ESClass::Array: {
      MOZ_TRY(ArrayToSource(cx, sourceOut, obj, visitedObjects));
      return mozilla::Ok();
    }
    case js::ESClass::Set: {
      MOZ_TRY(SetToSource(cx, sourceOut, obj, visitedObjects));
      return mozilla::Ok();
    }
    case js::ESClass::Map: {
      MOZ_TRY(MapToSource(cx, sourceOut, obj, visitedObjects));
      return mozilla::Ok();
    }
    case js::ESClass::Promise: {
      MOZ_TRY(PromiseToSource(cx, sourceOut, obj, visitedObjects));
      return mozilla::Ok();
    }
    default: {
      std::string sourceString;
      if (JS::IsWeakMapObject(obj)) {
        sourceOut += "WeakMap { <items unknown> }";
        return mozilla::Ok();
      }
      auto cls = JS::GetClass(obj);
      std::string className(cls->name);
      if (className == "WeakSet") {
        sourceOut += "WeakSet { <items unknown> }";
        return mozilla::Ok();
      }

      // Lookup the class name if a custom class
      mozilla::Maybe<std::string> name_str = get_class_name(cx, obj);
      if (!name_str.isNothing() && *name_str != "Object") {
        sourceOut += *name_str;
        sourceOut += " ";
      }

      MOZ_TRY(ObjectToSource(cx, sourceOut, obj, visitedObjects));
      return mozilla::Ok();
    }
    }
  }
  case JS::ValueType::String: {
    auto msg = core::encode(cx, val);
    if (!msg) {
      return JS::Result<mozilla::Ok>(JS::Error());
    }
    sourceOut += '"';
    sourceOut += std::string(msg.begin(), msg.len);
    sourceOut += '"';
    return mozilla::Ok();
  }
  default: {
    JS::RootedString source(cx, JS_ValueToSource(cx, val));
    auto msg = core::encode(cx, source);
    if (!msg) {
      return JS::Result<mozilla::Ok>(JS::Error());
    }
    sourceOut += std::string(msg.begin(), msg.len);
    return mozilla::Ok();
  }
  }
}

namespace builtins::web::console {

__attribute__((weak))
void builtin_impl_console_log(Console::LogType log_ty, const char *msg) {
  const char *prefix = "";
  switch (log_ty) {
  case Console::LogType::Log:
    prefix = "Log";
    break;
  case Console::LogType::Debug:
    prefix = "Debug";
    break;
  case Console::LogType::Info:
    prefix = "Info";
    break;
  case Console::LogType::Warn:
    prefix = "Warn";
    break;
  case Console::LogType::Error:
    prefix = "Error";
    break;
  }
  fprintf(stdout, "%s: %s\n", prefix, msg);
  fflush(stdout);
}

template <Console::LogType log_ty>
static bool console_out(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  std::string fullLogLine = "";
  auto length = args.length();
  JS::RootedObjectVector visitedObjects(cx);
  for (int i = 0; i < length; i++) {
    JS::HandleValue arg = args.get(i);
    std::string source = "";
    auto result = ToSource(cx, source, arg, &visitedObjects);
    if (result.isErr()) {
      return false;
    }
    // strip quotes for direct string logs
    if (source[0] == '"' && source[source.length() - 1] == '"') {
      source = source.substr(1, source.length() - 2);
    }
    if (fullLogLine.length()) {
      fullLogLine += " ";
      fullLogLine += source;
    } else {
      fullLogLine += source;
    }
  }

  builtin_impl_console_log(log_ty, fullLogLine.c_str());

  args.rval().setUndefined();
  return true;
}

// https://console.spec.whatwg.org/#assert
// assert(condition, ...data)
static bool assert(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  auto condition = args.get(0).toBoolean();
  // 1. If condition is true, return.
  if (!condition) {
    return true;
  }

  // 2. Let message be a string without any formatting specifiers indicating
  // generically an assertion failure (such as "Assertion failed").
  std::string message = "Assertion failed";
  // 3. If data is empty, append message to data.
  // 4. Otherwise:
  // 4.1. Let first be data[0].
  // 4.2. If Type(first) is not String, then prepend message to data.
  // 4.3. Otherwise:
  // 4.3.1. Let concat be the concatenation of message, U+003A (:), U+0020
  // SPACE, and first. 4.3.2. Set data[0] to concat.
  auto length = args.length();
  if (length > 1) {
    message += ": ";
    JS::RootedObjectVector visitedObjects(cx);
    for (int i = 0; i < length; i++) {
      JS::HandleValue arg = args.get(i);
      std::string source = "";
      auto result = ToSource(cx, source, arg, &visitedObjects);
      if (result.isErr()) {
        return false;
      }
      // strip quotes for direct string logs
      if (source[0] == '"' && source[source.length() - 1] == '"') {
        source = source.substr(1, source.length() - 2);
      }
      if (!message.empty()) {
        message += " ";
        message += source;
      } else {
        message += source;
      }
    }
  }

  // 5. Perform Logger("assert", data).
  builtin_impl_console_log(Console::LogType::Error, message.c_str());
  return true;
}

static bool count(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  // 1. Let map be the associated count map.
  std::string concat = "";
  std::string label = "";
  if (args.hasDefined(0)) {
    auto label_val = args.get(0);
    auto label_string = core::encode(cx, label_val);
    if (!label_string) {
      return false;
    }
    label = std::string{label_string.begin(), label_string.len};
  } else {
    label = "default";
  }
  size_t count = 0;
  // 2. If map[label] exists, set map[label] to map[label] + 1.
  auto it = count_map.find(label);
  if (it != count_map.end()) {
    count = it->second;
  }
  count += 1;
  count_map[label] = count;
  // 4. Let concat be the concatenation of label, U+003A (:), U+0020 SPACE, and
  // ToString(map[label]).
  concat += label;
  concat += ": ";
  concat += std::to_string(count);
  // 5. Perform Logger("count", « concat »).
  builtin_impl_console_log(Console::LogType::Log, concat.c_str());
  args.rval().setUndefined();
  return true;
}

static bool countReset(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  // 1. Let map be the associated count map.
  std::string label;
  if (args.hasDefined(0)) {
    auto label_val = args.get(0);
    auto label_string = core::encode(cx, label_val);
    if (!label_string) {
      return false;
    }
    label = std::string{label_string.begin(), label_string.len};
  } else {
    label = "default";
  }
  // 2. If map[label] exists, set map[label] to 0.
  auto it = count_map.find(label);
  if (it != count_map.end()) {
    it->second = 0;
  } else {
    // 3. Otherwise:
    // 3.1. Let message be a string without any formatting specifiers indicating
    // generically that the given label does not have an associated count. 3.2.
    // Perform Logger("countReset", « message »);
    std::string message = "Count for '";
    message += label;
    message += "' does not exist";
    builtin_impl_console_log(Console::LogType::Warn, message.c_str());
  }

  args.rval().setUndefined();
  return true;
}

static bool no_op(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}

static bool time(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  std::string label;
  if (args.hasDefined(0)) {
    auto label_val = args.get(0);
    auto label_string = core::encode(cx, label_val);
    if (!label_string) {
      return false;
    }
    label = std::string{label_string.begin(), label_string.len};
  } else {
    label = "default";
  }
  // 1. If the associated timer table contains an entry with key label, return,
  // optionally reporting a warning to the console indicating that a timer with
  // label label has already been started.
  if (!timer_map.contains(label)) {
    // 2. Otherwise, set the value of the entry with key label in the associated
    // timer table to the current time.
    timer_map[label] = std::chrono::high_resolution_clock::now();
  }

  args.rval().setUndefined();
  return true;
}

static bool timeLog(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  // 1. Let timerTable be the associated timer table.
  std::string label;
  if (args.hasDefined(0)) {
    auto label_val = args.get(0);
    auto label_string = core::encode(cx, label_val);
    if (!label_string) {
      return false;
    }
    label = std::string{label_string.begin(), label_string.len};
  } else {
    label = "default";
  }
  if (!timer_map.contains(label)) {
    std::string message = "No such label '";
    message += label;
    message += "' for console.timeLog()";
    builtin_impl_console_log(Console::LogType::Warn, message.c_str());
    args.rval().setUndefined();
    return true;
  }

  // 2. Let startTime be timerTable[label].
  auto startTime = timer_map[label];

  // 3. Let duration be a string representing the difference between the current
  // time and startTime, in an implementation-defined format.
  auto finish = std::chrono::high_resolution_clock::now();
  auto duration = FpMilliseconds(finish - startTime).count();

  // 4. Let concat be the concatenation of label, U+003A (:), U+0020 SPACE, and
  // duration.
  std::string concat = label;
  concat += ": ";
  concat += std::to_string(duration);
  concat += "ms";

  std::string data = "";
  if (args.length() > 1) {
    auto length = args.length();
    JS::RootedObjectVector visitedObjects(cx);
    for (int i = 1; i < length; i++) {
      JS::HandleValue arg = args.get(i);
      std::string source = "";
      auto result = ToSource(cx, source, arg, &visitedObjects);
      if (result.isErr()) {
        return false;
      }
      // strip quotes for direct string logs
      if (source[0] == '"' && source[source.length() - 1] == '"') {
        source = source.substr(1, source.length() - 2);
      }
      if (data.length()) {
        data += " ";
        data += source;
      } else {
        data += source;
      }
    }
  }

  // 5. Prepend concat to data.
  concat += " ";
  concat += data;

  // 6. Perform Printer("timeLog", « concat »).
  builtin_impl_console_log(Console::LogType::Log, concat.c_str());

  args.rval().setUndefined();
  return true;
}

static bool timeEnd(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  // 1. Let timerTable be the associated timer table.
  std::string label;
  if (args.hasDefined(0)) {
    auto label_val = args.get(0);
    auto label_string = core::encode(cx, label_val);
    if (!label_string) {
      return false;
    }
    label = std::string{label_string.begin(), label_string.len};
  } else {
    label = "default";
  }
  if (!timer_map.contains(label)) {
    std::string message = "No such label '";
    message += label;
    message += "' for console.timeEnd()";
    builtin_impl_console_log(Console::LogType::Warn, message.c_str());
    args.rval().setUndefined();
    return true;
  }

  // 2. Let startTime be timerTable[label].
  // 3. Remove timerTable[label].
  auto startTime = timer_map.extract(label).mapped();

  // 4. Let duration be a string representing the difference between the current
  // time and startTime, in an implementation-defined format.
  auto finish = std::chrono::high_resolution_clock::now();
  auto duration = FpMilliseconds(finish - startTime).count();
  // 5. Let concat be the concatenation of label, U+003A (:), U+0020 SPACE, and
  // duration.
  std::string concat = label;
  concat += ": ";
  concat += std::to_string(duration);
  concat += "ms";

  // 6. Perform Printer("timeEnd", « concat »).
  builtin_impl_console_log(Console::LogType::Info, concat.c_str());

  args.rval().setUndefined();
  return true;
}

static bool dir(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  std::string fullLogLine = "";
  JS::RootedObjectVector visitedObjects(cx);
  JS::HandleValue arg = args.get(0);
  std::string source = "";
  auto result = ToSource(cx, source, arg, &visitedObjects);
  if (result.isErr()) {
    return false;
  }
  // strip quotes for direct string logs
  if (source[0] == '"' && source[source.length() - 1] == '"') {
    source = source.substr(1, source.length() - 2);
  }
  if (fullLogLine.length()) {
    fullLogLine += " ";
    fullLogLine += source;
  } else {
    fullLogLine += source;
  }

  builtin_impl_console_log(Console::LogType::Log, fullLogLine.c_str());

  args.rval().setUndefined();
  return true;
}

static bool trace(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  // 1. Let trace be some implementation-specific, potentially-interactive
  // representation of the callstack from where this function was called.

  JS::RootedObject stack(cx);
  if (!CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::MaxFrames(1u << 7)))) {
    return false;
  }
  auto principals = JS::GetRealmPrincipals(JS::GetCurrentRealmOrNull(cx));
  JS::RootedString str(cx);
  if (!BuildStackString(cx, principals, stack, &str)) {
    return false;
  }
  auto stack_string = core::encode(cx, str);
  if (!stack_string) {
    return false;
  }

  // 2. Optionally, let formattedData be the result of Formatter(data), and
  // incorporate formattedData as a label for trace.
  std::string fullLogLine = "";
  JS::RootedObjectVector visitedObjects(cx);
  for (int i = 0; i < args.length(); i++) {
    JS::HandleValue arg = args.get(i);
    std::string source = "";
    auto result = ToSource(cx, source, arg, &visitedObjects);
    if (result.isErr()) {
      return false;
    }
    // strip quotes for direct string logs
    if (source[0] == '"' && source[source.length() - 1] == '"') {
      source = source.substr(1, source.length() - 2);
    }
    if (fullLogLine.length()) {
      fullLogLine += " ";
      fullLogLine += source;
    } else {
      fullLogLine += source;
    }
  }
  fullLogLine += "\n";
  fullLogLine += stack_string.begin();

  // 3. Perform Printer("trace", « trace »).
  builtin_impl_console_log(Console::LogType::Log, fullLogLine.c_str());
  args.rval().setUndefined();
  return true;
}

const JSFunctionSpec Console::methods[] = {
    JS_FN("assert", assert, 0, JSPROP_ENUMERATE),
    JS_FN("clear", no_op, 0, JSPROP_ENUMERATE),
    JS_FN("count", count, 0, JSPROP_ENUMERATE),
    JS_FN("countReset", countReset, 0, JSPROP_ENUMERATE),
    JS_FN("debug", (console_out<Console::LogType::Debug>), 0, JSPROP_ENUMERATE),
    JS_FN("dir", dir, 0, JSPROP_ENUMERATE),
    JS_FN("dirxml", (console_out<Console::LogType::Log>), 0, JSPROP_ENUMERATE),
    JS_FN("error", (console_out<Console::LogType::Error>), 0, JSPROP_ENUMERATE),
    JS_FN("group", (console_out<Console::LogType::Log>), 0, JSPROP_ENUMERATE),
    JS_FN("groupCollapsed", (console_out<Console::LogType::Log>), 0, JSPROP_ENUMERATE),
    JS_FN("groupEnd", no_op, 0, JSPROP_ENUMERATE),
    JS_FN("info", (console_out<Console::LogType::Info>), 0, JSPROP_ENUMERATE),
    JS_FN("log", (console_out<Console::LogType::Log>), 0, JSPROP_ENUMERATE),
    JS_FN("table", (console_out<Console::LogType::Log>), 0, JSPROP_ENUMERATE),
    JS_FN("time", time, 0, JSPROP_ENUMERATE),
    JS_FN("timeEnd", timeEnd, 0, JSPROP_ENUMERATE),
    JS_FN("timeLog", timeLog, 0, JSPROP_ENUMERATE),
    JS_FN("trace", trace, 0, JSPROP_ENUMERATE),
    JS_FN("warn", (console_out<Console::LogType::Warn>), 0, JSPROP_ENUMERATE),
    JS_FS_END};

const JSPropertySpec Console::properties[] = {
    JS_STRING_SYM_PS(toStringTag, "console", JSPROP_READONLY), JS_PS_END};

bool install(api::Engine *engine) {
  JS::RootedObject proto(engine->cx(), JS_NewPlainObject(engine->cx()));
  JS::RootedObject console(engine->cx(),
                           JS_NewObjectWithGivenProto(engine->cx(), &Console::class_, proto));
  if (!console) {
    return false;
  }
  if (!JS_DefineProperty(engine->cx(), engine->global(), "console", console, 0)) {
    return false;
  }
  if (!JS_DefineProperties(engine->cx(), console, Console::properties)) {
    return false;
  }
  return JS_DefineFunctions(engine->cx(), console, Console::methods);
}
} // namespace builtins::web::console
