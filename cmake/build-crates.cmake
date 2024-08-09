# To avoid conflicts caused by multiple definitions of the same symbols,
# we need to build all Rust crates as part of a single staticlib.
# This is done by synthesizing a Rust crate that depends on all the others,
# and contains `pub use` statements for each of them.
#
# You might ask why we don't just do all of this in Rust itself, without CMake
# involved at all.
# A key reason is that this setup enables us to add Rust crates wherever we'd also
# be able to add C++ libraries, and have them all be built together.
# Notably, this way we can have host-API implementations that include Rust code,
# while keeping the host-API implementation fully self-contained, without changes
# outside its own folder.

set(RUST_STATICLIB_RS "${CMAKE_CURRENT_BINARY_DIR}/rust-staticlib.rs" CACHE INTERNAL "Path to the Rust staticlibs bundler source file" FORCE)
set(RUST_STATICLIB_TOML "${CMAKE_CURRENT_BINARY_DIR}/Cargo.toml" CACHE INTERNAL "Path to the Rust staticlibs bundler Cargo.toml file" FORCE)

configure_file("runtime/crates/staticlib-template/rust-staticlib.rs.in" "${RUST_STATICLIB_RS}" COPYONLY)
configure_file("runtime/crates/staticlib-template/Cargo.toml.in" "${RUST_STATICLIB_TOML}")

corrosion_import_crate(
        MANIFEST_PATH ${RUST_STATICLIB_TOML}
        CRATES "rust-staticlib"
        NO_LINKER_OVERRIDE
)

# Add a Rust library to the staticlib bundle.
function(add_rust_lib name path)
    add_library(${name} INTERFACE)
    target_include_directories(${name} INTERFACE "${path}")
    file(APPEND $CACHE{RUST_STATICLIB_TOML} "${name} = { path = \"${path}\", features = [${ARGN}] }\n")
    string(REPLACE "-" "_" name ${name})
    file(APPEND $CACHE{RUST_STATICLIB_RS} "pub use ${name};\n")
endfunction()

# These two crates are needed by SpiderMonkey:
add_rust_lib(rust-encoding "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-encoding")
add_rust_lib(rust-hooks "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-hooks")

# The rust-hooks crate needs a supporting CPP file
add_library(rust-hooks-wrappers STATIC "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-hooks/src/wrappers.cpp")
target_link_libraries(rust-hooks-wrappers PRIVATE spidermonkey)
add_library(rust-crates STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(rust-crates PRIVATE rust_staticlib rust-hooks-wrappers)

# Add crates as needed here:
add_rust_lib(rust-url "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-url")
add_rust_lib(multipart "${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-multipart" "\"capi\", \"simd\"")
