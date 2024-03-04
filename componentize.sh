#!/usr/bin/env bash

set -euo pipefail

# Use $2 as output file if provided, otherwise use the input base name with a .wasm extension
if [ $# -gt 1 ]
then
  OUT_FILE="$2"
else
  BASENAME="$(basename "$1")"
  OUT_FILE="${BASENAME%.*}.wasm"
fi

echo "$1" | WASMTIME_BACKTRACE_DETAILS=1 wizer --allow-wasi --wasm-bulk-memory true --inherit-stdio true --dir "$(dirname "$1")" -o "$OUT_FILE" -- "$(dirname "$0")/starling.wasm"
wasm-tools component new -v --adapt "wasi_snapshot_preview1=$(dirname "$0")/preview1-adapter.wasm" --output "$OUT_FILE" "$OUT_FILE"
