string(TOLOWER ${CMAKE_BUILD_TYPE} RUST_BUILD_TYPE)

set(RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/wasm32-wasi/${RUST_BUILD_TYPE}")
set(CARGO_FLAGS --workspace --target=wasm32-wasi -Zbuild-std=panic_abort,std -Zunstable-options --out-dir ${RUST_TARGET_DIR})
if (RUST_BUILD_TYPE STREQUAL "Release")
    set(CARGO_FLAGS ${CARGO_FLAGS} --release)
endif()
set(RUST_FLAGS RUSTFLAGS=-Crelocation-model=pic)

add_custom_target(build-cargo-workspace
    COMMAND ${RUST_FLAGS} cargo +${Rust_TOOLCHAIN} build --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml ${CARGO_FLAGS})

add_library(rust-encoding SHARED ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
set_property(TARGET rust-encoding PROPERTY LINK_FLAGS "-Wl,--whole-archive ${RUST_TARGET_DIR}/librust_encoding.a -Wl,--no-whole-archive")
add_dependencies(rust-encoding build-cargo-workspace)
target_include_directories(rust-encoding INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-encoding/)

add_library(rust-url SHARED ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
set_property(TARGET rust-url PROPERTY LINK_FLAGS "-Wl,--whole-archive ${RUST_TARGET_DIR}/librust_url.a -Wl,--no-whole-archive")
add_dependencies(rust-url build-cargo-workspace)
target_include_directories(rust-url INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-url/)
