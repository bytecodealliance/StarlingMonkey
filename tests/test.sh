set -euo pipefail

test_component=$1
test_dir=$2

wasmtime="${WASMTIME:-wasmtime}"
test_name="$(basename $test_dir)"

test_body_expectation="$test_dir/expect_body.txt"
test_stdout_expectation="$test_dir/expect_stdout.txt"
test_stderr_expectation="$test_dir/expect_stderr.txt"
test_status_expectation=$(cat "$test_dir/expect_status.txt" 2> /dev/null || echo "200")

if [ ! -f $test_component ]; then
   echo "Test component $test_component does not exist."
   exit 1
fi

body_log="$test_dir/body.log"
stdout_log="$test_dir/stdout.log"
stderr_log="$test_dir/stderr.log"

$wasmtime serve -S common --addr 0.0.0.0:8181 $test_component 1> "$stdout_log" 2> "$stderr_log" &
wasmtime_pid="$!"

function cleanup {
   kill -9 ${wasmtime_pid}
}

trap cleanup EXIT

until cat $stderr_log | grep -m 1 "Serving HTTP" >/dev/null || ! ps -p ${wasmtime_pid} >/dev/null; do : ; done

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

status_code=$(curl --write-out %{http_code} --silent --output "$body_log" http://localhost:8181)

if [ ! "$status_code" = "$test_status_expectation" ]; then
   echo "Bad status code $status_code, expected $test_status_expectation"
   >&2 cat "$stderr_log"
   >&2 cat "$body_log"
   exit 1
fi

if [ -f $test_body_expectation ]; then
   cmp "$body_log" "$test_body_expectation"
fi

if [ -f $test_stdout_expectation ]; then
   cmp "$stdout_log" "$test_stdout_expectation"
fi

if [ -f $test_stderr_expectation ]; then
   tail -n +2 "$stderr_log" > "$stderr_log"
   cmp "$stderr_log" "$test_stderr_expectation"
fi

rm "$body_log"
rm "$stdout_log"
rm "$stderr_log"

trap '' EXIT
echo "Test Completed Successfully"
kill -9 ${wasmtime_pid}
exit 0
