# My Barrier

`my_barrier` is a cross-platform LAN suite prototype for sharing keyboard/mouse input, transferring files directly between trusted devices, and synchronizing clipboard text across macOS, Windows, and Linux.

Current implementation: C++20 daemon/CLI MVP, with one shared wire protocol for macOS, Windows, and Linux builds.

## Implemented
- Persistent device identity in the local config directory.
- TCP daemon with a small framed protocol.
- UDP LAN discovery responder/client.
- Explicit pairing with a user-provided pairing code and per-peer shared secret.
- File-backed clipboard text sync.
- Direct file transfer with safe destination filename handling and content hash verification.
- Input event channel with authorization and logging-only adapter.
- Platform capability diagnostics.

## Not Yet Implemented
- Real encrypted transport. Pairing currently creates a shared secret, but the MVP transport is plaintext on LAN.
- Native GUI clipboard adapters.
- Real keyboard/mouse capture and injection. The input path is present, authorized, and logged, ready for native adapters.
- Desktop UI.

## Build

Current Ubuntu host output:

```text
build/mybarrier
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

Current Linux package output on this host:

```text
dist/mybarrier-linux-x86_64/mybarrier
dist/mybarrier-linux-x86_64.tar.gz
build/mybarrier-0.2.0-Linux-x86_64.deb
build/mybarrier-0.2.0-Linux-x86_64.tar.gz
build/mybarrier-0.2.0-Linux-x86_64.zip
```

macOS and Windows binaries must be built on macOS/Windows or by `.github/workflows/build.yml` CI runners. See `docs/github-ci.md` for GitHub Actions packaging steps.

## Basic Usage

Show local status:

```bash
./build/mybarrier status
```

Start a daemon that accepts pairing:

```bash
./build/mybarrier serve --name linux-box --port 37373 --pair-code 123456
```

Discover devices on the LAN:

```bash
./build/mybarrier discover
```

Pair with another daemon:

```bash
./build/mybarrier pair --host 192.168.1.20 --port 37373 --code 123456
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
