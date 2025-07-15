# Building and Running

## Requirements

The runtime's build is managed by [cmake](https://cmake.org/), which also takes care of downloading
the build dependencies. To properly manage the Rust toolchain, the build script expects
[rustup](https://rustup.rs/) to be installed in the system.

See also [Project workflow using `just`](../developer/just.md) for documentation on how to use
`just` to streamline the project interface.

## Usage

With sufficiently new versions of `cmake` and `rustup` installed, the build process is as follows:

1. Clone the repo

```console
git clone https://github.com/bytecodealliance/StarlingMonkey
cd StarlingMonkey
```

2. Run the configuration script

For a release configuration, run

```console
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
```

For a debug configuration, run

```console
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
```

3. Build the runtime

Building the runtime is done in two phases: first, cmake is used to build a raw version as a
WebAssembly core module. Then, that module is turned into a
[WebAssembly Component](https://component-model.bytecodealliance.org/) using the `componentize.sh`
script.

The following command will build the `starling-raw.wasm` runtime module in the `cmake-build-release`
directory:

```console
# Use cmake-build-debug for the debug build
# Change the value for `--parallel` to match the number of CPU cores in your system
cmake --build cmake-build-release --parallel 8
```

Then, the `starling-raw.wasm` module can be turned into a component with the following command:

```console
cd cmake-build-release
./componentize.sh -o starling.wasm
```

The resulting runtime can be used to load and evaluate JS code dynamically:

```console
wasmtime -S http starling.wasm -e "console.log('hello world')"
# or, to load a file:
wasmtime -S http --dir . starling.wasm index.js
```

Alternatively, a JS file can be provided during componentization:

```console
cd cmake-build-release
./componentize.sh index.js -o starling.wasm
```

This way, the JS file will be loaded during componentization, and the top-level code will be
executed, and can e.g. register a handler for the `fetch` event to serve HTTP requests.
