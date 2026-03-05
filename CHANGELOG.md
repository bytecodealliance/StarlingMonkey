# Changelog

## [0.3.0](https://github.com/bytecodealliance/StarlingMonkey/compare/starlingmonkey-v0.2.1...starlingmonkey-v0.3.0) (2026-03-05)


### Features

* reenable weval patches ([#303](https://github.com/bytecodealliance/StarlingMonkey/issues/303)) ([643aa45](https://github.com/bytecodealliance/StarlingMonkey/commit/643aa451947694ba8cbf2e51cb2b63d7bc972d92))
* update wasi to 0.2.10 ([#300](https://github.com/bytecodealliance/StarlingMonkey/issues/300)) ([c0873b7](https://github.com/bytecodealliance/StarlingMonkey/commit/c0873b74b728898dc91311fc75ea2f27c154f5ae))
* **url:** implement `URLSearchParams.size` property ([#296](https://github.com/bytecodealliance/StarlingMonkey/issues/296)) ([27d1d5d](https://github.com/bytecodealliance/StarlingMonkey/commit/27d1d5d5126c475d948aaadffed6c543758e3b73))


### Bug Fixes

* aarch64-&gt;arm64 in wasi-sdk download url ([#302](https://github.com/bytecodealliance/StarlingMonkey/issues/302)) ([6ff0d0d](https://github.com/bytecodealliance/StarlingMonkey/commit/6ff0d0d0bdf08192ecb7826a9f27cf3098980a80))
* **ci:** add weval artifacts to the release ([#304](https://github.com/bytecodealliance/StarlingMonkey/issues/304)) ([1614930](https://github.com/bytecodealliance/StarlingMonkey/commit/161493015f735195d4795cf07f025e44d26a5396))
* **ci:** Fix release-please setup ([#282](https://github.com/bytecodealliance/StarlingMonkey/issues/282)) ([9d9a34b](https://github.com/bytecodealliance/StarlingMonkey/commit/9d9a34b9a1de6c5fedb65d0a44b77677bf02b9bc))
* **ci:** Unify release-please CI workflows ([#284](https://github.com/bytecodealliance/StarlingMonkey/issues/284)) ([c5f80bd](https://github.com/bytecodealliance/StarlingMonkey/commit/c5f80bd82209913141723f14016ef4157daaf964))
* **debugger:** Fix path normalization in debugger sourcemaps handling ([#279](https://github.com/bytecodealliance/StarlingMonkey/issues/279)) ([afcf222](https://github.com/bytecodealliance/StarlingMonkey/commit/afcf222f512eb211d1b29d7a427fc8db6dd27f84))
* Prevent `URLSearchParams.toString()` from returning unencoded params when not modified ([#292](https://github.com/bytecodealliance/StarlingMonkey/issues/292)) ([a2ddae3](https://github.com/bytecodealliance/StarlingMonkey/commit/a2ddae3d463c61437fafc490308016fbc2a53cdd))
* **url:** allow `SpecSlice` to hold empty strings ([#293](https://github.com/bytecodealliance/StarlingMonkey/issues/293)) ([faa9114](https://github.com/bytecodealliance/StarlingMonkey/commit/faa9114ac16b3ba665d7863915f7edb866b91248))
* **url:** fix leaky `Drop`s for `SpecString` and `CVec` ([#294](https://github.com/bytecodealliance/StarlingMonkey/issues/294)) ([51d5388](https://github.com/bytecodealliance/StarlingMonkey/commit/51d5388c48811688329bb5fd2a13384574ac6cfd))
* **url:** make URLSearchParams record constructor spec compliant ([#295](https://github.com/bytecodealliance/StarlingMonkey/issues/295)) ([d591115](https://github.com/bytecodealliance/StarlingMonkey/commit/d59111599472cf2cd921cde323847824bea4a8a8))

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
