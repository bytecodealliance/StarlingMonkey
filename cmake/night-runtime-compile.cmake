# Compiles one NightMonkey runtime source file against SpiderMonkey's private
# headers, with the flags libjs itself was compiled with, so that the object,
# shape, string and script layouts the runtime bakes into its code match the
# engine it is linked with. Both come from dist/include-private, which a
# `--enable-external-compiler-hooks` build of SpiderMonkey exports:
# js-build-config.json records the flags, and the headers mirror js/src.
#
# Run in script mode from cmake/nightmonkey.cmake:
#   cmake -DCXX=<compiler> -DSPIDERMONKEY_DIST=<dist> -DNIGHTMONKEY_SOURCE_DIR=<src>
#         -DSOURCE=<file.cpp> -DOBJECT=<file.o> [-DDEPFILE=<file.d>]
#         -P night-runtime-compile.cmake
#
# This mirrors the flag handling of NightMonkey's own CMakeLists.txt, except
# that the compiler is the one StarlingMonkey builds with (the same one that
# built libjs) rather than the path recorded in the JSON, which does not
# survive being moved to another machine as pre-built artifacts.
cmake_minimum_required(VERSION 3.27)

foreach(var CXX SPIDERMONKEY_DIST NIGHTMONKEY_SOURCE_DIR SOURCE OBJECT)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "night-runtime-compile.cmake: ${var} is required")
    endif()
endforeach()

set(SM_CONFIG_JSON "${SPIDERMONKEY_DIST}/include-private/js-build-config.json")
if (NOT EXISTS "${SM_CONFIG_JSON}")
    message(FATAL_ERROR "${SM_CONFIG_JSON} not found: SpiderMonkey must be built with --enable-external-compiler-hooks")
endif()
file(READ "${SM_CONFIG_JSON}" SM_JSON)

function(sm_json_list out key)
    string(JSON n LENGTH "${SM_JSON}" ${key})
    set(result)
    if (n GREATER 0)
        math(EXPR last "${n} - 1")
        foreach(i RANGE ${last})
            string(JSON v GET "${SM_JSON}" ${key} ${i})
            list(APPEND result "${v}")
        endforeach()
    endif()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

sm_json_list(SM_CXX_BASE_FLAGS cxx_base_flags)
sm_json_list(SM_OS_CXXFLAGS os_cxxflags)
sm_json_list(SM_OPTIMIZE_FLAGS optimize_flags)
sm_json_list(SM_DEBUG_FLAGS debug_flags)
sm_json_list(SM_EXTRA_CXXFLAGS extra_cxxflags)
sm_json_list(SM_WARNINGS_CXXFLAGS warnings_cxxflags)
sm_json_list(SM_DEBUG_DEFINES debug_defines)
sm_json_list(SM_LIBRARY_DEFINES library_defines)
sm_json_list(SM_FORCE_INCLUDES force_includes)
sm_json_list(SM_INCLUDE_DIRS include_dirs)

# The base flags are the target, standard and (if one was configured) the
# sysroot. A sysroot recorded on another machine is dropped in favor of the
# compiler's own.
set(FLAGS)
set(skip_next FALSE)
foreach(flag ${SM_CXX_BASE_FLAGS})
    if (skip_next)
        set(skip_next FALSE)
        if (EXISTS "${flag}")
            list(APPEND FLAGS "--sysroot" "${flag}")
        endif()
    elseif (flag STREQUAL "--sysroot")
        set(skip_next TRUE)
    elseif (flag MATCHES "^--sysroot=(.*)$")
        if (EXISTS "${CMAKE_MATCH_1}")
            list(APPEND FLAGS "${flag}")
        endif()
    else()
        list(APPEND FLAGS "${flag}")
    endif()
endforeach()

list(APPEND FLAGS ${SM_OS_CXXFLAGS} ${SM_OPTIMIZE_FLAGS} ${SM_DEBUG_FLAGS} ${SM_EXTRA_CXXFLAGS})
foreach(define ${SM_DEBUG_DEFINES})
    list(APPEND FLAGS "-D${define}=1")
endforeach()
foreach(define ${SM_LIBRARY_DEFINES})
    list(APPEND FLAGS "-D${define}")
endforeach()
list(APPEND FLAGS "-DENABLE_JS_NIGHTMONKEY=1")
# The configuration headers every engine TU sees.
foreach(header ${SM_FORCE_INCLUDES})
    list(APPEND FLAGS "-include" "${SPIDERMONKEY_DIST}/include-private/${header}")
endforeach()
list(APPEND FLAGS "-I${NIGHTMONKEY_SOURCE_DIR}")
foreach(dir ${SM_INCLUDE_DIRS})
    list(APPEND FLAGS "-I${SPIDERMONKEY_DIST}/${dir}")
endforeach()
# The engine's own warning set (its headers are not clean under a plain -Wall),
# plus what the runtime's sources need.
list(APPEND FLAGS ${SM_WARNINGS_CXXFLAGS} -Wno-invalid-offsetof -Wno-unused-private-field)
if (DEFINED DEPFILE)
    list(APPEND FLAGS -MD -MF "${DEPFILE}")
endif()

execute_process(
    COMMAND ${CXX} ${FLAGS} -c "${SOURCE}" -o "${OBJECT}"
    RESULT_VARIABLE result
)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "Compiling ${SOURCE} failed")
endif()
