# Based on https://stackoverflow.com/a/72187533
set(OPENSSL_VERSION 3.0.16)
set(OPENSSL_HASH "SHA256=57e03c50feab5d31b152af2b764f10379aecd8ee92f16c985983ce4a99f7ef86")
set(OPENSSL_INSTALL_DIR ${CMAKE_BINARY_DIR}/deps/OpenSSL)
set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include)
include(ExternalProject)

if (CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set(MAKE_COMMAND "$(MAKE)")
else()
    find_program(MAKE_COMMAND NAMES make gmake)
endif()

CPMAddPackage(NAME
        OpenSSL
        URL https://openssl.org/source/old/3.0/openssl-${OPENSSL_VERSION}.tar.gz
        URL_HASH ${OPENSSL_HASH}
        PATCHES
            ${CMAKE_CURRENT_SOURCE_DIR}/deps/patches/getuid.patch
            ${CMAKE_CURRENT_SOURCE_DIR}/deps/patches/rand.patch
        DOWNLOAD_ONLY TRUE
)
ExternalProject_Add(
        OpenSSL
        SOURCE_DIR ${OpenSSL_SOURCE_DIR}
        CONFIGURE_COMMAND
        CC="clang" CFLAGS="--sysroot=${WASI_SDK_PREFIX}/share/wasi-sysroot" ${OpenSSL_SOURCE_DIR}/config
        --prefix=${OPENSSL_INSTALL_DIR}
        --openssldir=${OPENSSL_INSTALL_DIR}
        -static -no-sock -no-asm -no-ui-console -no-egd
        -no-afalgeng -no-tests -no-stdio -no-threads no-dso
        -D_WASI_EMULATED_SIGNAL
        -D_WASI_EMULATED_PROCESS_CLOCKS
        -D_WASI_EMULATED_GETPID
        -DHAVE_FORK=0
        -DNO_SYSLOG
        -DNO_CHMOD
        -DOPENSSL_NO_SECURE_MEMORY
        --with-rand-seed=getrandom
        --prefix=${OPENSSL_INSTALL_DIR}
        --cross-compile-prefix=${WASI_SDK_PREFIX}/bin/
        linux-x32
        BUILD_COMMAND ${MAKE_COMMAND}
        TEST_COMMAND ""
        INSTALL_COMMAND ${MAKE_COMMAND} install_sw
        INSTALL_DIR ${OPENSSL_INSTALL_DIR}
        BUILD_BYPRODUCTS ${OPENSSL_INSTALL_DIR}/libx32/libcrypto.a
)

# We cannot use find_library because ExternalProject_Add() is performed at build time.
# And to please the property INTERFACE_INCLUDE_DIRECTORIES,
# we make the include directory in advance.
file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/libx32/libcrypto.a)
target_include_directories(OpenSSL::Crypto INTERFACE ${OPENSSL_INCLUDE_DIR})
add_dependencies(OpenSSL::Crypto OpenSSL)
