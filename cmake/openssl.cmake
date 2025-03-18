# Based on https://stackoverflow.com/a/72187533
set(OPENSSL_VERSION 3.0.7)
set(OPENSSL_SOURCE_DIR ${CMAKE_BINARY_DIR}/deps-src/OpenSSL)
set(OPENSSL_INSTALL_DIR ${CMAKE_BINARY_DIR}/deps/OpenSSL)
set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include)
include(ExternalProject)

if (CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set(MAKE_COMMAND "$(MAKE)")
else()
    find_program(MAKE_COMMAND NAMES make gmake)
endif()

ExternalProject_Add(
        OpenSSL
        SOURCE_DIR ${OPENSSL_SOURCE_DIR}
        URL https://openssl.org/source/old/3.0/openssl-${OPENSSL_VERSION}.tar.gz
        URL_HASH SHA256=83049d042a260e696f62406ac5c08bf706fd84383f945cf21bd61e9ed95c396e
        USES_TERMINAL_DOWNLOAD TRUE
        PATCH_COMMAND
        patch -d ${OPENSSL_SOURCE_DIR} -t -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/deps/patches/getuid.patch &&
        patch -d ${OPENSSL_SOURCE_DIR} -t -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/deps/patches/rand.patch
        CONFIGURE_COMMAND
        CC="clang" CFLAGS="--sysroot=${WASI_SDK_PREFIX}/share/wasi-sysroot" ${OPENSSL_SOURCE_DIR}/config
        --prefix=${OPENSSL_INSTALL_DIR}
        --openssldir=${OPENSSL_INSTALL_DIR}
        -static -no-sock -no-asm -no-ui-console -no-egd
        -no-afalgeng -no-tests -no-stdio -no-threads
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
set_property(TARGET OpenSSL::Crypto PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
add_dependencies(OpenSSL::Crypto OpenSSL)
