// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include <IGUIEnvironment.h>
#include <memory>
#include "game.h"
#include "gui/statusTextHelper.h"
#include "network/connection_security.h"


class Client;
class EnrichedString;
class GUIChatConsole;
struct MapDrawControl;
struct PointedThing;

/*
 * This object intend to contain the core UI elements
 * It includes:
 *   - status texts
 *   - debug texts
 *   - chat texts
 *   - hud flags
 */
class GameUI
{
        // Temporary between coding time to move things here
        friend class Game;

        // Permit unittests to access members directly
        friend class TestGameUI;

public:
        GameUI() = default;
        ~GameUI() = default;

        // Flags that can, or may, change during main game loop
        struct Flags
        {
                bool show_chat = true;
                bool show_hud = true;
                bool show_minimal_debug = false;
                bool show_basic_debug = false;
                bool show_profiler_graph = false;
                // Overlay flags — controlled by User Interface settings
                // These follow the extensible overlay pattern: each overlay has a
                // corresponding bool setting (show_*_overlay / show_*_info) that
                // appears as a checkbox in Settings → User Interfaces → HUD.
                // To add a new overlay: (1) add a flag here, (2) add a setting in
                // defaultsettings.cpp, (3) add it to settingtypes.txt under [**HUD],
                // (4) add it to minetest.conf.example, (5) check the flag in update().

                // v9: Original security overlay
                bool show_security_overlay = true;    // from show_security_overlay setting
                bool show_connection_info = false;    // from show_connection_info setting

                // v9: New encryption detail overlays (toggleable in settings, update every second)
                bool show_enc_session_overlay = false;    // Session ID, fingerprint, uptime
                bool show_enc_packets_overlay = false;    // C2S/S2C packet counts + rate
                bool show_enc_cipher_overlay = false;     // Full cipher suite details
                bool show_enc_score_overlay = false;      // Visual security score bar
                bool show_enc_authfail_overlay = false;   // Auth failures + replay attempts
                bool show_enc_nonce_overlay = false;      // Nonce counters + space usage
                bool show_enc_latency_overlay = false;    // RTT, packet loss, overhead
                bool show_enc_timeline_overlay = false;   // Connection event timeline
                bool show_enc_pfs_overlay = false;        // Forward secrecy (ECDH X25519)
                bool show_enc_trust_overlay = false;       // Fingerprint pinning/trust
                bool show_enc_health_overlay = false;      // Security health summary
                bool show_enc_bandwidth_overlay = false;   // Encryption bandwidth impact
        };

        void init();
        void update(const RunStats &stats, Client *client, MapDrawControl *draw_control,
                        const CameraOrientation &cam, const PointedThing &pointed_old,
                        const GUIChatConsole *chat_console, float dtime);

        void initFlags();
        const Flags &getFlags() const { return m_flags; }

        inline void setInfoText(const std::wstring &str) { m_infotext = str; }
        inline void clearInfoText() { m_infotext.clear(); }

        inline void showStatusText(const std::wstring &str)
        {
                if (m_status_text)
                        m_status_text->showStatusText(str);
        }
        void showTranslatedStatusText(const char *str);
        inline void clearStatusText()
        {
                if (m_status_text)
                        m_status_text->clearStatusText();
        }

        bool isChatVisible()
        {
                return m_flags.show_chat && m_recent_chat_count != 0 && m_profiler_current_page == 0;
        }
        void setChatText(const EnrichedString &chat_text, u32 recent_chat_count);
        void updateChatSize();

        void updateProfiler();

        void toggleChat(Client *client);
        void toggleHud();
        void toggleProfiler();

        void clearText();

        // Security overlay — shows a persistent warning when the connection is not encrypted
        void setConnectionSecurity(ConnectionSecurity sec);
        void setConnectionSecurityInfo(const ConnectionSecurityInfo& info);
        ConnectionSecurity getConnectionSecurity() const { return m_security_info.state; }
        const ConnectionSecurityInfo& getConnectionSecurityInfo() const { return m_security_info; }
        bool shouldShowSecurityOverlay() const;
        bool shouldShowSecureIndicator() const;
        void resetSecurityInfo();
        void setSingleplayerMode(bool singleplayer) { m_singleplayer_mode = singleplayer; }

private:
        // v9: Render the security overlay based on the current mode setting
        void drawSecurityOverlay(const v2u32 &screensize);

        // v9: Render encryption detail overlays (each toggleable in settings)
        void drawEncryptionSessionOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionPacketsOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionCipherOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionScoreOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionAuthFailOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionNonceOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionLatencyOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionTimelineOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionPFSOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionTrustOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionHealthOverlay(const v2u32 &screensize, s32 &y_offset);
        void drawEncryptionBandwidthOverlay(const v2u32 &screensize, s32 &y_offset);

        // v9: Helper to create a positioned overlay text element
        gui::IGUIStaticText* createOverlayText(gui::IGUIStaticText* &element,
                const v2u32 &screensize, s32 x, s32 y,
                const video::SColor &text_color, const video::SColor &bg_color);

        // v9: Update live encryption stats from PeerEncryptionState
        void syncEncryptionLiveStats(Client *client);

private:
        Flags m_flags;

        float m_drawtime_avg = 0;

        gui::IGUIStaticText *m_guitext = nullptr;  // First line of debug text
        gui::IGUIStaticText *m_guitext2 = nullptr; // Second line of debug text

        gui::IGUIStaticText *m_guitext_info = nullptr; // At the middle of the screen
        std::wstring m_infotext;

        std::unique_ptr<StatusTextHelper> m_status_text = nullptr;

        gui::IGUIStaticText *m_guitext_chat = nullptr; // Chat text
        u32 m_recent_chat_count = 0;
        core::rect<s32> m_current_chat_size{0, 0, 0, 0};

        gui::IGUIStaticText *m_guitext_profiler = nullptr; // Profiler text
        u8 m_profiler_current_page = 0;
        u8 m_profiler_max_page = 1;

        // Security overlay — persistent warning banner for insecure connections
        gui::IGUIStaticText *m_guitext_security = nullptr;
        ConnectionSecurityInfo m_security_info;
        bool m_singleplayer_mode = false;

        // v9: Encryption detail overlay GUI elements (left side, below debug text)
        gui::IGUIStaticText *m_guitext_enc_session = nullptr;    // Session & fingerprint
        gui::IGUIStaticText *m_guitext_enc_packets = nullptr;    // Packet counters
        gui::IGUIStaticText *m_guitext_enc_cipher = nullptr;     // Cipher suite detail
        gui::IGUIStaticText *m_guitext_enc_score = nullptr;      // Security score bar
        gui::IGUIStaticText *m_guitext_enc_authfail = nullptr;   // Auth failures
        gui::IGUIStaticText *m_guitext_enc_nonce = nullptr;      // Nonce counters
        gui::IGUIStaticText *m_guitext_enc_latency = nullptr;    // Latency & overhead
        gui::IGUIStaticText *m_guitext_enc_timeline = nullptr;   // Connection timeline
        gui::IGUIStaticText *m_guitext_enc_pfs = nullptr;        // Forward secrecy
        gui::IGUIStaticText *m_guitext_enc_trust = nullptr;       // Fingerprint pinning
        gui::IGUIStaticText *m_guitext_enc_health = nullptr;      // Security health
        gui::IGUIStaticText *m_guitext_enc_bandwidth = nullptr;   // Bandwidth impact

        // v9: Packet rate tracking (for per-second rate display)
        u64 m_last_c2s_packets = 0;
        u64 m_last_s2c_packets = 0;
        float m_packet_rate_timer = 0.0f;
};
