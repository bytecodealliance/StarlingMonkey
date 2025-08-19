#ifndef SCRIPTLOADER_H
#define SCRIPTLOADER_H

#include <extension-api.h>

#include <js/CompileOptions.h>
#include <js/Modules.h>
#include <js/SourceText.h>

class ScriptLoader {
public:
  ScriptLoader(api::Engine *engine, JS::CompileOptions *opts,
               mozilla::Maybe<std::string> path_prefix);
  ~ScriptLoader();

  static bool define_builtin_module(const char* id, HandleValue builtin);
  static void enable_module_mode(bool enable);
  static bool eval_top_level_script(const char *path, JS::SourceText<mozilla::Utf8Unit> &source,
                             MutableHandleValue result, MutableHandleValue tla_promise);
  static bool load_script(JSContext* cx, const char *script_path, JS::SourceText<mozilla::Utf8Unit> &script);

  /**
   * Load a script without attempting to resolve its path relative to a base path.
   *
   * This is useful for loading ancillary scripts without interfering with, or depending on,
   * the script loader's state as determined by loading and running content scripts.
   */
  static bool load_resolved_script(JSContext *cx, const char *specifier, const char *resolved_path,
                            JS::SourceText<mozilla::Utf8Unit> &script);
};

#endif //SCRIPTLOADER_H
