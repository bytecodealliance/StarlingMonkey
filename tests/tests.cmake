enable_testing()

find_program(BASH_PROGRAM bash)
include("wizer")
include("wasmtime")

function(test TEST_NAME)
    get_target_property(RUNTIME_DIR starling.wasm BINARY_DIR)

    add_custom_command(
            OUTPUT test-${TEST_NAME}.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E env "WASM_TOOLS=${WASM_TOOLS_DIR}/wasm-tools" env "WIZER=${WIZER_DIR}/wizer" ${RUNTIME_DIR}/componentize.sh ${CMAKE_SOURCE_DIR}/tests/cases/${TEST_NAME}/${TEST_NAME}.js test-${TEST_NAME}.wasm
            DEPENDS ${ARG_SOURCES} ${RUNTIME_DIR}/componentize.sh starling.wasm
            VERBATIM
    )
    add_custom_target(test-cases DEPENDS test-${TEST_NAME}.wasm)
    add_test(${TEST_NAME} ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/tests/test.sh test-${TEST_NAME}.wasm ${CMAKE_SOURCE_DIR}/tests/cases/${TEST_NAME}/expectation.txt)
endfunction()

test(smoke)

set_property(TEST smoke PROPERTY ENVIRONMENT "WASMTIME=${WASMTIME}")
