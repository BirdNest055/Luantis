# Module: Network Communications (src/network/)

## Purpose
The communications subsystem implements a custom UDP-based reliable transport protocol (MTP) with AES-256-GCM encryption, X25519 ECDH forward secrecy, and SRP authentication. It handles all network I/O for the game engine.

## Architecture Overview
```
┌──────────────────────────────────────────────┐
│  Client/Server Packet Handlers               │
│  (client/, server/ — domain-split handlers)  │
├──────────────────────────────────────────────┤
│  Opcode Dispatch                             │
│  (clientopcodes, serveropcodes)              │
├──────────────────────────────────────────────┤
│  Transport Layer (mtp/)                      │
│  Connection, Peer, Channel, Threads          │
│  Reliable delivery, flow control, splitting  │
├──────────────────────────────────────────────┤
│  Encryption Layer (encryption/)              │
│  IPacketEncryptor, IReplayProtector,         │
│  IKeyDeriver, PeerEncryptionState            │
├──────────────────────────────────────────────┤
│  Socket Layer (socket/)                      │
│  INetworkSocket, UDPSocket, Address          │
└──────────────────────────────────────────────┘
```

## Key Classes
- `Connection` — Main MTP connection (send/receive/peer management)
- `PeerEncryptionState` — Per-peer encryption keys, nonces, ECDH state
- `IPacketEncryptor` — Abstract interface for encrypt/decrypt
- `IReplayProtector` — Abstract interface for replay defense
- `IKeyDeriver` — Abstract interface for key derivation
- `INetworkSocket` — Abstract interface for socket I/O
- `NetworkPacket` — Binary serialization/deserialization

## Encryption Flow
1. SRP authentication → session key
2. Session key → HKDF → C2S/S2C keys + nonce bases
3. ECDH X25519 exchange → shared secret mixed into keys (forward secrecy)
4. Packets: check 0x80 flag → encrypt/decrypt or pass through as plaintext
5. Replay protection via bitmap sliding window

## Invariants
- 0x80 flag is the SOLE determinant of whether a packet is encrypted
- Nonce counters are NEVER reused with the same key
- Single-player mode NEVER activates encryption
- C2S key on server = S2C key on client (and vice versa)

## Common Modifications
- **Add a new packet type**: Add to `networkprotocol.h` enum, add handler in `client/` or `server/`, add to opcode table
- **Change encryption algorithm**: Implement new `IPacketEncryptor` subclass
- **Add a new test**: Use `TestBase` pattern, add to `src/unittest/`
- **Fix encryption bugs**: Look in `mtp/threads.cpp` → `rawSend()`/`receive()` and `crypto.cpp`

## File Map
- `client/` — Client packet handler files (auth, encryption, media, game, HUD, misc)
- `server/` — Server packet handler files (auth, encryption, game, misc)
- `encryption/` — Crypto interfaces, key derivation, replay protection, logging, tracing
- `mtp/` — Transport implementation (connection, peer, channel, threads, buffers)
- `socket/` — Socket interface and UDP implementation
- `protocol/` — Wire format definitions
