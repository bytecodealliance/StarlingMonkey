#!/usr/bin/env sh

OUT_FILE="$(basename "$1").wasm"
echo "$1" | WASMTIME_BACKTRACE_DETAILS=1 wizer --allow-wasi --wasm-bulk-memory true --inherit-stdio true --dir "$(dirname "$1")" -o "$OUT_FILE" -- "$(dirname "$0")/${RUNTIME_FILE}"
wasm-tools component new -v --adapt "wasi_snapshot_preview1=$(dirname "$0")/${ADAPTER_FILE}" --output "$OUT_FILE" "$OUT_FILE"
