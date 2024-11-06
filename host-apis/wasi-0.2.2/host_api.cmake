add_library(host_api STATIC
        ${HOST_API}/host_api.cpp
        ${HOST_API}/host_call.cpp
        ${HOST_API}/bindings/bindings.c
        ${HOST_API}/bindings/bindings_component_type.o
        ${CMAKE_CURRENT_SOURCE_DIR}/include/host_api.h
)

target_link_libraries(host_api PRIVATE spidermonkey)
target_include_directories(host_api PRIVATE include)
target_include_directories(host_api PRIVATE ${HOST_API})
target_include_directories(host_api PUBLIC ${HOST_API}/include)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ADAPTER "debug")
else()
    set(ADAPTER "release")
endif()
set(ADAPTER "${HOST_API}/preview1-adapter-${ADAPTER}/wasi_snapshot_preview1.wasm")
