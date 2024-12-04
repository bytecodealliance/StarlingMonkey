ncpus := num_cpus()
justdir := justfile_directory()
flavour := 'debug'
builddir := justdir / 'cmake-build-' + flavour
force := 'false'

alias b := build
alias t := integration-test
alias w := wpt-test
alias c := componentize
alias fmt := format

# List all recipes
default:
    @echo 'Default flavour {{ flavour }}'
    @echo 'Default build directory {{ builddir }}'
    @just --list

# Build specified target or all otherwise
build target="" flags="":
    #!/usr/bin/env bash
    set -euo pipefail
    echo 'Setting build directory to {{ builddir }}, build type {{ flavour }}'

    # Only run configure step if build directory doesn't exist yet
    if ! {{ path_exists(builddir) }} || {{ force }} = 'true'; then
        cmake -S . -B {{ builddir }} {{ flags }} -DCMAKE_BUILD_TYPE={{ capitalize(flavour) }}
    else
        echo 'build directory already exists, skipping cmake configure'
    fi

    # Build target
    cmake --build {{ builddir }} --parallel {{ ncpus }} {{ if target == "" { target } else { "--target " + target } }}

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

# Format code using clang-format. Use --fix to fix files inplace
format *ARGS:
    {{ justdir }}/scripts/clang-format.sh {{ ARGS }}

# Run integration test
integration-test: (build "integration-test-server")
    ctest --test-dir {{ builddir }} -j {{ ncpus }} --output-on-failure

# Build web platform test suite
[group('wpt')]
wpt-build: (build "wpt-runtime" "-DENABLE_WPT:BOOL=ON")

# Run web platform test suite
[group('wpt')]
wpt-test filter="": wpt-build
    WPT_FILTER={{ filter }} ctest --test-dir {{ builddir }} -R wpt --verbose

# Update web platform test expectations
[group('wpt')]
wpt-update: wpt-build
    WPT_FLAGS="--update-expectations" ctest --test-dir {{ builddir }} -R wpt --verbose

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
