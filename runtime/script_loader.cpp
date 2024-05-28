#include "script_loader.h"
#include "encode.h"

#include <cstdio>
#include <iostream>
#include <js/CompilationAndEvaluation.h>
#include <js/MapAndSet.h>
#include <js/Value.h>
#include <jsfriendapi.h>
#include <sys/stat.h>

static JSContext* CONTEXT;
static ScriptLoader* SCRIPT_LOADER;
JS::PersistentRootedObject moduleRegistry;
JS::PersistentRootedObject builtinModules;
static bool MODULE_MODE = true;
static char* BASE_PATH = nullptr;
JS::CompileOptions *COMPILE_OPTS;

using host_api::HostString;

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

// strip off base when possible for nicer debugging stacks
static const char* strip_base(const char* resolved_path, const char* base) {
  size_t base_len = strlen(base);
  if (strncmp(resolved_path, base, base_len) != 0) {
    return strdup(resolved_path);
  }
  size_t path_len = strlen(resolved_path);
  char* buf(js_pod_malloc<char>(path_len - base_len + 1));
  strncpy(buf, &resolved_path[base_len], path_len - base_len + 1);
  return buf;
}

struct stat s;

static const char* resolve_extension(const char* resolved_path) {
  if (stat(resolved_path, &s) == 0) {
    return resolved_path;
  }
  size_t len = strlen(resolved_path);
  if (strncmp(resolved_path + len - 3, ".js", 3) == 0) {
    return resolved_path;
  }
  char* resolved_path_ext = new char[len + 4];
  strncpy(resolved_path_ext, resolved_path, len);
  strncpy(resolved_path_ext + len, ".js", 4);
  MOZ_ASSERT(strlen(resolved_path_ext) == len + 3);
  if (stat(resolved_path_ext, &s) == 0) {
    delete[] resolved_path;
    return resolved_path_ext;
  }
  delete[] resolved_path_ext;
  return resolved_path;
}

static const char* resolve_path(const char* path, const char* base, size_t base_len) {
  MOZ_ASSERT(base);
  while (base_len > 0 && base[base_len - 1] != '/') {
    base_len--;
  }
  size_t path_len = strlen(path);

  // create the maximum buffer size as a working buffer
  char* resolved_path = new char[base_len + path_len + 1];

  // copy the base in if used
  size_t resolved_len = base_len;
  if (path[0] == '/') {
    resolved_len = 0;
  } else {
    strncpy(resolved_path, base, base_len);
  }

  // Iterate through each segment of the path, copying each segment into the resolved path,
  // while handling backtracking for .. segments and skipping . segments appropriately.
  size_t path_from_idx = 0;
  size_t path_cur_idx = 0;
  while (path_cur_idx < path_len) {
    // read until the end or the next / to get the segment position
    // as the substring between path_from_idx and path_cur_idx
    while (path_cur_idx < path_len && path[path_cur_idx] != '/')
      path_cur_idx++;
    if (path_cur_idx == path_from_idx)
      break;
    // . segment to skip
    if (path_cur_idx - path_from_idx == 1 && path[path_from_idx] == '.') {
      path_cur_idx++;
      path_from_idx = path_cur_idx;
      continue;
    }
    // .. segment backtracking
    if (path_cur_idx - path_from_idx == 2 && path[path_from_idx] == '.' && path[path_from_idx + 1] == '.') {
      path_cur_idx++;
      path_from_idx = path_cur_idx;
      if (resolved_len > 0 && resolved_path[resolved_len - 1] == '/') {
        resolved_len --;
      }
      while (resolved_len > 0 && resolved_path[resolved_len - 1] != '/') {
        resolved_len--;
      }
      continue;
    }
    // normal segment to copy (with the trailing / if not the last segment)
    if (path[path_cur_idx] == '/')
      path_cur_idx++;
    strncpy(resolved_path + resolved_len, path + path_from_idx, path_cur_idx - path_from_idx);
    resolved_len += path_cur_idx - path_from_idx;
    path_from_idx = path_cur_idx;
  }

  // finalize the buffer
  resolved_path[resolved_len] = '\0';
  MOZ_ASSERT(strlen(resolved_path) == resolved_len);
  return resolve_extension(resolved_path);
}

static bool load_script(JSContext *cx, const char *script_path, const char* resolved_path,
                        JS::SourceText<mozilla::Utf8Unit> &script);

static JSObject* get_module(JSContext* cx, JS::SourceText<mozilla::Utf8Unit> &source,
                            const char* resolved_path, const JS::CompileOptions &opts) {
  RootedObject module(cx, JS::CompileModule(cx, opts, source));
  if (!module) {
    return nullptr;
  }
  RootedValue module_val(cx, ObjectValue(*module));

  RootedObject info(cx, JS_NewPlainObject(cx));
  if (!info) {
    return nullptr;
  }

  RootedString resolved_path_str(cx, JS_NewStringCopyZ(cx, resolved_path));
  if (!resolved_path_str) {
    return nullptr;
  }
  RootedValue resolved_path_val(cx, StringValue(resolved_path_str));

  if (!JS_DefineProperty(cx, info, "id", resolved_path_val, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  SetModulePrivate(module, ObjectValue(*info));

  if (!MapSet(cx, moduleRegistry, resolved_path_val, module_val)) {
    return nullptr;
  }

  return module;
}

static JSObject* get_module(JSContext* cx, const char* specifier, const char* resolved_path,
                            const JS::CompileOptions &opts) {
  RootedString resolved_path_str(cx, JS_NewStringCopyZ(cx, resolved_path));
  if (!resolved_path_str) {
    return nullptr;
  }
  RootedValue resolved_path_val(cx, StringValue(resolved_path_str));

  RootedValue module_val(cx);
  if (!JS::MapGet(cx, moduleRegistry, resolved_path_val, &module_val)) {
    return nullptr;
  }

  if (!module_val.isUndefined()) {
    return &module_val.toObject();
  }

  JS::SourceText<mozilla::Utf8Unit> source;
  if (!load_script(cx, specifier, resolved_path, source)) {
    return nullptr;
  }

  return get_module(cx, source, resolved_path, opts);
}

static JSObject* get_builtin_module(JSContext* cx, HandleValue id, HandleObject builtin) {
  RootedValue module_val(cx);
  MOZ_ASSERT(id.isString());
  if (!JS::MapGet(cx, moduleRegistry, id, &module_val)) {
    return nullptr;
  }
  if (!module_val.isUndefined()) {
    return &module_val.toObject();
  }

  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  JS::SourceText<mozilla::Utf8Unit> source;

  std::string code = "const { ";
  JS::RootedIdVector props(cx);
  GetPropertyKeys(cx, builtin, JSITER_OWNONLY, &props);

  size_t length = props.length();
  bool firstValue = true;
  for (size_t i = 0; i < length; ++i) {
    if (firstValue) {
      firstValue = false;  
    } else {
      code += ", ";
    }

    code += "'";
    const auto &prop = props[i];
    JS::RootedValue key(cx, js::IdToValue(prop));
    if (!key.isString()) {
      return nullptr;
    }
    auto key_str = core::encode(cx, key);
    code += std::string_view(key_str.ptr.get(), key_str.len);
    code += "': ";

    code += "e";
    code += std::to_string(i);
  }

  code += " } = import.meta.builtin;\nexport { ";

  firstValue = true;
  for (size_t i = 0; i < length; ++i) {
    if (firstValue) {
      firstValue = false;  
    } else {
      code += ", ";
    }

    code += "e";
    code += std::to_string(i);
    
    code += " as '";
    const auto &prop = props[i];
    JS::RootedValue key(cx, js::IdToValue(prop));
    if (!key.isString()) {
      return nullptr;
    }
    auto key_str = core::encode(cx, key);
    code += std::string_view(key_str.ptr.get(), key_str.len);
    code += "'";
  }
  code += " }\n";

  if (!source.init(cx, code.c_str(), strlen(code.c_str()), JS::SourceOwnership::Borrowed)) {
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

  if (!JS_DefineProperty(cx, info, "id", id, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  SetModulePrivate(module, ObjectValue(*info));

  if (!MapSet(cx, moduleRegistry, id, module_val)) {
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

  RootedValue builtin_val(cx);
  if (!MapGet(cx, builtinModules, path_val, &builtin_val)) {
    return nullptr; 
  }
  if (!builtin_val.isUndefined()) {
    RootedValue specifier_val(cx, StringValue(specifier));
    RootedObject builtin_obj(cx, &builtin_val.toObject());
    return get_builtin_module(cx, specifier_val, builtin_obj);
  }

  RootedObject info(cx, &referencingPrivate.toObject());
  RootedValue parent_path_val(cx);
  if (!JS_GetProperty(cx, info, "id", &parent_path_val)) {
    return nullptr;
  }
  if (!parent_path_val.isString()) {
    return nullptr;
  }

  HostString str = core::encode(cx, parent_path_val);
  const char* resolved_path = resolve_path(path.get(), str.ptr.get(), str.len);

  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  opts.setFileAndLine(strip_base(resolved_path, BASE_PATH), 1);
  return get_module(cx, path.get(), resolved_path, opts);
}

bool module_metadata_hook(JSContext* cx, HandleValue referencingPrivate, HandleObject metaObject) {
  RootedObject info(cx, &referencingPrivate.toObject());
  RootedValue parent_id_val(cx);
  if (!JS_GetProperty(cx, info, "id", &parent_id_val)) {
    return false;
  }
  if (!parent_id_val.isString()) {
    return false;
  }
  RootedValue builtin_val(cx);
  if (!MapGet(cx, builtinModules, parent_id_val, &builtin_val)) {
    return false;
  }
  if (builtin_val.isUndefined()) {
    return false;
  }
  JS_SetProperty(cx, metaObject, "builtin", builtin_val);
  return true;
}

ScriptLoader::ScriptLoader(JSContext *cx, JS::CompileOptions *opts) {
  MOZ_ASSERT(!SCRIPT_LOADER);
  SCRIPT_LOADER = this;
  CONTEXT = cx;
  COMPILE_OPTS = opts;
  moduleRegistry.init(cx, JS::NewMapObject(cx));
  builtinModules.init(cx, JS::NewMapObject(cx));
  MOZ_RELEASE_ASSERT(moduleRegistry);
  MOZ_RELEASE_ASSERT(builtinModules);
  JSRuntime *rt = JS_GetRuntime(cx);
  SetModuleResolveHook(rt, module_resolve_hook);
  SetModuleMetadataHook(rt, module_metadata_hook);
}

bool ScriptLoader::define_builtin_module(const char* id, HandleValue builtin) {
  RootedString id_str(CONTEXT, JS_NewStringCopyZ(CONTEXT, id));
  if (!id_str) {
    return false;
  }
  RootedValue module_val(CONTEXT);
  RootedValue id_val(CONTEXT, StringValue(id_str));
  bool already_exists;
  if (!MapHas(CONTEXT, builtinModules, id_val, &already_exists)) {
    return false;
  }
  if (already_exists) {
    fprintf(stderr, "Unable to define builtin %s, as it already exists", id);
    return false;
  }
  if (!MapSet(CONTEXT, builtinModules, id_val, builtin)) {
    return false;
  }
  return true;
}

void ScriptLoader::enable_module_mode(bool enable) {
  MODULE_MODE = enable;
}

static bool load_script(JSContext *cx, const char *specifier, const char* resolved_path,
                               JS::SourceText<mozilla::Utf8Unit> &script) {
  FILE *file = fopen(resolved_path, "r");
  if (!file) {
    std::cerr << "Error opening file " << specifier << " (resolved to " << resolved_path << "): "
              << std::strerror(errno) << std::endl;
    return false;
  }

  AutoCloseFile autoclose(file);
  if (fseek(file, 0, SEEK_END) != 0) {
    std::cerr << "Error seeking file " << resolved_path << std::endl;
    return false;
  }
  size_t len = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0) {
    std::cerr << "Error seeking file " << resolved_path << std::endl;
    return false;
  }

  UniqueChars buf(js_pod_malloc<char>(len + 1));
  if (!buf) {
    std::cerr << "Out of memory reading " << resolved_path << std::endl;
    return false;
  }
  size_t cc = fread(buf.get(), sizeof(char), len, file);
  if (cc != len) {
    std::cerr << "Error reading file " << resolved_path << std::endl;
    return false;
  }

  return script.init(cx, std::move(buf), len);
}

bool ScriptLoader::load_script(JSContext *cx, const char *script_path,
                               JS::SourceText<mozilla::Utf8Unit> &script) {
  const char *resolved_path;
  if (!BASE_PATH) {
    auto last_slash = strrchr(script_path, '/');
    size_t base_len;
    if (last_slash) {
      last_slash++;
      base_len = last_slash - script_path;
      BASE_PATH = new char[base_len + 1];
      MOZ_ASSERT(BASE_PATH);
      strncpy(BASE_PATH, script_path, base_len);
      BASE_PATH[base_len] = '\0';
    } else {
      BASE_PATH = strdup("./");
    }
    resolved_path = script_path;
  } else {
    resolved_path = resolve_path(script_path, BASE_PATH, strlen(BASE_PATH));
  }

  return ::load_script(cx, script_path, resolved_path, script);
}

bool ScriptLoader::eval_top_level_script(const char *path, JS::SourceText<mozilla::Utf8Unit> &source,
                                         MutableHandleValue result, MutableHandleValue tla_promise) {
  JSContext *cx = CONTEXT;

  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  opts.setFileAndLine(strip_base(path, BASE_PATH), 1);
  JS::RootedScript script(cx);
  RootedObject module(cx);
  if (MODULE_MODE) {
    // Disabling GGC during compilation seems to slightly reduce the number of
    // pages touched post-deploy.
    // (Whereas disabling it during execution below meaningfully increases it,
    // which is why this is scoped to just compilation.)
    JS::AutoDisableGenerationalGC noGGC(cx);
    module = get_module(cx, source, path, opts);
    if (!module) {
      return false;
    }
    if (!ModuleLink(cx, module)) {
      return false;
    }
  } else {
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

  if (!ModuleEvaluate(cx, module, tla_promise)) {
    return false;
  }

  JS::RootedObject ns(cx, JS::GetModuleNamespace(cx, module));
  result.setObject(*ns);
  return true;
}
