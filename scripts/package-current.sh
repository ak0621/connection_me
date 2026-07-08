#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release -j
cmake --build "$BUILD_DIR" --target package --config Release

os_raw="$(uname -s)"
arch_raw="$(uname -m)"
case "$os_raw" in
  Linux*) os="linux"; exe="$BUILD_DIR/mybarrier" ;;
  Darwin*) os="macos"; exe="$BUILD_DIR/mybarrier" ;;
  MINGW*|MSYS*|CYGWIN*) os="windows"; exe="$BUILD_DIR/Release/mybarrier.exe" ;;
  *) os="unknown"; exe="$BUILD_DIR/mybarrier" ;;
esac
case "$arch_raw" in
  x86_64|amd64) arch="x86_64" ;;
  arm64|aarch64) arch="arm64" ;;
  *) arch="$arch_raw" ;;
esac

name="mybarrier-${os}-${arch}"
out="$DIST_DIR/$name"
mkdir -p "$out"
cp "$exe" "$out/"
cp README.md "$out/"
cp docs/build-artifacts.md "$out/"
cp docs/implementation.md "$out/"

tar -C "$DIST_DIR" -czf "$DIST_DIR/$name.tar.gz" "$name"

echo "Packaged directory: $out"
echo "Archive: $DIST_DIR/$name.tar.gz"
echo "CPack packages:"
find "$BUILD_DIR" -maxdepth 1 -type f -name 'mybarrier-*' -print
