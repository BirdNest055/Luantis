# ADR 003: Why 0x80 Flag for Encryption Detection

## Status
Accepted

## Context
In previous versions (v9.13/v9.14), the code checked `encryption_active` (a per-peer boolean) to determine whether a received packet should be encrypted. This caused problems during the encryption activation transition:

1. **Race condition**: The client would set `encryption_active = true` before the server had activated its encryption. The server then sent plaintext packets that the client rejected as security violations.
2. **Grace periods**: To work around the race condition, grace periods were added — a window during which plaintext packets were tolerated even though encryption was "active." Grace periods are fragile and error-prone.
3. **Transition error spam**: During the transition from plaintext to encrypted, legitimate plaintext packets would log security warnings, obscuring real issues.

The fundamental problem was that `encryption_active` is a *local state variable* that does not reflect the *remote peer's* encryption state. The wire format already had an 0x80 flag byte on encrypted packets, but it was being ignored in favor of the local boolean.

## Decision
The 0x80 flag byte is the SOLE determinant of whether a received packet is encrypted. If the flag is absent, the packet is always processed as plaintext regardless of `encryption_active`. If the flag is present, the packet is always decrypted.

This means:
- No grace periods are needed.
- No transition error spam.
- The wire format is self-describing — you can tell from the packet itself whether it's encrypted.

## Consequences

### Positive
- **Eliminates race conditions**: The receiver's local encryption state no longer affects packet processing. A plaintext packet is always plaintext, an encrypted packet is always encrypted.
- **No grace periods**: Removes the fragile grace period logic and its associated bugs.
- **Cleaner logging**: No spurious security warnings during the plaintext-to-encrypted transition.
- **Self-describing wire format**: Any packet sniffer or debugging tool can determine encryption status from the packet alone.

### Negative
- **Encrypted flag must be stripped before MTP parsing**: The 0x80 flag is in the same byte position as MTP packet type bits. The receive thread must strip the flag before passing the packet to MTP processing. This is a minor but important implementation detail.
- **Unencrypted packets during active encryption are silently accepted**: If an attacker strips the 0x80 flag from an encrypted packet, it will be processed as plaintext (which will likely fail parsing). However, this is a downgrade attack that is better addressed by the deferred activation mechanism (ADR 006).

### Neutral
- The 0x80 flag is in the first byte of the packet, which is also where MTP packet type information lives. The flag uses the high bit, which was previously unused by MTP packet types (0-3 only use the low 2 bits).

## Alternatives Considered

### encryption_active boolean (v9.13/v9.14 approach)
Rejected because it caused race conditions, required grace periods, and produced transition error spam. The fundamental problem is that local state cannot accurately reflect remote state.

### Dual check (0x80 flag AND encryption_active)
Considered but rejected. Checking both conditions reintroduces the race condition — if `encryption_active` is false but the packet has the 0x80 flag, what should happen? Rejecting it would prevent the client from processing the server's first encrypted packet. Accepting it makes `encryption_active` redundant.

### Negotiation-based activation
Considered but rejected. Adding an explicit "encryption active" handshake message would add latency and complexity. The 0x80 flag is implicit — the first packet with the flag set IS the activation signal.
