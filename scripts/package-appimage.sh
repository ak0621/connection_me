#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "AppImage packaging is only supported on Linux." >&2
  exit 2
fi

BUILD_DIR="${BUILD_DIR:-build}"
APPDIR="${APPDIR:-AppDir}"
RECIPE="${RECIPE:-packaging/appimage/AppImageBuilder_x86_64.yml}"
GUI_MODE="${MYBARRIER_BUILD_GUI:-ON}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DMYBARRIER_BUILD_GUI="$GUI_MODE"
cmake --build "$BUILD_DIR" --config Release -j

if [[ ! -x "$BUILD_DIR/MyBarrier" ]]; then
  echo "MyBarrier desktop UI was not built. Install Qt 6 Widgets or set MYBARRIER_BUILD_GUI=ON on a Qt-enabled host." >&2
  exit 2
fi

if ! command -v appimage-builder >/dev/null 2>&1; then
  echo "appimage-builder is required. In CI use AppImageCrafters/build-appimage with $RECIPE." >&2
  exit 2
fi

rm -rf "$APPDIR"
DESTDIR="$PWD/$APPDIR" cmake --install "$BUILD_DIR" --config Release --prefix /usr
appimage-builder --recipe "$RECIPE"
