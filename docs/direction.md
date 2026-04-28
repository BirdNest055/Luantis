# Luanti-Secure Direction Document

> **Note:** This document extends the Luanti direction with Luanti-Secure-specific goals. The upstream Luanti goals below are preserved for reference.

## 0. Luanti-Secure-Specific Roadmap

Luanti-Secure is a fork of Luanti focused on **real, verifiable encrypted communications**. The following are the medium-term goals specific to Luanti-Secure, tracked as version milestones.

### 0.1 Encryption Completeness (v9.x)

- **Phase 7: Toggleable Overlay System** — Mini/Standard/Detailed security overlay modes, toggleable from Settings. Currently the overlay is a simple text banner; this would add a proper multi-mode system with color-coded lock icons.
- **Phase 8: Security Info Settings Tab** — A proper in-game tab showing real-time security details: cipher, key exchange, auth method, replay protection status, packet stats, and honest limitations.
- **Phase 10: CI/CD** — GitHub Actions workflow for automated builds and test runs across platforms.
- **Crypto Layer Reconciliation** — Two parallel crypto API implementations exist (top-level X25519 API and detailed P-256/ECDSA API in `src/network/crypto/`). These need to be merged: use X25519 for key exchange, add ECDSA verification, use the replay-protected NonceCounter, and adopt the 3-message handshake.

### 0.2 Forward Secrecy (v10.x)

The current encryption architecture derives keys from the SRP session key, which is itself derived from the password. This means that if the password is compromised, all past sessions can be decrypted. Forward secrecy requires adding an ephemeral key exchange (X25519) on top of SRP:
- Generate ephemeral X25519 key pairs on both sides
- Exchange public keys in a post-auth handshake
- Derive a second set of session keys from the ECDH shared secret
- Use a dual-key derivation that combines SRP + ECDH

### 0.3 Certificate & Identity Management (v11.x)

Currently using Trust On First Use (TOFU) with SRP verifier hash. Future work:
- Self-signed certificate management for servers
- ECDSA identity keys for server authentication
- Certificate pinning in client
- Optional CA-based verification mode

### 0.4 Quantum-Resistant Key Exchange (v12.x, research)

AES-256 is believed to be quantum-resistant, but SRP is vulnerable to quantum attacks. Research:
- Post-quantum key encapsulation mechanisms (ML-KEM / Kyber)
- Hybrid classical+PQ key exchange
- Migration path for existing deployments


## 1. Upstream Luanti Long-term Roadmap

The long-term roadmaps, aims, and guiding philosophies are set out using the
following documents:

* [What is Minetest? (archived)](https://web.archive.org/web/20160328054721/http://c55.me/blog/?p=1491)
* [celeron55's roadmap](https://forum.luanti.org/viewtopic.php?t=9177)
* [celeron55's comment in "A clear mission statement for Minetest is missing"](https://github.com/luanti-org/luanti/issues/3476#issuecomment-167399287)
* [Core developer to-do/wish lists](https://forum.luanti.org/viewforum.php?f=7)

## 2. Medium-term Roadmap

These are the current medium-term goals for Luanti development, in no
particular order.

These goals were created from the top points in a
[roadmap brainstorm](https://github.com/luanti-org/luanti/issues/16162).
This is reviewed approximately every two years.

Pull requests that address one of these goals will be labeled as "Roadmap".
PRs that are not on the roadmap will be closed unless they receive a concept
approval within a month. Issues can be used for preapproval.
Bug fixes are exempt from this, and are always accepted and prioritized.
See [CONTRIBUTING.md](../.github/CONTRIBUTING.md) for more info.

### 2.1 SSCSM

Server-sent client-side modding (SSCSM) is a long requested feature (both by
modders and core devs) where, as the name suggests, the server sends modding
scripts to the client for it to execute in a sandbox. This allows mods to run
code on the client, similar to how web pages can run javascript code in web
browsers.

Due to the avoided network latency, the engine can provide APIs in this new
scripting environment for things that need to happen in a synchronous fashion,
allowing mods to overwrite various things that are currently hard-coded in the
client.
For some APIs, it also just makes more sense to have them exist on the client,
and not tunnel every call through the server, only leading to technical debt
once SSCSM arrives.
Lastly, parts of the computation of mods can be moved to the client, improving
server performance.

Instead of focusing on short-term solutions that will inevitably lead to more
technical debt to deal with, SSCSM paves the way for a cleaner architecture
designed to stay.

### 2.2 Input Handling

Luanti keys are currently limited to a small subset, not allowing game developers
to map the majority of the keys a device usually offers. This limits the possibilities
of game creators, forcing them to either implement a workaround or, worse, forget
about it.

Using a gamepad also represents a known issue in Luanti, as some devices might not
work at all or result in an uncomfortable user experience.

### 2.3 UI Improvements

A [formspec replacement](https://github.com/luanti-org/luanti/issues/6527) is
needed to make GUIs better and easier to create. This replacement could also
be a replacement for HUDs, allowing for a unified API.

A [new mainmenu](https://github.com/luanti-org/luanti/issues/6733) is needed to
improve user experience. First impressions matter, and the current main menu
doesn't do a very good job at selling Luanti or explaining what it is.

The UI code is undergoing rapid changes, so it is especially important to make
an issue for any large changes before spending lots of time.
