set(SM_REV 702095ed0ba2316b4f2905bbd597d0b2fa23951e)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SM_BUILD_TYPE debug)
else()
    set(SM_BUILD_TYPE release)
endif()

CPMAddPackage(NAME spidermonkey-${SM_BUILD_TYPE}
        URL https://github.com/bytecodealliance/spidermonkey-wasi-embedding/releases/download/rev_${SM_REV}/spidermonkey-wasm-static-lib_${SM_BUILD_TYPE}.tar.gz
        DOWNLOAD_ONLY YES
)

set(SM_SOURCE_DIR ${CPM_PACKAGE_spidermonkey-${SM_BUILD_TYPE}_SOURCE_DIR} CACHE STRING "Path to spidermonkey ${SM_BUILD_TYPE} build" FORCE)
set(SM_INCLUDE_DIR ${SM_SOURCE_DIR}/include)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp "")
file(GLOB SM_OBJS ${SM_SOURCE_DIR}/lib/*.o)

add_library(spidermonkey STATIC)
target_sources(spidermonkey PRIVATE ${SM_OBJS} ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
set_property(TARGET spidermonkey PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SM_INCLUDE_DIR})
target_link_libraries(spidermonkey PUBLIC ${SM_SOURCE_DIR}/lib/libjs_static.a ${SM_SOURCE_DIR}/lib/libjsrust.a)

add_compile_definitions("MOZ_JS_STREAMS")
