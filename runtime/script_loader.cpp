#include "script_loader.h"

#include <cstdio>
#include <iostream>
#include <js/CompilationAndEvaluation.h>
#include <js/MapAndSet.h>
#include <js/Value.h>

static JSContext* CONTEXT;
static ScriptLoader* SCRIPT_LOADER;
JS::PersistentRootedObject moduleRegistry;
static bool MODULE_MODE = true;
static char* BASE_PATH = nullptr;
JS::CompileOptions *COMPILE_OPTS;

class AutoCloseFile {
  FILE* file;

public:
  explicit AutoCloseFile(FILE* f) : file(f) {}
  ~AutoCloseFile() {
    release();
  }
  bool release() {
    bool success = true;
    if (file && file != stdin && file != stdout && file != stderr) {
      success = !fclose(file);
    }
    file = nullptr;
    return success;
  }
};

static char* resolve_path(const char* path, const char* base) {
  MOZ_ASSERT(base);
  if (path[0] == '/') {
    return strdup(path);
  }
  if (path[0] == '.' && path[1] == '/') {
    path = path + 2;
  }
  size_t base_len = strlen(base);
  size_t len = base_len + strlen(path) + 1;
  char* resolved_path = new char[len];
  strncpy(resolved_path, base, base_len);
  strncpy(resolved_path + base_len, path, len - base_len);
  MOZ_ASSERT(strlen(resolved_path) == len - 1);

  return resolved_path;
}

static bool load_script(JSContext *cx, const char *script_path, const char* resolved_path,
                        JS::SourceText<mozilla::Utf8Unit> &script);

static JSObject* get_module(JSContext* cx, const char* specifier, const char* resolved_path,
                            const JS::CompileOptions &opts) {
  RootedString resolve_path_str(cx, JS_NewStringCopyZ(cx, resolved_path));
  if (!resolve_path_str) {
    return nullptr;
  }

  RootedValue module_val(cx);
  RootedValue resolve_path_val(cx, StringValue(resolve_path_str));
  if (!JS::MapGet(cx, moduleRegistry, resolve_path_val, &module_val)) {
    return nullptr;
  }

  if (!module_val.isUndefined()) {
    return &module_val.toObject();
  }

  JS::SourceText<mozilla::Utf8Unit> source;
  if (!load_script(cx, specifier, resolved_path, source)) {
    return nullptr;
  }

  RootedObject module(cx, JS::CompileModule(cx, opts, source));
  if (!module) {
    return nullptr;
  }
  module_val.setObject(*module);

  RootedObject info(cx, JS_NewPlainObject(cx));
  if (!info) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, info, "path", resolve_path_val, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  SetModulePrivate(module, ObjectValue(*info));

  if (!MapSet(cx, moduleRegistry, resolve_path_val, module_val)) {
    return nullptr;
  }

  return module;
}

JSObject* module_resolve_hook(JSContext* cx, HandleValue referencingPrivate,
                              HandleObject moduleRequest) {
  RootedString specifier(cx, GetModuleRequestSpecifier(cx, moduleRequest));
  if (!specifier) {
    return nullptr;
  }

  RootedValue path_val(cx, StringValue(specifier));
  auto path = JS_EncodeStringToUTF8(cx, specifier);
  if (!path) {
    return nullptr;
  }
  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  opts.setFileAndLine(path.get(), 1);
  auto resolved_path = resolve_path(path.get(), BASE_PATH);
  return get_module(cx, path.get(), resolved_path, opts);
}

 ScriptLoader::ScriptLoader(JSContext *cx, JS::CompileOptions *opts) {
  MOZ_ASSERT(!SCRIPT_LOADER);
  SCRIPT_LOADER = this;
  CONTEXT = cx;
  COMPILE_OPTS = opts;
  moduleRegistry.init(cx, JS::NewMapObject(cx));
  MOZ_RELEASE_ASSERT(moduleRegistry);
  JSRuntime *rt = JS_GetRuntime(cx);
  SetModuleResolveHook(rt, module_resolve_hook);
}

void ScriptLoader::enable_module_mode(bool enable) {
  MODULE_MODE = enable;
}

static bool load_script(JSContext *cx, const char *specifier, const char* resolved_path,
                               JS::SourceText<mozilla::Utf8Unit> &script) {
  FILE *file = fopen(resolved_path, "r");
  if (!file) {
    std::cerr << "Error opening file " << specifier << " (resolved to " << resolved_path << ")"
              << std::endl;
    return false;
  }

  AutoCloseFile autoclose(file);
  if (fseek(file, 0, SEEK_END) != 0) {
    std::cerr << "Error seeking file " << specifier << std::endl;
    return false;
  }
  size_t len = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0) {
    std::cerr << "Error seeking file " << specifier << std::endl;
    return false;
  }

  UniqueChars buf(js_pod_malloc<char>(len + 1));
  if (!buf) {
    std::cerr << "Out of memory reading " << specifier << std::endl;
    return false;
  }
  size_t cc = fread(buf.get(), sizeof(char), len, file);
  if (cc != len) {
    std::cerr << "Error reading file " << specifier << std::endl;
    return false;
  }

  return script.init(cx, std::move(buf), len);
}

bool ScriptLoader::load_script(JSContext *cx, const char *script_path,
                               JS::SourceText<mozilla::Utf8Unit> &script) {
  auto resolved_path = resolve_path(script_path, BASE_PATH);
  return ::load_script(cx, script_path, resolved_path, script);
}

bool ScriptLoader::load_top_level_script(const char *path, MutableHandleValue result) {
  JSContext *cx = CONTEXT;

  MOZ_ASSERT(!BASE_PATH);
  auto last_slash = strrchr(path, '/');
  size_t base_len;
  if (last_slash) {
    last_slash++;
    base_len = last_slash - path;
    BASE_PATH = new char[base_len + 1];
    MOZ_ASSERT(BASE_PATH);
    strncpy(BASE_PATH, path, base_len);
    BASE_PATH[base_len] = '\0';
  } else {
    BASE_PATH = strdup("./");
  }

  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  opts.setFileAndLine(path, 1);
  JS::RootedScript script(cx);
  RootedObject module(cx);
  if (MODULE_MODE) {
    // Disabling GGC during compilation seems to slightly reduce the number of
    // pages touched post-deploy.
    // (Whereas disabling it during execution below meaningfully increases it,
    // which is why this is scoped to just compilation.)
    JS::AutoDisableGenerationalGC noGGC(cx);
    module = get_module(cx, path, path, opts);
    if (!module) {
      return false;
    }
    if (!ModuleLink(cx, module)) {
      return false;
    }
  } else {
    JS::SourceText<mozilla::Utf8Unit> source;
    if (!::load_script(cx, path, path, source)) {
      return false;
    }
    // See comment above about disabling GGC during compilation.
    JS::AutoDisableGenerationalGC noGGC(cx);
    script = JS::Compile(cx, opts, source);
    if (!script) {
      return false;
    }
  }

  // TODO(performance): verify that it's better to perform a shrinking GC here,
  // as manual testing indicates. Running a shrinking GC here causes *fewer* 4kb
  // pages to be written to when processing a request, at least for one fairly
  // large input script.
  //
  // A hypothesis for why this is the case could be that the objects allocated
  // by parsing the script (but not evaluating it) tend to be read-only, so
  // optimizing them for compactness makes sense and doesn't fragment writes
  // later on.
  // https://github.com/fastly/js-compute-runtime/issues/222
  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);

  // Execute the top-level module script.
  if (!MODULE_MODE) {
    return JS_ExecuteScript(cx, script, result);
  }
  
  if (!ModuleEvaluate(cx, module, result)) {
    return false;
  }
  
  // modules return the top-level await promise in the result value
  // we don't currently support TLA, instead we reassign result
  // with the module namespace
  JS::RootedObject ns(cx, JS::GetModuleNamespace(cx, module));
  result.setObject(*ns);
  return true;
}
