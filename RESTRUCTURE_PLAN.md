# Clawtest Comms/Network Restructuring Plan

**Branch**: `clawtest-v9.16-restructure` (derived from `clawtest-v9.15`)
**Date**: 2026-04-26
**Scope**: `src/network/` — the complete communications and networking subsystem
**Goal**: Defragment, modularize, test, and make the comms layer AI-agent friendly while preserving all existing behavior

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current State Assessment](#2-current-state-assessment)
3. [Scoring Dashboard (10-Point System)](#3-scoring-dashboard-10-point-system)
4. [Problem Catalog](#4-problem-catalog)
5. [Restructuring Principles](#5-restructuring-principles)
6. [Phase 1 — Defragmentation (No Behavioral Change)](#6-phase-1--defragmentation-no-behavioral-change)
7. [Phase 2 — Decoupling & Interface Extraction](#7-phase-2--decoupling--interface-extraction)
8. [Phase 3 — Test Infrastructure & Coverage](#8-phase-3--test-infrastructure--coverage)
9. [Phase 4 — Structured Logging & Observability](#9-phase-4--structured-logging--observability)
10. [Phase 5 — LLM-Agent Friendliness](#10-phase-5--llm-agent-friendliness)
11. [Migration Safety: How We Avoid Breaking Things](#11-migration-safety-how-we-avoid-breaking-things)
12. [File Map: Before → After](#12-file-map-before--after)
13. [Risk Register](#13-risk-register)
14. [Acceptance Criteria](#14-acceptance-criteria)
15. [Appendix A — Full File Inventory](#appendix-a--full-file-inventory)
16. [Appendix B — God Object Decomposition Details](#appendix-b--god-object-decomposition-details)
17. [Appendix C — Test Gap Matrix](#appendix-c--test-gap-matrix)

---

## 1. Executive Summary

The Clawtest comms/network subsystem spans **30 files**, **~15,000 lines of production code**, and **~9,200 lines of test code**. While the recently added crypto layer (v9.11–v9.15) is well-tested at the unit level, the underlying transport, packet processing, and handler layers suffer from severe structural problems:

- **7 god objects/files** over 1,000 lines each (one at 2,296 lines)
- **~300 lines of copy-pasted key derivation code** duplicated 3 times
- **55% of public network methods have zero test coverage**
- **Zero integration tests** for encrypted connections through the real transport
- **14+ `dynamic_cast` downcasts** indicating broken abstractions
- **`friend` class declarations** that eliminate encapsulation entirely
- **No mock infrastructure** for the network layer

This plan describes a **5-phase restructuring** that fixes these problems without changing any external behavior. Each phase is independently committable, independently testable, and can be rolled back safely.

**Estimated effort**: 4–6 weeks for a single developer, or 2–3 weeks with AI-agent assistance.

---

## 2. Current State Assessment

### 2.1 Codebase Size

| Category | Files | Lines (prod) | Lines (test) |
|---|---|---|---|
| Crypto & Encryption | 6 | 2,838 | 7,734 |
| Protocol Definitions | 2 | 1,091 | 0 |
| Connection & Transport (mtp/) | 6 | 4,580 | 357 |
| Packet Handlers | 4 | 4,848 | 0 |
| Serialization | 2 | 673 | 0 |
| Socket & Address | 4 | 630 | 271 |
| Security Info & Config | 4 | 817 | 814 |
| **Total** | **28** | **~15,477** | **~9,176** |

### 2.2 File Health Overview

| File | Lines | Health | Primary Problem |
|---|---|---|---|
| `clientpackethandler.cpp` | 2,296 | 🔴 Critical | God file: 62 handlers, 6+ domains |
| `serverpackethandler.cpp` | 2,117 | 🔴 Critical | God file: 25 handlers, 4+ domains |
| `mtp/threads.cpp` | 1,794 | 🔴 Critical | Two threads in one file, oversized methods |
| `mtp/impl.cpp` | 1,751 | 🔴 Critical | 7+ class implementations in one file |
| `crypto.h` | 596 | 🟠 Poor | God struct `PeerEncryptionState` (596 lines) |
| `connection_security.h` | 723 | 🟠 Poor | God struct `ConnectionSecurityInfo` (562 lines) |
| `networkprotocol.h` | 1,004 | 🟡 Fair | Navigation difficulty, wire docs mixed with enums |
| `crypto.cpp` | 1,180 | 🟠 Poor | 300 lines of duplicated key derivation |
| `encryption_log.h` | 261 | 🟡 Fair | Macro-based logging, inline banners |
| `mtp/internal.h` | 546 | 🟠 Poor | 7 classes in one header, crypto coupling |
| `mtp/impl.h` | 316 | 🟡 Fair | friend abuse, raw pointers |
| `networkpacket.h/cpp` | 673 | 🟡 Fair | Dual-use offset, legacy code |
| `socket.h/cpp` | 338 | 🟢 OK | IPv4/IPv6 duplication |
| `address.h/cpp` | 292 | 🟢 OK | Platform header leak, global settings |
| `peerhandler.h` | 33 | 🟢 Good | Clean interface |
| `connection.h/cpp` | 104 | 🟢 Good | Clean interface (minor crypto coupling) |

### 2.3 Dependency Graph (Current)

```
clientpackethandler.cpp ──→ client/client.h ──→ [massive include chain]
serverpackethandler.cpp ──→ server.h ──→ [massive include chain]
         ↕
    opcodes.h ──→ client/client.h | server.h  (circular via Client/Server)
         ↕
  connection.h ──→ crypto.h  (interface leaks implementation)
         ↕
  mtp/impl.h ──→ mtp/internal.h ──→ crypto.h  (transport coupled to crypto)
         ↕
  mtp/threads.cpp ──→ crypto.h, encryption_log.h  (inline encryption in thread code)
```

The dependency graph has **3 cycles** and **2 unnecessary cross-layer dependencies** (transport → crypto, interface → implementation).

---

## 3. Scoring Dashboard (10-Point System)

### 3.1 Current Scores

| Dimension | Score | Explanation |
|---|---|---|
| **Modularity** | 2/10 | God objects, mixed responsibilities, no separation of concerns. Crypto, replay protection, and fingerprint persistence are one struct. |
| **Cohesion** | 3/10 | Most files contain multiple unrelated responsibilities. `clientpackethandler.cpp` handles auth, encryption, inventory, HUD, particles, and media. |
| **Coupling** | 2/10 | `friend` classes eliminate encapsulation. 14+ `dynamic_cast` downcasts. Transport layer directly knows about AES-256-GCM internals. |
| **Test Coverage** | 4/10 | Crypto/state layer is well-tested (380+ tests), but transport and handler layers are at ~0% unit coverage. 55% of public methods untested. |
| **Code Duplication** | 3/10 | ~300 lines of key derivation copied 3x. Test helper functions copy-pasted across 5+ files. IPv4/IPv6 socket code duplicated. |
| **Naming Clarity** | 5/10 | Inconsistent naming: `ServerCommandFactory` is in `clientopcodes.h`. `m_read_offset` used for both reading and writing. Typo `avaialble`. |
| **Documentation** | 4/10 | Wire format docs exist but are embedded in enum headers. No architecture docs. No module-level documentation. Crypto state machine undocumented. |
| **LLM-Agent Friendliness** | 3/10 | God files exceed context windows. No module boundaries. No `AI_CONTEXT.md` files. Mixed concerns force agents to understand everything. |
| **Observability/Logging** | 4/10 | Macro-based encryption logging. No structured logging. No packet-level tracing. Error handling inconsistent (some log+return, some crash). |
| **Build Organization** | 5/10 | CMakeLists.txt separates client/server/common. But `mtp/` subdirectory is only partial separation. No per-module CMake. |
| **OVERALL** | **3.5/10** | The codebase works but is fragile, hard to test, and hostile to both human and AI-agent maintainers. |

### 3.2 Target Scores (After Restructuring)

| Dimension | Current | Target | Delta |
|---|---|---|---|
| **Modularity** | 2 | 8 | +6 |
| **Cohesion** | 3 | 8 | +5 |
| **Coupling** | 2 | 7 | +5 |
| **Test Coverage** | 4 | 8 | +4 |
| **Code Duplication** | 3 | 8 | +5 |
| **Naming Clarity** | 5 | 8 | +3 |
| **Documentation** | 4 | 7 | +3 |
| **LLM-Agent Friendliness** | 3 | 9 | +6 |
| **Observability/Logging** | 4 | 7 | +3 |
| **Build Organization** | 5 | 8 | +3 |
| **OVERALL** | **3.5** | **7.8** | **+4.3** |

---

## 4. Problem Catalog

### P1: God Files (Severity: CRITICAL)

| ID | File | Lines | Why It's a Problem |
|---|---|---|---|
| P1.1 | `clientpackethandler.cpp` | 2,296 | 62 methods spanning auth, encryption, inventory, HUD, particles, media, sound. No human or AI can reason about this in one pass. |
| P1.2 | `serverpackethandler.cpp` | 2,117 | 25 methods spanning auth, encryption, interaction, inventory, environment. Same problem. |
| P1.3 | `mtp/threads.cpp` | 1,794 | Send thread + receive thread in one file. Five methods over 100 lines. Encryption logic inline. |
| P1.4 | `mtp/impl.cpp` | 1,751 | Implementation of 7+ classes (Connection, Peer, UDPPeer, Channel, ReliablePacketBuffer, IncomingSplitBuffer, BufferedPacket). |

### P2: God Structs (Severity: CRITICAL)

| ID | Struct | Lines | Why It's a Problem |
|---|---|---|---|
| P2.1 | `PeerEncryptionState` (crypto.h) | 596 | Mixes: key storage, nonce management, replay bitmap, ECDH state, fingerprint data, audit logging, session management, key rotation. Should be 5+ classes. |
| P2.2 | `ConnectionSecurityInfo` (connection_security.h) | 562 | Mixes: security flags, scoring algorithms, display formatting, statistics, UI string generation. Should be 3+ classes. |
| P2.3 | `DirectionalEncryptionState` (crypto.h) | 152 | Mixes: key/nonce storage with replay protection logic. Replay protection is a network protocol concern, not a crypto primitive. |

### P3: Code Duplication (Severity: HIGH)

| ID | Location | Duplicated Lines | Pattern |
|---|---|---|---|
| P3.1 | `crypto.cpp`: `initFromSRPSessionKey`, `rotateKeys`, `mixECDHSecretIntoKeys` | ~300 | All 3 functions follow: derive salt → HKDF for C2S key → S2C key → C2S nonce → S2C nonce → session ID. 15 near-identical HKDF call blocks. |
| P3.2 | `socket.cpp`: `Bind()`, `Send()`, `Receive()` | ~60 | IPv4/IPv6 branches with identical structure. |
| P3.3 | Test files: `makeTestSessionKey()`, `makeTestEncState()`, `encryptPacket()`, etc. | ~200 | Helper functions copy-pasted across 5+ test files. |
| P3.4 | `connection_security.h`: `getXxxString()` methods | ~80 | 7 static + 7 instance method pairs that are pure delegation. |

### P4: Coupling Violations (Severity: HIGH)

| ID | From | To | Why It's Bad |
|---|---|---|---|
| P4.1 | `connection.h` (interface) | `crypto.h` (implementation) | Abstract interface depends on concrete `PeerEncryptionState`. |
| P4.2 | `mtp/internal.h` (transport) | `crypto.h` | `UDPPeer` embeds `PeerEncryptionState` directly. |
| P4.3 | `mtp/threads.cpp` (transport) | `crypto.h` | `rawSend()` and `receive()` contain 200+ lines of encryption/decryption logic. |
| P4.4 | `clientopcodes.h` | `client/client.h` | Network dispatch depends on full Client class definition. |
| P4.5 | `serveropcodes.h` | `server.h` | Network dispatch depends on full Server class definition. |
| P4.6 | `Connection` ↔ Thread classes | `friend` declarations | Complete encapsulation breakdown. Thread code accesses Connection internals directly. |

### P5: Missing Abstractions (Severity: HIGH)

| ID | What's Missing | Why We Need It |
|---|---|---|
| P5.1 | `IPacketEncryptor` interface | Cannot test transport without real encryption. Cannot swap cipher suites. |
| P5.2 | `IReplayProtector` interface | Replay protection is baked into crypto state. Cannot test independently. |
| P5.3 | `IKeyDeriver` interface | Key derivation is hardcoded to HKDF-SHA256. Cannot test transport without real key derivation. |
| P5.4 | `IPacketHandler` interface | Opcode dispatch is hardcoded to Client/Server methods. Cannot test dispatch without full Client/Server. |
| P5.5 | `IFingerprintStore` interface | File I/O mixed into crypto. Cannot test crypto without filesystem. |
| P5.6 | `INetworkSocket` interface | Socket code is concrete. Cannot test connection logic without real UDP. |

### P6: Test Gaps (Severity: HIGH)

| ID | Gap | Risk |
|---|---|---|
| P6.1 | Zero unit tests for `ConnectionSendThread` | Cannot verify reliable delivery, resends, flow control. |
| P6.2 | Zero unit tests for `ConnectionReceiveThread` | Cannot verify packet dispatch, split reassembly, ACK handling. |
| P6.3 | Zero unit tests for packet handlers (4,413 lines) | Cannot verify 87 handler methods. |
| P6.4 | Zero integration tests for encrypted connections | Crypto tested in isolation, never through real transport. |
| P6.5 | No mock infrastructure for network layer | All network tests require real UDP sockets. |
| P6.6 | `rotateKeys()` cross-side consistency untested | Local-only rotation is dead code; no test proves it would work across network. |

### P7: Logging & Observability (Severity: MEDIUM)

| ID | Problem | Impact |
|---|---|---|
| P7.1 | Macro-based logging (`enclog_*`) | No type safety, no runtime disable, no redirect for tests. |
| P7.2 | No structured packet tracing | Cannot trace packet lifecycle through send → encrypt → socket → receive → decrypt → dispatch. |
| P7.3 | Inconsistent error handling | Some errors log+return, some `FATAL_ERROR_IF` crash, some silently set nullptr. |
| P7.4 | Hard-coded security score in thread code | `rawSend()` logs `70, "Fair"` regardless of actual score. |
| P7.5 | No correlation IDs | Cannot correlate send-side events with receive-side events for the same packet. |

### P8: Naming & Code Quality (Severity: MEDIUM)

| ID | Problem | Location |
|---|---|---|
| P8.1 | `m_iteration_packets_avaialble` (typo) | `threads.h`, `threads.cpp` (5 occurrences) |
| P8.2 | `ServerCommandFactory` in `clientopcodes.h` | Confusing cross-naming |
| P8.3 | `m_read_offset` used for reading AND writing | `networkpacket.h` |
| P8.4 | `PlayerListModifer` (typo: Modifier) | `networkprotocol.h` |
| P8.5 | `oldForgePacket()` legacy code (7+ years) | `networkpacket.cpp` |
| P8.6 | `INTERNET_SIMULATOR` debug code in production | `socket.cpp` |

---

## 5. Restructuring Principles

1. **No Behavioral Change**: Every phase preserves the exact same external behavior. If the game worked before, it works after.
2. **Incremental & Committable**: Each phase is broken into steps that can be committed individually. No "big bang" rewrites.
3. **Test-First**: Tests are added BEFORE restructuring. The test suite is the safety net that proves no behavior changed.
4. **Compilation Gate**: Every intermediate step must compile and pass all existing tests.
5. **AI-Agent Accessibility**: Every module gets a `AI_CONTEXT.md` that an LLM agent can read to understand the module without reading all source files.
6. **Interface Segregation**: Replace `friend` access with explicit interfaces. Replace `dynamic_cast` with virtual dispatch.
7. **Single Responsibility**: Each file has one reason to change. Each class has one job.

---

## 6. Phase 1 — Defragmentation (No Behavioral Change)

**Goal**: Split god files into focused modules. No new interfaces, no new abstractions — just reorganization.
**Duration**: 1–2 weeks
**Risk**: LOW (pure file splitting, no logic changes)

### Step 1.1: Split `clientpackethandler.cpp` (2,296 lines → 6 files)

**Target structure**:
```
src/network/client/
  client_auth_handler.cpp       ~350 lines  (Hello, Init, SRP, ECDH)
  client_encryption_handler.cpp ~200 lines  (ECDH pubkey exchange)
  client_media_handler.cpp      ~400 lines  (Media, textures, itemdefs)
  client_game_handler.cpp       ~600 lines  (Inventory, interact, HP, death)
  client_hud_handler.cpp        ~400 lines  (HUD, minimap, sky, particles)
  client_misc_handler.cpp       ~350 lines  (Chat, time, modchannel, etc.)
```

**Method**: Each `Client::handleCommand_Xxx()` method moves to exactly one file. The files are `#include`d back into a single `clientpackethandler.cpp` at first (include-split pattern), then formally registered in CMakeLists.txt.

**Verification**: Compile + run all existing tests.

### Step 1.2: Split `serverpackethandler.cpp` (2,117 lines → 4 files)

**Target structure**:
```
src/network/server/
  server_auth_handler.cpp       ~500 lines  (Init, FirstSrp, SRP exchange)
  server_encryption_handler.cpp ~200 lines  (ECDH pubkey exchange)
  server_game_handler.cpp       ~800 lines  (Interact, inventory, env, chat)
  server_misc_handler.cpp       ~600 lines  (Playerpos, GOTBLOCKS, modchannel)
```

### Step 1.3: Split `mtp/threads.cpp` (1,794 lines → 2 files)

**Target structure**:
```
src/network/mtp/
  send_thread.cpp    ~800 lines  (ConnectionSendThread implementation)
  receive_thread.cpp  ~994 lines  (ConnectionReceiveThread implementation)
```

### Step 1.4: Split `mtp/impl.cpp` (1,751 lines → 5 files)

**Target structure**:
```
src/network/mtp/
  buffered_packet.cpp  ~200 lines  (BufferedPacket, makePacket helpers)
  reliable_buffer.cpp  ~250 lines  (ReliablePacketBuffer)
  split_buffer.cpp     ~150 lines  (IncomingSplitBuffer)
  channel.cpp          ~350 lines  (Channel implementation)
  peer.cpp             ~450 lines  (Peer, UDPPeer implementation)
  connection_core.cpp   ~350 lines  (Connection implementation)
```

### Step 1.5: Extract `FingerprintStore` from `crypto.h/cpp`

**Target structure**:
```
src/network/
  fingerprint_store.h    ~60 lines  (FingerprintStore class declaration)
  fingerprint_store.cpp  ~120 lines  (FingerprintStore implementation)
```

**Verification**: Compile + run all existing tests. FingerprintStore tests already exist and should pass unchanged.

### Step 1.6: Extract `ConnectionSecurityInfo` into `.cpp`

Move all inline methods from `connection_security.h` into a new `connection_security.cpp`. This is the highest-ROI single change — it removes 400+ lines of business logic from a header without changing any behavior.

**Target structure**:
```
src/network/
  connection_security.h    ~200 lines  (struct declaration only)
  connection_security.cpp  ~530 lines  (all method implementations)
```

### Step 1.7: Extract inline methods from `DirectionalEncryptionState`

Move `shiftBitmap()`, `isAlreadySeen()`, `markReceived()`, `isNotReplay()`, `updateCounter()`, `nextNonce()` from `crypto.h` to `crypto.cpp`. Currently ~100 lines of inline logic in the header.

### Phase 1 Deliverables Checklist

- [ ] `clientpackethandler.cpp` split into 6 files
- [ ] `serverpackethandler.cpp` split into 4 files
- [ ] `mtp/threads.cpp` split into `send_thread.cpp` + `receive_thread.cpp`
- [ ] `mtp/impl.cpp` split into 6 files
- [ ] `fingerprint_store.h/cpp` extracted from crypto
- [ ] `connection_security.cpp` extracted from header
- [ ] `DirectionalEncryptionState` methods moved to .cpp
- [ ] All files compile
- [ ] All 380+ existing tests pass
- [ ] CMakeLists.txt updated

---

## 7. Phase 2 — Decoupling & Interface Extraction

**Goal**: Break coupling cycles, introduce interfaces, eliminate `friend` and `dynamic_cast`.
**Duration**: 2–3 weeks
**Risk**: MEDIUM (interface changes may affect call sites)

### Step 2.1: Extract `IPacketEncryptor` Interface

**Current**: `rawSend()` and `receive()` contain 200+ lines of AES-256-GCM encryption/decryption logic inline.

**Target**:
```cpp
// src/network/packet_encryptor.h
class IPacketEncryptor {
public:
    virtual ~IPacketEncryptor() = default;

    // Returns encrypted packet with 0x80 flag prepended, or nullopt on failure
    virtual std::optional<SharedBuffer<u8>> encrypt(
        session_t peer_id, const SharedBuffer<u8>& plaintext) = 0;

    // Returns decrypted plaintext, or nullopt on auth failure
    virtual std::optional<SharedBuffer<u8>> decrypt(
        session_t peer_id, const SharedBuffer<u8>& ciphertext) = 0;

    // Check if a peer has active encryption
    virtual bool isEncrypted(session_t peer_id) const = 0;
};
```

**Concrete implementation**:
```cpp
// src/network/aes256gcm_encryptor.h
class AES256GCMEncryptor : public IPacketEncryptor {
    // Uses PeerEncryptionState internally
    // Contains all the logic currently in rawSend()/receive()
};
```

**Impact**: `rawSend()` shrinks from 124 lines to ~20 lines. `receive()` shrinks by ~100 lines. Encryption becomes independently testable.

### Step 2.2: Extract `IReplayProtector` Interface

**Current**: Replay protection is embedded in `DirectionalEncryptionState`.

**Target**:
```cpp
// src/network/replay_protection.h
class IReplayProtector {
public:
    virtual ~IReplayProtector() = default;
    virtual bool isNotReplay(u64 counter) = 0;
    virtual void markReceived(u64 counter) = 0;
};

// src/network/bitmap_replay_protector.h
class BitmapReplayProtector : public IReplayProtector {
    // Exact same logic as DirectionalEncryptionState::shiftBitmap etc.
    // Moved out of the crypto struct
};
```

**Impact**: `DirectionalEncryptionState` becomes a pure data struct (keys + nonces). Replay protection is independently testable and swappable.

### Step 2.3: Extract `IKeyDeriver` Interface

**Current**: Key derivation is 3 near-identical functions totaling ~300 lines of duplication.

**Target**:
```cpp
// src/network/key_deriver.h
class IKeyDeriver {
public:
    virtual ~IKeyDeriver() = default;
    virtual DirectionalKeys deriveKeys(
        const u8* ikm, size_t ikm_len,
        const std::string& version_info) = 0;
};

// src/network/hkdf_key_deriver.h
class HKDFKeyDeriver : public IKeyDeriver {
    // Single implementation of the deriveKeys pattern
    // Used by initFromSRPSessionKey, rotateKeys, mixECDHSecretIntoKeys
};
```

**Impact**: Eliminates ~200 lines of duplication. The 3 key derivation functions become ~10 lines each (call `deriveKeys` with different info strings).

### Step 2.4: Extract `IPacketHandler` Interface

**Current**: Opcode dispatch tables directly reference `Client::handleCommand_Xxx` and `Server::handleCommand_Xxx`.

**Target**:
```cpp
// src/network/packet_handler.h
class IPacketHandler {
public:
    virtual ~IPacketHandler() = default;
    virtual void handle(NetworkPacket& pkt) = 0;
};
```

This allows each handler method to be a standalone class rather than a method on the massive Client/Server class. It also allows testing handlers without the full Client/Server.

**Impact**: Breaks the `clientopcodes.h → client/client.h` dependency cycle.

### Step 2.5: Extract `INetworkSocket` Interface

**Current**: `UDPSocket` is a concrete class. Network tests require real UDP.

**Target**:
```cpp
// src/network/network_socket.h
class INetworkSocket {
public:
    virtual ~INetworkSocket() = default;
    virtual void Bind(Address addr) = 0;
    virtual void Send(const Address& addr, const void* data, size_t len) = 0;
    virtual int Receive(Address& sender, void* data, size_t len) = 0;
};

// src/network/udp_socket.h
class UDPSocket : public INetworkSocket { /* existing code */ };

// src/network/mock_socket.h  (TEST ONLY)
class MockSocket : public INetworkSocket {
    // Records sent packets, provides canned responses
};
```

**Impact**: Connection logic becomes testable without real network I/O.

### Step 2.6: Decouple `connection.h` from `crypto.h`

**Current**: `IConnection::SetPeerEncryptionState()` takes `const PeerEncryptionState&`.

**Target**: Replace with `SetPeerEncryption(session_t peer_id, std::shared_ptr<IPacketEncryptor>)`. The interface no longer knows about `PeerEncryptionState`.

### Step 2.7: Eliminate `friend` Declarations

Replace each `friend` access pattern with explicit method calls:

| Current | Replacement |
|---|---|
| `ConnectionSendThread` accesses `m_command_queue` directly | `Connection::dequeueCommand()` method |
| `ConnectionReceiveThread` accesses `m_udpSocket` directly | `Connection::receiveRawPacket()` method |
| `ConnectionSendThread` accesses `m_peers` directly | `Connection::getPeer()` / `Connection::forEachPeer()` methods |
| `ConnectionSendThread` is friend of `UDPPeer` | `UDPPeer::getChannel()` / `UDPPeer::getEncryptionState()` accessors |

### Step 2.8: Eliminate `dynamic_cast<UDPPeer*>`

Replace with virtual methods on `Peer`:
- `Peer::isUDP()` → virtual bool
- `Peer::asUDPPeer()` → virtual `UDPPeer*` (returns nullptr for non-UDP peers)
- Or better: move the called methods up to `Peer` as virtual methods

### Phase 2 Deliverables Checklist

- [ ] `IPacketEncryptor` interface + `AES256GCMEncryptor` implementation
- [ ] `IReplayProtector` interface + `BitmapReplayProtector` implementation
- [ ] `IKeyDeriver` interface + `HKDFKeyDeriver` implementation
- [ ] `IPacketHandler` interface
- [ ] `INetworkSocket` interface + `MockSocket` implementation
- [ ] `connection.h` decoupled from `crypto.h`
- [ ] All `friend` declarations eliminated
- [ ] All `dynamic_cast<UDPPeer*>` eliminated
- [ ] All files compile
- [ ] All existing tests pass

---

## 8. Phase 3 — Test Infrastructure & Coverage

**Goal**: Build test infrastructure, fill coverage gaps, achieve 80%+ method coverage.
**Duration**: 2–3 weeks
**Risk**: LOW (test-only changes)

### Step 3.1: Shared Test Utilities

**Current**: `makeTestSessionKey()`, `makeTestEncState()`, `encryptPacket()`, `decryptPacket()`, `buildEncryptedPacket()`, etc. are copy-pasted across 5+ test files.

**Target**:
```
src/unittest/network_test_helpers.h   — Shared test utilities
src/unittest/network_test_helpers.cpp — Implementations
```

Contents:
- `makeTestSessionKey()` — single definition
- `makeTestEncState()` — single definition
- `encryptPacket()` / `decryptPacket()` — single definition
- `buildEncryptedPacket()` / `buildPlaintextPacket()` — single definition
- `MockSocket` — test-only mock socket
- `MockPeerHandler` — test-only peer handler
- `PacketFactory` — create test packets of any type

### Step 3.2: Mock Infrastructure

Create mocks for all interfaces extracted in Phase 2:

```
src/unittest/mocks/
  mock_packet_encryptor.h     — Records encrypt/decrypt calls
  mock_replay_protector.h     — Configurable replay decisions
  mock_key_deriver.h          — Returns canned keys
  mock_network_socket.h       — Records sent packets, provides canned receives
  mock_peer_handler.h         — Records peer lifecycle events
  mock_fingerprint_store.h    — In-memory fingerprint storage
```

### Step 3.3: Transport Layer Tests

Add unit tests for the currently untested transport layer:

| Test File | What It Tests | Priority |
|---|---|---|
| `test_reliable_buffer.cpp` | `ReliablePacketBuffer::insert()`, out-of-order, duplicate, wraparound | CRITICAL |
| `test_split_buffer.cpp` | `IncomingSplitBuffer::insert()`, reassembly, missing chunks | CRITICAL |
| `test_channel.cpp` | `Channel` sequence numbers, window sizing, bandwidth stats | HIGH |
| `test_send_thread.cpp` | `ConnectionSendThread` reliable/unreiable dispatch, resends, timeouts | HIGH |
| `test_receive_thread.cpp` | `ConnectionReceiveThread` packet type routing, ACK handling | HIGH |
| `test_packet_encryptor.cpp` | `AES256GCMEncryptor` encrypt/decrypt through interface | HIGH |
| `test_replay_protection.cpp` | `BitmapReplayProtector` window, shift, boundary cases | HIGH |
| `test_key_deriver.cpp` | `HKDFKeyDeriver` key derivation consistency | MEDIUM |

### Step 3.4: Integration Tests

| Test File | What It Tests |
|---|---|
| `test_encrypted_connection.cpp` | Full encrypted session: SRP → key derivation → activate → send encrypted → receive decrypted |
| `test_reliable_delivery.cpp` | Packet loss + resend → eventual delivery |
| `test_split_packets.cpp` | Large payload → split → reassemble |
| `test_multi_peer.cpp` | Multiple peers with different encryption states |
| `test_key_rotation_integration.cpp` | Rotate keys while packets in flight → no auth failures |

### Step 3.5: Handler Tests

For each handler domain (auth, game, media, HUD), add at least 3 tests:
1. Valid packet → correct state change
2. Invalid/malformed packet → graceful rejection
3. Edge case (empty payload, max-size payload, wrong state)

### Phase 3 Deliverables Checklist

- [ ] `network_test_helpers.h/cpp` created
- [ ] Mock infrastructure for all 6 interfaces
- [ ] `test_reliable_buffer.cpp` (target: 15+ tests)
- [ ] `test_split_buffer.cpp` (target: 10+ tests)
- [ ] `test_channel.cpp` (target: 10+ tests)
- [ ] `test_send_thread.cpp` (target: 15+ tests)
- [ ] `test_receive_thread.cpp` (target: 15+ tests)
- [ ] `test_packet_encryptor.cpp` (target: 10+ tests)
- [ ] `test_replay_protection.cpp` (target: 15+ tests)
- [ ] `test_key_deriver.cpp` (target: 10+ tests)
- [ ] `test_encrypted_connection.cpp` (integration, target: 5+ tests)
- [ ] Handler tests for auth, game, media, HUD domains
- [ ] Total test count target: 600+ (up from 380+)

---

## 9. Phase 4 — Structured Logging & Observability

**Goal**: Replace macro-based logging with structured, testable, controllable logging.
**Duration**: 1 week
**Risk**: LOW (logging changes don't affect behavior)

### Step 4.1: Replace `enclog_*` Macros with Logger Class

**Current**:
```cpp
#define enclog_init(msg) infostream << ENC_TAG_INIT << msg
```

**Target**:
```cpp
// src/network/encryption_logger.h
class EncryptionLogger {
public:
    enum class Level { Init, Activate, Send, Recv, Security, Error, Disable, Audit };

    void log(Level level, session_t peer_id, std::string_view message);
    void log(Level level, session_t peer_id, std::initializer_list<KVPair> fields);

    // Configuration
    void setLevel(Level min_level);
    void setEnabled(bool enabled);
    void setOutput(std::ostream* stream);

    static EncryptionLogger& instance();  // Or inject via dependency
};
```

**Impact**: Logging becomes testable, controllable, redirectable. Can be disabled in tests. Can be configured at runtime.

### Step 4.2: Add Packet Tracing

```cpp
// src/network/packet_trace.h
class PacketTrace {
public:
    struct Entry {
        u64 timestamp_ms;
        session_t peer_id;
        u8 channel;
        PacketDirection direction;  // Sent / Received
        PacketPhase phase;          // Raw / Encrypted / Decrypted / Dispatched
        std::string summary;
    };

    void record(Entry entry);
    std::vector<Entry> query(session_t peer_id, PacketPhase phase);
    void enable(bool enabled);
};
```

This enables: "Show me all packets for peer 42 that were encrypted but failed decryption" — impossible today.

### Step 4.3: Add Correlation IDs

Each packet gets a unique `trace_id` that follows it through the entire lifecycle. This allows correlating:
- "Packet #4712 was sent at T+1.2s, encrypted with nonce N, received at T+1.3s, decrypted successfully, dispatched as TOCLIENT_INVENTORY"

### Step 4.4: Fix Inconsistent Error Handling

| Current | Fix |
|---|---|
| `FATAL_ERROR_IF(refcount != 0)` | Replace with `errorstream` + graceful cleanup |
| `PeerHelper` silently sets `m_peer = nullptr` | Return error code or throw |
| `ReliablePacketBuffer::insert()` logs error + returns normally | Return `std::optional` or error code |
| Hard-coded `70, "Fair"` in `rawSend()` | Use `ConnectionSecurityInfo::getSecurityScore()` |

### Phase 4 Deliverables Checklist

- [ ] `EncryptionLogger` class replaces all `enclog_*` macros
- [ ] `PacketTrace` class for packet lifecycle tracking
- [ ] Correlation IDs added to packet flow
- [ ] Error handling standardized (no more `FATAL_ERROR_IF` for non-fatal conditions)
- [ ] Hard-coded security score replaced with dynamic calculation
- [ ] All `enclog_*` macro calls removed

---

## 10. Phase 5 — LLM-Agent Friendliness

**Goal**: Make the codebase navigable and understandable by AI coding agents.
**Duration**: 1 week
**Risk**: NONE (documentation-only)

### Step 5.1: Add `AI_CONTEXT.md` to Each Module

Every directory under `src/network/` gets an `AI_CONTEXT.md` with:

```markdown
# Module: [name]

## Purpose
[1-2 sentence description of what this module does]

## Key Classes
- `ClassName` — [1-line description]

## Dependencies
- Depends on: [list]
- Depended on by: [list]

## Invariants
- [Things that must always be true]

## Common Modifications
- To add a new packet handler: [step-by-step]
- To change encryption: [step-by-step]
- To add a new test: [step-by-step]

## File Map
- `filename.cpp` — [what's in it]
```

Target files:
```
src/network/AI_CONTEXT.md
src/network/mtp/AI_CONTEXT.md
src/network/client/AI_CONTEXT.md
src/network/server/AI_CONTEXT.md
```

### Step 5.2: Add Architecture Decision Records

Create `docs/adr/` directory with records for every major design decision:

```
docs/adr/
  001-why-mtp-custom-transport.md
  002-why-aes256gcm-not-chacha20.md
  003-why-0x80-flag-for-encryption-detection.md
  004-why-replay-bitmap-not-timestamp.md
  005-why-hkdf-not-pbkdf2.md
  006-why-deferred-activation.md
```

Each ADR contains: Context, Decision, Consequences, Alternatives Considered.

### Step 5.3: Add State Machine Documentation

The encryption state machine is currently undocumented and implicit. Document it:

```
docs/
  encryption_state_machine.md   — All states, transitions, and guards
  connection_lifecycle.md       — Peer connection/disconnection flow
  packet_flow.md                — How a packet travels from send to receive
```

### Step 5.4: Module Boundaries via Namespace

Currently everything is in `namespace con` or global. Restructure into:

```cpp
namespace crypto { /* AES-256-GCM, HKDF, X25519 */ }
namespace encryption { /* PeerEncryptionState, PacketEncryptor, ReplayProtector */ }
namespace transport { /* Connection, Peer, Channel, threads */ }
namespace protocol { /* ToClientCommand, ToServerCommand, wire format */ }
namespace handlers { /* client and server packet handlers */ }
```

### Step 5.5: File Size Budget

Set maximum file size limits. Any file exceeding these triggers a mandatory split:

| Type | Max Lines | Current Violators |
|---|---|---|
| Header (.h) | 300 lines | `crypto.h` (596), `connection_security.h` (723), `internal.h` (546), `networkprotocol.h` (1,004) |
| Implementation (.cpp) | 800 lines | `clientpackethandler.cpp` (2,296), `serverpackethandler.cpp` (2,117), `threads.cpp` (1,794), `impl.cpp` (1,751), `crypto.cpp` (1,180) |
| Test (.cpp) | 600 lines | None currently |

### Step 5.6: Fix All Naming Issues

| Current | Fixed |
|---|---|
| `m_iteration_packets_avaialble` | `m_iteration_packets_available` |
| `ServerCommandFactory` (in clientopcodes.h) | `ClientToServerCommandFactory` |
| `ClientCommandFactory` (in serveropcodes.h) | `ServerToClientCommandFactory` |
| `m_read_offset` (dual-use) | `m_read_offset` + `m_write_offset` (separate) |
| `PlayerListModifer` | `PlayerListModifier` |
| `PeerHandler` vs `PeerHelper` | `IPeerEventHandler` vs `PeerRefGuard` |

### Phase 5 Deliverables Checklist

- [ ] `AI_CONTEXT.md` for each module directory
- [ ] 6 Architecture Decision Records
- [ ] State machine documentation (encryption, connection, packet flow)
- [ ] Namespace reorganization
- [ ] File size budgets enforced (all files under limits)
- [ ] All naming issues fixed
- [ ] `NETWORKING.md` top-level guide for new developers/agents

---

## 11. Migration Safety: How We Avoid Breaking Things

### The Compile-Test-Commit Cycle

Every step follows this cycle:

1. **Write tests first** (for new interfaces or previously untested code)
2. **Make the structural change** (move code, extract interface, etc.)
3. **Compile** — if it doesn't compile, revert
4. **Run all tests** — if any test fails, revert
5. **Commit with descriptive message**

### Rollback Strategy

Each phase is on its own commit chain. If Phase 2 introduces a regression:
1. Revert to the Phase 1 final commit
2. All Phase 1 improvements are preserved
3. Phase 2 can be retried

### Behavioral Preservation Tests

Before starting any phase, add these regression tests:

```cpp
TEST(Regression, EncryptedPacketRoundtrip) {
    // Encrypt a packet, decrypt it, verify payload matches
}

TEST(Regression, PlaintextPacketAccepted) {
    // Packets without 0x80 flag are processed as plaintext
}

TEST(Regression, SingleplayerNoEncryption) {
    // Single-player mode never activates encryption
}

TEST(Regression, EncryptedPacketRejectedWithoutKey) {
    // Encrypted packet with no key → GCM auth failure → dropped
}
```

These tests ensure that the restructuring never breaks the v9.15 fixes.

---

## 12. File Map: Before → After

### Before (Current Structure)

```
src/network/
  address.h / address.cpp
  clientopcodes.h / clientopcodes.cpp
  connection.h / connection.cpp
  connection_security.h
  crypto.h / crypto.cpp
  encryption_config.h / encryption_config.cpp
  encryption_log.h
  networkexceptions.h
  networkpacket.h / networkpacket.cpp
  networkprotocol.h / networkprotocol.cpp
  peerhandler.h
  serveropcodes.h / serveropcodes.cpp
  socket.h / socket.cpp
  clientpackethandler.cpp           ← 2,296 lines
  serverpackethandler.cpp           ← 2,117 lines
  mtp/
    internal.h
    impl.h / impl.cpp               ← 1,751 lines
    threads.h / threads.cpp         ← 1,794 lines
```

### After (Target Structure)

```
src/network/
  AI_CONTEXT.md                     ← LLM agent entry point
  NETWORKING.md                     ← Human-readable architecture guide

  # Protocol layer — wire format definitions
  protocol/
    AI_CONTEXT.md
    networkprotocol.h / networkprotocol.cpp
    opcodes.h                       ← shared opcode types (no Client/Server dependency)

  # Socket & Address — low-level I/O
  socket/
    AI_CONTEXT.md
    network_socket.h                ← INetworkSocket interface
    address.h / address.cpp
    udp_socket.h / udp_socket.cpp

  # Packet serialization
  packet/
    AI_CONTEXT.md
    networkpacket.h / networkpacket.cpp

  # Encryption & Security
  encryption/
    AI_CONTEXT.md
    crypto.h / crypto.cpp           ← primitives only (AES, HKDF, X25519)
    packet_encryptor.h              ← IPacketEncryptor interface
    aes256gcm_encryptor.h / .cpp    ← concrete encryptor
    key_deriver.h                   ← IKeyDeriver interface
    hkdf_key_deriver.h / .cpp       ← concrete key deriver
    replay_protection.h             ← IReplayProtector interface
    bitmap_replay_protector.h/.cpp  ← concrete replay protector
    peer_encryption_state.h / .cpp  ← simplified (keys + nonces + ECDH, no replay, no fingerprint)
    fingerprint_store.h / .cpp      ← extracted from crypto
    connection_security.h / .cpp    ← extracted methods to .cpp
    encryption_config.h / .cpp
    encryption_logger.h / .cpp      ← replaces enclog_* macros
    packet_trace.h / .cpp           ← structured packet tracing

  # Transport — MTP reliable delivery
  transport/
    AI_CONTEXT.md
    connection.h / connection.cpp    ← IConnection interface (no crypto dependency)
    peerhandler.h
    networkexceptions.h
    internal.h                       ← MTP packet types, structs
    channel.h / channel.cpp          ← extracted from internal/impl
    peer.h / peer.cpp                ← extracted Peer + UDPPeer
    buffered_packet.h / .cpp         ← extracted BufferedPacket helpers
    reliable_buffer.h / .cpp         ← extracted ReliablePacketBuffer
    split_buffer.h / .cpp            ← extracted IncomingSplitBuffer
    connection_core.h / .cpp         ← Connection implementation
    send_thread.h / .cpp             ← ConnectionSendThread only
    receive_thread.h / .cpp          ← ConnectionReceiveThread only

  # Client packet handlers
  client/
    AI_CONTEXT.md
    clientopcodes.h / clientopcodes.cpp
    client_auth_handler.cpp
    client_encryption_handler.cpp
    client_media_handler.cpp
    client_game_handler.cpp
    client_hud_handler.cpp
    client_misc_handler.cpp

  # Server packet handlers
  server/
    AI_CONTEXT.md
    serveropcodes.h / serveropcodes.cpp
    server_auth_handler.cpp
    server_encryption_handler.cpp
    server_game_handler.cpp
    server_misc_handler.cpp
```

**File count**: 28 → ~55 (more files, but each under 300/800 line limits)
**Largest file**: 800 lines (down from 2,296)
**Average file size**: ~180 lines header, ~400 lines implementation (down from ~350/~800)

---

## 13. Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | Splitting `clientpackethandler.cpp` breaks include chains | Medium | High | Use include-split pattern first (files #included into original), then migrate CMake |
| R2 | Interface extraction changes vtable layout, affecting performance | Low | Medium | Profile before/after. Virtual call overhead is negligible compared to UDP I/O |
| R3 | Removing `friend` requires adding many accessors | High | Low | Accessors are cheap and improve encapsulation. Accept the code bloat. |
| R4 | Namespace reorganization breaks `using` declarations | Medium | Low | Use `using` aliases during transition. Search-replace is safe. |
| R5 | Mock infrastructure hides real bugs | Low | High | Always run integration tests with real UDP alongside mock-based unit tests |
| R6 | Test helper consolidation breaks existing tests | Medium | Medium | Create shared helpers first, then migrate tests one at a time |
| R7 | Phase 2 changes are too large for a single commit | High | Medium | Each interface extraction is its own commit. Never combine interface + implementation changes. |

---

## 14. Acceptance Criteria

The restructuring is complete when ALL of the following are true:

### Structural

- [ ] No source file exceeds 800 lines of implementation or 300 lines of header
- [ ] No struct/class exceeds 200 lines of declaration
- [ ] No `friend` declarations in the network layer
- [ ] No `dynamic_cast` in the network layer
- [ ] Zero circular include dependencies (verifiable with include-what-you-use)
- [ ] `connection.h` does not include `crypto.h`

### Testing

- [ ] 600+ test methods (up from 380+)
- [ ] 80%+ of public network methods have at least 1 test
- [ ] All 20 critical untested code paths from Appendix C have tests
- [ ] At least 1 integration test for encrypted connections
- [ ] Mock infrastructure exists for all 6 interfaces

### Code Quality

- [ ] Zero copy-pasted test helpers (all in `network_test_helpers.h`)
- [ ] Zero `enclog_*` macro calls (replaced with `EncryptionLogger`)
- [ ] Zero typos in variable names (`avaialble`, `Modifer`)
- [ ] Key derivation code duplication eliminated (< 20 lines of shared pattern)
- [ ] All hard-coded security scores replaced with dynamic calculation

### Documentation

- [ ] `AI_CONTEXT.md` exists in every module directory
- [ ] 6 Architecture Decision Records written
- [ ] State machine documentation for encryption, connection, packet flow
- [ ] `NETWORKING.md` top-level guide

### Behavioral

- [ ] All v9.15 fixes still work (0x80 flag detection, no grace periods, singleplayer bypass)
- [ ] Game compiles and runs on Linux, Windows
- [ ] All existing tests pass
- [ ] No new compiler warnings

---

## Appendix A — Full File Inventory

| File | Lines | Classes/Structs | # Methods | Problems |
|---|---|---|---|---|
| `address.h` | 72 | `Address`, `IPv6AddressBytes` | 15 | Platform headers leak, global settings |
| `address.cpp` | 220 | — | — | IPv4/IPv6 duplication |
| `clientopcodes.h` | 36 | — | — | Includes full `client/client.h` |
| `clientopcodes.cpp` | 217 | — | — | Sparse tables, naming confusion |
| `clientpackethandler.cpp` | 2,296 | — | 62 | GOD FILE |
| `connection.h` | 87 | `IPeer`, `IConnection` | 12 | Crypto coupling |
| `connection.cpp` | 17 | — | 1 | Raw `new`, hard-coded MTU |
| `connection_security.h` | 723 | `ConnectionSecurityInfo` | 20+ | GOD STRUCT, inline business logic |
| `crypto.h` | 596 | 5 structs | 17+ | GOD STRUCT, inline methods |
| `crypto.cpp` | 1,180 | — | 12 | 300 lines duplication |
| `encryption_config.h` | 72 | `EncryptionConfig` | 4 | Global settings coupling |
| `encryption_config.cpp` | 61 | — | — | Clean |
| `encryption_log.h` | 261 | `EncLog` | 10+ | Macro-based, inline banners |
| `networkexceptions.h` | 57 | 7 exception classes | — | Inconsistent constructors |
| `networkpacket.h` | 147 | `NetworkPacket` | 30+ | Dual-use offset, legacy code |
| `networkpacket.cpp` | 526 | — | — | Repetitive boilerplate |
| `networkprotocol.h` | 1,004 | 7 enums | — | GOD FILE (documentation-heavy) |
| `networkprotocol.cpp` | 87 | — | — | Clean |
| `peerhandler.h` | 33 | `PeerHandler` | 2 | Clean |
| `serveropcodes.h` | 35 | — | — | Includes full `server.h` |
| `serveropcodes.cpp` | 218 | — | — | Naming confusion |
| `serverpackethandler.cpp` | 2,117 | — | 25 | GOD FILE |
| `socket.h` | 38 | `UDPSocket` | 8 | Clean |
| `socket.cpp` | 300 | — | — | IPv4/IPv6 duplication, INTERNET_SIMULATOR |
| `mtp/internal.h` | 546 | 7 structs | 20+ | Crypto coupling, large header |
| `mtp/impl.h` | 316 | 5 classes | 30+ | friend abuse, raw pointers |
| `mtp/impl.cpp` | 1,751 | — | 40+ | GOD FILE (7+ classes) |
| `mtp/threads.h` | 176 | 2 thread classes | 15+ | Typo `avaialble` |
| `mtp/threads.cpp` | 1,794 | — | 20+ | GOD FILE, encryption inline |

**TOTAL**: ~15,477 lines production code, 30 files

---

## Appendix B — God Object Decomposition Details

### B.1: `PeerEncryptionState` Decomposition

**Current** (596 lines in `crypto.h`):

```
PeerEncryptionState
  ├── Key storage (c2s.key, s2c.key, c2s.nonce_counter, s2c.nonce_counter)  → EncryptionKeys
  ├── Replay bitmap (c2s.shiftBitmap, s2c.shiftBitmap, etc.)                → IReplayProtector
  ├── ECDH state (ecdh_private_key, ecdh_public_key, ecdh_shared_secret)    → ECDHState
  ├── Fingerprint data (session_id, peer_fingerprint, local_fingerprint)     → SessionIdentity
  ├── Audit logging (last_audit_time_ms, packets_since_audit)               → EncryptionLogger
  ├── Session management (active, ecdh_completed, is_server)                → SessionState
  └── Key derivation (initFromSRPSessionKey, rotateKeys, mixECDHSecret)     → IKeyDeriver
```

**Target**:

```cpp
// encryption/encryption_keys.h (~80 lines)
struct EncryptionKeys {
    DirectionalKeys c2s, s2c;
    std::atomic<bool> active{false};
    std::mutex mutex;
    auto lock() { return std::unique_lock<std::mutex>(mutex); }
};

// encryption/peer_encryption_state.h (~120 lines)
struct PeerEncryptionState {
    EncryptionKeys keys;
    std::unique_ptr<IReplayProtector> replay_c2s;
    std::unique_ptr<IReplayProtector> replay_s2c;
    std::unique_ptr<IKeyDeriver> key_deriver;
    ECDHState ecdh;
    SessionIdentity identity;
    std::atomic<bool> ecdh_completed{false};
    bool is_server = false;

    void initFromSRPSessionKey(...);
    void activate();
    void disable();
};
```

### B.2: `ConnectionSecurityInfo` Decomposition

**Current** (562 lines in `connection_security.h`):

```
ConnectionSecurityInfo
  ├── Security flags (encryption, cipher, forward_secrecy, auth, replay, cert, tls)  → SecurityFlags
  ├── Security scoring (getBaseSecurityScore, getBonusScore, getSecurityScore)        → SecurityScorer
  ├── Display formatting (getSecurityScoreString, getSecurityScoreBar, getXxxString)  → SecurityFormatter
  └── Statistics tracking (bytes_sent, bytes_received, packets, etc.)                 → ConnectionStats
```

**Target**:

```cpp
// encryption/security_flags.h (~80 lines)
struct SecurityFlags { /* bool flags only */ };

// encryption/security_scorer.h / .cpp (~150 lines)
class SecurityScorer {
    int getBaseScore(const SecurityFlags& flags) const;
    int getBonusScore(const SecurityFlags& flags) const;
    int getTotalScore(const SecurityFlags& flags) const;
};

// encryption/security_formatter.h / .cpp (~100 lines)
class SecurityFormatter {
    static std::string getScoreString(int score);
    static std::string getScoreBar(int score);
    static std::string getEncryptionString(SecurityFlags::Encryption);
    // ...
};

// encryption/connection_stats.h (~60 lines)
struct ConnectionStats { /* counters only */ };

// encryption/connection_security_info.h (~100 lines)
struct ConnectionSecurityInfo {
    SecurityFlags flags;
    ConnectionStats stats;
    // Convenience methods delegate to SecurityScorer/SecurityFormatter
};
```

---

## Appendix C — Test Gap Matrix

### Network Public Methods — Coverage Status

| Class | Method | Has Test? | Priority |
|---|---|---|---|
| `IConnection` | `Serve()` | ⚠️ Partial | MEDIUM |
| `IConnection` | `Connect()` | ⚠️ Partial | MEDIUM |
| `IConnection` | `Connected()` | ⚠️ Partial | LOW |
| `IConnection` | `Disconnect()` | ❌ No | MEDIUM |
| `IConnection` | `DisconnectPeer()` | ❌ No | HIGH |
| `IConnection` | `ReceiveTimeoutMs()` | ⚠️ Partial | MEDIUM |
| `IConnection` | `Send()` | ⚠️ Partial | MEDIUM |
| `IConnection` | `GetPeerID()` | ❌ No | LOW |
| `IConnection` | `GetPeerAddress()` | ❌ No | MEDIUM |
| `IConnection` | `getPeerStat()` | ❌ No | MEDIUM |
| `IConnection` | `getLocalStat()` | ❌ No | MEDIUM |
| `IConnection` | `SetPeerEncryptionState()` | ❌ No | HIGH |
| `IConnection` | `ActivatePeerEncryption()` | ❌ No | HIGH |
| `ConnectionSendThread` | `run()` | ⚠️ Indirect | — |
| `ConnectionSendThread` | `runTimeouts()` | ❌ No | CRITICAL |
| `ConnectionSendThread` | `resendReliable()` | ❌ No | CRITICAL |
| `ConnectionSendThread` | `rawSend()` | ❌ No | HIGH |
| `ConnectionSendThread` | `processReliableCommand()` | ❌ No | CRITICAL |
| `ConnectionSendThread` | `processNonReliableCommand()` | ❌ No | CRITICAL |
| `ConnectionSendThread` | `sendPackets()` | ❌ No | HIGH |
| `ConnectionSendThread` | `sendAsPacketReliable()` | ❌ No | CRITICAL |
| `ConnectionReceiveThread` | `receive()` | ❌ No | CRITICAL |
| `ConnectionReceiveThread` | `processPacket()` | ❌ No | CRITICAL |
| `ConnectionReceiveThread` | `handlePacketType_Control()` | ❌ No | CRITICAL |
| `ConnectionReceiveThread` | `handlePacketType_Original()` | ❌ No | MEDIUM |
| `ConnectionReceiveThread` | `handlePacketType_Split()` | ❌ No | CRITICAL |
| `ConnectionReceiveThread` | `handlePacketType_Reliable()` | ❌ No | HIGH |
| `DirectionalEncryptionState` | `markReceived()` | ❌ No | HIGH |
| `DirectionalEncryptionState` | `isAlreadySeen()` | ❌ No | HIGH |
| `DirectionalEncryptionState` | `shiftBitmap()` | ❌ No | HIGH |
| `PeerEncryptionState` | `rotateKeys()` (cross-side) | ❌ No | HIGH |
| `NetworkPacket` | All `>>`/`<<` operators | ❌ No | MEDIUM |
| `ReliablePacketBuffer` | `insert()` | ❌ No | CRITICAL |
| `IncomingSplitBuffer` | `insert()` | ❌ No | CRITICAL |
| `Channel` | `UpdateTimers()` | ❌ No | HIGH |
| `Channel` | `PutPacketInRingBuffer()` | ❌ No | HIGH |

### Top 10 Most Critical Gaps (Must Fix in Phase 3)

1. `ConnectionReceiveThread::processPacket()` — All packet dispatch, zero coverage
2. `ConnectionSendThread::processReliableCommand()` — All reliable sending
3. `ReliablePacketBuffer::insert()` — Core reliability mechanism
4. `ConnectionReceiveThread::handlePacketType_Split()` — Packet reassembly
5. `ConnectionSendThread::sendAsPacketReliable()` — Reliable queuing
6. `ConnectionSendThread::runTimeouts()` — Peer timeout detection
7. `ConnectionReceiveThread::handlePacketType_Control()` — ACK/NACK flow control
8. `IConnection::SetPeerEncryptionState()` + `ActivatePeerEncryption()` — Encryption at connection layer
9. `DirectionalEncryptionState` bitmap methods — Replay defense
10. Encrypted connection integration test — End-to-end encrypted flow

---

## Appendix D — Key Derivation Deduplication Detail

### Current Code (3 × ~100 lines = ~300 lines)

```
initFromSRPSessionKey():
  salt = HKDF(ikm, "", "Luanti v9 Salt")
  c2s_key = HKDF(ikm, salt, "Luanti v9 C2S Key")
  s2c_key = HKDF(ikm, salt, "Luanti v9 S2C Key")
  c2s_nonce = HKDF(ikm, salt, "Luanti v9 C2S Nonce")
  s2c_nonce = HKDF(ikm, salt, "Luanti v9 S2C Nonce")
  session_id = HKDF(ikm, salt, "Luanti v9 Session ID")
  + logging, error handling, memcpy

rotateKeys():
  salt = HKDF(ikm, "", "Luanti v9.11 Salt (Rotation)")
  c2s_key = HKDF(ikm, salt, "Luanti v9.11 C2S Key (Rotation)")
  ... identical pattern with different info strings

mixECDHSecretIntoKeys():
  salt = HKDF(ikm, "", "Luanti v9.11 Salt (ECDH+SRP)")
  c2s_key = HKDF(ikm, salt, "Luanti v9.11 C2S Key (ECDH+SRP)")
  ... identical pattern with different info strings
```

### Proposed Code (~120 lines total)

```cpp
// key_deriver.h
struct DerivedKeys {
    u8 c2s_key[32], s2c_key[32];
    u8 c2s_nonce[32], s2c_nonce[32];
    u8 session_id[32];
};

class HKDFKeyDeriver {
    DerivedKeys derive(const u8* ikm, size_t ikm_len,
                       const std::string& version_prefix) {
        auto salt = hkdf_sha256(ikm, ikm_len, {}, version_prefix + " Salt");
        auto c2s_key = hkdf_sha256(ikm, ikm_len, salt, version_prefix + " C2S Key");
        auto s2c_key = hkdf_sha256(ikm, ikm_len, salt, version_prefix + " S2C Key");
        auto c2s_nonce = hkdf_sha256(ikm, ikm_len, salt, version_prefix + " C2S Nonce");
        auto s2c_nonce = hkdf_sha256(ikm, ikm_len, salt, version_prefix + " S2C Nonce");
        auto session_id = hkdf_sha256(ikm, ikm_len, salt, version_prefix + " Session ID");
        return {c2s_key, s2c_key, c2s_nonce, s2c_nonce, session_id};
    }
};

// Usage:
void PeerEncryptionState::initFromSRPSessionKey(...) {
    auto keys = HKDFKeyDeriver().derive(ikm, ikm_len, "Luanti v9");
    // copy keys into c2s/s2c state
}

void PeerEncryptionState::rotateKeys() {
    auto keys = HKDFKeyDeriver().derive(ikm, ikm_len, "Luanti v9.11 (Rotation)");
    // copy keys into c2s/s2c state
}

void PeerEncryptionState::mixECDHSecretIntoKeys(...) {
    auto keys = HKDFKeyDeriver().derive(ikm, ikm_len, "Luanti v9.11 (ECDH+SRP)");
    // copy keys into c2s/s2c state
}
```

**Lines saved**: ~180 lines of duplication eliminated.

---

## Appendix E — `friend` Class Elimination Detail

### Current `friend` Declarations

| Class | Friends With | What It Accesses |
|---|---|---|
| `Connection` | `ConnectionSendThread` | `m_command_queue`, `m_udpSocket`, `m_peers`, `m_event` |
| `Connection` | `ConnectionReceiveThread` | Same as above |
| `UDPPeer` | `PeerHelper` | `m_refcount`, `IncUseCount()`, `DecUseCount()` |
| `UDPPeer` | `ConnectionSendThread` | `channels[]`, `resend_timeout`, `m_increment_packets_remaining` |
| `UDPPeer` | `ConnectionReceiveThread` | `channels[]` |
| `UDPPeer` | `Connection` | Full access |

### Replacement Accessors

```cpp
// Connection public interface additions
class Connection {
public:
    // Thread-safe command queue access
    ConnectionCommandPtr dequeueCommand();
    bool hasCommands() const;

    // Socket access (for thread operations)
    INetworkSocket& socket();

    // Peer management
    PeerRef getPeer(session_t id);
    void forEachPeer(std::function<void(session_t, PeerRef&)> fn);

    // Event queueing
    void pushEvent(ConnectionEvent event);
};

// UDPPeer public interface additions
class UDPPeer {
public:
    Channel& getChannel(u8 channelnum);
    const Channel& getChannel(u8 channelnum) const;
    IReplayProtector& getReplayProtector(u8 channelnum);
    float getResendTimeout() const;
    void setResendTimeout(float timeout);
    int getIncrementPacketsRemaining() const;
    void decrementIncrementPacketsRemaining();
};
```

---

*End of restructuring plan. This document should be updated as each phase progresses, marking completed steps and noting any deviations from the original plan.*
