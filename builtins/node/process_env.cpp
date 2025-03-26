#include "process_env.h"
#include "extension-api.h"
#include "host_api.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include <cstdio>

namespace builtins::node::process_env {

// Custom proxy handler for environment variables
class EnvProxyHandler : public js::BaseProxyHandler {
public:
  EnvProxyHandler() : BaseProxyHandler(nullptr) {}

  bool get(JSContext* cx, JS::HandleObject proxy, JS::HandleValue receiver,
           JS::HandleId id, JS::MutableHandleValue vp) const override {
    // Convert id to string
    JS::RootedValue idVal(cx);
    if (!JS_IdToValue(cx, id, &idVal) || !idVal.isString()) {
      return false;
    }

    // Convert to UTF8
    JS::RootedString idStr(cx, idVal.toString());
    JS::UniqueChars propNameUtf8 = JS_EncodeStringToUTF8(cx, idStr);
    if (!propNameUtf8) {
      return false;
    }

    // Get all environment variables
    auto env_vars = host_api::environment_get_environment();

    // Look for the requested environment variable
    for (const auto& [key, value] : env_vars) {
      if (key == propNameUtf8.get()) {
        // Found it! Convert the value to a JS string
        JS::RootedString value_str(cx, JS_NewStringCopyZ(cx, value.c_str()));
        if (!value_str) {
          return false;
        }
        vp.setString(value_str);
        return true;
      }
    }

    // Not found, return undefined
    vp.setUndefined();
    return true;
  }

  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy,
                               JS::HandleId id,
                               JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    // Convert id to string
    JS::RootedValue idVal(cx);
    if (!JS_IdToValue(cx, id, &idVal) || !idVal.isString()) {
      return false;
    }

    // Convert to UTF8
    JS::RootedString idStr(cx, idVal.toString());
    JS::UniqueChars propNameUtf8 = JS_EncodeStringToUTF8(cx, idStr);
    if (!propNameUtf8) {
      return false;
    }

    // Get all environment variables
    auto env_vars = host_api::environment_get_environment();

    // Look for the requested environment variable
    for (const auto& [key, value] : env_vars) {
      if (key == propNameUtf8.get()) {
        // Found it! Create a property descriptor
        JS::RootedString value_str(cx, JS_NewStringCopyZ(cx, value.c_str()));
        if (!value_str) {
          return false;
        }
        desc.set(mozilla::Some(JS::PropertyDescriptor::Data(
            JS::StringValue(value_str),
            JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT
        )));
        return true;
      }
    }

    // Not found
    desc.set(mozilla::Nothing());
    return true;
  }

  bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                     JS::Handle<JS::PropertyDescriptor> desc,
                     JS::ObjectOpResult& result) const override {
    // Environment variables are read-only
    return result.failReadOnly();
  }

  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy,
                      JS::MutableHandleIdVector props) const override {
    // Get all environment variables
    auto env_vars = host_api::environment_get_environment();

    // Add each environment variable name to the property list
    for (const auto& [key, value] : env_vars) {
      JS::RootedString key_str(cx, JS_NewStringCopyZ(cx, key.c_str()));
      if (!key_str) {
        return false;
      }
      JS::RootedId id(cx);
      if (!JS_StringToId(cx, key_str, &id)) {
        return false;
      }
      if (!props.append(id)) {
        return false;
      }
    }

    return true;
  }

  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
               JS::ObjectOpResult& result) const override {
    // Environment variables are read-only
    return result.failReadOnly();
  }

  bool preventExtensions(JSContext* cx, JS::HandleObject proxy,
                        JS::ObjectOpResult& result) const override {
    // Environment variables are already non-extensible
    return result.succeed();
  }

  bool isExtensible(JSContext* cx, JS::HandleObject proxy,
                    bool* extensible) const override {
    *extensible = false;
    return true;
  }

  bool getPrototypeIfOrdinary(JSContext* cx, JS::HandleObject proxy,
                             bool* isOrdinary,
                             JS::MutableHandleObject protop) const override {
    *isOrdinary = true;
    protop.set(nullptr);
    return true;
  }
};

static EnvProxyHandler envProxyHandler;

bool install(api::Engine* engine) {
  auto cx = engine->cx();
  auto global = engine->global();

  // Create process object if it doesn't exist
  JS::RootedObject process(cx);
  JS::RootedValue process_val(cx);
  if (!JS_GetProperty(cx, global, "process", &process_val) || process_val.isUndefined()) {
    process = JS_NewPlainObject(cx);
    if (!process) {
      return false;
    }
    if (!JS_DefineProperty(cx, global, "process", process, JSPROP_ENUMERATE)) {
      return false;
    }
  } else {
    process = &process_val.toObject();
  }

  // Create the target object (empty object)
  JS::RootedObject target(cx, JS_NewPlainObject(cx));
  if (!target) {
    return false;
  }

  // Create the proxy with the target object
  JS::RootedValue proxyVal(cx);
  JS::RootedValue targetVal(cx, JS::ObjectValue(*target));
  JS::RootedObject proxy(cx, NewProxyObject(cx, &envProxyHandler, targetVal, nullptr, js::ProxyOptions()));
  if (!proxy) {
    return false;
  }
  proxyVal.setObject(*proxy);

  // Add env to process
  if (!JS_DefineProperty(cx, process, "env", proxyVal, JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

} // namespace builtins::node::process_env
