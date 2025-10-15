# Changelog

## [0.2.1](https://github.com/bytecodealliance/StarlingMonkey/compare/v0.2.0...v0.2.1) (2025-10-15)


### Features

* add support for setting initialization-time script location ([176ccdd](https://github.com/bytecodealliance/StarlingMonkey/commit/176ccddf25787ea2e24fceb225bc0935719ce55b))
* **crypto:** Add SPKI format support for ECDSA and RSA keys ([#277](https://github.com/bytecodealliance/StarlingMonkey/issues/277)) ([0a64399](https://github.com/bytecodealliance/StarlingMonkey/commit/0a643990a5664623c0f56d36ee5f2060df340ca4))


### Bug Fixes

* **ci:** build starling target in release job ([#271](https://github.com/bytecodealliance/StarlingMonkey/issues/271)) ([d360913](https://github.com/bytecodealliance/StarlingMonkey/commit/d360913a650b0f25ce52459008dca352948d89db))
* deprecation warning in panic hook ([#272](https://github.com/bytecodealliance/StarlingMonkey/issues/272)) ([66dd74c](https://github.com/bytecodealliance/StarlingMonkey/commit/66dd74c5e60fec833ede4d6ffaa84d2a7b6962f6))

## [0.2.0](https://github.com/bytecodealliance/StarlingMonkey/compare/v0.1.0...v0.2.0) (2025-08-29)


### Features

* **crypto:** add support for PKCS[#8](https://github.com/bytecodealliance/StarlingMonkey/issues/8) key import ([#251](https://github.com/bytecodealliance/StarlingMonkey/issues/251)) ([059d09a](https://github.com/bytecodealliance/StarlingMonkey/commit/059d09af07d4a32aec463855dae1dcec226f4d45))
* **dx:** add default message for missing handler ([#264](https://github.com/bytecodealliance/StarlingMonkey/issues/264)) ([58b6042](https://github.com/bytecodealliance/StarlingMonkey/commit/58b604204e5423d04e22a7be0b86747ec691b7cd))
* implement AbortController and AbortSignal ([#240](https://github.com/bytecodealliance/StarlingMonkey/issues/240)) ([215b1d9](https://github.com/bytecodealliance/StarlingMonkey/commit/215b1d9acf14ad16a17541d897510a6ddf8ec31c))

## 0.1.0 (2025-08-06)

While this is the first tagged release in this repository, StarlingMonkey build artifacts have been distributed as part of [ComponentizJS](https://github.com/bytecodealliance/ComponentizeJS) releases for some time.

This release is based on [SpiderMonkey](https://spidermonkey.dev/) version 127 and in addition to SpiderMonkey's JavaScript support includes a wide range of web builtins, including:

* **Console**: `console`
* **Crypto**: (A subset of) `SubtleCrypto`, `Crypto`, `crypto`, `CryptoKey`
* **Encoding**: `TextEncoder`, `TextDecoder`, `CompressionStream`, `DecompressionStream`
* **Fetch**: `fetch`, `Request`, `Response`, `Headers`
* **Forms, Files, and Blobs**: `FormData`, `MultipartFormData`, `File`, `Blob`
* **Legacy Encoding**: `atob`, `btoa`, `decodeURI`, `encodeURI`, `decodeURIComponent`, `encodeURIComponent`
* **Location**: `WorkerLocation`, `location`
* **Performance**: `Performance`
* **Streams**: `ReadableStream`, `ReadableStreamBYOBReader`, `ReadableStreamBYOBRequest`, `ReadableStreamDefaultReader`, `ReadableStreamDefaultController`, `ReadableByteStreamController`, `WritableStream`, `ByteLengthQueuingStrategy`, `CountQueuingStrategy`, `TransformStream`
* **Structured Clone**: `structuredClone`
* **Task**: `queueMicrotask`, `setInterval` `setTimeout` `clearInterval` `clearTimeout`
* **URL**: `URL`, `URLSearchParams`
