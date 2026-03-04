set(WASI_0_2_0 "${HOST_API}/../wasi-0.2.0")
set(WASI_0_2_3 "${HOST_API}/../wasi-0.2.3")

add_library(host_api STATIC
        ${WASI_0_2_0}/host_api.cpp
        ${WASI_0_2_0}/host_call.cpp
        ${WASI_0_2_3}/sockets.cpp
        ${HOST_API}/bindings/bindings.c
        ${HOST_API}/bindings/bindings_component_type.o
)

target_link_libraries(host_api PRIVATE spidermonkey)
target_include_directories(host_api PRIVATE include)
target_include_directories(host_api PRIVATE ${WASI_0_2_0})
target_include_directories(host_api PUBLIC ${WASI_0_2_0}/include)
target_include_directories(host_api PUBLIC ${WASI_0_2_3}/include)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ADAPTER "debug")
else()
    set(ADAPTER "release")
endif()
set(ADAPTER "${WASI_0_2_0}/preview1-adapter-${ADAPTER}/wasi_snapshot_preview1.wasm")
