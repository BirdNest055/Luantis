# Luanti-Secure v9: Real Encryption Implementation Plan

## The Problem (Discovered via Wireshark)

The original `secure_connection` feature in upstream Luanti was **security theater**:
- Server sets flags in `TOCLIENT_HELLO` claiming AES-256-GCM, ECDH X25519, TLS 1.3
- Client blindly trusts those flags and hides the "INSECURE CONNECTION" overlay
- **In reality**: All game data is **plaintext UDP** — confirmed by Wireshark capture
- The "server fingerprint" is `myrand()` — random bytes, not derived from any key
- The security score (up to 100/100 "Excellent") is based on self-reported, unverified flags

This is **worse than no security** because it gives users a false sense of security.

---

## v9 Goal: Real Encryption with Honest UI

Implement **actual AES-256-GCM packet encryption** using the SRP session key,
with honest UI that accurately reports what protection exists and what doesn't.

**STATUS: COMPLETE** — Phases 1-6, 9, and 10 are done. Phases 7, 8 remain (UI enhancements, not security-critical). v9.9 adds bonus scoring, HKDF salt, key rotation, exact replay bitmap. v9.10 adds documentation and encryption data flow guide. v9.11 adds ECDH X25519 forward secrecy — real PFS with full handshake, 22 TDD tests, and 100/100 security score. v9.12 fixes the encryption activation race condition — client defers activation until server's first encrypted packet, receive path auto-activates on successful decrypt.

---

## Architecture: SRP-Derived AES-256-GCM

### Why This Approach

1. SRP authentication already produces a **32-byte shared session key** on both sides
2. The key was previously **discarded** after auth succeeds — we captured it
3. OpenSSL 3.5.5 is available on the system with full AES-256-GCM + HKDF support
4. No new network round-trips needed — encryption activates immediately after SRP auth

### What This Gives Us (REAL)
- **Packet encryption**: AES-256-GCM encrypts all post-auth game traffic
- **Authentication**: SRP proves both sides know the password
- **Integrity**: GCM authentication tag detects tampering
- **Replay protection**: Monotonic nonce counters with sliding window
- **Key separation**: HKDF derives separate C2S and S2C keys
- **Wireshark-proof**: Captured packets show ciphertext, not game data

### What This Does NOT Give Us (HONEST LIMITATIONS)
- **No Certificate-based trust**: No PKI, no CA verification
  - Uses "Trust On First Use" (TOFU) model with SRP verifier hash
  - Future: Could add self-signed cert management
- **No Quantum resistance**: AES-256 is believed quantum-resistant, but SRP is not

### Forward Secrecy (v9.11)

With the addition of ECDH X25519 key exchange in v9.11, forward secrecy is now **real**:
- Server generates ephemeral X25519 keypair during auth, sends pubkey before encryption activation
- Client generates ephemeral X25519 keypair, computes shared secret, mixes into keys via HKDF, sends its pubkey back
- Ephemeral private keys are destroyed after session
- Even if the SRP password is later compromised, past sessions cannot be decrypted
- Key exchange: KEY_EXCHANGE_ECDH_X25519
- TLS equivalent: TLS_1_3_EQUIVALENT

### Wire Protocol (After Encryption Active)

**Before auth** (plaintext — same as now):
```
[Base Header 7B][MTP Packet Data]
```

**After auth** (encrypted):
```
[Base Header 7B][Encrypted Flag 1B][Nonce 12B][Ciphertext NB][GCM Tag 16B]
```

- Base header stays unencrypted (needed for routing: protocol_id, peer_id, channel)
- Encrypted flag byte: `0x00` = plaintext, `0x80` = AES-256-GCM encrypted
- Nonce: 12-byte monotonically increasing counter (per direction)
- Ciphertext: AES-256-GCM encrypted MTP packet data
- GCM Tag: 16-byte authentication tag

---

## Implementation Phases

### Phase 1: Crypto Infrastructure (NEW FILES) — DONE
**Files**: `src/network/crypto.h`, `src/network/crypto.cpp`
**Tests**: `src/network/crypto_test.cpp`

Created the cryptographic primitive layer:
- `AES256GCM` class: encrypt/decrypt with 12-byte nonce + 16-byte tag
- `HKDFSHA256` function: derive keys from SRP session key
- `SecureRandom` function: generate nonces, session IDs
- Unit tests for all crypto primitives (known test vectors from RFCs)

**Key derivation schema**:
```
SRP Session Key (32 bytes)
    |
    +-- HKDF(salt=derived, info="Luanti v9 HKDF Salt") -> HKDF salt (16 bytes)
    |
    +-- HKDF(salt=hkdf_salt, info="Luanti v9 C2S Key") -> Client->Server AES key (32 bytes)
    +-- HKDF(salt=hkdf_salt, info="Luanti v9 S2C Key") -> Server->Client AES key (32 bytes)
    +-- HKDF(salt=hkdf_salt, info="Luanti v9 C2S Nonce") -> C2S nonce base (4 bytes)
    +-- HKDF(salt=hkdf_salt, info="Luanti v9 S2C Nonce") -> S2C nonce base (4 bytes)
    +-- HKDF(salt=hkdf_salt, info="Luanti v9 Session ID") -> Session ID (16 bytes -> hex)
```

Nonce format: `[4-byte base from HKDF][8-byte counter]`
- Base ensures different sessions have different nonce spaces
- Counter increments per packet (monotonic, never reuses)
- 8-byte counter gives 2^64 packets before nonce exhaustion

### Phase 2: Session Key Capture — DONE
**Files**: Modified `src/network/clientpackethandler.cpp`, `src/network/serverpackethandler.cpp`, `src/network/clientiface.cpp`, `src/client/client.h`, `src/client/client.cpp`

**Client side** (in `handleCommand_AuthAccept()`):
- BEFORE calling `deleteAuthData()`, call `srp_user_get_session_key()`
- Store 32-byte key in `Client::m_encryption_session_key`
- Pass to connection layer for key derivation

**Server side** (in `handleCommand_SrpBytesM()`, after verification):
- BEFORE calling `acceptAuth()`, call `srp_verifier_get_session_key()`
- Store 32-byte key in `ClientInterface` / `RemoteClient`
- Pass to connection layer for key derivation

**Also**: FIRST_SRP case (new account) handled — no SRP exchange, so no session key. Encryption is not possible; falls back to plaintext with honest UI.

### Phase 3: Encryption State in Connection Layer — DONE
**Files**: Modified `src/network/mtp/impl.h`, `src/network/mtp/impl.cpp`, `src/network/mtp/internal.h`

Added encryption state to the `Connection` and `UDPPeer` classes:
- `PeerEncryptionState` on `UDPPeer`
- `SetPeerEncryptionState()` method
- Separate C2S and S2C key storage, nonce bases, and counters

### Phase 4: Packet Encryption/Decryption — DONE
**Files**: Modified `src/network/mtp/threads.cpp`, `src/network/mtp/impl.cpp`

**Encrypt path** (in `rawSend()`):
1. Check if encryption_active for this peer
2. If yes: build nonce, encrypt with AES-256-GCM, prepend flag + nonce, append tag
3. If no: send plaintext

**Decrypt path** (in `receive()`):
1. Check encrypted flag byte after stripping base header
2. If 0x01: read nonce + tag, decrypt, verify GCM tag
3. If 0x00: process normally

### Phase 5: Replay Protection — DONE
**Files**: Modified `src/network/mtp/internal.h`, `src/network/mtp/threads.cpp`

- Track last received nonce counter per direction per peer
- Accept packets with counter >= last_accepted (allow small reordering)
- Sliding window of 64 packets for out-of-order but legitimate packets
- Reject packets with counter significantly behind (likely replay)
- Log replay detection events

### Phase 6: Honest Security Info — DONE
**Files**: Modified `src/network/connection_security.h`, `src/network/clientpackethandler.cpp`, `src/network/serverpackethandler.cpp`

Replaced fake `connectionSecurityInfoFromFlags()` with **real data**:
- When encryption is active: `state = Encrypted`, real cipher, real key exchange (SRP), honest forward_secrecy=false, honest cert_status=TOFU
- When encryption is inactive: `state = Insecure`, all fields at default "None"/"N/A"

### Phase 7: Toggleable Overlay System — NOT STARTED
**Files**: Modify `src/client/gameui.h`, `src/client/gameui.cpp`, `src/defaultsettings.cpp`

Three overlay modes (toggleable from Settings):
1. Mini — small lock icon in corner
2. Standard — lock icon + status text
3. Detailed — multi-line panel with all security properties
4. Off — no overlay

### Phase 8: Security Info Settings Tab — NOT STARTED
**Files**: Modify Lua settings tab, `src/defaultsettings.cpp`

Real security info tab with status banner, encryption details, honest limitations, session info, packet stats.

### Phase 9: CMake Build Integration — DONE
**Files**: Modified `src/CMakeLists.txt`, `src/network/CMakeLists.txt`

- Added `crypto.cpp` and `encryption_config.cpp` to network source list
- `find_package(OpenSSL 3.0 REQUIRED)` for crypto module
- Link `OpenSSL::Crypto` to the network library target
- `VERSION_EXTRA` set to "v9.3" in root CMakeLists.txt

### Phase 10: CI/CD & Testing — DONE (v9.5–v9.6)
**Files**: `.github/workflows/build.yml`, `build_env.sh`, `build_linux.sh`, `irr/src/CMakeLists.txt`

- v9.5: Consolidated 10 upstream workflows into single `build.yml` with toggleable options
- v9.5: Build client + server runs by default on every push/PR
- v9.5: Lint jobs (cpp_lint, lua_lint) are toggleable, default OFF
- v9.6: Fixed hardcoded paths in `irr/src/CMakeLists.txt` (SDL2 include dirs now resolved dynamically from CMAKE_PREFIX_PATH)
- v9.6: `build_env.sh` rewritten to auto-detect `LOCAL_PREFIX` (no hardcoded absolutes)
- v9.6: `build_linux.sh` gained `--local-prefix PATH` option with auto-detection
- v9.6: CI workflow runs successfully on GitHub Actions (Ubuntu 24.04), all 11 steps pass

---

## v9.2-9.3 Additions: Modular Encryption & Interactive Scripts

### Modular Encryption Architecture (v9.3)

New files `src/network/encryption_config.h` and `src/network/encryption_config.cpp` provide a centralized, single-point-of-truth system for controlling whether encryption is active:

- `EncryptionConfig::shouldEncrypt()` — reads `secure_connection` setting, returns bool
- `EncryptionConfig::getModeString()` — returns "secure" or "insecure"
- `EncryptionConfig::logEncryptionDecision()` — logs encryption decisions for debugging
- `EncryptionConfig::getSecurityFlags()` — computes security flags for TOCLIENT_HELLO

Both `serverpackethandler.cpp` and `clientpackethandler.cpp` now use this module instead of directly reading `g_settings->getBool("secure_connection")`. This ensures a single point of truth for encryption decisions and makes it easy to add more complex policy logic in the future.

### Insecure Mode Actually Works (v9.2)

Previously, `secure_connection = false` only set UI flags but AES-256-GCM encryption was activated unconditionally after SRP auth. Now:

- **Server side**: When `shouldEncrypt()` is false, calls `encryption_state.disable()` instead of `initFromSRPSessionKey()`. SRP auth still runs for password verification, but no encryption keys are derived.
- **Client side**: Same fix — when `shouldEncrypt()` is false, encryption is disabled after SRP auth completes.

### Interactive Start Scripts (v9.3)

`start_server.sh` and `start_client.sh` both provide:
- Interactive menus for choosing secure/insecure mode
- Player name, port, address, game selection
- CLI flags for non-interactive use (`--secure`, `--insecure`, `--go`, etc.)
- Temp config generation with proper `secure_connection` setting
- Pause on exit (so the terminal doesn't close immediately on crash)

### Test Script (v9.3)

`test_encryption_toggle.sh` — 14 TDD tests verifying:
- Server starts in both secure and insecure modes
- Correct log messages for each mode
- Encryption config module is compiled into binaries
- Version strings are correct
- Start scripts have valid syntax and no crash-prone patterns

---

## File Change Summary

### NEW FILES (v9.3)
| File | Purpose |
|------|---------|
| `VERSION` | Version file containing "9.3" |
| `src/network/encryption_config.h` | Centralized encryption policy manager declarations |
| `src/network/encryption_config.cpp` | Centralized encryption policy manager implementation |
| `test_encryption_toggle.sh` | 14 TDD tests for encryption toggle |

### EXISTING FILES FROM v9.0
| File | Purpose |
|------|---------|
| `src/network/crypto.h` | AES-256-GCM, HKDF, secure random declarations |
| `src/network/crypto.cpp` | OpenSSL-based implementation |
| `src/network/encrypted_connection.h` | EncryptedPacket, Handshake, SecureChannel |
| `src/network/encrypted_connection.cpp` | Encrypted connection implementation |

### MODIFIED FILES (v9.0 - v9.3)
| File | Changes |
|------|---------|
| `CMakeLists.txt` | VERSION_EXTRA set to v9.3 |
| `src/network/CMakeLists.txt` | Added crypto.cpp, encryption_config.cpp |
| `src/network/connection_security.h` | New constants, honest security info |
| `src/network/clientpackethandler.cpp` | Extract SRP key, encryption toggle, populate real security info |
| `src/network/serverpackethandler.cpp` | Extract SRP key, encryption toggle, set real security flags |
| `src/network/clientiface.cpp` | Store session key before verifier deletion |
| `src/network/mtp/impl.h` | Add encryption state to Connection class |
| `src/network/mtp/impl.cpp` | Implement enableEncryption(), encrypt/decrypt helpers |
| `src/network/mtp/internal.h` | Add encryption state to UDPPeer |
| `src/network/mtp/threads.cpp` | Encrypt in rawSend(), decrypt in receive() |
| `src/client/client.h` | Add session key storage, encryption state |
| `src/client/client.cpp` | Wire encryption into client lifecycle |
| `src/client/gameui.h` | Add overlay mode support |
| `src/client/gameui.cpp` | Implement security overlays |
| `src/defaultsettings.cpp` | All security-related settings |
| `builtin/settingtypes.txt` | Updated secure_connection description with insecure mode behavior |
| `start_server.sh` | Interactive menu, secure/insecure toggle, CLI flags |
| `start_client.sh` | Interactive menu, secure/insecure toggle, CLI flags |

---

## Testing Strategy

### Unit Tests (Phase 1) — DONE
- AES-256-GCM encrypt/decrypt with known test vectors (NIST SP 800-38D)
- HKDF-SHA256 with RFC 5869 test vectors
- Nonce counter monotonicity
- Key derivation correctness (same SRP key produces same derived keys)

### Integration Tests (v9.3) — DONE
- `test_encryption_toggle.sh`: 14 tests verifying encryption toggle behavior
- Server starts in both secure and insecure modes
- Correct log messages for each mode
- Encryption config module is compiled into binaries

### Manual Verification
- Run server with `secure_connection = true`, connect with client
- Capture traffic with Wireshark on loopback
- Verify: UDP payload is NOT readable game data
- Verify: No `4f457403` protocol ID visible in data portion (it's encrypted)
- Switch to `secure_connection = false`, verify: traffic IS readable

---

## Honest Security Score Breakdown (v9.9+)

**Base scoring** (unchanged from v9.1):
| Property | Points | Status |
|----------|--------|--------|
| Encryption active | +30 | Real AES-256-GCM |
| Strong cipher suite | +15 | AES-256-GCM |
| Forward secrecy | +15 | ECDH X25519 (v9.11: now real) |
| Authentication | +15 | SRP password auth |
| Replay protection | +10 | Nonce counters + exact bitmap |
| Certificate verification | +10 | Pinned (returning) / TOFU (first) |
| TLS version | +5 | TLS 1.3 equivalent (v9.11: ECDH+AEAD+replay) |

**Bonus scoring** (v9.9, max +15):
| Bonus | Points | Condition |
|-------|--------|-----------|
| TOFU acknowledged | +3 | First connection with TOFU trust |
| Key rotation capable | +5 | Session rekeying implemented |
| Salted HKDF | +2 | HKDF uses derived salt |
| Exact replay bitmap | +2 | Bitmap tracking within sliding window |
| Integrity verified | +3 | Zero auth failures in session |

**Typical scores:** With ECDH: First connection 100/100 (Excellent), Returning 100/100 (Excellent). Without ECDH (SRP-only): First connection 85/100 (Good), Returning 90/100 (Good). Insecure: 0/100.

---

## v9.9–v9.10 Additions: Bonus Scoring & Bug Fixes

### v9.9: TDD Encryption Scoring & Bug Fixes

- Added bonus scoring fields to `ConnectionSecurityInfo`: `tofu_acknowledged`, `key_rotation_capable`, `salted_key_derivation`, `exact_replay_bitmap`, `integrity_verified`
- Added `getBonusScore()` and `getBonusBreakdown()` methods
- Extended `populateRealSecurityInfo()` with `key_rotation_supported` parameter (11-param overload)
- Added `DirectionalEncryptionState` replay bitmap (exact tracking within sliding window)
- Added `PeerEncryptionState::rotateKeys()` for session key rotation
- Added `PeerEncryptionState::hkdf_salt` field and `key_rotation_count`
- Fixed build errors: `i64` → `s64` type alias, `populateRealSecurityInfo` overload ordering
- Fixed encryption key mismatch: changed from random HKDF salt to deterministic salt derivation from SRP session key
- New test file: `test_security_score_v99.cpp`

### v9.10: Documentation & Encryption Data Flow

- Updated all project documentation for v9.10
- Created `docs/ENCRYPTION_DATA_FLOW.md` — comprehensive guide to encryption system
- Updated version in VERSION, CMakeLists.txt, and all MD files

### v9.11: ECDH X25519 Forward Secrecy

Implements real forward secrecy using ECDH X25519 key exchange on top of SRP authentication:

- **New wire protocol**: `TOCLIENT_ECDH_PUBKEY` (0x65) and `TOSERVER_ECDH_PUBKEY` (0x54) packet types
- **Server flow**: After `acceptAuth()`, generates X25519 keypair, sends `TOCLIENT_ECDH_PUBKEY`, waits for `TOSERVER_ECDH_PUBKEY`, computes shared secret, mixes into keys via `mixECDHSecretIntoKeys()`, activates encryption
- **Client flow**: Receives `TOCLIENT_ECDH_PUBKEY`, computes shared secret, mixes into keys, sends back `TOSERVER_ECDH_PUBKEY`, activates encryption with ECDH+SRP keys
- **Client defers encryption activation**: Instead of activating in `handleCommand_AuthAccept`, waits for `TOCLIENT_ECDH_PUBKEY` and activates in `handleCommand_EcdhPubkey`
- **Server defers activation**: Instead of activating immediately after `acceptAuth()`, waits for `TOSERVER_ECDH_PUBKEY` and activates in `handleCommand_EcdhPubkey`
- **Fixed `mixECDHSecretIntoKeys()`**: Was using unsalted HKDF (`nullptr, 0` for salt) — now derives salt deterministically from combined IKM (SRP + ECDH)
- **Fixed `rotateKeys()`**: Was using `secure_random()` for HKDF salt — now derives salt deterministically from rotation IKM
- **22 TDD tests** in `test_forward_secrecy.cpp`: X25519 key generation, ECDH shared secret consistency, salted HKDF key derivation, both-sides-same-keys, ECDH keys differ from SRP-only, full ECDH handshake encrypt/decrypt roundtrip, key rotation with deterministic salt, forward secrecy property, security scoring with/without ECDH, insecure mode does not attempt ECDH
- **Security score**: With ECDH completed: Forward secrecy +15, Key exchange = KEY_EXCHANGE_ECDH_X25519, TLS version = TLS_1_3_EQUIVALENT, typical score 100/100 (Excellent)

---

## Progress Tracker

| Phase | Status | Notes |
|-------|--------|-------|
| Phase 1: Crypto Infrastructure | DONE | AES-256-GCM, HKDF, secure random, all with OpenSSL 3.5.5 |
| Phase 2: Session Key Capture | DONE | Client + server extract SRP key before deletion |
| Phase 3: Encryption State in Connection | DONE | PeerEncryptionState on UDPPeer, SetPeerEncryptionState() |
| Phase 4: Packet Encryption/Decryption | DONE | Encrypt in rawSend(), decrypt in receive() |
| Phase 5: Replay Protection | DONE | Nonce counters + sliding window in DirectionalEncryptionState |
| Phase 6: Honest Security Info | DONE | populateRealSecurityInfo(), no more fake claims, score 70/100 |
| Phase 7: Toggleable Overlays | NOT STARTED | |
| Phase 8: Security Info Settings Tab | NOT STARTED | |
| Phase 9: CMake Build Integration | DONE | OpenSSL REQUIRED, crypto.cpp + encryption_config.cpp in build |
| Phase 10: CI/CD & Testing | DONE (v9.5–v9.6) | Consolidated CI, portable build system, CI passes on GitHub Actions |
| v9.2: Insecure mode fix | DONE | encryption_state.disable() when secure_connection=false |
| v9.3: Modular encryption | DONE | encryption_config.h/cpp centralized policy manager |
| v9.3: Interactive start scripts | DONE | start_server.sh + start_client.sh with menus |
| v9.3: Version numbering | DONE | VERSION file, CMakeLists VERSION_EXTRA, --version output |
| v9.3: Test script | DONE | test_encryption_toggle.sh with 14 TDD tests |
| v9.5: Consolidated CI | DONE | Single build.yml, toggleable options |
| v9.6: Portable build system | DONE | No hardcoded paths, auto-detect local-prefix, cross-PC support |
| v9.8: VS Code tasks | DONE | .vscode/tasks.json with Start Server, Build Both, Start Client |
| v9.9: Bonus encryption scoring | DONE | TOFU acknowledged, key rotation, salted HKDF, exact replay, integrity |
| v9.9: Build error fixes | DONE | i64→s64, overload ordering, deterministic HKDF salt |
| v9.10: Documentation update | DONE | All MDs updated, docs/ENCRYPTION_DATA_FLOW.md created |
| v9.11: ECDH X25519 forward secrecy | DONE | Real PFS via ECDH handshake, deferred encryption activation |
| v9.11: ECDH wire protocol | DONE | TOCLIENT_ECDH_PUBKEY (0x65), TOSERVER_ECDH_PUBKEY (0x54) |
| v9.11: ECDH salt bug fixes | DONE | mixECDHSecretIntoKeys unsalted HKDF, rotateKeys random salt |
| v9.11: Forward secrecy TDD tests | DONE | 22 tests in test_forward_secrecy.cpp |
| v9.11: Test bug fixes | DONE | TOFU bonus with CERT_PINNED, concurrent nonce mutex, tamper flag AAD, security score string |
| v9.11: Documentation update | DONE | VERSION→9.11, README, ai-codebase-reference, OPENSECURE_GUIDE |
| v9.11: Security score 100/100 | DONE | ECDH+SRP = Excellent with forward secrecy |
| v9.12: Encryption activation race condition fix | DONE | Client defers activation until server's first encrypted packet, receive path auto-activates on successful decrypt |
| v9.12: SetPeerEncryptionState field fix | DONE | Now copies all fields including ecdh_completed, hkdf_salt, key_rotation_count, ECDH keys |
| v9.12: Receive path 0x80 flag detection | DONE | Detects encrypted packets by flag byte regardless of active state, with key-initialized guard |
| v9.19: GCM auth spam fix | DONE | Prevent SetPeerEncryptionState from clobbering SRP keys before ECDH completes |
| v9.20: Fake encryption score fix | DONE | Use real connection state instead of hardcoded encryption_active=true |
| v9.21: Build error fix | DONE | Move isEncryptionActive() from client.h inline to client.cpp (incomplete type error) |
| v9.22: Settings panel fix | DONE | Write all 16 g_settings keys, sync activated_at from connection layer, both secure/insecure modes work |
| v9.23: Log toggle feature | DONE | Add --no-log/--log flags to start scripts, encryption_log_level setting (none/error/action/trace), log suppression prevents debug.txt and trace file creation |
| v9.24: Settingtypes context fix | DONE | Fix encryption_log_level context [server,client] → [common]; Luanti parser only accepts single context values (common/client/server/world_creation) |
| v9.25: Encryption log autocreate | DONE | encryption_trace.log created at any non-none level (not just trace); all enclog_* macros write to trace file via EncLogLine class; fixes missing log file after manual deletion |
| v9.26: Gitignore media folder | DONE | media/ added to .gitignore |
| v9.27: Project rename | DONE | Clawtest → Luanti-Secure across all files |
| v9.28: Minecraft-like keybinds | DONE | E=inventory, Ctrl=sprint, F5=camera, F3=debug, G=fog, C=zoom |
| v9.29–v9.37: Internal iterations | DONE | Build improvements, CI refinements, keypair GUI, overlap fixes |
| v9.38: Server info overlay | DONE | Tab key shows in-game overlay with server name, player list, ping, uptime |
| v9.39: Voice chat E2EE | DONE | Opus voice with X25519 ECDH + AES-256-GCM; push-to-talk; keypair auth |
| v9.40: Documentation consolidation | DONE | All docs moved to docs/; fixed voice_chat.lua crash; fixed Tab overlay toggle |
| v9.41–v9.44: Voice server authority | DONE | Voice packet routing, settingtypes fixes, keybind settings fix |
| v9.45–v9.49: Internal iterations | DONE | Clay GUI exploration (not merged), build improvements |
| v9.50: Centralized GUITheme system | DONE | 93 constants across 8 namespaces; 14 files refactored; 89 TDD tests; validate() |

---

## v9.19–v9.25: Encryption Spamming Problem & Solution

### The Problem: Encryption Log Spam

During development and testing of the AES-256-GCM encryption layer, verbose encryption logging produced excessive output that caused real performance problems:

- **180MB log file**: A single logging session generated a 180MB `debug.txt` file from per-packet encryption trace messages (`[ENC:TRACE]` entries for every encrypted/decrypted packet)
- **Game slowdown**: The I/O overhead of writing massive log files caused noticeable game lag, especially during high-traffic sessions (world loading, chunk generation, many players)
- **No way to disable**: The encryption log level was hardcoded to `action` (or `trace` during debugging), with no way to reduce or suppress output without recompiling
- **Unwanted even when encryption works**: Once encryption is confirmed working, the log output adds no value but continues to consume resources

### The Solution: Multi-Layered Log Control (v9.23 + v9.24)

Two complementary changes were made to solve this problem permanently:

**1. Log Toggle in Start Scripts (v9.23)**

The `start_client.sh` and `start_server.sh` scripts now accept `--no-log` (default) and `--log` flags:

- `--no-log` (default): The game starts with NO debug.txt creation and NO encryption trace file output. The `debug_log_level` setting is set to `none` at startup via the temp config, preventing any log data from being generated in the first place. This means zero I/O overhead from logging — no files are created, no data is written.
- `--log`: Enables normal logging with `debug_log_level = action`. The `encryption_log_level` setting controls how verbose the encryption-specific messages are.

The key design principle: **when logging is OFF, no extra data is generated at all**. This is not about redirecting or hiding output — the logging infrastructure itself is suppressed before the game even starts, via the temp config file that the startup script generates. This prevents the 180MB log file problem from ever recurring.

**2. Encryption Log Level Setting (v9.23 + v9.24)**

A new `encryption_log_level` setting controls the verbosity of `[ENC:...]` log messages independently of the general log level:

| Level | Output | Use Case |
|-------|--------|----------|
| `none` | No encryption log messages at all | Production servers, normal gameplay |
| `error` | Only `[ENC:ERROR]` — encryption failures | Monitoring for issues without noise |
| `action` | Activation, security, disable, and error events (default) | Development, first-time setup |
| `trace` | Everything including per-packet diagnostics | Debugging specific encryption issues |

**Bug fixed in v9.24**: The initial `encryption_log_level` setting used `[server,client]` as its context annotation in `settingtypes.txt`. The Luanti settingtypes parser (`builtin/common/settings/settingtypes.lua`) only accepts single context values (`common`, `client`, `server`, `world_creation`), so the comma-separated `[server,client]` was treated as an unknown context, producing `ERROR[Main]: Unknown context in settingtypes.txt`. Fixed by changing to `[common]` — the correct context for settings that apply to both server and client.

**Autocreate fix (v9.25)**: In v9.23/v9.24, `encryption_trace.log` was only created when `encryption_log_level = trace`. At the default "action" level (which is what `--log` sets), the file was never created. If a user manually deleted the file and restarted with `--log`, the file would not be recreated — a test-driven-development finding. In v9.25, two changes were made: (1) The trace file is now created at ANY non-none log level (error, action, or trace), not just at trace level. This is done by changing the guard in `ensureTraceFileOpen()` from `shouldLog(ENC_LOG_TRACE)` to `shouldLog(ENC_LOG_ERROR)`. (2) ALL `enclog_*` macros (not just `enclog_trace`) now write to the trace file via the new `EncLogLine` class, making `encryption_trace.log` the single destination for all encryption log events. The `EncLogLine` class is a generalization of the previous `TraceLine` class that supports any log level and any output stream, with automatic dual output to both the standard stream and the trace file.

### How to Use

```bash
# Normal gameplay — no logs, no overhead (default)
./start_client.sh --name player1 --address localhost --go

# Debug an encryption issue — enable full logging
./start_client.sh --name player1 --address localhost --log --go

# Fine-tune encryption log level in minetest.conf
encryption_log_level = none    # Zero encryption log noise
encryption_log_level = error   # Only failures
encryption_log_level = action  # Key events (default)
encryption_log_level = trace   # Per-packet diagnostics (generates large logs!)
```

---

## v9.50: Centralized GUITheme System

### The Problem: Hardcoded Magic Numbers

Before v9.50, all GUI styling values were hardcoded throughout 30+ source files in `src/gui/`. Colors like `video::SColor(140, 0, 0, 0)` appeared without explanation. Sizes like `padding = 7` had no documentation. When a value needed to change (e.g., making all modal backgrounds slightly darker), developers had to search every file for the specific magic number and update each occurrence individually. This was error-prone, inconsistent, and made it impossible to implement runtime theme switching.

### The Solution: Single Source of Truth

`src/gui/GUITheme.h` defines all GUI styling constants in one place using a namespace hierarchy:

- **`GUITheme::Colors`** — 35 color constants for modal backgrounds, tooltips, buttons, tables, chat, inventory, status text, profiler, and more
- **`GUITheme::Sizing`** — 42 layout/spacing constants for button heights, slot spacing, padding, dialog sizes, tooltip dimensions, scrollbar widths, etc.
- **`GUITheme::Timing`** — 11 animation/duration constants for cursor blink speed, chat console speed, status text duration, double-click thresholds, etc.
- **`GUITheme::ButtonModifiers`** — 2 hover/press color interpolation multipliers
- **`GUITheme::Fonts`** — 8 font mode and size constants
- **`GUITheme::Sounds`** — 1 default sound constant
- **`GUITheme::Dialogs`** — 6 standard dialog dimension constants
- **`GUITheme::validate()`** — Runtime validation ensuring all constants are in valid ranges

### Why Namespace Instead of Class

C++ does not allow nested namespaces inside a class. Using `namespace GUITheme` with nested `namespace Colors`, `namespace Sizing`, etc. allows clean hierarchical access like `GUITheme::Colors::MODAL_BG` while keeping all constants together. A class-based approach would require separate files or overly verbose access patterns.

### Test Coverage

89 TDD tests in `gui_test/gui_theme_test.cpp` validate:
- All 8 namespaces have correct value types and ranges
- `validate()` returns true with default values
- `validate()` returns false when any value is out of range
- Constants are referenced by their consuming files (no dead constants)

### Files Refactored

14 GUI files were updated to replace hardcoded values with GUITheme constants:
- `guiFormSpecMenu.cpp` — modal backgrounds, tooltip colors, focus border
- `guiButton.h/cpp` — button colors, hover/press modifiers
- `guiPasswordChange.cpp`, `guiVolumeChange.cpp`, `guiOpenURL.cpp` — dialog dimensions
- `guiChatConsole.h/cpp` — chat console colors and timing
- `guiInventoryList.h` — slot colors and border width
- `guiTable.h` — table colors and row padding
- `statusTextHelper.h/cpp` — status text colors and duration
- `guiEngine.cpp` — main menu header/footer padding
- `touchcontrols.cpp`, `touchscreeneditor.cpp` — touch overlay colors

### Future Work

- **Hot-reload**: `GUITheme_Init()` stubs exist for future runtime theme reloading
- **Config file loading**: Load theme values from a JSON/TOML config file instead of hardcoding
- **WYSIWYG editor**: A web-based theme editor was prototyped on `clawtest-v9.51-gui-theme-editor` branch
