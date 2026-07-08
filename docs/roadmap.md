# Implementation Roadmap

## Phase 0: Toolchain Setup
- Use the available C++20/GCC/CMake toolchain.
- Install Qt 6 later for the native desktop UI.
- Configure `/dev/uinput` permissions later for Linux input injection.
- Decide whether to initialize git in `/home/akun/work/my_barrier`.

## Phase 1: Core Daemon Skeleton
- Create C++20/CMake workspace.
- Add daemon/CLI binary.
- Add local config directory management.
- Add structured protocol and platform diagnostics.
- Add initial TCP daemon and UDP discovery.

Deliverable:
- `mybarrier status` returns device id, hostname, version, and enabled capabilities.

Status: implemented in the C++ MVP.

## Phase 2: Discovery And Pairing
- Implement mDNS discovery.
- Add UDP broadcast fallback.
- Generate local identity key.
- Implement pairing handshake with numeric confirmation.
- Persist peer trust store.

Deliverable:
- two devices can discover and pair on the same LAN.

## Phase 3: Clipboard Sync
- Define clipboard protocol messages.
- Implement text clipboard adapter per platform.
- Add loop prevention and size limits.
- Add pause/resume sync setting.

Deliverable:
- paired devices sync text clipboard content reliably.

## Phase 4: File Transfer
- Implement transfer metadata negotiation.
- Stream chunks with progress.
- Verify file hashes.
- Support cancel and resume.
- Add safe destination handling.

Deliverable:
- paired devices can send files and directories directly over LAN.

## Phase 5: Input Sharing
- Define normalized input event schema.
- Implement screen-edge switching and emergency return hotkey.
- Implement Linux X11 backend first if a GUI/X11 test session is available.
- Add macOS and Windows adapters.
- Add permission diagnostics.

Deliverable:
- one controller device can operate one target device across a configured screen edge.

## Phase 6: Desktop UI
- Build Tauri UI for:
  - discovered devices;
  - pairing;
  - permissions;
  - layout editor;
  - clipboard toggle;
  - file transfer history;
  - diagnostics.

Deliverable:
- users can configure and operate the suite without CLI commands.

## Phase 7: Packaging
- Linux: `.deb`/AppImage plus service integration.
- macOS: signed `.dmg` with permission onboarding.
- Windows: signed installer with background service option.

## Test Strategy
- Unit tests for protocol encoding, trust store, path sanitization, hash verification.
- Integration tests for daemon-to-daemon local loopback.
- LAN tests across two machines.
- Platform tests for input/clipboard adapters on each OS.
- Security tests for unpaired peer rejection, replay rejection, and malformed transfer paths.
