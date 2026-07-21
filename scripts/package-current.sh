#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"
GUI_MODE="${MYBARRIER_BUILD_GUI:-AUTO}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DMYBARRIER_BUILD_GUI="$GUI_MODE"
cmake --build "$BUILD_DIR" --config Release -j
cmake --build "$BUILD_DIR" --target package --config Release

version="$(sed -n 's/^project(mybarrier VERSION \([^ ]*\).*/\1/p' CMakeLists.txt)"
if [[ -z "$version" ]]; then
  version="dev"
fi

os_raw="$(uname -s)"
arch_raw="$(uname -m)"
case "$os_raw" in
  Linux*)
    os="linux"
    cli="$BUILD_DIR/mybarrier"
    gui="$BUILD_DIR/MyBarrier"
    ;;
  Darwin*)
    os="macos"
    cli="$BUILD_DIR/mybarrier"
    gui="$BUILD_DIR/MyBarrier.app"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    os="windows"
    cli="$BUILD_DIR/Release/mybarrier.exe"
    gui="$BUILD_DIR/Release/MyBarrier.exe"
    if [[ ! -e "$cli" && -e "$BUILD_DIR/mybarrier.exe" ]]; then cli="$BUILD_DIR/mybarrier.exe"; fi
    if [[ ! -e "$gui" && -e "$BUILD_DIR/MyBarrier.exe" ]]; then gui="$BUILD_DIR/MyBarrier.exe"; fi
    ;;
  *)
    os="unknown"
    cli="$BUILD_DIR/mybarrier"
    gui="$BUILD_DIR/MyBarrier"
    ;;
esac
case "$arch_raw" in
  x86_64|amd64) arch="x86-64" ;;
  arm64|aarch64) arch="arm-64" ;;
  *) arch="${arch_raw//_/-}" ;;
esac

name="MyBarrier-$version-$os-$arch"
out="$DIST_DIR/$name"
mkdir -p "$out"

if [[ -e "$cli" ]]; then
  cp "$cli" "$out/"
fi
if [[ -d "$gui" ]]; then
  cp -R "$gui" "$out/"
elif [[ -e "$gui" ]]; then
  cp "$gui" "$out/"
fi
cp README.md "$out/"
cp docs/build-artifacts.md "$out/"
cp docs/implementation.md "$out/"

tar -C "$DIST_DIR" -czf "$DIST_DIR/$name.tar.gz" "$name"

echo "Packaged directory: $out"
if [[ -e "$gui" ]]; then
  echo "Runnable desktop UI included: $gui"
else
  echo "Desktop UI not included (MYBARRIER_BUILD_GUI=$GUI_MODE)"
fi
echo "Archive: $DIST_DIR/$name.tar.gz"
echo "CPack packages:"
find "$BUILD_DIR" -maxdepth 1 -type f -name 'MyBarrier-*' -print
