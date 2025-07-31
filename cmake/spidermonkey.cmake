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

    add_library(spidermonkey INTERFACE)
    target_include_directories(spidermonkey INTERFACE ${SM_INCLUDE_DIR})
    target_link_libraries(spidermonkey INTERFACE ${SM_LIB_DIR}/libspidermonkey.a)
else()
    CPMAddPackage(NAME gecko-source
            GIT_REPOSITORY "https://github.com/bytecodealliance/firefox.git"
            GIT_TAG "${SM_TAG}"
            DOWNLOAD_ONLY YES
    )
    set(SM_SOURCE_DIR ${CPM_PACKAGE_gecko-source_SOURCE_DIR})
    set(SM_OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR}/spidermonkey)
    set(SM_LIB_DIR "${SM_OBJ_DIR}/dist")
    set(SM_INCLUDE_DIR "${SM_LIB_DIR}/include")

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

    # Set up compiler environment
    find_program(SM_HOST_CC clang c REQUIRED DOCS "C compiler for building SpiderMonkey")
    find_program(SM_HOST_CXX clang++ c++ REQUIRED DOCS "C++ compiler for building")

    set(MOZCONFIG "${CMAKE_CURRENT_BINARY_DIR}/mozconfig-${SM_BUILD_TYPE}")
    set(MOZCONFIG_CONTENT "ac_add_options --enable-project=js
ac_add_options --disable-js-shell
ac_add_options --target=wasm32-unknown-wasi
ac_add_options --without-system-zlib
ac_add_options --without-intl-api
ac_add_options --disable-jit
ac_add_options --disable-shared-js
ac_add_options --disable-shared-memory
ac_add_options --disable-tests
ac_add_options --disable-clang-plugin
ac_add_options --enable-jitspew
ac_add_options --enable-optimize=-O3
ac_add_options --enable-js-streams
ac_add_options --enable-portable-baseline-interp
ac_add_options --prefix=${SM_OBJ_DIR}/dist
mk_add_options MOZ_OBJDIR=${SM_OBJ_DIR}
mk_add_options AUTOCLOBBER=1
")

    # Add WASI sysroot if available
    if(DEFINED ENV{WASI_SYSROOT})
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --with-sysroot=\"$ENV{WASI_SYSROOT}\"\n")
    endif()

    # Platform-specific configuration
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --disable-stdcxx-compat\n")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --host=aarch64-apple-darwin\n")
    else()
        message(FATAL_ERROR "Unsupported build platform: ${CMAKE_HOST_SYSTEM_NAME}")
    endif()

    # Mode-specific configuration
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --enable-debug\n")
    else()
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --disable-debug\n")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --enable-lto=thin\n")
    endif()

    # Weval-specific configuration
    if(WEVAL)
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --enable-portable-baseline-interp-force\n")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --enable-aot-ics\n")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --enable-aot-ics-force\n")
        string(APPEND MOZCONFIG_CONTENT "ac_add_options --enable-pbl-weval\n")
    endif()

    file(GENERATE OUTPUT ${MOZCONFIG} CONTENT "${MOZCONFIG_CONTENT}")

    add_custom_command(
        OUTPUT ${SM_OBJS} ${SM_LIB_DIR}/libjs_static.a
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E env
            CC=${CMAKE_C_COMPILER}
            CXX=${CMAKE_CXX_COMPILER}
            AR=${CMAKE_AR}
            HOST_CC=${SM_HOST_CC}
            HOST_CXX=${SM_HOST_CXX}
            MOZCONFIG=${MOZCONFIG}
            SM_SOURCE_DIR=${SM_SOURCE_DIR}
            SM_OBJ_DIR=${SM_OBJ_DIR}
            python3 ${SM_SOURCE_DIR}/mach --no-interactive build
        COMMAND ${CMAKE_COMMAND} -E rm -f ${SM_INCLUDE_DIR}/js-confdefs.h
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${SM_OBJ_DIR}/js/src/js-confdefs.h ${SM_INCLUDE_DIR}/js-confdefs.h
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${SM_OBJ_DIR}/js/src/build/libjs_static.a ${SM_LIB_DIR}/libjs_static.a
        DEPENDS ${MOZCONFIG}
        COMMENT "Building SpiderMonkey for WASI"
        VERBATIM
    )

    # Create combined static library including everything needed for embedding SpiderMonkey.
    set(LIB_SM ${SM_LIB_DIR}/libspidermonkey.a)
    add_custom_command(
        OUTPUT ${LIB_SM}
        COMMAND ${CMAKE_COMMAND} -E copy ${SM_LIB_DIR}/libjs_static.a ${LIB_SM}
        COMMAND ${CMAKE_AR} -q ${LIB_SM} ${SM_OBJS}
        DEPENDS ${SM_OBJS} ${SM_LIB_DIR}/libjs_static.a
        COMMENT "Creating combined SpiderMonkey library"
        VERBATIM
    )
    add_custom_target(spidermonkey_build DEPENDS ${LIB_SM})

    add_library(spidermonkey INTERFACE)
    add_dependencies(spidermonkey spidermonkey_build)
    target_include_directories(spidermonkey INTERFACE ${SM_INCLUDE_DIR})
    target_link_libraries(spidermonkey INTERFACE ${LIB_SM})
endif()

# SpiderMonkey's builds include a header that defines some configuration options that need to be set
# to ensure e.g. object layout is identical to the one used in the build.
# We include this header in all compilations.
## (And because that file doesn't exist until the SpiderMonkey build is complete, we create a placeholder for now.)
if (NOT EXISTS ${SM_INCLUDE_DIR}/js-confdefs.h)
    file(WRITE ${SM_INCLUDE_DIR}/js-confdefs.h "// Placeholder\n")
endif()
target_compile_options(spidermonkey INTERFACE -include ${SM_INCLUDE_DIR}/js-confdefs.h)
