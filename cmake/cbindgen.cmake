set(CBINDGEN_VERSION 0.29.0)

# cbindgen doesn't have pre-built binaries for all platforms, so we install it via cargo-binstall. Which we install first, too.
find_program(CBINDGEN_EXECUTABLE cbindgen)
if(NOT CBINDGEN_EXECUTABLE)
    find_program(CARGO_BINSTALL_EXECUTABLE cargo-binstall)
    if(NOT CARGO_BINSTALL_EXECUTABLE)
        execute_process(
                COMMAND curl -L --tlsv1.2 -sSf https://raw.githubusercontent.com/cargo-bins/cargo-binstall/main/install-from-binstall-release.sh
                COMMAND bash
        )
    endif()
    execute_process(
            COMMAND cargo binstall -y cbindgen
    )
endif()
