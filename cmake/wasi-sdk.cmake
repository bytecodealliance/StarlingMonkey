set(WASI_SDK_VERSION 30 CACHE STRING "Version of wasi-sdk to use" FORCE)

string(REPLACE "aarch64" "arm64" WASI_SDK_ARCH ${HOST_CPU})
set(WASI_SDK_URL "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_SDK_VERSION}/wasi-sdk-${WASI_SDK_VERSION}.0-${WASI_SDK_ARCH}-${HOST_OS}.tar.gz")
CPMAddPackage(NAME wasi-sdk URL ${WASI_SDK_URL})
set(WASI_SDK_PREFIX ${CPM_PACKAGE_wasi-sdk_SOURCE_DIR})
set(CMAKE_TOOLCHAIN_FILE ${WASI_SDK_PREFIX}/share/cmake/wasi-sdk.cmake)
