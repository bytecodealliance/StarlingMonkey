# JS.wasm

## Building and Running

1. Clone the repo and fetch the submodules

```bash
git clone https://github.com/fermyon/js.wasm
cd js.wasm
git submodule update --recursive --remote --init  
```

1. Downloand and build the `spidermonkey` engine

```
cd deps/spidermonkey
./download-engine.sh
..build-engine.sh
```

1. Download and install wasi-sdk

Visit the [was-sdk repo](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-20) and download the appropriate release.

extract and install the module (commands for macos)


```bash
tar -xf wasi-sdk-20.0-macos.tar.gz
# The following command might require sudo
mv was-sdk-20.0 /opt/wasi-sdk
```

If you prefer not to move `wasi-sdk`, you could alternatively set an environment variable pointing to the location of the wasi-sdk

```bash
export WASI_SDK=/path/to/wasi-sdk
```

1. Build the wasm module

```bash
cd ../../
# make sure you are in the root of the project
```

For a release build 

```bash
make all
```

For a debug build

```bash
DEBUG=1
```

1. Testing the build 

By default the JavaScript file `./tests/smoke.js` will be embedded in the build. To test the app using [`Spin`](https://github.com/fermyon/spin), use the following command

```bash
spin up
```

Then visit https://localhost:3000 

1. Building a different JS file

You can use the env var `TEST_JS`