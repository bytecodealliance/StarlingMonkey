<div align="center">
  <h1><code>StarlingMonkey</code></h1>

  <p>
    <strong>A SpiderMonkey-based JS runtime on WebAssembly</strong>
  </p>

  <strong>A <a href="https://bytecodealliance.org/">Bytecode Alliance</a> project</strong>

  <p>
    <a href="https://github.com/bytecodealliance/StarlingMonkey/actions?query=workflow%3ACI"><img src="https://github.com/bytecodealliance/StarlingMonkey/workflows/CI/badge.svg" alt="build status" /></a>
    <a href="https://bytecodealliance.zulipchat.com/#narrow/stream/459697-StarlingMonkey"><img src="https://img.shields.io/badge/zulip-join_chat-brightgreen.svg" alt="zulip chat" /></a>
  </p>

  <h3>
    <a href="#building-and-running">Building</a>
    <span> | </span>
    <a href="ADOPTERS.md">Adopters</a>
    <span> | </span>
    <a href="https://bytecodealliance.zulipchat.com/#narrow/stream/459697-StarlingMonkey">Chat</a>
  </h3>
</div>

StarlingMonkey is a [SpiderMonkey](https://spidermonkey.dev/) based JS runtime optimized for use in [WebAssembly Components](https://component-model.bytecodealliance.org/).
StarlingMonkey's core builtins target WASI 0.2.0 to support a Component Model based event loop and standards-compliant implementations of key web builtins, including the fetch API, WHATWG Streams, text encoding, and others. To support tailoring for specific use cases, it's designed to be highly modular, and can be readily extended with custom builtins and host APIs.

StarlingMonkey is used in production for Fastly's JS Compute platform, and Fermyon's Spin JS SDK. See the [ADOPTERS](ADOPTERS.md) file for more details.

## Building and Running

### Requirements

The runtime's build is managed by [cmake](https://cmake.org/), which also takes care of downloading the build dependencies.
To properly manage the Rust toolchain, the build script expects [rustup](https://rustup.rs/) to be installed in the system.

### Usage

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

Building the runtime is done in two phases: first, cmake is used to build a raw version as a WebAssembly core module. Then, that module is turned into a [WebAssembly Component](https://component-model.bytecodealliance.org/) using the `componentize.sh` script.

The following command will build the `starling-raw.wasm` runtime module in the `cmake-build-release` directory:
```bash
# Use cmake-build-debug for the debug build
# Change the value for `--parallel` to match the number of CPU cores in your system
cmake --build cmake-build-release --parallel 8
```

Then, the `starling-raw.wasm` module can be turned into a component with the following command:

```bash
cd cmake-build-release
./componentize.sh -o starling.wasm
```

The resulting runtime can be used to load and evaluate JS code dynamically:

```bash
wasmtime -S http starling.wasm -e "console.log('hello world')"
# or, to load a file:
wasmtime -S http --dir . starling.wasm index.js
```

Alternatively, a JS file can be provided during componentization:

```bash
cd cmake-build-release
./componentize.sh index.js -o starling.wasm
```

This way, the JS file will be loaded during componentization, and the top-level code will be executed, and can e.g. register a handler for the `fetch` event to serve HTTP requests.

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

### Web Platform Tests

To run the [Web Platform Tests](https://web-platform-tests.org/) suite, the WPT runner requires `Node.js` above v18.0 to be installed, and during build configuration the option `ENABLE_WPT:BOOL=ON` must be set.

```bash
cmake -S . -B cmake-build-debug -DENABLE_WPT:BOOL=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --parallel 8 --target wpt-runtime
cat deps/wpt-hosts | sudo tee -a /etc/hosts # Required to resolve test server hostnames
cd cmake-build-debug
ctest -R wpt --verbose # Note: some of the tests run fairly slowly in debug builds, so be patient
```

The Web Platform Tests checkout can also be customized by setting the `WPT_ROOT=[path to your WPT checkout]` environment variable to the cmake command.

WPT tests can be filtered with the `WPT_FILTER=string` variable, for example:

```bash
WPT_FILTER=fetch ctest -R wpt -v
```

Custom flags can also be passed to the test runner via `WPT_FLAGS="..."`, for example to update expectations use:

```bash
WPT_FLAGS="--update-expectations" ctest -R wpt -v
```

### Configuring available builtins

StarlingMonkey supports enabling/disabling bundled builtins using CMake options. You can get a full list of bundled builtins by running the following shell command:

```shell
cmake -P [PATH_TO_STARLING_MONKEY]/cmake/builtins.cmake
```

Note that it's required to include builtins defining all exports defined by the used host API. Using the default WASI 0.2.0 host API, that means including the `fetch_event` builtin.

### Running project-specific commands using `just`

The justfile provides a streamlined interface for executing common project-specific tasks. To install just, you can use the command `cargo install just` or `cargo binstall just`. Alternatively, refer to the official [installation instructions](https://github.com/casey/just?tab=readme-ov-file#installation) for your specific system.

Once installed, navigate to the project directory and run `just` commands as needed. For instance, the following commands will configure a default `cmake-build-debug` directory and build the project.

``` shell
just build
```

To load a JS script during componentization and serve its output using `Wasmtime`, run:
``` shell
just serve <filename>.js
```

To build and run integration tests run:

``` shell
just test
```

To build and run Web Platform Tests run:

``` shell
just wpt-setup # prepare WPT hosts
just wpt-test # run all tests
just wpt-test console/console-log-symbol.any.js # run specific test
```

To view a complete list of available recipes, run:

``` shell
just --list

```

> [!NOTE]
> By default, the CMake configuration step is skipped if the build directory already exists. However, this can sometimes cause issues if the existing build directory was configured for a different target. For instance:
> - Running `just build` creates a build directory for the default target,
> - Running `just wpt-build` afterward may fail because the WPT target hasn’t been configured in the existing build directory.
>
> To resolve this, you can force cmake to reconfigure the build directory by adding the `reconfigure=true` parameter. For example:
>
> ``` shell
> just reconfigure=true wpt-build
> ```

#### Customizing build
The default build mode is debug, which automatically configures the build directory to `cmake-build-debug`. You can switch to a different build mode, such as release, by specifying the mode parameter. For example:

``` shell
just mode=release build
```

This command will set the build mode to release, and the build directory will automatically change to `cmake-build-release`.

If you want to override the default build directory, you can use the `builddir` parameter. 

``` shell
just builddir=mybuilddir mode=release build
```

This command configures CMake to use `mybuilddir` as the build directory and sets the build mode to `release`.

#### Starting the WPT Server
You can also start a Web Platform Tests (WPT) server with:

``` shell
just wpt-server
```

After starting the server, individual tests can be run by sending a request with the test name to the server instance. For example:

``` shell
curl http://127.0.0.1:7676/console/console-log-symbol.any.js

```

### Using StarlingMonkey as a CMake sub-project

StarlingMonkey can be used as a subproject in a larger CMake project.

The importing project must at a minimum contain the following line in its `CMakeLists.txt`:

```cmake
include("${PATH_TO_STARLING_MONKEY}/cmake/add_as_subproject.cmake")
```

With that line added (and `${PATH_TO_STARLING_MONKEY}` replaced with the actual path to StarlingMonkey), the importing project will have all the build targets of StarlingMonkey available to it.

Note that building the `starling-raw.wasm` target itself will result in the linked `starling-raw.wasm` file being created in the `starling-raw.wasm` sub-directory of the importing project's build directory.

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

The [host-apis](host-apis) directory can contain implementations of the host API for different 
versions of WASI—or in theory any other host interface. Those can be selected by setting the 
`HOST_API` environment variable to the 
name of one of the directories. Currently, only an implementation in terms of [wasi-0.2.0]
(host-apis/wasi-0.2.0) is provided, and used by default.

To provide a custom host API implementation, you can set `HOST_API` to the (absolute) path of a directory containing that implementation.

## Developing Changes to SpiderMonkey

StarlingMonkey uses SpiderMonkey as its underlying JS engine, and by default,
downloads build artifacts from [a wrapper
repository](https://github.com/bytecodealliance/spidermonkey-wasi-embedding)
around [our local SpiderMonkey
tree](https://github.com/bytecodealliance/gecko-dev). That wrapper repository
contains a SpiderMonkey commit-hash in a file, and its CI jobs build the
artifacts that StarlingMonkey downloads during its build.

This flow is optimized for ease of development of StarlingMonkey, and avoiding
the need to build SpiderMonkey locally, which requires some additional tools
and is resource-intensive. However, sometimes it is necessary or desirable to
make modifications to SpiderMonkey directly, whether to make fixes or optimize
performance.

In order to do so, first clone the above two repositories, with `gecko-dev`
(SpiderMonkey itself) as a subdirectory to `spidermonkey-wasi-embedding`:

```shell
git clone https://github.com/bytecodealliance/spidermonkey-wasi-embedding
cd spidermonkey-wasi-embedding/
git clone https://github.com/bytecodealliance/gecko-dev
```

and switch to the commit that we are currently using:

```shell
git checkout `cat ../gecko-revision`
# now edit the source
```

Then make changes as necessary, eventually rebuilding from the
`spidermonkey-wasi-embedding` root:

```shell
cd ../ # back to spidermonkey-wasi-embedding
./rebuild.sh release
```

This will produce a `release/` directory with artifacts of the same form
normally downloaded by StarlingMonkey. So, finally, from within StarlingMonkey,
set an environment variable `SPIDERMONKEY_BINARIES`:

```shell
export SPIDERMONKEY_BINARIES=/path/to/spidermonkey-wasi-embedding/release
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel 8
```

and use/test as above.
