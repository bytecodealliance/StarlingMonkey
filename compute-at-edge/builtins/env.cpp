
#include "env.h"
#include "core/encode.h"

namespace builtins {

bool Env::env_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "fastly.env.get", 1))
    return false;

  auto var_name_chars = core::encode(cx, args[0]);
  if (!var_name_chars) {
    return false;
  }
  JS::RootedString env_var(cx, JS_NewStringCopyZ(cx, getenv(var_name_chars.begin())));
  if (!env_var)
    return false;

  args.rval().setString(env_var);
  return true;
}

const JSFunctionSpec Env::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec Env::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec Env::methods[] = {JS_FN("get", env_get, 1, JSPROP_ENUMERATE), JS_FS_END};

const JSPropertySpec Env::properties[] = {JS_PS_END};

JSObject *Env::create(JSContext *cx) {
  JS::RootedObject env(cx, JS_NewPlainObject(cx));
  if (!env || !JS_DefineFunctions(cx, env, methods))
    return nullptr;
  return env;
}

} // namespace builtins
