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
            folder of StarlingMonkey, or be an absolute path pointing to custom host API implementation.")
    endif()
else()
    set(HOST_API ${CMAKE_CURRENT_SOURCE_DIR}/host-apis/wasi-0.2.0)
endif()
message(STATUS "Using host API: ${HOST_API}")

include("CPM")
include("toolchain")

project(StarlingMonkey)

include("init-corrosion")

include("wasm-tools")
include("binaryen")
include("wizer")
include("weval")
include("wasmtime")

include("fmt")
include("spidermonkey")
include("openssl")
include("host_api")
include("build-crates")

add_library(extension_api INTERFACE include/extension-api.h runtime/encode.h runtime/decode.h)
target_link_libraries(extension_api INTERFACE rust-url spidermonkey)
target_include_directories(extension_api INTERFACE include deps/include runtime)

include("builtins")

option(ENABLE_WPT "Enable WPT harness support" OFF)
if (ENABLE_WPT)
    include("tests/wpt-harness/wpt.cmake")
endif()

add_executable(starling.wasm
        runtime/js.cpp
        runtime/allocator.cpp
        runtime/encode.cpp
        runtime/decode.cpp
        runtime/engine.cpp
        runtime/event_loop.cpp
        runtime/builtin.cpp
        runtime/script_loader.cpp
)

option(USE_WASM_OPT "use wasm-opt to optimize the StarlingMonkey binary" ON)

# For release builds, use wasm-opt to optimize the generated wasm file.
if (USE_WASM_OPT)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(
                TARGET starling.wasm
                POST_BUILD
                COMMAND ${WASM_OPT} --strip-debug -O3 -o starling.wasm starling.wasm
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_custom_command(
                TARGET starling.wasm
                POST_BUILD
                COMMAND ${WASM_OPT} -O3 -g -o starling.wasm starling.wasm
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(
            TARGET starling.wasm
            POST_BUILD
            COMMAND ${WASM_OPT} --strip-debug -O0 -o starling.wasm starling.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endif()

target_link_libraries(starling.wasm PRIVATE host_api extension_api builtins spidermonkey rust-url)

# build a compilation cache of ICs
if(WEVAL)
    include("weval")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.js "function main() {}")
    add_custom_target(
        starling-ics.wevalcache
        ALL
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND rm -f starling-ics.wevalcache
        COMMAND echo ./null.js | ${WEVAL_BIN} weval --dir . --show-stats --cache starling-ics.wevalcache -w -i starling.wasm -o /dev/null
        DEPENDS starling.wasm
        VERBATIM
    )
    set(WEVAL_CACHE_FILE "starling-ics.wevalcache")
    set(AOT 1)
else()
    set(AOT 0)
endif()

set(RUNTIME_FILE "starling.wasm")
set(ADAPTER_FILE "preview1-adapter.wasm")
configure_file("componentize.sh" "${CMAKE_CURRENT_BINARY_DIR}/componentize.sh" @ONLY)
configure_file(${ADAPTER} "${CMAKE_CURRENT_BINARY_DIR}/${ADAPTER_FILE}" COPYONLY)
configure_file(spin.toml spin.toml COPYONLY)

function(componentize OUTPUT)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    list(TRANSFORM ARG_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    list(JOIN ARG_SOURCES " " SOURCES)
    get_target_property(RUNTIME_DIR starling.wasm BINARY_DIR)

    add_custom_command(
            OUTPUT ${OUTPUT}.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} ${RUNTIME_DIR}/componentize.sh ${SOURCES} -o ${OUTPUT}.wasm
            DEPENDS ${ARG_SOURCES} ${RUNTIME_DIR}/componentize.sh starling.wasm
            VERBATIM
    )
    add_custom_target(${OUTPUT} DEPENDS ${OUTPUT}.wasm)
endfunction()

componentize(smoke-test SOURCES tests/cases/smoke/smoke.js)
componentize(runtime-eval)

include("tests/tests.cmake")
