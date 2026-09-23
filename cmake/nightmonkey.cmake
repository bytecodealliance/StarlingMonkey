# NightMonkey: an ahead-of-time JS-to-Wasm compiler layered on top of
# SpiderMonkey (https://github.com/bytecodealliance/nightmonkey).
#
# NightMonkey has two halves, both built here from a pinned checkout of its
# repository:
#
# - The runtime (`runtime/*.cpp` in the NightMonkey tree): the helpers that
#   compiled code calls into, the snapshot registration and activation, and the
#   engine's external compiler hook table. It is compiled against the private
#   headers and with the exact flags of the SpiderMonkey build it is linked
#   with, both of which a `--enable-external-compiler-hooks` build exports under
#   dist/include-private, and linked into starling-raw.wasm.
# - The `nightmonkey` host binary: a cargo build of the compiler that
#   transforms a wizened snapshot of the runtime into one with compiled bodies.
#   componentize.sh runs it when `--enable-nightmonkey` is passed.
#
# The compiler and the runtime share layout facts with the engine, so all
# three must come from the same trees: the NightMonkey pin below and the
# SpiderMonkey pin in cmake/spidermonkey.cmake move together. The compiler
# carries a checked-in opcode table per engine version, and the build checks
# the selected one against the engine's Opcodes.h (below).
set(NIGHTMONKEY_TAG 9f9170b0b2faf12541f3f31048ce0264bd7b0b05)
set(NIGHTMONKEY_REPO_URL https://github.com/bytecodealliance/nightmonkey.git)
set(NIGHTMONKEY_ENGINE_VERSION "ff147" CACHE STRING
    "Engine version NightMonkey is built for (compiler/src/opcodes/<version>.rs in its tree)")

include("manage-git-source")

# Everything that links against SpiderMonkey also links against this target;
# it is empty unless NIGHTMONKEY is enabled.
add_library(nightmonkey INTERFACE)

if (NOT NIGHTMONKEY)
    return()
endif()

# Like SM_SOURCE_DIR, this can point at a local checkout for in-tree
# development of NightMonkey itself.
set(NIGHTMONKEY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps/nightmonkey-source" CACHE PATH
    "Path to a local NightMonkey source checkout")
if (NIGHTMONKEY_SOURCE_DIR STREQUAL "${CMAKE_SOURCE_DIR}/deps/nightmonkey-source")
    manage_git_source(
        NAME nightmonkey
        REPO_URL ${NIGHTMONKEY_REPO_URL}
        TAG ${NIGHTMONKEY_TAG}
        SOURCE_DIR ${NIGHTMONKEY_SOURCE_DIR}
    )
elseif (NOT EXISTS "${NIGHTMONKEY_SOURCE_DIR}/runtime/Night.h")
    message(FATAL_ERROR "NIGHTMONKEY_SOURCE_DIR does not contain a NightMonkey checkout: ${NIGHTMONKEY_SOURCE_DIR}")
else()
    message(STATUS "Using NightMonkey source from ${NIGHTMONKEY_SOURCE_DIR}")
endif()

# The SpiderMonkey dist/ directory: `include/`, `include-private/` (with
# js-build-config.json) and `system_wrappers/`, whether built from source or
# unpacked from pre-built artifacts.
set(NIGHTMONKEY_SM_DIST ${SM_LIB_DIR})
set(NIGHTMONKEY_OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR}/nightmonkey-obj)
file(MAKE_DIRECTORY ${NIGHTMONKEY_OBJ_DIR})

# --- the opcode table check --------------------------------------------------
#
# NightMonkey's compiler does not read the engine at build time: its opcode
# table is generated ahead of time and checked in per engine version,
# selected by the cargo feature of the same name. This verifies that the
# selected table is what the SpiderMonkey being built against generates, so
# an engine whose bytecode differs is refused rather than miscompiled. The
# runtime and compiler builds depend on it (and it on the engine build).
find_program(NIGHTMONKEY_PYTHON python3 REQUIRED DOC "python3, for NightMonkey's scripts/gen_opcodes.py")
add_custom_target(nightmonkey_opcodes_check
    COMMAND ${NIGHTMONKEY_PYTHON} ${NIGHTMONKEY_SOURCE_DIR}/scripts/gen_opcodes.py check
        ${NIGHTMONKEY_ENGINE_VERSION} ${NIGHTMONKEY_SM_DIST}/include-private/vm/Opcodes.h
    COMMENT "Checking NightMonkey's ${NIGHTMONKEY_ENGINE_VERSION} opcode table against SpiderMonkey's Opcodes.h"
    VERBATIM
)
if (TARGET spidermonkey_build)
    add_dependencies(nightmonkey_opcodes_check spidermonkey_build)
endif()

# --- the runtime -------------------------------------------------------------
#
# The compile flags come from js-build-config.json, which only exists once
# SpiderMonkey has been built, so each source is compiled by a script that
# reads them at build time (cmake/night-runtime-compile.cmake). The engine
# build is ordered first through a target-level dependency (a file-level one
# on the engine library would make the Makefile generator run the engine build
# once per consuming target); the headers each object actually depends on,
# including the engine's, are tracked through the compiler's depfile.
file(GLOB NIGHT_RUNTIME_SOURCES ${NIGHTMONKEY_SOURCE_DIR}/runtime/*.cpp)
# The in-process compilation lane (the jit-test harness) is not part of the
# snapshot flow.
list(FILTER NIGHT_RUNTIME_SOURCES EXCLUDE REGEX "/NightInproc[A-Za-z]*\\.cpp$")
set(NIGHT_RUNTIME_COMPILE_SCRIPT ${CMAKE_SOURCE_DIR}/cmake/night-runtime-compile.cmake)

set(NIGHT_RUNTIME_OBJS)
foreach(source ${NIGHT_RUNTIME_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(object ${NIGHTMONKEY_OBJ_DIR}/${name}.o)
    add_custom_command(
        OUTPUT ${object}
        COMMAND ${CMAKE_COMMAND}
            -DCXX=${CMAKE_CXX_COMPILER}
            -DSPIDERMONKEY_DIST=${NIGHTMONKEY_SM_DIST}
            -DNIGHTMONKEY_SOURCE_DIR=${NIGHTMONKEY_SOURCE_DIR}
            -DSOURCE=${source}
            -DOBJECT=${object}
            -DDEPFILE=${object}.d
            -P ${NIGHT_RUNTIME_COMPILE_SCRIPT}
        DEPENDS ${source} ${NIGHT_RUNTIME_COMPILE_SCRIPT}
        DEPFILE ${object}.d
        COMMENT "Compiling NightMonkey runtime: ${name}.cpp"
        VERBATIM
    )
    list(APPEND NIGHT_RUNTIME_OBJS ${object})
endforeach()

set(NIGHT_RUNTIME_LIB ${NIGHTMONKEY_OBJ_DIR}/libnight_runtime.a)
add_custom_command(
    OUTPUT ${NIGHT_RUNTIME_LIB}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${NIGHT_RUNTIME_LIB}
    COMMAND ${CMAKE_AR} qcs ${NIGHT_RUNTIME_LIB} ${NIGHT_RUNTIME_OBJS}
    DEPENDS ${NIGHT_RUNTIME_OBJS}
    COMMENT "Creating NightMonkey runtime library"
    VERBATIM
)
add_custom_target(night_runtime_build DEPENDS ${NIGHT_RUNTIME_LIB})
add_dependencies(night_runtime_build nightmonkey_opcodes_check)

# --- the compiler ------------------------------------------------------------
find_program(CARGO_BIN cargo REQUIRED DOC "cargo, for building the NightMonkey compiler")
set(NIGHTMONKEY_CARGO_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/nightmonkey-cargo)
set(NIGHTMONKEY_BIN "${NIGHTMONKEY_CARGO_TARGET_DIR}/release/nightmonkey" CACHE FILEPATH
    "Path to the NightMonkey compiler" FORCE)
add_custom_target(nightmonkey_compiler
    # Self-contained apart from the engine version, which selects the
    # checked-in opcode table.
    COMMAND ${CARGO_BIN} build --release -p nightmonkey
        --no-default-features --features ${NIGHTMONKEY_ENGINE_VERSION}
        --manifest-path ${NIGHTMONKEY_SOURCE_DIR}/Cargo.toml
        --target-dir ${NIGHTMONKEY_CARGO_TARGET_DIR}
    BYPRODUCTS ${NIGHTMONKEY_BIN}
    # Run from the StarlingMonkey tree so rustup picks up its rust-toolchain.toml.
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building the NightMonkey compiler"
    VERBATIM
)
add_dependencies(nightmonkey_compiler nightmonkey_opcodes_check)

# --- the interface target ----------------------------------------------------
add_dependencies(nightmonkey night_runtime_build nightmonkey_compiler)
# `runtime/Night.h` etc. are included from the NightMonkey tree, and gate their
# declarations on ENABLE_JS_NIGHTMONKEY.
target_include_directories(nightmonkey INTERFACE ${NIGHTMONKEY_SOURCE_DIR})
target_compile_definitions(nightmonkey INTERFACE ENABLE_JS_NIGHTMONKEY=1)
# The whole runtime is linked in: the compiled bodies the snapshot transform
# appends call its exported helpers by name, so nothing in it may be dropped as
# unreferenced.
target_link_libraries(nightmonkey INTERFACE
    -Wl,--whole-archive ${NIGHT_RUNTIME_LIB} -Wl,--no-whole-archive)
