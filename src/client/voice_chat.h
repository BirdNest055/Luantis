// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 BirdNest055

#pragma once

#include "irrlichttypes.h"
#include "config.h"

#if USE_VOICE_CHAT

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>

#include <opus/opus.h>

// Forward declarations for OpenSSL types
typedef struct evp_pkey_st EVP_PKEY;

// Voice chat constants
constexpr int VOICE_SAMPLE_RATE = 48000;          // 48kHz — Opus standard
constexpr int VOICE_CHANNELS = 1;                  // Mono
constexpr int VOICE_FRAME_MS = 20;                 // 20ms frames
constexpr int VOICE_FRAME_SAMPLES = VOICE_SAMPLE_RATE * VOICE_FRAME_MS / 1000; // 960
constexpr size_t VOICE_MAX_OPUS_FRAME = 4000;      // Max Opus frame size in bytes
constexpr size_t VOICE_NONCE_SIZE = 12;            // AES-GCM nonce
constexpr size_t VOICE_TAG_SIZE = 16;              // AES-GCM auth tag
constexpr u8 VOICE_CHANNEL_GLOBAL = 0;             // Global channel (heard by everyone)
constexpr u32 VOICE_INVALID_GROUP_ID = 0;          // Invalid group ID

/**
 * Per-peer voice state for the client.
 * Tracks whether a peer is talking, muted, and holds the E2EE session key.
 */
struct VoicePeerState {
        u16 peer_id = 0;
        std::string name;
        bool voice_enabled = false;
        bool is_talking = false;
        bool is_muted = false;

        // E2EE: per-peer X25519-derived session key
        bool e2ee_active = false;
        std::vector<u8> session_key;   // 32 bytes, AES-256 key
        std::vector<u8> peer_pubkey;   // 32 bytes, peer's X25519 public key
        u64 send_counter = 0;
        u64 recv_counter = 0;
};

/**
 * Voice group information.
 * Groups are persistent chat rooms with invited members.
 */
struct VoiceGroup {
        u32 group_id = VOICE_INVALID_GROUP_ID;
        std::string name;
        u16 owner_peer_id = 0;
        std::vector<u16> members;
        bool active = false;
};

/**
 * Voice chat mode for the local client.
 */
enum class VoiceChatMode : u8 {
        DISABLED = 0,   // Voice chat off (server-disallowed or client opted out)
        PTT = 1,        // Push-to-talk (hold key)
        TOGGLE = 2,     // Toggle on/off
};

/**
 * Callback types for voice events.
 * These allow the game loop and HUD to react to voice state changes.
 */
using VoiceTalkingCallback = std::function<void(u16 peer_id, bool talking)>;
using VoicePeerListCallback = std::function<void()>;
using VoiceGroupInviteCallback = std::function<void(u32 group_id, const std::string &name, u16 inviter_id, const std::string &inviter_name)>;
using VoiceGroupUpdateCallback = std::function<void(u32 group_id, u8 update_type)>;

/**
 * VoiceChatManager — Client-side voice chat subsystem.
 *
 * Architecture:
 *   Audio Capture → Opus Encode → [E2EE Encrypt] → Network Send
 *   Network Recv → [E2EE Decrypt] → Opus Decode → Audio Playback
 *
 * E2EE:
 *   Each client generates an ephemeral X25519 keypair when voice is enabled.
 *   Public keys are exchanged via the server (TOCLIENT/VOICE_KEY_EXCHANGE).
 *   The shared secret is derived via X25519 ECDH, then HKDF-SHA256 derives
 *   the AES-256-GCM session key. Voice packets are encrypted/decrypted per-peer.
 *
 * The server NEVER sees unencrypted voice data — it only relays Opus frames.
 * When E2EE is active, even the server cannot listen to voice conversations.
 */
class VoiceChatManager {
public:
        VoiceChatManager();
        ~VoiceChatManager();

        // === Lifecycle ===
        void init();                          // Initialize Opus codecs + audio
        void deinit();                        // Clean shutdown
        void step(float dtime);               // Called every frame from game loop

        // === Server Authority ===
        void setServerVoiceAllowed(bool allowed);
        bool isServerVoiceAllowed() const { return m_server_voice_allowed; }

        // === Client Opt-Out ===
        void setReceiveOptOut(bool opt_out);
        bool isReceiveOptOut() const { return m_receive_opt_out; }
        bool isVoiceActive() const { return m_server_voice_allowed && !m_receive_opt_out; }
        void setMode(VoiceChatMode mode);
        VoiceChatMode getMode() const { return m_mode; }
        void setVolume(float vol);
        float getVolume() const { return m_volume; }
        void setE2EE(bool enabled);
        bool isE2EEActive() const { return m_e2ee_enabled; }
        void setNoiseSuppression(bool enabled);
        void setBitrate(int bitrate_bps);

        // === Push-to-Talk ===
        void setPTTActive(bool active);       // Called when PTT key is pressed/released
        bool isPTTActive() const { return m_ptt_active; }

        // === Transmission Control ===
        bool isTransmitting() const { return m_is_transmitting; }
        u8 getActiveChannel() const { return m_active_channel; }
        void setActiveChannel(u8 channel_id) { m_active_channel = channel_id; }

        // === Audio I/O ===
        bool captureAudioFrame(std::vector<opus_int16> &samples);  // Capture one frame
        void queueReceivedAudio(u16 peer_id, u8 channel_id,
                const std::vector<u8> &opus_data, u16 seq_num);

        // === Opus Codec ===
        std::vector<u8> encodeFrame(const std::vector<opus_int16> &samples);
        std::vector<opus_int16> decodeFrame(u16 peer_id, const std::vector<u8> &opus_data);

        // === E2EE ===
        bool encryptVoiceData(std::vector<u8> &data, u16 peer_id);
        bool decryptVoiceData(std::vector<u8> &data, u16 peer_id, const std::vector<u8> &nonce);
        void handlePeerKeyExchange(u16 peer_id, const u8 pubkey[32]);
        const u8* getLocalPublicKey() const { return m_local_pubkey.data(); }

        // === Peer Management ===
        VoicePeerState* getPeer(u16 peer_id);
        void updatePeerList(const std::vector<VoicePeerState> &peers);
        void setPeerMuted(u16 peer_id, bool muted);
        bool isPeerMuted(u16 peer_id) const;
        void setPeerTalking(u16 peer_id, bool talking);
        const std::unordered_map<u16, VoicePeerState>& getPeers() const { return m_peers; }

        // === Voice Groups ===
        u32 createGroup(const std::string &name);
        void inviteToGroup(u32 group_id, u16 peer_id);
        void joinGroup(u32 group_id);
        void leaveGroup(u32 group_id);
        VoiceGroup* getGroup(u32 group_id);
        const std::unordered_map<u32, VoiceGroup>& getGroups() const { return m_groups; }
        void handleGroupUpdate(u32 group_id, u8 update_type,
                const std::vector<std::pair<u16, std::string>> &members);

        // === Callbacks ===
        void setOnPeerTalking(VoiceTalkingCallback cb) { m_on_peer_talking = cb; }
        void setOnPeerListUpdate(VoicePeerListCallback cb) { m_on_peer_list_update = cb; }
        void setOnGroupInvite(VoiceGroupInviteCallback cb) { m_on_group_invite = cb; }
        void setOnGroupUpdate(VoiceGroupUpdateCallback cb) { m_on_group_update = cb; }

        // === Send Functions (called by client to send packets) ===
        // These are set by the Client class to wire up network sending
        std::function<void(u8 channel_id)> sendVoiceStart;
        std::function<void(u8 channel_id, u16 seq_num, const std::vector<u8> &opus_data,
                bool e2ee, const std::vector<u8> &nonce)> sendVoiceData;
        std::function<void(u8 channel_id)> sendVoiceStop;
        std::function<void(bool opt_out)> sendVoiceOptOut;
        std::function<void(u16 peer_id, bool muted)> sendVoiceMute;
        std::function<void(const std::string &name)> sendVoiceGroupCreate;
        std::function<void(u32 group_id, u16 peer_id)> sendVoiceGroupInvite;
        std::function<void(u32 group_id)> sendVoiceGroupJoin;
        std::function<void(u32 group_id)> sendVoiceGroupLeave;
        std::function<void(const u8 pubkey[32])> sendVoiceKeyExchange;

private:
        // Audio capture/playback
        void initAudioCapture();
        void deinitAudioCapture();
        void initAudioPlayback();
        void deinitAudioPlayback();

        // Internal Opus
        OpusEncoder *m_encoder = nullptr;
        OpusDecoder *m_decoder = nullptr;        // Default decoder (for peers without one)
        std::unordered_map<u16, OpusDecoder*> m_peer_decoders;  // Per-peer decoders
        void initOpus();
        void deinitOpus();

        // E2EE internals
        void generateEphemeralKeypair();
        bool deriveSessionKey(u16 peer_id);

        // State
        bool m_server_voice_allowed = false;  // Set by TOCLIENT_VOICE_STATE
        bool m_receive_opt_out = false;           // Client chooses to mute incoming voice
        bool m_initialized = false;
        VoiceChatMode m_mode = VoiceChatMode::PTT;
        float m_volume = 0.8f;
        bool m_e2ee_enabled = true;
        bool m_noise_suppression = true;
        int m_bitrate = 32000;
        bool m_ptt_active = false;
        bool m_is_transmitting = false;
        bool m_voice_toggle_on = false;    // For toggle mode
        u8 m_active_channel = VOICE_CHANNEL_GLOBAL;
        u16 m_seq_num = 0;

        // Audio buffer for capture
        std::vector<opus_int16> m_capture_buffer;

        // E2EE keys (local ephemeral X25519)
        std::vector<u8> m_local_privkey;  // 32 bytes
        std::vector<u8> m_local_pubkey;   // 32 bytes

        // Peer states
        std::unordered_map<u16, VoicePeerState> m_peers;

        // Voice groups
        std::unordered_map<u32, VoiceGroup> m_groups;
        u32 m_next_group_id = 1;

        // Playback queue per peer
        struct QueuedFrame {
                u16 peer_id;
                u8 channel_id;
                std::vector<u8> opus_data;
                u16 seq_num;
        };
        std::vector<QueuedFrame> m_playback_queue;
        std::mutex m_playback_mutex;
        void processPlaybackQueue();

        // VAD (Voice Activity Detection) energy threshold
        float m_vad_threshold = 0.3f;
        bool checkVAD(const std::vector<opus_int16> &samples);

        // Transmission timer
        float m_transmit_timer = 0.0f;
        float m_frame_interval = VOICE_FRAME_MS / 1000.0f;

        // Callbacks
        VoiceTalkingCallback m_on_peer_talking;
        VoicePeerListCallback m_on_peer_list_update;
        VoiceGroupInviteCallback m_on_group_invite;
        VoiceGroupUpdateCallback m_on_group_update;
};

#endif // USE_VOICE_CHAT
