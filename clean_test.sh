rm -rf cmake-build-debug

cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug

cmake --build cmake-build-debug --target integration-test-server

CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir cmake-build-debug -j8
