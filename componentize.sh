#!/usr/bin/env bash

#set -euo pipefail

wizer="${WIZER:-@WIZER_BIN@}"
wasm_tools="${WASM_TOOLS:-@WASM_TOOLS_BIN@}"
weval="${WEVAL:-@WEVAL_BIN@}"
aot=@AOT@
preopen_dir="${PREOPEN_DIR:-}"

usage() {
  echo "Usage: $(basename "$0")  [--verbose] [--legacy-script] [input.js] [-o output.wasm]"
  echo "       Providing an input file but no output uses the input base name with a .wasm extension"
  echo "       Providing an output file but no input creates a component without running any top-level script"
  echo "       Specifying '--legacy-script' causes evaluation as a legacy JS script instead of a module"
  exit 1
}

if [ $# -lt 1 ]
then
  usage
fi

IN_FILE=""
OUT_FILE=""
LEGACY_SCRIPT_PARAM=""
VERBOSE=0

while [ $# -gt 0 ]
do
    case "$1" in
        --legacy-script)
            LEGACY_SCRIPT_PARAM="$1 "
            IN_FILE="$2"
            shift 2
            ;;
        -o|--output)
            OUT_FILE="$2"
            shift 2
            ;;
        -v|--verbose)
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

  STARLING_ARGS="$LEGACY_SCRIPT_PARAM$IN_FILE"
  if [[ $VERBOSE -ne 0 ]]; then
      STARLING_ARGS="--verbose $STARLING_ARGS"
      echo "Componentizing with args $STARLING_ARGS"
  fi

  if [[ $aot -ne 0 ]]; then
      WEVAL_VERBOSE=""
      if [[ $VERBOSE -ne 0 ]]; then
          WEVAL_VERBOSE="--verbose --show-stats"
          echo "Using AOT compilation"
      fi

      echo "$STARLING_ARGS" | WASMTIME_BACKTRACE_DETAILS=1 $weval weval -w $preopen_dir \
           --cache-ro "$(dirname "$0")/starling-ics.wevalcache" \
           $WEVAL_VERBOSE \
           -o "$OUT_FILE" \
           -i "$(dirname "$0")/starling.wasm"
  else
      echo "$STARLING_ARGS" | WASMTIME_BACKTRACE_DETAILS=1 $wizer --allow-wasi --wasm-bulk-memory true \
           --inherit-stdio true --inherit-env true $preopen_dir -o "$OUT_FILE" \
           -- "$(dirname "$0")/starling.wasm"
  fi
else
  echo "Creating runtime-eval component $OUT_FILE"
  cp "$(dirname "$0")/starling.wasm" "$OUT_FILE"
fi

$wasm_tools component new -v --adapt "wasi_snapshot_preview1=$(dirname "$0")/preview1-adapter.wasm" --output "$OUT_FILE" "$OUT_FILE"
