# My Barrier

`my_barrier` is a cross-platform LAN suite prototype for sharing keyboard/mouse input, transferring files directly between trusted devices, and synchronizing clipboard text across macOS, Windows, and Linux.

Current implementation: C++20 daemon/CLI MVP plus an optional Qt Widgets desktop UI, with one shared wire protocol for macOS, Windows, and Linux builds.

## Implemented
- Persistent device identity in the local config directory.
- TCP daemon with a small framed protocol.
- UDP LAN discovery responder/client.
- Explicit pairing with a user-provided pairing code and per-peer shared secret.
- File-backed clipboard text sync.
- Direct file transfer with safe destination filename handling and content hash verification.
- Input event channel with authorization and logging-only adapter.
- Barrier-style `server` and `client` CLI entry points for role-specific startup.
- Barrier-style screen configuration export/import with `section: screens`, `section: links`, and `section: options`.
- Bidirectional screen layout links and control-channel messages for keepalive, screen info, enter/leave, and options.
- Platform capability diagnostics.
- Optional Qt Widgets desktop UI for local device settings, daemon control, pairing, peer permissions, layout, clipboard send, diagnostics, and a visible runtime log dock on Linux, macOS, and Windows.

## Not Yet Implemented
- Real encrypted transport. Pairing currently creates a shared secret, but the MVP transport is plaintext on LAN.
- Native GUI clipboard adapters.
- Real keyboard/mouse capture and injection. The input path is present, authorized, and logged, ready for native adapters.
- Native service integration and signed desktop installers.

## Build

Current Ubuntu host output:

```text
build/mybarrier
```

If Qt 6 Widgets is installed, CMake also builds the desktop UI:

```text
build/MyBarrier
```

Build on Linux/macOS:

```bash
scripts/build.sh
```

Build on Windows PowerShell:

```powershell
.\scripts\build.ps1
```

Package the current platform:

```bash
scripts/package-current.sh
```

Build a Linux AppImage on a Qt-enabled Linux host with `appimage-builder` installed:

```bash
scripts/package-appimage.sh
```

GUI build control:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMYBARRIER_BUILD_GUI=ON
cmake --build build --config Release --parallel
```

`MYBARRIER_BUILD_GUI=AUTO` is the default. It builds the Qt UI when Qt 6 Widgets is available and otherwise keeps the CLI build working. Use `ON` in CI or release builds to require the desktop app.

Current Linux package output on this host uses LocalSend-style, platform-explicit names:

```text
dist/MyBarrier-0.2.0-linux-x86-64/
dist/MyBarrier-0.2.0-linux-x86-64.tar.gz
build/MyBarrier-0.2.0-linux-x86-64.deb
build/MyBarrier-0.2.0-linux-x86-64.tar.gz
build/MyBarrier-0.2.0-linux-x86-64.zip
```

When Qt is available, the `dist/` directory contains both the CLI and the `MyBarrier` desktop UI. The desktop UI opens with a `Run Log` window that records startup, daemon output, CLI commands, stdout/stderr, and exit codes.

macOS and Windows binaries must be built on macOS/Windows or by `.github/workflows/build.yml` CI runners. The workflow installs Qt and requires the GUI build, so the generated runnable artifacts and installers contain the interactive `MyBarrier` desktop app. Linux CI also creates an AppImage from an AppDir recipe modeled after LocalSend. See `docs/github-ci.md` for GitHub Actions packaging steps.

## Basic Usage

Show local status:

```bash
./build/mybarrier status
```

Start a Barrier-style server that accepts pairing:

```bash
./build/mybarrier server --name linux-box --port 37373 --pair-code 123456 --write-config
```

Start a Barrier-style client and pair it with a server:

```bash
./build/mybarrier client --name laptop --host 192.168.1.20 --server-port 37373 --listen-port 37373 --code 123456
```

The lower-level daemon command is still available:

```bash
./build/mybarrier serve --name linux-box --port 37373 --role server --pair-code 123456
```

Discover devices on the LAN:

```bash
./build/mybarrier discover
```

Pair with another daemon:

```bash
./build/mybarrier pair --host 192.168.1.20 --port 37373 --code 123456
```

Create a Barrier-style screen link and export the generated config:

```bash
./build/mybarrier layout-add --from linux-box --side right --to laptop --bidirectional
./build/mybarrier config-export --path ./barrier.conf
```

Allow a paired peer to send input events on the receiving machine:

```bash
./build/mybarrier peer-permit --peer PEER_ID_OR_NAME --input 1
```

Send clipboard text:

```bash
./build/mybarrier clipboard-send --peer linux-box --text "hello"
```

Send a file:

```bash
./build/mybarrier send-file --peer linux-box --path ./example.txt
```

Run diagnostics:

```bash
./build/mybarrier diag
```

## Local Two-Daemon Test

Use separate config homes so one machine can simulate two devices:

```bash
MYBARRIER_HOME=/tmp/mb-a ./build/mybarrier status --name alpha --port 38373
MYBARRIER_HOME=/tmp/mb-b ./build/mybarrier serve --name beta --port 38374 --pair-code 123456
MYBARRIER_HOME=/tmp/mb-a ./build/mybarrier pair --host 127.0.0.1 --port 38374 --code 123456
MYBARRIER_HOME=/tmp/mb-a ./build/mybarrier clipboard-send --peer beta --text "hello beta"
MYBARRIER_HOME=/tmp/mb-b ./build/mybarrier clipboard-get
```

## Documents
- `docs/environment.md`: current host environment report.
- `docs/design.md`: product and system architecture.
- `docs/roadmap.md`: implementation phases and MVP plan.
- `docs/implementation.md`: implementation notes for this C++ MVP.
- `docs/build-artifacts.md`: platform build outputs and CI artifact names.
- `docs/github-ci.md`: GitHub repository and Actions packaging instructions.
