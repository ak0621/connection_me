# Barrier Source Gap Analysis

Date: 2026-07-07

## Source Reviewed
- Repository: https://github.com/debauchee/barrier.git
- Local clone: `/tmp/barrier-source`
- Commit: `653e4badeb88f61de901581667d4465d7b1e2d52`

Key Barrier areas inspected:
- `src/lib/barrier`: protocol, screen abstractions, clipboard, key/mouse types, stream chunking, drag/drop.
- `src/lib/server`: server algorithm, screen graph config, client proxies, primary client, input filter.
- `src/lib/client`: client connection, server proxy, local screen integration.
- `src/lib/platform`: native Windows/macOS/X11 clipboard, input, screen, key state, drag/drop adapters.
- `src/lib/net`: TCP sockets, secure sockets, certificate fingerprints.
- `src/gui`: Qt GUI, config editor, zeroconf, SSL fingerprint UI, process control.

## High-Level Conclusion

The current `mybarrier` implementation is a working C++ MVP for LAN daemon basics, pairing, file-backed clipboard sync, direct file transfer, and an authorized input-event channel. It is not yet equivalent to Barrier's actual peripheral sharing implementation.

Barrier's real product core is built around:
- a primary/secondary screen model;
- screen graph configuration and edge switching;
- platform screen adapters that capture and synthesize keyboard/mouse events;
- a versioned binary protocol with screen enter/leave, key/mouse, clipboard, file, drag/drop, keepalive, and options messages;
- TLS/certificate fingerprint verification;
- Qt GUI and service/process control.

## Gap Matrix

| Area | Barrier Source Reference | Current Implementation | Missing / Risk |
|------|--------------------------|------------------------|----------------|
| Server/client role model | `src/lib/server/Server.h`, `src/lib/client/Client.h`, `src/lib/server/PrimaryClient.h` | Single daemon handles generic commands in `src/daemon.cpp`. | No primary/secondary role lifecycle, no reconnecting client, no active screen state machine. |
| Screen topology | `src/lib/server/Config.h` | Peer store only has id/name/host/port/permissions in `src/config.h`. | No screen grid, edge links, aliases, per-screen options, jump zones, switch delays. |
| Native input capture | `src/lib/platform/*Screen.*`, `IPlatformScreen.h` | `src/platform.cpp` only reports planned capabilities. | No global keyboard/mouse hook, no cursor tracking, no hotkeys, no edge detection. |
| Native input injection | `IPlatformScreen.h`, `MSWindowsScreen.cpp`, `OSXScreen.mm`, `XWindowsScreen.cpp` | `INPUT_EVENT` is authorized then written to `input-events.log`. | No real key/mouse injection, key map, modifier handling, stuck-key prevention, relative/absolute mouse motion. |
| Protocol compatibility | `src/lib/barrier/protocol_types.h/.cpp` | Simple text frame: command + headers + body. | No Barrier protocol handshake, version negotiation, keepalive, enter/leave, key/mouse message types, option messages. |
| Clipboard | `Clipboard.*`, platform clipboard converters | File-backed `clipboard.txt` text only. | No native clipboard integration, no ownership/grab tracking, no multi-format text/HTML/image, no chunked streaming. |
| File transfer / drag-drop | `StreamChunker.*`, `FileChunk.*`, `DragInformation.*`, `DropHelper.*` | Whole file in one frame with FNV hash. | No chunk streaming, resume, cancel, directory recursion, drag/drop integration, large file controls. |
| Encryption | `SecureSocket.*`, `FingerprintDatabase.*`, GUI fingerprint flow | Pairing creates shared secret but transport is plaintext. | No TLS/Noise/QUIC encryption, no cert generation, no fingerprint trust database, no replay protection. |
| Discovery | `src/gui/src/Zeroconf*` | UDP broadcast responder/client. | No DNS-SD/mDNS zeroconf integration, no GUI auto-config records, limited multi-interface handling. |
| GUI | `src/gui` | CLI only. | No Qt UI, tray, setup wizard, screen layout editor, logs, fingerprint prompt, service mode controls. |
| Process/service integration | `src/cmd/barriers`, `barrierc`, `barrierd`, GUI process control | Single `mybarrier` binary. | No separate server/client commands, no daemon/service install, no tray receivers, no platform manifests. |
| Event loop architecture | `src/lib/base`, `src/lib/mt`, `SocketMultiplexer` | Thread-per-client and blocking sockets. | No central event queue, timer system, socket multiplexer, structured async lifecycle. |
| Options and hotkeys | `option_types.h`, `InputFilter.*`, GUI dialogs | Peer permission toggles only. | No switch delay, lock cursor, keyboard broadcast, hotkey actions, per-screen options. |
| Screen saver / power/session handling | `IPlatformScreen.h`, platform screen saver classes | Not implemented. | No screen saver sync, suspend/resume, session switch handling. |
| Tests | Barrier has unit and GUI tests in several areas | Smoke tests for daemon basics. | No unit tests for protocol limits, crypto, path safety edge cases, topology, native adapters. |

## What Is Implemented Now

Current code covers these MVP pieces:
- `src/daemon.cpp`: daemon listener, UDP discovery, pair command, clipboard push, file send, input event logging.
- `src/config.h/.cpp`: local device identity, peer trust store, file-backed clipboard.
- `src/protocol.h/.cpp`: small framed protocol.
- `src/net.h/.cpp`: basic TCP/UDP socket wrappers.
- `src/platform.cpp`: platform capability diagnostics only.
- `tests/smoke.sh`: pairing, clipboard sync, input authorization/logging, file transfer.
- `tests/discovery_smoke.sh`: UDP discovery.

## Priority To Reach Barrier-Like Functionality

1. Add a real role model: `server`/`client` mode, active peer, reconnect, status events.
2. Add screen topology config: devices, edges, coordinates, jump zones, emergency hotkey.
3. Replace `INPUT_EVENT` logging with native adapter interfaces and implement Linux X11 first.
4. Add native clipboard adapters for Linux X11/Wayland, Windows, and macOS.
5. Add encrypted transport and peer identity verification.
6. Replace whole-file transfer with chunked streaming and progress/cancel.
7. Add mDNS/DNS-SD discovery in addition to UDP fallback.
8. Add Qt UI for pairing, permissions, screen layout, logs, diagnostics, and trust prompts.
9. Add packaging/service integration per OS.

## Immediate Next Implementation Slice

The next smallest high-value slice should be Linux X11 keyboard/mouse path:
- define `PlatformScreen` interface similar in spirit to Barrier's `IPlatformScreen` but smaller;
- implement X11 diagnostics and dependency detection;
- implement XTest-based fake mouse/key events;
- implement edge detection for one local controller screen;
- map normalized `INPUT_EVENT` messages to real X11 injection.

This requires a graphical X11 session and development libraries, which this host currently does not expose.
