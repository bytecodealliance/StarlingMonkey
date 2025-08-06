# Ensure that corrosion-rs is initialized, so we can use it or depend on variables it sets.
# NOTE: This file must not be called `corrosion.cmake`, because otherwise it'll be called recursively instead of the
# same-named one coming with Corrosion.

# Necessary to make cross-compiling to wasm32-wasi work for crates with -sys dependencies.
set(Rust_CARGO_TARGET_LINK_NATIVE_LIBS "")

# Parse Rust version and target from rust-toolchain.toml.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml" RUST_TOOLCHAIN_FILE)
if (NOT ${RUST_TOOLCHAIN_FILE} MATCHES [[channel *= *"([^\"]+)\"]])
    message(FATAL_ERROR "Couldn't parse Rust toolchain version from file ${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml")
endif()
set(Rust_TOOLCHAIN ${CMAKE_MATCH_1})

if (NOT ${RUST_TOOLCHAIN_FILE} MATCHES [[targets = *\[ *"([^\"]+)]])
    message(FATAL_ERROR "Couldn't parse Rust target from file ${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml")
endif()
set(Rust_CARGO_TARGET ${CMAKE_MATCH_1})

if (NOT Rust_VERSION VERSION_EQUAL ${Rust_TOOLCHAIN})
    execute_process(COMMAND rustup toolchain install ${Rust_TOOLCHAIN})
endif()
execute_process(COMMAND rustup target add --toolchain ${Rust_TOOLCHAIN} ${Rust_CARGO_TARGET})

CPMAddPackage("gh:corrosion-rs/corrosion@0.5.1")
string(TOLOWER ${Rust_CARGO_HOST_ARCH} HOST_ARCH)
