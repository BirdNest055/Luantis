# ADR 002: Why AES-256-GCM Not ChaCha20-Poly1305

## Status
Accepted

## Context
The encryption layer needs an Authenticated Encryption with Associated Data (AEAD) cipher for packet encryption. Both AES-256-GCM and ChaCha20-Poly1305 are modern AEAD ciphers with comparable security properties:

- **AES-256-GCM**: NIST standard, widely deployed, hardware-accelerated on x86 via AES-NI instructions.
- **ChaCha20-Poly1305**: RFC 8439, constant-time software implementation, faster than AES on platforms without hardware acceleration.

The game server primarily runs on x86_64 Linux servers with AES-NI support. Clients run on a mix of x86_64 (desktop) and ARM (mobile) platforms. Both ciphers are supported by OpenSSL.

## Decision
Use AES-256-GCM as the sole AEAD cipher for packet encryption.

## Consequences

### Positive
- **Hardware acceleration**: AES-NI on x86_64 provides ~5-10x throughput improvement over software-only AES, making AES-256-GCM faster than ChaCha20-Poly1305 on the primary target platform (x86 servers).
- **Simpler code**: Supporting one cipher reduces code complexity, test surface, and potential for cipher-negotiation vulnerabilities.
- **Standard compliance**: AES-256-GCM is a NIST-recommended AEAD cipher with extensive cryptographic review.
- **OpenSSL support**: First-class support in OpenSSL with hardware acceleration automatically used when available.

### Negative
- **ARM performance**: On ARM devices without AES-NI (some mobile clients), AES-256-GCM is slower than ChaCha20-Poly1305. This affects mobile clients but is not the primary target.
- **No cipher agility**: If a vulnerability were discovered in AES-256-GCM (or GCM specifically), there is no fallback cipher. However, the `IPacketEncryptor` interface allows adding one if needed.

### Neutral
- Both ciphers have 256-bit keys and 128-bit tags, so security strength is equivalent.
- GCM's nonce misuse resistance is poor (nonce reuse is catastrophic), but MTP enforces strict nonce counter management to prevent this.

## Alternatives Considered

### ChaCha20-Poly1305
Rejected as the primary cipher because the server platform (x86_64 with AES-NI) benefits more from hardware-accelerated AES. It could be added as a fallback for ARM clients, but the additional complexity is not justified given the mobile client performance is acceptable with AES-256-GCM.

### Both (cipher negotiation)
Considered but rejected. Cipher negotiation adds complexity (negotiation protocol, dual code paths, test matrix explosion) for marginal benefit. The `IPacketEncryptor` abstract interface was designed to support adding ChaCha20-Poly1305 later if ARM performance becomes critical.

### AES-128-GCM
Rejected because 128-bit keys provide less security margin, and AES-256-GCM with AES-NI has negligible performance overhead compared to AES-128-GCM.
