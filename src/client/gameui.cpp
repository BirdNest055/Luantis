// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "gameui.h"
#include <irrlicht_changes/static_text.h>
#include <gettext.h>
#include "gui/mainmenumanager.h"
#include "gui/guiChatConsole.h"
#include "gui/statusTextHelper.h"
#include "gui/touchcontrols.h"
#include "util/enriched_string.h"
#include "util/pointedthing.h"
#include "client.h"
#include "clientmap.h"
#include "fontengine.h"
#include "hud_element.h" // HUD_FLAG_*
#include "nodedef.h"
#include "localplayer.h"
#include "profiler.h"
#include "renderingengine.h"
#include "settings.h"
#include "version.h"
#include "network/crypto.h"
#include <IGUIFont.h>
#include <iomanip>
#include <ctime>

inline static const char *yawToDirectionString(int yaw)
{
        static const char *direction[4] =
                {"North +Z", "West -X", "South -Z", "East +X"};

        yaw = wrapDegrees_0_360(yaw);
        yaw = (yaw + 45) % 360 / 90;

        return direction[yaw];
}

void GameUI::init()
{
        // First line of debug text
        m_guitext = gui::StaticText::add(guienv, utf8_to_wide(PROJECT_NAME_C).c_str(),
                core::recti(), false, true, guiroot);

        // Second line of debug text
        m_guitext2 = gui::StaticText::add(guienv, L"", core::recti(), false,
                true, guiroot);

        // Chat text
        m_guitext_chat = gui::StaticText::add(guienv, L"", core::recti(),
                false, true, guiroot);
        u16 chat_font_size = g_settings->getU16("chat_font_size");
        if (chat_font_size != 0) {
                m_guitext_chat->setOverrideFont(g_fontengine->getFont(
                        rangelim(chat_font_size, 5, 72), FM_Unspecified));
        }


        // Infotext of nodes and objects.
        // If in debug mode, object debug infos shown here, too.
        // Located on the left on the screen, below chat.
        u32 chat_font_height = m_guitext_chat->getActiveFont()->getDimension(L"Ay").Height;
        m_guitext_info = gui::StaticText::add(guienv, L"",
                // Size is limited; text will be truncated after 6 lines.
                core::rect<s32>(0, 0, 400, g_fontengine->getTextHeight() * 6) +
                        v2s32(100, chat_font_height *
                        (g_settings->getU16("recent_chat_messages") + 3)),
                        false, true, guiroot);

        // Status message for in-game notifications (fly/fast mode, volume changes, etc.)
        m_status_text = std::make_unique<StatusTextHelper>(guienv, guiroot);
        m_status_text->setGameStyle();

        // Profiler text (size is updated when text is updated)
        m_guitext_profiler = gui::StaticText::add(guienv, L"<Profiler>",
                core::recti(), false, false, guiroot);
        m_guitext_profiler->setOverrideFont(g_fontengine->getFont(
                g_fontengine->getDefaultFontSize() * 0.9f, FM_Mono));
        m_guitext_profiler->setVisible(false);

        // Security overlay — persistent warning banner when connection is insecure
        // Positioned at the top-right of the screen, always visible when insecure
        m_guitext_security = gui::StaticText::add(guienv, L"",
                core::recti(), false, true, guiroot);
        m_guitext_security->setOverrideColor(video::SColor(255, 255, 80, 80)); // Red text
        m_guitext_security->setBackgroundColor(video::SColor(180, 40, 0, 0)); // Semi-transparent dark red bg
        m_guitext_security->setDrawBackground(true);
        m_guitext_security->setOverrideFont(g_fontengine->getFont(
                g_fontengine->getDefaultFontSize() * 0.85f, FM_Mono));
        m_guitext_security->setVisible(false);

        // v9: Encryption detail overlays — positioned on the left side below debug text
        // Each overlay shows specific encryption details and updates every frame.
        // All use monospace font for alignment, semi-transparent dark background.
        auto init_enc_overlay = [&](gui::IGUIStaticText* &elem, const wchar_t* name) {
                elem = gui::StaticText::add(guienv, name,
                        core::recti(), false, true, guiroot);
                elem->setOverrideFont(g_fontengine->getFont(
                        g_fontengine->getDefaultFontSize() * 0.8f, FM_Mono));
                elem->setBackgroundColor(video::SColor(160, 0, 0, 0)); // Semi-transparent black bg
                elem->setDrawBackground(true);
                elem->setOverrideColor(video::SColor(255, 200, 220, 200)); // Light green text
                elem->setVisible(false);
        };

        init_enc_overlay(m_guitext_enc_session,  L"");
        init_enc_overlay(m_guitext_enc_packets,  L"");
        init_enc_overlay(m_guitext_enc_cipher,   L"");
        init_enc_overlay(m_guitext_enc_score,    L"");
        init_enc_overlay(m_guitext_enc_authfail, L"");
        init_enc_overlay(m_guitext_enc_nonce,    L"");
        init_enc_overlay(m_guitext_enc_latency,  L"");
        init_enc_overlay(m_guitext_enc_timeline, L"");
        init_enc_overlay(m_guitext_enc_pfs,       L"");
        init_enc_overlay(m_guitext_enc_trust,     L"");
        init_enc_overlay(m_guitext_enc_health,    L"");
        init_enc_overlay(m_guitext_enc_bandwidth, L"");
}

void GameUI::update(const RunStats &stats, Client *client, MapDrawControl *draw_control,
        const CameraOrientation &cam, const PointedThing &pointed_old,
        const GUIChatConsole *chat_console, float dtime)
{
        v2u32 screensize = RenderingEngine::getWindowSize();

        LocalPlayer *player = client->getEnv().getLocalPlayer();

        s32 minimal_debug_height = 0;

        // Minimal debug text must only contain info that can't give a gameplay advantage
        if (m_flags.show_minimal_debug) {
                const u16 fps = 1.0f / stats.dtime_jitter.avg;
                m_drawtime_avg *= 0.95f;
                m_drawtime_avg += 0.05f * (stats.drawtime / 1000);

                std::ostringstream os(std::ios_base::binary);
                os << std::fixed
                        << PROJECT_NAME_C " " << g_version_hash
                        << " | FPS: " << fps
                        << std::setprecision(m_drawtime_avg < 10 ? 1 : 0)
                        << " | drawtime: " << m_drawtime_avg << "ms"
                        << std::setprecision(1)
                        << " | dtime jitter: "
                        << (stats.dtime_jitter.max_fraction * 100.0f) << "%"
                        << std::setprecision(1)
                        << " | view range: "
                        << (draw_control->range_all ? "All" : itos(draw_control->wanted_range))
                        << std::setprecision(2)
                        << " | RTT: " << (client->getRTT() * 1000.0f) << "ms";

                // Connection info overlay — shows security state in debug line
                // when show_connection_info is enabled (Settings → User Interfaces → HUD)
                // In singleplayer, show "Local" instead of "Insecure" since
                // localhost doesn't need network encryption.
                if (m_flags.show_connection_info) {
                        if (m_singleplayer_mode)
                                os << " | Local";
                        else
                                os << " | "
                                   << (m_security_info.isSecure() ? "Secure" : "Insecure");
                }

                m_guitext->setRelativePosition(core::rect<s32>(5, 5, screensize.X, screensize.Y));

                setStaticText(m_guitext, utf8_to_wide(os.str()));

                minimal_debug_height = m_guitext->getTextHeight();
        }

        // Finally set the guitext visible depending on the flag
        m_guitext->setVisible(m_flags.show_minimal_debug);

        // Basic debug text also shows info that might give a gameplay advantage
        if (m_flags.show_basic_debug) {
                v3f player_position = player->getPosition();

                std::ostringstream os(std::ios_base::binary);
                os << std::setprecision(1) << std::fixed
                        << "pos: (" << (player_position.X / BS)
                        << ", " << (player_position.Y / BS)
                        << ", " << (player_position.Z / BS)
                        << ") | yaw: " << (wrapDegrees_0_360(cam.camera_yaw)) << "° "
                        << yawToDirectionString(cam.camera_yaw)
                        << " | pitch: " << (-wrapDegrees_180(cam.camera_pitch)) << "°"
                        << " | seed: " << ((u64)client->getMapSeed());

                if (pointed_old.type == POINTEDTHING_NODE) {
                        ClientMap &map = client->getEnv().getClientMap();
                        const NodeDefManager *nodedef = client->getNodeDefManager();
                        MapNode n = map.getNode(pointed_old.node_undersurface);

                        if (n.getContent() != CONTENT_IGNORE) {
                                if (nodedef->get(n).name == "unknown") {
                                        os << ", pointed: <unknown node>";
                                } else {
                                        os << ", pointed: " << nodedef->get(n).name;
                                }
                                os << ", param2: " << (u64) n.getParam2();
                        }
                }

                m_guitext2->setRelativePosition(core::rect<s32>(5, 5 + minimal_debug_height,
                                screensize.X, screensize.Y));

                setStaticText(m_guitext2, utf8_to_wide(os.str()).c_str());
        }

        m_guitext2->setVisible(m_flags.show_basic_debug);

        setStaticText(m_guitext_info, m_infotext.c_str());
        m_guitext_info->setVisible(m_flags.show_hud && g_menumgr.menuCount() == 0);

        // Update status message element
        if (m_status_text) {
                // Handle touch control override if needed
                bool overridden = g_touchcontrols && g_touchcontrols->isStatusTextOverridden();
                if (overridden) {
                        m_status_text->setVisible(false);
                        if (g_touchcontrols)
                                g_touchcontrols->getStatusText()->setVisible(true);
                } else {
                        if (g_touchcontrols)
                                g_touchcontrols->getStatusText()->setVisible(false);
                        m_status_text->update(dtime);
                }
        }

        // Hide chat when disabled by server or when console is visible
        m_guitext_chat->setVisible(isChatVisible() && !chat_console->isVisible() && (player->hud_flags & HUD_FLAG_CHAT_VISIBLE));

        // Update security overlay — persistent warning for insecure connections
        if (m_guitext_security) {
                // v9.20: Sync security info from client, including the LIVE
                // encryption state from the connection layer. This ensures
                // the score reflects reality (not stale/fake data) by reading
                // the connection layer's actual active/ECDH state every frame.
                if (client) {
                        client->syncSecurityInfoFromConnection();
                        m_security_info = client->getConnectionSecurityInfo();
                }

                // Re-read the overlay setting each frame so toggling takes effect immediately
                m_flags.show_security_overlay = g_settings->getBool("show_security_overlay");
                m_flags.show_connection_info = g_settings->getBool("show_connection_info");

                // Re-read encryption detail overlay settings each frame
                m_flags.show_enc_session_overlay  = g_settings->getBool("show_enc_session_overlay");
                m_flags.show_enc_packets_overlay  = g_settings->getBool("show_enc_packets_overlay");
                m_flags.show_enc_cipher_overlay   = g_settings->getBool("show_enc_cipher_overlay");
                m_flags.show_enc_score_overlay    = g_settings->getBool("show_enc_score_overlay");
                m_flags.show_enc_authfail_overlay = g_settings->getBool("show_enc_authfail_overlay");
                m_flags.show_enc_nonce_overlay    = g_settings->getBool("show_enc_nonce_overlay");
                m_flags.show_enc_latency_overlay  = g_settings->getBool("show_enc_latency_overlay");
                m_flags.show_enc_timeline_overlay = g_settings->getBool("show_enc_timeline_overlay");
                m_flags.show_enc_pfs_overlay       = g_settings->getBool("show_enc_pfs_overlay");
                m_flags.show_enc_trust_overlay     = g_settings->getBool("show_enc_trust_overlay");
                m_flags.show_enc_health_overlay    = g_settings->getBool("show_enc_health_overlay");
                m_flags.show_enc_bandwidth_overlay = g_settings->getBool("show_enc_bandwidth_overlay");

                bool should_show = shouldShowSecurityOverlay() || shouldShowSecureIndicator();

                if (should_show && !m_singleplayer_mode) {
                        drawSecurityOverlay(screensize);
                } else {
                        m_guitext_security->setVisible(false);
                }
        }

        // v9: Update live encryption stats from PeerEncryptionState
        if (client && !m_singleplayer_mode) {
                syncEncryptionLiveStats(client);

                // Update packet rate tracking (measure per-second rate)
                m_packet_rate_timer += dtime;
                if (m_packet_rate_timer >= 1.0f) {
                        u64 cur_c2s = m_security_info.c2s_packets_processed;
                        u64 cur_s2c = m_security_info.s2c_packets_processed;
                        m_security_info.c2s_packets_per_sec = (float)(cur_c2s - m_last_c2s_packets) / m_packet_rate_timer;
                        m_security_info.s2c_packets_per_sec = (float)(cur_s2c - m_last_s2c_packets) / m_packet_rate_timer;
                        m_last_c2s_packets = cur_c2s;
                        m_last_s2c_packets = cur_s2c;
                        m_packet_rate_timer = 0.0f;
                }

                // Draw encryption detail overlays (left side, stacked below debug text)
                // Only draw when encryption is active or the overlay is explicitly enabled
                s32 y_offset = 5;
                if (m_flags.show_minimal_debug)
                        y_offset += m_guitext->getTextHeight();
                if (m_flags.show_basic_debug)
                        y_offset += m_guitext2->getTextHeight();

                if (m_flags.show_enc_session_overlay)
                        drawEncryptionSessionOverlay(screensize, y_offset);
                else if (m_guitext_enc_session)
                        m_guitext_enc_session->setVisible(false);

                if (m_flags.show_enc_packets_overlay)
                        drawEncryptionPacketsOverlay(screensize, y_offset);
                else if (m_guitext_enc_packets)
                        m_guitext_enc_packets->setVisible(false);

                if (m_flags.show_enc_cipher_overlay)
                        drawEncryptionCipherOverlay(screensize, y_offset);
                else if (m_guitext_enc_cipher)
                        m_guitext_enc_cipher->setVisible(false);

                if (m_flags.show_enc_score_overlay)
                        drawEncryptionScoreOverlay(screensize, y_offset);
                else if (m_guitext_enc_score)
                        m_guitext_enc_score->setVisible(false);

                if (m_flags.show_enc_authfail_overlay)
                        drawEncryptionAuthFailOverlay(screensize, y_offset);
                else if (m_guitext_enc_authfail)
                        m_guitext_enc_authfail->setVisible(false);

                if (m_flags.show_enc_nonce_overlay)
                        drawEncryptionNonceOverlay(screensize, y_offset);
                else if (m_guitext_enc_nonce)
                        m_guitext_enc_nonce->setVisible(false);

                if (m_flags.show_enc_latency_overlay)
                        drawEncryptionLatencyOverlay(screensize, y_offset);
                else if (m_guitext_enc_latency)
                        m_guitext_enc_latency->setVisible(false);

                if (m_flags.show_enc_timeline_overlay)
                        drawEncryptionTimelineOverlay(screensize, y_offset);
                else if (m_guitext_enc_timeline)
                        m_guitext_enc_timeline->setVisible(false);

                if (m_flags.show_enc_pfs_overlay)
                        drawEncryptionPFSOverlay(screensize, y_offset);
                else if (m_guitext_enc_pfs)
                        m_guitext_enc_pfs->setVisible(false);

                if (m_flags.show_enc_trust_overlay)
                        drawEncryptionTrustOverlay(screensize, y_offset);
                else if (m_guitext_enc_trust)
                        m_guitext_enc_trust->setVisible(false);

                if (m_flags.show_enc_health_overlay)
                        drawEncryptionHealthOverlay(screensize, y_offset);
                else if (m_guitext_enc_health)
                        m_guitext_enc_health->setVisible(false);

                if (m_flags.show_enc_bandwidth_overlay)
                        drawEncryptionBandwidthOverlay(screensize, y_offset);
                else if (m_guitext_enc_bandwidth)
                        m_guitext_enc_bandwidth->setVisible(false);
        } else {
                // Hide all encryption overlays when not connected or in singleplayer
                if (m_guitext_enc_session)  m_guitext_enc_session->setVisible(false);
                if (m_guitext_enc_packets)  m_guitext_enc_packets->setVisible(false);
                if (m_guitext_enc_cipher)   m_guitext_enc_cipher->setVisible(false);
                if (m_guitext_enc_score)    m_guitext_enc_score->setVisible(false);
                if (m_guitext_enc_authfail) m_guitext_enc_authfail->setVisible(false);
                if (m_guitext_enc_nonce)    m_guitext_enc_nonce->setVisible(false);
                if (m_guitext_enc_latency)  m_guitext_enc_latency->setVisible(false);
                if (m_guitext_enc_timeline) m_guitext_enc_timeline->setVisible(false);
                if (m_guitext_enc_pfs)       m_guitext_enc_pfs->setVisible(false);
                if (m_guitext_enc_trust)     m_guitext_enc_trust->setVisible(false);
                if (m_guitext_enc_health)    m_guitext_enc_health->setVisible(false);
                if (m_guitext_enc_bandwidth) m_guitext_enc_bandwidth->setVisible(false);
        }
}

void GameUI::initFlags()
{
        m_flags = GameUI::Flags();
        // Read overlay settings from g_settings (controlled by Settings → User Interfaces → HUD)
        // This is the extensible overlay pattern: each overlay checkbox maps to a flag.
        m_flags.show_security_overlay = g_settings->getBool("show_security_overlay");
        m_flags.show_connection_info = g_settings->getBool("show_connection_info");
        m_flags.show_enc_session_overlay  = g_settings->getBool("show_enc_session_overlay");
        m_flags.show_enc_packets_overlay  = g_settings->getBool("show_enc_packets_overlay");
        m_flags.show_enc_cipher_overlay   = g_settings->getBool("show_enc_cipher_overlay");
        m_flags.show_enc_score_overlay    = g_settings->getBool("show_enc_score_overlay");
        m_flags.show_enc_authfail_overlay = g_settings->getBool("show_enc_authfail_overlay");
        m_flags.show_enc_nonce_overlay    = g_settings->getBool("show_enc_nonce_overlay");
        m_flags.show_enc_latency_overlay  = g_settings->getBool("show_enc_latency_overlay");
        m_flags.show_enc_timeline_overlay = g_settings->getBool("show_enc_timeline_overlay");
        m_flags.show_enc_pfs_overlay       = g_settings->getBool("show_enc_pfs_overlay");
        m_flags.show_enc_trust_overlay     = g_settings->getBool("show_enc_trust_overlay");
        m_flags.show_enc_health_overlay    = g_settings->getBool("show_enc_health_overlay");
        m_flags.show_enc_bandwidth_overlay = g_settings->getBool("show_enc_bandwidth_overlay");
}

void GameUI::showTranslatedStatusText(const char *str)
{
        showStatusText(wstrgettext(str));
}

void GameUI::setChatText(const EnrichedString &chat_text, u32 recent_chat_count)
{
        setStaticText(m_guitext_chat, chat_text);

        m_recent_chat_count = recent_chat_count;
}

void GameUI::updateChatSize()
{
        // Update gui element size and position
        s32 chat_y = 5;

        if (m_flags.show_minimal_debug)
                chat_y += m_guitext->getTextHeight();
        if (m_flags.show_basic_debug)
                chat_y += m_guitext2->getTextHeight();

        const v2u32 window_size = RenderingEngine::getWindowSize();

        core::rect<s32> chat_size(10, chat_y, window_size.X - 20, 0);
        chat_size.LowerRightCorner.Y = std::min((s32)window_size.Y,
                        m_guitext_chat->getTextHeight() + chat_y);

        if (chat_size == m_current_chat_size)
                return;
        m_current_chat_size = chat_size;

        m_guitext_chat->setRelativePosition(chat_size);
}

void GameUI::updateProfiler()
{
        m_guitext_profiler->setVisible(m_profiler_current_page != 0);
        if (m_profiler_current_page == 0)
                return;

        // NOTE: Profiler values are snapshot-ed here and may be incomplete if
        // the frame is still in progress (e.g., some subsystems haven't updated
        // their counters yet). This is acceptable for a debug display, but
        // consumers should not rely on profiler values being fully consistent
        // within a single updateProfiler() call. To improve: cache the profiler
        // values at the end of each frame and display the cached values here.
        std::ostringstream oss(std::ios_base::binary);
        oss << "Profiler page " << (int)m_profiler_current_page
                << "/" << (int)m_profiler_max_page
                << ", elapsed: " << g_profiler->getElapsedMs() << " ms" << std::endl;
        g_profiler->print(oss, m_profiler_current_page, m_profiler_max_page);

        EnrichedString str(utf8_to_wide(oss.str()));
        str.setBackground(video::SColor(120, 0, 0, 0));
        setStaticText(m_guitext_profiler, str);

        v2s32 upper_left(5, 10);
        if (m_flags.show_minimal_debug)
                upper_left.Y += m_guitext->getTextHeight();
        if (m_flags.show_basic_debug)
                upper_left.Y += m_guitext2->getTextHeight();

        v2s32 lower_right = upper_left;
        lower_right.X += m_guitext_profiler->getTextWidth() + 5;
        lower_right.Y += m_guitext_profiler->getTextHeight();

        m_guitext_profiler->setRelativePosition(core::recti(upper_left, lower_right));

        // Really dumb heuristic (we have a fixed number of pages, not a fixed page size)
        const v2u32 window_size = RenderingEngine::getWindowSize();
        if (upper_left.Y + m_guitext_profiler->getTextHeight()
                > window_size.Y * 0.7f) {
                if (m_profiler_max_page < 5) {
                        m_profiler_max_page++;
                        updateProfiler(); // do it again
                }
        }
}

void GameUI::toggleChat(Client *client)
{
        if (client->getEnv().getLocalPlayer()->hud_flags & HUD_FLAG_CHAT_VISIBLE) {
                m_flags.show_chat = !m_flags.show_chat;
                if (m_flags.show_chat)
                        showTranslatedStatusText("Chat shown");
                else
                        showTranslatedStatusText("Chat hidden");
        } else {
                showTranslatedStatusText("Chat currently disabled by game or mod");
        }

}

void GameUI::toggleHud()
{
        m_flags.show_hud = !m_flags.show_hud;
        if (m_flags.show_hud)
                showTranslatedStatusText("HUD shown");
        else
                showTranslatedStatusText("HUD hidden");
}

void GameUI::toggleProfiler()
{
        m_profiler_current_page = (m_profiler_current_page + 1) % (m_profiler_max_page + 1);

        // Only update profiler when it's being displayed, not when hiding
        if (m_profiler_current_page != 0) {
                updateProfiler();
                std::wstring msg = fwgettext("Profiler shown (page %d of %d)",
                                m_profiler_current_page, m_profiler_max_page);
                showStatusText(msg);
        } else {
                showTranslatedStatusText("Profiler hidden");
        }
}

void GameUI::clearText()
{
        if (m_guitext_chat) {
                m_guitext_chat->remove();
                m_guitext_chat = nullptr;
        }

        if (m_guitext) {
                m_guitext->remove();
                m_guitext = nullptr;
        }

        if (m_guitext2) {
                m_guitext2->remove();
                m_guitext2 = nullptr;
        }

        if (m_guitext_info) {
                m_guitext_info->remove();
                m_guitext_info = nullptr;
        }

        m_status_text.reset();

        if (m_guitext_profiler) {
                m_guitext_profiler->remove();
                m_guitext_profiler = nullptr;
        }

        if (m_guitext_security) {
                m_guitext_security->remove();
                m_guitext_security = nullptr;
        }

        // v9: Clean up encryption detail overlay elements
        auto cleanup_overlay = [](gui::IGUIStaticText* &elem) {
                if (elem) {
                        elem->remove();
                        elem = nullptr;
                }
        };
        cleanup_overlay(m_guitext_enc_session);
        cleanup_overlay(m_guitext_enc_packets);
        cleanup_overlay(m_guitext_enc_cipher);
        cleanup_overlay(m_guitext_enc_score);
        cleanup_overlay(m_guitext_enc_authfail);
        cleanup_overlay(m_guitext_enc_nonce);
        cleanup_overlay(m_guitext_enc_latency);
        cleanup_overlay(m_guitext_enc_timeline);
        cleanup_overlay(m_guitext_enc_pfs);
        cleanup_overlay(m_guitext_enc_trust);
        cleanup_overlay(m_guitext_enc_health);
        cleanup_overlay(m_guitext_enc_bandwidth);
}

void GameUI::setConnectionSecurity(ConnectionSecurity sec)
{
        m_security_info.state = sec;
}

void GameUI::setConnectionSecurityInfo(const ConnectionSecurityInfo& info)
{
        m_security_info = info;
}

bool GameUI::shouldShowSecurityOverlay() const
{
        // If the user has disabled the security overlay via settings, respect that.
        // This allows users who understand the risks to suppress the banner.
        if (!m_flags.show_security_overlay)
                return false;

        // In singleplayer mode, the connection is local and inherently secure,
        // so we don't show the warning.
        if (m_singleplayer_mode)
                return false;

        // Show the overlay when the connection is not encrypted.
        // The overlay is always visible when insecure AND the setting is enabled,
        // regardless of HUD or debug mode, because it's a critical security warning.
        return !m_security_info.isSecure();
}

bool GameUI::shouldShowSecureIndicator() const
{
        // Show a green "SECURE CONNECTION" indicator when:
        // 1. The connection is encrypted
        // 2. The user has enabled the security overlay setting (controls both indicators)
        // 3. Not in singleplayer mode (local connection, no need to show indicator)
        if (!m_flags.show_security_overlay)
                return false;

        if (m_singleplayer_mode)
                return false;

        return m_security_info.isSecure();
}

void GameUI::resetSecurityInfo()
{
        // Reset security info to defaults (called on disconnect)
        m_security_info = ConnectionSecurityInfo();
}

void GameUI::drawSecurityOverlay(const v2u32 &screensize)
{
        // v9: Toggleable security overlay with three display modes.
        // The mode is controlled by the "security_overlay_mode" setting:
        //   "mini"     — Small lock icon (compact)
        //   "standard" — Lock icon + status text + encryption algorithm
        //   "detailed" — Multi-line panel with full security info
        //   "off"      — No overlay shown at all
        std::string mode = g_settings->get("security_overlay_mode");

        if (mode == "off") {
                m_guitext_security->setVisible(false);
                return;
        }

        bool is_secure = m_security_info.isSecure();
        std::wstring security_text;

        if (mode == "mini") {
                // Mini mode: just a small icon
                if (is_secure) {
                        security_text = L"[+] AES-256-GCM";
                } else {
                        security_text = L"[!] NOT ENCRYPTED";
                }
        } else if (mode == "detailed") {
                // Detailed mode: multi-line panel with all security info
                if (is_secure) {
                        std::wostringstream wos;
                        wos << L"[+] ENCRYPTED CONNECTION\n"
                            << L"Cipher: " << utf8_to_wide(m_security_info.getCipherSuiteString()) << L"\n"
                            << L"Key: " << utf8_to_wide(m_security_info.getKeyExchangeString()) << L"\n"
                            << L"Auth: " << utf8_to_wide(m_security_info.getAuthenticationString()) << L"\n"
                            << L"Replay: " << (m_security_info.isReplayProtected() ? L"Yes" : L"No") << L"\n"
                            << L"PFS: " << (m_security_info.isForwardSecret() ? L"Yes" : L"No") << L"\n"
                            << L"Score: " << utf8_to_wide(m_security_info.getSecurityScoreString());
                        security_text = wos.str();
                } else {
                        std::wostringstream wos;
                        wos << L"[!] CONNECTION NOT ENCRYPTED\n"
                            << L"All game traffic is visible on the network\n"
                            << L"Score: " << utf8_to_wide(m_security_info.getSecurityScoreString());
                        security_text = wos.str();
                }
        } else {
                // Standard mode (default): icon + status + algorithm
                if (is_secure) {
                        security_text = L"[+] SECURE: " +
                                utf8_to_wide(m_security_info.getCipherSuiteString() + " | " +
                                        m_security_info.getKeyExchangeString());
                } else {
                        security_text = L"[!] INSECURE CONNECTION";
                }
        }

        setStaticText(m_guitext_security, security_text);

        // Set colors based on security state
        if (is_secure) {
                // Green text + semi-transparent dark green background
                m_guitext_security->setOverrideColor(video::SColor(255, 80, 255, 80));
                m_guitext_security->setBackgroundColor(video::SColor(140, 0, 30, 0));
        } else {
                // Red text + semi-transparent dark red background
                m_guitext_security->setOverrideColor(video::SColor(255, 255, 80, 80));
                m_guitext_security->setBackgroundColor(video::SColor(180, 40, 0, 0));
        }

        // Position at top-right corner with padding
        u32 text_width = m_guitext_security->getTextWidth() + 10;
        u32 text_height = m_guitext_security->getTextHeight() + 6;
        s32 x_pos = screensize.X - text_width - 10;
        s32 y_pos = 5;
        m_guitext_security->setRelativePosition(
                core::rect<s32>(x_pos, y_pos, x_pos + text_width, y_pos + text_height));

        m_guitext_security->setVisible(true);
}

// ============================================================================
// v9: Live encryption stats sync from PeerEncryptionState
// ============================================================================

void GameUI::syncEncryptionLiveStats(Client *client)
{
        if (!client)
                return;

        const auto &enc_state = client->getEncryptionState();
        auto lock = enc_state.lock();

        // Copy live stats from PeerEncryptionState into m_security_info
        m_security_info.c2s_packets_processed = enc_state.c2s.packets_processed;
        m_security_info.s2c_packets_processed = enc_state.s2c.packets_processed;
        m_security_info.c2s_auth_failures = enc_state.c2s.auth_failures;
        m_security_info.s2c_auth_failures = enc_state.s2c.auth_failures;
        m_security_info.c2s_replay_attempts = enc_state.c2s.replay_attempts;
        m_security_info.s2c_replay_attempts = enc_state.s2c.replay_attempts;
        m_security_info.c2s_nonce_counter = enc_state.c2s.nonce_counter;
        m_security_info.s2c_nonce_counter = enc_state.s2c.nonce_counter;

        // Copy RTT from client
        m_security_info.rtt_ms = client->getRTT() * 1000.0f;

        // Encryption overhead (only when encrypted)
        if (m_security_info.isSecure()) {
                m_security_info.encryption_overhead_bytes = ENCRYPTED_PACKET_OVERHEAD;
        } else {
                m_security_info.encryption_overhead_bytes = 0;
        }

        // Timestamps
        m_security_info.encryption_activated_at = enc_state.activated_at;
}

// ============================================================================
// v9: Helper to position an overlay text element
// ============================================================================

gui::IGUIStaticText* GameUI::createOverlayText(gui::IGUIStaticText* &element,
        const v2u32 &screensize, s32 x, s32 y,
        const video::SColor &text_color, const video::SColor &bg_color)
{
        if (!element)
                return nullptr;
        element->setOverrideColor(text_color);
        element->setBackgroundColor(bg_color);
        // Position will be set by the caller after text is set
        return element;
}

// ============================================================================
// Overlay 1: Session & Fingerprint
// Shows: Session ID (truncated), server fingerprint, connection uptime
// ============================================================================

void GameUI::drawEncryptionSessionOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_session)
                return;

        std::wostringstream wos;
        wos << L"[ENC:SESSION] ";
        if (m_security_info.isSecure()) {
                std::string sid = m_security_info.session_id;
                if (sid.length() > 16)
                        sid = sid.substr(0, 16) + "...";
                wos << L"SID=" << utf8_to_wide(sid)
                    << L" | FP=" << utf8_to_wide(m_security_info.server_fingerprint)
                    << L" | Up=" << utf8_to_wide(m_security_info.getConnectionUptimeString());
        } else {
                wos << L"NOT ENCRYPTED - No session";
        }

        setStaticText(m_guitext_enc_session, wos.str());

        bool is_secure = m_security_info.isSecure();
        m_guitext_enc_session->setOverrideColor(is_secure ?
                video::SColor(255, 160, 230, 160) : video::SColor(255, 255, 120, 120));
        m_guitext_enc_session->setBackgroundColor(is_secure ?
                video::SColor(150, 0, 20, 0) : video::SColor(150, 30, 0, 0));

        u32 tw = m_guitext_enc_session->getTextWidth() + 8;
        u32 th = m_guitext_enc_session->getTextHeight() + 4;
        m_guitext_enc_session->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_session->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 2: Packet Counter
// Shows: C2S/S2C packet counts, total, rate per second
// ============================================================================

void GameUI::drawEncryptionPacketsOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_packets)
                return;

        std::wostringstream wos;
        wos << L"[ENC:PACKETS] ";
        if (m_security_info.isSecure()) {
                wos << L"C2S=" << (u64)m_security_info.c2s_packets_processed
                    << L" S2C=" << (u64)m_security_info.s2c_packets_processed
                    << L" Total=" << m_security_info.getTotalPacketsProcessed()
                    << L" | ";
                wos << std::fixed << std::setprecision(1);
                wos << L"Rate: C2S=" << m_security_info.c2s_packets_per_sec << L"/s"
                    << L" S2C=" << m_security_info.s2c_packets_per_sec << L"/s"
                    << L" Total=" << m_security_info.getTotalPacketsPerSec() << L"/s";
        } else {
                wos << L"NOT ENCRYPTED";
        }

        setStaticText(m_guitext_enc_packets, wos.str());

        m_guitext_enc_packets->setOverrideColor(video::SColor(255, 180, 200, 255));
        m_guitext_enc_packets->setBackgroundColor(video::SColor(150, 0, 0, 20));

        u32 tw = m_guitext_enc_packets->getTextWidth() + 8;
        u32 th = m_guitext_enc_packets->getTextHeight() + 4;
        m_guitext_enc_packets->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_packets->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 3: Cipher Suite Detail
// Shows: Full cipher suite, key exchange, TLS version, auth method
// ============================================================================

void GameUI::drawEncryptionCipherOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_cipher)
                return;

        std::wostringstream wos;
        wos << L"[ENC:CIPHER] ";
        if (m_security_info.isSecure()) {
                wos << L"Cipher=" << utf8_to_wide(m_security_info.getCipherSuiteString())
                    << L" | KEX=" << utf8_to_wide(m_security_info.getKeyExchangeString())
                    << L" | Auth=" << utf8_to_wide(m_security_info.getAuthenticationString())
                    << L" | TLS=" << utf8_to_wide(m_security_info.getTlsVersionString());
        } else {
                wos << L"NO CIPHER - Connection plaintext";
        }

        setStaticText(m_guitext_enc_cipher, wos.str());

        bool is_secure = m_security_info.isSecure();
        m_guitext_enc_cipher->setOverrideColor(is_secure ?
                video::SColor(255, 160, 230, 160) : video::SColor(255, 255, 120, 120));
        m_guitext_enc_cipher->setBackgroundColor(is_secure ?
                video::SColor(150, 0, 20, 0) : video::SColor(150, 30, 0, 0));

        u32 tw = m_guitext_enc_cipher->getTextWidth() + 8;
        u32 th = m_guitext_enc_cipher->getTextHeight() + 4;
        m_guitext_enc_cipher->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_cipher->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 4: Security Score Bar
// Shows: Visual score bar 0-100 with label and grade
// ============================================================================

void GameUI::drawEncryptionScoreOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_score)
                return;

        int score = m_security_info.getSecurityScore();
        std::string bar_str = m_security_info.getSecurityScoreBar(25);

        std::wostringstream wos;
        wos << L"[ENC:SCORE] " << utf8_to_wide(bar_str);

        // Add breakdown on second line
        wos << L"\n  +30(enc)=" << (m_security_info.isSecure() ? L"Y" : L"N");
        wos << L" +15(cipher)=" << (m_security_info.cipher_suite == ConnectionSecurityInfo::CIPHER_AES_256_GCM ||
                m_security_info.cipher_suite == ConnectionSecurityInfo::CIPHER_CHACHA20_POLY1305 ? L"Y" : L"N");
        wos << L" +15(PFS)=" << (m_security_info.isForwardSecret() ? L"Y" : L"N");
        wos << L" +15(auth)=" << (m_security_info.isAuthenticated() ? L"Y" : L"N");
        wos << L" +10(replay)=" << (m_security_info.isReplayProtected() ? L"Y" : L"N");
        wos << L" +10(cert)=" << (m_security_info.certificate_status == ConnectionSecurityInfo::CERT_VERIFIED ||
                m_security_info.certificate_status == ConnectionSecurityInfo::CERT_PINNED ? L"Y" : L"N");
        wos << L" +5(TLS)=" << (m_security_info.tls_version == ConnectionSecurityInfo::TLS_1_3 ||
                m_security_info.tls_version == ConnectionSecurityInfo::TLS_1_3_EQUIVALENT ? L"Y" : L"N");

        setStaticText(m_guitext_enc_score, wos.str());

        // Color based on score
        video::SColor text_color;
        video::SColor bg_color;
        if (score >= 80) {
                text_color = video::SColor(255, 80, 255, 80);   // Green
                bg_color = video::SColor(150, 0, 25, 0);
        } else if (score >= 60) {
                text_color = video::SColor(255, 200, 255, 80);  // Yellow-green
                bg_color = video::SColor(150, 20, 15, 0);
        } else if (score >= 30) {
                text_color = video::SColor(255, 255, 200, 80);  // Yellow
                bg_color = video::SColor(150, 25, 15, 0);
        } else {
                text_color = video::SColor(255, 255, 80, 80);   // Red
                bg_color = video::SColor(150, 30, 0, 0);
        }
        m_guitext_enc_score->setOverrideColor(text_color);
        m_guitext_enc_score->setBackgroundColor(bg_color);

        u32 tw = m_guitext_enc_score->getTextWidth() + 8;
        u32 th = m_guitext_enc_score->getTextHeight() + 4;
        m_guitext_enc_score->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_score->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 5: Auth Failures & Replay Attempts
// Shows: C2S/S2C auth failure counts, replay attempts detected
// ============================================================================

void GameUI::drawEncryptionAuthFailOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_authfail)
                return;

        std::wostringstream wos;
        wos << L"[ENC:AUTH] ";
        if (m_security_info.isSecure()) {
                u64 total_auth_fail = m_security_info.getTotalAuthFailures();
                u64 total_replay = m_security_info.getTotalReplayAttempts();
                wos << L"AuthFail: C2S=" << (u64)m_security_info.c2s_auth_failures
                    << L" S2C=" << (u64)m_security_info.s2c_auth_failures
                    << L" Total=" << total_auth_fail
                    << L" | Replay: C2S=" << (u64)m_security_info.c2s_replay_attempts
                    << L" S2C=" << (u64)m_security_info.s2c_replay_attempts
                    << L" Total=" << total_replay;

                if (total_auth_fail > 0 || total_replay > 0) {
                        wos << L" *** SECURITY ALERT ***";
                }
        } else {
                wos << L"NO ENCRYPTION - Auth tracking unavailable";
        }

        setStaticText(m_guitext_enc_authfail, wos.str());

        bool has_alerts = m_security_info.getTotalAuthFailures() > 0 ||
                          m_security_info.getTotalReplayAttempts() > 0;
        bool is_secure = m_security_info.isSecure();

        video::SColor text_color;
        video::SColor bg_color;
        if (has_alerts) {
                text_color = video::SColor(255, 255, 80, 80);   // Red alert
                bg_color = video::SColor(180, 50, 0, 0);
        } else if (is_secure) {
                text_color = video::SColor(255, 160, 230, 160); // Green
                bg_color = video::SColor(150, 0, 20, 0);
        } else {
                text_color = video::SColor(255, 255, 150, 150); // Dim red
                bg_color = video::SColor(150, 30, 0, 0);
        }
        m_guitext_enc_authfail->setOverrideColor(text_color);
        m_guitext_enc_authfail->setBackgroundColor(bg_color);

        u32 tw = m_guitext_enc_authfail->getTextWidth() + 8;
        u32 th = m_guitext_enc_authfail->getTextHeight() + 4;
        m_guitext_enc_authfail->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_authfail->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 6: Nonce Counter
// Shows: Current nonce counters, packets until wrap, % of nonce space used
// ============================================================================

void GameUI::drawEncryptionNonceOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_nonce)
                return;

        std::wostringstream wos;
        wos << L"[ENC:NONCE] ";
        if (m_security_info.isSecure()) {
                wos << L"C2S counter=" << (u64)m_security_info.c2s_nonce_counter
                    << L" S2C counter=" << (u64)m_security_info.s2c_nonce_counter;

                // Packets until wrap (2^64 is practically infinite, but show it)
                u64 max_nonce = (u64)-1;
                u64 c2s_remaining = max_nonce - m_security_info.c2s_nonce_counter;
                u64 s2c_remaining = max_nonce - m_security_info.s2c_nonce_counter;
                wos << L"\n  C2S remaining=" << c2s_remaining
                    << L" S2C remaining=" << s2c_remaining;

                // Show percentage used (scientific notation for very small values)
                double pct_used = m_security_info.getNonceSpaceUsedPercent();
                wos << std::fixed << std::setprecision(15);
                wos << L" | Space used: " << pct_used << L"%";
        } else {
                wos << L"NO ENCRYPTION - No nonce tracking";
        }

        setStaticText(m_guitext_enc_nonce, wos.str());

        m_guitext_enc_nonce->setOverrideColor(video::SColor(255, 180, 200, 255));
        m_guitext_enc_nonce->setBackgroundColor(video::SColor(150, 0, 0, 20));

        u32 tw = m_guitext_enc_nonce->getTextWidth() + 8;
        u32 th = m_guitext_enc_nonce->getTextHeight() + 4;
        m_guitext_enc_nonce->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_nonce->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 7: Network Latency & Encryption Overhead
// Shows: RTT, packet loss, encryption overhead per packet
// ============================================================================

void GameUI::drawEncryptionLatencyOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_latency)
                return;

        std::wostringstream wos;
        wos << L"[ENC:NET] ";
        wos << std::fixed << std::setprecision(1);
        wos << L"RTT=" << m_security_info.rtt_ms << L"ms";

        if (m_security_info.isSecure()) {
                wos << L" | Overhead=" << m_security_info.encryption_overhead_bytes << L"B/pkt"
                    << L" | PFS=" << (m_security_info.isForwardSecret() ? L"Y" : L"N")
                    << L" | Replay=" << (m_security_info.isReplayProtected() ? L"Y" : L"N")
                    << L" | Cert=" << utf8_to_wide(m_security_info.getCertificateStatusString());
        } else {
                wos << L" | NOT ENCRYPTED (0B overhead, no protection)";
        }

        setStaticText(m_guitext_enc_latency, wos.str());

        bool is_secure = m_security_info.isSecure();
        m_guitext_enc_latency->setOverrideColor(is_secure ?
                video::SColor(255, 160, 230, 160) : video::SColor(255, 255, 120, 120));
        m_guitext_enc_latency->setBackgroundColor(is_secure ?
                video::SColor(150, 0, 20, 0) : video::SColor(150, 30, 0, 0));

        u32 tw = m_guitext_enc_latency->getTextWidth() + 8;
        u32 th = m_guitext_enc_latency->getTextHeight() + 4;
        m_guitext_enc_latency->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_latency->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 8: Connection Timeline
// Shows: Key events timeline: init -> derived -> activated -> now
// ============================================================================

void GameUI::drawEncryptionTimelineOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_timeline)
                return;

        std::wostringstream wos;
        wos << L"[ENC:TIMELINE] ";

        u64 now = (u64)time(nullptr);

        if (m_security_info.connected_since > 0) {
                u64 connected_ago = now - m_security_info.connected_since;
                wos << L"Connected=" << connected_ago << L"s ago";

                if (m_security_info.keys_derived_at > 0) {
                        u64 derived_ago = now - m_security_info.keys_derived_at;
                        u64 derive_delay = m_security_info.keys_derived_at - m_security_info.connected_since;
                        wos << L" | KeysDerived=" << derived_ago << L"s ago"
                            << L" (delay=" << derive_delay << L"s)";
                } else {
                        wos << L" | Keys: NOT DERIVED";
                }

                if (m_security_info.encryption_activated_at > 0) {
                        u64 activated_ago = now - m_security_info.encryption_activated_at;
                        u64 activate_delay = m_security_info.encryption_activated_at - m_security_info.connected_since;
                        wos << L" | Activated=" << activated_ago << L"s ago"
                            << L" (delay=" << activate_delay << L"s)";
                } else {
                        wos << L" | Activation: PENDING";
                }

                // Status summary
                if (m_security_info.isSecure()) {
                        wos << L"\n  Status: FULLY ENCRYPTED since activation";
                } else {
                        wos << L"\n  Status: NOT ENCRYPTED - traffic is plaintext!";
                }
        } else {
                wos << L"NOT CONNECTED";
        }

        setStaticText(m_guitext_enc_timeline, wos.str());

        bool is_secure = m_security_info.isSecure();
        m_guitext_enc_timeline->setOverrideColor(is_secure ?
                video::SColor(255, 160, 230, 160) : video::SColor(255, 255, 120, 120));
        m_guitext_enc_timeline->setBackgroundColor(is_secure ?
                video::SColor(150, 0, 20, 0) : video::SColor(150, 30, 0, 0));

        u32 tw = m_guitext_enc_timeline->getTextWidth() + 8;
        u32 th = m_guitext_enc_timeline->getTextHeight() + 4;
        m_guitext_enc_timeline->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_timeline->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 9: Forward Secrecy (PFS)
// Shows: ECDH X25519 forward secrecy status, key exchange method
// ============================================================================

void GameUI::drawEncryptionPFSOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_pfs)
                return;

        std::wostringstream wos;
        wos << L"[ENC:PFS] ";
        if (m_security_info.isSecure()) {
                if (m_security_info.forward_secrecy) {
                        wos << L"PFS=Yes | ECDH=X25519 | KEX=" << utf8_to_wide(m_security_info.getKeyExchangeString())
                            << L" | Keys=ECDH+SRP Combined";
                } else {
                        wos << L"PFS=No | ECDH=Not Completed | KEX=SRP Only";
                }
        } else {
                wos << L"NOT ENCRYPTED";
        }

        setStaticText(m_guitext_enc_pfs, wos.str());

        bool has_pfs = m_security_info.isSecure() && m_security_info.forward_secrecy;
        bool is_secure = m_security_info.isSecure();
        m_guitext_enc_pfs->setOverrideColor(has_pfs ?
                video::SColor(255, 80, 255, 80) : (is_secure ?
                video::SColor(255, 255, 200, 80) : video::SColor(255, 255, 120, 120)));
        m_guitext_enc_pfs->setBackgroundColor(has_pfs ?
                video::SColor(150, 0, 30, 0) : (is_secure ?
                video::SColor(150, 25, 15, 0) : video::SColor(150, 30, 0, 0)));

        u32 tw = m_guitext_enc_pfs->getTextWidth() + 8;
        u32 th = m_guitext_enc_pfs->getTextHeight() + 4;
        m_guitext_enc_pfs->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_pfs->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 10: Fingerprint Pinning / Trust
// Shows: Certificate pinning status, verification result, fingerprint
// ============================================================================

void GameUI::drawEncryptionTrustOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_trust)
                return;

        std::wostringstream wos;
        wos << L"[ENC:TRUST] ";
        if (m_security_info.isSecure()) {
                if (m_security_info.fingerprint_pinned) {
                        std::string fp = m_security_info.server_fingerprint;
                        if (fp.length() > 20)
                                fp = fp.substr(0, 20) + "...";
                        wos << L"Pin=Yes | Verify=Match | Cert=Pinned (Verified) | FP=SHA256:" << utf8_to_wide(fp);
                } else {
                        wos << L"Pin=No | TOFU | Cert=Trust On First Use";
                }
        } else {
                wos << L"NOT ENCRYPTED";
        }

        setStaticText(m_guitext_enc_trust, wos.str());

        bool is_pinned = m_security_info.isSecure() && m_security_info.fingerprint_pinned;
        bool is_secure = m_security_info.isSecure();
        m_guitext_enc_trust->setOverrideColor(is_pinned ?
                video::SColor(255, 80, 255, 80) : (is_secure ?
                video::SColor(255, 255, 200, 80) : video::SColor(255, 255, 120, 120)));
        m_guitext_enc_trust->setBackgroundColor(is_pinned ?
                video::SColor(150, 0, 30, 0) : (is_secure ?
                video::SColor(150, 25, 15, 0) : video::SColor(150, 30, 0, 0)));

        u32 tw = m_guitext_enc_trust->getTextWidth() + 8;
        u32 th = m_guitext_enc_trust->getTextHeight() + 4;
        m_guitext_enc_trust->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_trust->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 11: Security Health Summary
// Shows: Overall security score, PFS, pin status, auth failures, replay attempts
// ============================================================================

void GameUI::drawEncryptionHealthOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_health)
                return;

        int score = m_security_info.getSecurityScore();
        std::string grade = m_security_info.getSecurityScoreString();

        std::wostringstream wos;
        wos << L"[ENC:HEALTH] ";
        if (m_security_info.isSecure()) {
                wos << L"Score=" << score << L"/100 (" << utf8_to_wide(grade) << L")"
                    << L" | PFS=" << (m_security_info.isForwardSecret() ? L"Yes" : L"No")
                    << L" | Pin=" << (m_security_info.fingerprint_pinned ? L"Yes" : L"No")
                    << L" | " << m_security_info.getTotalAuthFailures() << L" AuthFail"
                    << L" | " << m_security_info.getTotalReplayAttempts() << L" Replay";
        } else {
                wos << L"Score=" << score << L"/100 (" << utf8_to_wide(grade) << L")"
                    << L" | NOT ENCRYPTED";
        }

        setStaticText(m_guitext_enc_health, wos.str());

        // Color based on score
        video::SColor text_color;
        video::SColor bg_color;
        if (score >= 80) {
                text_color = video::SColor(255, 80, 255, 80);   // Green
                bg_color = video::SColor(150, 0, 25, 0);
        } else if (score >= 60) {
                text_color = video::SColor(255, 200, 255, 80);  // Yellow-green
                bg_color = video::SColor(150, 20, 15, 0);
        } else if (score >= 30) {
                text_color = video::SColor(255, 255, 200, 80);  // Yellow
                bg_color = video::SColor(150, 25, 15, 0);
        } else {
                text_color = video::SColor(255, 255, 80, 80);   // Red
                bg_color = video::SColor(150, 30, 0, 0);
        }
        m_guitext_enc_health->setOverrideColor(text_color);
        m_guitext_enc_health->setBackgroundColor(bg_color);

        u32 tw = m_guitext_enc_health->getTextWidth() + 8;
        u32 th = m_guitext_enc_health->getTextHeight() + 4;
        m_guitext_enc_health->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_health->setVisible(true);

        y_offset += th + 2;
}

// ============================================================================
// Overlay 12: Encryption Bandwidth Impact
// Shows: Per-packet overhead, total overhead, packet rate, overhead percentage
// ============================================================================

void GameUI::drawEncryptionBandwidthOverlay(const v2u32 &screensize, s32 &y_offset)
{
        if (!m_guitext_enc_bandwidth)
                return;

        std::wostringstream wos;
        wos << L"[ENC:BW] ";
        if (m_security_info.isSecure()) {
                u32 oh_per_pkt = m_security_info.encryption_overhead_bytes;
                u64 total_pkts = m_security_info.getTotalPacketsProcessed();
                u64 total_oh = (u64)oh_per_pkt * total_pkts;

                // Calculate overhead percentage (assume ~200B avg payload per packet)
                const u32 avg_payload_size = 200;
                float oh_pct = 0.0f;
                if (total_pkts > 0) {
                        u64 total_payload = (u64)avg_payload_size * total_pkts;
                        u64 total_with_oh = total_payload + total_oh;
                        if (total_with_oh > 0)
                                oh_pct = (float)total_oh / (float)total_with_oh * 100.0f;
                }

                // Format total overhead in human-readable units
                std::string oh_str;
                if (total_oh >= 1048576)
                        oh_str = std::to_string(total_oh / 1048576) + "." +
                                 std::to_string((total_oh % 1048576) * 10 / 1048576) + "MB";
                else if (total_oh >= 1024)
                        oh_str = std::to_string(total_oh / 1024) + "KB";
                else
                        oh_str = std::to_string(total_oh) + "B";

                wos << L"Overhead=" << oh_per_pkt << L"B/pkt"
                    << L" | TotalOH=" << utf8_to_wide(oh_str)
                    << L" | Rate=" << std::fixed << std::setprecision(0)
                    << m_security_info.getTotalPacketsPerSec() << L"pkt/s"
                    << L" | OH%=" << std::setprecision(1) << oh_pct << L"%";
        } else {
                wos << L"NOT ENCRYPTED - No overhead";
        }

        setStaticText(m_guitext_enc_bandwidth, wos.str());

        m_guitext_enc_bandwidth->setOverrideColor(video::SColor(255, 180, 200, 255));
        m_guitext_enc_bandwidth->setBackgroundColor(video::SColor(150, 0, 0, 20));

        u32 tw = m_guitext_enc_bandwidth->getTextWidth() + 8;
        u32 th = m_guitext_enc_bandwidth->getTextHeight() + 4;
        m_guitext_enc_bandwidth->setRelativePosition(
                core::rect<s32>(5, y_offset, 5 + tw, y_offset + th));
        m_guitext_enc_bandwidth->setVisible(true);

        y_offset += th + 2;
}
