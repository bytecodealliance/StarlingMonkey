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
    <a href="#quick-start">Building</a>
    <span> | </span>
    <a href="ADOPTERS.md">Adopters</a>
    <span> | </span>
    <a href="https://bytecodealliance.github.io/StarlingMonkey">Documentation</a>
    <span> | </span>
    <a href="https://bytecodealliance.zulipchat.com/#narrow/stream/459697-StarlingMonkey">Chat</a>
  </h3>
</div>

StarlingMonkey is a [SpiderMonkey][spidermonkey] based JS runtime optimized for use in [WebAssembly
Components][wasm-component]. StarlingMonkey's core builtins target WASI 0.2.0 to support a Component
Model based event loop and standards-compliant implementations of key web builtins, including the
fetch API, WHATWG Streams, text encoding, and others. To support tailoring for specific use cases,
it's designed to be highly modular, and can be readily extended with custom builtins and host APIs.

StarlingMonkey is used in production for Fastly's JS Compute platform, and Fermyon's Spin JS SDK.
See the [ADOPTERS](ADOPTERS.md) file for more details.

## Documentation

For comprehensive documentation, visit our [Documentation Site][gh-pages].

## Quick Start

### Requirements

The runtime's build is managed by [cmake][cmake], which also takes care of downloading the build
dependencies. To properly manage the Rust toolchain, the build script expects
[rustup](https://rustup.rs/) to be installed in the system.

### Usage

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

The build system provides two targets for the runtime: `starling-raw.wasm` and `starling.wasm`. The former is a raw WebAssembly core module that can be used to build a WebAssembly Component, while the latter is the final componentized runtime that can be used directly with a WebAssembly Component-aware runtime like [wasmtime](https://wasmtime.dev/).

A key difference is that `starling.wasm` can only be used for runtime-evaluation of JavaScript code,
while `starling-raw.wasm` can be used to build a WebAssembly Component that is specialized for a specific
JavaScript application, and as a result has much faster startup times.

## Using StarlingMonkey with dynamically loaded JS code

The following command will build the `starling.wasm` runtime module in the `cmake-build-release`
directory:

```console
# Use cmake-build-debug for the debug build
cmake --build cmake-build-release -t starling --parallel $(nproc)
```

The resulting runtime can be used to load and evaluate JS code dynamically:

```console
wasmtime -S http cmake-build-release/starling.wasm -e "console.log('hello world')"
# or, to load a file:
wasmtime -S http --dir . starling.wasm index.js
```


## Creating a specialized runtime for your JS code

To create a specialized version of the runtime, first build a raw, unspecialized core wasm version of StarlingMonkey:

```console
# Use cmake-build-debug for the debug build
cmake --build cmake-build-release -t starling-raw.wasm --parallel $(nproc)
```

Then, the `starling-raw.wasm` module can be turned into a component specialized for your code with the following command:

```console
cd cmake-build-release
./componentize.sh index.js -o index.wasm
```

This mode currently only supports the creation of HTTP server components, which means that the `index.js` file must register a `fetch` event handler. For example, your `index.js` could contain the following code:

```javascript
addEventListener('fetch', event => {
  event.respondWith(new Response('Hello, world!'));
});
```

Componentizing this code like above allows running it like this:

```console
wasmtime serve -S cli --dir . index.wasm
```

[cmake]: https://cmake.org/
[gh-pages]: https://bytecodealliance.github.io/StarlingMonkey/
[spidermonkey]: https://spidermonkey.dev/
[wasm-component]: https://component-model.bytecodealliance.org/
