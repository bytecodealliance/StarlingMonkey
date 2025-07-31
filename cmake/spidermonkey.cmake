set(SM_TAG FIREFOX_140_0_4_RELEASE_STARLING)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SM_BUILD_TYPE debug)
else()
    set(SM_BUILD_TYPE release)
endif()

option(WEVAL "Build with a SpiderMonkey variant that supports weval-based AOT compilation" OFF)

if (WEVAL)
    set(SM_BUILD_TYPE "${SM_BUILD_TYPE}_weval")
endif()

# If the developer has specified an alternate local set of SpiderMonkey
# artifacts, use them. This allows for local/in-tree development without
# requiring a roundtrip through GitHub CI.
#
# This can be set, for example, to the output directly (`release/` or `debug/`)
# under a local clone of the `spidermonkey-wasi-embedding` repo.
if (DEFINED ENV{SPIDERMONKEY_BINARIES})
    set(SM_LIB_DIR $ENV{SPIDERMONKEY_BINARIES})
    message(STATUS "Using pre-built SpiderMonkey artifacts from local directory ${SM_LIB_DIR}")
else()
    set(SM_URL https://github.com/bytecodealliance/starlingmonkey/releases/download/libspidermonkey_${SM_TAG}/spidermonkey-static-${SM_BUILD_TYPE}.tar.gz)
    execute_process(
            COMMAND curl -s -o /dev/null -w "%{http_code}" ${SM_URL}
            RESULT_VARIABLE CURL_RESULT
            OUTPUT_VARIABLE HTTP_STATUS
    )

    if (CURL_RESULT EQUAL 0 AND HTTP_STATUS STREQUAL "200")
        message(STATUS "Using pre-built SpiderMonkey artifacts from ${SM_URL}")
        CPMAddPackage(NAME spidermonkey-${SM_BUILD_TYPE}
                URL ${SM_URL}
                DOWNLOAD_ONLY YES
        )
        set(SM_LIB_DIR ${CPM_PACKAGE_spidermonkey-${SM_BUILD_TYPE}_SOURCE_DIR} CACHE STRING "Path to spidermonkey ${SM_BUILD_TYPE} build" FORCE)
    else()
        message(STATUS "No pre-built ${SM_BUILD_TYPE} SpiderMonkey artifacts available for tag ${SM_TAG}. Building from source.")
    endif()
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp "")

if (DEFINED SM_LIB_DIR)
    set(SM_INCLUDE_DIR ${SM_LIB_DIR}/include)

    file(GLOB SM_OBJS ${SM_LIB_DIR}/lib/*.o)

    add_library(spidermonkey STATIC)
    target_sources(spidermonkey PRIVATE ${SM_OBJS} ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
    target_include_directories(spidermonkey PUBLIC ${SM_INCLUDE_DIR})
    target_link_libraries(spidermonkey PUBLIC ${SM_LIB_DIR}/lib/libjs_static.a)
else()
    CPMAddPackage(NAME gecko-source
            GIT_REPOSITORY "https://github.com/bytecodealliance/firefox.git"
            GIT_TAG "${SM_TAG}"
            DOWNLOAD_ONLY YES
    )
    set(SM_SOURCE_DIR ${CPM_PACKAGE_gecko-source_SOURCE_DIR})
    set(SM_OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR}/sm-obj-${SM_BUILD_TYPE})
    set(SM_INCLUDE_DIR "${SM_OBJ_DIR}/dist/include")

    # Additional obj files needed, but not part of libjs_static.a
    set(SM_OBJ_FILES
        memory/build/Unified_cpp_memory_build0.o
        memory/mozalloc/Unified_cpp_memory_mozalloc0.o
        mfbt/Unified_cpp_mfbt0.o
        mfbt/Unified_cpp_mfbt1.o
        mozglue/misc/AutoProfilerLabel.o
        mozglue/misc/ConditionVariable_noop.o
        mozglue/misc/Debug.o
        mozglue/misc/Decimal.o
        mozglue/misc/MmapFaultHandler.o
        mozglue/misc/Mutex_noop.o
        mozglue/misc/Now.o
        mozglue/misc/Printf.o
        mozglue/misc/SIMD.o
        mozglue/misc/StackWalk.o
        mozglue/misc/TimeStamp.o
        mozglue/misc/TimeStamp_posix.o
        mozglue/misc/Uptime.o
        mozglue/static/lz4.o
        mozglue/static/lz4frame.o
        mozglue/static/lz4hc.o
        mozglue/static/xxhash.o
        third_party/fmt/Unified_cpp_third_party_fmt0.o
    )
    set(SM_OBJS)
    foreach(obj_file ${SM_OBJ_FILES})
        list(APPEND SM_OBJS ${SM_OBJ_DIR}/${obj_file})
    endforeach()

    add_custom_command(
        OUTPUT ${SM_OBJS} ${SM_OBJ_DIR}/js/src/build/libjs_static.a
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E env WASI_SDK_PATH=${WASI_SDK_PREFIX} SM_SOURCE_DIR=${SM_SOURCE_DIR}
            SM_OBJ_DIR=${SM_OBJ_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-spidermonkey.sh ${SM_BUILD_TYPE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-spidermonkey.sh
        VERBATIM
    )
    add_library(js_static_additional STATIC)
    target_sources(js_static_additional PRIVATE ${SM_OBJS} ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
    add_library(spidermonkey INTERFACE)
    target_include_directories(spidermonkey INTERFACE ${SM_INCLUDE_DIR})
    target_link_libraries(spidermonkey INTERFACE js_static_additional ${SM_OBJ_DIR}/js/src/build/libjs_static.a)
endif()

# SpiderMonkey's builds include a header that defines some configuration options that need to be set
# to ensure e.g. object layout is identical to the one used in the build.
# We include this header in all compilations.
add_compile_options(-include ${SM_INCLUDE_DIR}/js-confdefs.h)
