# The StarlingMonkey JS Runtime

StarlingMonkey is a [SpiderMonkey](https://spidermonkey.dev/) based JS runtime optimized for use in [WebAssembly Components](https://component-model.bytecodealliance.org/).
StarlingMonkey's core builtins target WASI 0.2.0 to support a Component Model based event loop and standards-compliant implementations of key web builtins, including the fetch API, WHATWG Streams, text encoding, and others. To support tailoring for specific use cases, it's designed to be highly modular, and can be readily extended with custom builtins and host APIs.

## Requirements

The runtime's build is managed by [cmake](https://cmake.org/), which also takes care of downloading the build dependencies.
To properly manage the Rust toolchain, the build script expects [rustup](https://rustup.rs/) to be installed in the system.

## Building and Running

With sufficiently new versions of `cmake` and `rustup` installed, the build process is as follows:

1. Clone the repo

```bash
git clone https://github.com/bytecodealliance/StarlingMonkey
cd StarlingMonkey
```

2. Run the configuration script

For a release configuration, run
```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
```

For a debug configuration, run
```bash
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
```

3. Build the runtime

The following command will build the `starling.wasm` runtime module in the `cmake-build-release` directory:
```bash
# Use cmake-build-debug for the debug build
# Change the value for `--parallel` to match the number of CPU cores in your system
cmake --build cmake-build-release --parallel 8
```

4. Testing the build 

After completing the build (a debug build in this case), the integration test runner can be built:

```bash
cmake --build cmake-build-debug --target integration-test-server
```

Then tests can be run with `ctest` directly via:

```bash
ctest --test-dir cmake-build-debug -j8 --output-on-failure
```

Alternatively, the integration test server can be directly run with `wasmtime serve` via:

```bash
wasmtime serve -S common cmake-build-debug/test-server.wasm
```

Then visit http://0.0.0.0:8080/timers, or any test name and filter of the form `[testName]/[filter]`

5. Using the runtime with other JS applications

The build directory contains a shell script `componentize.sh` that can be used to create components from JS applications. `componentize.sh` takes a single argument, the path to the JS application, and creates a component with a name of the form `[input-file-name].wasm` in the current working directory.

For example, the following command is equivalent to the `cmake` invocation from step 5, and will create the component `cmake-build-release/smoke.wasm`:

```bash
cd cmake-build-release
./componentize.sh ../tests/smoke.js
```


## Thorough testing with the Web Platform Tests suite

StarlingMonkey includes a test runner for the [Web Platform Tests](https://web-platform-tests.org/) suite. The test runner is built as part of the `starling.wasm` runtime, and can be run using the `wpt-test` target.

### Requirements

The WPT runner requires `Node.js` to be installed, and during build configuration the option `ENABLE_WPT:BOOL=ON` must be set.

When running the test, `WPT_ROOT` must be set to the path of a checkout of the WPT suite at revision `1014eae5e66f8f334610d5a1521756f7a2fb769f`:

```bash
WPT_ROOT=[path to your WPT checkout] cmake -S . -B cmake-build-debug -DENABLE_WPT:BOOL=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --parallel 8 --target wpt-runtime
cd cmake-build-debug
ctest --verbose # Note: some of the tests run fairly slowly in debug builds, so be patient
```

## Configuring available builtins
StarlingMonkey supports enabling/disabling bundled builtins using CMake options. You can get a full list of bundled builtins by running the following shell command:
```shell
cmake -P [PATH_TO_STARLING_MONKEY]/cmake/builtins.cmake
```

Note that it's required to include builtins defining all exports defined by the used host API. Using the default WASI 0.2.0 host API, that means including the `fetch_event` builtin.


## Using StarlingMonkey as a CMake sub-project

StarlingMonkey can be used as a subproject in a larger CMake project.

The importing project must at a minimum contain the following line in its `CMakeLists.txt`:

```cmake
include("${PATH_TO_STARLING_MONKEY}/cmake/add_as_subproject.cmake")
```

With that line added (and `${PATH_TO_STARLING_MONKEY}` replaced with the actual path to StarlingMonkey), the importing project will have all the build targets of StarlingMonkey available to it.

Note that building the `starling.wasm` target itself will result in the linked `starling.wasm` file being created in the `starling.wasm` sub-directory of the importing project's build directory.

To make use of importing StarlingMonkey in this way, you'll probably want to add additional builtins, or provide your own implementation of the host interface.

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
