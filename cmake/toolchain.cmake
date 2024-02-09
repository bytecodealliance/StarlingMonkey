string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} HOST_OS)

if (${HOST_OS} STREQUAL "darwin")
    set(HOST_OS "macos")
endif()

include("compile-flags")
include("wasi-sdk")
