// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for FingerprintStore — server fingerprint pinning
// and Trust-On-First-Use (TOFU) verification.
//
// These tests prove that:
// - FingerprintStore records and verifies server fingerprints
// - Unknown servers return 0 (TOFU — first connection)
// - Matching fingerprints return 1 (verified)
// - Mismatched fingerprints return -1 (possible MITM)
// - Fingerprints can be saved to and loaded from files
// - The store handles multiple servers correctly

#include "test.h"

#include "network/crypto.h"

#include <cstring>
#include <fstream>
#include <cstdio>

class TestFingerprintStore : public TestBase
{
public:
        TestFingerprintStore() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestFingerprintStore"; }

        void runTests(IGameDef *gamedef);

        // Basic operations
        void testRecordAndVerify();
        void testUnknownServerReturnsZero();
        void testMatchingFingerprintReturnsOne();
        void testMismatchedFingerprintReturnsMinusOne();
        void testGetStoredFingerprint();
        void testGetStoredFingerprintUnknown();
        void testMultipleServers();
        void testUpdateFingerprint();
        void testClear();
        void testSize();

        // File I/O
        void testSaveAndLoad();
        void testLoadNonexistentFile();
        void testLoadWithComments();
        void testSaveAndLoadMultiple();

        // Edge cases
        void testEmptyFingerprint();
        void testSameServerDifferentPorts();
};

static TestFingerprintStore g_test_instance;

void TestFingerprintStore::runTests(IGameDef *gamedef)
{
        TEST(testRecordAndVerify);
        TEST(testUnknownServerReturnsZero);
        TEST(testMatchingFingerprintReturnsOne);
        TEST(testMismatchedFingerprintReturnsMinusOne);
        TEST(testGetStoredFingerprint);
        TEST(testGetStoredFingerprintUnknown);
        TEST(testMultipleServers);
        TEST(testUpdateFingerprint);
        TEST(testClear);
        TEST(testSize);
        TEST(testSaveAndLoad);
        TEST(testLoadNonexistentFile);
        TEST(testLoadWithComments);
        TEST(testSaveAndLoadMultiple);
        TEST(testEmptyFingerprint);
        TEST(testSameServerDifferentPorts);
}

void TestFingerprintStore::testRecordAndVerify()
{
        FingerprintStore store;
        store.record("example.com", 30000, "SHA256:abc123");

        // The fingerprint should now be verified
        int result = store.verify("example.com", 30000, "SHA256:abc123");
        UASSERTEQ(int, result, 1);
}

void TestFingerprintStore::testUnknownServerReturnsZero()
{
        FingerprintStore store;
        // No fingerprints recorded — any server should be unknown
        int result = store.verify("unknown.com", 30000, "SHA256:xyz789");
        UASSERTEQ(int, result, 0);
}

void TestFingerprintStore::testMatchingFingerprintReturnsOne()
{
        FingerprintStore store;
        store.record("game.example.com", 30000, "SHA256:deadbeef");

        int result = store.verify("game.example.com", 30000, "SHA256:deadbeef");
        UASSERTEQ(int, result, 1);
}

void TestFingerprintStore::testMismatchedFingerprintReturnsMinusOne()
{
        FingerprintStore store;
        store.record("game.example.com", 30000, "SHA256:deadbeef");

        // Different fingerprint — possible MITM!
        int result = store.verify("game.example.com", 30000, "SHA256:badbeef");
        UASSERTEQ(int, result, -1);
}

void TestFingerprintStore::testGetStoredFingerprint()
{
        FingerprintStore store;
        store.record("myserver.com", 30000, "SHA256:aabbccdd");

        std::string stored = store.getStoredFingerprint("myserver.com", 30000);
        UASSERT(stored == "SHA256:aabbccdd");
}

void TestFingerprintStore::testGetStoredFingerprintUnknown()
{
        FingerprintStore store;
        std::string stored = store.getStoredFingerprint("unknown.com", 30000);
        UASSERT(stored.empty());
}

void TestFingerprintStore::testMultipleServers()
{
        FingerprintStore store;
        store.record("server1.com", 30000, "SHA256:aaa");
        store.record("server2.com", 30000, "SHA256:bbb");
        store.record("server3.com", 30000, "SHA256:ccc");

        UASSERTEQ(int, store.verify("server1.com", 30000, "SHA256:aaa"), 1);
        UASSERTEQ(int, store.verify("server2.com", 30000, "SHA256:bbb"), 1);
        UASSERTEQ(int, store.verify("server3.com", 30000, "SHA256:ccc"), 1);
        UASSERTEQ(int, store.verify("server4.com", 30000, "SHA256:ddd"), 0);
}

void TestFingerprintStore::testUpdateFingerprint()
{
        FingerprintStore store;
        store.record("server.com", 30000, "SHA256:old");
        store.record("server.com", 30000, "SHA256:new");

        // Should have the new fingerprint
        std::string stored = store.getStoredFingerprint("server.com", 30000);
        UASSERT(stored == "SHA256:new");
}

void TestFingerprintStore::testClear()
{
        FingerprintStore store;
        store.record("server.com", 30000, "SHA256:abc");
        UASSERTEQ(size_t, store.size(), 1u);

        store.clear();
        UASSERTEQ(size_t, store.size(), 0u);
}

void TestFingerprintStore::testSize()
{
        FingerprintStore store;
        UASSERTEQ(size_t, store.size(), 0u);

        store.record("a.com", 30000, "SHA256:a");
        UASSERTEQ(size_t, store.size(), 1u);

        store.record("b.com", 30000, "SHA256:b");
        UASSERTEQ(size_t, store.size(), 2u);
}

void TestFingerprintStore::testSaveAndLoad()
{
        FingerprintStore store1;
        store1.record("testserver.com", 30000, "SHA256:abc123def456");

        std::string filepath = "/tmp/test_fingerprint_store.txt";
        bool save_ok = store1.save(filepath);
        UASSERT(save_ok);

        FingerprintStore store2;
        bool load_ok = store2.load(filepath);
        UASSERT(load_ok);

        // Loaded store should have the same fingerprint
        std::string loaded = store2.getStoredFingerprint("testserver.com", 30000);
        UASSERT(loaded == "SHA256:abc123def456");

        // Cleanup
        std::remove(filepath.c_str());
}

void TestFingerprintStore::testLoadNonexistentFile()
{
        FingerprintStore store;
        bool load_ok = store.load("/tmp/nonexistent_file_12345.txt");
        // Loading a nonexistent file should succeed (first run)
        UASSERT(load_ok);
        UASSERTEQ(size_t, store.size(), 0u);
}

void TestFingerprintStore::testLoadWithComments()
{
        std::string filepath = "/tmp/test_fp_comments.txt";
        {
                std::ofstream file(filepath);
                file << "# This is a comment\n";
                file << "\n";
                file << "game.example.com:30000 SHA256:aaa\n";
                file << "# Another comment\n";
                file << "other.example.com:30000 SHA256:bbb\n";
        }

        FingerprintStore store;
        bool load_ok = store.load(filepath);
        UASSERT(load_ok);
        UASSERTEQ(size_t, store.size(), 2u);
        UASSERT(store.getStoredFingerprint("game.example.com", 30000) == "SHA256:aaa");
        UASSERT(store.getStoredFingerprint("other.example.com", 30000) == "SHA256:bbb");

        std::remove(filepath.c_str());
}

void TestFingerprintStore::testSaveAndLoadMultiple()
{
        FingerprintStore store1;
        store1.record("s1.com", 30000, "SHA256:111");
        store1.record("s2.com", 30001, "SHA256:222");
        store1.record("s3.com", 30002, "SHA256:333");

        std::string filepath = "/tmp/test_fp_multiple.txt";
        store1.save(filepath);

        FingerprintStore store2;
        store2.load(filepath);

        UASSERTEQ(size_t, store2.size(), 3u);
        UASSERTEQ(int, store2.verify("s1.com", 30000, "SHA256:111"), 1);
        UASSERTEQ(int, store2.verify("s2.com", 30001, "SHA256:222"), 1);
        UASSERTEQ(int, store2.verify("s3.com", 30002, "SHA256:333"), 1);

        std::remove(filepath.c_str());
}

void TestFingerprintStore::testEmptyFingerprint()
{
        FingerprintStore store;
        store.record("server.com", 30000, "");

        // An empty fingerprint should still be recorded
        std::string stored = store.getStoredFingerprint("server.com", 30000);
        UASSERT(stored.empty());
}

void TestFingerprintStore::testSameServerDifferentPorts()
{
        FingerprintStore store;
        store.record("server.com", 30000, "SHA256:port30000");
        store.record("server.com", 30001, "SHA256:port30001");

        UASSERT(store.getStoredFingerprint("server.com", 30000) == "SHA256:port30000");
        UASSERT(store.getStoredFingerprint("server.com", 30001) == "SHA256:port30001");

        // Verify each port independently
        UASSERTEQ(int, store.verify("server.com", 30000, "SHA256:port30000"), 1);
        UASSERTEQ(int, store.verify("server.com", 30001, "SHA256:port30001"), 1);
}
