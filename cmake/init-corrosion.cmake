# Ensure that corrosion-rs is initialized, so we can use it or depend on variables it sets.
# NOTE: This file must not be called `corrosion.cmake`, because otherwise it'll be called recursively instead of the
# same-named one coming with Corrosion.

set(Rust_CARGO_TARGET wasm32-wasip1)
# Necessary to make cross-compiling to wasm32-wasi work for crates with -sys dependencies.
set(Rust_CARGO_TARGET_LINK_NATIVE_LIBS "")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml" Rust_TOOLCHAIN REGEX "^channel ?=")
string(REGEX MATCH "[0-9.]+" Rust_TOOLCHAIN "${Rust_TOOLCHAIN}")
execute_process(COMMAND rustup toolchain install ${Rust_TOOLCHAIN})
execute_process(COMMAND rustup target add --toolchain ${Rust_TOOLCHAIN} wasm32-wasi)

CPMAddPackage("gh:corrosion-rs/corrosion#be76480232216a64f65e3b1d9794d68cbac6c690")
string(TOLOWER ${Rust_CARGO_HOST_ARCH} HOST_ARCH)
