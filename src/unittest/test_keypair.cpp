// Luanti-Secure
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti-Secure contributors

#include "test.h"

#include "util/keypair.h"
#include "util/auth.h"
#include "util/base64.h"
#include <filesystem>
#include <fstream>

class TestKeypair : public TestBase
{
public:
        TestKeypair() { TestManager::registerTestModule(this); }
        const char *getName() override { return "TestKeypair"; }

        void runTests(IGameDef *gamedef) override
        {
                TEST(testKeypairGeneration);
                TEST(testSignVerifyCycle);
                TEST(testKeypairPersistence);
                TEST(testPublicKeyEncoding);
                TEST(testChallengeGeneration);
                TEST(testPerServerUsernameMemory);
                TEST(testSignatureFailsWithWrongKey);
                TEST(testSignatureFailsWithTamperedMessage);
                TEST(testRegenerateKeypair);
                TEST(testKeypairFileExists);
                TEST(testGetServerUserList);
                TEST(testForgetServerUser);
                TEST(testServerEntryMetadata);
                TEST(testLegacyServerUserFormat);
                // v9.37: Per-username keypair tests
                TEST(testPerUsernameKeypairGeneration);
                TEST(testDifferentUsernamesDifferentKeys);
                TEST(testSwitchUsernameReloadsKeypair);
                TEST(testPerUsernameSignVerify);
                TEST(testListKeypairs);
                TEST(testDeleteKeypair);
                TEST(testGetPublicKeyBase64ForUser);
                TEST(testLegacyKeypairMigration);
                TEST(testSanitizeUsername);
                // v9.41: Server name in history entries
                TEST(testServerEntryServerName);
                TEST(testServerNamePersistence);
                TEST(testLegacyFormatNoServerName);
                TEST(testRememberServerUserWithName);
                TEST(testUpdateServerNameOnRevisit);
                TEST(testGetHistoryList);
        }

private:
        void testKeypairGeneration()
        {
                // Create a temporary directory for the keypair
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                UASSERT(km.ensureKeypair("testuser"));
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "testuser");

                std::string pubkey = km.getPublicKey();
                UASSERT(pubkey.size() == ED25519_PUBLIC_KEY_SIZE);

                std::string pubkey_b64 = km.getPublicKeyBase64();
                UASSERT(!pubkey_b64.empty());
                UASSERT(base64_is_valid(pubkey_b64));

                // Clean up
                std::filesystem::remove_all(tmpdir);
        }

        void testSignVerifyCycle()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_sign_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                UASSERT(km.ensureKeypair("signuser"));

                std::string message = "Hello, world!";
                std::string signature = km.sign(message);
                UASSERT(signature.size() == ED25519_SIGNATURE_SIZE);

                std::string pubkey = km.getPublicKey();
                UASSERT(KeypairManager::verify(pubkey, message, signature));

                std::filesystem::remove_all(tmpdir);
        }

        void testKeypairPersistence()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_persist_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                // Generate and save
                {
                        KeypairManager km(tmpdir);
                        UASSERT(km.ensureKeypair("persistuser"));
                }

                // Load from disk
                KeypairManager km2(tmpdir);
                UASSERT(km2.ensureKeypair("persistuser"));
                UASSERT(km2.hasKeypair());
                UASSERT(km2.getCurrentUsername() == "persistuser");

                // Sign with loaded keypair
                std::string message = "Persistence test";
                std::string signature = km2.sign(message);
                UASSERT(!signature.empty());
                UASSERT(KeypairManager::verify(km2.getPublicKey(), message, signature));

                std::filesystem::remove_all(tmpdir);
        }

        void testPublicKeyEncoding()
        {
                // Test the #2# encoding format
                std::string raw_pubkey(ED25519_PUBLIC_KEY_SIZE, 'A');
                std::string encoded = encode_keypair_pubkey(raw_pubkey);

                UASSERT(is_keypair_auth(encoded));
                UASSERT(encoded.substr(0, 3) == "#2#");

                std::string decoded;
                UASSERT(decode_keypair_pubkey(encoded, &decoded));
                UASSERT(decoded == raw_pubkey);

                // Test that SRP encoding is not keypair auth
                std::string srp_encoded = "#1#salt#verifier";
                UASSERT(!is_keypair_auth(srp_encoded));
        }

        void testChallengeGeneration()
        {
                std::string nonce1 = KeypairManager::generateChallenge();
                std::string nonce2 = KeypairManager::generateChallenge();

                UASSERT(nonce1.size() == KEYPAIR_CHALLENGE_SIZE);
                UASSERT(nonce2.size() == KEYPAIR_CHALLENGE_SIZE);
                // Two challenges should be different (cryptographically random)
                UASSERT(nonce1 != nonce2);
        }

        void testPerServerUsernameMemory()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_users_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("memuser");

                // Remember a username for a server
                km.rememberServerUser("example.com:30000", "player1");

                UASSERT(km.hasServerUser("example.com:30000"));
                UASSERT(km.getServerUser("example.com:30000") == "player1");

                // Different server should have no user yet
                UASSERT(!km.hasServerUser("other.com:30000"));
                UASSERT(km.getServerUser("other.com:30000") == "");

                // Remember another server
                km.rememberServerUser("other.com:30000", "player2");
                UASSERT(km.getServerUser("other.com:30000") == "player2");

                // Test persistence across reloads
                {
                        KeypairManager km2(tmpdir);
                        km2.ensureKeypair("memuser");
                        UASSERT(km2.getServerUser("example.com:30000") == "player1");
                        UASSERT(km2.getServerUser("other.com:30000") == "player2");
                }

                std::filesystem::remove_all(tmpdir);
        }

        void testSignatureFailsWithWrongKey()
        {
                std::string tmpdir1 = std::filesystem::temp_directory_path().string() + "/keypair_test_wrong1_" + std::to_string(::getpid());
                std::string tmpdir2 = std::filesystem::temp_directory_path().string() + "/keypair_test_wrong2_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir1);
                fs::CreateDir(tmpdir2);

                KeypairManager km1(tmpdir1);
                KeypairManager km2(tmpdir2);
                km1.ensureKeypair("user1");
                km2.ensureKeypair("user2");

                std::string message = "Test message";
                std::string signature = km1.sign(message);

                // Verify with the WRONG public key should fail
                std::string wrong_pubkey = km2.getPublicKey();
                UASSERT(!KeypairManager::verify(wrong_pubkey, message, signature));

                // Verify with the CORRECT public key should succeed
                std::string correct_pubkey = km1.getPublicKey();
                UASSERT(KeypairManager::verify(correct_pubkey, message, signature));

                std::filesystem::remove_all(tmpdir1);
                std::filesystem::remove_all(tmpdir2);
        }

        void testSignatureFailsWithTamperedMessage()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_tamper_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("tamperuser");

                std::string message = "Original message";
                std::string signature = km.sign(message);

                // Tampered message should fail verification
                std::string tampered = "Tampered message";
                UASSERT(!KeypairManager::verify(km.getPublicKey(), tampered, signature));

                // Original message should still pass
                UASSERT(KeypairManager::verify(km.getPublicKey(), message, signature));

                std::filesystem::remove_all(tmpdir);
        }

        void testRegenerateKeypair()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_regen_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                UASSERT(km.ensureKeypair("regenuser"));
                std::string old_pubkey = km.getPublicKey();
                std::string old_pubkey_b64 = km.getPublicKeyBase64();
                UASSERT(!old_pubkey.empty());

                // Sign a message with the old key
                std::string message = "Before regeneration";
                std::string old_signature = km.sign(message);
                UASSERT(!old_signature.empty());

                // Regenerate
                UASSERT(km.regenerateKeypair());
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "regenuser");

                // New key should be different
                std::string new_pubkey = km.getPublicKey();
                UASSERT(new_pubkey != old_pubkey);
                UASSERT(km.getPublicKeyBase64() != old_pubkey_b64);

                // Old signature should NOT verify with new key
                UASSERT(!KeypairManager::verify(new_pubkey, message, old_signature));

                // New key should sign correctly
                std::string new_signature = km.sign(message);
                UASSERT(!new_signature.empty());
                UASSERT(KeypairManager::verify(new_pubkey, message, new_signature));

                std::filesystem::remove_all(tmpdir);
        }

        void testKeypairFileExists()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_exists_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // No keypair file yet
                UASSERT(!km.keypairFileExists("newuser"));

                // After generating, file should exist
                UASSERT(km.ensureKeypair("newuser"));
                UASSERT(km.keypairFileExists("newuser"));

                std::filesystem::remove_all(tmpdir);
        }

        void testGetServerUserList()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_list_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("listuser");

                // Empty list initially
                auto list = km.getServerUserList();
                UASSERT(list.empty());

                // Add some servers
                km.rememberServerUser("server1.com:30000", "player1");
                km.rememberServerUser("server2.com:30000", "player2");
                km.rememberServerUser("server3.com:30000", "player3");

                list = km.getServerUserList();
                UASSERT(list.size() == 3);

                // Check entries exist (order may vary due to std::map)
                // v9.35: ServerEntry now has username, created_at, last_used_at
                bool found1 = false, found2 = false, found3 = false;
                for (const auto &[server, entry] : list) {
                        if (server == "server1.com:30000" && entry.username == "player1") found1 = true;
                        if (server == "server2.com:30000" && entry.username == "player2") found2 = true;
                        if (server == "server3.com:30000" && entry.username == "player3") found3 = true;
                        // Verify metadata timestamps are set
                        UASSERT(!entry.created_at.empty());
                        UASSERT(!entry.last_used_at.empty());
                }
                UASSERT(found1 && found2 && found3);

                std::filesystem::remove_all(tmpdir);
        }

        void testForgetServerUser()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_forget_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("forgetuser");

                km.rememberServerUser("keep.com:30000", "keep_player");
                km.rememberServerUser("forget.com:30000", "forget_player");

                UASSERT(km.hasServerUser("keep.com:30000"));
                UASSERT(km.hasServerUser("forget.com:30000"));

                // Forget one server
                UASSERT(km.forgetServerUser("forget.com:30000"));

                // Forgotten server should be gone
                UASSERT(!km.hasServerUser("forget.com:30000"));
                UASSERT(km.getServerUser("forget.com:30000") == "");

                // Other server should still be there
                UASSERT(km.hasServerUser("keep.com:30000"));
                UASSERT(km.getServerUser("keep.com:30000") == "keep_player");

                // Forgetting a non-existent server should return false
                UASSERT(!km.forgetServerUser("nonexistent.com:30000"));

                // Test persistence of forget operation
                {
                        KeypairManager km2(tmpdir);
                        km2.ensureKeypair("forgetuser");
                        UASSERT(!km2.hasServerUser("forget.com:30000"));
                        UASSERT(km2.hasServerUser("keep.com:30000"));
                }

                std::filesystem::remove_all(tmpdir);
        }

        void testServerEntryMetadata()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_meta_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("metauser");

                // Remember a server - this should set both created_at and last_used_at
                km.rememberServerUser("meta.com:30000", "metauser");
                auto list = km.getServerUserList();
                UASSERT(list.size() == 1);
                UASSERT(list[0].first == "meta.com:30000");
                UASSERT(list[0].second.username == "metauser");
                UASSERT(!list[0].second.created_at.empty());
                UASSERT(!list[0].second.last_used_at.empty());

                // First time: created_at and last_used_at should be the same
                std::string first_created = list[0].second.created_at;
                std::string first_last_used = list[0].second.last_used_at;

                // Re-remember the same server - should update last_used_at but preserve created_at
                km.rememberServerUser("meta.com:30000", "metauser2");
                list = km.getServerUserList();
                UASSERT(list[0].second.username == "metauser2");
                UASSERT(list[0].second.created_at == first_created); // preserved
                // last_used_at may or may not have changed (depends on timing)
                UASSERT(!list[0].second.last_used_at.empty());

                std::filesystem::remove_all(tmpdir);
        }

        void testLegacyServerUserFormat()
        {
                // v9.35: Test backward compatibility with the v9.29-v9.34 format
                // where server_users was {"server:port": "username"} instead of
                // {"server:port": {"username": "...", "created_at": "...", "last_used_at": "..."}}
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_legacy_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                // Write a legacy-format JSON file
                std::string filepath = tmpdir + "/keypair_server_users.json";
                std::ofstream file(filepath);
                file << "{\"legacy.com:30000\":\"olduser\",\"another.com:30000\":\"anotheruser\"}";
                file.close();

                // Load it with KeypairManager
                KeypairManager km(tmpdir);
                km.ensureKeypair("legacyuser");

                // Should be able to read the legacy format
                UASSERT(km.hasServerUser("legacy.com:30000"));
                UASSERT(km.getServerUser("legacy.com:30000") == "olduser");
                UASSERT(km.getServerUser("another.com:30000") == "anotheruser");

                // Check that the entries were loaded as ServerEntry structs
                auto list = km.getServerUserList();
                UASSERT(list.size() == 2);

                // Legacy entries should have empty timestamps
                for (const auto &[server, entry] : list) {
                        UASSERT(entry.username != "");
                        // created_at and last_used_at should be empty for legacy entries
                        // (they were not in the original format)
                }

                // After remembering, it should upgrade to the new format
                km.rememberServerUser("legacy.com:30000", "olduser");
                list = km.getServerUserList();
                for (const auto &[server, entry] : list) {
                        if (server == "legacy.com:30000") {
                                UASSERT(entry.username == "olduser");
                                // Now timestamps should be set since rememberServerUser was called
                                UASSERT(!entry.last_used_at.empty());
                        }
                }

                std::filesystem::remove_all(tmpdir);
        }

        // ======================================================================
        // v9.37: Per-username keypair tests
        // ======================================================================

        void testPerUsernameKeypairGeneration()
        {
                // Each username should get its own keypair file
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_peruser_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // Generate keypair for "warrior"
                UASSERT(km.ensureKeypair("warrior"));
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "warrior");
                std::string warrior_pubkey = km.getPublicKey();
                UASSERT(warrior_pubkey.size() == ED25519_PUBLIC_KEY_SIZE);

                // The file should exist under the keys directory
                UASSERT(km.keypairFileExists("warrior"));

                std::filesystem::remove_all(tmpdir);
        }

        void testDifferentUsernamesDifferentKeys()
        {
                // Different usernames must have different keypairs
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_diffkeys_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // Generate keypair for "warrior"
                UASSERT(km.ensureKeypair("warrior"));
                std::string warrior_pubkey = km.getPublicKey();

                // Switch to "builder" — should get a different keypair
                UASSERT(km.ensureKeypair("builder"));
                std::string builder_pubkey = km.getPublicKey();

                // Different usernames must produce different public keys
                UASSERT(warrior_pubkey != builder_pubkey);

                // Both files should exist
                UASSERT(km.keypairFileExists("warrior"));
                UASSERT(km.keypairFileExists("builder"));

                // Current user should be "builder"
                UASSERT(km.getCurrentUsername() == "builder");

                std::filesystem::remove_all(tmpdir);
        }

        void testSwitchUsernameReloadsKeypair()
        {
                // Switching between usernames should reload the correct keypair
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_switch_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // Generate keypairs for two users
                UASSERT(km.ensureKeypair("alice"));
                std::string alice_pubkey = km.getPublicKey();
                std::string alice_sig = km.sign("hello");

                UASSERT(km.ensureKeypair("bob"));
                std::string bob_pubkey = km.getPublicKey();
                std::string bob_sig = km.sign("hello");

                // Keys must be different
                UASSERT(alice_pubkey != bob_pubkey);

                // Switch back to alice
                UASSERT(km.ensureKeypair("alice"));
                std::string alice_pubkey2 = km.getPublicKey();
                UASSERT(alice_pubkey == alice_pubkey2);

                // Verify alice's signature with alice's public key
                UASSERT(KeypairManager::verify(alice_pubkey, "hello", alice_sig));
                // Bob's signature should NOT verify with alice's key
                UASSERT(!KeypairManager::verify(alice_pubkey, "hello", bob_sig));

                // Switch back to bob
                UASSERT(km.ensureKeypair("bob"));
                std::string bob_pubkey2 = km.getPublicKey();
                UASSERT(bob_pubkey == bob_pubkey2);
                // Bob's signature verifies with bob's key
                UASSERT(KeypairManager::verify(bob_pubkey, "hello", bob_sig));

                std::filesystem::remove_all(tmpdir);
        }

        void testPerUsernameSignVerify()
        {
                // Each username can sign and verify independently
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_persign_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // Generate keypairs for two characters on the same server
                UASSERT(km.ensureKeypair("warrior"));
                std::string warrior_pubkey = km.getPublicKey();
                std::string warrior_sig = km.sign("challenge_nonce");

                UASSERT(km.ensureKeypair("builder"));
                std::string builder_pubkey = km.getPublicKey();
                std::string builder_sig = km.sign("challenge_nonce");

                // Each user's signature verifies with their own key
                UASSERT(KeypairManager::verify(warrior_pubkey, "challenge_nonce", warrior_sig));
                UASSERT(KeypairManager::verify(builder_pubkey, "challenge_nonce", builder_sig));

                // Cross-verification must fail
                UASSERT(!KeypairManager::verify(warrior_pubkey, "challenge_nonce", builder_sig));
                UASSERT(!KeypairManager::verify(builder_pubkey, "challenge_nonce", warrior_sig));

                std::filesystem::remove_all(tmpdir);
        }

        void testListKeypairs()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_listkp_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // Initially, no keypairs
                auto keypairs = km.listKeypairs();
                UASSERT(keypairs.empty());

                // Generate keypairs for several users
                km.ensureKeypair("alpha");
                km.ensureKeypair("beta");
                km.ensureKeypair("gamma");

                // List should contain all three
                keypairs = km.listKeypairs();
                UASSERT(keypairs.size() == 3);

                // Check all three are present (order may vary)
                bool found_alpha = false, found_beta = false, found_gamma = false;
                for (const auto &name : keypairs) {
                        if (name == "alpha") found_alpha = true;
                        if (name == "beta") found_beta = true;
                        if (name == "gamma") found_gamma = true;
                }
                UASSERT(found_alpha && found_beta && found_gamma);

                std::filesystem::remove_all(tmpdir);
        }

        void testDeleteKeypair()
        {
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_deletekp_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("deleteme");
                km.ensureKeypair("keepme");

                UASSERT(km.keypairFileExists("deleteme"));
                UASSERT(km.keypairFileExists("keepme"));

                // Delete one keypair
                UASSERT(km.deleteKeypair("deleteme"));
                UASSERT(!km.keypairFileExists("deleteme"));
                UASSERT(km.keypairFileExists("keepme"));

                // The deleted keypair should not appear in the list
                auto keypairs = km.listKeypairs();
                bool found_delete = false, found_keep = false;
                for (const auto &name : keypairs) {
                        if (name == "deleteme") found_delete = true;
                        if (name == "keepme") found_keep = true;
                }
                UASSERT(!found_delete);
                UASSERT(found_keep);

                // Deleting non-existent should return false
                UASSERT(!km.deleteKeypair("nonexistent"));

                // Deleting the currently loaded keypair should unload it
                km.ensureKeypair("keepme");
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "keepme");
                km.deleteKeypair("keepme");
                UASSERT(!km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "");

                std::filesystem::remove_all(tmpdir);
        }

        void testGetPublicKeyBase64ForUser()
        {
                // Should be able to get a user's pubkey without switching the current keypair
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_getpk_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("userA");
                std::string userA_pubkey_b64 = km.getPublicKeyBase64();

                km.ensureKeypair("userB");
                std::string userB_pubkey_b64 = km.getPublicKeyBase64();

                // Now userB is loaded. Get userA's pubkey without switching.
                std::string userA_pubkey_b64_via_lookup = km.getPublicKeyBase64ForUser("userA");
                UASSERT(userA_pubkey_b64_via_lookup == userA_pubkey_b64);

                // Current keypair should still be userB
                UASSERT(km.getCurrentUsername() == "userB");
                UASSERT(km.getPublicKeyBase64() == userB_pubkey_b64);

                // Non-existent user should return empty
                UASSERT(km.getPublicKeyBase64ForUser("nonexistent") == "");

                std::filesystem::remove_all(tmpdir);
        }

        void testLegacyKeypairMigration()
        {
                // v9.37: If the old client_ed25519_key file exists and a
                // username-specific key doesn't, the legacy file should be
                // migrated to that username.
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_migrate_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                // Create a legacy keypair file manually
                KeypairManager km_legacy(tmpdir);
                UASSERT(km_legacy.ensureKeypair("original_user"));
                // Copy the per-username file to the legacy location
                std::string src_path = tmpdir + "/client_ed25519_keys/original_user.key";
                std::string dst_path = tmpdir + "/client_ed25519_key";
                {
                        std::ifstream src(src_path, std::ios::binary);
                        std::ofstream dst(dst_path, std::ios::binary);
                        dst << src.rdbuf();
                }
                // Remove the per-username directory to simulate pre-v9.37 state
                std::filesystem::remove_all(tmpdir + "/client_ed25519_keys");

                // Now, a fresh KeypairManager should migrate the legacy key
                // when ensureKeypair is called for "migrated_user"
                KeypairManager km(tmpdir);
                UASSERT(km.ensureKeypair("migrated_user"));
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "migrated_user");

                // The per-username file should now exist
                UASSERT(km.keypairFileExists("migrated_user"));

                // The legacy file should have been deleted after migration
                UASSERT(!fs::PathExists(dst_path));

                // The migrated key should be the same as the original
                UASSERT(km.getPublicKey().size() == ED25519_PUBLIC_KEY_SIZE);

                std::filesystem::remove_all(tmpdir);
        }

        void testSanitizeUsername()
        {
                // Usernames with special characters should be sanitized for filenames
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_sanitize_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);

                // Normal username should work fine
                UASSERT(km.ensureKeypair("normal_user123"));
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "normal_user123");

                // Username with special chars — only safe chars should be used for filename
                // but the current_username should still be the original
                UASSERT(km.ensureKeypair("user@example"));
                UASSERT(km.hasKeypair());
                UASSERT(km.getCurrentUsername() == "user@example");

                // Both should be stored as separate files
                UASSERT(km.keypairFileExists("normal_user123"));
                UASSERT(km.keypairFileExists("user@example"));

                std::filesystem::remove_all(tmpdir);
        }

        // ======================================================================
        // v9.41: Server name in history entries
        // ======================================================================

        void testServerEntryServerName()
        {
                // v9.41: ServerEntry should have a server_name field
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_sname_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("histuser");

                // Remember a server WITH a name
                km.rememberServerUser("myserver.com:30000", "histuser", "My Awesome Server");

                auto list = km.getServerUserList();
                UASSERT(list.size() == 1);
                UASSERT(list[0].first == "myserver.com:30000");
                UASSERT(list[0].second.username == "histuser");
                UASSERT(list[0].second.server_name == "My Awesome Server");
                UASSERT(!list[0].second.created_at.empty());
                UASSERT(!list[0].second.last_used_at.empty());

                std::filesystem::remove_all(tmpdir);
        }

        void testServerNamePersistence()
        {
                // server_name should survive reload from disk
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_snpersist_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                {
                        KeypairManager km(tmpdir);
                        km.ensureKeypair("persistuser");
                        km.rememberServerUser("game.example.net:30000", "persistuser", "Game Central");
                }

                // Reload from disk
                KeypairManager km2(tmpdir);
                km2.ensureKeypair("persistuser");
                auto list = km2.getServerUserList();
                UASSERT(list.size() == 1);
                UASSERT(list[0].second.server_name == "Game Central");
                UASSERT(list[0].second.username == "persistuser");

                std::filesystem::remove_all(tmpdir);
        }

        void testLegacyFormatNoServerName()
        {
                // v9.41: Loading a JSON file without server_name should not break
                // (backward compatibility with v9.35-v9.40 format)
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_legacy_sname_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                // Write v9.35 format JSON (no server_name field)
                std::string filepath = tmpdir + "/keypair_server_users.json";
                std::ofstream file(filepath);
                file << "{\"oldserver.com:30000\":{\"username\":\"oldplayer\",\"created_at\":\"2026-01-15T08:30:00Z\",\"last_used_at\":\"2026-03-20T14:45:00Z\"}}";
                file.close();

                KeypairManager km(tmpdir);
                km.ensureKeypair("legacyuser");

                UASSERT(km.hasServerUser("oldserver.com:30000"));
                auto list = km.getServerUserList();
                UASSERT(list.size() == 1);
                UASSERT(list[0].second.username == "oldplayer");
                // server_name should be empty for legacy entries
                UASSERT(list[0].second.server_name == "");
                UASSERT(list[0].second.created_at == "2026-01-15T08:30:00Z");

                std::filesystem::remove_all(tmpdir);
        }

        void testRememberServerUserWithName()
        {
                // v9.41: rememberServerUser with 3-param overload (including server_name)
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_remember_name_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("nameuser");

                // 2-param overload (backward compat — server_name should be empty)
                km.rememberServerUser("compat.com:30000", "compatuser");
                auto list = km.getServerUserList();
                bool found_compat = false;
                for (const auto &[server, entry] : list) {
                        if (server == "compat.com:30000") {
                                UASSERT(entry.username == "compatuser");
                                UASSERT(entry.server_name == "");
                                found_compat = true;
                        }
                }
                UASSERT(found_compat);

                // 3-param overload (with server_name)
                km.rememberServerUser("named.com:30000", "nameduser", "Named Server");
                list = km.getServerUserList();
                bool found_named = false;
                for (const auto &[server, entry] : list) {
                        if (server == "named.com:30000") {
                                UASSERT(entry.username == "nameduser");
                                UASSERT(entry.server_name == "Named Server");
                                found_named = true;
                        }
                }
                UASSERT(found_named);

                std::filesystem::remove_all(tmpdir);
        }

        void testUpdateServerNameOnRevisit()
        {
                // When revisiting a server, server_name should be updated if provided
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_sname_upd_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("upduser");

                // First visit: no server_name
                km.rememberServerUser("upd.com:30000", "upduser");
                auto list = km.getServerUserList();
                UASSERT(list[0].second.server_name == "");

                // Second visit: WITH server_name — should update
                km.rememberServerUser("upd.com:30000", "upduser", "Updated Server Name");
                list = km.getServerUserList();
                UASSERT(list[0].second.server_name == "Updated Server Name");
                // Username should still be the same
                UASSERT(list[0].second.username == "upduser");
                // created_at should be preserved (not overwritten)
                std::string original_created = list[0].second.created_at;

                // Third visit: different server_name — should update again
                km.rememberServerUser("upd.com:30000", "upduser", "Even Newer Name");
                list = km.getServerUserList();
                UASSERT(list[0].second.server_name == "Even Newer Name");
                UASSERT(list[0].second.created_at == original_created);

                std::filesystem::remove_all(tmpdir);
        }

        void testGetHistoryList()
        {
                // v9.41: getServerUserList should return full history with server_name
                std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_history_" + std::to_string(::getpid());
                fs::CreateDir(tmpdir);

                KeypairManager km(tmpdir);
                km.ensureKeypair("histuser");

                // Add multiple servers with names
                km.rememberServerUser("alpha.com:30000", "alphauser", "Alpha World");
                km.rememberServerUser("beta.com:30000", "betauser", "Beta Land");
                km.rememberServerUser("gamma.com:30000", "gammauser", ""); // empty name

                auto list = km.getServerUserList();
                UASSERT(list.size() == 3);

                // Verify each entry has the right data
                int found = 0;
                for (const auto &[server, entry] : list) {
                        if (server == "alpha.com:30000") {
                                UASSERT(entry.username == "alphauser");
                                UASSERT(entry.server_name == "Alpha World");
                                UASSERT(!entry.created_at.empty());
                                UASSERT(!entry.last_used_at.empty());
                                found++;
                        }
                        if (server == "beta.com:30000") {
                                UASSERT(entry.username == "betauser");
                                UASSERT(entry.server_name == "Beta Land");
                                found++;
                        }
                        if (server == "gamma.com:30000") {
                                UASSERT(entry.username == "gammauser");
                                UASSERT(entry.server_name == ""); // empty name
                                found++;
                        }
                }
                UASSERT(found == 3);

                std::filesystem::remove_all(tmpdir);
        }
};

static TestKeypair g_test_instance;
