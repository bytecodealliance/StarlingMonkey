set(BINARYEN_VERSION 117)

set(BINARYEN_ARCH ${HOST_ARCH})
if(HOST_OS STREQUAL "macos" AND HOST_ARCH STREQUAL "aarch64")
    set(BINARYEN_ARCH "arm64")
endif()

set(BINARYEN_URL https://github.com/WebAssembly/binaryen/releases/download/version_${BINARYEN_VERSION}/binaryen-version_${BINARYEN_VERSION}-${BINARYEN_ARCH}-${HOST_OS}.tar.gz)
CPMAddPackage(NAME binaryen URL ${BINARYEN_URL} DOWNLOAD_ONLY TRUE)
set(BINARYEN_DIR ${CPM_PACKAGE_binaryen_SOURCE_DIR}/bin)
set(WASM_OPT ${BINARYEN_DIR}/wasm-opt)
