# Clawtest v9: Real Encryption Implementation Plan

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

**STATUS: COMPLETE** — Phases 1-6, 9, and 10 are done. Phases 7, 8 remain (UI enhancements, not security-critical).

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
- **No Forward secrecy**: Key is derived from password, not ephemeral ECDH
  - If the password is compromised, past sessions can be decrypted
  - Future: Could add X25519 key exchange on top for PFS
- **No Certificate-based trust**: No PKI, no CA verification
  - Uses "Trust On First Use" (TOFU) model with SRP verifier hash
  - Future: Could add self-signed cert management
- **No Quantum resistance**: AES-256 is believed quantum-resistant, but SRP is not

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
- Encrypted flag byte: `0x00` = plaintext, `0x01` = AES-256-GCM encrypted
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
    +-- HKDF(salt="", info="Luanti v9 C2S Key") -> Client->Server AES key (32 bytes)
    +-- HKDF(salt="", info="Luanti v9 S2C Key") -> Server->Client AES key (32 bytes)
    +-- HKDF(salt="", info="Luanti v9 C2S Nonce Base") -> C2S nonce base (4 bytes)
    +-- HKDF(salt="", info="Luanti v9 S2C Nonce Base") -> S2C nonce base (4 bytes)
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

## Honest Security Score Breakdown

| Property | Points | v9 Status |
|----------|--------|-----------|
| Encryption active | +30 | Real AES-256-GCM |
| Strong cipher suite | +15 | AES-256-GCM |
| Forward secrecy | +0 | Not available (SRP-derived) |
| Authentication | +15 | SRP password auth |
| Replay protection | +10 | Nonce counters + sliding window |
| Certificate verification | +0 | TOFU only |
| TLS version | +0 | Custom protocol |
| **Total** | **70/100** | **Good (honestly)** |

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
