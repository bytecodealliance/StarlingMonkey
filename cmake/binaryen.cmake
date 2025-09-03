set(BINARYEN_VERSION 123)

set(BINARYEN_ARCH ${HOST_ARCH})
if(HOST_OS STREQUAL "macos" AND HOST_ARCH STREQUAL "aarch64")
    set(BINARYEN_ARCH "arm64")
endif()

set(BINARYEN_URL https://github.com/WebAssembly/binaryen/releases/download/version_${BINARYEN_VERSION}/binaryen-version_${BINARYEN_VERSION}-${BINARYEN_ARCH}-${HOST_OS}.tar.gz)
CPMAddPackage(NAME binaryen URL ${BINARYEN_URL} DOWNLOAD_ONLY TRUE)
set(BINARYEN_DIR ${CPM_PACKAGE_binaryen_SOURCE_DIR}/bin)
set(WASM_OPT ${BINARYEN_DIR}/wasm-opt CACHE FILEPATH "Path to wasm-opt binary")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/cxx-wrapper.in ${CMAKE_BINARY_DIR}/cxx-wrapper)
set(CMAKE_CXX_COMPILER "${CMAKE_BINARY_DIR}/cxx-wrapper")
