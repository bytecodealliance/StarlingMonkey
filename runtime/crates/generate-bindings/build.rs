/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{env, str};
use std::process::Command;
use std::fs;

use bindgen::{Builder, EnumVariation, Formatter};

fn main() {
    let out_dir = "..";
    let config = JSAPIBindgenConfig;
    generate_bindings(&config, &out_dir);
    let config = StarlingBindgenConfig;
    generate_bindings(&config, &out_dir);
    generate_wrappers();
}

fn generate_wrappers() {
    let mode = if build_debug_bindings() {
        "debug"
    } else {
        "release"
    };

    let input_file = "jsapi";
    let input_path = format!("../jsapi-rs/src/{}/bindings_{}.rs", input_file, mode);
    let output_path = format!("../spidermonkey-rs/src/rust/jsapi_wrapped/{}_wrappers_{}.rs", input_file, mode);

    // Format the content with rustfmt and get the result
    let formatted_content = format_rust_code(&input_path);

    // Apply the wrapper generation pipeline
    let processed_functions = process_bindings_content(&formatted_content);

    // Generate the final wrapper file content
    let content = generate_wrapper_file_content(&processed_functions);
    // Write the final content to the output file
    fs::write(&output_path, content)
        .unwrap_or_else(|_| panic!("Failed to write to {}", output_path));
}

fn format_rust_code(input_file: &str) -> String {
    let output = Command::new("rustfmt")
        .arg(input_file)
        .arg("--config")
        .arg("max_width=1000")
        .arg("--emit=stdout")
        .output()
        .expect("Failed to run rustfmt");

    if !output.status.success() {
        panic!("rustfmt failed: {}", String::from_utf8_lossy(&output.stderr));
    }
    
    String::from_utf8(output.stdout)
        .expect("Failed to convert rustfmt output to string")
}

fn process_bindings_content(content: &str) -> Vec<String> {
    BindingsProcessor::new(content)
        .filter_initial_patterns()
        .join_multiline_constructs()
        .remove_empty_braces()
        .normalize_pub_keywords()
        .remove_semicolons()
        .filter_function_declarations()
        .apply_namespace_transforms()
        .filter_unwanted_return_types()
        .collect_results()
}

struct BindingsProcessor {
    content: String,
}

impl BindingsProcessor {
    fn new(content: &str) -> Self {
        Self {
            content: content.to_string(),
        }
    }

    fn filter_initial_patterns(mut self) -> Self {
        let lines: Vec<&str> = self.content
            .lines()
            .filter(|line| !line.contains("link_name"))
            .filter(|line| !line.contains("\"]"))
            .filter(|line| !line.contains("/**"))
            .collect();

        self.content = lines.join("\n");
        self
    }

    fn join_multiline_constructs(mut self) -> Self {
        self.content = self.content
            .replace(",\n ", ", ")     // Join function parameters
            .replace(":\n ", ": ")     // Join type annotations
            .replace("\n ->", " ->");  // Join return type arrows
        self
    }

    fn remove_empty_braces(mut self) -> Self {
        let lines: Vec<String> = self.content
            .lines()
            .map(|line| line.trim())
            .filter(|line| *line != "}")
            .map(|line| line.to_string())
            .collect();

        self.content = lines.join("\n");
        self
    }

    fn normalize_pub_keywords(mut self) -> Self {
        let lines: Vec<String> = self.content
            .lines()
            .map(|line| {
                if let Some(pub_pos) = line.find("pub") {
                    let before_pub = &line[..pub_pos];
                    if before_pub.trim().is_empty() {
                        format!("pub{}", &line[pub_pos + 3..])
                    } else {
                        line.to_string()
                    }
                } else {
                    line.to_string()
                }
            })
            .collect();

        self.content = lines.join("\n");
        self
    }

    fn remove_semicolons(mut self) -> Self {
        self.content = self.content
            .replace(";\n", "\n")
            .replace(";", "");
        self
    }

    fn filter_function_declarations(mut self) -> Self {
        let lines: Vec<String> = self.content
            .lines()
            .map(|line| line.trim())
            .filter(|line| self.is_wanted_function(line))
            .map(|line| line.to_string())
            .collect();

        self.content = lines.join("\n");
        self
    }

    fn is_wanted_function(&self, line: &str) -> bool {
        line.contains("pub fn")
            && line.contains("Handle")
            && !line.contains("roxyHandler")
            && !line.split_whitespace().any(|word| word == "IdVector") // Word boundary match
            && !line.contains("pub fn Unbox")
            && !line.contains("CopyAsyncStack")
    }

    fn apply_namespace_transforms(mut self) -> Self {
        self.content = self.content
            .replace("root::", "raw::")
            .replace("Handle<*mut JSObject>", "HandleObject");
        self
    }

    fn filter_unwanted_return_types(mut self) -> Self {
        let lines: Vec<String> = self.content
            .lines()
            .filter(|line| !line.contains("> HandleObject"))
            .filter(|line| !line.contains("MutableHandleObjectVector"))
            .map(|line| line.to_string())
            .collect();

        self.content = lines.join("\n");
        self
    }

    fn collect_results(self) -> Vec<String> {
        self.content
            .lines()
            .map(|line| line.trim())
            .filter(|line| !line.is_empty())
            .map(|line| line.to_string())
            .collect()
    }
}

fn generate_wrapper_file_content(functions: &[String]) -> String {
    let mut output = String::from(
        r#"mod raw {
  #[allow(unused_imports)]
  pub use crate::raw::*;
  pub use crate::raw::JS::*;
  pub use crate::raw::JS::dbg::*;
  pub use crate::raw::JS::detail::*;
  pub use crate::raw::js::*;
  pub use crate::raw::jsglue::*;
}

"#
    );

    for function in functions {
        output.push_str(&format!("wrap!(raw: {});\n", function));
    }

    output
}

fn generate_bindings(config: &impl BindgenConfig, build_dir: &str) {
    let in_file = config.in_file();

    let mut builder = bindgen::builder()
        .rust_target(bindgen::RustTarget::stable(88, 0).unwrap())
        .header(in_file)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // We're using the `rustified enum` feature because it provides the best
        // ergonomics, and we trust that SpiderMonkey won't provide values that
        // aren't in the enum, so the safety concerns of this feature don't apply.
        .default_enum_style(EnumVariation::Rust { non_exhaustive: false, })
        .derive_partialeq(true)
        .derive_debug(true)
        .merge_extern_blocks(true)
        .wrap_static_fns(true)
        .translate_enum_integer_types(true)
        .fit_macro_constants(true)
        .generate_cstr(true)
        .size_t_is_usize(true)
        .enable_function_attribute_detection()
        .enable_cxx_namespaces()
        .layout_tests(false)
        .formatter(Formatter::Prettyplease)
        .clang_arg("-I")
        .clang_arg(config.sm_headers())
        .clang_arg("-x")
        .clang_arg("c++")
        .clang_arg("-fvisibility=default")
        .clang_arg("--target=wasm32-wasip1")
        .emit_diagnostics()
        ;

    match get_env("SYSROOT").as_str() {
        "" => {}
        sysroot_path => {
            builder = builder.clang_arg("--sysroot")
                .clang_arg(sysroot_path);
        }
    }

    for flag in config.cc_flags() {
        builder = builder.clang_arg(flag);
    }

    for ty in config.unsafe_impl_sync_types() {
        builder = builder.raw_line(format!("unsafe impl Sync for root::{} {{}}", ty));
    }

    for ty in config.allowlist_types() {
        builder = builder.allowlist_type(ty);
    }

    for var in config.blocklist_vars() {
        builder = builder.blocklist_var(var);
    }

    for var in config.allowlist_vars() {
        builder = builder.allowlist_var(var);
    }

    for func in config.allowlist_functions() {
        builder = builder.allowlist_function(func);
    }

    for func in config.blocklist_functions() {
        builder = builder.blocklist_function(func);
    }

    for ty in config.opaque_types() {
        builder = builder.opaque_type(ty);
    }

    for ty in config.blocklist_types() {
        builder = builder.blocklist_type(ty);
    }

    for ty in config.blocklist_items() {
        builder = builder.blocklist_item(ty);
    }

    for ty in config.allowlist_items() {
        builder = builder.allowlist_item(ty);
    }

    for &(module, raw_line) in config.module_raw_lines() {
        builder = builder.module_raw_line(module, raw_line);
    }

    builder = config.apply_additional(builder);

    // This should ideally be added by bindgen itself, but until then, adding it as a raw line
    // at the top level happens to work.
    builder = builder.raw_line("#[allow(
    unnecessary_transmutes,
    reason = \"https://github.com/rust-lang/rust-bindgen/issues/3241\"
)]");

    let out = format!("{build_dir}/{}", config.out_file());

    let cmd = format!("LIBCLANG_PATH={} bindgen {}",
                      get_env("LIBCLANG_PATH"),
                      builder.command_line_flags().iter().map(|flag| if flag.contains(|c| " \n\t:*".contains(c)) {
                          format!("\"{flag}\"")
                      } else {
                          flag.to_string()
                      }).collect::<Vec<_>>().join(" "));

    println!(
        "Generating bindings {out} with clang version {}. Command line:\n{cmd}",
        bindgen::clang_version().full
    );

    let bindings = builder
        .generate()
        .expect("Should generate JSAPI bindings OK");

    println!("Writing bindings to file {out:?}");
    bindings
        .write_to_file(out)
        .expect("Should write bindings to file OK");
}

/// Get an environment variable, or the empty string if it is not set.
/// Also prints a rerun-if-env-changed directive for the variable.
fn get_env(name: &str) -> String {
    println!("cargo:rerun-if-env-changed={name}");
    env::var(name).unwrap_or_else(|_| String::from(""))
}

fn build_debug_bindings() -> bool {
    match get_env("DEBUG").as_str() {
        "1" | "true" | "TRUE" => true,
        _ => false,
    }
}

trait BindgenConfig {
    const UNSAFE_IMPL_SYNC_TYPES: &'static [&'static str] = &[];
    fn unsafe_impl_sync_types(&self) -> &'static [&'static str] {
        Self::UNSAFE_IMPL_SYNC_TYPES
    }

    /// Items of any kind we want to generate bindings to.
    const ALLOWLIST_ITEMS: &'static [&'static str] = &[];
    fn allowlist_items(&self) -> &'static [&'static str] {
        Self::ALLOWLIST_ITEMS
    }

    /// Items for which we should NEVER generate bindings, even if it is used within
    /// a type or function signature that we are generating bindings for.
    const BLOCKLIST_ITEMS: &'static [&'static str] = &[];
    fn blocklist_items(&self) -> &'static [&'static str] {
        Self::BLOCKLIST_ITEMS
    }

    const ALLOWLIST_TYPES: &'static [&'static str] = &[];
    fn allowlist_types(&self) -> &'static [&'static str] {
        Self::ALLOWLIST_TYPES
    }

    /// Global variables we want to generate bindings to.
    const BLOCKLIST_VARS: &'static [&'static str] = &[];
    fn blocklist_vars(&self) -> &'static [&'static str] {
        Self::BLOCKLIST_VARS
    }

    /// Global variables we want to generate bindings to.
    const ALLOWLIST_VARS: &'static [&'static str] = &[];
    fn allowlist_vars(&self) -> &'static [&'static str] {
        Self::ALLOWLIST_VARS
    }

    /// Functions we want to generate bindings to.
    const ALLOWLIST_FUNCTIONS: &'static [&'static str] = &[];
    fn allowlist_functions(&self) -> &'static [&'static str] {
        Self::ALLOWLIST_FUNCTIONS
    }

    /// Functions we do not want to generate bindings to.
    const BLOCKLIST_FUNCTIONS: &'static [&'static str] = &[];
    fn blocklist_functions(&self) -> &'static [&'static str] {
        Self::BLOCKLIST_FUNCTIONS
    }

    /// Types that should be treated as an opaque blob of bytes whenever they show
    /// up within a whitelisted type.
    ///
    /// These are types which are too tricky for bindgen to handle, and/or use C++
    /// features that don't have an equivalent in rust, such as partial template
    /// specialization.
    const OPAQUE_TYPES: &'static [&'static str] = &[];
    fn opaque_types(&self) -> &'static [&'static str] {
        Self::OPAQUE_TYPES
    }

    /// Types for which we should NEVER generate bindings, even if it is used within
    /// a type or function signature that we are generating bindings for.
    const BLOCKLIST_TYPES: &'static [&'static str] = &[];
    fn blocklist_types(&self) -> &'static [&'static str] {
        Self::BLOCKLIST_TYPES
    }

    /// Definitions for types that were blacklisted
    const MODULE_RAW_LINES: &'static [(&'static str, &'static str)];
    fn module_raw_lines(&self) -> &'static [(&'static str, &'static str)] {
        Self::MODULE_RAW_LINES
    }

    fn cc_flags(&self) -> Vec<&'static str> {
        let mut result = vec![
            "-DRUST_BINDGEN",
            "-DSTATIC_JS_API",
            "-std=gnu++20",
            "-Wall",
            "-Qunused-arguments",
            "-fno-sized-deallocation",
            "-fno-aligned-new",
            "-mthread-model", "single",
            "-fPIC",
            "-fno-rtti",
            "-fno-exceptions",
            "-fno-math-errno",
            "-pipe",
            "-fno-omit-frame-pointer",
            "-funwind-tables",
            "-m32"
        ];

        if build_debug_bindings() {
            result.extend(& ["-DJS_GC_ZEAL", "-DDEBUG", "-DJS_DEBUG"]);
        }

        result
    }

    fn sm_headers(&self) -> String {
        let path = get_env("SM_HEADERS");
        if path == "" {
            if env::var("IJ_RESTARTER_LOG").is_ok() {
                // We're most likely running under IntelliJ's (CLion's, RustRover's) automatic
                // `cargo check` run which doesn't support any configuration, so just quit here.
                std::process::exit(0);
            }

            println!("cargo::error=SM_HEADERS must be set to the directory containing \
                          SpiderMonkey's headers");
            std::process::exit(1);
        }
        path
    }

    fn in_file(&self) -> &str;

    fn out_file(&self) -> &str;

    fn apply_additional(&self, builder: Builder) -> Builder {
        builder
    }
}

struct JSAPIBindgenConfig;

impl BindgenConfig for JSAPIBindgenConfig {
    const UNSAFE_IMPL_SYNC_TYPES: &'static [&'static str] = &[
        "JSClass",
        "JSFunctionSpec",
        "JSNativeWrapper",
        "JSPropertySpec",
        "JSTypedMethodJitInfo",
    ];

    const ALLOWLIST_ITEMS: &'static [&'static str] = &[
        "JS.*",
        "js::.*",
        "JS_.*",
        "mozilla::.*",
        "jsglue::.*",
        "JSCLASS_.*",
        "JSFUN_.*",
        "JSITER_.*",
        "JSPROP_.*",
        "js::Proxy.*",
        "std::unique_ptr",
    ];

    const BLOCKLIST_ITEMS: &'static [&'static str] = &[
        // We'll be using libc::FILE.
        "FILE",
        // We provide our own definition because we need to express trait bounds in
        // the definition of the struct to make our Drop implementation correct.
        "JS::Heap",
        // We provide our own definition because SM's use of templates
        // is more than bindgen can cope with.
        "JS::Rooted",
        "JS::RootedTuple",
        // We don't need them and bindgen doesn't like them.
        "JS::HandleVector",
        "JS::MutableHandleVector",
        "JS::Rooted.*Vector",
        "JS::RootedValueArray",
        // Bindgen can't handle the AutoFilename class.
        "JS::DescribeScriptedCaller",
        // Bindgen generates a bad enum for `HashTable_RebuildStatus`.
        // We don't need any of these, so just block them all.
        "JS::.*Stencil.*",
        "JS::.*Delazifications.*",
        "mozilla::Maybe::.*",
    ];

    const BLOCKLIST_FUNCTIONS: &'static [&'static str] = &[
        "JS::CopyAsyncStack",
        "JS::CreateError",
        "JS::DecodeMultiStencilsOffThread",
        "JS::DecodeStencilOffThread",
        "JS::DescribeScriptedCaller",
        "JS::EncodeStencil",
        "JS::FinishDecodeMultiStencilsOffThread",
        "JS::FinishIncrementalEncoding",
        "JS::FromPropertyDescriptor",
        "JS::GetExceptionCause",
        "JS::GetModulePrivate",
        "JS::GetOptimizedEncodingBuildId",
        "JS::GetPromiseResult",
        "JS::GetRegExpFlags",
        "JS::GetScriptPrivate",
        "JS::GetScriptTranscodingBuildId",
        "JS::GetScriptedCallerPrivate",
        "JS::MaybeGetScriptPrivate",
        "JS::NewArrayBufferWithContents",
        "JS::NewExternalArrayBuffer",
        "JS::SimpleStringToBigInt",
        "JS::DeflateStringToUTF8Buffer",
        "JS::InitSelfHostedCode",
        "JS::ArrayBuffer_getData",
        "JS::ArrayBufferView_getData",
        "JS::StringIsASCII",
        "JS::dbg::FireOnGarbageCollectionHook",
        "JS_EncodeStringToUTF8BufferPartial",
        "JS_GetEmptyStringValue",
        "JS_GetErrorType",
        "JS_GetOwnPropertyDescriptorById",
        "JS_GetOwnPropertyDescriptor",
        "JS_GetOwnUCPropertyDescriptor",
        "JS_GetPropertyDescriptorById",
        "JS_GetPropertyDescriptor",
        "JS_GetReservedSlot",
        "JS_GetUCPropertyDescriptor",
        "JS_NewLatin1String",
        "JS_NewUCStringDontDeflate",
        "JS_NewUCString",
        "JS_PCToLineNumber",
        "js::AppendUnique",
        "js::SetPropertyIgnoringNamedGetter",
        "mozilla::detail::StreamPayload*",
    ];

    const OPAQUE_TYPES: &'static [&'static str] = &[
        "JS::StackGCVector.*",
        "JS::PersistentRooted.*",
        "JS::detail::CallArgsBase",
        "js::detail::UniqueSelector.*",
        "mozilla::BufferList",
        "mozilla::UniquePtr.*",
        "mozilla::Variant",
        "mozilla::Maybe",
        "mozilla::Hash.*",
        "mozilla::detail::Hash.*",
        "RefPtr_Proxy.*",
        "std::.*",
    ];

    /// Definitions for types that were blocklisted
    const MODULE_RAW_LINES: &'static [(&'static str, &'static str)] = &[
        ("root", "pub type FILE = ::libc::FILE;"),
        ("root::JS", "pub type Heap<T> = crate::jsgc::Heap<T>;"),
        ("root::JS", "pub type Rooted<T> = crate::jsgc::Rooted<T>;"),
    ];

    fn in_file(&self) -> &str {
        "../jsapi-rs/cpp/jsglue.cpp"
    }

    fn out_file(&self) -> &str {

        if build_debug_bindings() {
            "jsapi-rs/src/jsapi/bindings_debug.rs"
        } else {
            "jsapi-rs/src/jsapi/bindings_release.rs"
        }
    }
}

struct StarlingBindgenConfig;

impl BindgenConfig for StarlingBindgenConfig {
    const ALLOWLIST_ITEMS: &'static [&'static str] = &[
        "api::.*",
    ];

    const BLOCKLIST_ITEMS: &'static [&'static str] = &[
        "std::.*",
        "mozilla::.*",
        "glue::.*",
        "JS::.*",
        "js::.*",
        "JS.*",
        // Bindgen can't handle the use of std::vector here.
        "api::AsyncTask_select",
    ];

    const OPAQUE_TYPES: &'static [&'static str] = &[
        "api::Engine",
        "std::unique_ptr",
    ];

    const MODULE_RAW_LINES: &'static [(&'static str, &'static str)] = &[
        ("root", "pub use spidermonkey_rs::raw::*;"),
        ("root::JS", "pub use spidermonkey_rs::raw::JS::*;"),
    ];

    fn in_file(&self) -> &str {
        "../../../include/extension-api.h"
    }

    fn out_file(&self) -> &str {
        "starlingmonkey-rs/src/bindings.rs"
    }
}
