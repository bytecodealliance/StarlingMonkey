set(WIZER_VERSION v6.0.0 CACHE STRING "Version of wizer to use")

set(WIZER_URL https://github.com/bytecodealliance/wizer/releases/download/${WIZER_VERSION}/wizer-${WIZER_VERSION}-${HOST_ARCH}-${HOST_OS}.tar.xz)
CPMAddPackage(NAME wizer URL ${WIZER_URL} DOWNLOAD_ONLY TRUE)
set(WIZER_DIR ${CPM_PACKAGE_wizer_SOURCE_DIR})
