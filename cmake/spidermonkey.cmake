if (DEFINED ENV{SM_SOURCE_DIR})
    message(STATUS "Using spidermonkey build from $ENV{SM_SOURCE_DIR}")
    set(SM_SOURCE_DIR $ENV{SM_SOURCE_DIR})
else ()

    set(SM_REV 97832280cdaadeba106beeea832dc6632cb9b611)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(SM_BUILD_TYPE debug)
    else()
        set(SM_BUILD_TYPE release)
    endif()

    CPMAddPackage(NAME spidermonkey-${SM_BUILD_TYPE}
            URL https://github.com/tschneidereit/spidermonkey-wasi-embedding/releases/download/rev_${SM_REV}/spidermonkey-wasm-static-lib_${SM_BUILD_TYPE}.tar.gz
            DOWNLOAD_ONLY YES
    )

    set(SM_SOURCE_DIR ${CPM_PACKAGE_spidermonkey-${SM_BUILD_TYPE}_SOURCE_DIR} CACHE STRING "Path to spidermonkey ${SM_BUILD_TYPE} build" FORCE)
endif ()

set(SM_INCLUDE_DIR ${SM_SOURCE_DIR}/include)

file(WRITE ${SM_SOURCE_DIR}/null.cpp "")
file(GLOB SM_OBJS ${SM_SOURCE_DIR}/lib/*.o)

add_library(jsapi INTERFACE)
set_property(TARGET jsapi PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SM_INCLUDE_DIR})

add_library(spidermonkey STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_sources(spidermonkey PRIVATE ${SM_OBJS} ${SM_SOURCE_DIR}/lib/libjs_static.a ${SM_SOURCE_DIR}/lib/libjsrust.a)

add_library(spidermonkey-shared SHARED ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
set_property(TARGET spidermonkey-shared PROPERTY LINK_FLAGS "-Wl,--whole-archive ${CMAKE_CURRENT_BINARY_DIR}/libspidermonkey.a ${SM_SOURCE_DIR}/lib/libjs_static.a ${SM_SOURCE_DIR}/lib/libjsrust.a -Wl,--no-whole-archive -Wl,--strip-all")

add_compile_definitions("MOZ_JS_STREAMS")

file(READ ${SM_SOURCE_DIR}/rust-toolchain.toml toolchain_file)
string(REGEX MATCH "channel = \"([^\"]+)\"" _ ${toolchain_file})
set(Rust_TOOLCHAIN ${CMAKE_MATCH_1})
message(STATUS "Using SpiderMonkey's Rust toolchain ${Rust_TOOLCHAIN}")
