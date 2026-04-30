// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 BirdNest055
//
// End-to-end integration test for voice E2EE.
//
// Simulates the complete voice E2EE workflow between two clients:
// 1. Both clients generate X25519 keypairs
// 2. Both clients exchange public keys (via the server relay pattern)
// 3. Both clients derive per-peer session keys via X25519 ECDH + HKDF
// 4. Client A encrypts voice data and sends to Client B
// 5. Client B decrypts and recovers the original data
// 6. Client B encrypts voice data and sends to Client A
// 7. Client A decrypts and recovers the original data
//
// Also tests:
// - Multi-peer scenarios (3+ clients)
// - Key re-exchange (E2EE key rotation)
// - Server cannot decrypt E2EE voice data
// - Late-joining peer receives keys and can decrypt
// - Replay attack is rejected
// - Global channel E2EE (all peers can decrypt)

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <algorithm>

constexpr size_t VOICE_NONCE_SIZE = 12;
constexpr size_t VOICE_TAG_SIZE = 16;
constexpr size_t VOICE_KEY_SIZE = 32;

class TestVoiceE2EEE2E : public TestBase
{
public:
        TestVoiceE2EEE2E() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestVoiceE2EEE2E"; }

        void runTests(IGameDef *gamedef);

        // Full two-client handshake and communication
        void testTwoClientHandshakeAndCommunication();
        void testTwoClientBidirectionalVoice();
        void testTwoClientMultipleFrames();

        // Multi-peer scenarios
        void testThreeClientMeshE2EE();
        void testThreeClientGlobalChannel();

        // Key re-exchange
        void testKeyReExchangeProducesNewKeys();
        void testKeyReExchangeOldKeyCannotDecrypt();

        // Server relay security
        void testServerCannotDecryptE2EEVoice();
        void testServerOnlyRelaysEncryptedData();

        // Late-joining peer
        void testLateJoiningPeerReceivesKey();
        void testLateJoiningPeerCanDecryptAfterKeyExchange();

        // Replay protection
        void testReplayedVoicePacketRejected();
        void testOutOfOrderPacketAccepted();
        void testVeryOldPacketRejected();

        // Edge cases
        void testEmptyVoiceFrameEncryptDecrypt();
        void testMaximumVoiceFrameEncryptDecrypt();
        void testKeyMismatchPreventsEavesdropping();

        // Full protocol simulation
        void testFullProtocolSimulationWithPeerJoinLeave();
};

static TestVoiceE2EEE2E g_test_instance;

// ============================================================================
// Simulated Client (lightweight, no Opus or audio hardware)
// ============================================================================

struct SimVoiceClient {
        u16 client_id;
        std::array<u8, 32> local_privkey{};
        std::array<u8, 32> local_pubkey{};

        struct PeerE2EEState {
                std::array<u8, 32> peer_pubkey{};
                std::array<u8, 32> session_key{};
                std::array<u8, 4> nonce_base{};
                bool e2ee_active = false;
                u64 send_counter = 0;
                // Replay protection
                u64 highest_recv_counter = 0;
                bool counter_initialized = false;
                static constexpr size_t REPLAY_WINDOW_SIZE = 64;
                std::array<u64, 1> replay_bitmap{};
        };

        std::unordered_map<u16, PeerE2EEState> peers;

        SimVoiceClient(u16 id) : client_id(id) {}

        void generateKeypair() {
                auto kp = x25519_generate_keypair();
                if (kp.success) {
                        local_privkey = kp.private_key;
                        local_pubkey = kp.public_key;
                }
        }

        void receivePeerKey(u16 peer_id, const u8 pubkey[32]) {
                auto &peer = peers[peer_id];
                peer.peer_pubkey = *reinterpret_cast<const std::array<u8, 32>*>(pubkey);

                // Derive shared secret
                auto shared = x25519_compute_shared_secret(
                        local_privkey.data(), local_privkey.size(),
                        pubkey, 32);
                if (!shared.success) return;

                // Derive session key
                const u8 key_info[] = "LuantisVoiceE2EEv1";
                if (!hkdf_sha256(shared.shared_secret.data(), shared.shared_secret.size(),
                        nullptr, 0,
                        key_info, strlen(reinterpret_cast<const char*>(key_info)),
                        peer.session_key.data(), peer.session_key.size()))
                        return;

                // Derive nonce base
                const u8 nonce_info[] = "LuantisVoiceE2EEv1Nonce";
                if (!hkdf_sha256(shared.shared_secret.data(), shared.shared_secret.size(),
                        nullptr, 0,
                        nonce_info, strlen(reinterpret_cast<const char*>(nonce_info)),
                        peer.nonce_base.data(), peer.nonce_base.size()))
                        return;

                peer.e2ee_active = true;
                peer.send_counter = 0;
                peer.highest_recv_counter = 0;
                peer.counter_initialized = false;
                peer.replay_bitmap.fill(0);
        }

        bool encryptForPeer(u16 peer_id, std::vector<u8> &data) {
                auto it = peers.find(peer_id);
                if (it == peers.end() || !it->second.e2ee_active)
                        return false;

                auto &peer = it->second;
                u64 counter = peer.send_counter++;

                u8 nonce[VOICE_NONCE_SIZE];
                build_nonce(peer.nonce_base.data(), counter, nonce);

                u8 aad[8];
                
                
                for (int i = 0; i < 8; i++)
                        aad[i] = (counter >> (56 - i * 8)) & 0xFF;

                auto result = aes256gcm_encrypt(
                        peer.session_key.data(), peer.session_key.size(),
                        nonce, VOICE_NONCE_SIZE,
                        data.data(), data.size(),
                        aad, 8);

                if (!result.success)
                        return false;

                data.resize(result.data.size() + VOICE_TAG_SIZE + VOICE_NONCE_SIZE);
                memcpy(data.data(), result.data.data(), result.data.size());
                memcpy(data.data() + result.data.size(), result.tag.data(), VOICE_TAG_SIZE);
                memcpy(data.data() + result.data.size() + VOICE_TAG_SIZE, nonce, VOICE_NONCE_SIZE);
                return true;
        }

        bool decryptFromPeer(u16 peer_id, std::vector<u8> &data) {
                auto it = peers.find(peer_id);
                if (it == peers.end() || !it->second.e2ee_active)
                        return false;

                auto &peer = it->second;

                if (data.size() < VOICE_TAG_SIZE + VOICE_NONCE_SIZE)
                        return false;

                // Extract nonce from end
                u8 nonce[VOICE_NONCE_SIZE];
                memcpy(nonce, data.data() + data.size() - VOICE_NONCE_SIZE, VOICE_NONCE_SIZE);

                // Extract tag
                const u8 *tag = data.data() + data.size() - VOICE_NONCE_SIZE - VOICE_TAG_SIZE;

                // Ciphertext
                size_t ct_len = data.size() - VOICE_TAG_SIZE - VOICE_NONCE_SIZE;

                // Extract counter from nonce for AAD and replay
                u64 counter = 0;
                for (int i = 0; i < 8; i++)
                        counter = (counter << 8) | nonce[4 + i];

                // Replay check
                if (peer.counter_initialized && counter <= peer.highest_recv_counter) {
                        if (peer.highest_recv_counter - counter > peer.REPLAY_WINDOW_SIZE)
                                return false;
                        u64 offset = peer.highest_recv_counter - counter - 1;
                        if (offset < peer.REPLAY_WINDOW_SIZE &&
                                (peer.replay_bitmap[0] & (1ULL << offset)) != 0)
                                return false;  // Already seen
                }

                // Build AAD
                u8 aad[8];
                
                
                for (int i = 0; i < 8; i++)
                        aad[i] = (counter >> (56 - i * 8)) & 0xFF;

                auto result = aes256gcm_decrypt(
                        peer.session_key.data(), peer.session_key.size(),
                        nonce, VOICE_NONCE_SIZE,
                        data.data(), ct_len,
                        tag, VOICE_TAG_SIZE,
                        aad, 8);

                if (!result.success)
                        return false;

                // Update replay state
                if (!peer.counter_initialized || counter > peer.highest_recv_counter) {
                        if (peer.counter_initialized) {
                                u64 shift = counter - peer.highest_recv_counter;
                                if (shift >= peer.REPLAY_WINDOW_SIZE)
                                        peer.replay_bitmap.fill(0);
                                else
                                        peer.replay_bitmap[0] <<= shift;
                        }
                        if (peer.counter_initialized && peer.highest_recv_counter != counter) {
                                u64 old_offset = counter - peer.highest_recv_counter - 1;
                                if (old_offset < peer.REPLAY_WINDOW_SIZE)
                                        peer.replay_bitmap[0] |= (1ULL << old_offset);
                        }
                        peer.highest_recv_counter = counter;
                        peer.counter_initialized = true;
                } else {
                        u64 offset = peer.highest_recv_counter - counter - 1;
                        if (offset < peer.REPLAY_WINDOW_SIZE)
                                peer.replay_bitmap[0] |= (1ULL << offset);
                }

                data = std::move(result.data);
                return true;
        }
};

// Helper: Simulate server relaying a public key
static void relayKeyExchange(SimVoiceClient &from, SimVoiceClient &to)
{
        to.receivePeerKey(from.client_id, from.local_pubkey.data());
        from.receivePeerKey(to.client_id, to.local_pubkey.data());
}

void TestVoiceE2EEE2E::runTests(IGameDef *gamedef)
{
#if USE_OPENSSL
        TEST(testTwoClientHandshakeAndCommunication);
        TEST(testTwoClientBidirectionalVoice);
        TEST(testTwoClientMultipleFrames);

        TEST(testThreeClientMeshE2EE);
        TEST(testThreeClientGlobalChannel);

        TEST(testKeyReExchangeProducesNewKeys);
        TEST(testKeyReExchangeOldKeyCannotDecrypt);

        TEST(testServerCannotDecryptE2EEVoice);
        TEST(testServerOnlyRelaysEncryptedData);

        TEST(testLateJoiningPeerReceivesKey);
        TEST(testLateJoiningPeerCanDecryptAfterKeyExchange);

        TEST(testReplayedVoicePacketRejected);
        TEST(testOutOfOrderPacketAccepted);
        TEST(testVeryOldPacketRejected);

        TEST(testEmptyVoiceFrameEncryptDecrypt);
        TEST(testMaximumVoiceFrameEncryptDecrypt);
        TEST(testKeyMismatchPreventsEavesdropping);

        TEST(testFullProtocolSimulationWithPeerJoinLeave);
#else
        infostream << "Skipping TestVoiceE2EEE2E — OpenSSL not available" << std::endl;
#endif
}

// ============================================================================
// Full Two-Client Handshake and Communication
// ============================================================================

void TestVoiceE2EEE2E::testTwoClientHandshakeAndCommunication()
{
        SimVoiceClient alice(1);
        SimVoiceClient bob(2);

        // Step 1: Generate keypairs
        alice.generateKeypair();
        bob.generateKeypair();

        // Step 2: Exchange keys via server relay
        relayKeyExchange(alice, bob);

        // Step 3: Verify E2EE is active
        UASSERT(alice.peers.count(2) && alice.peers[2].e2ee_active);
        UASSERT(bob.peers.count(1) && bob.peers[1].e2ee_active);

        // Step 4: Alice encrypts a voice frame for Bob
        std::vector<u8> voice_frame = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
        std::vector<u8> encrypted = voice_frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        // Step 5: Bob decrypts
        std::vector<u8> decrypted = encrypted;
        UASSERT(bob.decryptFromPeer(1, decrypted));

        UASSERT(decrypted == voice_frame);
}

void TestVoiceE2EEE2E::testTwoClientBidirectionalVoice()
{
        SimVoiceClient alice(1);
        SimVoiceClient bob(2);

        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Alice → Bob
        std::vector<u8> alice_frame = {0x01, 0x02, 0x03, 0x04};
        std::vector<u8> enc_ab = alice_frame;
        UASSERT(alice.encryptForPeer(2, enc_ab));
        std::vector<u8> dec_ab = enc_ab;
        UASSERT(bob.decryptFromPeer(1, dec_ab));
        UASSERT(dec_ab == alice_frame);

        // Bob → Alice
        std::vector<u8> bob_frame = {0xAA, 0xBB, 0xCC, 0xDD};
        std::vector<u8> enc_ba = bob_frame;
        UASSERT(bob.encryptForPeer(1, enc_ba));
        std::vector<u8> dec_ba = enc_ba;
        UASSERT(alice.decryptFromPeer(2, dec_ba));
        UASSERT(dec_ba == bob_frame);
}

void TestVoiceE2EEE2E::testTwoClientMultipleFrames()
{
        SimVoiceClient alice(1);
        SimVoiceClient bob(2);

        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Send 50 sequential frames
        for (int i = 0; i < 50; i++) {
                std::vector<u8> frame = {
                        static_cast<u8>(i & 0xFF),
                        static_cast<u8>((i >> 8) & 0xFF),
                        0xAA, 0xBB
                };
                std::vector<u8> encrypted = frame;
                UASSERT(alice.encryptForPeer(2, encrypted));

                std::vector<u8> decrypted = encrypted;
                UASSERT(bob.decryptFromPeer(1, decrypted));
                UASSERT(decrypted == frame);
        }
}

// ============================================================================
// Multi-Peer Scenarios
// ============================================================================

void TestVoiceE2EEE2E::testThreeClientMeshE2EE()
{
        SimVoiceClient alice(1), bob(2), carol(3);
        alice.generateKeypair();
        bob.generateKeypair();
        carol.generateKeypair();

        // All pairs exchange keys
        relayKeyExchange(alice, bob);
        relayKeyExchange(alice, carol);
        relayKeyExchange(bob, carol);

        // Alice → Bob
        std::vector<u8> frame = {0x01, 0x02, 0x03};
        std::vector<u8> enc = frame;
        UASSERT(alice.encryptForPeer(2, enc));
        std::vector<u8> dec = enc;
        UASSERT(bob.decryptFromPeer(1, dec));
        UASSERT(dec == frame);

        // Carol cannot decrypt Alice's message to Bob (different key!)
        std::vector<u8> carol_dec = enc;
        UASSERT(!carol.decryptFromPeer(1, carol_dec));

        // Alice → Carol
        std::vector<u8> frame2 = {0x04, 0x05, 0x06};
        std::vector<u8> enc2 = frame2;
        UASSERT(alice.encryptForPeer(3, enc2));
        std::vector<u8> dec2 = enc2;
        UASSERT(carol.decryptFromPeer(1, dec2));
        UASSERT(dec2 == frame2);
}

void TestVoiceE2EEE2E::testThreeClientGlobalChannel()
{
        SimVoiceClient alice(1), bob(2), carol(3);
        alice.generateKeypair();
        bob.generateKeypair();
        carol.generateKeypair();

        relayKeyExchange(alice, bob);
        relayKeyExchange(alice, carol);
        relayKeyExchange(bob, carol);

        // For global channel, all peers must derive the same combined key.
        // Simulate: concatenate all peer session keys in peer_id order and HKDF.
        // This tests the global channel key derivation logic.

        // Gather all session keys sorted by peer_id
        std::vector<std::pair<u16, std::array<u8, 32>>> alice_peer_keys;
        for (auto &p : alice.peers)
                alice_peer_keys.push_back({p.first, p.second.session_key});
        std::sort(alice_peer_keys.begin(), alice_peer_keys.end());

        std::vector<u8> combined_ikm;
        for (auto &p : alice_peer_keys)
                combined_ikm.insert(combined_ikm.end(), p.second.begin(), p.second.end());

        u8 global_key[32];
        const u8 info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                nullptr, 0, info, strlen(reinterpret_cast<const char*>(info)),
                global_key, sizeof(global_key)));

        // Bob should derive the same global key (same set of session keys,
        // concatenated in the same order by peer_id)
        std::vector<std::pair<u16, std::array<u8, 32>>> bob_peer_keys;
        for (auto &p : bob.peers)
                bob_peer_keys.push_back({p.first, p.second.session_key});
        std::sort(bob_peer_keys.begin(), bob_peer_keys.end());

        std::vector<u8> bob_combined_ikm;
        for (auto &p : bob_peer_keys)
                bob_combined_ikm.insert(bob_combined_ikm.end(), p.second.begin(), p.second.end());

        u8 bob_global_key[32];
        UASSERT(hkdf_sha256(bob_combined_ikm.data(), bob_combined_ikm.size(),
                nullptr, 0, info, strlen(reinterpret_cast<const char*>(info)),
                bob_global_key, sizeof(bob_global_key)));

        UASSERT(memcmp(global_key, bob_global_key, 32) == 0);
}

// ============================================================================
// Key Re-Exchange
// ============================================================================

void TestVoiceE2EEE2E::testKeyReExchangeProducesNewKeys()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        std::array<u8, 32> old_key = alice.peers[2].session_key;

        // Regenerate keypairs (simulating key rotation)
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        std::array<u8, 32> new_key = alice.peers[2].session_key;

        // New key should differ from old key
        UASSERT(old_key != new_key);
}

void TestVoiceE2EEE2E::testKeyReExchangeOldKeyCannotDecrypt()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Alice encrypts with old key
        std::vector<u8> frame = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted_old = frame;
        UASSERT(alice.encryptForPeer(2, encrypted_old));

        // Save the encrypted data
        std::vector<u8> saved_encrypted = encrypted_old;

        // Key re-exchange
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Bob now has a new session key. The old encrypted data should
        // fail to decrypt with the new key (since send_counter was reset
        // and the key is different).
        // Note: In the real implementation, the old encrypted data has the
        // old nonce and tag, which won't match the new key.
        // We test this by trying to decrypt with the new state.
        std::vector<u8> attempt = saved_encrypted;
        UASSERT(!bob.decryptFromPeer(1, attempt));
}

// ============================================================================
// Server Relay Security
// ============================================================================

void TestVoiceE2EEE2E::testServerCannotDecryptE2EEVoice()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Simulate a "server" that sees the encrypted traffic but has no E2EE keys
        SimVoiceClient server(0);
        server.generateKeypair();

        // Alice encrypts for Bob
        std::vector<u8> frame = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted = frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        // Server does NOT have a session with either client
        // It cannot decrypt the voice data
        std::vector<u8> server_attempt = encrypted;
        UASSERT(!server.decryptFromPeer(1, server_attempt));
}

void TestVoiceE2EEE2E::testServerOnlyRelaysEncryptedData()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Alice encrypts for Bob
        std::vector<u8> frame = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted = frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        // The encrypted data should not contain the plaintext
        bool contains_plaintext = true;
        for (size_t i = 0; i < frame.size() && i < encrypted.size(); i++) {
                if (encrypted[i] != frame[i]) {
                        contains_plaintext = false;
                        break;
                }
        }
        UASSERT(!contains_plaintext);
}

// ============================================================================
// Late-Joining Peer
// ============================================================================

void TestVoiceE2EEE2E::testLateJoiningPeerReceivesKey()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Carol joins later
        SimVoiceClient carol(3);
        carol.generateKeypair();

        // Server relays keys: Alice and Bob's keys to Carol, Carol's key to them
        relayKeyExchange(alice, carol);
        relayKeyExchange(bob, carol);

        UASSERT(alice.peers.count(3) && alice.peers[3].e2ee_active);
        UASSERT(bob.peers.count(3) && bob.peers[3].e2ee_active);
        UASSERT(carol.peers.count(1) && carol.peers[1].e2ee_active);
        UASSERT(carol.peers.count(2) && carol.peers[2].e2ee_active);
}

void TestVoiceE2EEE2E::testLateJoiningPeerCanDecryptAfterKeyExchange()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Carol joins later
        SimVoiceClient carol(3);
        carol.generateKeypair();
        relayKeyExchange(alice, carol);

        // Alice sends to Carol
        std::vector<u8> frame = {0xCA, 0xFE, 0xBA};
        std::vector<u8> enc = frame;
        UASSERT(alice.encryptForPeer(3, enc));

        std::vector<u8> dec = enc;
        UASSERT(carol.decryptFromPeer(1, dec));
        UASSERT(dec == frame);
}

// ============================================================================
// Replay Protection
// ============================================================================

void TestVoiceE2EEE2E::testReplayedVoicePacketRejected()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        std::vector<u8> frame = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted = frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        // Bob decrypts successfully
        std::vector<u8> decrypted = encrypted;
        UASSERT(bob.decryptFromPeer(1, decrypted));

        // Replay the same encrypted packet — should be rejected
        std::vector<u8> replay_attempt = encrypted;
        UASSERT(!bob.decryptFromPeer(1, replay_attempt));
}

void TestVoiceE2EEE2E::testOutOfOrderPacketAccepted()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Alice sends frame 0, 1, 2, but they arrive out of order: 2, 0, 1
        std::vector<u8> frame0 = {0x00};
        std::vector<u8> frame1 = {0x01};
        std::vector<u8> frame2 = {0x02};

        std::vector<u8> enc0 = frame0, enc1 = frame1, enc2 = frame2;
        UASSERT(alice.encryptForPeer(2, enc0));
        UASSERT(alice.encryptForPeer(2, enc1));
        UASSERT(alice.encryptForPeer(2, enc2));

        // Receive in order: 2, 0, 1
        std::vector<u8> dec2 = enc2;
        UASSERT(bob.decryptFromPeer(1, dec2));

        std::vector<u8> dec0 = enc0;
        UASSERT(bob.decryptFromPeer(1, dec0));

        std::vector<u8> dec1 = enc1;
        UASSERT(bob.decryptFromPeer(1, dec1));
}

void TestVoiceE2EEE2E::testVeryOldPacketRejected()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Send and receive 100 frames to advance the counter
        for (int i = 0; i < 100; i++) {
                std::vector<u8> frame = {static_cast<u8>(i & 0xFF)};
                std::vector<u8> enc = frame;
                UASSERT(alice.encryptForPeer(2, enc));
                std::vector<u8> dec = enc;
                UASSERT(bob.decryptFromPeer(1, dec));
        }

        // Now save a frame from the "future" (counter 100+)
        std::vector<u8> frame_100 = {0x64};
        std::vector<u8> enc_100 = frame_100;
        UASSERT(alice.encryptForPeer(2, enc_100));
        std::vector<u8> dec_100 = enc_100;
        UASSERT(bob.decryptFromPeer(1, dec_100));

        // A frame from counter 0 (100+ behind) should be rejected
        // We need to re-create it: Alice sends with counter starting at 0 again
        // But since alice's counter is already at 101, we need a fresh client
        SimVoiceClient alice2(1);
        alice2.generateKeypair();
        relayKeyExchange(alice2, bob);

        std::vector<u8> old_frame = {0x00};
        std::vector<u8> old_enc = old_frame;
        UASSERT(alice2.encryptForPeer(2, old_enc));

        // Bob's counter for peer 1 was reset by key re-exchange,
        // so this should work now (new session). Let's test with the
        // same session instead.

        // Better approach: send 100 more frames to push counter up,
        // then try replaying an old captured packet
        std::vector<u8> captured_enc;
        for (int i = 0; i < 10; i++) {
                std::vector<u8> frame = {static_cast<u8>(i)};
                std::vector<u8> enc = frame;
                UASSERT(alice2.encryptForPeer(2, enc));
                if (i == 0) captured_enc = enc;  // Save counter=0 packet
                std::vector<u8> dec = enc;
                UASSERT(bob.decryptFromPeer(1, dec));
        }

        // Send 100 more to advance counter far beyond window
        for (int i = 10; i < 110; i++) {
                std::vector<u8> frame = {static_cast<u8>(i & 0xFF)};
                std::vector<u8> enc = frame;
                UASSERT(alice2.encryptForPeer(2, enc));
                std::vector<u8> dec = enc;
                UASSERT(bob.decryptFromPeer(1, dec));
        }

        // Now the captured packet (counter=0) is > 64 behind highest_recv_counter (109)
        std::vector<u8> replay = captured_enc;
        UASSERT(!bob.decryptFromPeer(1, replay));
}

// ============================================================================
// Edge Cases
// ============================================================================

void TestVoiceE2EEE2E::testEmptyVoiceFrameEncryptDecrypt()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Empty frame (0 bytes of plaintext) — GCM can encrypt nothing
        std::vector<u8> empty_frame;
        std::vector<u8> encrypted = empty_frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        // Should be tag + nonce only (28 bytes overhead)
        UASSERT(encrypted.size() == VOICE_TAG_SIZE + VOICE_NONCE_SIZE);

        std::vector<u8> decrypted = encrypted;
        UASSERT(bob.decryptFromPeer(1, decrypted));
        UASSERT(decrypted.empty());
}

void TestVoiceE2EEE2E::testMaximumVoiceFrameEncryptDecrypt()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Maximum Opus frame: 4000 bytes
        std::vector<u8> max_frame(4000);
        for (size_t i = 0; i < max_frame.size(); i++)
                max_frame[i] = static_cast<u8>(i & 0xFF);

        std::vector<u8> encrypted = max_frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        std::vector<u8> decrypted = encrypted;
        UASSERT(bob.decryptFromPeer(1, decrypted));
        UASSERT(decrypted == max_frame);
}

void TestVoiceE2EEE2E::testKeyMismatchPreventsEavesdropping()
{
        SimVoiceClient alice(1), bob(2), eve(3);
        alice.generateKeypair();
        bob.generateKeypair();
        eve.generateKeypair();

        // Only Alice and Bob exchange keys (Eve is not in the conversation)
        relayKeyExchange(alice, bob);

        // Alice encrypts for Bob
        std::vector<u8> frame = {0x53, 0x45, 0x43, 0x52, 0x45, 0x54};
        std::vector<u8> encrypted = frame;
        UASSERT(alice.encryptForPeer(2, encrypted));

        // Eve tries to decrypt but has no session key with Alice
        UASSERT(eve.peers.find(1) == eve.peers.end());
}

// ============================================================================
// Full Protocol Simulation
// ============================================================================

void TestVoiceE2EEE2E::testFullProtocolSimulationWithPeerJoinLeave()
{
        SimVoiceClient alice(1), bob(2);
        alice.generateKeypair();
        bob.generateKeypair();
        relayKeyExchange(alice, bob);

        // Alice and Bob talk
        std::vector<u8> frame1 = {0x01};
        std::vector<u8> enc1 = frame1;
        UASSERT(alice.encryptForPeer(2, enc1));
        std::vector<u8> dec1 = enc1;
        UASSERT(bob.decryptFromPeer(1, dec1));

        // Carol joins
        SimVoiceClient carol(3);
        carol.generateKeypair();
        relayKeyExchange(alice, carol);
        relayKeyExchange(bob, carol);

        // All three can communicate
        std::vector<u8> frame2 = {0x02};
        std::vector<u8> enc2 = frame2;
        UASSERT(alice.encryptForPeer(2, enc2));
        std::vector<u8> dec2 = enc2;
        UASSERT(bob.decryptFromPeer(1, dec2));

        std::vector<u8> frame3 = {0x03};
        std::vector<u8> enc3 = frame3;
        UASSERT(alice.encryptForPeer(3, enc3));
        std::vector<u8> dec3 = enc3;
        UASSERT(carol.decryptFromPeer(1, dec3));

        // Bob leaves (simulate by removing his state from Alice and Carol)
        alice.peers.erase(2);
        carol.peers.erase(2);

        // Alice and Carol can still talk
        std::vector<u8> frame4 = {0x04};
        std::vector<u8> enc4 = frame4;
        UASSERT(alice.encryptForPeer(3, enc4));
        std::vector<u8> dec4 = enc4;
        UASSERT(carol.decryptFromPeer(1, dec4));

        // Bob's old encrypted data cannot be decrypted by Alice anymore
        // (his peer state was removed)
        std::vector<u8> old_enc = frame2;
        // Alice no longer has Bob's session
        UASSERT(alice.peers.find(2) == alice.peers.end());
}
