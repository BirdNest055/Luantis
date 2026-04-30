// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 BirdNest055

#include "voice_chat.h"

#if USE_VOICE_CHAT

#include "log_internal.h"
#include "settings.h"
#include "config.h"
#include "porting.h"
#include "util/numeric.h"
#include "network/crypto.h"  // hkdf_sha256, x25519_compute_shared_secret, build_nonce, aes256gcm_encrypt/decrypt

#if USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#endif

#include <cstring>
#include <cmath>
#include <algorithm>

// ============================================================
// Construction / Destruction
// ============================================================

VoiceChatManager::VoiceChatManager()
{
        m_local_privkey.resize(32, 0);
        m_local_pubkey.resize(32, 0);
}

VoiceChatManager::~VoiceChatManager()
{
        deinit();
}

// ============================================================
// Lifecycle
// ============================================================

void VoiceChatManager::init()
{
        if (m_initialized)
                return;

        infostream << "VoiceChat: Initializing voice chat subsystem" << std::endl;

        initOpus();
        generateEphemeralKeypair();

        m_volume = g_settings->getFloat("voice_chat_volume", 0.0f, 1.0f);
        m_e2ee_enabled = g_settings->getBool("voice_chat_e2ee");
        m_noise_suppression = g_settings->getBool("voice_chat_noise_suppression");
        m_bitrate = g_settings->getS32("voice_chat_bitrate");
        m_vad_threshold = g_settings->getFloat("voice_chat_vad_threshold", 0.0f, 1.0f);

        std::string mode_str = g_settings->get("voice_chat_mode");
        if (mode_str == "toggle")
                m_mode = VoiceChatMode::TOGGLE;
        else
                m_mode = VoiceChatMode::PTT;

        m_initialized = true;
        infostream << "VoiceChat: Initialized (mode=" << (m_mode == VoiceChatMode::PTT ? "PTT" : "toggle")
                   << ", e2ee=" << m_e2ee_enabled << ", bitrate=" << m_bitrate << ")" << std::endl;
}

void VoiceChatManager::deinit()
{
        if (!m_initialized)
                return;

        infostream << "VoiceChat: Shutting down voice chat subsystem" << std::endl;

        // Stop transmitting
        if (m_is_transmitting) {
                m_is_transmitting = false;
                if (sendVoiceStop)
                        sendVoiceStop(m_active_channel);
        }

        deinitOpus();
        m_peer_decoders.clear();
        m_peers.clear();
        m_groups.clear();

        m_initialized = false;
}

void VoiceChatManager::step(float dtime)
{
        if (!isVoiceActive() || !m_initialized)
                return;

        // Determine if we should be transmitting
        bool should_transmit = false;

        if (m_mode == VoiceChatMode::PTT) {
                should_transmit = m_ptt_active;
        } else if (m_mode == VoiceChatMode::TOGGLE) {
                should_transmit = m_voice_toggle_on;
        }

        // Start/stop transmission
        if (should_transmit && !m_is_transmitting) {
                m_is_transmitting = true;
                m_seq_num = 0;
                if (sendVoiceStart)
                        sendVoiceStart(m_active_channel);
                infostream << "VoiceChat: Started transmitting on channel " << (int)m_active_channel << std::endl;
        } else if (!should_transmit && m_is_transmitting) {
                m_is_transmitting = false;
                if (sendVoiceStop)
                        sendVoiceStop(m_active_channel);
                infostream << "VoiceChat: Stopped transmitting" << std::endl;
        }

        // Capture and send audio frames while transmitting
        if (m_is_transmitting) {
                m_transmit_timer += dtime;
                while (m_transmit_timer >= m_frame_interval) {
                        m_transmit_timer -= m_frame_interval;

                        // Capture audio
                        std::vector<opus_int16> samples;
                        if (captureAudioFrame(samples)) {
                                // Check VAD
                                if (checkVAD(samples)) {
                                        // Encode
                                        std::vector<u8> encoded = encodeFrame(samples);
                                        if (!encoded.empty()) {
                                                // Encrypt if E2EE
                                                std::vector<u8> nonce;
                                                bool encrypted = false;

                                                if (m_e2ee_enabled) {
                                                        // Encrypt per-peer (but for global channel, we need
                                                        // to send the same encrypted data to all peers)
                                                        // For global channel, we use a single session key derived
                                                        // from all peers' shared secrets combined via HKDF
                                                        encrypted = encryptVoiceData(encoded, 0); // 0 = use global key
                                                        if (encrypted && encoded.size() >= VOICE_NONCE_SIZE) {
                                                                // Extract nonce from end of encrypted data
                                                                nonce.assign(encoded.end() - VOICE_NONCE_SIZE, encoded.end());
                                                        }
                                                }

                                                // Send
                                                if (sendVoiceData) {
                                                        sendVoiceData(m_active_channel, m_seq_num,
                                                                encoded, encrypted, nonce);
                                                }
                                                m_seq_num++;
                                        }
                                }
                        }
                }
        }

        // Process playback queue
        processPlaybackQueue();
}

// ============================================================
// Client Settings
// ============================================================

void VoiceChatManager::setServerVoiceAllowed(bool allowed)
{
        if (m_server_voice_allowed == allowed)
                return;

        m_server_voice_allowed = allowed;

        if (allowed && !m_receive_opt_out) {
                init();
                // Send our public key for E2EE
                if (m_e2ee_enabled && sendVoiceKeyExchange) {
                        sendVoiceKeyExchange(m_local_pubkey.data());
                }
        } else {
                // Server disallows voice - stop transmitting
                if (m_is_transmitting && sendVoiceStop) {
                        sendVoiceStop(m_active_channel);
                        m_is_transmitting = false;
                }
        }

        infostream << "VoiceChat: Server voice " << (allowed ? "allowed" : "disallowed") << std::endl;
}

void VoiceChatManager::setReceiveOptOut(bool opt_out)
{
        if (m_receive_opt_out == opt_out)
                return;

        m_receive_opt_out = opt_out;

        if (opt_out) {
                // Client opts out - stop transmitting and receiving
                if (m_is_transmitting && sendVoiceStop) {
                        sendVoiceStop(m_active_channel);
                        m_is_transmitting = false;
                }
        } else if (m_server_voice_allowed) {
                // Client opts back in and server allows - initialize
                init();
                if (m_e2ee_enabled && sendVoiceKeyExchange) {
                        sendVoiceKeyExchange(m_local_pubkey.data());
                }
        }

        if (sendVoiceOptOut)
                sendVoiceOptOut(opt_out);

        infostream << "VoiceChat: Client " << (opt_out ? "opted out" : "opted in") << " of voice" << std::endl;
}

void VoiceChatManager::setMode(VoiceChatMode mode)
{
        m_mode = mode;
        if (m_mode == VoiceChatMode::TOGGLE) {
                g_settings->set("voice_chat_mode", "toggle");
        } else {
                g_settings->set("voice_chat_mode", "ptt");
        }
}

void VoiceChatManager::setVolume(float vol)
{
        m_volume = std::clamp(vol, 0.0f, 1.0f);
        g_settings->setFloat("voice_chat_volume", m_volume);
}

void VoiceChatManager::setE2EE(bool enabled)
{
        m_e2ee_enabled = enabled;
        g_settings->setBool("voice_chat_e2ee", enabled);

        if (enabled && isVoiceActive() && sendVoiceKeyExchange) {
                // Regenerate keypair and send new public key
                generateEphemeralKeypair();
                sendVoiceKeyExchange(m_local_pubkey.data());
        }

        infostream << "VoiceChat: E2EE " << (enabled ? "enabled" : "disabled") << std::endl;
}

void VoiceChatManager::setNoiseSuppression(bool enabled)
{
        m_noise_suppression = enabled;
        if (m_encoder) {
                opus_encoder_ctl(m_encoder, OPUS_SET_DTX(enabled ? 0 : 1));
        }
}

void VoiceChatManager::setBitrate(int bitrate_bps)
{
        m_bitrate = bitrate_bps;
        if (m_encoder) {
                opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(m_bitrate));
        }
}

// ============================================================
// Push-to-Talk
// ============================================================

void VoiceChatManager::setPTTActive(bool active)
{
        m_ptt_active = active;
}

// ============================================================
// Audio I/O
// ============================================================

bool VoiceChatManager::captureAudioFrame(std::vector<opus_int16> &samples)
{
        // In a full implementation, this would capture from the microphone
        // using OpenAL capture or a platform-specific audio API.
        // For now, we provide the framework and the actual capture will
        // be integrated with the existing OpenAL sound system.
        samples.resize(VOICE_FRAME_SAMPLES, 0);
        return false; // No audio captured (stub — will be implemented with OpenAL capture)
}

void VoiceChatManager::queueReceivedAudio(u16 peer_id, u8 channel_id,
        const std::vector<u8> &opus_data, u16 seq_num)
{
        if (m_peers.count(peer_id) && m_peers[peer_id].is_muted)
                return; // Drop audio from muted peers

        std::lock_guard<std::mutex> lock(m_playback_mutex);
        m_playback_queue.push_back({peer_id, channel_id, opus_data, seq_num});
}

// ============================================================
// Opus Codec
// ============================================================

void VoiceChatManager::initOpus()
{
        int err;

        // Encoder
        m_encoder = opus_encoder_create(VOICE_SAMPLE_RATE, VOICE_CHANNELS,
                OPUS_APPLICATION_VOIP, &err);
        if (err != OPUS_OK) {
                errorstream << "VoiceChat: Failed to create Opus encoder: "
                            << opus_strerror(err) << std::endl;
                m_encoder = nullptr;
                return;
        }

        // Configure encoder
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(m_bitrate));
        opus_encoder_ctl(m_encoder, OPUS_SET_VBR(1));            // Variable bitrate
        opus_encoder_ctl(m_encoder, OPUS_SET_VBR_CONSTRAINT(0));  // Unconstrained VBR
        opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));       // Medium complexity
        opus_encoder_ctl(m_encoder, OPUS_SET_DTX(1));              // Discontinuous transmission
        opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(10)); // Expect 10% loss
        opus_encoder_ctl(m_encoder, OPUS_SET_LSB_DEPTH(16));

        // Default decoder
        m_decoder = opus_decoder_create(VOICE_SAMPLE_RATE, VOICE_CHANNELS, &err);
        if (err != OPUS_OK) {
                errorstream << "VoiceChat: Failed to create Opus decoder: "
                            << opus_strerror(err) << std::endl;
                m_decoder = nullptr;
                return;
        }

        infostream << "VoiceChat: Opus codec initialized" << std::endl;
}

void VoiceChatManager::deinitOpus()
{
        if (m_encoder) {
                opus_encoder_destroy(m_encoder);
                m_encoder = nullptr;
        }
        if (m_decoder) {
                opus_decoder_destroy(m_decoder);
                m_decoder = nullptr;
        }
        for (auto &pair : m_peer_decoders) {
                opus_decoder_destroy(pair.second);
        }
        m_peer_decoders.clear();
}

std::vector<u8> VoiceChatManager::encodeFrame(const std::vector<opus_int16> &samples)
{
        if (!m_encoder || samples.empty())
                return {};

        std::vector<u8> output(VOICE_MAX_OPUS_FRAME);
        int len = opus_encode(m_encoder, samples.data(),
                VOICE_FRAME_SAMPLES, output.data(), output.size());

        if (len < 0) {
                errorstream << "VoiceChat: Opus encode failed: " << opus_strerror(len) << std::endl;
                return {};
        }

        output.resize(len);
        return output;
}

std::vector<opus_int16> VoiceChatManager::decodeFrame(u16 peer_id, const std::vector<u8> &opus_data)
{
        // Get or create per-peer decoder
        OpusDecoder *decoder = nullptr;
        auto it = m_peer_decoders.find(peer_id);
        if (it != m_peer_decoders.end()) {
                decoder = it->second;
        } else {
                int err;
                decoder = opus_decoder_create(VOICE_SAMPLE_RATE, VOICE_CHANNELS, &err);
                if (err != OPUS_OK) {
                        errorstream << "VoiceChat: Failed to create decoder for peer "
                                    << peer_id << ": " << opus_strerror(err) << std::endl;
                        decoder = m_decoder; // Fallback to default
                } else {
                        m_peer_decoders[peer_id] = decoder;
                }
        }

        if (!decoder)
                return {};

        std::vector<opus_int16> output(VOICE_FRAME_SAMPLES);
        int len = opus_decode(decoder, opus_data.data(), opus_data.size(),
                output.data(), VOICE_FRAME_SAMPLES, 0 /* decode FEC */);

        if (len < 0) {
                errorstream << "VoiceChat: Opus decode failed for peer " << peer_id
                            << ": " << opus_strerror(len) << std::endl;
                return {};
        }

        output.resize(len * VOICE_CHANNELS);

        // Apply volume
        if (m_volume < 1.0f) {
                for (auto &sample : output) {
                        sample = (opus_int16)(sample * m_volume);
                }
        }

        return output;
}

// ============================================================
// E2EE (End-to-End Encryption)
// ============================================================

void VoiceChatManager::generateEphemeralKeypair()
{
#if USE_OPENSSL
        // Generate ephemeral X25519 keypair for voice E2EE
        EVP_PKEY *pkey = nullptr;
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
        if (!ctx) {
                errorstream << "VoiceChat: Failed to create X25519 context" << std::endl;
                return;
        }

        if (EVP_PKEY_keygen_init(ctx) <= 0) {
                errorstream << "VoiceChat: Failed to init X25519 keygen" << std::endl;
                EVP_PKEY_CTX_free(ctx);
                return;
        }

        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
                errorstream << "VoiceChat: Failed to generate X25519 keypair" << std::endl;
                EVP_PKEY_CTX_free(ctx);
                return;
        }
        EVP_PKEY_CTX_free(ctx);

        // Extract raw public key (X25519 uses the same raw API as Ed25519)
        size_t pub_len = 0;
        if (EVP_PKEY_get_raw_public_key(pkey, nullptr, &pub_len) != 1 || pub_len != 32) {
                errorstream << "VoiceChat: Failed to query X25519 public key size" << std::endl;
                EVP_PKEY_free(pkey);
                return;
        }
        if (EVP_PKEY_get_raw_public_key(pkey, m_local_pubkey.data(), &pub_len) != 1) {
                errorstream << "VoiceChat: Failed to extract X25519 public key" << std::endl;
                EVP_PKEY_free(pkey);
                return;
        }

        // Extract raw private key
        size_t priv_len = 0;
        if (EVP_PKEY_get_raw_private_key(pkey, nullptr, &priv_len) != 1 || priv_len != 32) {
                errorstream << "VoiceChat: Failed to query X25519 private key size" << std::endl;
                EVP_PKEY_free(pkey);
                return;
        }
        if (EVP_PKEY_get_raw_private_key(pkey, m_local_privkey.data(), &priv_len) != 1) {
                errorstream << "VoiceChat: Failed to extract X25519 private key" << std::endl;
                EVP_PKEY_free(pkey);
                return;
        }

        EVP_PKEY_free(pkey);

        infostream << "VoiceChat: Generated ephemeral X25519 keypair for voice E2EE" << std::endl;
#else
        warningstream << "VoiceChat: E2EE requires OpenSSL — generating dummy keypair" << std::endl;
        // Fill with random bytes as fallback (NOT secure)
        for (int i = 0; i < 32; i++) {
                m_local_privkey[i] = myrand() & 0xFF;
                m_local_pubkey[i] = myrand() & 0xFF;
        }
#endif
}

void VoiceChatManager::handlePeerKeyExchange(u16 peer_id, const u8 pubkey[32])
{
#if USE_OPENSSL
        auto &peer = m_peers[peer_id];
        peer.peer_id = peer_id;
        peer.peer_pubkey.assign(pubkey, pubkey + 32);

        // Derive session key using X25519 ECDH
        if (deriveSessionKey(peer_id)) {
                peer.e2ee_active = true;
                infostream << "VoiceChat: E2EE session established with peer " << peer_id << std::endl;
        } else {
                errorstream << "VoiceChat: Failed to derive E2EE session key with peer " << peer_id << std::endl;
        }
#else
        infostream << "VoiceChat: Received key from peer " << peer_id
                   << " but OpenSSL not available — E2EE disabled" << std::endl;
#endif
}

bool VoiceChatManager::deriveSessionKey(u16 peer_id)
{
#if USE_OPENSSL
        auto it = m_peers.find(peer_id);
        if (it == m_peers.end())
                return false;

        auto &peer = it->second;
        if (peer.peer_pubkey.size() != 32 || m_local_privkey.size() != 32)
                return false;

        // Use the shared crypto module's X25519 implementation for consistency
        // with the transport-layer ECDH. This avoids code duplication and ensures
        // both layers use the same well-tested key exchange.
        auto shared = x25519_compute_shared_secret(
                m_local_privkey.data(), m_local_privkey.size(),
                peer.peer_pubkey.data(), peer.peer_pubkey.size());
        if (!shared.success) {
                errorstream << "VoiceChat: X25519 shared secret derivation failed for peer "
                            << peer_id << ": " << shared.error_msg << std::endl;
                return false;
        }

        // Derive AES-256 key using HKDF-SHA256 (using the shared hkdf_sha256 function
        // for consistency with transport-layer key derivation)
        peer.session_key.resize(32);
        const u8 info[] = "LuantisVoiceE2EEv1";

        if (!hkdf_sha256(
                shared.shared_secret.data(), shared.shared_secret.size(),
                nullptr, 0,  // no salt (matches original behavior)
                info, strlen(reinterpret_cast<const char*>(info)),
                peer.session_key.data(), peer.session_key.size())) {
                errorstream << "VoiceChat: HKDF key derivation failed for peer " << peer_id << std::endl;
                return false;
        }

        // Reset counters
        peer.send_counter = 0;
        peer.recv_counter = 0;

        // Derive a per-peer nonce base for deterministic nonce construction
        // (matches the transport-layer pattern: 4-byte base + 8-byte counter)
        peer.nonce_base.resize(NONCE_BASE_SIZE);
        const u8 nonce_info[] = "LuantisVoiceE2EEv1Nonce";
        if (!hkdf_sha256(
                shared.shared_secret.data(), shared.shared_secret.size(),
                nullptr, 0,
                nonce_info, strlen(reinterpret_cast<const char*>(nonce_info)),
                peer.nonce_base.data(), peer.nonce_base.size())) {
                errorstream << "VoiceChat: HKDF nonce base derivation failed for peer " << peer_id << std::endl;
                return false;
        }

        infostream << "VoiceChat: Session key derived for peer " << peer_id << std::endl;
        return true;
#else
        return false;
#endif
}

bool VoiceChatManager::encryptVoiceData(std::vector<u8> &data, u16 peer_id)
{
#if USE_OPENSSL
        // For global channel (peer_id=0), derive a combined key from all
        // active peer session keys using HKDF (NOT XOR — XOR is insecure:
        // if two peers have the same key, XOR produces zero; and it leaks
        // relationships between keys).
        // For per-peer, use the peer's session key directly.

        std::vector<u8> key;
        std::vector<u8> nonce_base;

        if (peer_id == 0) {
                // Global channel: combine all peer session keys via HKDF
                if (m_peers.empty())
                        return false;

                // Concatenate all active peer session keys as HKDF input
                std::vector<u8> combined_ikm;
                for (auto &pair : m_peers) {
                        if (pair.second.e2ee_active && pair.second.session_key.size() == 32) {
                                combined_ikm.insert(combined_ikm.end(),
                                        pair.second.session_key.begin(),
                                        pair.second.session_key.end());
                        }
                }
                if (combined_ikm.empty())
                        return false;

                // Derive a combined key via HKDF
                key.resize(32);
                const u8 key_info[] = "LuantisVoiceGlobalE2EEv1";
                if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                        nullptr, 0,
                        key_info, strlen(reinterpret_cast<const char*>(key_info)),
                        key.data(), key.size())) {
                        return false;
                }

                // Derive a combined nonce base via HKDF
                nonce_base.resize(NONCE_BASE_SIZE);
                const u8 nonce_info[] = "LuantisVoiceGlobalE2EEv1Nonce";
                if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                        nullptr, 0,
                        nonce_info, strlen(reinterpret_cast<const char*>(nonce_info)),
                        nonce_base.data(), nonce_base.size())) {
                        return false;
                }
        } else {
                auto it = m_peers.find(peer_id);
                if (it == m_peers.end() || !it->second.e2ee_active)
                        return false;
                key = it->second.session_key;
                nonce_base = it->second.nonce_base;
        }

        if (key.size() != 32)
                return false;

        // Build deterministic nonce: 4-byte HKDF-derived base + 8-byte counter
        // This matches the transport-layer nonce construction pattern and guarantees
        // nonce uniqueness without relying on random bytes (which could collide).
        u64 counter = 0;
        if (peer_id == 0) {
                // Global channel: use a global send counter
                counter = m_global_send_counter++;
        } else {
                auto it = m_peers.find(peer_id);
                if (it != m_peers.end()) {
                        counter = it->second.send_counter++;
                }
        }

        std::vector<u8> nonce(VOICE_NONCE_SIZE, 0);
        build_nonce(nonce_base.data(), counter, nonce.data());

        // Build AAD: counter (8 bytes)
        // The peer_id is NOT included in AAD because encrypt uses the recipient's
        // peer_id while decrypt uses the sender's peer_id — they would never match.
        // Instead, the session key itself is already per-peer, and the counter
        // in the nonce provides replay protection. The AAD binds the counter
        // to the ciphertext to prevent counter reuse across different contexts.
        u8 aad[8];
        for (int i = 0; i < 8; i++) {
                aad[i] = (counter >> (56 - i * 8)) & 0xFF;
        }

        // Use the shared crypto module's AES-256-GCM for consistency with transport layer
        auto result = aes256gcm_encrypt(
                key.data(), key.size(),
                nonce.data(), nonce.size(),
                data.data(), data.size(),
                aad, sizeof(aad));

        if (!result.success) {
                errorstream << "VoiceChat: E2EE encryption failed for peer " << peer_id
                            << ": " << result.error_msg << std::endl;
                return false;
        }

        // Build output: ciphertext + tag + nonce (so the receiver can extract the nonce)
        data.resize(result.data.size() + VOICE_TAG_SIZE + VOICE_NONCE_SIZE);
        memcpy(data.data(), result.data.data(), result.data.size());
        memcpy(data.data() + result.data.size(), result.tag.data(), VOICE_TAG_SIZE);
        memcpy(data.data() + result.data.size() + VOICE_TAG_SIZE, nonce.data(), VOICE_NONCE_SIZE);

        return true;
#else
        return false;
#endif
}

bool VoiceChatManager::decryptVoiceData(std::vector<u8> &data, u16 peer_id, const std::vector<u8> &nonce)
{
#if USE_OPENSSL
        auto it = m_peers.find(peer_id);
        if (it == m_peers.end() || !it->second.e2ee_active || it->second.session_key.size() != 32)
                return false;

        // Wire format: [ciphertext][tag(16)][nonce(12)] — nonce is also passed separately
        // Minimum size: tag + nonce = 28 bytes (but nonce is passed separately,
        // so data has at minimum: ciphertext + tag = 16+ bytes)
        if (data.size() < VOICE_TAG_SIZE)
                return false;

        // Split: data = [ciphertext][tag]
        size_t ciphertext_len = data.size() - VOICE_TAG_SIZE;
        const u8 *ciphertext = data.data();
        const u8 *tag = data.data() + ciphertext_len;

        // Extract counter from nonce for AAD verification and replay protection
        // Nonce format: [4-byte base][8-byte counter big-endian]
        u64 counter = 0;
        if (nonce.size() >= VOICE_NONCE_SIZE) {
                for (int i = 0; i < 8; i++) {
                        counter = (counter << 8) | nonce[4 + i];
                }
        }

        // Replay protection: check if this counter has already been seen
        if (!it->second.isNotReplay(counter)) {
                warningstream << "VoiceChat: E2EE replay detected for peer " << peer_id
                              << " counter=" << counter << std::endl;
                return false;
        }

        // Build AAD: counter (8 bytes) — must match encrypt-side AAD
        u8 aad[8];
        for (int i = 0; i < 8; i++) {
                aad[i] = (counter >> (56 - i * 8)) & 0xFF;
        }

        // Use the shared crypto module's AES-256-GCM for consistency
        auto result = aes256gcm_decrypt(
                it->second.session_key.data(), it->second.session_key.size(),
                nonce.data(), nonce.size(),
                ciphertext, ciphertext_len,
                tag, VOICE_TAG_SIZE,
                aad, sizeof(aad));

        if (!result.success) {
                warningstream << "VoiceChat: E2EE decryption FAILED for peer " << peer_id
                              << ": " << result.error_msg << std::endl;
                return false;
        }

        // Mark the counter as received (for replay protection)
        it->second.markAndAdvance(counter);

        // Replace data with plaintext only (strip tag and nonce)
        data = std::move(result.data);
        return true;
#else
        return false;
#endif
}

// ============================================================
// Peer Management
// ============================================================

VoicePeerState* VoiceChatManager::getPeer(u16 peer_id)
{
        auto it = m_peers.find(peer_id);
        return it != m_peers.end() ? &it->second : nullptr;
}

void VoiceChatManager::updatePeerList(const std::vector<VoicePeerState> &peers)
{
        // BUG FIX: Previously, m_peers.clear() destroyed all E2EE session state
        // (session keys, nonce bases, replay bitmaps) whenever the peer list was
        // updated. Now we MERGE the new list with existing state, preserving
        // E2EE session keys for peers that remain connected.
        std::unordered_map<u16, VoicePeerState> new_peers;

        for (const auto &peer : peers) {
                auto existing = m_peers.find(peer.peer_id);
                if (existing != m_peers.end()) {
                        // Peer already exists — preserve E2EE state, update metadata
                        VoicePeerState merged = existing->second;
                        merged.name = peer.name;
                        merged.voice_enabled = peer.voice_enabled;
                        merged.is_talking = peer.is_talking;
                        // Keep: e2ee_active, session_key, peer_pubkey, nonce_base,
                        // send_counter, recv_counter, replay state
                        new_peers[peer.peer_id] = merged;
                } else {
                        // New peer — add as-is (E2EE will be established later
                        // when we receive their public key via VOICE_KEY_EXCHANGE)
                        new_peers[peer.peer_id] = peer;
                }
        }

        m_peers = std::move(new_peers);
        if (m_on_peer_list_update)
                m_on_peer_list_update();
}

void VoiceChatManager::setPeerMuted(u16 peer_id, bool muted)
{
        auto it = m_peers.find(peer_id);
        if (it != m_peers.end()) {
                it->second.is_muted = muted;
                if (sendVoiceMute)
                        sendVoiceMute(peer_id, muted);
        }
}

bool VoiceChatManager::isPeerMuted(u16 peer_id) const
{
        auto it = m_peers.find(peer_id);
        return it != m_peers.end() ? it->second.is_muted : false;
}

void VoiceChatManager::setPeerTalking(u16 peer_id, bool talking)
{
        auto it = m_peers.find(peer_id);
        if (it != m_peers.end() && it->second.is_talking != talking) {
                it->second.is_talking = talking;
                if (m_on_peer_talking)
                        m_on_peer_talking(peer_id, talking);
        }
}

// ============================================================
// Voice Groups
// ============================================================

u32 VoiceChatManager::createGroup(const std::string &name)
{
        if (sendVoiceGroupCreate) {
                sendVoiceGroupCreate(name);
        }
        // Server will assign the actual group_id and send it back
        // via VOICE_GROUP_UPDATE. Return 0 for now.
        return 0;
}

void VoiceChatManager::inviteToGroup(u32 group_id, u16 peer_id)
{
        if (sendVoiceGroupInvite)
                sendVoiceGroupInvite(group_id, peer_id);
}

void VoiceChatManager::joinGroup(u32 group_id)
{
        if (sendVoiceGroupJoin)
                sendVoiceGroupJoin(group_id);
}

void VoiceChatManager::leaveGroup(u32 group_id)
{
        if (sendVoiceGroupLeave)
                sendVoiceGroupLeave(group_id);

        // Server will send VOICE_GROUP_UPDATE which handles the actual removal
        // from the local state via handleGroupUpdate()
}

VoiceGroup* VoiceChatManager::getGroup(u32 group_id)
{
        auto it = m_groups.find(group_id);
        return it != m_groups.end() ? &it->second : nullptr;
}

void VoiceChatManager::handleGroupUpdate(u32 group_id, u8 update_type,
        const std::vector<std::pair<u16, std::string>> &members)
{
        if (update_type == 3) { // Disbanded
                m_groups.erase(group_id);
        } else {
                auto &group = m_groups[group_id];
                group.group_id = group_id;
                group.active = (update_type != 3);
                group.members.clear();
                for (const auto &m : members) {
                        group.members.push_back(m.first);
                }
        }

        if (m_on_group_update)
                m_on_group_update(group_id, update_type);
}

// ============================================================
// Voice Activity Detection
// ============================================================

bool VoiceChatManager::checkVAD(const std::vector<opus_int16> &samples)
{
        if (samples.empty())
                return false;

        // Compute RMS energy
        double sum_sq = 0.0;
        for (opus_int16 sample : samples) {
                double s = sample / 32768.0;
                sum_sq += s * s;
        }
        double rms = std::sqrt(sum_sq / samples.size());

        return rms > m_vad_threshold;
}

// ============================================================
// Audio Capture/Playback (stubs — will be implemented with OpenAL)
// ============================================================

void VoiceChatManager::initAudioCapture()
{
        // TODO: Initialize OpenAL capture device
        // ALCdevice *device = alcCaptureOpenDevice(input_device_name,
        //     VOICE_SAMPLE_RATE, AL_FORMAT_MONO16, VOICE_FRAME_SAMPLES * 4);
}

void VoiceChatManager::deinitAudioCapture()
{
        // TODO: Close OpenAL capture device
}

void VoiceChatManager::initAudioPlayback()
{
        // TODO: Initialize OpenAL playback sources for each peer
}

void VoiceChatManager::deinitAudioPlayback()
{
        // TODO: Clean up OpenAL playback sources
}

void VoiceChatManager::processPlaybackQueue()
{
        std::lock_guard<std::mutex> lock(m_playback_mutex);
        for (auto &frame : m_playback_queue) {
                // Decode
                std::vector<opus_int16> pcm = decodeFrame(frame.peer_id, frame.opus_data);
                if (pcm.empty())
                        continue;

                // TODO: Queue PCM data to OpenAL source for this peer
                // In a full implementation, each peer gets an OpenAL source
                // and we buffer the decoded PCM data for playback.
        }
        m_playback_queue.clear();
}

#endif // USE_VOICE_CHAT
