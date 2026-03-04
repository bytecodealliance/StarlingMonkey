cmake_minimum_required(VERSION 3.27)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

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
    set(HOST_API ${CMAKE_CURRENT_SOURCE_DIR}/host-apis/wasi-0.2.10)
endif()
message(STATUS "Using host API: ${HOST_API}")

# Ensure that the CPM cache is created outside the build dir, even if no location is specified by the developer.
if(NOT DEFINED ENV{CPM_SOURCE_CACHE})
    set(ENV{CPM_SOURCE_CACHE} ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpm_cache)
endif()
include("CPM")
include("toolchain")

project(StarlingMonkey)

include("init-corrosion")

include("wasm-tools")
include("binaryen")
include("wizer")
include("weval")
include("wasmtime")
include("cbindgen")

include("spidermonkey")
include("openssl")
include("${HOST_API}/host_api.cmake")
include("build-crates")
include("lint")

option(ENABLE_JS_DEBUGGER "Enable support for debugging content via a socket connection" ON)

add_library(extension_api INTERFACE include/extension-api.h runtime/encode.h runtime/decode.h)
if (ENABLE_JS_DEBUGGER)
    target_compile_definitions(extension_api INTERFACE ENABLE_JS_DEBUGGER)
endif()
target_link_libraries(extension_api INTERFACE rust-url spidermonkey)
target_include_directories(extension_api INTERFACE include deps/include runtime)

include("builtins")

include("tests/wpt-harness/wpt.cmake")

set(SOURCES
    runtime/js.cpp
    runtime/allocator.cpp
    runtime/encode.cpp
    runtime/decode.cpp
    runtime/engine.cpp
    runtime/event_loop.cpp
    runtime/builtin.cpp
    runtime/script_loader.cpp
    runtime/debugger.cpp
)

add_executable(starling-raw.wasm ${SOURCES})

target_link_libraries(starling-raw.wasm PRIVATE host_api extension_api builtins spidermonkey rust-crates)

option(USE_WASM_OPT "use wasm-opt to optimize the StarlingMonkey binary" ON)

# For release builds, use wasm-opt to optimize the generated wasm file.
if (USE_WASM_OPT)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(
                TARGET starling-raw.wasm
                POST_BUILD
                COMMAND ${WASM_OPT} --strip-debug -O3 -o starling-raw.wasm starling-raw.wasm
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_custom_command(
                TARGET starling-raw.wasm
                POST_BUILD
                COMMAND ${WASM_OPT} -O3 -g -o starling-raw.wasm starling-raw.wasm
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(
            TARGET starling-raw.wasm
            POST_BUILD
            COMMAND ${WASM_OPT} --strip-debug -O0 -o starling-raw.wasm starling-raw.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endif()

# build a compilation cache of ICs
if(WEVAL)
    include("weval")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.js "function main() {}")
    add_custom_target(
        starling-ics.wevalcache
        ALL
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND rm -f starling-ics.wevalcache
        COMMAND echo ./null.js | ${WEVAL_BIN} weval --dir . --show-stats --cache starling-ics.wevalcache -w -i starling-raw.wasm -o /dev/null
        DEPENDS starling-raw.wasm
        VERBATIM
    )
    set(WEVAL_CACHE_FILE "starling-ics.wevalcache")
    set(AOT 1)
else()
    set(AOT 0)
endif()

set(RUNTIME_FILE "starling-raw.wasm")
set(ADAPTER_FILE "preview1-adapter.wasm")
configure_file("componentize.sh.in" "${CMAKE_CURRENT_BINARY_DIR}/componentize.sh" @ONLY)
if(EXISTS ${ADAPTER})
    configure_file(${ADAPTER} "${CMAKE_CURRENT_BINARY_DIR}/${ADAPTER_FILE}" COPYONLY)
endif()

function(componentize OUTPUT)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    list(TRANSFORM ARG_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    list(JOIN ARG_SOURCES " " SOURCES)
    get_target_property(RUNTIME_DIR starling-raw.wasm BINARY_DIR)

    add_custom_command(
            OUTPUT ${OUTPUT}.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${RUNTIME_DIR}/componentize.sh ${SOURCES} -o ${OUTPUT}.wasm
            DEPENDS ${ARG_SOURCES} ${RUNTIME_DIR}/componentize.sh starling-raw.wasm
            VERBATIM
    )
    add_custom_target(${OUTPUT} DEPENDS ${OUTPUT}.wasm)
endfunction()

componentize(smoke-test SOURCES tests/cases/smoke/smoke.js)
componentize(starling)

include("tests/tests.cmake")
