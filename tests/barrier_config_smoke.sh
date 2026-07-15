#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./build/mybarrier}"
BASE="${TMPDIR:-/tmp}/mybarrier-config-smoke-$$"
HOME_DIR="$BASE/home"
mkdir -p "$HOME_DIR"
trap 'rm -rf "$BASE"' EXIT

MYBARRIER_HOME="$HOME_DIR" "$BIN" status --name alpha --port 38373 --role server >/dev/null
MYBARRIER_HOME="$HOME_DIR" "$BIN" layout-add --from alpha --side right --to beta --bidirectional >/tmp/mybarrier-config-layout.txt
MYBARRIER_HOME="$HOME_DIR" "$BIN" layout-list | grep -q "from=alpha side=right to=beta"
MYBARRIER_HOME="$HOME_DIR" "$BIN" layout-list | grep -q "from=beta side=left to=alpha"

CONFIG="$BASE/barrier.conf"
MYBARRIER_HOME="$HOME_DIR" "$BIN" config-export --path "$CONFIG" >/tmp/mybarrier-config-export.txt
grep -q "section: screens" "$CONFIG"
grep -q "section: links" "$CONFIG"
grep -q "right = beta" "$CONFIG"
grep -q "left = alpha" "$CONFIG"

IMPORTED="$BASE/imported"
mkdir -p "$IMPORTED"
MYBARRIER_HOME="$IMPORTED" "$BIN" config-import --path "$CONFIG" >/tmp/mybarrier-config-import.txt
MYBARRIER_HOME="$IMPORTED" "$BIN" layout-list | grep -q "from=alpha side=right to=beta"
MYBARRIER_HOME="$IMPORTED" "$BIN" layout-list | grep -q "from=beta side=left to=alpha"

echo "barrier config smoke ok"
