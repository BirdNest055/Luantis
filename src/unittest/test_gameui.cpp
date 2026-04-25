// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "test.h"

#include "client/gameui.h"
#include "gui/statusTextHelper.h"
#include "network/connection_security.h"

class TestGameUI : public TestBase
{
public:
        TestGameUI() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestGameUI"; }

        void runTests(IGameDef *gamedef);

        void testInit();
        void testInfoText();
        void testStatusText();
        void testSecurityOverlayDefault();
        void testSecurityOverlayInsecure();
        void testSecurityOverlayEncrypted();
        void testSecurityOverlaySingleplayer();
        void testSecurityOverlaySingleplayerInsecure();
        void testSetConnectionSecurity();
        void testSecurityOverlaySettingDisabled();
        void testSecurityOverlaySettingEnabled();
        void testSecurityOverlaySettingOverridesInsecure();
        void testConnectionInfoFlagDefault();
        void testOverlayFlagDefaults();
        // v8: Security info tab tests
        void testSecurityInfoDefaultNotConnected();
        void testSecurityInfoSetAndGetFullInfo();
        void testSecurityInfoStateTransitionOnConnect();
        void testSecurityInfoResetOnDisconnect();
        void testSecurityInfoEncryptedIndicatorLogic();
        void testSecurityInfoOverlayNotShownWhenEncrypted();
        void testSecurityInfoStringConversionsComplete();
};

static TestGameUI g_test_instance;

void TestGameUI::runTests(IGameDef *gamedef)
{
        TEST(testInit);
        TEST(testInfoText);
        TEST(testStatusText);
        TEST(testSecurityOverlayDefault);
        TEST(testSecurityOverlayInsecure);
        TEST(testSecurityOverlayEncrypted);
        TEST(testSecurityOverlaySingleplayer);
        TEST(testSecurityOverlaySingleplayerInsecure);
        TEST(testSetConnectionSecurity);
        TEST(testSecurityOverlaySettingDisabled);
        TEST(testSecurityOverlaySettingEnabled);
        TEST(testSecurityOverlaySettingOverridesInsecure);
        TEST(testConnectionInfoFlagDefault);
        TEST(testOverlayFlagDefaults);
        // v8: Security info tab tests
        TEST(testSecurityInfoDefaultNotConnected);
        TEST(testSecurityInfoSetAndGetFullInfo);
        TEST(testSecurityInfoStateTransitionOnConnect);
        TEST(testSecurityInfoResetOnDisconnect);
        TEST(testSecurityInfoEncryptedIndicatorLogic);
        TEST(testSecurityInfoOverlayNotShownWhenEncrypted);
        TEST(testSecurityInfoStringConversionsComplete);
}

void TestGameUI::testInit()
{
        GameUI gui{};
        // Ensure flags on GameUI init
        UASSERT(gui.getFlags().show_chat)
        UASSERT(gui.getFlags().show_hud)
        UASSERT(!gui.getFlags().show_profiler_graph)

        // And after the initFlags init stage
        gui.initFlags();
        UASSERT(gui.getFlags().show_chat)
        UASSERT(gui.getFlags().show_hud)
        UASSERT(!gui.getFlags().show_profiler_graph)

        // @TODO verify if we can create non UI nulldevice to test this function
        // gui.init();
}

void TestGameUI::testStatusText()
{
        StatusTextHelper status_text(nullptr);

        UASSERT(status_text.getStatusText().empty());
        UASSERT(status_text.getStatusTextTime() == 0.0f);

        status_text.showStatusText(L"test status");
        UASSERT(status_text.getStatusText() == L"test status");
        UASSERT(status_text.getStatusTextTime() == 0.0f);

        status_text.clearStatusText();
        UASSERT(status_text.getStatusText().empty());
        UASSERT(status_text.getStatusTextTime() == 0.0f);
}

void TestGameUI::testInfoText()
{
        GameUI gui{};
        gui.setInfoText(L"test info");

        UASSERT(gui.m_infotext == L"test info");
}

void TestGameUI::testSecurityOverlayDefault()
{
        // By default, GameUI should be in insecure mode (no connection yet)
        GameUI gui{};

        // Default connection security should be Insecure
        UASSERT(gui.getConnectionSecurity() == ConnectionSecurity::Insecure);

        // Overlay should be visible by default (not yet connected = insecure,
        // and show_security_overlay defaults to true in the Flags struct)
        // Note: singleplayer_mode defaults to false
        UASSERT(gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityOverlayInsecure()
{
        GameUI gui{};
        gui.setConnectionSecurity(ConnectionSecurity::Insecure);

        // When insecure, overlay should be visible
        UASSERT(gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityOverlayEncrypted()
{
        GameUI gui{};
        gui.setConnectionSecurity(ConnectionSecurity::Encrypted);

        // When encrypted, overlay should NOT be visible
        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityOverlaySingleplayer()
{
        GameUI gui{};
        gui.setSingleplayerMode(true);
        gui.setConnectionSecurity(ConnectionSecurity::Insecure);

        // In singleplayer mode, even if connection is marked insecure,
        // the overlay should NOT show (local connection is inherently secure)
        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityOverlaySingleplayerInsecure()
{
        GameUI gui{};
        gui.setSingleplayerMode(true);
        gui.setConnectionSecurity(ConnectionSecurity::Encrypted);

        // Singleplayer + encrypted should also not show overlay
        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSetConnectionSecurity()
{
        GameUI gui{};

        // Verify initial state
        UASSERT(gui.getConnectionSecurity() == ConnectionSecurity::Insecure);

        // Change to encrypted
        gui.setConnectionSecurity(ConnectionSecurity::Encrypted);
        UASSERT(gui.getConnectionSecurity() == ConnectionSecurity::Encrypted);
        UASSERT(!gui.shouldShowSecurityOverlay());

        // Change back to insecure
        gui.setConnectionSecurity(ConnectionSecurity::Insecure);
        UASSERT(gui.getConnectionSecurity() == ConnectionSecurity::Insecure);
        UASSERT(gui.shouldShowSecurityOverlay());

        // Test setting full security info
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.forward_secrecy = true;
        info.replay_protection = true;
        gui.setConnectionSecurityInfo(info);
        UASSERT(gui.getConnectionSecurityInfo().isSecure());
        UASSERT(gui.getConnectionSecurityInfo().encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM);
        UASSERT(gui.getConnectionSecurityInfo().forward_secrecy);
}

void TestGameUI::testSecurityOverlaySettingDisabled()
{
        // When the user disables the security overlay via the settings checkbox,
        // shouldShowSecurityOverlay() must return false even on insecure connections.
        GameUI gui{};
        gui.setConnectionSecurity(ConnectionSecurity::Insecure);

        // Default: show_security_overlay flag is true, overlay shows
        UASSERT(gui.shouldShowSecurityOverlay());

        // Simulate user unchecking the "Show security overlay" checkbox
        gui.m_flags.show_security_overlay = false;

        // Now overlay should NOT show, even though connection is insecure
        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityOverlaySettingEnabled()
{
        // When the setting is enabled (default), overlay shows on insecure connections.
        GameUI gui{};
        gui.setConnectionSecurity(ConnectionSecurity::Insecure);
        gui.m_flags.show_security_overlay = true;

        UASSERT(gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityOverlaySettingOverridesInsecure()
{
        // The setting should override even an insecure connection.
        // This tests that the user's preference takes priority.
        GameUI gui{};
        gui.setConnectionSecurity(ConnectionSecurity::Insecure);
        gui.m_flags.show_security_overlay = false;

        // Setting disabled → no overlay, even on insecure connection
        UASSERT(!gui.shouldShowSecurityOverlay());

        // Re-enable → overlay shows again
        gui.m_flags.show_security_overlay = true;
        UASSERT(gui.shouldShowSecurityOverlay());

        // Setting enabled but connection is secure → still no overlay
        gui.setConnectionSecurity(ConnectionSecurity::Encrypted);
        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testConnectionInfoFlagDefault()
{
        // The show_connection_info flag should default to false
        GameUI gui{};
        UASSERT(!gui.getFlags().show_connection_info);
}

void TestGameUI::testOverlayFlagDefaults()
{
        // Verify all overlay flags have correct defaults in the Flags struct.
        // This is the "extensible overlay pattern" — each overlay checkbox
        // maps to a flag here. When adding new overlays, add a test here too.
        GameUI::Flags flags{};

        // Security overlay: default ON (warning shown by default)
        UASSERT(flags.show_security_overlay);

        // Connection info in debug: default OFF (opt-in)
        UASSERT(!flags.show_connection_info);

        // Existing flags should still have their original defaults
        UASSERT(flags.show_chat);
        UASSERT(flags.show_hud);
        UASSERT(!flags.show_minimal_debug);
        UASSERT(!flags.show_basic_debug);
        UASSERT(!flags.show_profiler_graph);
}

// --- v8: Security info tab tests ---

void TestGameUI::testSecurityInfoDefaultNotConnected()
{
        // Before connecting, security info should reflect "not connected" state
        GameUI gui{};
        const auto& info = gui.getConnectionSecurityInfo();

        // Default state is Insecure (not yet connected)
        UASSERT(info.state == ConnectionSecurity::Insecure);
        UASSERT(info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);
        UASSERT(info.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_NONE);
        UASSERT(info.authentication == ConnectionSecurityInfo::AUTH_NONE);
        UASSERT(info.cipher_suite == ConnectionSecurityInfo::CIPHER_NONE);
        UASSERT(info.certificate_status == ConnectionSecurityInfo::CERT_NOT_VERIFIED);
        UASSERT(!info.forward_secrecy);
        UASSERT(!info.replay_protection);
        UASSERT(info.protocol_version == 0);
        UASSERT(info.server_address.empty());
        UASSERT(info.server_port == 0);
}

void TestGameUI::testSecurityInfoSetAndGetFullInfo()
{
        // Setting full security info should be retrievable field-by-field
        GameUI gui{};
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;
        info.forward_secrecy = true;
        info.replay_protection = true;
        info.protocol_version = 42;
        info.server_address = "game.example.com";
        info.server_port = 30000;

        gui.setConnectionSecurityInfo(info);

        const auto& retrieved = gui.getConnectionSecurityInfo();
        UASSERT(retrieved.state == ConnectionSecurity::Encrypted);
        UASSERT(retrieved.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM);
        UASSERT(retrieved.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519);
        UASSERT(retrieved.authentication == ConnectionSecurityInfo::AUTH_SRP);
        UASSERT(retrieved.cipher_suite == ConnectionSecurityInfo::CIPHER_AES_256_GCM);
        UASSERT(retrieved.certificate_status == ConnectionSecurityInfo::CERT_VERIFIED);
        UASSERT(retrieved.forward_secrecy);
        UASSERT(retrieved.replay_protection);
        UASSERT(retrieved.protocol_version == 42);
        UASSERT(retrieved.server_address == "game.example.com");
        UASSERT(retrieved.server_port == 30000);
}

void TestGameUI::testSecurityInfoStateTransitionOnConnect()
{
        // Simulate connecting to a secure server: state transitions from
        // Insecure (default) to Encrypted (after SRP auth completes).
        // v9.1 honest: connectionSecurityInfoFromFlags() does NOT set Encrypted,
        // so we use populateRealSecurityInfo() for the real encrypted state.
        GameUI gui{};

        // Initially insecure
        UASSERT(gui.getConnectionSecurity() == ConnectionSecurity::Insecure);
        UASSERT(!gui.getConnectionSecurityInfo().isSecure());

        // v9.1: Using populateRealSecurityInfo for honest security info
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   /* encryption_active */
                true,   /* ecdh_completed */
                true,   /* fingerprint_pinned */
                1,      /* fingerprint_verify_result = match */
                "session-abc", "SHA256:def", 0, 44, "secure.example.com", 30000);

        gui.setConnectionSecurityInfo(info);

        // Now encrypted
        UASSERT(gui.getConnectionSecurity() == ConnectionSecurity::Encrypted);
        UASSERT(gui.getConnectionSecurityInfo().isSecure());
        UASSERT(gui.getConnectionSecurityInfo().forward_secrecy);
        UASSERT(gui.getConnectionSecurityInfo().replay_protection);
        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityInfoResetOnDisconnect()
{
        // After disconnecting, security info should reset to insecure defaults
        GameUI gui{};

        // First, establish a secure connection state
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.protocol_version = 44;
        info.server_address = "game.example.com";
        info.server_port = 30000;
        gui.setConnectionSecurityInfo(info);

        UASSERT(gui.getConnectionSecurityInfo().isSecure());

        // Simulate disconnect by resetting security info
        gui.setConnectionSecurityInfo(ConnectionSecurityInfo{});

        // Should be back to insecure defaults
        const auto& reset_info = gui.getConnectionSecurityInfo();
        UASSERT(reset_info.state == ConnectionSecurity::Insecure);
        UASSERT(reset_info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);
        UASSERT(reset_info.protocol_version == 0);
        UASSERT(reset_info.server_address.empty());
        UASSERT(gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityInfoEncryptedIndicatorLogic()
{
        // When encrypted, the overlay should NOT show (no "INSECURE" banner)
        // But the debug line should show "Secure" when show_connection_info is on
        GameUI gui{};

        // Insecure → overlay shows, debug says "Insecure"
        UASSERT(gui.shouldShowSecurityOverlay());
        UASSERT(!gui.getConnectionSecurityInfo().isSecure());

        // Encrypted → overlay hidden, debug says "Secure"
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        gui.setConnectionSecurityInfo(info);

        UASSERT(!gui.shouldShowSecurityOverlay());
        UASSERT(gui.getConnectionSecurityInfo().isSecure());
        UASSERT(gui.getConnectionSecurityInfo().getStateString() == "Encrypted");
}

void TestGameUI::testSecurityInfoOverlayNotShownWhenEncrypted()
{
        // Double-check: encrypted connection + overlay setting enabled = no overlay
        GameUI gui{};
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        gui.setConnectionSecurityInfo(info);
        gui.m_flags.show_security_overlay = true;

        UASSERT(!gui.shouldShowSecurityOverlay());
}

void TestGameUI::testSecurityInfoStringConversionsComplete()
{
        // Verify all string conversion methods work and return non-empty strings
        // for the values used in the security info tab UI
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;

        // All string methods should return meaningful (non-empty) strings
        UASSERT(!info.getStateString().empty());
        UASSERT(!info.getEncryptionString().empty());
        UASSERT(!info.getKeyExchangeString().empty());
        UASSERT(!info.getAuthenticationString().empty());
        UASSERT(!info.getCipherSuiteString().empty());
        UASSERT(!info.getCertificateStatusString().empty());

        // Specific expected values
        UASSERT(info.getStateString() == "Encrypted");
        UASSERT(info.getEncryptionString() == "AES-256-GCM");
        UASSERT(info.getKeyExchangeString() == "ECDH (X25519)");
        UASSERT(info.getAuthenticationString() == "SRP");
        UASSERT(info.getCipherSuiteString() == "AES-256-GCM");
        UASSERT(info.getCertificateStatusString() == "Verified");
}
