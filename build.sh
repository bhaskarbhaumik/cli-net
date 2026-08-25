#!/usr/bin/env bash
set -euo pipefail

# Compiler detection: prefer clang (LLVM), fall back to gcc
if command -v clang &>/dev/null; then
    CC=clang
elif command -v gcc &>/dev/null; then
    CC=gcc
else
    echo "error: no C compiler found (tried clang, gcc)" >&2
    exit 1
fi

echo "compiler : $(command -v "$CC")"
echo "cmake    : $(cmake --version | head -1)"
echo

BUILD_DIR="${BUILD_DIR:-build}"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX:-/usr/local}"

cmake --build "$BUILD_DIR" --config Release

echo
echo "artifact : $BUILD_DIR/net"
