#ifndef SCRIPTLOADER_H
#define SCRIPTLOADER_H

#include <extension-api.h>

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include <js/CompileOptions.h>
#include <js/Modules.h>
#include <js/SourceText.h>
#pragma clang diagnostic pop

class ScriptLoader {
public:
  ScriptLoader(JSContext* cx, JS::CompileOptions* opts);
  ~ScriptLoader();

  bool define_builtin_module(const char* id, HandleValue builtin);
  void enable_module_mode(bool enable);
  bool eval_top_level_script(const char *path, JS::SourceText<mozilla::Utf8Unit> &source,
                             MutableHandleValue result, MutableHandleValue tla_promise);
  bool load_script(JSContext* cx, const char *script_path, JS::SourceText<mozilla::Utf8Unit> &script);
};

#endif //SCRIPTLOADER_H
