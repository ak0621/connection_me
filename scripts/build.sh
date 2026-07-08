#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release -j

case "$(uname -s)" in
  Linux*)   exe="$BUILD_DIR/mybarrier" ;;
  Darwin*)  exe="$BUILD_DIR/mybarrier" ;;
  MINGW*|MSYS*|CYGWIN*) exe="$BUILD_DIR/Release/mybarrier.exe" ;;
  *) exe="$BUILD_DIR/mybarrier" ;;
esac

echo "Built: $exe"
