# Module: Transport Layer (src/network/mtp/)

## Purpose
Implements the Minetest Transport Protocol (MTP) — a custom UDP-based reliable delivery protocol with flow control, packet splitting, and timeout management.

## Key Classes
- `Connection` — Main connection manager (serves, connects, sends, receives)
- `ConnectionSendThread` — Background thread for sending packets
- `ConnectionReceiveThread` — Background thread for receiving packets
- `UDPPeer` — UDP peer with channels and encryption state
- `Channel` — Per-channel sequence numbers, reliable buffer, stats
- `ReliablePacketBuffer` — Ordered buffer for reliable packet delivery
- `IncomingSplitBuffer` — Reassembly buffer for split packets
- `BufferedPacket` — Packet wrapper with metadata (timestamp, peer, etc.)

## Packet Types
- CONTROL (0): ACK, NACK, SET_PEER_ID, PING
- ORIGINAL (1): Unreliable, unsplit packet
- SPLIT (2): Fragment of a split packet
- RELIABLE (3): Reliable wrapper (contains one of the above)

## Thread Architecture
- Send thread: dequeues commands → processes reliable/unreliable → encrypts → sends via socket
- Receive thread: receives from socket → checks 0x80 flag → decrypts → processes packet type → dispatches

## Dependencies
- Depends on: encryption/ (for IPacketEncryptor), socket/ (for INetworkSocket)
- Depended on by: Client, Server (via IConnection interface)
