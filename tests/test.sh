set -euo pipefail

test_component=$1
test_expectation=$2

output_dir=$(dirname $test_expectation)

if [ ! -f $test_component ]; then
   echo "Test component $test_component does not exist."
   exit 1
fi

if [ ! -f $test_expectation ]; then
   echo "Test expectation $test_expectation does not exist."
   exit 1
fi

wasmtime_log="$output_dir/wasmtime.log"
response_body_log="$output_dir/response.log"

wasmtime serve -S common --addr 0.0.0.0:8181 $test_component 2> "$wasmtime_log" &
wasmtime_pid="$!"

function cleanup {
   kill -9 ${wasmtime_pid}
}

trap cleanup EXIT

until cat $wasmtime_log | grep -m 1 "Serving HTTP" >/dev/null || ! ps -p ${wasmtime_pid} >/dev/null; do : ; done

if ! ps -p ${wasmtime_pid} >/dev/null; then
   echo "Wasmtime exited early"
   >&2 cat "$wasmtime_log"
   exit 1
fi

if ! cat "$wasmtime_log" | grep -m 1 "Serving HTTP"; then
   echo "Unexpected Wasmtime output"
   >&2 cat "$wasmtime_log"
   exit 1
fi

status_code=$(curl --write-out %{http_code} --silent --output "$response_body_log" http://localhost:8181)

if [ ! "$status_code" = "200" ]; then
   echo "Bad status code $status_code"
   >&2 cat "$wasmtime_log"
   >&2 cat "$response_body_log"
   exit 1
fi

cmp "$response_body_log" "$test_expectation"
rm "$response_body_log"

trap '' EXIT

echo "Test Completed Successfully"
kill -9 ${wasmtime_pid}
rm $wasmtime_log
exit 0
