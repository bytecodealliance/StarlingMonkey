set(SM_REV b30ec577023a5c62a779f7b9a9a1f14bd3b38cdd)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SM_BUILD_TYPE debug)
else()
    set(SM_BUILD_TYPE release)
endif()
set(SM_BUILD_TYPE_DASH ${SM_BUILD_TYPE})

option(WEVAL "Build with a SpiderMonkey variant that supports weval-based AOT compilation" OFF)

if (WEVAL)
    set(SM_BUILD_TYPE_DASH "${SM_BUILD_TYPE}-weval")
    set(SM_BUILD_TYPE "${SM_BUILD_TYPE}_weval")
endif()

# If the developer has specified an alternate local set of SpiderMonkey
# artifacts, use them. This allows for local/in-tree development without
# requiring a roundtrip through GitHub CI.
#
# This can be set, for example, to the output directly (`release/` or `debug/`)
# under a local clone of the `spidermonkey-wasi-embedding` repo.
if (DEFINED ENV{SPIDERMONKEY_BINARIES})
    set(SM_SOURCE_DIR $ENV{SPIDERMONKEY_BINARIES})
else()
    CPMAddPackage(NAME spidermonkey-${SM_BUILD_TYPE}
            URL https://github.com/andreiltd/spidermonkey-wasi-embedding/releases/download/rev_${SM_REV}/spidermonkey-wasm-static-lib_${SM_BUILD_TYPE}.tar.gz
            DOWNLOAD_ONLY YES
    )

    set(SM_SOURCE_DIR ${CPM_PACKAGE_spidermonkey-${SM_BUILD_TYPE}_SOURCE_DIR} CACHE STRING "Path to spidermonkey ${SM_BUILD_TYPE} build" FORCE)
endif()

set(SM_INCLUDE_DIR ${SM_SOURCE_DIR}/include)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp "")
file(GLOB SM_OBJS ${SM_SOURCE_DIR}/lib/*.o)

add_library(spidermonkey STATIC)
target_sources(spidermonkey PRIVATE ${SM_OBJS} ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_include_directories(spidermonkey PUBLIC ${SM_INCLUDE_DIR})
target_link_libraries(spidermonkey PUBLIC ${SM_SOURCE_DIR}/lib/libjs_static.a)

add_compile_definitions("MOZ_JS_STREAMS")
