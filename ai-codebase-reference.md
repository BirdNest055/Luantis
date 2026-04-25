# AI Codebase Reference — Clawtest Project

> **Purpose:** This file gives any AI agent a complete, self-contained picture of the current codebase state, all modifications made, key files, architecture, and how things connect. Read this first before working on any task.
> **Last Updated:** 2026-04-26 | **Project Version:** v9.11

---

## 1. Project Overview

This is **Clawtest** — a fork of the **Luanti** (formerly Minetest) voxel game engine (version 5.16.0-dev) with **real, verifiable encrypted communications** between server and client. The project modifies both C++ engine code and Lua scripts to:

1. Display a persistent in-game warning overlay when the connection is insecure (v7)
2. Add a dedicated "Security Info" settings tab showing technical details of the connection's security (v8)
3. Implement real AES-256-GCM packet encryption derived from SRP session keys (v9.0)
4. Make insecure mode actually disable encryption, not just UI flags (v9.2)
5. Provide a modular encryption toggle architecture with centralized policy management (v9.3)
6. Offer interactive start scripts with secure/insecure mode selection (v9.3)
7. Use proper version numbering throughout (v9.3)
8. ECDH X25519 forward secrecy for real forward secrecy (v9.1)
9. Bonus encryption scoring for first-time and returning connections (v9.9)
10. Documentation and encryption data flow guide (v9.10)
11. ECDH X25519 forward secrecy with TDD, wire protocol, test bug fixes (v9.11)

**Repository:** https://github.com/BirdNest055/Clawtest
**Current branch:** `clawtest-v9.11`
**Upstream:** https://github.com/luanti-org/luanti (version 5.16.0-dev)

---

## 2. Current Git State

- **`main` branch:** Upstream Luanti 5.16.0-dev
- **`clawtest-upload` branch:** Previous development branch (v9.0-v9.2 work)
- **`clawtest-v9.3` branch:** Previous development branch (v9.3 work)
- **`clawtest-v9.11` branch:** Current branch with all v9.11 changes (ECDH forward secrecy, test fixes)

**Commit on clawtest-v9.11:**
```
v9.11: fix test failures (TOFU bonus, concurrent nonce, tamper flag, security score), update VERSION and docs
```

---

## 3. Key Architecture — Security Feature Stack

### 3.1 Data Flow: Server → Client → UI

```
Server                              Client                              UI
───────                             ──────                              ──

serverpackethandler.cpp             clientpackethandler.cpp              gameui.cpp
  Uses EncryptionConfig               Uses EncryptionConfig               Reads m_security_info
  When shouldEncrypt():               When shouldEncrypt():               Renders overlay banner
    Init AES-256-GCM from SRP key      Init AES-256-GCM from SRP key
  When !shouldEncrypt():              When !shouldEncrypt():
    encryption_state.disable()          encryption_state.disable()

  getSecurityFlags()                  Parses security_flags               security_info_component.lua
  Sends in TOCLIENT_HELLO             Builds ConnectionSecurityInfo         Reads core.settings
                                       Writes to g_settings (runtime)       Renders security info panel
```

### 3.2 The EncryptionConfig Module (v9.3 — NEW)

Defined in `src/network/encryption_config.h` and `encryption_config.cpp`. This is the **single point of truth** for all encryption decisions.

| Function | Return | Purpose |
|----------|--------|---------|
| `shouldEncrypt()` | `bool` | Reads `secure_connection` setting; returns true if encryption should be active |
| `getModeString()` | `string` | Returns "secure" or "insecure" |
| `logEncryptionDecision(peer_id, is_server, activated)` | `void` | Logs encryption decision for debugging |
| `getSecurityFlags()` | `u8` | Computes security flags for TOCLIENT_HELLO packet |

**Usage pattern** (in both server and client packet handlers):
```cpp
#include "encryption_config.h"

bool secure_mode = EncryptionConfig::shouldEncrypt();
if (secure_mode) {
    encryption_state.initFromSRPSessionKey(key, key_len);
    EncryptionConfig::logEncryptionDecision(peer_id, is_server, true);
} else {
    encryption_state.disable();
    EncryptionConfig::logEncryptionDecision(peer_id, is_server, false);
}
```

### 3.3 The ConnectionSecurityInfo Struct

Defined in `src/network/connection_security.h`. This is the **central data structure** for security info.

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `state` | `ConnectionSecurity` | `Insecure` | Overall secure/insecure state |
| `encryption_algorithm` | `int` | `ENCRYPTION_NONE (0)` | AES-256-GCM, ChaCha20-Poly1305, None |
| `key_exchange` | `int` | `KEY_EXCHANGE_NONE (0)` | SRP, ECDH X25519, None |
| `authentication` | `int` | `AUTH_NONE (0)` | SRP, ECDSA, None |
| `cipher_suite` | `int` | `CIPHER_NONE (0)` | AES-256-GCM, ChaCha20-Poly1305, None |
| `certificate_status` | `int` | `CERT_NOT_VERIFIED (0)` | Verified, Self-Signed, Expired, Not Verified |
| `forward_secrecy` | `bool` | `false` | Whether forward secrecy is provided |
| `replay_protection` | `bool` | `false` | Whether replay protection is active |
| `protocol_version` | `u16` | `0` | Negotiated protocol version |
| `server_address` | `string` | `""` | Server hostname or IP |
| `server_port` | `u16` | `0` | Server port |
| `session_id` | `string` | `""` | Random hex session identifier |
| `connected_since` | `u64` | `0` | Unix timestamp of connection start |
| `server_fingerprint` | `string` | `""` | Server public key fingerprint |
| `tls_version` | `int` | `TLS_NONE` | TLS version |
| `tofu_acknowledged` | `bool` | `false` | TOFU provides partial verification credit (+3) |
| `key_rotation_capable` | `bool` | `false` | Session key rekeying implemented (+5) |
| `salted_key_derivation` | `bool` | `false` | HKDF uses salt for key separation (+2) |
| `exact_replay_bitmap` | `bool` | `false` | Bitmap tracking within window (+2) |
| `integrity_verified` | `bool` | `false` | Zero auth failures in session (+3) |

Has convenience methods: `isSecure()`, `isForwardSecret()`, `isReplayProtected()`, `isAuthenticated()`, `getSecurityScore()` (0-100), `getSecurityScoreString()`, `getTlsVersionString()`, and static `get*String()` methods for human-readable output.

**Bonus scoring methods:** `getBonusScore()` returns the sum of bonus field values, and `getBonusBreakdown()` returns a human-readable breakdown. The total security score is `getSecurityScore()` = `getBaseSecurityScore()` + `getBonusScore()`, capped at 100.

### 3.4 Security Flags (Wire Protocol)

Sent as a `u8` bitfield in TOCLIENT_HELLO:

| Bit | Flag | Value | Meaning |
|-----|------|-------|---------|
| 0 | `ENCRYPTED` | `0x01` | Connection is encrypted |
| 1 | `ENCRYPTION_SUPPORTED` | `0x02` | Encryption supported but not active |
| 2 | `FORWARD_SECRECY` | `0x04` | Forward secrecy provided |
| 3 | `AUTHENTICATED` | `0x08` | Server identity authenticated |
| 4 | `REPLAY_PROTECTED` | `0x10` | Replay protection active |

### 3.5 Settings (User-Facing)

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
| `security_info_key_exchange` | `"SRP"` | Key exchange method |
| `security_info_authentication` | `"SRP"` | Auth method |
| `security_info_cipher_suite` | `"AES-256-GCM"` | Cipher suite |
| `security_info_cert_status` | `"Not Verified"` | Certificate status |
| `security_info_forward_secrecy` | `"No"` | Forward secrecy |
| `security_info_replay_protection` | `"Yes"` | Replay protection |
| `security_info_protocol_version` | `"42"` | Protocol version |
| `security_info_server_address` | `"localhost"` | Server address |
| `security_info_server_port` | `"30000"` | Server port |
| `security_info_session_id` | `"a1b2c3d4"` | Session identifier |
| `security_info_connected_since` | `"1714000000"` | Connection start timestamp |
| `security_info_server_fingerprint` | `"SHA256:abc..."` | Server fingerprint |
| `security_info_tls_version` | `"Custom"` | TLS version |
| `security_info_security_score` | `"70"` | Security score (0-100) |

---

## 4. Key Modified Files — What Changed

### 4.1 Encryption Architecture (v9.0-v9.3)

**`src/network/encryption_config.h`** (v9.3 — NEW):
- `EncryptionConfig` namespace with `shouldEncrypt()`, `getModeString()`, `logEncryptionDecision()`, `getSecurityFlags()`
- Single point of truth for encryption policy decisions
- All packet handlers MUST use this instead of reading `g_settings` directly

**`src/network/encryption_config.cpp`** (v9.3 — NEW):
- Implementation reads `g_settings->getBool("secure_connection")`
- Logs every encryption decision with peer_id, role, mode, and outcome
- Computes security flags based on encryption mode

**`src/network/connection_security.h`** — Core security types:
- v7: `ConnectionSecurity` enum, `ConnectionSecurityFlags` namespace, `isConnectionSecure()`, `connectionSecurityFromFlags()`
- v8: `ConnectionSecurityInfo` struct (14 fields + constants + convenience methods + string converters)
- v9: Updated with honest security info (no fake claims)

**`src/network/clientpackethandler.cpp`** — Client processes TOCLIENT_HELLO:
- Uses `EncryptionConfig::shouldEncrypt()` for encryption decision
- When secure: initializes AES-256-GCM from SRP session key
- When insecure: calls `encryption_state.disable()` — SRP still runs for auth but no encryption
- Parses optional `security_flags` byte (forward-compatible)
- Populates `ConnectionSecurityInfo` with real data

**`src/network/serverpackethandler.cpp`** — Server handles SRP and encryption:
- Uses `EncryptionConfig::shouldEncrypt()` for encryption decision
- When secure: initializes AES-256-GCM from SRP session key
- When insecure: calls `encryption_state.disable()`
- Uses `EncryptionConfig::getSecurityFlags()` for TOCLIENT_HELLO

### 4.2 Crypto Layer (v9.0)

**`src/network/crypto.h`** — Top-level crypto API:
- `KeyPair` — X25519 ephemeral key pair for ECDH
- `AES256GCM` — Static encrypt/decrypt with 12-byte nonce, optional AAD
- `HKDF` — HKDF-SHA256 key derivation
- `Random` — CSPRNG via `RAND_bytes`
- `SessionKeys` — Derives bidirectional keys (C2S, S2C) from shared secret

**`src/network/crypto.cpp`** — OpenSSL 3.x implementation

**`src/network/encrypted_connection.h`** — Encrypted connection layer:
- `EncryptedPacket` — Wire format: `[1B version][8B counter][12B nonce][N ciphertext + 16B tag]`
- `EncryptedConnection::Handshake` — 2-message key exchange
- `EncryptedConnection::SecureChannel` — Wraps `con::IConnection` to add encryption

**`src/network/encrypted_connection.cpp`** — Implementation

### 4.3 UI Layer (v7-v8)

**`src/client/gameui.h/cpp`** — Security overlay rendering:
- `setConnectionSecurity()`, `m_guitext_security` overlay element
- `setConnectionSecurityInfo()`, `m_security_info` full struct
- `Flags::show_security_overlay` and `Flags::show_connection_info` toggle checkboxes

**`src/client/client.h`** — Client stores security info:
- `ConnectionSecurityInfo m_security_info` member
- `getConnectionSecurityInfo()`, `setConnectionSecurityInfo()`

**`src/defaultsettings.cpp`** — All security defaults:
- `secure_connection = true` (under Network)
- `show_security_overlay = true` (under Visuals)
- `show_connection_info = false` (under Visuals)
- All `security_info_*` runtime settings default to "N/A" or "Not Connected"

### 4.4 Lua Layer (v8)

**`builtin/common/settings/security_info_component.lua`**:
- Custom read-only settings component with `id = "security_info"`, `full_width = true`
- Reads 16 runtime settings via `core.settings:get("security_info_*")`
- Displays color-coded status banner, six sections, security score

**`builtin/common/settings/dlg_settings.lua`**:
- Inserts component into `security_info` page AND `client_connection_security` page

**`builtin/settingtypes.txt`**:
- `[Security Info]` top-level section
- `[**Connection Security]` heading
- `secure_connection` with description explaining insecure mode behavior (plaintext)

### 4.5 Start Scripts (v9.3)

**`start_server.sh`** — Interactive server management script:
- Menu options: security mode, port, game, world, admin player, max players, MOTD, run mode
- CLI flags: `--secure`, `--insecure`, `--go`, `--name`, `--port`, `--game`, `--world`
- Generates temp config with `secure_connection = true/false`
- Pauses on exit so terminal doesn't close on crash

**`start_client.sh`** — Interactive client start script:
- Menu options: server address, port, player name, password, security mode, display mode, resolution
- CLI flags: `--secure`, `--insecure`, `--go`, `--name`, `--address`, `--port`
- Generates temp config with `secure_connection = true/false`
- Pauses on exit

### 4.6 Test Files

**`src/unittest/test_connection_security.cpp`** (v7, 11 tests):
- Tests `ConnectionSecurity` enum, `isConnectionSecure()`, `connectionSecurityFromFlags()`

**`src/unittest/test_connection_security_info.cpp`** (v8, 33 tests):
- Tests `ConnectionSecurityInfo` struct, security score calculation, TLS version strings

**`src/unittest/test_encrypted_connection.cpp`** (v9, 31 tests):
- Tests crypto primitives, encrypted connection layer, replay protection

**`test_encryption_toggle.sh`** (v9.3, 14 tests):
- Tests server start in secure/insecure modes
- Tests log messages for each mode
- Tests EncryptionConfig module is compiled in
- Tests version strings
- Tests start script syntax and patterns

---

## 5. Build & Script Files

### 5.1 Build Script: `build_linux.sh`

Fully automated Linux build script with interactive menus. Supports Debian/Ubuntu, Fedora, Arch, Alpine, openSUSE.

**Key flags:** `--run`, `--no-deps`, `--remove-deps`, `--clean`, `--non-interactive`, `--client`, `--server`, `--both`

**Quick build command:**
```bash
./build_linux.sh --non-interactive --no-deps --client --run-in-place
```

### 5.2 Build Environment

- Local prefix at `/home/z/my-project/local-prefix/` with extracted .deb packages
- Build uses `build_env.sh` for environment setup
- OpenSSL 3.5.5 is required for the encryption module
- Binary output path: `bin/luanti` (client) and `bin/luantiserver` (server)

---

## 6. Version History

| Version | Branch | Feature |
|---------|--------|---------|
| v7 | `v7-overlay-settings` / `main` | Secure connection overlay + settings checkbox |
| v8 | `v8-security-info-tab` | Security Info settings tab with technical connection details |
| v9.0 | `clawtest-upload` | Real AES-256-GCM encryption implemented and integrated |
| v9.2 | `clawtest-upload` | Insecure mode actually disables encryption (not just UI flags) |
| v9.3 | `clawtest-v9.3` | Modular encryption architecture, interactive start scripts, version numbering |
| v9.8 | `clawtest-v9.8` | VS Code tasks for build and run |
| v9.9 | `clawtest-v9.9` | TDD encryption scoring — bonus system, HKDF salt, key rotation, exact replay bitmap, build fixes |
| v9.10 | `clawtest-v9.10` | Documentation update, ENCRYPTION_DATA_FLOW.md, all MDs updated |
| v9.11 | `clawtest-v9.11` | ECDH X25519 forward secrecy with TDD — wire protocol, salted HKDF in mixECDHSecretIntoKeys, deterministic salt in rotateKeys, 22 TDD tests, test bug fixes |

### v9.3 Feature Summary
- `EncryptionConfig` namespace — centralized encryption policy manager
- Interactive `start_server.sh` and `start_client.sh` with menus
- `VERSION` file and `CMakeLists.txt VERSION_EXTRA = "v9.3"`
- `test_encryption_toggle.sh` — 14 TDD tests
- `builtin/settingtypes.txt` — updated description for `secure_connection`
- Pause-on-exit in start scripts to prevent terminal closing on crash

---

## 7. File Tree — Modified/Created Files (v9.3)

```
Clawtest/
+-- VERSION                                    <- "9.3"
+-- CMakeLists.txt                             <- VERSION_EXTRA = "v9.3"
+-- README.md                                  <- Updated for Clawtest
+-- V9_PLAN.md                                 <- Updated progress tracker
+-- ai-agent-instructions.md                   <- Updated conventions
+-- ai-codebase-reference.md                   <- THIS FILE
+-- luanti-project-map.md                      <- Full project map
+-- build_linux.sh                             <- Build script
+-- build_env.sh                               <- Build environment setup
+-- start_server.sh                            <- Interactive server script
+-- start_client.sh                            <- Interactive client script
+-- test_encryption_toggle.sh                  <- Encryption toggle tests (14 tests)
+-- ENCRYPTION_DATA_FLOW.md                     <- v9.10: Comprehensive encryption data flow guide
+-- .vscode/
|   +-- tasks.json                              <- v9.8: VS Code tasks
+-- builtin/
|   +-- settingtypes.txt                       <- Updated secure_connection description
|   +-- common/settings/
|       +-- dlg_settings.lua                   <- v8: Inserts security_info_component
|       +-- security_info_component.lua        <- v8: Security info panel
+-- src/
    +-- defaultsettings.cpp                    <- All security settings
    +-- network/
    |   +-- encryption_config.h                <- v9.3: Centralized encryption policy
    |   +-- encryption_config.cpp              <- v9.3: Implementation
    |   +-- connection_security.h              <- Core: enums, flags, ConnectionSecurityInfo
    |   +-- clientpackethandler.cpp            <- Uses EncryptionConfig for encryption decisions + ECDH
    |   +-- serverpackethandler.cpp            <- Uses EncryptionConfig for encryption decisions + ECDH
    |   +-- crypto.h                           <- v9.0+11: Crypto API + X25519 ECDH + PeerEncryptionState
    |   +-- crypto.cpp                         <- v9.0+11: OpenSSL implementation + ECDH + key mixing
    |   +-- encrypted_connection.h             <- v9.0: Encrypted packet/handshake/channel
    |   +-- encrypted_connection.cpp           <- v9.0: Implementation
    |   +-- CMakeLists.txt                     <- Added crypto.cpp, encryption_config.cpp
    +-- client/
    |   +-- client.h                           <- m_security_info member + accessors
    |   +-- gameui.h                           <- Security overlay + info members
    |   +-- gameui.cpp                         <- Security overlay rendering
    +-- unittest/
        +-- test_connection_security.cpp       <- v7: 11 tests
        +-- test_connection_security_info.cpp  <- v8: 33 tests
        +-- test_encrypted_connection.cpp      <- v9: 31 tests
        +-- test_security_score_v99.cpp          <- v9.9: Bonus scoring tests
        +-- test_forward_secrecy.cpp           <- v9.11: 22 ECDH forward secrecy TDD tests
        +-- test_ecdh_x25519.cpp               <- v9.11: 17 X25519 crypto TDD tests
        +-- test_peer_encryption_state.cpp     <- v9.9+11: 31 peer encryption state tests
        +-- test_encrypted_packet_format.cpp   <- v9.9+11: 27 packet format tests
        +-- test_gameui.cpp                    <- Extended with security tests
```

---

## 8. Important Patterns & Conventions

### 8.1 Encryption Toggle Pattern

Always use `EncryptionConfig::shouldEncrypt()` — never read `g_settings` directly:

```cpp
// CORRECT:
bool secure_mode = EncryptionConfig::shouldEncrypt();
if (secure_mode) {
    encryption_state.initFromSRPSessionKey(key, key_len);
} else {
    encryption_state.disable();
}

// WRONG:
if (g_settings->getBool("secure_connection")) {  // bypasses the centralized policy
    encryption_state.initFromSRPSessionKey(key, key_len);
}
```

### 8.2 Overlay Pattern

Each overlay follows this pattern:
1. Add a `bool show_*` flag in `GameUI::Flags`
2. Add a `show_*` setting in `defaultsettings.cpp`
3. Add it to `settingtypes.txt` under the appropriate section
4. Add it to `minetest.conf.example`
5. Check the flag in `GameUI::update()` to render the overlay
6. Write TDD tests before implementing

### 8.3 Settings Architecture

- **Persistent settings:** Defined in `defaultsettings.cpp`, appear in `settingtypes.txt`, saved to `minetest.conf`
- **Runtime settings:** Also defined in `defaultsettings.cpp` but prefixed `security_info_*`, populated by C++ at runtime, read by Lua settings dialog, NOT saved to disk
- **The bridge:** C++ writes `g_settings->set("security_info_encryption", ...)` in `clientpackethandler.cpp`; Lua reads `core.settings:get("security_info_encryption")` in `security_info_component.lua`

### 8.4 Packet Extension Pattern

The `security_flags` byte in TOCLIENT_HELLO is **forward-compatible**: old servers that don't send it will cause the `pkt >> security_flags` to fail silently (caught by a try-catch), defaulting to 0 (insecure). This means the client works with both old and new servers.

### 8.5 Version Bumping Pattern

When bumping the version number, update ALL of these:
1. `VERSION` file — just the number (e.g., "9.4")
2. `CMakeLists.txt` — `VERSION_EXTRA` string (e.g., "v9.4")
3. `test_encryption_toggle.sh` — version check in test_08 and test_09
4. Commit message references the version
5. Branch name includes the version (e.g., `clawtest-v9.4`)

---

## 9. Known Issues & TODOs

1. **Phase 7 (Toggleable Overlays) not started** — mini/standard/detailed security overlay modes
2. **Phase 8 (Security Info Settings Tab) not started** — real security info tab in Settings
3. **Phase 10 (CI/CD) not started** — GitHub Actions workflow for automated builds
4. **start_server.sh potential crash** — may still have edge cases; pause-on-exit helps debug
5. **Crypto layer reconciliation** — X25519 ECDH is now fully integrated; two parallel crypto APIs remain in `src/network/crypto/` (P-256/ECDSA) not yet integrated
6. **Key rotation wire protocol** — `rotateKeys()` exists but requires a protocol exchange to coordinate with the peer; no TOSERVER_KEY_ROTATION / TOCLIENT_KEY_ROTATION packet types yet
7. **Compiler warnings in test files** (sign compare, unused variables) — see TODO_FIXME_LIST.md
