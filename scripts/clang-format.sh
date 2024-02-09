#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<EOF
Usage: $0 [-h] [--fix] [--wasi-sdk]

  Format c/c++ source that's tracked by git. Exits with a non-zero return code
  when formatting is applied.

  -h          Print this message
  --fix       Format files in-place
  --wasi-sdk  Path to the wasi-sdk to use when invoking clang-format
EOF
}

fix=
clang_format=clang-format

while [ "$#" -gt 0 ] ; do
  case $1 in

    -h)
      usage
      exit 1
      ;;

    --fix)
      fix=1
      ;;

    --wasi-sdk)
      clang_format="$2/bin/clang-format"
      shift
      ;;

    *)
      echo "Unrecognized option: $1"
      echo
      usage
      exit 1
      ;;

  esac

  shift
done

cd "$(dirname "${BASH_SOURCE[0]}")/.."

failure=
for file in $(git ls-files | grep '\.\(cpp\|h\)$'); do
  if grep -F -x "$file" clang-format.ignore > /dev/null || grep -F -x "$(basename "$file")" clang-format.ignore > /dev/null; then
    echo "Ignoring ${file}"
    continue
  fi

  formatted="${file}.formatted"
  ${clang_format} "$file" > "$formatted"
  if ! cmp -s "$file" "$formatted"; then
    if [ -z "$fix" ]; then
      rm "$formatted"
      echo "${file} needs formatting"
      failure=1
    else
      echo "${file} formatted"
      mv "$formatted" "$file"
    fi
  fi

  rm -f "$formatted"
done

if [ -n "$failure" ]; then
  exit 1
fi
