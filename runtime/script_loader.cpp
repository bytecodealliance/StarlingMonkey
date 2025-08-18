#include "script_loader.h"
#include "encode.h"

#include <cstdio>
#include <js/CompilationAndEvaluation.h>
#include <js/MapAndSet.h>
#include <js/Value.h>
#include <jsfriendapi.h>
#include <sys/stat.h>

namespace {

static api::Engine* ENGINE;
static ScriptLoader* SCRIPT_LOADER;
static bool MODULE_MODE = true;
static std::string BASE_PATH;

JS::PersistentRootedObject moduleRegistry;
JS::PersistentRootedObject builtinModules;
JS::CompileOptions *COMPILE_OPTS;

mozilla::Maybe<std::string> PATH_PREFIX = mozilla::Nothing();

} // namespace

using host_api::HostString;

namespace ScriptLoaderErrors {
DEF_ERR(ModuleLoadingError, JSEXN_REFERENCEERR,
        "Error loading module \"{0}\" (resolved path \"{1}\"): {2}", 3)
DEF_ERR(BuiltinModuleExists, JSEXN_TYPEERR, "Builtin module \"{0}\" already exists", 1)
};

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
    // Clang analyzer complains about the stream leaking when stream is stdin, stdout, or stderr.
    // NOLINTNEXTLINE(clang-analyzer-unix.Stream)
    return success;
  }
};

// strip off the given prefix when possible for nicer debugging stacks
static std::string strip_prefix(std::string_view resolved_path,
                                mozilla::Maybe<std::string> path_prefix) {
  if (!path_prefix) {
    return std::string(resolved_path);
  }

  const auto& base = *path_prefix;
  if (!resolved_path.starts_with(base)) {
    return std::string(resolved_path);
  }

  return std::string(resolved_path.substr(base.size()));
}

struct stat s;


static std::string resolve_extension(std::string resolved_path) {
  if (stat(resolved_path.c_str(), &s) == 0) {
    return resolved_path;
  }

  if (resolved_path.size() >= 3 &&
      resolved_path.compare(resolved_path.size() - 3, 3, ".js") == 0) {
    return resolved_path;
  }

  std::string with_ext = resolved_path + ".js";
  if (stat(with_ext.c_str(), &s) == 0) {
    return with_ext;
  }
  return resolved_path;
}

static std::string resolve_path(std::string_view path, std::string_view base) {
  size_t base_len = base.size();
  while (base_len > 0 && base[base_len - 1] != '/') {
    base_len--;
  }
  size_t path_len = path.size();

  // create the maximum buffer size as a working buffer
  std::string resolved_path;
  resolved_path.reserve(base_len + path_len + 1);

  // copy the base in if used
  size_t resolved_len = base_len;
  if (!path.empty() && path[0] == '/') {
    resolved_len = 0;
  } else {
    resolved_path.assign(base.substr(0, base_len));
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
        resolved_len--;
      }
      while (resolved_len > 0 && resolved_path[resolved_len - 1] != '/') {
        resolved_len--;
      }
      // Resize string to match resolved_len
      resolved_path.resize(resolved_len);
      continue;
    }
    // normal segment to copy (with the trailing / if not the last segment)
    if (path_cur_idx < path_len && path[path_cur_idx] == '/')
      path_cur_idx++;

    // Copy segment equivalent to: strncpy(resolved_path + resolved_len, path + path_from_idx, path_cur_idx - path_from_idx);
    resolved_path.append(path.substr(path_from_idx, path_cur_idx - path_from_idx));
    resolved_len += path_cur_idx - path_from_idx;
    path_from_idx = path_cur_idx;
  }

  return resolve_extension(std::move(resolved_path));
}

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
  if (!SCRIPT_LOADER->load_resolved_script(cx, specifier, resolved_path, source)) {
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
  opts.setFile("<internal>");
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
  auto resolved_path = resolve_path(path.get(), str.ptr.get());

  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  auto stripped = strip_prefix(resolved_path, PATH_PREFIX);
  opts.setFileAndLine(stripped.c_str(), 1);
  return get_module(cx, path.get(), resolved_path.c_str(), opts);
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

ScriptLoader::ScriptLoader(api::Engine* engine, JS::CompileOptions *opts,
                           mozilla::Maybe<std::string> path_prefix) {
  MOZ_ASSERT(!SCRIPT_LOADER);
  ENGINE = engine;
  SCRIPT_LOADER = this;
  COMPILE_OPTS = opts;
  PATH_PREFIX = std::move(path_prefix);
  JSContext* cx = engine->cx();
  moduleRegistry.init(cx, JS::NewMapObject(cx));
  builtinModules.init(cx, JS::NewMapObject(cx));
  MOZ_RELEASE_ASSERT(moduleRegistry);
  MOZ_RELEASE_ASSERT(builtinModules);
  JSRuntime *rt = JS_GetRuntime(cx);
  SetModuleResolveHook(rt, module_resolve_hook);
  SetModuleMetadataHook(rt, module_metadata_hook);
}

bool ScriptLoader::define_builtin_module(const char* id, HandleValue builtin) {
  JSContext* cx = ENGINE->cx();
  RootedString id_str(cx, JS_NewStringCopyZ(cx, id));
  if (!id_str) {
    return false;
  }
  RootedValue module_val(cx);
  RootedValue id_val(cx, StringValue(id_str));
  bool already_exists = false;
  if (!MapHas(cx, builtinModules, id_val, &already_exists)) {
    return false;
  }
  if (already_exists) {
    return api::throw_error(cx, ScriptLoaderErrors::BuiltinModuleExists, "id");
  }
  if (!MapSet(cx, builtinModules, id_val, builtin)) {
    return false;
  }
  return true;
}

void ScriptLoader::enable_module_mode(bool enable) {
  MODULE_MODE = enable;
}

bool ScriptLoader::load_resolved_script(JSContext *cx, const char *specifier,
                                        const char* resolved_path,
                                        JS::SourceText<mozilla::Utf8Unit> &script) {
  FILE *file = fopen(resolved_path, "r");
  if (!file) {
    return api::throw_error(cx, ScriptLoaderErrors::ModuleLoadingError,
      specifier, resolved_path, std::strerror(errno));
  }

  AutoCloseFile autoclose(file);
  if (fseek(file, 0, SEEK_END) != 0) {
    return api::throw_error(cx, ScriptLoaderErrors::ModuleLoadingError,
      specifier, resolved_path, "can't read from file");
  }
  size_t len = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0) {
    return api::throw_error(cx, ScriptLoaderErrors::ModuleLoadingError,
      specifier, resolved_path, "can't read from file");
  }

  UniqueChars buf(js_pod_malloc<char>(len + 1));
  if (!buf) {
    return api::throw_error(cx, ScriptLoaderErrors::ModuleLoadingError,
      specifier, resolved_path, "out of memory while reading file");
  }
  size_t cc = fread(buf.get(), sizeof(char), len, file);
  if (cc != len) {
    return api::throw_error(cx, ScriptLoaderErrors::ModuleLoadingError,
      specifier, resolved_path, "error reading file");
  }

  return script.init(cx, std::move(buf), len);
}

bool ScriptLoader::load_script(JSContext *cx, const char *script_path,
                               JS::SourceText<mozilla::Utf8Unit> &script) {

  std::string path(script_path);
  std::string resolved;
  if (BASE_PATH.empty()) {
    auto pos = path.find_last_of('/');
    if (pos != std::string::npos) {
      BASE_PATH = path.substr(0, pos + 1);
    } else {
      BASE_PATH = "./";
    }
    resolved = path;
  } else {
    resolved = resolve_path(path, BASE_PATH);
  }
  return load_resolved_script(cx, script_path, resolved.c_str(), script);
}

bool ScriptLoader::eval_top_level_script(const char *path, JS::SourceText<mozilla::Utf8Unit> &source,
                                         MutableHandleValue result, MutableHandleValue tla_promise) {
  JSContext *cx = ENGINE->cx();

  JS::CompileOptions opts(cx, *COMPILE_OPTS);
  auto stripped = strip_prefix(path, PATH_PREFIX);
  opts.setFileAndLine(stripped.c_str(), 1);

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
  if (ENGINE->state() == api::EngineState::ScriptPreInitializing) {
    JS::PrepareForFullGC(cx);
    JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);
  }

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
