#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./build/mybarrier}"
BASE="${TMPDIR:-/tmp}/mybarrier-smoke-$$"
A="$BASE/a"
B="$BASE/b"
mkdir -p "$A" "$B"

MYBARRIER_HOME="$A" "$BIN" status --name alpha --port 38373 --role client >/tmp/mybarrier-smoke-alpha.txt
MYBARRIER_HOME="$B" "$BIN" status --name beta --port 38374 --role server >/tmp/mybarrier-smoke-beta.txt
MYBARRIER_HOME="$A" "$BIN" layout-add --from alpha --side right --to beta >/tmp/mybarrier-smoke-layout.txt
MYBARRIER_HOME="$A" "$BIN" layout-list | grep -q "from=alpha side=right to=beta"

MYBARRIER_HOME="$B" "$BIN" serve --name beta --port 38374 --role server --discovery-port 38375 --pair-code 123456 >"$BASE/server.log" 2>&1 &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null || true; rm -rf "$BASE"' EXIT
sleep 0.4

MYBARRIER_HOME="$A" "$BIN" pair --host 127.0.0.1 --port 38374 --code 123456
MYBARRIER_HOME="$A" "$BIN" ping --peer beta | grep -q "Pong=1"
MYBARRIER_HOME="$A" "$BIN" clipboard-send --peer beta --text smoke-text
[[ "$(MYBARRIER_HOME="$B" "$BIN" clipboard-get)" == "smoke-text" ]]

CLIENT_ID=$(MYBARRIER_HOME="$A" "$BIN" status | awk -F= '/device_id/ {print $2}')
MYBARRIER_HOME="$B" "$BIN" peer-permit --peer "$CLIENT_ID" --input 1
MYBARRIER_HOME="$A" "$BIN" input-send --peer beta --type key_down --payload A >/tmp/mybarrier-smoke-input.txt
[[ -s "$B/input-events.log" ]]

echo 'hello file' >"$BASE/sample.txt"
MYBARRIER_HOME="$A" "$BIN" send-file --peer beta --path "$BASE/sample.txt" --chunk-size 4 >/tmp/mybarrier-smoke-file.txt
grep -q "Mode=chunked" /tmp/mybarrier-smoke-file.txt
[[ -f "$B/received/sample.txt" ]]

echo "smoke ok"
