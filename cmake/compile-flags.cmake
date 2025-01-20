set(WASI 1)
set(CMAKE_CXX_STANDARD 20)
add_compile_definitions("$<$<CONFIG:DEBUG>:DEBUG=1>")

list(APPEND CMAKE_EXE_LINKER_FLAGS
        -Wl,-z,stack-size=1048576 -Wl,--stack-first
        -mexec-model=reactor
        -lwasi-emulated-signal
        -lwasi-emulated-process-clocks
        -lwasi-emulated-getpid
)
list(JOIN CMAKE_EXE_LINKER_FLAGS " " CMAKE_EXE_LINKER_FLAGS)

list(APPEND CMAKE_CXX_FLAGS
        -std=gnu++20 -Wall -Werror -Qunused-arguments -Wimplicit-fallthrough
        -fno-sized-deallocation -fno-aligned-new -mthread-model single
        -fPIC -fno-rtti -fno-exceptions -fno-math-errno -pipe
        -fno-omit-frame-pointer -funwind-tables -m32
)
list(JOIN CMAKE_CXX_FLAGS " " CMAKE_CXX_FLAGS)

list(APPEND CMAKE_C_FLAGS
        -Wall -Werror -Wno-unknown-attributes -Wno-pointer-to-int-cast
        -Wno-int-to-pointer-cast -m32
)
list(JOIN CMAKE_C_FLAGS " " CMAKE_C_FLAGS)
