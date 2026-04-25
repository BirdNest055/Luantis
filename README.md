<div align="center">
    <img src="textures/base/pack/logo.png" width="32%">
    <h1>Clawtest</h1>
    <p><em>A fork of Luanti (formerly Minetest) with real encrypted communications</em></p>
    <img src="https://img.shields.io/badge/version-v9.3-blue.svg" alt="Version">
    <a href="https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html"><img src="https://img.shields.io/badge/license-LGPLv2.1%2B-blue.svg" alt="License"></a>
    <img src="https://img.shields.io/badge/encryption-AES--256--GCM-green.svg" alt="Encryption">
    <img src="https://img.shields.io/badge/auth-SRP-orange.svg" alt="Auth">
</div>
<br>

Clawtest is a fork of the [Luanti](https://github.com/luanti-org/luanti) voxel game engine with **real, verifiable encrypted communications** between server and client. Unlike the upstream Luanti, where `secure_connection` was security theater (flags without actual encryption), Clawtest implements **AES-256-GCM packet encryption** derived from the SRP authentication session key, with honest UI that accurately reports what protection exists and what does not.

Copyright (C) 2010-2026 Perttu Ahola <celeron55@gmail.com>
and contributors (see source file comments and the version control log)

Table of Contents
------------------

1. [What's Different from Luanti](#whats-different-from-luanti)
2. [Encryption Architecture](#encryption-architecture)
3. [Quick Start](#quick-start)
4. [Default Controls](#default-controls)
5. [Paths](#paths)
6. [Configuration File](#configuration-file)
7. [Command-line Options](#command-line-options)
8. [Compiling](#compiling)
9. [Docker](#docker)
10. [Version Scheme](#version-scheme)


What's Different from Luanti
-----------------------------

Clawtest extends Luanti v5.16.0-dev with these major features:

| Feature | Version | Description |
|---------|---------|-------------|
| Real AES-256-GCM encryption | v9.0+ | SRP session key derives AES-256-GCM keys via HKDF; all post-auth traffic is encrypted |
| Modular encryption toggle | v9.3+ | `encryption_config.h/cpp` — centralized policy manager, single point of truth for encryption decisions |
| Interactive start scripts | v9.3+ | `start_server.sh` and `start_client.sh` with menus for secure/insecure mode, player name, port, etc. |
| Honest security UI | v7+ | Security overlay accurately reports connection state; score of 70/100 honestly (no forward secrecy, no PKI) |
| Security info settings tab | v8+ | Dedicated tab showing cipher, key exchange, auth method, replay protection, and limitations |
| Insecure mode actually works | v9.2+ | When `secure_connection = false`, encryption is genuinely disabled (SRP still runs for auth, but no AES-GCM) |
| Version numbering | v9.3+ | `VERSION` file, `VERSION_EXTRA` in CMake, versioned zip filenames |

Encryption Architecture
-----------------------

### How It Works

1. **SRP Authentication**: Client and server authenticate via SRP (Secure Remote Password) protocol. SRP produces a 32-byte shared session key on both sides.
2. **Key Derivation**: When `secure_connection = true`, the SRP session key is fed through HKDF-SHA256 to derive separate C2S and S2C AES-256-GCM keys, plus nonce bases.
3. **Packet Encryption**: After auth, all game traffic is encrypted with AES-256-GCM. Each packet includes a 12-byte nonce and 16-byte GCM authentication tag.
4. **Replay Protection**: Monotonic nonce counters with sliding window prevent replay attacks.
5. **Insecure Mode**: When `secure_connection = false`, SRP still runs (for password verification) but the session key is NOT used for encryption. All traffic is plaintext. This is useful for LAN testing or debugging.

### What This Gives Us (REAL)

- Packet encryption: AES-256-GCM encrypts all post-auth game traffic
- Authentication: SRP proves both sides know the password
- Integrity: GCM authentication tag detects tampering
- Replay protection: Monotonic nonce counters with sliding window
- Key separation: HKDF derives separate C2S and S2C keys
- Wireshark-proof: Captured packets show ciphertext, not game data

### Honest Limitations

- No forward secrecy: Key is derived from password, not ephemeral ECDH. If the password is compromised, past sessions can be decrypted.
- No certificate-based trust: Uses "Trust On First Use" (TOFU) model with SRP verifier hash.
- No quantum resistance: AES-256 is believed quantum-resistant, but SRP is not.

### Security Score: 70/100 (Honest)

| Property | Points | Status |
|----------|--------|--------|
| Encryption active | +30 | AES-256-GCM |
| Strong cipher suite | +15 | AES-256-GCM |
| Forward secrecy | +0 | Not available (SRP-derived) |
| Authentication | +15 | SRP password auth |
| Replay protection | +10 | Nonce counters + sliding window |
| Certificate verification | +0 | TOFU only |
| TLS version | +0 | Custom protocol |
| **Total** | **70** | **Good (honestly)** |


Quick Start
-----------

### Server

```bash
./start_server.sh
```

Interactive menu lets you choose: secure/insecure mode, port, game, world, admin player, max players, MOTD, and run mode (foreground/background/screen).

Or use CLI flags:
```bash
./start_server.sh --insecure --port 30001 --game devtest --go
./start_server.sh --secure --port 30000 --game minetest --go
```

### Client

```bash
./start_client.sh
```

Interactive menu lets you choose: server address, port, player name, password, secure/insecure mode, display mode, and resolution.

Or use CLI flags:
```bash
./start_client.sh --insecure --name player1 --address localhost --go
./start_client.sh --secure --name admin --address 192.168.1.100 --go
```


Default controls
----------------
All controls are re-bindable using settings.
Some can be changed in the key config dialog in the settings tab.

| Button                        | Action                                                         |
|-------------------------------|----------------------------------------------------------------|
| Move mouse                    | Look around                                                    |
| W, A, S, D                    | Move                                                           |
| Space                         | Jump/move up                                                   |
| Shift                         | Sneak/move down                                                |
| Q                             | Drop itemstack                                                 |
| Shift + Q                     | Drop single item                                               |
| Left mouse button             | Dig/punch/use                                                  |
| Right mouse button            | Place/use                                                      |
| Shift + right mouse button    | Build (without using)                                          |
| I                             | Inventory menu                                                 |
| Mouse wheel                   | Select item                                                    |
| 0-9                           | Select item                                                    |
| Z                             | Zoom (needs zoom privilege)                                    |
| T                             | Chat                                                           |
| /                             | Command                                                        |
| Esc                           | Pause menu/abort/exit (pauses only singleplayer game)          |
| +                             | Increase view range                                            |
| -                             | Decrease view range                                            |
| K                             | Enable/disable fly mode (needs fly privilege)                  |
| J                             | Enable/disable fast mode (needs fast privilege)                |
| H                             | Enable/disable noclip mode (needs noclip privilege)            |
| E                             | Aux1 (Move fast in fast mode. Games may add special features)  |
| C                             | Cycle through camera modes                                     |
| V                             | Cycle through minimap modes                                    |
| Shift + V                     | Change minimap orientation                                     |
| F1                            | Hide/show HUD                                                  |
| F2                            | Hide/show chat                                                 |
| F3                            | Disable/enable fog                                             |
| F4                            | Disable/enable camera update (Mapblocks are not updated anymore when disabled, disabled in release builds)  |
| F5                            | Cycle through debug information screens                        |
| F6                            | Cycle through profiler info screens                            |
| F10                           | Show/hide console                                              |
| F12                           | Take screenshot                                                |

Paths
-----
Locations:

* `bin`   - Compiled binaries
* `share` - Distributed read-only data
* `user`  - User-created modifiable data

Where each location is on each platform:

* Windows .zip / RUN_IN_PLACE source:
    * `bin`   = `bin`
    * `share` = `.`
    * `user`  = `.`
* Windows installed:
    * `bin`   = `C:\Program Files\Minetest\bin (Depends on the install location)`
    * `share` = `C:\Program Files\Minetest (Depends on the install location)`
    * `user`  = `%APPDATA%\Minetest` or `%MINETEST_USER_PATH%`
* Linux installed:
    * `bin`   = `/usr/bin`
    * `share` = `/usr/share/minetest`
    * `user`  = `~/.minetest` or `$MINETEST_USER_PATH`
* macOS:
    * `bin`   = `Contents/MacOS`
    * `share` = `Contents/Resources`
    * `user`  = `Contents/User` or `~/Library/Application Support/minetest` or `$MINETEST_USER_PATH`

Worlds can be found as separate folders in: `user/worlds/`

Configuration file
------------------
- Default location:
    `user/minetest.conf`
- This file is created by closing Luanti for the first time.
- A specific file can be specified on the command line:
    `--config <path-to-file>`
- A run-in-place build will look for the configuration file in
    `location_of_exe/../minetest.conf` and also `location_of_exe/../../minetest.conf`

Key Clawtest settings:
- `secure_connection` (default: `true`) — When true, AES-256-GCM encryption is activated after SRP auth. When false, SRP still runs for authentication but traffic is plaintext.
- `show_security_overlay` (default: `true`) — Show security status overlay in-game.
- `show_connection_info` (default: `false`) — Show detailed connection info in settings tab.

Command-line options
--------------------
- Use `--help`

Compiling
---------

- [Compiling - common information](doc/compiling/README.md)
- [Compiling on GNU/Linux](doc/compiling/linux.md)
- [Compiling on Windows](doc/compiling/windows.md)
- [Compiling on MacOS](doc/compiling/macos.md)

Quick build (Linux, run-in-place):
```bash
./build_linux.sh --non-interactive --no-deps --client --run-in-place
```

Docker
------

- [Developing minetestserver with Docker](doc/developing/docker.md)
- [Running a server with Docker](doc/docker_server.md)

Version scheme
--------------

Clawtest uses a dual version scheme:

1. **Engine version** (from upstream Luanti): `major.minor.patch` (currently 5.16.1)
2. **Clawtest version** (encryption feature version): `v9.X` (currently v9.3)

The full version string is `5.16.1-v9.3-dev`, displayed via `--version` and in the UI.

The Clawtest version tracks encryption feature development:
- v7: Secure connection overlay + settings toggle
- v8: Security Info settings tab with technical connection details
- v9.0: Real AES-256-GCM encryption implemented and integrated
- v9.2: Insecure mode actually disables encryption (not just UI flags)
- v9.3: Modular encryption architecture, interactive start scripts, version numbering

Git tags follow the pattern `clawtest-v9.3`, `clawtest-v9.4`, etc.
