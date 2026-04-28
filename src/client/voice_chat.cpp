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

#if USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
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
        if (!m_enabled || !m_initialized)
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

void VoiceChatManager::setEnabled(bool enabled)
{
        if (m_enabled == enabled)
                return;

        m_enabled = enabled;

        if (enabled) {
                init();
                // Send our public key for E2EE
                if (m_e2ee_enabled && sendVoiceKeyExchange) {
                        sendVoiceKeyExchange(m_local_pubkey.data());
                }
        } else {
                // Stop transmitting
                if (m_is_transmitting && sendVoiceStop) {
                        sendVoiceStop(m_active_channel);
                        m_is_transmitting = false;
                }
        }

        if (sendVoiceEnable)
                sendVoiceEnable(enabled);

        infostream << "VoiceChat: " << (enabled ? "Enabled" : "Disabled") << std::endl;
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

        if (enabled && m_enabled && sendVoiceKeyExchange) {
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

        // Extract raw public key
        size_t pub_len = 32;
        EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                m_local_pubkey.data(), pub_len, &pub_len);

        // Extract raw private key
        size_t priv_len = 32;
        EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                m_local_privkey.data(), priv_len, &priv_len);

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

        // Create EVP_PKEY from our private key
        EVP_PKEY *priv_pkey = nullptr;
        OSSL_PARAM params[] = {
                OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                        m_local_privkey.data(), 32),
                OSSL_PARAM_construct_end()
        };
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(nullptr, "X25519", nullptr);
        if (!ctx)
                return false;

        if (EVP_PKEY_fromdata_init(ctx) <= 0) {
                EVP_PKEY_CTX_free(ctx);
                return false;
        }

        if (EVP_PKEY_fromdata(ctx, &priv_pkey, EVP_PKEY_PRIVATE_KEY, params) <= 0) {
                EVP_PKEY_CTX_free(ctx);
                return false;
        }
        EVP_PKEY_CTX_free(ctx);

        // Create EVP_PKEY from peer's public key
        EVP_PKEY *pub_pkey = nullptr;
        OSSL_PARAM pub_params[] = {
                OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                        peer.peer_pubkey.data(), 32),
                OSSL_PARAM_construct_end()
        };
        ctx = EVP_PKEY_CTX_new_from_name(nullptr, "X25519", nullptr);
        if (!ctx) {
                EVP_PKEY_free(priv_pkey);
                return false;
        }

        if (EVP_PKEY_fromdata_init(ctx) <= 0) {
                EVP_PKEY_CTX_free(ctx);
                EVP_PKEY_free(priv_pkey);
                return false;
        }

        if (EVP_PKEY_fromdata(ctx, &pub_pkey, EVP_PKEY_PUBLIC_KEY, pub_params) <= 0) {
                EVP_PKEY_CTX_free(ctx);
                EVP_PKEY_free(priv_pkey);
                return false;
        }
        EVP_PKEY_CTX_free(ctx);

        // Derive shared secret
        EVP_PKEY_CTX *derive_ctx = EVP_PKEY_CTX_new(priv_pkey, nullptr);
        if (!derive_ctx) {
                EVP_PKEY_free(priv_pkey);
                EVP_PKEY_free(pub_pkey);
                return false;
        }

        if (EVP_PKEY_derive_init(derive_ctx) <= 0) {
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(priv_pkey);
                EVP_PKEY_free(pub_pkey);
                return false;
        }

        if (EVP_PKEY_derive_set_peer(derive_ctx, pub_pkey) <= 0) {
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(priv_pkey);
                EVP_PKEY_free(pub_pkey);
                return false;
        }

        size_t secret_len = 0;
        if (EVP_PKEY_derive(derive_ctx, nullptr, &secret_len) <= 0 || secret_len != 32) {
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(priv_pkey);
                EVP_PKEY_free(pub_pkey);
                return false;
        }

        std::vector<u8> shared_secret(32);
        if (EVP_PKEY_derive(derive_ctx, shared_secret.data(), &secret_len) <= 0) {
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(priv_pkey);
                EVP_PKEY_free(pub_pkey);
                return false;
        }

        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(priv_pkey);
        EVP_PKEY_free(pub_pkey);

        // Derive AES-256 key using HKDF-SHA256
        peer.session_key.resize(32);
        const char info[] = "LuantisVoiceE2EEv1";

        EVP_PKEY_CTX *kdf_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
        if (!kdf_ctx)
                return false;

        if (EVP_PKEY_derive_init(kdf_ctx) <= 0 ||
            EVP_PKEY_CTX_set_hkdf_md(kdf_ctx, EVP_sha256()) <= 0 ||
            EVP_PKEY_CTX_set1_hkdf_salt(kdf_ctx, nullptr, 0) <= 0 ||
            EVP_PKEY_CTX_set1_hkdf_key(kdf_ctx, shared_secret.data(), 32) <= 0 ||
            EVP_PKEY_CTX_add1_hkdf_info(kdf_ctx, (const u8*)info, strlen(info)) <= 0) {
                EVP_PKEY_CTX_free(kdf_ctx);
                return false;
        }

        size_t key_len = 32;
        if (EVP_PKEY_derive(kdf_ctx, peer.session_key.data(), &key_len) <= 0 || key_len != 32) {
                EVP_PKEY_CTX_free(kdf_ctx);
                return false;
        }

        EVP_PKEY_CTX_free(kdf_ctx);

        // Reset counters
        peer.send_counter = 0;
        peer.recv_counter = 0;

        return true;
#else
        return false;
#endif
}

bool VoiceChatManager::encryptVoiceData(std::vector<u8> &data, u16 peer_id)
{
#if USE_OPENSSL
        // For global channel (peer_id=0), we encrypt with a key derived from
        // all active peer keys combined. For group channels, we use the group key.
        // Simplification: for global channel, we encrypt once with a combined key.

        std::vector<u8> key;

        if (peer_id == 0) {
                // Global channel: derive a key from all peer session keys
                if (m_peers.empty()) {
                        // No peers — can't do E2EE yet
                        return false;
                }

                // XOR all peer session keys together as a simple combiner
                // (In production, you'd use a proper KDF with all keys as input)
                key.resize(32, 0);
                for (auto &pair : m_peers) {
                        if (pair.second.e2ee_active && pair.second.session_key.size() == 32) {
                                for (int i = 0; i < 32; i++) {
                                        key[i] ^= pair.second.session_key[i];
                                }
                        }
                }
        } else {
                auto it = m_peers.find(peer_id);
                if (it == m_peers.end() || !it->second.e2ee_active)
                        return false;
                key = it->second.session_key;
        }

        if (key.size() != 32)
                return false;

        // Generate nonce: 4-byte base + 8-byte counter
        u64 counter = 0;
        if (peer_id == 0) {
                // Use a global counter
                static u64 global_counter = 0;
                counter = global_counter++;
        } else {
                auto it = m_peers.find(peer_id);
                if (it != m_peers.end()) {
                        counter = it->second.send_counter++;
                }
        }

        std::vector<u8> nonce(VOICE_NONCE_SIZE, 0);
        // Nonce format: 4 random bytes + 8-byte big-endian counter
        RAND_bytes(nonce.data(), 4);
        for (int i = 0; i < 8; i++) {
                nonce[4 + i] = (counter >> (56 - i * 8)) & 0xFF;
        }

        // AES-256-GCM encrypt
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
                return false;

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, VOICE_NONCE_SIZE, nullptr) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        // Add AAD (peer_id + counter for authentication)
        u8 aad[10];
        aad[0] = (peer_id >> 8) & 0xFF;
        aad[1] = peer_id & 0xFF;
        for (int i = 0; i < 8; i++) {
                aad[2 + i] = (counter >> (56 - i * 8)) & 0xFF;
        }

        int aad_len;
        EVP_EncryptUpdate(ctx, nullptr, &aad_len, aad, sizeof(aad));

        // Encrypt in-place
        std::vector<u8> ciphertext(data.size());
        int out_len = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len, data.data(), data.size()) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        int final_len = 0;
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len, &final_len) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        // Get auth tag
        u8 tag[VOICE_TAG_SIZE];
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, VOICE_TAG_SIZE, tag) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }
        EVP_CIPHER_CTX_free(ctx);

        // Build output: ciphertext + tag + nonce
        data.resize(out_len + final_len + VOICE_TAG_SIZE + VOICE_NONCE_SIZE);
        memcpy(data.data(), ciphertext.data(), out_len + final_len);
        memcpy(data.data() + out_len + final_len, tag, VOICE_TAG_SIZE);
        memcpy(data.data() + out_len + final_len + VOICE_TAG_SIZE, nonce.data(), VOICE_NONCE_SIZE);

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

        if (data.size() < VOICE_TAG_SIZE + VOICE_NONCE_SIZE)
                return false;

        // Split: ciphertext + tag + nonce (nonce provided separately)
        size_t ciphertext_len = data.size() - VOICE_TAG_SIZE;
        const u8 *ciphertext = data.data();
        const u8 *tag = data.data() + ciphertext_len;

        // Decrypt
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
                return false;

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, VOICE_NONCE_SIZE, nullptr) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, it->second.session_key.data(), nonce.data()) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        // AAD
        u64 counter = it->second.recv_counter++;
        u8 aad[10];
        aad[0] = (peer_id >> 8) & 0xFF;
        aad[1] = peer_id & 0xFF;
        for (int i = 0; i < 8; i++) {
                aad[2 + i] = (counter >> (56 - i * 8)) & 0xFF;
        }

        int aad_len;
        EVP_DecryptUpdate(ctx, nullptr, &aad_len, aad, sizeof(aad));

        std::vector<u8> plaintext(ciphertext_len);
        int out_len = 0;
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, ciphertext, ciphertext_len) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        // Set expected tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, VOICE_TAG_SIZE, (void*)tag) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                return false;
        }

        int final_len = 0;
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) != 1) {
                // Authentication failed — data was tampered with
                EVP_CIPHER_CTX_free(ctx);
                warningstream << "VoiceChat: E2EE decryption FAILED for peer " << peer_id
                              << " — possible tampering or key mismatch" << std::endl;
                return false;
        }

        EVP_CIPHER_CTX_free(ctx);

        data.assign(plaintext.data(), plaintext.data() + out_len + final_len);
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
        m_peers.clear();
        for (const auto &peer : peers) {
                m_peers[peer.peer_id] = peer;
        }
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
