ENABLE_TESTING()

function(componentize OUTPUT)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    list(TRANSFORM ARG_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    list(JOIN ARG_SOURCES " " SOURCES)
    get_target_property(RUNTIME_DIR starling.wasm BINARY_DIR)

    add_custom_command(
            OUTPUT ${OUTPUT}.wasm
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${WASM_TOOLS_DIR};${WIZER_DIR};$ENV{PATH}" ${RUNTIME_DIR}/componentize.sh ${SOURCES} ${OUTPUT}.wasm
            DEPENDS ${ARG_SOURCES} ${RUNTIME_DIR}/componentize.sh starling.wasm
            VERBATIM
    )
    add_custom_target(test-cases DEPENDS ${OUTPUT}.wasm)
endfunction()

find_program (BASH_PROGRAM bash)


componentize(test-smoke SOURCES tests/cases/smoke/smoke.js)
add_test(Smoke ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/tests/test.sh test-smoke.wasm ${CMAKE_SOURCE_DIR}/tests/cases/smoke/expectation.txt)
