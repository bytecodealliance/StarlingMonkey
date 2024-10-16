# Adds StarlingMonkey as a CMake sub-project, initializes the correct toolchain, and
# exposes the `add_builtin` CMake function.

cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." ${CMAKE_BINARY_DIR}/starling-raw.wasm)

set(PATH_BACKUP CMAKE_MODULE_PATH)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
include("toolchain")
include("add_builtin")
set(CMAKE_MODULE_PATH PATH_BACKUP)
