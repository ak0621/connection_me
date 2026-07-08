#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./build/mybarrier}"
BASE="${TMPDIR:-/tmp}/mybarrier-discovery-smoke-$$"
B="$BASE/b"
mkdir -p "$B"

MYBARRIER_HOME="$B" "$BIN" serve --name discover-beta --port 39374 --discovery-port 39375 --pair-code 123456 >"$BASE/server.log" 2>&1 &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null || true; rm -rf "$BASE"' EXIT
sleep 0.4

OUT=$(MYBARRIER_HOME="$BASE/a" "$BIN" discover --timeout 1000 --discovery-port 39375)
echo "$OUT"
grep -q 'name=discover-beta' <<<"$OUT"
echo "discovery smoke ok"
