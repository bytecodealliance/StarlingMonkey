set(RUST_STATICLIB_RS "${CMAKE_CURRENT_BINARY_DIR}/rust-staticlib.rs" CACHE INTERNAL "Path to the Rust staticlibs bundler source file" FORCE)
set(RUST_STATICLIB_TOML "${CMAKE_CURRENT_BINARY_DIR}/Cargo.toml" CACHE INTERNAL "Path to the Rust staticlibs bundler Cargo.toml file" FORCE)
configure_file("runtime/crates/staticlib-template/rust-staticlib.rs.in" "${RUST_STATICLIB_RS}" COPYONLY)

# Add the debugmozjs feature for debug builds.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUGMOZJS_FEATURE "\"debugmozjs\"")
endif()
configure_file("runtime/crates/staticlib-template/Cargo.toml.in" "${RUST_STATICLIB_TOML}")

corrosion_import_crate(
        MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/runtime/crates/Cargo.toml
        CRATES "generate-bindings"
)
corrosion_set_env_vars(generate_bindings
        SYSROOT=${WASI_SDK_PREFIX}/share/wasi-sysroot
        CXXFLAGS="${CMAKE_CXX_FLAGS}"
        BIN_DIR=${CMAKE_CURRENT_BINARY_DIR}
        SM_HEADERS=${SM_INCLUDE_DIR}
        RUST_LOG=bindgen
)

corrosion_import_crate(
        MANIFEST_PATH ${RUST_STATICLIB_TOML}
        CRATES "rust-staticlib"
        NO_LINKER_OVERRIDE
)

add_dependencies("cargo-prebuild_rust_staticlib" cargo-build_generate_bindings)

add_library(rust-glue ${CMAKE_CURRENT_SOURCE_DIR}/runtime/crates/jsapi-rs/cpp/jsglue.cpp)
target_include_directories(rust-glue PRIVATE ${SM_INCLUDE_DIR})
add_dependencies(rust_staticlib rust-glue)

function(add_rust_lib name path)
    add_library(${name} INTERFACE)
    file(APPEND $CACHE{RUST_STATICLIB_TOML} "${name} = { path = \"${path}\" }\n")
    string(REPLACE "-" "_" name ${name})
    file(APPEND $CACHE{RUST_STATICLIB_RS} "pub use ${name};\n")
endfunction()

add_rust_lib("rust-url" "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-url")
set_property(TARGET rust-url PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-url")
