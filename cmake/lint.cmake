set(CLANG_TIDY ${WASI_SDK_PREFIX}/bin/clang-tidy)
set(RUN_CLANG_TIDY ${WASI_SDK_PREFIX}/bin/run-clang-tidy)
set(CLANG_APPLY_REPLACEMENTS ${WASI_SDK_PREFIX}/bin/clang-apply-replacements)
set(WASI_SYSROOT ${WASI_SDK_PREFIX}/share/wasi-sysroot)

include(ProcessorCount)
ProcessorCount(NPROC)
if(NOT NPROC EQUAL 0)
  set(CLANG_TIDY_PARALLEL -j${NPROC})
endif()

add_custom_target(clang-tidy
  COMMAND ${RUN_CLANG_TIDY}
          ${CLANG_TIDY_PARALLEL}
          -p=${CMAKE_BINARY_DIR}
          -clang-tidy-binary=${CLANG_TIDY}
          -extra-arg=--sysroot=${WASI_SYSROOT}
          -extra-arg=-isystem${CMAKE_BINARY_DIR}/spidermonkey-obj/dist/include
          -config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
          ${CMAKE_SOURCE_DIR}/builtins
          ${CMAKE_SOURCE_DIR}/runtime
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running clang‑tidy over the codebase"
  VERBATIM)

add_custom_target(clang-tidy-fix
  COMMAND ${RUN_CLANG_TIDY}
          ${CLANG_TIDY_PARALLEL}
          -p=${CMAKE_BINARY_DIR}
          -clang-tidy-binary=${CLANG_TIDY}
          -clang-apply-replacements-binary=${CLANG_APPLY_REPLACEMENTS}
          -extra-arg=--sysroot=${WASI_SYSROOT}
          -extra-arg=-isystem${CMAKE_BINARY_DIR}/spidermonkey-obj/dist/include
          -config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
          -fix
          ${CMAKE_SOURCE_DIR}/builtins
          ${CMAKE_SOURCE_DIR}/runtime
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running clang‑tidy over the codebase and applying fixes"
  VERBATIM)

add_dependencies(clang-tidy starling-raw.wasm)
add_dependencies(clang-tidy-fix starling-raw.wasm)
