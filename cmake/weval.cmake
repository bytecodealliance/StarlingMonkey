set(WEVAL_VERSION v0.3.4)

set(WEVAL_URL https://github.com/bytecodealliance/weval/releases/download/${WEVAL_VERSION}/weval-${WEVAL_VERSION}-${HOST_ARCH}-${HOST_OS}.tar.xz)
CPMAddPackage(NAME weval URL ${WEVAL_URL} DOWNLOAD_ONLY TRUE)
set(WEVAL_DIR ${CPM_PACKAGE_weval_SOURCE_DIR})
set(WEVAL_BIN ${WEVAL_DIR}/weval CACHE FILEPATH "Path to weval binary")
