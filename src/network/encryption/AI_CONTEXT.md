# Module: Encryption Layer (src/network/encryption/)

## Purpose
Provides authenticated encryption, key derivation, replay protection, and security scoring for network connections.

## Key Classes
- `IPacketEncryptor` — Abstract interface for packet encrypt/decrypt
- `IReplayProtector` — Abstract interface for replay defense  
- `IKeyDeriver` — Abstract interface for key derivation
- `BitmapReplayProtector` — Concrete bitmap-based sliding window replay protection
- `HKDFKeyDeriver` — Concrete HKDF-SHA256 key derivation (unified from 3 duplicated functions)
- `PeerEncryptionState` — Full peer encryption state (keys, nonces, ECDH, fingerprints)
- `FingerprintStore` — TOFU fingerprint persistence
- `ConnectionSecurityInfo` — Security scoring and display
- `EncryptionLogger` — Structured encryption logging
- `PacketTrace` — Packet lifecycle tracing

## Key Derivation Flow
```
SRP session key (32 bytes)
    │
    ├─[initFromSRPSessionKey]──→ HKDF → C2S key, S2C key, C2S nonce, S2C nonce, session ID
    │
    ├─[mixECDHSecretIntoKeys]──→ ECDH shared + SRP key → HKDF → new C2S/S2C keys
    │
    └─[rotateKeys]──→ Current key material → HKDF → new C2S/S2C keys (local-only)
```

## Dependencies
- Depends on: OpenSSL (AES-256-GCM, HKDF-SHA256, X25519)
- Depended on by: mtp/threads.cpp (rawSend, receive), client/server handlers

## Invariants
- Key derivation always uses HKDF-SHA256 with direction-specific info strings
- C2S/S2C keys are NEVER the same (different HKDF info strings)
- FingerprintStore is the ONLY file I/O in the encryption layer
