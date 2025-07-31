#!/usr/bin/env bash

set -euo pipefail
set -x

# If WASI_SDK_PATH is set, use it to set up the compiler environment.
if [[ -n "${WASI_SDK_PATH:-}" ]]; then
  export CC="${WASI_SDK_PATH}/bin/clang"
  export CXX="${WASI_SDK_PATH}/bin/clang++"
  if [[ -z "${HOST_CC:-}" ]]; then
    export HOST_CC=$(which clang)
  fi
  if [[ -z "${HOST_CXX:-}" ]]; then
    export HOST_CXX=$(which clang++)
  fi
else
  # Otherwise, check that all required environment variables are set.
  if [[ -z "${WASI_SYSROOT:-}" ]]; then
      echo "Error: WASI_SYSROOT environment variable is not set."
      exit 1
  fi
  if [[ -z "${CC:-}" ]]; then
      echo "Error: CC environment variable must be set to a clang that can target wasm32-wasip1."
      exit 1
  fi
  if [[ -z "${CXX:-}" ]]; then
      echo "Error: CXX environment variable must be set to a clang++ that can target wasm32-wasip1."
      exit 1
  fi
  if [[ -z "${HOST_CC:-}" ]]; then
      echo "Error: HOST_CC environment variable is not set."
      exit 1
  fi
  if [[ -z "${HOST_CXX:-}" ]]; then
      echo "Error: HOST_CXX environment variable is not set."
      exit 1
  fi
fi

mode="${1:-release}"
weval=""
if [[ $# -gt 1 ]] && [[ "$2" == "weval" ]]; then
    weval=-weval
fi
mozconfig="$(pwd)/mozconfig-${mode}${weval}"
objdir="obj-$mode${weval}"
outdir="$mode${weval}"

cat << EOF > "$mozconfig"
ac_add_options --enable-project=js
ac_add_options --disable-js-shell # Prevents building Rust code, which we need to do ourselves anyway
ac_add_options --target=wasm32-unknown-wasi
ac_add_options --without-system-zlib
ac_add_options --without-intl-api
ac_add_options --disable-jit
ac_add_options --disable-shared-js
ac_add_options --disable-shared-memory
ac_add_options --disable-tests
ac_add_options --disable-clang-plugin
ac_add_options --enable-jitspew
ac_add_options --enable-optimize=-O3
ac_add_options --enable-js-streams
ac_add_options --enable-portable-baseline-interp
ac_add_options --prefix=${SM_OBJ_DIR}/dist
mk_add_options MOZ_OBJDIR=${SM_OBJ_DIR}
mk_add_options AUTOCLOBBER=1
EOF

if [[ -n "${WASI_SYSROOT:-}" ]]; then
    echo "ac_add_options --with-sysroot=\"${WASI_SYSROOT}\"" >> "$mozconfig"
fi

target="$(uname)"
case "$target" in
  Linux)
    echo "ac_add_options --disable-stdcxx-compat" >> "$mozconfig"
    platform="linux64-x64"
    ;;

  Darwin)
    echo "ac_add_options --host=aarch64-apple-darwin" >> "$mozconfig"
    platform="macosx64-aarch64"
    ;;

  *)
    echo "Unsupported build target: $target"
    exit 1
    ;;
esac

case "$mode" in
  release)
    echo "ac_add_options --disable-debug" >> "$mozconfig"
    ;;

  debug)
    echo "ac_add_options --enable-debug" >> "$mozconfig"
    ;;

  *)
    echo "Unknown build type: $mode"
    exit 1
    ;;
esac

case "$weval" in
  -weval)
    echo "ac_add_options --enable-portable-baseline-interp-force" >> "$mozconfig"
    echo "ac_add_options --enable-aot-ics" >> "$mozconfig"
    echo "ac_add_options --enable-aot-ics-force" >> "$mozconfig"
    echo "ac_add_options --enable-pbl-weval" >> "$mozconfig"
    ;;
esac

# Build SpiderMonkey for WASI
MOZCONFIG="${mozconfig}" python3 "${SM_SOURCE_DIR}/mach" --no-interactive build

cp -p "${SM_OBJ_DIR}/js/src/js-confdefs.h" "${SM_OBJ_DIR}/dist/include/"
