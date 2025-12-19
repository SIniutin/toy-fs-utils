#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="./build"
mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR" -DCMAKE_C_COMPILER=gcc
cmake --build "$BUILD_DIR"
