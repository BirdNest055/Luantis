# AI Codebase Reference — Luanti Security Overlay Project

> **Purpose:** This file gives any AI agent a complete, self-contained picture of the current codebase state, all modifications made, key files, architecture, and how things connect. Read this first before working on any task.
> **Last Updated:** 2026-04-22 | **Project Version:** v8 (in development)

---

## 1. Project Overview

This is a fork of the **Luanti** (formerly Minetest) voxel game engine (version 5.16.0-dev) with added **security overlay** and **connection security info** features. The project modifies both C++ engine code and Lua scripts to:

1. Display a persistent in-game warning overlay when the connection is insecure (v7)
2. Add a dedicated "Security Info" settings tab showing technical details of the connection's security (v8)
3. Track detailed security information (encryption algorithm, key exchange, authentication, etc.) from server to client
4. Provide a security score, session tracking, and server fingerprint to prove security is real (v8)

**Repository:** https://github.com/BirdNest055/luanti-v7-overlay-settings.git
**Branch:** `v8-security-info-tab` (current development), `main` (v7 baseline)

---

## 2. Current Git State

- **`main` branch:** Single commit "just uploaded the folder" — contains all v7 work
- **`v8-security-info-tab` branch:** Local branch with uncommitted v8 changes on top of main
- **No tags yet** — v7.0.0 tag still needs to be applied to main

**Uncommitted changes on v8 branch (vs main):**

| Status | File | Description |
|--------|------|-------------|
| Modified | `builtin/common/settings/dlg_settings.lua` | Inserts security_info_component into Security Info page and Connection Security page |
| Modified | `builtin/common/settings/security_info_component.lua` | Enhanced v8: security score, session info, fingerprint, visual redesign |
| Modified | `builtin/settingtypes.txt` | Added [Security Info] top-level section + [**Connection Security] heading |
| Modified | `luanti-project-map.md` | Updated project documentation |
| Modified | `src/client/client.cpp` | v8: Reset new security info fields on disconnect |
| Modified | `src/client/client.h` | Added `ConnectionSecurityInfo m_security_info` and accessors |
| Modified | `src/client/gameui.h` | Added security overlay members, `setConnectionSecurityInfo()`, `shouldShowSecurityOverlay()` |
| Modified | `src/client/gameui.cpp` | Security overlay rendering, security info tracking, settings reads |
| Modified | `src/defaultsettings.cpp` | Added `secure_connection`, `show_security_overlay`, `show_connection_info`, all `security_info_*` runtime settings, v8 fields |
| Modified | `src/network/clientpackethandler.cpp` | Parses `security_flags`, builds `ConnectionSecurityInfo`, generates session_id/fingerprint, writes v8 settings |
| Modified | `src/network/connection_security.h` | v8: Added TLS version, session_id, connected_since, server_fingerprint, security score |
| Modified | `src/network/serverpackethandler.cpp` | Computes and sends `security_flags` byte in TOCLIENT_HELLO |
| Modified | `src/settings_translation_file.cpp` | Added v8 Security Info tab translation entries |
| Modified | `src/unittest/CMakeLists.txt` | Added `test_connection_security_info.cpp` to test sources |
| Modified | `src/unittest/test_connection_security_info.cpp` | v8: 33 TDD tests (12 new for v8 fields, security score, TLS version) |
| Modified | `src/unittest/test_gameui.cpp` | Extended with v8 security info tests |

---

## 3. Key Architecture — Security Feature Stack

### 3.1 Data Flow: Server → Client → UI

```
Server                         Client                           UI
───────                        ──────                           ──

serverpackethandler.cpp        clientpackethandler.cpp           gameui.cpp
  Computes security_flags       Parses security_flags             Reads m_security_info
  Sends in TOCLIENT_HELLO       Builds ConnectionSecurityInfo     Renders overlay banner
                                Stores in Client::m_security_info  
                                Writes to g_settings (runtime)    security_info_component.lua
                                                                   Reads core.settings
                                                                   Renders security info panel
```

### 3.2 The ConnectionSecurityInfo Struct

Defined in `src/network/connection_security.h` (lines 69-313). This is the **central data structure** for v8. Fields:

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `state` | `ConnectionSecurity` | `Insecure` | Overall secure/insecure state |
| `encryption_algorithm` | `int` | `ENCRYPTION_NONE (0)` | AES-256-GCM, ChaCha20-Poly1305, None |
| `key_exchange` | `int` | `KEY_EXCHANGE_NONE (0)` | ECDH X25519, ECDH P-256, None |
| `authentication` | `int` | `AUTH_NONE (0)` | SRP, ECDSA, None |
| `cipher_suite` | `int` | `CIPHER_NONE (0)` | AES-256-GCM, ChaCha20-Poly1305, None |
| `certificate_status` | `int` | `CERT_NOT_VERIFIED (0)` | Verified, Self-Signed, Expired, Not Verified |
| `forward_secrecy` | `bool` | `false` | Whether forward secrecy is provided |
| `replay_protection` | `bool` | `false` | Whether replay protection is active |
| `protocol_version` | `u16` | `0` | Negotiated protocol version |
| `server_address` | `string` | `""` | Server hostname or IP |
| `server_port` | `u16` | `0` | Server port |
| `session_id` | `string` | `""` | v8: Random hex session identifier |
| `connected_since` | `u64` | `0` | v8: Unix timestamp of connection start |
| `server_fingerprint` | `string` | `""` | v8: Server public key fingerprint |
| `tls_version` | `int` | `TLS_NONE` | v8: TLS version |

Has convenience methods: `isSecure()`, `isForwardSecret()`, `isReplayProtected()`, `isAuthenticated()`, `getSecurityScore()` (0-100), `getSecurityScoreString()`, `getTlsVersionString()`, and static `get*String()` methods for human-readable output.

### 3.3 Security Flags (Wire Protocol)

Sent as a `u8` bitfield in TOCLIENT_HELLO:

| Bit | Flag | Value | Meaning |
|-----|------|-------|---------|
| 0 | `ENCRYPTED` | `0x01` | Connection is encrypted |
| 1 | `ENCRYPTION_SUPPORTED` | `0x02` | Encryption supported but not active |
| 2 | `FORWARD_SECRECY` | `0x04` | Forward secrecy provided |
| 3 | `AUTHENTICATED` | `0x08` | Server identity authenticated |
| 4 | `REPLAY_PROTECTED` | `0x10` | Replay protection active |

### 3.4 Settings (User-Facing)

All defined in `src/defaultsettings.cpp` and `builtin/settingtypes.txt`:

| Setting | Default | Type | Where Visible |
|---------|---------|------|---------------|
| `secure_connection` | `true` | bool | Settings → Security Info |
| `show_security_overlay` | `true` | bool | Settings → Security Info |
| `show_connection_info` | `false` | bool | Settings → Security Info |

**Runtime settings** (not saved, populated by C++ client on connect):

| Setting | Example Value | Purpose |
|---------|---------------|---------|
| `security_info_state` | `"Encrypted"` | Connection state |
| `security_info_encryption` | `"AES-256-GCM"` | Encryption algorithm |
| `security_info_key_exchange` | `"ECDH (X25519)"` | Key exchange method |
| `security_info_authentication` | `"SRP"` | Auth method |
| `security_info_cipher_suite` | `"AES-256-GCM"` | Cipher suite |
| `security_info_cert_status` | `"Verified"` | Certificate status |
| `security_info_forward_secrecy` | `"Yes"` | Forward secrecy |
| `security_info_replay_protection` | `"Yes"` | Replay protection |
| `security_info_protocol_version` | `"42"` | Protocol version |
| `security_info_server_address` | `"example.com"` | Server address |
| `security_info_server_port` | `"30000"` | Server port |
| `security_info_session_id` | `"a1b2c3d4"` | v8: Session identifier |
| `security_info_connected_since` | `"1714000000"` | v8: Connection start timestamp |
| `security_info_server_fingerprint` | `"SHA256:abc..."` | v8: Server fingerprint |
| `security_info_tls_version` | `"TLSv1.3"` | v8: TLS version |
| `security_info_security_score` | `"85"` | v8: Security score (0-100) |

### 3.5 Lua Settings Component

`builtin/common/settings/security_info_component.lua` — a custom read-only component registered in the settings dialog. It:
- Reads 16 runtime settings via `core.settings:get("security_info_*")`
- Displays a visual status banner (color-coded: green/red/yellow)
- Renders six sections: Status Banner, Encryption & Authentication, Certificate & Trust, Security Properties, Session & Connection, Warning/Info box
- Provides contextual explanations for each security property
- Tracks session info (session_id, connected_since) and server fingerprint
- Shows a computed security score (0-100) with visual indicator
- Is inserted at position 1 in the `client_connection_security` page via `dlg_settings.lua`

---

## 4. Key Modified Files — What Changed

### 4.1 C++ Files

**`src/network/connection_security.h`** — The heart of v7+v8 security:
- v7: `ConnectionSecurity` enum, `ConnectionSecurityFlags` namespace, `isConnectionSecure()`, `connectionSecurityFromFlags()`
- v8: `ConnectionSecurityInfo` struct (11 fields + constants + convenience methods + string converters), `connectionSecurityInfoFromFlags()` builder

**`src/client/client.h`** — Client stores security info:
- Added `#include "network/connection_security.h"`
- Added `ConnectionSecurityInfo m_security_info` member
- Added `getConnectionSecurityInfo()`, `setConnectionSecurityInfo()`

**`src/client/gameui.h/cpp`** — UI displays security:
- v7: `setConnectionSecurity()`, `m_guitext_security` overlay element, `shouldShowSecurityOverlay()`
- v8: `setConnectionSecurityInfo()`, `m_security_info` full struct, `getConnectionSecurityInfo()`
- `Flags::show_security_overlay` and `Flags::show_connection_info` toggle checkboxes
- Overlay only shows when: insecure AND setting enabled AND not singleplayer

**`src/defaultsettings.cpp`** — All security defaults:
- `secure_connection = true` (under Network)
- `show_security_overlay = true` (under Visuals)
- `show_connection_info = false` (under Visuals)
- All `security_info_*` runtime settings default to "N/A" or "Not Connected"

**`src/network/clientpackethandler.cpp`** — Client processes TOCLIENT_HELLO:
- Parses optional `security_flags` byte (forward-compatible: old servers won't send it)
- Calls `connectionSecurityInfoFromFlags()` to build `ConnectionSecurityInfo`
- Populates `protocol_version`, `server_address`, `server_port`
- Detects SRP auth if `auth_mechs` contains `AUTH_MECHANISM_SRP`
- Writes all security info to `g_settings` runtime settings for Lua to read
- Logs full security info on connection

**`src/network/serverpackethandler.cpp`** — Server sends security flags:
- When `secure_connection` setting is true: sets `ENCRYPTED | FORWARD_SECRECY | AUTHENTICATED | REPLAY_PROTECTED`
- When false but encryption possible: sets `ENCRYPTION_SUPPORTED`
- Writes `security_flags` byte into `TOCLIENT_HELLO` packet

### 4.2 Lua Files

**`builtin/common/settings/security_info_component.lua`** (v8 enhanced):
- Custom settings component with `id = "security_info"`, `full_width = true`
- `get_formspec()` reads 16 runtime settings, renders six-section layout with status banner
- Visual status banner: color-coded header (green/red/yellow) with security score
- Sections: Status Banner, Encryption & Authentication, Certificate & Trust, Security Properties, Session & Connection, Warning/Info box
- Session tracking (session_id, connected_since) and server fingerprint display
- Computed security score (0-100) with visual indicator
- Contextual explanations for each security property
- `on_submit()` returns false (read-only)

**`builtin/common/settings/dlg_settings.lua`** (modified):
- v8: Inserts component into auto-generated `security_info` page AND `client_connection_security` page
- Loads `security_info_component.lua` via `dofile()`

**`builtin/settingtypes.txt`** (modified):
- Added `[Security Info]` top-level section with `secure_connection`, `show_security_overlay`, `show_connection_info`
- Added `[**Connection Security]` heading

### 4.3 Test Files

**`src/unittest/test_connection_security.cpp`** (v7, 11 tests):
- Tests `ConnectionSecurity` enum values
- Tests `isConnectionSecure()` helper
- Tests `connectionSecurityFromFlags()` with various flag combinations
- Tests `ConnectionSecurityFlags` constants
- Tests `GameUI` security overlay defaults and behavior

**`src/unittest/test_connection_security_info.cpp`** (v8, 33 tests):
- Tests `ConnectionSecurityInfo` default values
- Tests insecure/encrypted connection info
- Tests custom values (ChaCha20, P-256, ECDSA, Self-Signed)
- Tests all `get*String()` methods for human-readable output
- Tests `isSecure()`, `isForwardSecret()`, `isReplayProtected()`, `isAuthenticated()`
- Tests `connectionSecurityInfoFromFlags()` with zero, all, partial, and forward-secrecy-only flags
- v8: Tests for `session_id`, `connected_since`, `server_fingerprint`, `tls_version` fields
- v8: Tests for `getSecurityScore()` and `getSecurityScoreString()` methods
- v8: Tests for `getTlsVersionString()` method
- v8: Tests for security score calculation with various connection configurations
- v8: Tests for TLS version string conversion
- Integration tests: overlay consistency, state transitions (insecure→encrypted→insecure)

**`src/unittest/test_gameui.cpp`** (extended in v8):
- Added tests for `setConnectionSecurity()`, overlay setting disabled/enabled, overlay flag defaults, connection info flag

---

## 5. Build & Script Files

### 5.1 Build Script: `build_linux.sh` (1,147 lines)

Fully automated Linux build script with interactive menus. Supports Debian/Ubuntu, Fedora, Arch, Alpine, openSUSE.

**Key flags:** `--run`, `--no-deps`, `--remove-deps`, `--clean`, `--non-interactive`, `--client`, `--server`, `--both`

**Important notes:**
- Ubuntu 24.04+: uses `libfreetype-dev` (not `libfreetype6-dev`)
- Binary output path: `${SOURCE_DIR}/bin/luanti`
- Uses `set -uo pipefail` (NOT `set -e` — too many false positives)
- ANSI-C quoting for colors: `RED=$'\033[0;31m'` (not `RED='\033[0;31m'`)

**Test suite:** `test_build_linux.sh` (633 lines, 53 TDD tests)

### 5.2 Server Script: `start_server.sh` (994 lines)

Interactive server management script.

**Key flags:** `--secure`, `--insecure`, `--foreground`, `--background`, `--screen`, `--world <name>`, `--list-worlds`, `--create-world <name>`, `--build`, `--status`, `--stop`, `--logs`, `--config`

**Test suite:** `test_start_server.sh` (1,451 lines, 108 TDD tests)

### 5.3 Client Script: `start_client.sh` (663 lines)

Client start/connect script with address, port, and player name options.

---

## 6. Crypto Layer (WIP — Not Yet Integrated)

Two parallel implementations exist in the repo but are **not yet wired into the actual network stack**:

### 6.1 Top-Level API (`src/network/crypto.h`, `src/network/encrypted_connection.h`)
- X25519 key exchange, AES-256-GCM, HKDF-SHA256
- 2-message handshake (simple key exchange)
- Simple nonce counter

### 6.2 Detailed API (`src/network/crypto/`)
- P-256 ECDH key exchange, ECDSA signatures, AES-256-GCM
- 3-message handshake with identity verification
- 64-bit sliding window replay protection

### 6.3 Current Status
- The security overlay and info tab currently work at the **protocol flag level** — they read a bitfield from TOCLIENT_HELLO
- When `secure_connection = true` on the server, it sets the ENCRYPTED flag
- The actual encrypted transport (crypto layer) is not yet integrated — the flag currently means "the server is configured for secure connections"
- Once the crypto layer is integrated, the flags will reflect actual transport-level security

### 6.4 Reconciliation Recommendation
Merge the best of both: X25519 for key exchange, ECDSA from the detailed layer, replay-protected NonceCounter, 3-message handshake.

---

## 7. File Tree — Modified/Created Files Only

```
luanti/
├── ai-codebase-reference.md          ← THIS FILE
├── ai-agent-instructions.md          ← Agent instructions
├── luanti-project-map.md             ← Full project map (1,459 lines)
├── build_linux.sh                    ← Build script (1,147 lines)
├── test_build_linux.sh               ← Build script tests (633 lines, 53 tests)
├── start_server.sh                   ← Server script (994 lines)
├── test_start_server.sh              ← Server script tests (1,451 lines, 108 tests)
├── start_client.sh                   ← Client script (663 lines)
├── builtin/
│   ├── settingtypes.txt              ← [Security Info] top-level section + [**Connection Security] heading
│   └── common/settings/
│       ├── dlg_settings.lua          ← v8: Inserts security_info_component into both pages
│       └── security_info_component.lua ← v8: Enhanced security info panel with score, session, fingerprint
├── src/
│   ├── defaultsettings.cpp           ← secure_connection + security_info_* settings (16 total, 5 new v8)
│   ├── network/
│   │   ├── connection_security.h     ← Core: enums, flags, ConnectionSecurityInfo + v8 (TLS, score, session, fingerprint)
│   │   ├── clientpackethandler.cpp   ← Parses security_flags, builds info, generates session_id/fingerprint, writes settings
│   │   ├── serverpackethandler.cpp   ← Computes security_flags, sends in TOCLIENT_HELLO
│   │   ├── crypto.h                  ← (WIP) Top-level crypto API
│   │   ├── crypto.cpp                ← (WIP) OpenSSL X25519/AES-GCM/HKDF
│   │   ├── encrypted_connection.h    ← (WIP) EncryptedPacket, Handshake, SecureChannel
│   │   ├── encrypted_connection.cpp  ← (WIP) Encrypted connection implementation
│   │   └── crypto/                   ← (WIP) Detailed crypto with P-256/ECDSA
│   ├── client/
│   │   ├── client.h                  ← m_security_info member + accessors
│   │   ├── gameui.h                  ← Security overlay + info members
│   │   └── gameui.cpp               ← Security overlay rendering + settings reads
│   └── unittest/
│       ├── test_connection_security.cpp      ← v7: 11 tests
│       ├── test_connection_security_info.cpp ← v8: 33 tests (21 original + 12 new v8)
│       ├── test_gameui.cpp                   ← Extended with security tests
│       ├── test_encrypted_connection.cpp     ← (WIP): 31 crypto tests
│       └── CMakeLists.txt                    ← Updated test sources
```

---

## 8. Important Patterns & Conventions

### 8.1 Overlay Pattern
Each overlay follows this pattern:
1. Add a `bool show_*` flag in `GameUI::Flags`
2. Add a `show_*` setting in `defaultsettings.cpp`
3. Add it to `settingtypes.txt` under the appropriate section
4. Add it to `minetest.conf.example`
5. Check the flag in `GameUI::update()` to render the overlay
6. Write TDD tests before implementing

### 8.2 Settings Architecture
- **Persistent settings:** Defined in `defaultsettings.cpp`, appear in `settingtypes.txt`, saved to `minetest.conf`
- **Runtime settings:** Also defined in `defaultsettings.cpp` but prefixed `security_info_*`, populated by C++ at runtime, read by Lua settings dialog, NOT saved to disk
- **The bridge:** C++ writes `g_settings->set("security_info_encryption", ...)` in `clientpackethandler.cpp`; Lua reads `core.settings:get("security_info_encryption")` in `security_info_component.lua`

### 8.3 Packet Extension Pattern
The `security_flags` byte in TOCLIENT_HELLO is **forward-compatible**: old servers that don't send it will cause the `pkt >> security_flags` to fail silently (caught by a try-catch), defaulting to 0 (insecure). This means the client works with both old and new servers.

---

## 9. Version History

| Version | Branch | Tag | Feature |
|---------|--------|-----|---------|
| v7 | `v7-overlay-settings` / `main` | `v7.0.0` (pending) | Secure connection overlay + settings checkbox |
| v8 | `v8-security-info-tab` | — | Security Info settings tab with technical connection details |

### v7 Feature Summary
- `ConnectionSecurity` enum (Insecure/Encrypted)
- `ConnectionSecurityFlags` bitfield (ENCRYPTED, ENCRYPTION_SUPPORTED, FORWARD_SECRECY, AUTHENTICATED, REPLAY_PROTECTED)
- `isConnectionSecure()` and `connectionSecurityFromFlags()` helpers
- Security overlay banner in `GameUI` (top-right warning when insecure)
- `secure_connection` setting (default true)
- `show_security_overlay` setting (default true)
- Server sends security_flags in TOCLIENT_HELLO
- Client parses flags and updates overlay
- 11 TDD tests in `test_connection_security.cpp`
- 14 TDD tests in `test_gameui.cpp`

### v8 Feature Summary
- `ConnectionSecurityInfo` struct with 11 fields + constants + string converters
- `connectionSecurityInfoFromFlags()` builder function
- Client populates full `ConnectionSecurityInfo` from server flags
- Runtime settings bridge (C++ writes, Lua reads)
- `security_info_component.lua` — color-coded security info panel in Settings
- `show_connection_info` setting (default false)
- 21 TDD tests in `test_connection_security_info.cpp`
- Extended `test_gameui.cpp` with security info tests

---

## 10. Known Issues & TODOs

1. **No git tags yet** — v7.0.0 tag needs to be applied to main
2. **v8 changes not committed** — all v8 changes are uncommitted on `v8-security-info-tab`
3. **Crypto layer not integrated** — the WIP crypto code exists but isn't wired into the actual network stack
4. **Security flags are currently conceptual** — when `secure_connection = true`, the server sets the ENCRYPTED flag, but actual transport encryption is not yet implemented
5. **NC variable bug** — there may be an unbound `NC` variable issue in `build_linux.sh` under `set -u` (mentioned in previous session context)
