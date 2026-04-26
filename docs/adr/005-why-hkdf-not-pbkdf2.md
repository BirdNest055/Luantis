# ADR 005: Why HKDF Not PBKDF2

## Status
Accepted

## Context
The encryption layer needs a key derivation function (KDF) to derive C2S/S2C encryption keys and nonce bases from the SRP session key. Two standard KDFs are available:

1. **HKDF (HMAC-based Extract-and-Expand Key Derivation Function, RFC 5869)**: Designed for deriving keys from already-uniform key material. Two-phase construction: Extract (optional, concentrates entropy) and Expand (generates output keying material). Fast — a few HMAC operations.

2. **PBKDF2 (Password-Based Key Derivation Function 2, RFC 8018)**: Designed for deriving keys from low-entropy passwords. Uses many iterations of a pseudorandom function (typically HMAC-SHA256) to make brute-force attacks expensive. Slow by design.

The input to the KDF is the SRP session key, which is a 256-bit cryptographically random value. This is already high-entropy key material — it does not need the brute-force resistance that PBKDF2 provides.

## Decision
Use HKDF-SHA256 for key derivation (implemented as `HKDFKeyDeriver`).

The SRP session key is used as input keying material (IKM) for HKDF-Expand, with direction-specific info strings ("C2S" and "S2C") to derive separate C2S and S2C keys. This approach was unified from three previously duplicated key derivation functions in the codebase.

## Consequences

### Positive
- **Correct for the use case**: HKDF is designed for deriving keys from already-uniform key material. PBKDF2 is designed for passwords. Using the right tool for the job avoids both security pitfalls (underived keys) and performance pitfalls (unnecessary iterations).
- **Fast**: HKDF-Expand requires only a few HMAC-SHA256 operations per key derivation. PBKDF2 with a reasonable iteration count (e.g., 100,000) would add significant latency to connection setup.
- **Direction separation**: HKDF's info parameter cleanly separates C2S and S2C keys using the same IKM. PBKDF2 would require a different salt or iteration count per direction, which is less elegant.
- **Standardized**: RFC 5869 with extensive cryptographic review.
- **Code unification**: The previous codebase had three separate key derivation functions that were essentially doing the same thing differently. HKDFKeyDeriver unifies them into one correct implementation.

### Negative
- **No brute-force resistance**: If the SRP session key were weak (which it isn't — SRP produces a 256-bit key), HKDF would not slow down a brute-force attack. PBKDF2 would. But since the SRP session key is high-entropy, this is not a concern.
- **Extract phase not used**: The current implementation uses only HKDF-Expand (skipping the Extract phase) because the SRP session key is already uniformly random. This is correct per RFC 5869 but could confuse readers who expect both phases.

### Neutral
- Both HKDF and PBKDF2 use HMAC-SHA256 internally, so the cryptographic primitive is the same. The difference is in the construction and number of iterations.

## Alternatives Considered

### PBKDF2
Rejected because it is designed for low-entropy password inputs and is intentionally slow. Using it with a high-entropy SRP session key wastes CPU time during connection setup for no security benefit. A typical PBKDF2 iteration count of 100,000 would add ~100ms to connection setup, which is unacceptable for a real-time game.

### HKDF with Extract phase
Considered but rejected. The Extract phase is used when the input keying material is not uniformly random (e.g., a Diffie-Hellman shared secret in a non-prime-order group). The SRP session key is already a uniformly random 256-bit value, so the Extract phase is unnecessary per RFC 5869 Section 3.1.

### Custom KDF (pre-unification approach)
Rejected. The previous codebase had three separate key derivation implementations (one for SRP session key, one for ECDH mixing, one for key rotation) that were effectively reimplementing HKDF-Expand differently. Unifying on HKDF-SHA256 reduces code duplication and ensures consistent, reviewed cryptography.
