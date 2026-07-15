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

The current `mybarrier` implementation is a working C++ MVP for LAN daemon basics, pairing, file-backed clipboard sync, direct file transfer, an authorized input-event channel, a Qt UI shell, and Barrier-style role/config/control-channel scaffolding. It is not yet equivalent to Barrier's actual native peripheral sharing implementation.

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
| Server/client role model | `src/lib/server/Server.h`, `src/lib/client/Client.h`, `src/lib/server/PrimaryClient.h` | `server` and `client` CLI commands map to role-specific daemon startup and optional pairing. | No active screen state machine, reconnect policy, or real edge-switch lifecycle yet. |
| Screen topology | `src/lib/server/Config.h` | `src/barrier_config.*` exports/imports Barrier-style `screens`, `links`, and `options`; `layout-add --bidirectional` creates reciprocal edges. | No GUI drag/drop grid persistence, jump zones, or full per-screen option model yet. |
| Native input capture | `src/lib/platform/*Screen.*`, `IPlatformScreen.h` | `src/platform.cpp` only reports planned capabilities. | No global keyboard/mouse hook, no cursor tracking, no hotkeys, no edge detection. |
| Native input injection | `IPlatformScreen.h`, `MSWindowsScreen.cpp`, `OSXScreen.mm`, `XWindowsScreen.cpp` | `INPUT_EVENT` is authorized then written to `input-events.log`. | No real key/mouse injection, key map, modifier handling, stuck-key prevention, relative/absolute mouse motion. |
| Protocol compatibility | `src/lib/barrier/protocol_types.h/.cpp` | Simple text frame now includes status metadata plus keepalive, screen-info, screen enter/leave, options, clipboard, file, and input-event commands. | Not wire-compatible with Barrier binary protocol and still lacks full key/mouse event semantics. |
| Clipboard | `Clipboard.*`, platform clipboard converters | File-backed `clipboard.txt` text only. | No native clipboard integration, no ownership/grab tracking, no multi-format text/HTML/image, no chunked streaming. |
| File transfer / drag-drop | `StreamChunker.*`, `FileChunk.*`, `DragInformation.*`, `DropHelper.*` | Whole file in one frame with FNV hash. | No chunk streaming, resume, cancel, directory recursion, drag/drop integration, large file controls. |
| Encryption | `SecureSocket.*`, `FingerprintDatabase.*`, GUI fingerprint flow | Pairing creates shared secret but transport is plaintext. | No TLS/Noise/QUIC encryption, no cert generation, no fingerprint trust database, no replay protection. |
| Discovery | `src/gui/src/Zeroconf*` | UDP broadcast responder/client. | No DNS-SD/mDNS zeroconf integration, no GUI auto-config records, limited multi-interface handling. |
| GUI | `src/gui` | Optional Qt Widgets UI shell exists for local settings, daemon control, pairing, permissions, layout, clipboard, and diagnostics. | No tray, setup wizard, native permission onboarding, fingerprint prompt, or service mode controls yet. |
| Process/service integration | `src/cmd/barriers`, `barrierc`, `barrierd`, GUI process control | Single `mybarrier` binary with `server` and `client` subcommands plus optional `MyBarrier` GUI. | No service install, tray receivers, or platform service manifests yet. |
| Event loop architecture | `src/lib/base`, `src/lib/mt`, `SocketMultiplexer` | Thread-per-client and blocking sockets. | No central event queue, timer system, socket multiplexer, structured async lifecycle. |
| Options and hotkeys | `option_types.h`, `InputFilter.*`, GUI dialogs | Peer permission toggles only. | No switch delay, lock cursor, keyboard broadcast, hotkey actions, per-screen options. |
| Screen saver / power/session handling | `IPlatformScreen.h`, platform screen saver classes | Not implemented. | No screen saver sync, suspend/resume, session switch handling. |
| Tests | Barrier has unit and GUI tests in several areas | Smoke tests for daemon basics. | No unit tests for protocol limits, crypto, path safety edge cases, topology, native adapters. |

## What Is Implemented Now

Current code covers these MVP pieces:
- `src/daemon.cpp`: daemon listener, UDP discovery, pair command, clipboard push, file send, input event logging, keepalive, screen-info, enter/leave, and options control messages.
- `src/config.h/.cpp`: local device identity, peer trust store, layout links, file-backed clipboard.
- `src/barrier_config.h/.cpp`: Barrier-style config export/import for screens, links, and options.
- `src/protocol.h/.cpp`: small framed protocol.
- `src/net.h/.cpp`: basic TCP/UDP socket wrappers.
- `src/platform.cpp`: platform capability diagnostics only.
- `src/gui`: optional Qt Widgets UI shell.
- `tests/smoke.sh`: pairing, clipboard sync, input authorization/logging, file transfer.
- `tests/discovery_smoke.sh`: UDP discovery.
- `tests/barrier_config_smoke.sh`: Barrier-style config export/import and bidirectional layout links.

## Priority To Reach Barrier-Like Functionality

1. Add active screen state, reconnect policy, and status events on top of the new `server`/`client` commands.
2. Extend screen topology config with coordinates, jump zones, emergency hotkey, and per-screen options.
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
