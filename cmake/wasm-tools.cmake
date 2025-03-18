set(WASM_TOOLS_VERSION 1.0.54)

set(WASM_TOOLS_URL https://github.com/bytecodealliance/wasm-tools/releases/download/wasm-tools-${WASM_TOOLS_VERSION}/wasm-tools-${WASM_TOOLS_VERSION}-${HOST_ARCH}-${HOST_OS}.tar.gz)
CPMAddPackage(NAME wasm-tools URL ${WASM_TOOLS_URL} DOWNLOAD_ONLY TRUE)
set(WASM_TOOLS_DIR ${CPM_PACKAGE_wasm-tools_SOURCE_DIR})
set(WASM_TOOLS_BIN ${WASM_TOOLS_DIR}/wasm-tools CACHE FILEPATH "Path to wasm-tools binary")
