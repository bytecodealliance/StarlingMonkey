enable_testing()

find_program(BASH_PROGRAM bash)
include("wizer")
include("wasmtime")

function(test TEST_NAME)
    get_target_property(RUNTIME_DIR starling.wasm BINARY_DIR)
    add_test(${TEST_NAME} ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/tests/test.sh ${RUNTIME_DIR} ${TEST_NAME})
    set_property(TEST ${TEST_NAME} PROPERTY ENVIRONMENT "WASMTIME=${WASMTIME};WIZER=${WIZER_DIR}/wizer;WASM_TOOLS=${WASM_TOOLS_DIR}/wasm-tools")
endfunction()

test(smoke)
test(tla)
test(syntax-err)
test(tla-err)
test(tla-runtime-resolve)
