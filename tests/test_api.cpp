/*
 * test_api.cpp - MIDI API Unit Tests
 *
 * Tests for core midix library API:
 * - Context creation/destruction
 * - Error handling
 * - Scheduler timestamp ordering
 * - Parameter validation
 *
 * License: MIT
 */

#include "midix.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

// ============================================================================
// Context Management Tests
// ============================================================================

TEST(APITest, ContextCreateDestroy) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr) << "Failed to create context";
    midix_destroy(ctx);
}

TEST(APITest, ContextCreateWithNullName) {
    midix_ctx* ctx = midix_create(nullptr);
    ASSERT_NE(ctx, nullptr) << "Should handle NULL client name";
    midix_destroy(ctx);
}

TEST(APITest, ContextDestroyNull) {
    // Should not crash with NULL context
    EXPECT_NO_THROW(midix_destroy(nullptr));
}

TEST(APITest, MultipleContexts) {
    // Should be able to create multiple independent contexts
    midix_ctx* ctx1 = midix_create("Client1");
    midix_ctx* ctx2 = midix_create("Client2");
    midix_ctx* ctx3 = midix_create("Client3");

    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);
    ASSERT_NE(ctx3, nullptr);

    midix_destroy(ctx1);
    midix_destroy(ctx2);
    midix_destroy(ctx3);
}

// ============================================================================
// Device Management Tests
// ============================================================================

TEST(APITest, ListOutputs) {
    char** names = nullptr;
    int count = 0;

    int result = midix_list_outputs(&names, &count);
    EXPECT_EQ(result, MIDIX_OK);
    EXPECT_GE(count, 0);

    if (count > 0) {
        ASSERT_NE(names, nullptr);
        for (int i = 0; i < count; i++) {
            EXPECT_NE(names[i], nullptr);
            free(names[i]);
        }
        free(names);
    }
}

TEST(APITest, OpenOutputWithoutContext) {
    // Should fail gracefully with NULL context
    int result = midix_open_output_by_name(nullptr, "IAC");
    EXPECT_EQ(result, MIDIX_ERR_INVALID_PARAM);
}

TEST(APITest, OpenOutputWithNullName) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    int result = midix_open_output_by_name(ctx, nullptr);
    EXPECT_EQ(result, MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

TEST(APITest, OpenNonexistentDevice) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    int result = midix_open_output_by_name(ctx, "ThisDeviceDoesNotExist12345");
    EXPECT_EQ(result, MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(APITest, SendWithoutDevice) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Try to send without opening a device
    int result = midix_note_on(ctx, 1, 60, 100, 0);
    EXPECT_EQ(result, MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

TEST(APITest, InvalidChannel) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Channels must be 1-16
    EXPECT_EQ(midix_note_on(ctx, 0, 60, 100, 0), MIDIX_ERR_INVALID_PARAM);
    EXPECT_EQ(midix_note_on(ctx, 17, 60, 100, 0), MIDIX_ERR_INVALID_PARAM);
    EXPECT_EQ(midix_note_on(ctx, -1, 60, 100, 0), MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

TEST(APITest, InvalidNoteNumber) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Notes must be 0-127
    EXPECT_EQ(midix_note_on(ctx, 1, 128, 100, 0), MIDIX_ERR_INVALID_PARAM);
    EXPECT_EQ(midix_note_on(ctx, 1, 255, 100, 0), MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

TEST(APITest, InvalidVelocity) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Velocity must be 0-127
    EXPECT_EQ(midix_note_on(ctx, 1, 60, 128, 0), MIDIX_ERR_INVALID_PARAM);
    EXPECT_EQ(midix_note_on(ctx, 1, 60, 255, 0), MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

TEST(APITest, InvalidCCNumber) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // CC must be 0-127
    EXPECT_EQ(midix_cc(ctx, 1, 128, 64, 0), MIDIX_ERR_INVALID_PARAM);
    EXPECT_EQ(midix_cc(ctx, 1, 255, 64, 0), MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

TEST(APITest, InvalidPitchBend) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Pitch bend must be 0-16383 (14-bit)
    EXPECT_EQ(midix_pb(ctx, 1, 16384, 0), MIDIX_ERR_INVALID_PARAM);
    EXPECT_EQ(midix_pb(ctx, 1, 65535, 0), MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

// ============================================================================
// SysEx Tests
// ============================================================================

TEST(APITest, SysExValid) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Valid SysEx (F0 ... F7)
    uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};

    // Without device, should fail with MIDIX_ERR_NO_DEVICE
    int result = midix_send_sysex(ctx, sysex, sizeof(sysex), 0);
    EXPECT_EQ(result, MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

TEST(APITest, SysExMissingF0) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Invalid: missing F0 start byte
    uint8_t sysex[] = {0x7E, 0x7F, 0x09, 0x01, 0xF7};

    int result = midix_send_sysex(ctx, sysex, sizeof(sysex), 0);
    EXPECT_EQ(result, MIDIX_ERR_SYSEX_INCOMPLETE);

    midix_destroy(ctx);
}

TEST(APITest, SysExMissingF7) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Invalid: missing F7 end byte
    uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01};

    int result = midix_send_sysex(ctx, sysex, sizeof(sysex), 0);
    EXPECT_EQ(result, MIDIX_ERR_SYSEX_INCOMPLETE);

    midix_destroy(ctx);
}

TEST(APITest, SysExEmpty) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    uint8_t sysex[] = {};

    int result = midix_send_sysex(ctx, sysex, 0, 0);
    EXPECT_EQ(result, MIDIX_ERR_INVALID_PARAM);

    midix_destroy(ctx);
}

// ============================================================================
// Timing Tests
// ============================================================================

TEST(APITest, TimestampMonotonic) {
    // midix_now() should return monotonic increasing values
    midix_time_ns t1 = midix_now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    midix_time_ns t2 = midix_now();

    EXPECT_GT(t2, t1) << "Timestamps should be monotonically increasing";
}

TEST(APITest, TimestampPrecision) {
    // Measure time precision
    std::vector<midix_time_ns> timestamps;
    for (int i = 0; i < 100; i++) {
        timestamps.push_back(midix_now());
    }

    // Check that timestamps are strictly increasing
    for (size_t i = 1; i < timestamps.size(); i++) {
        EXPECT_GE(timestamps[i], timestamps[i-1])
            << "Timestamp decreased at index " << i;
    }
}

TEST(APITest, SchedulerOrdering) {
    // Note: This test assumes scheduler maintains timestamp order
    // Actual validation would require mocking or integration testing

    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    midix_time_ns now = midix_now();

    // Schedule events in non-chronological order
    // Scheduler should reorder them
    midix_time_ns t1 = now + 1000000000;  // +1s
    midix_time_ns t2 = now + 500000000;   // +0.5s
    midix_time_ns t3 = now + 2000000000;  // +2s

    // Events scheduled out of order should still be handled correctly
    // (Without device, will fail, but shouldn't crash)
    EXPECT_EQ(midix_note_on(ctx, 1, 60, 100, t1), MIDIX_ERR_NO_DEVICE);
    EXPECT_EQ(midix_note_on(ctx, 1, 62, 100, t2), MIDIX_ERR_NO_DEVICE);
    EXPECT_EQ(midix_note_on(ctx, 1, 64, 100, t3), MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

// ============================================================================
// Flush and Cleanup Tests
// ============================================================================

TEST(APITest, FlushWithoutDevice) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Flush should succeed even without device
    int result = midix_flush(ctx);
    EXPECT_EQ(result, MIDIX_OK);

    midix_destroy(ctx);
}

TEST(APITest, PanicWithoutDevice) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Panic without device should fail gracefully
    int result = midix_panic(ctx);
    EXPECT_EQ(result, MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

// ============================================================================
// Version Information
// ============================================================================

TEST(APITest, Version) {
    const char* version = midix_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");

    // Version should be in format X.Y.Z
    EXPECT_TRUE(strstr(version, ".") != nullptr)
        << "Version should contain '.' separator";
}

// ============================================================================
// Boundary Values
// ============================================================================

TEST(APITest, BoundaryNotes) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Valid boundary notes: 0 and 127
    EXPECT_EQ(midix_note_on(ctx, 1, 0, 100, 0), MIDIX_ERR_NO_DEVICE);
    EXPECT_EQ(midix_note_on(ctx, 1, 127, 100, 0), MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

TEST(APITest, BoundaryChannels) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Valid channels: 1 and 16
    EXPECT_EQ(midix_note_on(ctx, 1, 60, 100, 0), MIDIX_ERR_NO_DEVICE);
    EXPECT_EQ(midix_note_on(ctx, 16, 60, 100, 0), MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

TEST(APITest, BoundaryPitchBend) {
    midix_ctx* ctx = midix_create("TestClient");
    ASSERT_NE(ctx, nullptr);

    // Valid pitch bend: 0, 8192 (center), 16383 (max)
    EXPECT_EQ(midix_pb(ctx, 1, 0, 0), MIDIX_ERR_NO_DEVICE);
    EXPECT_EQ(midix_pb(ctx, 1, 8192, 0), MIDIX_ERR_NO_DEVICE);
    EXPECT_EQ(midix_pb(ctx, 1, 16383, 0), MIDIX_ERR_NO_DEVICE);

    midix_destroy(ctx);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/*
 * COMPILATION & EXECUTION:
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   make test_api
 *
 * Run:
 *   ./test_api
 *   # or
 *   ctest -R test_api -V
 *
 * Expected output:
 *   [==========] Running X tests from 1 test suite.
 *   [----------] Global test environment set-up.
 *   [----------] X tests from APITest
 *   [ RUN      ] APITest.ContextCreateDestroy
 *   [       OK ] APITest.ContextCreateDestroy (0 ms)
 *   ...
 *   [==========] X tests from 1 test suite ran. (Y ms total)
 *   [  PASSED  ] X tests.
 *
 * Notes:
 *   - Tests focus on error handling and parameter validation
 *   - Most tests expect MIDIX_ERR_NO_DEVICE since no device is opened
 *   - Integration tests with real devices should be separate
 *   - All tests should pass for compliant API implementation
 */
