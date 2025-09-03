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

  ScriptLoader(const ScriptLoader &) = default;
  ScriptLoader(ScriptLoader &&) = delete;

  ScriptLoader &operator=(const ScriptLoader &) = default;
  ScriptLoader &operator=(ScriptLoader &&) = delete;

  ~ScriptLoader();

  static bool define_builtin_module(const char* id, HandleValue builtin);
  static void enable_module_mode(bool enable);

  static bool eval_top_level_script(std::string_view path, JS::SourceText<mozilla::Utf8Unit> &source,
                                    MutableHandleValue result, MutableHandleValue tla_promise);

    static bool load_script(JSContext *cx, std::string_view script_path,
                          JS::SourceText<mozilla::Utf8Unit> &script);

  /**
   * Load a script without attempting to resolve its path relative to a base path.
   *
   * This is useful for loading ancillary scripts without interfering with, or depending on,
   * the script loader's state as determined by loading and running content scripts.
   */
  static bool load_resolved_script(JSContext *cx, std::string_view specifier,
                                   std::string_view resolved_path,
                                   JS::SourceText<mozilla::Utf8Unit> &script);
};

#endif //SCRIPTLOADER_H
