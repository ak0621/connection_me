#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
GUI_MODE="${MYBARRIER_BUILD_GUI:-AUTO}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DMYBARRIER_BUILD_GUI="$GUI_MODE"
cmake --build "$BUILD_DIR" --config Release -j

case "$(uname -s)" in
  Linux*)
    cli="$BUILD_DIR/mybarrier"
    gui="$BUILD_DIR/MyBarrier"
    ;;
  Darwin*)
    cli="$BUILD_DIR/mybarrier"
    gui="$BUILD_DIR/MyBarrier.app"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    cli="$BUILD_DIR/Release/mybarrier.exe"
    gui="$BUILD_DIR/Release/MyBarrier.exe"
    if [[ ! -e "$cli" && -e "$BUILD_DIR/mybarrier.exe" ]]; then cli="$BUILD_DIR/mybarrier.exe"; fi
    if [[ ! -e "$gui" && -e "$BUILD_DIR/MyBarrier.exe" ]]; then gui="$BUILD_DIR/MyBarrier.exe"; fi
    ;;
  *)
    cli="$BUILD_DIR/mybarrier"
    gui="$BUILD_DIR/MyBarrier"
    ;;
esac

echo "Built CLI: $cli"
if [[ -e "$gui" ]]; then
  echo "Built desktop UI: $gui"
else
  echo "Desktop UI not built (MYBARRIER_BUILD_GUI=$GUI_MODE)"
fi
