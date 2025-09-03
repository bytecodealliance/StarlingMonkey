# Testing StarlingMonkey

## Testing the build

After completing the build (a debug build in this case), the integration test runner can be built:

```console
cmake --build cmake-build-debug --target integration-test-server
```

Then tests can be run with `ctest` directly via:

```console
ctest --test-dir cmake-build-debug -j$(nproc) --output-on-failure
```

Alternatively, the integration test server can be directly run with `wasmtime serve` via:

```console
wasmtime serve -S common cmake-build-debug/test-server.wasm
```

Then visit `http://0.0.0.0:8080/timers`, or any test name and filter of the form `[testName]/[filter]`

## Web Platform Tests

To run the [Web Platform Tests](https://web-platform-tests.org/) suite, the WPT runner requires
`Node.js` above v18.0 to be installed, and the list of hosts in `deps/wpt-hosts` needs to be added to `etc/hosts`.

```console
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --parallel $(nproc) --target wpt-runtime
cat deps/wpt-hosts | sudo tee -a /etc/hosts # Required to resolve test server hostnames
cd cmake-build-debug
ctest -R wpt --verbose # Note: some of the tests run fairly slowly in debug builds, so be patient
```

The Web Platform Tests checkout can also be customized by setting the
`WPT_ROOT=[path to your WPT checkout]` environment variable to the cmake command.

WPT tests can be filtered with the `WPT_FILTER=string` variable, for example:

```console
WPT_FILTER=fetch ctest -R wpt -v
```

Custom flags can also be passed to the test runner via `WPT_FLAGS="..."`, for example to update
expectations use:

```console
WPT_FLAGS="--update-expectations" ctest -R wpt -v
```
