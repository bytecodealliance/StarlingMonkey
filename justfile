ncpus := num_cpus()
justdir := justfile_directory()
mode := 'debug'
builddir := justdir / 'cmake-build-' + mode
reconfigure := 'false'

alias b := build
alias t := test
alias w := wpt-test
alias c := componentize
alias fmt := format

# List all recipes
default:
    @echo 'Default mode {{ mode }}'
    @echo 'Default build directory {{ builddir }}'
    @just --list

# Build specified target or all otherwise
build target="all" *flags:
    #!/usr/bin/env bash
    set -euo pipefail
    echo 'Setting build directory to {{ builddir }}, build type {{ mode }}'

    # Only run configure step if build directory doesn't exist yet
    if ! {{ path_exists(builddir) }} || {{ reconfigure }} = 'true'; then
        cmake -S . -B {{ builddir }} {{ flags }} -DCMAKE_BUILD_TYPE={{ capitalize(mode) }}
    else
        echo 'build directory already exists, skipping cmake configure'
    fi

    # Build target
    cmake --build {{ builddir }} --parallel {{ ncpus }} {{ if target == "" { "" } else { "--target " + target } }}

# Run clean target
clean:
    cmake --build {{ builddir }} --target clean

[private]
[confirm('proceed?')]
do_clean:
    rm -rf {{ builddir }}

# Remove build directory
clean-all: && do_clean
    @echo "This will remove {{builddir}}"

# Componentize js script
componentize script="" outfile="starling.wasm": build
    {{ builddir }}/componentize.sh {{ script }} -o {{ outfile }}

# Componentize and serve script with wasmtime
serve script: (componentize script)
    wasmtime serve -S common starling.wasm

# Format code using clang-format. Use --fix to fix files inplace
format *ARGS:
    {{ justdir }}/scripts/clang-format.sh {{ ARGS }}

# Run integration test
test regex="": (build "integration-test-server")
    ctest --test-dir {{ builddir }} -j {{ ncpus }} --output-on-failure {{ if regex == "" { regex } else { "-R " + regex } }}

# Build web platform test suite
[group('wpt')]
wpt-build: (build "wpt-runtime" "-DENABLE_WPT:BOOL=ON")

# Run web platform test suite
[group('wpt')]
wpt-test filter="": wpt-build
    WPT_FILTER={{ filter }} ctest --test-dir {{ builddir }} -R wpt --verbose

# Update web platform test expectations
[group('wpt')]
wpt-update filter="": wpt-build
    WPT_FLAGS="--update-expectations" WPT_FILTER={{ filter }} ctest --test-dir {{ builddir }} -R wpt --verbose

# Run wpt server
[group('wpt')]
wpt-server: wpt-build
    #!/usr/bin/env bash
    set -euo pipefail
    cd {{ builddir }}
    wpt_root=$(grep '^CPM_PACKAGE_wpt-suite_SOURCE_DIR:INTERNAL=' CMakeCache.txt | cut -d'=' -f2-)

    echo "Using wpt-suite at ${wpt_root}"
    WASMTIME_BACKTRACE_DETAILS= node {{ justdir }}/tests/wpt-harness/run-wpt.mjs --wpt-root=${wpt_root} -vv --interactive

# Prepare WPT hosts
[group('wpt')]
wpt-setup:
    cat deps/wpt-hosts | sudo tee -a /etc/hosts
