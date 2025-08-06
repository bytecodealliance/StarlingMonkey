 # Changelog

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

