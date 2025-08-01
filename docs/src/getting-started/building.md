# Building and Running

## Requirements

The runtime's build is managed by [cmake](https://cmake.org/), which also takes care of downloading
the build dependencies. To properly manage the Rust toolchain, the build script expects
[rustup](https://rustup.rs/) to be installed in the system.

See also [Project workflow using `just`](../developer/just.md) for documentation on how to use
`just` to streamline the project interface.

## Usage

### Step 1: Clone the repo

```console
git clone https://github.com/bytecodealliance/StarlingMonkey
cd StarlingMonkey
```

### Step 2: Run the configuration script

Choose one of the following configurations:

- For a release configuration:

  ```console
  cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
  ```

- For a debug configuration:
  ```console
  cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
  ```

### Step 3: Build the runtime

Building the runtime is done in two phases: first, cmake is used to build a raw version as a
WebAssembly core module. Then, that module is turned into a
[WebAssembly Component](https://component-model.bytecodealliance.org/) using the `componentize.sh`
script.

- **Build the WebAssembly component**

  The following command will build the runtime, creating the files `starling-raw.wasm` and
  `starling.wasm` in the `cmake-build-release` directory:

  ```console
  # Use cmake-build-debug for the debug build
  # Change the value for `--parallel` to match the number of CPU cores in your system
  cmake --build cmake-build-release --parallel 8 --target starling
  ```

- **Test the runtime**

  The `starling.wasm` component can be used to load and evaluate JS code dynamically:

  ```console
  wasmtime -S http starling.wasm -e "console.log('hello world')"
  # or, to load a file:
  wasmtime -S http --dir . starling.wasm index.js
  ```

- **Create components from JS files (alternative approach)**

  Alternatively, the `componentize.js` script also provided in the build directory lets us create a
  component from a JS file:

  ```console
  cd cmake-build-release
  ./componentize.sh index.js
  wasmtime -S http --dir . index.wasm
  ```

  This way, the JS file will be loaded during componentization, and the top-level code will be
  executed, and can e.g. register a handler for the `fetch` event to serve HTTP requests.
