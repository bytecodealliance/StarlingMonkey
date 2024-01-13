if (DEFINED ENV{WIZER_DIR})
    set(WIZER_DIR $ENV{WIZER_DIR})
    return()
endif ()

set(WIZER_VERSION v3.0.1 CACHE STRING "Version of wizer to use")

set(WIZER_URL https://github.com/bytecodealliance/wizer/releases/download/${WIZER_VERSION}/wizer-${WIZER_VERSION}-${HOST_ARCH}-${HOST_OS}.tar.xz)
CPMAddPackage(NAME wizer URL ${WIZER_URL} DOWNLOAD_ONLY TRUE)
set(WIZER_DIR ${CPM_PACKAGE_wizer_SOURCE_DIR})
