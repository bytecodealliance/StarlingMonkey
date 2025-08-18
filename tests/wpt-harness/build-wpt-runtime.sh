#!/usr/bin/env bash

set -euo pipefail

componentize_flags="${COMPONENTIZE_FLAGS:-}"

if [ -z "${WPT_ROOT:-}" ]; then
  echo "The WPT_ROOT environment variable is not set"
  exit 1
fi

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

inputs=(
  "${script_dir}/pre-harness.js"
  "${WPT_ROOT}/resources/testharness.js"
  "${script_dir}/post-harness.js"
)

cat "${inputs[@]}" > wpt-test-runner.js
./componentize.sh $componentize_flags --verbose --wpt-mode --legacy-script wpt-test-runner.js wpt-runtime.wasm
