enable_testing()

find_program(BASH_PROGRAM bash)
include("wizer")
include("wasmtime")
include("weval")

function(test_e2e TEST_NAME)
    get_target_property(RUNTIME_DIR starling-raw.wasm BINARY_DIR)
    add_test(e2e-${TEST_NAME} ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/tests/test.sh ${RUNTIME_DIR} ${CMAKE_SOURCE_DIR}/tests/e2e/${TEST_NAME})
    set_property(TEST e2e-${TEST_NAME} PROPERTY ENVIRONMENT "WASMTIME=${WASMTIME};WIZER=${WIZER_DIR}/wizer;WASM_TOOLS=${WASM_TOOLS_DIR}/wasm-tools")
    set_tests_properties(e2e-${TEST_NAME} PROPERTIES TIMEOUT 120)
endfunction()

function(test_integration TEST_NAME)
    get_target_property(RUNTIME_DIR starling-raw.wasm BINARY_DIR)

    add_test(integration-${TEST_NAME} ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/tests/test.sh ${RUNTIME_DIR} ${CMAKE_SOURCE_DIR}/tests/integration/${TEST_NAME} test-server.wasm ${TEST_NAME})
    set_property(TEST integration-${TEST_NAME} PROPERTY ENVIRONMENT "WASMTIME=${WASMTIME};WIZER=${WIZER_DIR}/wizer;WASM_TOOLS=${WASM_TOOLS_DIR}/wasm-tools;")
    set_tests_properties(integration-${TEST_NAME} PROPERTIES TIMEOUT 120)
endfunction()

function(integration_tests)
    get_target_property(RUNTIME_DIR starling-raw.wasm BINARY_DIR)
    set(TESTS_DIR ${CMAKE_SOURCE_DIR}/tests/integration)
    set(DEPS ${RUNTIME_DIR}/componentize.sh starling-raw.wasm ${TESTS_DIR}/test-server.js ${TESTS_DIR}/handlers.js
    )
    foreach(TEST_NAME ${ARGV})
        list(APPEND DEPS ${TESTS_DIR}/${TEST_NAME}/${TEST_NAME}.js)
    endforeach()

    add_custom_command(
            OUTPUT test-server.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E env "WASM_TOOLS=${WASM_TOOLS_DIR}/wasm-tools" env "WIZER=${WIZER_DIR}/wizer" env "PREOPEN_DIR=${CMAKE_SOURCE_DIR}/tests" ${RUNTIME_DIR}/componentize.sh ${TESTS_DIR}/test-server.js test-server.wasm
            DEPENDS ${DEPS}
            VERBATIM
    )
    add_custom_target(integration-test-server DEPENDS test-server.wasm)

    foreach(TEST_NAME ${ARGV})
        test_integration(${TEST_NAME})
    endforeach()
endfunction()

test_e2e(blob)
test_e2e(eventloop-stall)
test_e2e(headers)
test_e2e(runtime-err)
test_e2e(smoke)
test_e2e(syntax-err)
test_e2e(tla-err)
test_e2e(tla-runtime-resolve)
test_e2e(tla)
test_e2e(stream-forwarding)
test_e2e(multi-stream-forwarding)
test_e2e(teed-stream-as-outgoing-body)
test_e2e(init-script)
test_e2e(no-init-location)
test_e2e(init-location)

integration_tests(
    blob
    btoa
    crypto
    event
    fetch
    performance
    timers
)
