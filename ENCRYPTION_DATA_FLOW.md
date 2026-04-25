# Encryption Data Flow — Clawtest v9.11

> A comprehensive guide to how encryption works between the Clawtest server and client, what happens to data at each stage, and what protections exist (and don't exist) at every point.

## Table of Contents

1. [Overview](#overview)
2. [Connection Lifecycle](#connection-lifecycle)
3. [Data at Each Stage](#data-at-each-stage)
4. [Key Derivation in Detail](#key-derivation-in-detail)
5. [Packet Format: Plaintext vs Encrypted](#packet-format-plaintext-vs-encrypted)
6. [Replay Protection: How It Works](#replay-protection-how-it-works)
7. [Security Score Explained](#security-score-explained)
8. [What Encryption Does NOT Protect](#what-encryption-does-not-protect)
9. [Insecure Mode: What Happens](#insecure-mode-what-happens)
10. [Threat Model](#threat-model)

## Overview

Clawtest uses AES-256-GCM authenticated encryption to protect all game traffic between server and client after SRP authentication completes. This means that once a player logs in with a password, all subsequent network packets are encrypted and authenticated — an attacker capturing network traffic cannot read game data, modify packets without detection, or replay old packets.

The encryption is derived from the SRP (Secure Remote Password) authentication that already exists in Luanti. SRP produces a 32-byte shared session key on both sides during login. In upstream Luanti, this key was discarded after authentication. Clawtest captures this key and uses it to derive AES-256-GCM encryption keys via HKDF-SHA256.

### Key Properties

| Property | Implementation | Detail |
|----------|---------------|--------|
| Encryption algorithm | AES-256-GCM | 256-bit key, 12-byte nonce, 16-byte auth tag |
| Key derivation | HKDF-SHA256 | Salted with derived salt, separate C2S/S2C keys |
| Authentication | SRP (Secure Remote Password) | Mutual password-based authentication |
| Forward secrecy | ECDH X25519 | Real PFS — ephemeral key exchange during auth, past sessions protected |
| Replay protection | Monotonic counters + exact bitmap | 64-position sliding window with per-bit tracking |
| Integrity | GCM authentication tag | Detects any tampering with ciphertext or AAD |
| Key separation | Separate C2S and S2C keys | Each direction has its own key, nonce base, and counter |

## Connection Lifecycle

The connection between client and server goes through distinct phases. Here is what happens at each point:

### Phase 1: Initial Connection (Plaintext)

When the client first connects to the server, ALL traffic is plaintext. This is necessary because the two sides have not yet established a shared secret. During this phase:

- Client sends a connection request to the server (UDP)
- Server responds with TOCLIENT_HELLO, which may include security flags indicating that encryption is supported
- The base protocol header (protocol_id, peer_id, channel) is always sent in plaintext — it must be, because the routing layer needs to read it before the packet reaches the decryption layer

**Data at this point:** Completely visible on the network. An attacker with Wireshark can see the server address, port, and the fact that a Luanti/Clawtest connection is being established. The protocol ID (0x4f457403) is visible in every packet.

### Phase 2: SRP Authentication (Plaintext but Protected)

The client and server authenticate using SRP (Secure Remote Password) protocol. SRP is designed so that:
- The password is never sent over the network (not even as a hash)
- An eavesdropper cannot derive the password from captured traffic
- Both sides derive the same 32-byte session key without ever transmitting it
- The SRP exchange itself is plaintext on the wire, but mathematically resistant to offline attack

**Data at this point:** The SRP parameters (verifier, salt, public ephemeral values) are visible, but the password and session key are not. An attacker sees the SRP exchange but cannot compute the session key without the password.

### Phase 2.5: ECDH Key Exchange (v9.11)

After SRP auth succeeds but BEFORE encryption activates, the server and client perform an ECDH X25519 key exchange to establish forward secrecy:

1. **Server** generates an ephemeral X25519 keypair during auth
2. **Server** sends `TOCLIENT_ECDH_PUBKEY` (0x65) with its 32-byte public key — this is sent BEFORE encryption activation
3. **Client** receives `TOCLIENT_ECDH_PUBKEY`, generates its own ephemeral X25519 keypair
4. **Client** computes the ECDH shared secret: `X25519(client_private, server_public)` → 32-byte shared secret
5. **Client** calls `mixECDHSecretIntoKeys()` which re-derives keys using both the SRP session key AND the ECDH shared secret as combined IKM
6. **Client** sends `TOSERVER_ECDH_PUBKEY` (0x54) with its 32-byte public key back to the server
7. **Server** receives `TOSERVER_ECDH_PUBKEY`, computes the same ECDH shared secret: `X25519(server_private, client_public)`
8. **Server** calls `mixECDHSecretIntoKeys()` with the same combined IKM
9. Both sides now have the same ECDH+SRP-derived encryption keys

Both sides derive keys using salted HKDF where the salt is derived deterministically from the combined IKM (SRP session key + ECDH shared secret), ensuring both sides produce identical keys without any additional salt exchange.

**Why this provides forward secrecy:** The ephemeral X25519 private keys exist only in memory during the session and are destroyed on disconnect. Even if the SRP password is later compromised, an attacker cannot reconstruct the ECDH shared secret without those ephemeral private keys, and therefore cannot derive the session encryption keys to decrypt past captured traffic.

**Data at this point:** Two new packet types on the wire (ECDH pubkeys), both in plaintext since encryption has not yet activated. The ECDH pubkeys themselves are not secret — only the shared secret computed from them is.

### Phase 3: Key Derivation (Local Only)

After SRP + ECDH complete, both sides independently derive encryption keys using HKDF-SHA256. This is a purely local operation — no data is sent over the network during key derivation. The derivation steps are:

1. **Derive HKDF salt**: `HKDF(SRP_key || ECDH_secret, no_salt, "Luanti v9 HKDF Salt")` → 16-byte salt
2. **Derive C2S key**: `HKDF(SRP_key || ECDH_secret, salt, "Luanti v9 C2S Key")` → 32-byte AES key
3. **Derive S2C key**: `HKDF(SRP_key || ECDH_secret, salt, "Luanti v9 S2C Key")` → 32-byte AES key
4. **Derive C2S nonce base**: `HKDF(SRP_key || ECDH_secret, salt, "Luanti v9 C2S Nonce")` → 4-byte base
5. **Derive S2C nonce base**: `HKDF(SRP_key || ECDH_secret, salt, "Luanti v9 S2C Nonce")` → 4-byte base
6. **Derive session ID**: `HKDF(SRP_key || ECDH_secret, salt, "Luanti v9 Session ID")` → 16-byte ID (hex string)

The IKM (input keying material) is the concatenation of the SRP session key and the ECDH shared secret. The salt is deterministically derived from this combined IKM so both sides produce the same salt without exchanging it. This is critical — if each side generated a random salt independently, the derived keys would differ and decryption would fail (this was a bug in v9.9 and v9.11 pre-release).

**Data at this point:** No network traffic. The keys exist only in memory on each side.

### Phase 4: Encryption Activation (Transition)

After key derivation, there is a brief transition period where:
- The server must finish sending any queued plaintext packets (like AUTH_ACCEPT)
- The client must finish receiving those plaintext packets
- Both sides then activate encryption simultaneously

This is handled by deferring the activation command: `CONNCMD_ACTIVATE_ENCRYPTION` is queued but not executed until all pending plaintext packets have been sent. Once activated, ALL future packets in that direction are encrypted.

**Data at this point:** A mix — the last few plaintext packets may be in flight while the first encrypted packets are being prepared. The activation is atomic per-direction.

### Phase 5: Encrypted Communication (Protected)

From this point forward, all game traffic is encrypted with AES-256-GCM:
- Client → Server: Encrypted with C2S key, C2S nonce base, monotonically increasing counter
- Server → Client: Encrypted with S2C key, S2C nonce base, monotonically increasing counter
- Each packet includes a 12-byte nonce and 16-byte GCM authentication tag
- Replay protection uses exact bitmap tracking within a 64-position sliding window

**Data at this point:** Fully encrypted. An attacker with Wireshark sees only the base header (protocol_id, peer_id, channel), the encrypted flag byte (0x80), the nonce, and the ciphertext + tag. The game data (player positions, chat messages, block data, inventory) is completely hidden.

```
  Client                              Server
    |                                    |
    |  ──── Phase 1: Plaintext ────►    |
    |      Connection Request            |
    |  ◄─── TOCLIENT_HELLO ────────     |
    |                                    |
    |  ──── Phase 2: SRP Auth ────►     |
    |      SRP Init / Exchange           |
    |  ◄─── SRP Response ──────────     |
    |      (session key derived)         |
    |                                    |
    |  ◄─── Phase 2.5: ECDH ──────     |
    |      TOCLIENT_ECDH_PUBKEY (0x65)  |
    |      (server X25519 pubkey)        |
    |  ──── TOSERVER_ECDH_PUBKEY ──►    |
    |      (0x54, client X25519 pubkey)  |
    |      (ECDH shared secret derived)  |
    |                                    |
    |  ──── Phase 3: Key Deriv ──►      |
    |      (LOCAL ONLY — no traffic)     |
    |      HKDF(SRP+ECDH) → keys        |
    |                                    |
    |  ──── Phase 4: Transition ─►      |
    |      Last plaintext packets        |
    |      ACTIVATE_ENCRYPTION           |
    |                                    |
    |  ════ Phase 5: Encrypted ═══►     |
    |      AES-256-GCM on all data       |
    |  ◄══ AES-256-GCM on all data ═══  |
    |                                    |
```

## Data at Each Stage

This section provides a detailed breakdown of what data exists and what state it is in at each point between the server and client.

### Server Memory

| Data | State | Protection |
|------|-------|------------|
| SRP password hash (verifier) | Stored in auth database | Hashed with salt, never sent to client |
| SRP session key | In PeerEncryptionState | Zeroed on disconnect |
| AES-256-GCM C2S key | In DirectionalEncryptionState | Used for encrypting outgoing packets |
| AES-256-GCM S2C key | In DirectionalEncryptionState | Used for decrypting incoming packets |
| HKDF salt | In PeerEncryptionState | Used for key derivation |
| ECDH private key | In PeerEncryptionState | Zeroed on disconnect |
| Nonce counters | In DirectionalEncryptionState | Monotonically increasing, never reused |
| Replay bitmap | In DirectionalEncryptionState | Tracks seen counters in 64-position window |

### Network (Wire)

| Data | Direction | State | Visible to Attacker? |
|------|-----------|-------|---------------------|
| Base header (7 bytes) | Both | Plaintext | Yes — protocol_id, peer_id, channel |
| Encrypted flag (1 byte) | Both | Plaintext | Yes — 0x00 or 0x80 |
| Nonce (12 bytes) | Both | Plaintext (but unique per packet) | Yes — but nonces are not secret |
| Ciphertext (variable) | Both | Encrypted | No — appears as random bytes |
| GCM auth tag (16 bytes) | Both | Authentication tag | No — tag is not the data |

### Client Memory

Same as server, with C2S and S2C roles swapped (client encrypts with C2S key, decrypts with S2C key).

### Client Display

| Data | Source | Visible to Player? |
|------|--------|--------------------|
| Security score | ConnectionSecurityInfo | Yes — in security overlay and info tab |
| Encryption algorithm | ConnectionSecurityInfo | Yes — "AES-256-GCM" |
| Session ID | ConnectionSecurityInfo | Yes — hex string |
| Server fingerprint | ConnectionSecurityInfo | Yes — "SHA256:abc..." |
| Forward secrecy status | ConnectionSecurityInfo | Yes — "Yes (ECDH)" or "No (SRP-derived)" |
| Auth failure count | ConnectionSecurityInfo | Yes — in security info tab |
| Encryption keys | PeerEncryptionState | NO — never displayed, zeroed on disconnect |

## Key Derivation in Detail

### Why Salted HKDF?

In v9.0-v9.8, HKDF was called without a salt (empty salt). In v9.9, a salt was added for stronger key separation between sessions. The salt is deterministically derived from the SRP session key itself:

```
HKDF-SHA256(
    IKM  = SRP_session_key (32 bytes),
    salt = null (empty),
    info = "Luanti v9 HKDF Salt"
) → hkdf_salt (16 bytes)
```

This salt is then used in all subsequent HKDF derivations. Because the salt is derived from the session key (not random), both sides produce the same salt and therefore the same encryption keys. A random salt would require a protocol change to exchange the salt over the wire.

### Nonce Construction

Each encrypted packet uses a unique 12-byte nonce constructed as:

```
[4-byte nonce base from HKDF][8-byte counter, big-endian]
```

- The 4-byte base is different for C2S and S2C (derived with different HKDF info strings)
- The 8-byte counter starts at 0 and increments by 1 for each packet
- The counter never wraps around (2^64 packets per direction is effectively infinite)
- Each nonce is used with exactly one key for exactly one packet — this is critical for AES-GCM security

### Why Separate C2S and S2C Keys?

Using separate keys for each direction prevents nonce collision attacks. If both directions used the same key, a packet sent C2S with nonce N and a packet sent S2C with nonce N would violate the AES-GCM requirement that each nonce be used only once per key. By using separate keys, the same nonce counter value in different directions is safe.

```
  Key Derivation Flow
  ═══════════════════

  SRP Session Key (32 bytes)
       │
       ▼
  ┌─────────────────────────────┐
  │  HKDF(IKM=key, salt=∅,     │
  │       info="Luanti v9       │
  │       HKDF Salt")           │
  └──────────┬──────────────────┘
             │
             ▼
       hkdf_salt (16 bytes)
             │
     ┌───────┴───────┐
     │               │
     ▼               ▼
  ┌──────────┐  ┌──────────┐
  │ HKDF     │  │ HKDF     │
  │ C2S Key  │  │ S2C Key  │
  └────┬─────┘  └────┬─────┘
       │              │
       ▼              ▼
  AES-256 key    AES-256 key
  (C2S: 32B)    (S2C: 32B)

       │              │
  ┌────┴─────┐  ┌────┴─────┐
  │ HKDF     │  │ HKDF     │
  │ C2S Nonce│  │ S2C Nonce│
  └────┬─────┘  └────┬─────┘
       │              │
       ▼              ▼
  Nonce base     Nonce base
  (C2S: 4B)     (S2C: 4B)
```

## Packet Format: Plaintext vs Encrypted

### Plaintext Packet (before encryption, or insecure mode)

```
Offset  Size    Field
0       4       Protocol ID (0x4f457403)
4       2       Peer ID
6       1       Channel
7       N       MTP Packet Data (readable game data)
```

### Encrypted Packet (after encryption activates)

```
Offset  Size    Field
0       4       Protocol ID (0x4f457403)     ← plaintext, needed for routing
4       2       Peer ID                      ← plaintext, needed for routing
6       1       Channel                      ← plaintext, needed for routing
7       1       Encrypted Flag (0x80)        ← plaintext, indicates this is encrypted
8       12      Nonce                        ← plaintext, unique per packet
20      N-16    Ciphertext                   ← ENCRYPTED, contains the MTP Packet Data
20+N-16 16     GCM Authentication Tag       ← plaintext, verifies integrity
```

**Overhead:** 29 bytes per packet (1 flag + 12 nonce + 16 tag). For a typical game packet of 100-500 bytes, this is approximately 6-29% overhead.

**The encrypted flag byte (0x80)** is included as Additional Authenticated Data (AAD) in the GCM encryption. This means the flag is authenticated but not encrypted — if an attacker tries to change the flag from 0x80 to 0x00 (trying to downgrade an encrypted packet to plaintext), the GCM tag verification will fail and the packet will be rejected.

```
  Plaintext Packet                        Encrypted Packet
  ════════════════                        ════════════════

  ┌──────────────┐                        ┌──────────────┐
  │ Protocol ID  │ 4B                     │ Protocol ID  │ 4B
  ├──────────────┤                        ├──────────────┤
  │ Peer ID      │ 2B                     │ Peer ID      │ 2B
  ├──────────────┤                        ├──────────────┤
  │ Channel      │ 1B                     │ Channel      │ 1B
  ├──────────────┤                        ├──────────────┤
  │              │                        │ Enc Flag     │ 1B (0x80)
  │              │                        ├──────────────┤
  │  MTP Packet  │                        │ Nonce        │ 12B
  │  Data        │ NB                     ├──────────────┤
  │  (readable)  │                        │              │
  │              │                        │ Ciphertext   │ (N-16)B
  │              │                        │ (encrypted)  │
  │              │                        ├──────────────┤
  └──────────────┘                        │ GCM Auth Tag │ 16B
                                          └──────────────┘

  Total: 7 + N bytes                     Total: 20 + N bytes
                                          Overhead: +13 bytes header
                                                    +16 bytes tag
```

## Replay Protection: How It Works

Replay protection prevents an attacker from capturing a valid encrypted packet and re-sending it later. Without replay protection, an attacker could record a "move player north" packet and replay it to move the player without their consent.

### Sliding Window with Exact Bitmap (v9.9+)

Each direction maintains a high-water mark (the highest nonce counter seen) and a 64-bit bitmap tracking which counters within the window have been seen.

When a packet arrives:
1. If its counter > high-water mark: accept (advance the high-water mark, shift the bitmap)
2. If its counter is within 64 positions behind the high-water mark: check the bitmap
   - Bit not set: first time seeing this counter → accept, set the bit
   - Bit already set: already seen this counter → REJECT (replay detected!)
3. If its counter is more than 64 positions behind: REJECT (too old, likely replay)

The bitmap shift is efficient — when the high-water mark advances by N positions, the bitmap shifts right by N bits, with new positions filled with zeros.

### What This Prevents

- Exact duplicate packets: rejected by bitmap check
- Out-of-order packets within the window: accepted (bitmap tracks each position individually)
- Very old packets: rejected by the window boundary check
- Counter reset attacks: impossible (counters only increase, 64-bit space)

```
  Replay Protection: Sliding Window with Exact Bitmap
  ════════════════════════════════════════════════════

  High-water mark = 100

  Bitmap (64 bits, one per counter position):
  ┌─────────────────────────────────────────────────────────────────┐
  │ Counter:  37  38  39  40  41  42  ...  97  98  99  100         │
  │ Bit:       1   0   1   1   0   1  ...   1   1   1    1        │
  └─────────────────────────────────────────────────────────────────┘
              ▲                                       ▲         ▲
              │                                       │         │
         Behind window                           In window   Current
         → REJECT                                → check bit  high-water

  Packet arrives with counter = 98:
    → 98 is within 64 of 100 → check bit for 98 → bit is 1 (seen)
    → REJECT: replay detected!

  Packet arrives with counter = 42:
    → 42 is within 64 of 100 → check bit for 42 → bit is 1 (seen)
    → REJECT: replay detected!

  Packet arrives with counter = 41:
    → 41 is within 64 of 100 → check bit for 41 → bit is 0 (not seen)
    → ACCEPT: set bit for 41

  Packet arrives with counter = 30:
    → 30 is more than 64 behind 100 → REJECT: too old
```

## Security Score Explained

The security score is an honest assessment of the connection's protection level. It is calculated from two components:

### Base Score (max 100)

| Property | Points | When Awarded |
|----------|--------|-------------|
| Encryption active | +30 | State is Encrypted (AES-256-GCM is running) |
| Strong cipher suite | +15 | AES-256-GCM or ChaCha20-Poly1305 |
| Forward secrecy | +15 | ECDH X25519 key exchange completed |
| Authentication | +15 | SRP password authentication |
| Replay protection | +10 | Nonce counters + exact bitmap active |
| Certificate verification | +10 | Fingerprint pinned (returning connection) |
| TLS version | +5 | TLS 1.3 equivalent (ECDH+AEAD+replay) |

### Bonus Score (v9.9+, max +15, only with encryption active)

| Bonus | Points | When Awarded |
|-------|--------|-------------|
| TOFU acknowledged | +3 | First connection with TOFU trust model |
| Key rotation capable | +5 | PeerEncryptionState supports rotateKeys() |
| Salted HKDF | +2 | HKDF uses derived salt in key derivation |
| Exact replay bitmap | +2 | Bitmap tracking within sliding window |
| Integrity verified | +3 | Zero GCM auth failures in current session |

### Score Examples

| Scenario | Base | Bonus | Total | Label |
|----------|------|-------|-------|-------|
| No encryption (insecure mode) | 0 | 0 | 0 | Insecure |
| SRP-only encryption (no ECDH, first connection) | 70 | 15 | 85 | Good |
| SRP-only encryption, returning (pinned fingerprint) | 80 | 15 | 95 | Good |
| ECDH+SRP encryption, first connection | 85 | 15 | 100 | Excellent |
| ECDH+SRP encryption, returning (pinned fingerprint) | 100 | 15 | 100 | Excellent |

Note: The score is capped at 100 for display. Bonus points help first-time TOFU connections reach higher scores honestly, rather than pretending that TOFU provides the same verification as certificate pinning.

```
  Security Score Breakdown Visualization
  ══════════════════════════════════════

  ┌─────────────────────────────────────────────────────┐
  │                BASE SCORE (max 100)                 │
  │                                                     │
  │  Encryption ───── +30 ████████████                  │
  │  Cipher Suite ─── +15 ██████                        │
  │  Forward Secrecy ─ +15 ██████                       │
  │  Authentication ── +15 ██████                       │
  │  Replay Protect ── +10 ████                         │
  │  Cert Verify ───── +10 ████                         │
  │  TLS Equivalent ── +5  ██                           │
  │                                                     │
  ├─────────────────────────────────────────────────────┤
  │              BONUS SCORE (max +15)                  │
  │                                                     │
  │  TOFU Ack ──────── +3  ██                           │
  │  Key Rotation ──── +5  ███                          │
  │  Salted HKDF ───── +2  █                            │
  │  Exact Bitmap ──── +2  █                            │
  │  Integrity OK ──── +3  ██                           │
  └─────────────────────────────────────────────────────┘
```

## What Encryption Does NOT Protect

Being honest about limitations is a core principle of Clawtest's security design. Here is what the encryption does NOT protect against:

### Traffic Analysis
An attacker can still see:
- The fact that the client and server are communicating (packet timing, sizes, frequency)
- The server address and port
- Rough packet sizes (which may reveal game activity patterns)
- When the connection starts and stops

The encryption hides the CONTENT of packets, not their EXISTENCE.

### Endpoint Compromise
If the server or client machine itself is compromised:
- The attacker can read encryption keys from process memory
- The attacker can intercept data before encryption or after decryption
- The attacker can modify the game client to bypass encryption entirely

Encryption protects data in transit, not on the endpoints.

### SRP Password Quality
The encryption's strength depends on the SRP session key, which depends on the password:
- Weak passwords can be brute-forced from captured SRP exchanges
- If the password is compromised, the session key is compromised
- **Without ECDH**: If the password is compromised, past sessions can be decrypted
- **With ECDH (v9.11)**: Even if the password is later compromised, past sessions encrypted with ECDH+SRP-derived keys remain safe because the ephemeral X25519 private keys are destroyed after the session

ECDH X25519 forward secrecy (v9.11) directly mitigates the password compromise risk: the encryption keys are derived from both the SRP session key AND the ephemeral ECDH shared secret. Since the ephemeral private keys no longer exist after disconnect, there is no way to reconstruct the ECDH shared secret from captured traffic alone, regardless of whether the password is later cracked.

### Protocol Downgrade
An attacker with a man-in-the-middle position could potentially:
- Block the ECDH key exchange, forcing SRP-only encryption (score drops from 85→70)
- Block encryption entirely, forcing plaintext (if the client allows insecure mode)

Clawtest mitigates this by always attempting encryption by default and warning the user when it is not available.

### Metadata Leakage
The base protocol header (7 bytes) is always plaintext:
- Protocol ID reveals this is Luanti/Clawtest traffic
- Peer ID and channel reveal connection structure
- The encrypted flag (0x80) reveals that encryption is active

This is by design: the routing layer must read these fields before the packet reaches the decryption layer.

```
  What Encryption Protects vs. Doesn't
  ═════════════════════════════════════

  ┌──────────────────────────┐    ┌──────────────────────────┐
  │     PROTECTED ✓          │    │    NOT PROTECTED ✗        │
  │                          │    │                          │
  │  • Packet contents       │    │  • Packet existence      │
  │  • Chat messages         │    │  • Packet timing         │
  │  • Player positions      │    │  • Packet sizes          │
  │  • Inventory data        │    │  • Server address/port   │
  │  • World/mod data        │    │  • Connection duration   │
  │  • Password (SRP)        │    │  • Protocol metadata     │
  │  • Packet integrity      │    │  • Endpoint memory       │
  │  • Against replay        │    │  • Weak passwords        │
  │  • Against tampering     │    │  • MITM on first connect │
  └──────────────────────────┘    └──────────────────────────┘
```

## Insecure Mode: What Happens

When `secure_connection = false` (insecure mode):
- SRP authentication still runs normally (passwords are verified)
- The SRP session key is captured but NOT used for key derivation
- `encryption_state.disable()` is called instead of `initFromSRPSessionKey()`
- All game traffic is sent as plaintext UDP (same as upstream Luanti)
- The security overlay shows "INSECURE CONNECTION" in red
- The security score is 0/100 (Insecure)

Insecure mode exists for:
- LAN testing where encryption is unnecessary
- Debugging network protocol issues
- Performance benchmarking (measuring encryption overhead)
- Compatibility with clients that do not support encryption

**Warning:** In insecure mode, ALL game data (chat messages, player positions, world data, passwords during SRP) is visible on the network. Only use insecure mode on trusted networks.

```
  Insecure Mode vs. Secure Mode
  ═════════════════════════════

  Secure Mode (secure_connection = true)     Insecure Mode (secure_connection = false)
  ┌───────────────────────────────┐          ┌───────────────────────────────┐
  │  SRP Auth ──► Session Key     │          │  SRP Auth ──► Session Key     │
  │       │                       │          │       │                       │
  │       ▼                       │          │       ▼                       │
  │  HKDF Key Derivation         │          │  encryption_state.disable()   │
  │       │                       │          │       │                       │
  │       ▼                       │          │       ▼                       │
  │  AES-256-GCM Encrypt/Decrypt │          │  All packets: PLAINTEXT UDP  │
  │       │                       │          │       │                       │
  │       ▼                       │          │       ▼                       │
  │  Score: 70-100                │          │  Score: 0                     │
  │  Label: Good / Excellent      │          │  Label: INSECURE              │
  └───────────────────────────────┘          └───────────────────────────────┘
```

## Threat Model

### Attacker Capabilities and Defenses

| Attacker Capability | Can They... | Defense |
|--------------------|-----------|---------|
| Passive eavesdropping (Wireshark) | Read game data? | NO — AES-256-GCM encrypts all post-auth traffic |
| Packet tampering | Modify game data? | NO — GCM auth tag detects any modification |
| Replay attack | Resend old packets? | NO — Exact bitmap + sliding window rejects replays |
| Offline password attack | Crack password from capture? | Hard — SRP resists offline attacks, but weak passwords are vulnerable |
| Man-in-the-middle (first connection) | Impersonate server? | Possible — TOFU model, no PKI. Fingerprint pinning catches subsequent MITM |
| Man-in-the-middle (returning) | Impersonate server? | NO — Fingerprint mismatch detected and warned |
| Traffic analysis | Determine game activity? | Partially — Packet sizes and timing visible, content hidden |
| Endpoint compromise | Read decrypted data? | YES — Encryption protects transit, not endpoints |
| Quantum computer (future) | Break encryption? | AES-256 believed resistant; SRP is not quantum-resistant |

### Trust Model

1. **Server is trusted** — The server sees all decrypted data by necessity
2. **Client is trusted** — The player's machine handles decrypted data
3. **Network is untrusted** — All data in transit is encrypted and authenticated
4. **First connection is TOFU** — The server's fingerprint is recorded on first connection and verified on subsequent connections
5. **Password must be strong** — The entire security model depends on SRP, which depends on password quality

```
  Threat Model Summary
  ═══════════════════

                    ┌─────────────────────────────┐
                    │        ATTACKER              │
                    │                              │
                    │  Capabilities:               │
                    │  • Sniff network traffic     │
                    │  • Inject/modify packets     │
                    │  • Replay old packets        │
                    │  • MITM (on first connect)   │
                    └──────────┬──────────────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐
  │   Network     │  │   Protocol    │  │  Password     │
  │   Layer       │  │   Layer       │  │  Layer         │
  │               │  │               │  │               │
  │ DEFENSE:      │  │ DEFENSE:      │  │ DEFENSE:      │
  │ AES-256-GCM   │  │ Replay bitmap │  │ SRP auth      │
  │ encryption    │  │ GCM auth tag  │  │ (no pw sent)  │
  │               │  │ AAD on flag   │  │ Fingerprint   │
  │ RESULT:       │  │               │  │ pinning       │
  │ Data hidden ✓ │  │ RESULT:       │  │               │
  │               │  │ Tamper ✗      │  │ RESULT:       │
  │               │  │ Replay ✗      │  │ MITM caught ✓ │
  │               │  │               │  │ (on return)   │
  └───────────────┘  └───────────────┘  └───────────────┘
```
