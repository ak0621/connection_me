# Host Environment Report

Date: 2026-07-07

## Workspace
- Workspace root: `/home/akun/work`
- Project directory: `/home/akun/work/my_barrier`
- `/home/akun/work` is not a git repository.
- `/home/akun/work/my_barrier` was empty before project planning files were created.

## Operating System
- OS: Ubuntu 24.04.4 LTS (Noble Numbat)
- Kernel: Linux 6.17.0-23-generic
- Architecture: x86_64

## Session / Desktop
- `XDG_SESSION_TYPE=tty`
- `DISPLAY` is unset.
- `WAYLAND_DISPLAY` is unset.
- `XDG_CURRENT_DESKTOP` is unset.

Impact: this session is not a graphical desktop session. GUI clipboard, global input capture, and input injection cannot be validated here. Headless components such as device discovery, pairing, protocol handling, file transfer, and daemon logic can still be designed and tested.

## Network
- Active LAN interfaces:
  - `enp195s0`: `172.20.108.87/24`
  - `wlp194s0`: `172.26.177.26/22`
- Docker bridge:
  - `docker0`: `172.17.0.1/16`, down for product LAN discovery purposes.

Design impact: discovery should support multiple interfaces and ignore loopback/docker interfaces by default unless explicitly enabled.

## Available Tools
- Node.js: 24.16.0
- npm: 11.13.0
- Python: 3.12.3
- GCC/G++: 13.3.0
- Clang: 14.0.6
- CMake: 3.28.3
- GNU Make: 4.3
- Git: 2.43.0
- OpenSSL: 3.0.13

## Missing Tools
- Rust/Cargo
- Go
- Ninja
- Qt/qmake
- pkg-config
- Avahi CLI tools
- `xdotool`
- `ydotool`
- `xclip`
- `wl-copy`

## Linux Input Constraints
- `/dev/uinput` exists as `root:root` with mode `0600`.
- Current user: `akun`
- Current groups include `sudo`, `plugdev`, and `docker`, but not a dedicated input/uinput group.

Impact: Linux input injection through `/dev/uinput` will need one of:
- a udev rule granting access to a dedicated group;
- a small privileged helper;
- an X11 backend using XTest where X11 is available;
- reduced functionality under Wayland unless appropriate permissions/protocols are available.

## Immediate Setup Gap
The current C++ MVP builds with the tools already available on this host: GCC/G++ and CMake.

For the next native desktop UI phase, install Qt 6 development packages. For Linux input injection, configure `/dev/uinput` access through a udev rule, dedicated group, or privileged helper.
