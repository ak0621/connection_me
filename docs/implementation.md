# C++ MVP Implementation Notes

## Why C++ Here

C++ is a good fit for this product because keyboard/mouse capture and injection eventually need direct native APIs on macOS, Windows, and Linux. The current host already has GCC and CMake, while Rust/Cargo and Qt are missing, so the first runnable implementation is C++20 with no third-party dependencies.

## Current Architecture

- `src/main.cpp`: process entrypoint.
- `src/cli.*`: user-facing CLI commands.
- `src/daemon.*`: TCP daemon, UDP discovery, command handlers.
- `src/config.*`: device identity, trust store, local file-backed clipboard.
- `src/net.*`: small cross-platform socket wrapper.
- `src/protocol.*`: line/header/body framed protocol.
- `src/platform.*`: capability diagnostics and future native adapter boundary.

## Security Status

The MVP has explicit pairing and a per-peer shared secret. This protects daemon commands from unpaired peers but does not encrypt traffic yet. Production transport should add TLS, Noise, or QUIC with authenticated peer identities.

## Input Sharing Status

The input event channel exists and enforces peer authorization. On receipt, events are logged to `input-events.log`. Native injection should be added behind the platform adapter boundary:

- macOS: CGEventTap / CGEventPost.
- Windows: low-level hooks / SendInput.
- Linux X11: XInput/XRecord / XTest.
- Linux Wayland: likely `/dev/uinput` plus explicit permission setup or reduced support.

## Clipboard Status

The MVP uses a file-backed clipboard (`clipboard.txt`) so clipboard sync can be tested in a headless session. Native adapters should replace this for GUI sessions.


## Cross-Platform Build Status

The same source tree is configured to build on Linux, macOS, and Windows through CMake.

Implemented in this pass:
- CMake install and CPack package rules.
- Linux/macOS shell build and package scripts.
- Windows PowerShell build and package scripts.
- GitHub Actions matrix for Linux, macOS, and Windows artifacts.
- Runtime `artifact-info` command.

Current local artifact is Linux only because the host is Ubuntu. macOS and Windows artifacts require their corresponding OS runners.
