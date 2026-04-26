# ADR 001: Why MTP Custom Transport

## Status
Accepted

## Context
The game engine needs a network transport that can handle real-time game traffic with low latency. The requirements are:

- **Low latency**: Game state updates must arrive quickly; head-of-line blocking from ordered delivery of unrelated packets is unacceptable.
- **Selective reliability**: Some packets (e.g., player position) are latency-sensitive and can be lost, while others (e.g., inventory changes) must arrive reliably and in order.
- **Packet splitting**: Large game data (e.g., map blocks) may exceed MTU and need fragmentation/reassembly.
- **Flow control**: The sender must adapt to the receiver's capacity to avoid overwhelming the network.

TCP provides reliable ordered delivery but suffers from head-of-line blocking — a single lost packet stalls all subsequent packets until retransmission, which is fatal for real-time game traffic. QUIC (RFC 9000) solves this with stream multiplexing, but it did not exist when MTP was originally designed (the protocol predates QUIC by many years). Additionally, QUIC's complexity and TLS integration would require significant rework of the existing authentication (SRP) and encryption (AES-256-GCM with custom key derivation) layers.

## Decision
Use a custom UDP-based transport protocol (MTP — Minetest Transport Protocol) with:

- **Selective reliability**: Channels with per-channel reliable ordering. Unreliable packets skip retransmission.
- **Packet splitting**: Built-in fragmentation for packets exceeding MTU.
- **Flow control**: Timeout-based retransmission with exponential backoff.
- **Control packets**: ACK, NACK, SET_PEER_ID, PING for connection management.

## Consequences

### Positive
- Full control over congestion control and reliability semantics.
- No head-of-line blocking across channels — each channel's reliable queue is independent.
- Tight integration with the encryption layer (0x80 flag detection, per-peer encryption state).
- Minimal overhead — only the features the game needs, no unused QUIC/TLS machinery.

### Negative
- No standard tooling for debugging (Wireshark dissectors, etc.) — must build custom tooling.
- MTP must be maintained independently — bug fixes and security patches are not inherited from a standard library.
- No built-in multipath or connection migration (features QUIC provides).

### Neutral
- The protocol is simple enough that maintenance burden is low, but it would need significant work to match QUIC's feature set if requirements evolve.

## Alternatives Considered

### TCP
Rejected due to head-of-line blocking. A lost packet stalls all data on the connection, making real-time game updates unacceptably delayed. TCP also lacks per-packet reliability control — everything is reliable or nothing is.

### QUIC
Rejected primarily because it did not exist when MTP was designed. Even today, integrating QUIC would require replacing the SRP authentication and custom key derivation with TLS 1.3, which is a significant architectural change with unclear benefit given the existing encryption layer works correctly.

### Enet
Considered as an existing reliable-UDP library. Rejected because it lacks the channel-based reliability model MTP provides, and would still require custom integration with the encryption layer. Building MTP in-house gave full control over the reliability model.
