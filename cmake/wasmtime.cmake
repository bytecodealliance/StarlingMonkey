set(WASMTIME_VERSION v19.0.2)
set(WASMTIME_URL https://github.com/bytecodealliance/wasmtime/releases/download/${WASMTIME_VERSION}/wasmtime-${WASMTIME_VERSION}-${HOST_ARCH}-${HOST_OS}.tar.xz)
CPMAddPackage(NAME wasmtime URL ${WASMTIME_URL} DOWNLOAD_ONLY TRUE)
set(WASMTIME ${CPM_PACKAGE_wasmtime_SOURCE_DIR}/wasmtime)
