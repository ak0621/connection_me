# My Barrier Design

## Product Goal
Build a LAN-first cross-platform app that lets a user operate multiple macOS, Windows, and Linux machines as one workspace:

1. Share keyboard and mouse across devices.
2. Transfer files directly between trusted devices.
3. Synchronize clipboard content between trusted devices.

## Non-Goals For MVP
- Internet relay or cloud account system.
- NAT traversal outside the local network.
- Remote desktop video streaming.
- Multi-user enterprise policy management.

## Recommended Stack
- Core daemon: C++20.
- Build system: CMake.
- Desktop UI: Qt 6 is the natural C++ option; until Qt is installed, the MVP ships as daemon/CLI.
- Local IPC: Unix domain socket on macOS/Linux, named pipe on Windows.
- Wire format: the MVP uses a simple framed protocol; production should move to Protobuf or MessagePack with explicit schema versioning.
- Security: per-device identity, explicit pairing, per-peer shared secret in MVP, encrypted sessions in production.

Reasoning:
- Keyboard/mouse capture and injection need direct native OS APIs, and C++ integrates with those APIs cleanly on macOS, Windows, and Linux.
- Barrier/Synergy-style products commonly use C/C++ for exactly this reason.
- The current host already has GCC and CMake, so C++ is immediately buildable here.
- Platform-specific input and clipboard behavior can stay behind adapter interfaces.

Current host gap: Qt is not installed in this local shell, so the default local build skips the GUI. The project now has an optional Qt Widgets UI target that builds when Qt 6 Widgets is available, and CI requires that target for release artifacts.

## Runtime Model

```text
+-----------------------------+
| Desktop UI                  |
| - pairing                   |
| - device layout             |
| - transfer history          |
| - permission diagnostics    |
+--------------+--------------+
               |
               | local IPC
               v
+--------------+--------------+
| Background daemon           |
| - discovery                 |
| - pairing/trust store       |
| - encrypted peer channels   |
| - clipboard sync            |
| - file transfer             |
| - input event routing       |
+------+----------+-----------+
       |          |
       |          v
       |   platform adapters
       |   - input capture
       |   - input injection
       |   - clipboard
       |
       v
LAN peers over encrypted channels
```

Every device runs the daemon. The UI is optional after setup, but useful for pairing, permissions, layout, and diagnostics.

## Device Roles
- Controller: the device with the active keyboard/mouse.
- Target: the device receiving synthetic input.
- Peer: any paired device participating in file transfer or clipboard sync.

For MVP, one controller is active at a time. Later versions can support role handoff or multiple independent groups.

## LAN Discovery
Primary:
- mDNS service record, for example `_mybarrier._tcp.local`.

Fallback:
- UDP broadcast/multicast probe on each non-loopback LAN interface.

Discovery payload:
- device id hash;
- hostname and display name;
- daemon version;
- supported protocol version;
- listening port;
- pairing state flag.

Rules:
- Do not auto-trust discovered devices.
- Ignore Docker/loopback interfaces by default.
- Support multiple active interfaces.

## Pairing And Trust
Each device generates a long-term identity key on first launch.

Pairing flow:
1. Device A discovers Device B.
2. User initiates pairing in UI.
3. Devices exchange ephemeral keys.
4. UI shows a short numeric code or QR confirmation on both devices.
5. User confirms the code.
6. Devices store each other's identity public key and permissions.

Trust store fields:
- peer device id;
- peer public key;
- display name;
- permissions: input, clipboard, file receive, file send;
- last seen address;
- created/updated timestamps.

## Transport
Use one logical secure session per peer, with multiple typed channels:

- Control channel: reliable ordered messages for capabilities, permissions, heartbeats, layout changes.
- Clipboard channel: reliable messages with size limits and deduplication.
- File channel: reliable chunked transfer with resume, hashes, and temporary files.
- Input channel: low-latency event stream. It can initially run reliable; later optimize with QUIC datagrams or a prioritized stream.

MVP can start with TCP plus TLS or Noise-style encryption. QUIC is a good later target if low-latency input and multiplexing need stronger tuning.

## Keyboard And Mouse Sharing

### User Experience
- User arranges devices in a grid, similar to monitor layout.
- Moving the cursor past a configured screen edge switches control to the adjacent device.
- Modifier keys are released before switching to prevent stuck-key states.
- Emergency hotkey returns control to the local machine.

### Event Model
Messages should be normalized rather than sending raw OS structs:

```text
InputEvent {
  sequence_id
  source_device_id
  target_device_id
  timestamp_micros
  kind: key_down | key_up | pointer_move | pointer_button | scroll
  payload
}
```

The target adapter maps normalized events to native APIs.

### Platform Adapters
macOS:
- Capture: CGEventTap.
- Inject: CGEventPost.
- Permissions: Accessibility and Input Monitoring.
- Clipboard: NSPasteboard.

Windows:
- Capture: low-level keyboard/mouse hooks.
- Inject: SendInput.
- Clipboard: Win32 Clipboard API.
- Permissions: normal desktop app rights, with caveats around elevated apps and secure desktop.

Linux:
- X11 capture/inject: XInput2/XRecord plus XTest.
- Wayland: compositor restrictions apply. Prefer `/dev/uinput` for injection where permission is granted; capture may require compositor-specific protocols or reduced support.
- Clipboard: X11 selection APIs or Wayland data-control/portal paths depending on desktop.

## Clipboard Sync
Supported MVP types:
- UTF-8 text.
- HTML text as optional metadata.

Later:
- images;
- file lists;
- rich text;
- app-specific formats.

Loop prevention:
- each clipboard update carries `origin_device_id`, `content_hash`, and `generation`.
- peers do not re-broadcast content that originated from the same sync group.

Safety:
- configurable max size;
- optional "sync only text" mode;
- private-mode toggle to pause sync.

## File Transfer
MVP behavior:
- Send file or directory to a paired device.
- Receiver must accept or have an explicit auto-accept rule.
- Files stream in chunks.
- Each chunk has an offset and hash.
- Completed file is moved atomically from a temp path to the destination.

Transfer metadata:
- transfer id;
- source device id;
- destination device id;
- file name and relative path;
- total size;
- chunk size;
- content hash;
- resume token.

Security:
- never overwrite without confirmation unless a rule allows it;
- sanitize relative paths;
- reject path traversal;
- scan max size and free-space before accepting.

## Security Model
- All peer traffic is encrypted after pairing.
- Pairing requires explicit user confirmation.
- Device identity persists locally.
- Permissions are per peer and per feature.
- Input sharing is disabled by default until explicitly allowed.
- File receiving can require per-transfer confirmation.
- Clipboard sync can be paused globally or per peer.

Threats covered:
- passive LAN sniffing;
- accidental connection to untrusted devices;
- replayed stale messages;
- unauthorized file write attempts from unpaired devices.

Threats not solved in MVP:
- compromised paired devices;
- kernel-level keyloggers;
- malicious local admin/root users;
- secure desktop or login-screen input injection.

## Configuration
Suggested config layout:

macOS:
- `~/Library/Application Support/MyBarrier/`

Windows:
- `%APPDATA%/MyBarrier/`

Linux:
- `${XDG_CONFIG_HOME:-~/.config}/mybarrier/`

Files:
- `device.json`: local identity and display name.
- `peers.json`: trust store and permissions.
- `layout.json`: device adjacency and edge-switch settings.
- `settings.json`: feature toggles and limits.

## MVP Definition
MVP should prove the product in this order:

1. Daemon starts and exposes local status.
2. Devices discover each other on LAN.
3. Users pair two devices securely.
4. Text clipboard sync works.
5. File transfer works with progress and hash verification.
6. Keyboard/mouse sharing works on at least one platform pair.

For this host, start with steps 1-5 in headless mode. Full input validation needs a graphical session and platform permissions.
