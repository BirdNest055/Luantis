# ADR 004: Why Replay Bitmap Not Timestamp

## Status
Accepted

## Context
Replay protection prevents an attacker from capturing and retransmitting a valid encrypted packet. Two common approaches are:

1. **Timestamp-based**: Reject packets with timestamps outside an acceptable window (e.g., ±Δ seconds from current time). Requires synchronized clocks.
2. **Bitmap sliding window**: Track received sequence numbers in a bitmap. A 64-bit bitmap represents a window of 64 consecutive sequence numbers. Packets with sequence numbers within the window that have already been seen are rejected as replays.

Both approaches prevent replay attacks, but they differ in precision, complexity, and resource usage.

## Decision
Use a bitmap sliding window for replay protection (implemented as `BitmapReplayProtector`).

The bitmap tracks 64 consecutive sequence numbers per direction per peer. When a packet arrives:
- If its sequence number is below the window, it is rejected as too old.
- If its sequence number is within the window and the corresponding bit is already set, it is rejected as a replay.
- If its sequence number is within the window and the bit is not set, it is accepted and the bit is set.
- If its sequence number is above the window, the window slides forward and the packet is accepted.

## Consequences

### Positive
- **Exact replay detection**: The bitmap provides exact within-window detection — every duplicate sequence number is caught. Timestamp-based approaches only provide approximate window bounds.
- **O(1) lookup**: Checking and setting a bit in a 64-bit integer is a single bitwise operation. No comparisons, no sorting, no hash lookups.
- **Compact state**: 8 bytes per direction per peer (one uint64). A timestamp-based approach requires storing the timestamp of each received packet or maintaining a more complex data structure.
- **No clock synchronization**: The bitmap uses sequence numbers, which are monotonically increasing integers managed by the protocol. Timestamps require synchronized clocks, which adds complexity and failure modes (clock skew, NTP issues).

### Negative
- **Fixed window size**: The 64-bit bitmap provides a window of 64 packets. Packets older than 64 sequence numbers are rejected as "too old" regardless of how recently they were sent. This means highly reordered packet streams (more than 64 packets out of order) will have false rejections. In practice, network reordering rarely exceeds a few packets.
- **Per-direction state**: Both C2S and S2C directions need separate bitmaps, totaling 16 bytes per peer. This is negligible.

### Neutral
- The bitmap approach is independent of time — it does not require RTT estimation or clock synchronization. This makes it simpler but also means the "window" is measured in packets, not time.

## Alternatives Considered

### Timestamp-based replay protection
Rejected because it requires clock synchronization between peers, provides only approximate replay detection (within the time window), and has edge cases around clock skew. It also requires storing more state per packet (timestamps) or a more complex data structure for tracking seen timestamps.

### Hybrid (bitmap + timestamp)
Considered but rejected. Adding timestamps to the bitmap would allow the window to expire based on time as well as sequence number, but the added complexity is not justified. The 64-packet window is sufficient for normal network conditions.

### Larger bitmap (128 or 256 bits)
Considered but rejected. 64 bits covers a window large enough for any reasonable packet reordering scenario. Doubling or quadrupling the window size would double or quadruple memory usage for negligible practical benefit.
