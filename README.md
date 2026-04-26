<div align="center">
    <img src="textures/base/pack/logo.png" width="32%">
    <h1>Clawtest</h1>
    <p><em>A fork of Luanti (formerly Minetest) with real encrypted communications</em></p>
    <img src="https://img.shields.io/badge/version-v9.25-blue.svg" alt="Version">
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
| Portable build system | v9.6+ | No hardcoded paths — `build_env.sh` and `build_linux.sh` auto-detect `local-prefix/`, support `--local-prefix` flag, work on any Ubuntu/Debian machine |
| Consolidated CI | v9.5+ | Single GitHub Actions workflow with toggleable options (build, test, lint, package) |
| ECDH X25519 forward secrecy | v9.11+ | Real forward secrecy via ephemeral X25519 key exchange after SRP auth; protocol equivalent to TLS 1.3 |
| Bonus encryption scoring | v9.9+ | TOFU acknowledged (+3), key rotation capable (+5), salted HKDF (+2), exact replay bitmap (+2), integrity verified (+3) — first-connection score up to 85/100, returning 100/100 |
| VS Code tasks | v9.8+ | `.vscode/tasks.json` with Start Server, Build Both, Start Client tasks |
| Build error fixes | v9.9+ | Fixed `i64`→`s64` type alias, `populateRealSecurityInfo` overload ordering, deterministic HKDF salt derivation |
| Settings panel fix | v9.22+ | All 16 g_settings keys written, activated_at synced from connection layer, both secure and insecure modes work |
| Log toggle | v9.23+ | `--no-log`/`--log` flags in start scripts, `encryption_log_level` setting (none/error/action/trace), prevents 180MB log file problem |
| Settingtypes context fix | v9.24+ | Fixed `encryption_log_level` context from `[server,client]` to `[common]` (parser only accepts single context values) |
| Encryption log autocreate | v9.25+ | `encryption_trace.log` is now created at any non-none log level (not just trace); all `enclog_*` macros write to the trace file; fixes missing log file after manual deletion |

Encryption Architecture
-----------------------

### How It Works

1. **SRP Authentication**: Client and server authenticate via SRP (Secure Remote Password) protocol. SRP produces a 32-byte shared session key on both sides.
2. **Key Derivation**: When `secure_connection = true`, the SRP session key is fed through HKDF-SHA256 with a derived salt to produce separate C2S and S2C AES-256-GCM keys, plus nonce bases. The salt is deterministically derived from the SRP session key itself, ensuring both sides produce identical keys without a wire exchange.
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

- Forward secrecy: Available via ECDH X25519 key exchange. When ECDH completes, past sessions are protected even if the password is later compromised. Without ECDH, forward secrecy is not available.
- No certificate-based trust: Uses "Trust On First Use" (TOFU) model with SRP verifier hash.
- No quantum resistance: AES-256 is believed quantum-resistant, but SRP is not.

### Security Score: 85–100/100 (Honest)

**Base scoring** (unchanged from v9.1, max 100):
| Property | Points | Status |
|----------|--------|--------|
| Encryption active | +30 | AES-256-GCM |
| Strong cipher suite | +15 | AES-256-GCM |
| Forward secrecy | +15 | ECDH X25519 (when completed) |
| Authentication | +15 | SRP password auth |
| Replay protection | +10 | Nonce counters + exact bitmap |
| Certificate verification | +10 | Pinned (returning) / TOFU (first) |
| TLS version | +5 | TLS 1.3 equivalent (with ECDH) |

**Bonus scoring** (v9.9, max +15, only with encryption active):
| Bonus | Points | Condition |
|-------|--------|-----------|
| TOFU acknowledged | +3 | First connection with TOFU trust |
| Key rotation capable | +5 | Session rekeying implemented |
| Salted HKDF | +2 | HKDF uses derived salt |
| Exact replay bitmap | +2 | Bitmap tracking within window |
| Integrity verified | +3 | Zero auth failures in session |

**Typical scores:**
- First connection (ECDH + TOFU): 85/100 (Good)
- Returning connection (ECDH + pinned): 97/100 (Excellent)
- Returning with all bonuses: 100/100 (Excellent)
- No ECDH (SRP only): 70/100 (Fair)


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
* Linux run-in-place (this project):
    * `bin`   = `bin`
    * `share` = `.`
    * `user`  = `.`
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

### Prerequisites

- **GCC 7.5+** or **Clang 7.0.1+**
- **CMake 3.5+**
- **OpenSSL 3.0+** (required for encryption module)
- Standard libraries: zlib, zstd, sqlite3, GMP, JsonCPP, LuaJIT, SDL2, freetype, etc.

### Quick Build — Most PCs (system packages only)

The easiest way to build on any Ubuntu/Debian machine. The `build_linux.sh` script auto-detects your distribution, installs the right packages, and builds both client and server:

```bash
# Clone the repo
git clone https://github.com/BirdNest055/Clawtest.git
cd Clawtest

# Build client + server (interactive dependency menu)
./build_linux.sh --both --run-in-place

# Or non-interactive (for CI / automated builds)
./build_linux.sh --non-interactive --no-deps --both --run-in-place --clean
```

### Quick Build — With Local Dependencies

If you have libraries installed in a custom location (e.g., `local-prefix/`), use `--local-prefix`:

```bash
# Auto-detect: looks for local-prefix/ next to the project or one directory up
./build_linux.sh --both --run-in-place --local-prefix /path/to/local-prefix

# Or use build_env.sh (auto-detects local-prefix/)
source build_env.sh
./build_linux.sh --non-interactive --no-deps --both --run-in-place
```

The `build_env.sh` script auto-detects your `local-prefix/` directory and sets up `CMAKE_PREFIX_PATH`, `PKG_CONFIG_PATH`, `LD_LIBRARY_PATH`, etc. It can be configured in three ways:

1. **Auto-detect** (default): Searches for `local-prefix/` next to the project or one directory up
2. **Environment variable**: `LOCAL_PREFIX=/opt/deps source build_env.sh`
3. **Argument**: `source build_env.sh /opt/deps`

### Manual CMake Build

For more control over the build:

```bash
# With system packages only
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=1 -DBUILD_SERVER=1 -DRUN_IN_PLACE=TRUE
cmake --build build -j$(nproc)

# With local dependencies
source build_env.sh /path/to/local-prefix
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=1 -DBUILD_SERVER=1 -DRUN_IN_PLACE=TRUE
cmake --build build -j$(nproc)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `--client` | Yes (default) | Build graphical game client |
| `--server` | No | Build headless dedicated server |
| `--both` | - | Build both client and server |
| `--run-in-place` | No | Run from source directory (no install needed) |
| `--local-prefix PATH` | Auto | Path to locally-built dependencies |
| `--prefix PATH` | `/usr/local` | Install prefix for system install |
| `--release` | Yes | Release build (optimized) |
| `--debug` | No | Debug build (with symbols) |
| `--tests` | No | Build and run unit tests |
| `--clean` | No | Clean build directory before building |
| `--non-interactive` | No | Skip all menus, use defaults |
| `--enable-lto` | No | Enable Link-Time Optimization |
| `--verbose` | No | Verbose output |

### Cross-PC Build Notes

The build system is fully portable — no hardcoded absolute paths. Key design decisions:

- **`build_env.sh`** auto-detects `LOCAL_PREFIX` from the script's location, not from hardcoded paths
- **`build_linux.sh`** supports `--local-prefix PATH` and auto-discovers `local-prefix/` directories
- **`irr/src/CMakeLists.txt`** resolves SDL2 include directories dynamically from `CMAKE_PREFIX_PATH` instead of using hardcoded paths
- **Architecture suffix** (e.g., `x86_64-linux-gnu`) is auto-detected via `dpkg-architecture`
- Works on Ubuntu 22.04, 24.04, Debian, Fedora, Arch, Alpine, and other distros

### Running the Built Binaries

```bash
# With system packages
./bin/luanti --version
./bin/luantiserver --version

# With local dependencies
source build_env.sh
LD_LIBRARY_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu" ./bin/luanti --version
```

### More Compiling Documentation

- [Compiling - common information](doc/compiling/README.md)
- [Compiling on GNU/Linux](doc/compiling/linux.md)
- [Compiling on Windows](doc/compiling/windows.md)
- [Compiling on MacOS](doc/compiling/macos.md)

Docker
------

- [Developing minetestserver with Docker](doc/developing/docker.md)
- [Running a server with Docker](doc/docker_server.md)

Version scheme
--------------

Clawtest uses a dual version scheme:

1. **Engine version** (from upstream Luanti): `major.minor.patch` (currently 5.16.1)
2. **Clawtest version** (encryption feature version): `v9.X` (currently v9.25)

The full version string is `5.16.1-v9.25-dev`, displayed via `--version` and in the UI.

The Clawtest version tracks encryption feature development:
- v7: Secure connection overlay + settings toggle
- v8: Security Info settings tab with technical connection details
- v9.0: Real AES-256-GCM encryption implemented and integrated
- v9.2: Insecure mode actually disables encryption (not just UI flags)
- v9.3: Modular encryption architecture, interactive start scripts, version numbering
- v9.4: Version bump, minor fixes
- v9.5: Consolidated CI workflow (single build.yml), toggleable options
- v9.6: Portable build system — removed all hardcoded paths, auto-detect local-prefix, cross-PC support
- v9.7: Update all docs with cross-PC build commands, track CI warnings
- v9.8: VS Code tasks for build and run
- v9.9: TDD encryption scoring — bonus system, HKDF salt, key rotation, exact replay bitmap, build fixes (i64→s64, overload ordering, deterministic salt)
- v9.10: Documentation update, encryption data flow document
- v9.11: ECDH X25519 forward secrecy with TDD — wire protocol (TOCLIENT/TOSERVER_ECDH_PUBKEY), salted HKDF in mixECDHSecretIntoKeys, deterministic salt in rotateKeys, 22 TDD tests, test bug fixes (TOFU bonus, concurrent nonce, tamper flag, security score)
- v9.12: Encryption activation race condition fix — client defers until server's first encrypted packet, receive path auto-activates
- v9.19: GCM auth spam fix — prevent SetPeerEncryptionState from clobbering SRP keys before ECDH completes
- v9.20: Fake encryption score fix — use real connection state instead of hardcoded encryption_active=true
- v9.21: Build error fix — move isEncryptionActive() out of header (incomplete type error)
- v9.22: Settings panel fix — write all 16 g_settings keys, sync activated_at from connection, both secure/insecure modes work
- v9.23: Log toggle feature — `--no-log`/`--log` in start scripts, `encryption_log_level` setting (none/error/action/trace), prevents 180MB log file problem
- v9.24: Settingtypes context fix — `encryption_log_level` context `[server,client]` → `[common]` (Luanti parser only accepts single context values)
- v9.25: Encryption log autocreate — `encryption_trace.log` created at any non-none level, all `enclog_*` macros write to trace file, fixes missing file after deletion

Git tags follow the pattern `clawtest-v9.25`.
