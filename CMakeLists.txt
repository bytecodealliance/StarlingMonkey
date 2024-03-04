cmake_minimum_required(VERSION 3.27)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (DEFINED ENV{HOST_API})
    set(HOST_API $ENV{HOST_API})
    if (EXISTS ${HOST_API})
        cmake_path(ABSOLUTE_PATH HOST_API)
    else ()
        set(HOST_API ${CMAKE_CURRENT_SOURCE_DIR}/host-apis/$ENV{HOST_API})
    endif()
    if (NOT EXISTS ${HOST_API})
        message(FATAL_ERROR "Host API `$ENV{HOST_API}` not found. The HOST_API environment \
            variable must be the name of a host API implementation provided in the `host-apis` \
            folder of js.wasm, or be an absolute path pointing to custom host API implementation.")
    endif()
else()
    set(HOST_API ${CMAKE_CURRENT_SOURCE_DIR}/host-apis/wasi-0.2.0)
endif()
message(STATUS "Using host API: ${HOST_API}")

set(CPM_SOURCE_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpm_cache)
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)
include("CPM")
include("toolchain")

project(js_wasm)

include("init-corrosion")

include("wasm-tools")
include("wizer")
include("wasmtime")

include("fmt")
include("spidermonkey")
include("openssl")
include("host_api")
include("build-crates")

add_library(extension_api INTERFACE include/extension-api.h runtime/encode.h)
target_link_libraries(extension_api INTERFACE rust-url rust-encoding spidermonkey)
target_include_directories(extension_api INTERFACE include deps/include runtime)

include("builtins")

if (DEFINED ENV{WPT})
    include("tests/wpt-harness/wpt.cmake")
endif()

add_executable(js.wasm
        runtime/js.cpp
        runtime/allocator.cpp
        runtime/encode.cpp
        runtime/engine.cpp
        runtime/event_loop.cpp
        runtime/builtin.cpp)


# TODO: remove this once the runtime doesn't need to explicitly include the builtins anymore.
target_include_directories(js.wasm PRIVATE cpp)

target_link_libraries(js.wasm PRIVATE host_api extension_api builtins spidermonkey rust-url)

set(RUNTIME_FILE "js.wasm")
set(ADAPTER_FILE "preview1-adapter.wasm")
configure_file("componentize.sh" "${CMAKE_CURRENT_BINARY_DIR}/componentize.sh")
configure_file(${ADAPTER} "${CMAKE_CURRENT_BINARY_DIR}/${ADAPTER_FILE}" COPYONLY)
configure_file(spin.toml spin.toml COPYONLY)

add_custom_command(
        OUTPUT smoke.js.wasm
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${WASM_TOOLS_DIR};${WIZER_DIR};$ENV{PATH}" ./componentize.sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.js
        DEPENDS js.wasm componentize.sh spin.toml tests/smoke.js
        VERBATIM
)

add_custom_target(smoke-test DEPENDS smoke.js.wasm)
