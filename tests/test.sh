set -euo pipefail

test_dir="$2"
test_runtime="$1"
test_component="${3:-}"
test_name="$(basename $test_dir)"
test_serve_path="${4:-}"
componentize_flags="${COMPONENTIZE_FLAGS:-}"

wasmtime="${WASMTIME:-wasmtime}"

# Load test expectation fails to check, only those defined apply
test_serve_body_expectation="$test_dir/expect_serve_body.txt"
test_serve_headers_expectation="$test_dir/expect_serve_headers.txt"
test_serve_stdout_expectation="$test_dir/expect_serve_stdout.txt"
test_serve_stderr_expectation="$test_dir/expect_serve_stderr.txt"
test_serve_status_expectation=$(cat "$test_dir/expect_serve_status.txt" 2> /dev/null || echo "200")

body_log="$test_dir/body.log"
headers_log="$test_dir/headers.log"
stdout_log="$test_dir/stdout.log"
stderr_log="$test_dir/stderr.log"

# Optionally create the test component if not explicitly provided
if [ -z "$test_component" ]; then
   test_component="$test_dir/$test_name.wasm"

   # Run Wizer
   set +e
   PREOPEN_DIR="$(dirname $(dirname "$test_dir"))" "$test_runtime/componentize.sh" $componentize_flags "$test_dir/$test_name.js" "$test_component" 1> "$stdout_log" 2> "$stderr_log"
   wizer_result=$?
   set -e

   # Check optional Wizer expectations, only those defined apply
   test_wizer_fail_expectation="$test_dir/expect_wizer_fail"
   test_wizer_stdout_expectation="$test_dir/expect_wizer_stdout.txt"
   test_wizer_stderr_expectation="$test_dir/expect_wizer_stderr.txt"

   if [ -f "$test_wizer_fail_expectation" ] && [ $wizer_result -eq 0 ]; then
      echo "Expected Wizer to fail, but it succeeded."
      exit 1
   elif [ ! -f "$test_wizer_fail_expectation" ] && [ ! $wizer_result -eq 0 ]; then
      echo "Wizering failed."
      >&2 cat "$stderr_log"
      >&2 cat "$stdout_log"
      exit 1
   fi

   if [ -f "$test_wizer_stdout_expectation" ]; then
      cmp "$stdout_log" "$test_wizer_stdout_expectation"
   fi

   if [ -f "$test_wizer_stderr_expectation" ]; then
      mv "$stderr_log" "$stderr_log.orig"
      cat "$stderr_log.orig" | head -n $(cat "$test_wizer_stderr_expectation" | wc -l) > "$stderr_log"
      rm "$stderr_log.orig"
      cmp "$stderr_log" "$test_wizer_stderr_expectation"
   fi

   if [ ! -f "$test_component" ] || [ ! -s "$test_component" ]; then
      if [ -f "$test_serve_body_expectation" ] || [ -f "$test_serve_stdout_expectation" ] || [ -f "$test_serve_stderr_expectation" ] || [ -f "$test_dir/expect_serve_status.txt" ]; then
         echo "Test component $test_component does not exist, cannot verify serve expectations."
         exit 1
      else
         echo "Test component $test_component does not exist, exiting."
         rm "$stdout_log"
         rm "$stderr_log"
         if [ -f "$test_component" ]; then
            rm "$test_component"
         fi
         exit 0
      fi   
   fi
fi

$wasmtime serve -S common --addr 0.0.0.0:0 "$test_component" 1> "$stdout_log" 2> "$stderr_log" &
wasmtime_pid="$!"

function cleanup {
   kill -9 ${wasmtime_pid}
}

trap cleanup EXIT

until cat "$stderr_log" | grep -m 1 "Serving HTTP" >/dev/null || ! ps -p ${wasmtime_pid} >/dev/null; do : ; done

if ! ps -p ${wasmtime_pid} >/dev/null; then
   echo "Wasmtime exited early"
   >&2 cat "$stderr_log"
   exit 1
fi

if ! cat "$stderr_log" | grep -m 1 "Serving HTTP"; then
   echo "Unexpected Wasmtime output"
   >&2 cat "$stderr_log"
   exit 1
fi

port=$(cat "$stderr_log" | head -n 1 | tail -c 7 | head -c 5)

status_code=$(curl -A "test-agent" -H "eXample-hEader: Header Value" --write-out %{http_code} --silent -D "$headers_log" --output "$body_log" "http://localhost:$port/$test_serve_path")

if [ ! "$status_code" = "$test_serve_status_expectation" ]; then
   echo "Bad status code $status_code, expected $test_serve_status_expectation"
   >&2 cat "$stderr_log"
   >&2 cat "$stdout_log"
   >&2 cat "$headers_log"
   >&2 cat "$body_log"
   exit 1
fi

if [ -f "$test_serve_headers_expectation" ]; then
   mv "$headers_log" "$headers_log.orig"
   cat "$headers_log.orig" | head -n $(cat "$test_serve_headers_expectation" | wc -l) | sed 's/\r//g' > "$headers_log"
   rm "$headers_log.orig"
   cmp "$headers_log" "$test_serve_headers_expectation"
fi

if [ -f "$test_serve_body_expectation" ]; then
   cmp "$body_log" "$test_serve_body_expectation"
fi

if [ -f "$test_serve_stdout_expectation" ]; then
   cmp "$stdout_log" "$test_serve_stdout_expectation"
fi

if [ -f "$test_serve_stderr_expectation" ]; then
   tail -n +2 "$stderr_log" > "$stderr_log"
   cmp "$stderr_log" "$test_serve_stderr_expectation"
fi

rm "$body_log"
rm "$headers_log"
rm "$stdout_log"
rm "$stderr_log"

trap '' EXIT
echo "Test Completed Successfully"
kill -9 ${wasmtime_pid}
exit 0
