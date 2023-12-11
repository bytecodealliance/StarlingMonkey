# JS.wasm

## Requirements

The following dependencies are required to be installed in the system to build and run the JS runtime:
1. [WASI-SDK](https://github.com/WebAssembly/wasi-sdk/) (Versions known to work: 16 and 20)
2. [Rust](https://www.rust-lang.org/) version 1.68.2
3. The wasm32-wasi target for the same Rust version
4. The [wasm-tools](https://github.com/bytecodealliance/wasm-tools/) CLI (Version known to work: 1.0.54)
5. The [Wizer](https://github.com/bytecodealliance/wizer) CLI tool (Version known to work: 3.0.0)
6. The [Spin runtime](https://developer.fermyon.com/spin/index) (Version known to work: 2.0)

## Building and Running

1. Clone the repo and fetch the submodules

```bash
git clone https://github.com/fermyon/js.wasm
cd js.wasm
git submodule update --recursive --remote --init  
```

2. Download debug and release builds of the `spidermonkey` engine

```
(cd deps/spidermonkey && ./download-engine.sh)
(cd deps/spidermonkey && ./download-engine.sh debug)
```

3. Ensure you have a compatible version of the [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/) installed

The runtime needs to be built with a wasi-sdk that is compatible with the one used to build the JS engine. As of this writing, versions 16 and 20 have been tested and are working, but any version starting with 16 should work.

If you don't have a compatible version installed, download and install [version 20](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-20).


```bash
tar -xf wasi-sdk-20.0-macos.tar.gz
# Move the SDK to the default location; might require sudo
mv was-sdk-20.0 /opt/wasi-sdk
```

If you prefer not to place the SDK in the default location, you can alternatively instruct the build script to use another location using the `WASI_SDK` environment variable:

```bash
export WASI_SDK=/path/to/wasi-sdk
```

4. Build the runtime

By default, the file `tests/smoke.js` will be used when building the runtime. To use another JS file, set the environment variable `TEST_JS` to point to that file.
For a release build 

```bash
make all
```

For a debug build

```bash
DEBUG=1 make all
```

5. Testing the build 

To test the app using [`Spin`](https://github.com/fermyon/spin), use the following command

```bash
spin up
```

Then visit https://localhost:3000 
