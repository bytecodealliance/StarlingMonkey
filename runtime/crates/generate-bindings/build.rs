/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{env, str};
use std::process::Command;

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
    let script = "generate-bindings/src/generate_wrappers.sh";
    println!("cargo::rerun-if-changed=../{script}");
    Command::new(script)
        .current_dir("..")
        .output()
        .expect("Generating wrappers failed");
}

fn generate_bindings(config: &impl BindgenConfig, build_dir: &str) {
    let in_file = config.in_file();

    let mut builder = bindgen::builder()
        .rust_target(bindgen::RustTarget::Stable_1_73)
        .header(in_file)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // We're using the `rustified enum` feature because it provides the best
        // ergonomics, and we trust that SpiderMonkey won't provide values that
        // aren't in the enum, so the safety concerns of this feature don't apply.
        .default_enum_style(EnumVariation::Rust { non_exhaustive: false, })
        .derive_partialeq(true)
        .impl_partialeq(true)
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
        .clang_arg("--target=wasm32-wasi")
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

    const RAW_LINES: &'static [&'static str] = &[];

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

        match get_env("DEBUG").as_str() {
            "1" | "true" | "TRUE" => {
                result.extend(& ["-DJS_GC_ZEAL", "-DDEBUG", "-DJS_DEBUG"]);
            }
            _ => {}
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
        // We don't need them and bindgen doesn't like them.
        "JS::HandleVector",
        "JS::MutableHandleVector",
        "JS::Rooted.*Vector",
        "JS::RootedValueArray",
        // Bindgen can't handle the AutoFilename class.
        "JS::DescribeScriptedCaller",
        // Bindgen generates a bad enum for `HashTable_RebuildStatus`.
        // We don't need any of these, so just block them all.
        "mozilla::detail::HashTable_.*",
    ];

    const OPAQUE_TYPES: &'static [&'static str] = &[
        "JS::StackGCVector.*",
        "JS::PersistentRooted.*",
        "JS::detail::CallArgsBase",
        "js::detail::UniqueSelector.*",
        "mozilla::BufferList",
        "mozilla::UniquePtr.*",
        "mozilla::Variant",
        "mozilla::Hash.*",
        "mozilla::detail::Hash.*",
        "RefPtr_Proxy.*",
        "std::.*",
    ];

    const RAW_LINES: &'static [&'static str] = &[
        "pub use root::*;"
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
        "jsapi-rs/src/jsapi/bindings.rs"
    }
}

struct StarlingBindgenConfig;

impl BindgenConfig for StarlingBindgenConfig {
    const ALLOWLIST_ITEMS: &'static [&'static str] = &[
        "api::.*",
        "std::unique_ptr",
        "std::optional",
        "std::string",
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

    const RAW_LINES: &'static [&'static str] = &[
        "pub use root::*;",
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
