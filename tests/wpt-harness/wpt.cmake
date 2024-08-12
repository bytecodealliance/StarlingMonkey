enable_testing()

include("wasmtime")
include("weval")

if(WEVAL)
    set(COMPONENTIZE_FLAGS "--aot")
else()
    set(COMPONENTIZE_FLAGS "")
endif()

if(DEFINED ENV{WPT_ROOT})
    set(WPT_ROOT $ENV{WPT_ROOT})
else()
	CPMAddPackage(
	  NAME wpt-suite
	  GITHUB_REPOSITORY web-platform-tests/wpt
	  GIT_TAG bd65bb46410dd6ea3319e3688a5248a0a7d06960
	  DOWNLOAD_ONLY TRUE
	)
	set(WPT_ROOT ${CPM_PACKAGE_wpt-suite_SOURCE_DIR})
endif()

add_builtin(wpt_support
    SRC "${CMAKE_CURRENT_LIST_DIR}/wpt_builtins.cpp"
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/builtins/web/")

add_custom_command(
        OUTPUT wpt-runtime.wasm
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E env PATH=${WASM_TOOLS_DIR}:${WIZER_DIR}:$ENV{PATH} env "COMPONENTIZE_FLAGS=${COMPONENTIZE_FLAGS}" WPT_ROOT=${WPT_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wpt-harness/build-wpt-runtime.sh
        DEPENDS starling.wasm componentize.sh tests/wpt-harness/build-wpt-runtime.sh tests/wpt-harness/pre-harness.js tests/wpt-harness/post-harness.js
        VERBATIM
)

add_custom_target(wpt-runtime DEPENDS wpt-runtime.wasm)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BT_DETAILS "1")
endif ()

add_test(
        NAME wpt
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E env PATH=${WASMTIME_DIR}:$ENV{PATH} WASMTIME_BACKTRACE_DETAILS=${BT_DETAILS} node ${CMAKE_CURRENT_SOURCE_DIR}/tests/wpt-harness/run-wpt.mjs --wpt-root=${WPT_ROOT} -vv
)
