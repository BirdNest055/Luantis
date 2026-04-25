# Luanti v9: Real Encryption Implementation Plan

## The Problem (Discovered via Wireshark)

The current `secure_connection` feature is **security theater**:
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

---

## Architecture: SRP-Derived AES-256-GCM

### Why This Approach

1. SRP authentication already produces a **32-byte shared session key** on both sides
2. The key is currently **discarded** after auth succeeds — we just need to capture it
3. OpenSSL 3.5.5 is available on the system with full AES-256-GCM + HKDF support
4. No new network round-trips needed — encryption activates immediately after SRP auth

### What This Gives Us (REAL)
- ✅ **Packet encryption**: AES-256-GCM encrypts all post-auth game traffic
- ✅ **Authentication**: SRP proves both sides know the password
- ✅ **Integrity**: GCM authentication tag detects tampering
- ✅ **Replay protection**: Monotonic nonce counters with sliding window
- ✅ **Key separation**: HKDF derives separate C2S and S2C keys
- ✅ **Wireshark-proof**: Captured packets show ciphertext, not game data

### What This Does NOT Give Us (HONEST LIMITATIONS)
- ❌ **Forward secrecy**: Key is derived from password, not ephemeral ECDH
  - If the password is compromised, past sessions can be decrypted
  - Future: Could add X25519 key exchange on top for PFS
- ❌ **Certificate-based trust**: No PKI, no CA verification
  - Uses "Trust On First Use" (TOFU) model with SRP verifier hash
  - Future: Could add self-signed cert management
- ❌ **Quantum resistance**: AES-256 is believed quantum-resistant, but SRP is not

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

### Phase 1: Crypto Infrastructure (NEW FILES)
**Files**: `src/network/crypto.h`, `src/network/crypto.cpp`
**Tests**: `src/network/crypto_test.cpp`

Create the cryptographic primitive layer:
- `AES256GCM` class: encrypt/decrypt with 12-byte nonce + 16-byte tag
- `HKDFSHA256` function: derive keys from SRP session key
- `SecureRandom` function: generate nonces, session IDs
- Unit tests for all crypto primitives (known test vectors from RFCs)

**Key derivation schema**:
```
SRP Session Key (32 bytes)
    │
    ├── HKDF(salt="", info="Luanti v9 C2S Key") → Client→Server AES key (32 bytes)
    ├── HKDF(salt="", info="Luanti v9 S2C Key") → Server→Client AES key (32 bytes)
    ├── HKDF(salt="", info="Luanti v9 C2S Nonce Base") → C2S nonce base (4 bytes)
    └── HKDF(salt="", info="Luanti v9 S2C Nonce Base") → S2C nonce base (4 bytes)
```

Nonce format: `[4-byte base from HKDF][8-byte counter]`
- Base ensures different sessions have different nonce spaces
- Counter increments per packet (monotonic, never reuses)
- 8-byte counter gives 2^64 packets before nonce exhaustion

### Phase 2: Session Key Capture
**Files**: Modify `src/network/clientpackethandler.cpp`, `src/network/serverpackethandler.cpp`, `src/network/clientiface.cpp`, `src/client/client.h`, `src/client/client.cpp`

**Client side** (in `handleCommand_AuthAccept()`):
- BEFORE calling `deleteAuthData()`, call `srp_user_get_session_key()`
- Store 32-byte key in `Client::m_encryption_session_key`
- Pass to connection layer for key derivation

**Server side** (in `handleCommand_SrpBytesM()`, after verification):
- BEFORE calling `acceptAuth()`, call `srp_verifier_get_session_key()`
- Store 32-byte key in `ClientInterface` / `RemoteClient`
- Pass to connection layer for key derivation

**Also**: Handle FIRST_SRP case (new account) — no SRP exchange, so no session key.
In this case, encryption is not possible; fall back to plaintext with honest UI.

### Phase 3: Encryption State in Connection Layer
**Files**: Modify `src/network/mtp/impl.h`, `src/network/mtp/impl.cpp`, `src/network/mtp/internal.h`

Add encryption state to the `Connection` and `UDPPeer` classes:
- `bool m_encryption_active = false` — is encryption currently active?
- `std::array<u8, 32> m_c2s_key` — client-to-server encryption key
- `std::array<u8, 32> m_s2c_key` — server-to-client encryption key
- `std::array<u8, 4>  m_c2s_nonce_base` — nonce base for C2S
- `std::array<u8, 4>  m_s2c_nonce_base` — nonce base for S2C
- `u64 m_c2s_nonce_counter = 0` — monotonic C2S nonce counter
- `u64 m_s2c_nonce_counter = 0` — monotonic S2C nonce counter
- Method: `enableEncryption(const u8* session_key, size_t key_len, bool is_server)`

### Phase 4: Packet Encryption/Decryption
**Files**: Modify `src/network/mtp/threads.cpp`, `src/network/mtp/impl.cpp`

**Encrypt path** (in `rawSend()`):
```
1. Check if encryption_active for this peer
2. If yes:
   a. Build nonce from base + counter
   b. Encrypt packet data (after base header) with AES-256-GCM
   c. Prepend encrypted flag (0x01) + nonce (12B)
   d. Append GCM tag (16B)
   e. Increment nonce counter
3. If no:
   a. Send plaintext (no encrypted flag, data as-is)
```

**Decrypt path** (in `receive()`):
```
1. After stripping base header, check encrypted flag byte
2. If 0x01 (encrypted):
   a. Read nonce (12B)
   b. Read GCM tag (last 16B)
   c. Decrypt ciphertext with AES-256-GCM
   d. Verify GCM tag (authentication + integrity)
   e. If tag fails: DROP packet, log warning, increment error counter
   f. If tag passes: use decrypted data as MTP packet
3. If 0x00 (plaintext):
   a. Process normally (pre-auth packets)
```

### Phase 5: Replay Protection
**Files**: Modify `src/network/mtp/internal.h`, `src/network/mtp/threads.cpp`

- Track last received nonce counter per direction per peer
- Accept packets with counter >= last_accepted (allow small reordering)
- Sliding window of 64 packets for out-of-order but legitimate packets
- Reject packets with counter significantly behind (likely replay)
- Log replay detection events

### Phase 6: Honest Security Info
**Files**: Modify `src/network/connection_security.h`, `src/network/clientpackethandler.cpp`, `src/network/serverpackethandler.cpp`

Replace fake `connectionSecurityInfoFromFlags()` with **real data**:

```cpp
// Instead of blindly setting from flags, populate from ACTUAL state:
ConnectionSecurityInfo info;
if (encryption_actually_active) {
    info.state = ConnectionSecurity::Encrypted;
    info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
    info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
    info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_SRP;  // NEW constant
    info.authentication = ConnectionSecurityInfo::AUTH_SRP;
    info.replay_protection = true;
    info.forward_secrecy = false;  // HONEST — SRP doesn't provide PFS
    info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;  // NEW constant
    info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;  // NEW constant — not TLS
    info.server_fingerprint = real_fingerprint_from_srp_verifier;
    info.session_id = derived_from_key_material;
} else {
    info.state = ConnectionSecurity::Insecure;
    // All fields stay at default "None"/"N/A"
}
```

**New constants to add to `ConnectionSecurityInfo`**:
- `KEY_EXCHANGE_SRP = 3` — SRP-based key exchange
- `CERT_TRUST_ON_FIRST_USE = 4` — TOFU trust model
- `TLS_CUSTOM = 3` — Custom protocol, not standard TLS

### Phase 7: Toggleable Overlay System
**Files**: Modify `src/client/gameui.h`, `src/client/gameui.cpp`, `src/defaultsettings.cpp`

**Three overlay modes** (toggleable from Settings):

1. **Mini** (`security_overlay_mode = "mini"`)
   - Small lock icon: 🔒 (encrypted) or 🔓 (insecure)
   - Top-right corner, minimal screen space
   - Color-coded: green = encrypted, red = insecure

2. **Standard** (`security_overlay_mode = "standard"`)
   - Lock icon + status text: "🔒 AES-256-GCM" or "🔓 Not Encrypted"
   - Shows encryption algorithm name
   - Same position, slightly larger

3. **Detailed** (`security_overlay_mode = "detailed"`)
   - Multi-line panel:
   ```
   🔒 ENCRYPTED CONNECTION
   Cipher: AES-256-GCM | Key: SRP
   Replay: Protected | PFS: No
   Score: 70/100 (Good)
   ```
   - Semi-transparent background
   - Shows all key security properties at a glance

4. **Off** (`security_overlay_mode = "off"`)
   - No overlay shown at all
   - Security info still available in Settings tab

**New settings**:
- `security_overlay_mode` — "mini" / "standard" / "detailed" / "off" (default: "standard")
- `security_overlay_position` — "top-right" / "top-left" / "bottom-right" / "bottom-left" (default: "top-right")

### Phase 8: Security Info Settings Tab
**Files**: Modify Lua settings tab, `src/defaultsettings.cpp`

Real security info tab with:
- **Status Banner**: Green "ENCRYPTED" / Red "NOT ENCRYPTED" / Yellow "ENCRYPTED (Limited)"
- **Encryption Details**: Real cipher, real key exchange, real nonce state
- **Honest Limitations**: "No Forward Secrecy" explained, "Trust On First Use" explained
- **Session Info**: Session ID, connected since, server fingerprint (from real key)
- **Packet Stats**: Packets encrypted/decrypted, replay attempts detected, auth failures
- **Security Score**: Honest score based on real properties
  - +30 for encryption (real AES-256-GCM)
  - +15 for strong cipher suite
  - +15 for authentication (SRP)
  - +10 for replay protection
  - +0 for forward secrecy (honestly: we don't have it)
  - +0 for certificate verification (honestly: TOFU only)
  - Score: 70/100 (Good) — not perfect, but honest

### Phase 9: CMake Build Integration
**Files**: Modify `src/CMakeLists.txt`, `src/network/CMakeLists.txt`

- Add `crypto.cpp` to network source list
- Ensure `ENABLE_OPENSSL=ON` is required for v9
- Add `find_package(OpenSSL 3.0 REQUIRED)` for crypto module
- Link `OpenSSL::Crypto` to the network library target

### Phase 10: CI/CD & Testing
**Files**: Modify `.github/workflows/build.yml`

- Ensure `libssl-dev` is in CI dependencies
- Build with `-DENABLE_OPENSSL=ON`
- Run crypto unit tests in CI
- Verify build produces working client + server

---

## File Change Summary

### NEW FILES
| File | Purpose |
|------|---------|
| `src/network/crypto.h` | AES-256-GCM, HKDF, secure random declarations |
| `src/network/crypto.cpp` | OpenSSL-based implementation |
| `src/network/crypto_test.cpp` | Unit tests with RFC test vectors |

### MODIFIED FILES
| File | Changes |
|------|---------|
| `src/network/connection_security.h` | New constants (SRP, TOFU, Custom TLS), remove fake defaults |
| `src/network/clientpackethandler.cpp` | Extract SRP session key, populate real security info |
| `src/network/serverpackethandler.cpp` | Extract SRP session key, set real security flags |
| `src/network/clientiface.cpp` | Store session key before verifier deletion |
| `src/network/mtp/impl.h` | Add encryption state to Connection class |
| `src/network/mtp/impl.cpp` | Implement enableEncryption(), encrypt/decrypt helpers |
| `src/network/mtp/internal.h` | Add encryption state to UDPPeer |
| `src/network/mtp/threads.cpp` | Encrypt in rawSend(), decrypt in receive() |
| `src/client/client.h` | Add session key storage, encryption state |
| `src/client/client.cpp` | Wire encryption into client lifecycle |
| `src/client/gameui.h` | Add overlay mode support |
| `src/client/gameui.cpp` | Implement mini/standard/detailed overlays |
| `src/defaultsettings.cpp` | New overlay mode settings |
| `src/network/CMakeLists.txt` | Add crypto.cpp, link OpenSSL |
| `src/CMakeLists.txt` | Ensure OpenSSL is required |
| `.github/workflows/build.yml` | Add libssl-dev, ensure ENABLE_OPENSSL |

---

## Testing Strategy

### Unit Tests (Phase 1)
- AES-256-GCM encrypt/decrypt with known test vectors (NIST SP 800-38D)
- HKDF-SHA256 with RFC 5869 test vectors
- Nonce counter monotonicity
- Key derivation correctness (same SRP key → same derived keys)

### Integration Tests
- Build client + server with encryption enabled
- Wireshark capture confirms ciphertext (no plaintext game data)
- Server with encryption disabled: client shows "INSECURE"
- Server with encryption enabled: client shows "ENCRYPTED" with real details
- Auth failure: no encryption (keys never established)
- Packet tampering: GCM tag verification fails, packet dropped
- Replay attack: nonce counter rejects replayed packets

### Manual Verification
- Run server with `secure_connection = true`
- Connect with client
- Capture traffic with Wireshark on loopback
- Verify: UDP payload is NOT readable game data
- Verify: No `4f457403` protocol ID visible in data portion (it's encrypted)

---

## Honest Security Score Breakdown

| Property | Points | v9 Status |
|----------|--------|-----------|
| Encryption active | +30 | ✅ Real AES-256-GCM |
| Strong cipher suite | +15 | ✅ AES-256-GCM |
| Forward secrecy | +15 | ❌ Not available (SRP-derived) |
| Authentication | +15 | ✅ SRP password auth |
| Replay protection | +10 | ✅ Nonce counters + sliding window |
| Certificate verification | +10 | ❌ TOFU only |
| TLS version | +5 | ❌ Custom protocol |
| **Total** | **70/100** | **Good (honestly)** |

---

## Progress Tracker

| Phase | Status | Notes |
|-------|--------|-------|
| Phase 1: Crypto Infrastructure | ✅ Done | AES-256-GCM, HKDF, secure random, all with OpenSSL 3.5.5 |
| Phase 2: Session Key Capture | ✅ Done | Client + server extract SRP key before deletion |
| Phase 3: Encryption State in Connection | ✅ Done | PeerEncryptionState on UDPPeer, SetPeerEncryptionState() |
| Phase 4: Packet Encryption/Decryption | ✅ Done | Encrypt in rawSend(), decrypt in receive() |
| Phase 5: Replay Protection | ✅ Done | Nonce counters + sliding window in DirectionalEncryptionState |
| Phase 6: Honest Security Info | ✅ Done | populateRealSecurityInfo(), no more fake claims, score 70/100 |
| Phase 7: Toggleable Overlays | ⬜ Not started | |
| Phase 8: Security Info Settings Tab | ⬜ Not started | |
| Phase 9: CMake Build Integration | ✅ Done | OpenSSL REQUIRED, crypto.cpp in build |
| Phase 10: CI/CD & Testing | ⬜ Not started | |
