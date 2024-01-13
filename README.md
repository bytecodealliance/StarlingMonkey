# JS.wasm

## Requirements

The runtime's build is managed by [cmake](https://cmake.org/), which also takes care of downloading the build dependencies.
To properly manage the Rust toolchain, the build script expects [rustup](https://rustup.rs/) to be installed in the system.

### Overriding requirements

The build script automatically manages all requirements except those mentioned above, but it's possible to override some of them using environment variables.

Note that the build script will not check whether the requirements are satisfied, so it's up to the user to ensure that the overrides are valid. Additionally, requirements are checked and set at configuration time (i.e. during step 2 below), so it's up to the developer stay valid after configuration, or to reconfigure the build after making changes to locally installed dependencies.

The following environment variables can be used to override the build script's default behavior:

- `HOST_API` — use a specific implementation of the `host-api.h` API. The default is the `wasi-0.2.0` implementation. See the [dedicated section](#providing-a-custom-host-api-implementation) on specifying a custom host API implementation below for more details
 - `WASI_SDK_PREFIX` — use a locally installed [WASI SDK](https://github.com/WebAssembly/wasi-sdk/)
 - `WASM_TOOLS_DIR` — use a locally installed [wasm-tools](https://github.com/bytecodealliance/wasm-tools/)
 - `WIZER_DIR` — use a locally installed [wizer](https://github.com/bytecodealliance/wizer)
 - `SM_SOURCE_DIR` — use a local SpiderMonkey build. Note that the layout needs to match the one produced by the [spidermonkey-wasi-embedding](https://github.com/tschneidereit/spidermonkey-wasi-embedding/) project, and that you need to ensure that you provide a path to the right build type—i.e. a `debug` build for Debug configurations, and a `release` build for Release configurations.

## Building and Running

With sufficiently new versions of `cmake` and `rustup` installed, the build process is as follows:

### 1. Clone the repo

```bash
git clone https://github.com/fermyon/js.wasm
cd js.wasm
```

### 2. Run the configuration script

For a release configuration, run
```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
```

For a debug configuration, run
```bash
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
```

### 3. Build the runtime

The following command will build the `js.wasm` runtime module in the `cmake-build-release` directory:
```bash
# Use cmake-build-debug for the debug build
# Change the value for `--parallel` to match the number of CPU cores in your system
cmake --build cmake-build-release --parallel 8
```

### 4. Testing the build 

A simple test script is provided in `tests/smoke.js`. To run it using [`Spin`](https://github.com/fermyon/spin), use the following command:
```bash
cmake --build cmake-build-release --target smoke-test
cd cmake-build-release
spin up
```

Then visit https://localhost:3000 

### 5. Using the runtime with other JS applications

The build directory contains a shell script `componentize.sh` that can be used to create components from JS applications. `componentize.sh` takes a single argument, the path to the JS application, and creates a component with a name of the form `[input-file-name].wasm` in the current working directory.

For example, the following command is equivalent to the `cmake` invocation from step 5, and will create the component `cmake-build-release/smoke.js.wasm`:

```bash
cd cmake-build-release
./componentize.sh ../tests/smoke.js
```


## Using js.wasm as a CMake sub-project

The `js.wasm` runtime can be used as a subproject in a larger CMake project.

The importing project must at a minimum contain the following line in its `CMakeLists.txt`:

```cmake
include("${PATH_TO_JS_WASM}/cmake/add_as_subproject.cmake")
```

With that line added (and `${PATH_TO_JS_WASM}` replaced with the actual path to js.wasm), the importing project will have all the build targets of `js.wasm` available to it.

Note that building the `js.wasm` target itself will result in the linked `js.wasm` file being created in the `js.wasm` sub-directory of the importing project's build directory.

To make use of importing js.wasm in this way, you'll probably want to add additional builtins, or provide your own implementation of the host interface.

### Adding custom builtins

Adding builtins is as simple as calling `add_builtin` in the importing project's `CMakeLists.txt`. Say you want to add a builtin defined in the file `my-builtin.cpp`, like so:

```cpp
// The extension API is automatically on the include path for builtins.
#include "extension-api.h"

// The namespace name must match the name passed to `add_builtin` in the CMakeLists.txt
namespace my_project::my_builtin {

    bool install(api::Engine* engine) {
        printf("installing my-builtin\n");
        return true;
    }

} // namespace my_builtin
```

This file can now be included in the runtime's builtins like so:
```cmake
add_builtin(my_project::my_builtin SRC my-builtin.cpp)
```

If your builtin requires multiple `.cpp` files, you can pass all of them to `add_builtin` as values for the `SRC` argument.


### Providing a custom host API implementation

The [host-apis](host-apis) directory contains implementations of the host API for different versions of WASI. Those can be selected by setting the `HOST_API` environment variable to the name of one of the directories. By default, the [wasi-0.2.0](host-apis/wasi-0.2.0) host API is used.

To provide a custom host API implementation, you can set `HOST_API` to the (absolute) path of a directory containing that implementation.
