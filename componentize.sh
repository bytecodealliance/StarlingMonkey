#!/usr/bin/env bash

#set -euo pipefail

wizer="${WIZER:-wizer}"
wasm_tools="${WASM_TOOLS:-wasm-tools}"
weval="${WEVAL:-@WEVAL_BIN@}"
aot=@AOT@
preopen_dir="${PREOPEN_DIR:-}"

usage() {
  echo "Usage: $(basename "$0") [input.js] [--verbose] [-o output.wasm]"
  echo "       Providing an input file but no output uses the input base name with a .wasm extension"
  echo "       Providing an output file but no input creates a component without running any top-level script"
  exit 1
}

if [ $# -lt 1 ]
then
  usage
fi

IN_FILE=""
OUT_FILE=""
VERBOSE=0

while [ $# -gt 0 ]
do
    case "$1" in
        -o|--output)
            OUT_FILE="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        *)
            if [ -n "$IN_FILE" ] && [ -z "$OUT_FILE" ] && [ $# -eq 1 ]
            then
              OUT_FILE="$1"
            else
              IN_FILE="$1"
            fi
            shift
            ;;
    esac
done

# Exit if neither input file nor output file is provided.
if [ -z "$IN_FILE" ] && [ -z "$OUT_FILE" ]
then
  usage
fi

# Use the -o param as output file if provided, otherwise use the input base name with a .wasm
# extension.
if [ -z "$OUT_FILE" ]
then
  BASENAME="$(basename "$IN_FILE")"
  OUT_FILE="${BASENAME%.*}.wasm"
fi

if [[ -n "$IN_FILE" ]]; then
  if [[ -n $preopen_dir ]]; then
    preopen_dir="--dir "$preopen_dir""
  else
    preopen_dir="--dir "$(dirname "$IN_FILE")""
  fi
  echo "Componentizing $IN_FILE into $OUT_FILE"
else
  echo "Creating runtime-script component $OUT_FILE"
fi

if [[ $aot -ne 0 ]]; then
    WEVAL_VERBOSE=""
    if [[ $VERBOSE -ne 0 ]]; then
        WEVAL_VERBOSE="--verbose --show-stats"
    fi

    echo "$IN_FILE" | WASMTIME_BACKTRACE_DETAILS=1 $weval weval -w $preopen_dir \
         --cache-ro "$(dirname "$0")/starling-ics.wevalcache" \
         $WEVAL_VERBOSE \
         -o "$OUT_FILE" \
         -i "$(dirname "$0")/starling.wasm"
else
    echo "$IN_FILE" | WASMTIME_BACKTRACE_DETAILS=1 $wizer --allow-wasi --wasm-bulk-memory true \
         --inherit-stdio true --inherit-env true $preopen_dir -o "$OUT_FILE" \
         -- "$(dirname "$0")/starling.wasm"
fi
$wasm_tools component new -v --adapt "wasi_snapshot_preview1=$(dirname "$0")/preview1-adapter.wasm" --output "$OUT_FILE" "$OUT_FILE"
