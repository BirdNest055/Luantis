# ADR 006: Why Deferred Activation

## Status
Accepted

## Context
In v9.11, the client activated encryption immediately after completing SRP authentication and deriving the C2S/S2C keys. This caused a race condition:

1. Client completes SRP → derives keys → activates encryption immediately.
2. Client's next outgoing packet is encrypted (0x80 flag set).
3. Server has not yet completed its key derivation for this client (e.g., it is still processing other clients or the SRP response is in a different thread).
4. Server receives an encrypted packet but cannot decrypt it because it hasn't derived the keys yet.
5. Alternatively: Server sends a plaintext packet to the client, but the client (now in encryption-active mode) rejects it as a security violation because `encryption_active` was true but the packet had no 0x80 flag.

The core issue is that the client and server do not activate encryption atomically. There is always a window where one side has activated and the other hasn't.

## Decision
The client defers encryption activation until it receives the first encrypted packet (or ECDH public key) from the server. Specifically:

1. Client completes SRP → derives C2S/S2C keys → stores them but does NOT activate encryption.
2. Client continues sending plaintext packets (no 0x80 flag).
3. Server completes SRP → derives keys → activates encryption → sends encrypted packets (0x80 flag set).
4. Client receives first encrypted packet from server → NOW activates encryption.
5. Client's subsequent outgoing packets are encrypted.

This ensures the client never rejects a plaintext packet from the server, because the client remains in plaintext mode until it has proof that the server is ready for encrypted traffic.

## Consequences

### Positive
- **Eliminates the race condition entirely**: The client cannot reject plaintext packets from the server because it stays in plaintext mode until the server signals readiness (via the 0x80 flag on its first encrypted packet).
- **No grace periods needed**: Combined with ADR 003 (0x80 flag as sole encryption determinant), there is no need for fragile grace period logic.
- **Server-driven activation**: The server, as the authority, controls when encryption begins. The client follows the server's lead, which is the natural relationship in a client-server protocol.
- **Works with ECDH**: The server's ECDH public key message is also encrypted (0x80 flag set), so receiving it triggers activation. This means forward secrecy is established before the client activates encryption.

### Negative
- **Client stays in plaintext longer**: The client sends a few more packets in plaintext than strictly necessary. These packets are post-SRP-authentication packets that could theoretically be encrypted. However, they are already authenticated by SRP, so spoofing is not possible — only observation (which SRP does not prevent anyway; encryption is needed for confidentiality).
- **Server must send first encrypted packet**: If the server never sends an encrypted packet, the client stays in plaintext indefinitely. In practice, the server always sends an encrypted packet immediately after SRP completion, so this is not a concern.

### Neutral
- The deferral is asymmetric — only the client defers. The server activates encryption immediately after SRP completion. This asymmetry is intentional and reflects the client-server relationship.

## Alternatives Considered

### Immediate activation (v9.11 approach)
Rejected because it caused the race condition described above. The client would reject plaintext packets from the server during the activation transition window.

### Negotiation-based activation (explicit handshake)
Considered. The server would send an "EncryptionActive" message before sending encrypted packets. Rejected because it adds an extra round-trip and a new packet type, when the 0x80 flag already serves as an implicit activation signal. The first packet with the 0x80 flag IS the activation message.

### Simultaneous activation (countdown timer)
Considered. Both sides would activate encryption after a fixed delay. Rejected because it is fragile — network latency variation means the countdown cannot be synchronized, and it would reintroduce the race condition for fast or slow connections.
