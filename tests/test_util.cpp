/*
 * test_util.cpp - Utility Function Unit Tests
 *
 * Tests for MIDI utility functions:
 * - Note name to MIDI number conversion
 * - OMC (Octave for Middle C) variations
 * - Boundary value validation
 *
 * License: MIT
 */

#include <gtest/gtest.h>
#include <string>
#include <cstdint>

// Forward declarations for utility functions
// These should be implemented in src/common/util.c
extern "C" {
    int midix_note_name_to_number(const char* name, int omc);
    const char* midix_note_number_to_name(int note, int omc, char* buffer, size_t bufsize);
    bool midix_is_valid_note(int note);
    bool midix_is_valid_cc(int cc);
    bool midix_is_valid_channel(int channel);
}

// ============================================================================
// Note Name Conversion Tests
// ============================================================================

TEST(UtilTest, NoteNameToNumberBasic) {
    // Standard note names with omc=3 (C4 = middle C)
    EXPECT_EQ(midix_note_name_to_number("C4", 3), 60);
    EXPECT_EQ(midix_note_name_to_number("A4", 3), 69);  // A440
    EXPECT_EQ(midix_note_name_to_number("G4", 3), 67);
}

TEST(UtilTest, NoteNameToNumberAllChromatic) {
    // All 12 chromatic notes in octave 4 (omc=3)
    EXPECT_EQ(midix_note_name_to_number("C4", 3), 60);
    EXPECT_EQ(midix_note_name_to_number("C#4", 3), 61);
    EXPECT_EQ(midix_note_name_to_number("D4", 3), 62);
    EXPECT_EQ(midix_note_name_to_number("D#4", 3), 63);
    EXPECT_EQ(midix_note_name_to_number("E4", 3), 64);
    EXPECT_EQ(midix_note_name_to_number("F4", 3), 65);
    EXPECT_EQ(midix_note_name_to_number("F#4", 3), 66);
    EXPECT_EQ(midix_note_name_to_number("G4", 3), 67);
    EXPECT_EQ(midix_note_name_to_number("G#4", 3), 68);
    EXPECT_EQ(midix_note_name_to_number("A4", 3), 69);
    EXPECT_EQ(midix_note_name_to_number("A#4", 3), 70);
    EXPECT_EQ(midix_note_name_to_number("B4", 3), 71);
}

TEST(UtilTest, NoteNameToNumberFlats) {
    // Flats should produce same result as sharps
    EXPECT_EQ(midix_note_name_to_number("Db4", 3), 61);  // Same as C#4
    EXPECT_EQ(midix_note_name_to_number("Eb4", 3), 63);  // Same as D#4
    EXPECT_EQ(midix_note_name_to_number("Gb4", 3), 66);  // Same as F#4
    EXPECT_EQ(midix_note_name_to_number("Ab4", 3), 68);  // Same as G#4
    EXPECT_EQ(midix_note_name_to_number("Bb4", 3), 70);  // Same as A#4
}

TEST(UtilTest, NoteNameToNumberOMCVariations) {
    // Different OMC values for middle C
    EXPECT_EQ(midix_note_name_to_number("C3", 2), 60);  // omc=2
    EXPECT_EQ(midix_note_name_to_number("C4", 3), 60);  // omc=3 (default)
    EXPECT_EQ(midix_note_name_to_number("C5", 4), 60);  // omc=4

    // Yamaha convention (omc=2)
    EXPECT_EQ(midix_note_name_to_number("C3", 2), 60);
    EXPECT_EQ(midix_note_name_to_number("A3", 2), 69);

    // Roland convention (omc=4)
    EXPECT_EQ(midix_note_name_to_number("C5", 4), 60);
    EXPECT_EQ(midix_note_name_to_number("A5", 4), 69);
}

TEST(UtilTest, NoteNameToNumberBoundaries) {
    // Full MIDI note range: 0-127
    EXPECT_EQ(midix_note_name_to_number("C-2", 3), 0);
    EXPECT_EQ(midix_note_name_to_number("C-1", 3), 12);
    EXPECT_EQ(midix_note_name_to_number("C0", 3), 24);
    EXPECT_EQ(midix_note_name_to_number("C7", 3), 108);
    EXPECT_EQ(midix_note_name_to_number("C8", 3), 120);
    EXPECT_EQ(midix_note_name_to_number("G8", 3), 127);
}

TEST(UtilTest, NoteNameToNumberInvalid) {
    // Invalid note names should return -1
    EXPECT_EQ(midix_note_name_to_number("H4", 3), -1);
    EXPECT_EQ(midix_note_name_to_number("C", 3), -1);  // Missing octave
    EXPECT_EQ(midix_note_name_to_number("C99", 3), -1);  // Out of range
    EXPECT_EQ(midix_note_name_to_number("", 3), -1);
    EXPECT_EQ(midix_note_name_to_number(nullptr, 3), -1);
}

// ============================================================================
// Note Number to Name Conversion Tests
// ============================================================================

TEST(UtilTest, NoteNumberToNameBasic) {
    char buffer[8];

    // Middle C
    EXPECT_STREQ(midix_note_number_to_name(60, 3, buffer, sizeof(buffer)), "C4");

    // A440
    EXPECT_STREQ(midix_note_number_to_name(69, 3, buffer, sizeof(buffer)), "A4");

    // First and last MIDI notes
    EXPECT_STREQ(midix_note_number_to_name(0, 3, buffer, sizeof(buffer)), "C-2");
    EXPECT_STREQ(midix_note_number_to_name(127, 3, buffer, sizeof(buffer)), "G8");
}

TEST(UtilTest, NoteNumberToNameSharps) {
    char buffer[8];

    // Should use sharps for black keys
    EXPECT_STREQ(midix_note_number_to_name(61, 3, buffer, sizeof(buffer)), "C#4");
    EXPECT_STREQ(midix_note_number_to_name(63, 3, buffer, sizeof(buffer)), "D#4");
    EXPECT_STREQ(midix_note_number_to_name(66, 3, buffer, sizeof(buffer)), "F#4");
    EXPECT_STREQ(midix_note_number_to_name(68, 3, buffer, sizeof(buffer)), "G#4");
    EXPECT_STREQ(midix_note_number_to_name(70, 3, buffer, sizeof(buffer)), "A#4");
}

TEST(UtilTest, NoteNumberToNameOMC) {
    char buffer[8];

    // Same note number with different OMC values
    EXPECT_STREQ(midix_note_number_to_name(60, 2, buffer, sizeof(buffer)), "C3");
    EXPECT_STREQ(midix_note_number_to_name(60, 3, buffer, sizeof(buffer)), "C4");
    EXPECT_STREQ(midix_note_number_to_name(60, 4, buffer, sizeof(buffer)), "C5");
}

TEST(UtilTest, NoteNumberToNameInvalid) {
    char buffer[8];

    // Out of range should return NULL or empty
    EXPECT_EQ(midix_note_number_to_name(-1, 3, buffer, sizeof(buffer)), nullptr);
    EXPECT_EQ(midix_note_number_to_name(128, 3, buffer, sizeof(buffer)), nullptr);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST(UtilTest, ValidNote) {
    // Valid MIDI notes: 0-127
    EXPECT_TRUE(midix_is_valid_note(0));
    EXPECT_TRUE(midix_is_valid_note(60));
    EXPECT_TRUE(midix_is_valid_note(127));

    // Invalid
    EXPECT_FALSE(midix_is_valid_note(-1));
    EXPECT_FALSE(midix_is_valid_note(128));
    EXPECT_FALSE(midix_is_valid_note(255));
}

TEST(UtilTest, ValidCC) {
    // Valid MIDI CC: 0-127
    EXPECT_TRUE(midix_is_valid_cc(0));
    EXPECT_TRUE(midix_is_valid_cc(1));   // Modulation
    EXPECT_TRUE(midix_is_valid_cc(7));   // Volume
    EXPECT_TRUE(midix_is_valid_cc(64));  // Sustain
    EXPECT_TRUE(midix_is_valid_cc(127));

    // Invalid
    EXPECT_FALSE(midix_is_valid_cc(-1));
    EXPECT_FALSE(midix_is_valid_cc(128));
}

TEST(UtilTest, ValidChannel) {
    // Valid MIDI channels: 1-16 (API uses 1-indexed)
    EXPECT_TRUE(midix_is_valid_channel(1));
    EXPECT_TRUE(midix_is_valid_channel(10));  // Drums
    EXPECT_TRUE(midix_is_valid_channel(16));

    // Invalid
    EXPECT_FALSE(midix_is_valid_channel(0));
    EXPECT_FALSE(midix_is_valid_channel(17));
    EXPECT_FALSE(midix_is_valid_channel(-1));
}

// ============================================================================
// Round-Trip Conversion Tests
// ============================================================================

TEST(UtilTest, RoundTripConversion) {
    char buffer[8];

    // Convert note number → name → number
    for (int note = 0; note <= 127; note++) {
        const char* name = midix_note_number_to_name(note, 3, buffer, sizeof(buffer));
        ASSERT_NE(name, nullptr) << "Failed to convert note " << note;

        int converted = midix_note_name_to_number(name, 3);
        EXPECT_EQ(converted, note) << "Round-trip failed for note " << note
                                    << " (" << name << ")";
    }
}

TEST(UtilTest, RoundTripWithDifferentOMC) {
    char buffer[8];

    // Test with different OMC values
    for (int omc = 2; omc <= 4; omc++) {
        for (int note = 0; note <= 127; note++) {
            const char* name = midix_note_number_to_name(note, omc, buffer, sizeof(buffer));
            if (name) {
                int converted = midix_note_name_to_number(name, omc);
                EXPECT_EQ(converted, note) << "Round-trip failed for note " << note
                                            << " with omc=" << omc;
            }
        }
    }
}

// ============================================================================
// Boundary and Edge Cases
// ============================================================================

TEST(UtilTest, EdgeCasePianoRange) {
    // Standard 88-key piano: A0 (21) to C8 (108)
    EXPECT_EQ(midix_note_name_to_number("A0", 3), 21);
    EXPECT_EQ(midix_note_name_to_number("C8", 3), 108);

    char buffer[8];
    EXPECT_STREQ(midix_note_number_to_name(21, 3, buffer, sizeof(buffer)), "A0");
    EXPECT_STREQ(midix_note_number_to_name(108, 3, buffer, sizeof(buffer)), "C8");
}

TEST(UtilTest, EdgeCaseNegativeOctaves) {
    // MIDI supports octaves -2 to 8
    EXPECT_EQ(midix_note_name_to_number("C-2", 3), 0);
    EXPECT_EQ(midix_note_name_to_number("C-1", 3), 12);
    EXPECT_EQ(midix_note_name_to_number("B-1", 3), 23);

    char buffer[8];
    EXPECT_STREQ(midix_note_number_to_name(0, 3, buffer, sizeof(buffer)), "C-2");
    EXPECT_STREQ(midix_note_number_to_name(12, 3, buffer, sizeof(buffer)), "C-1");
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
 *   make test_util
 *
 * Run:
 *   ./test_util
 *   # or
 *   ctest -R test_util -V
 *
 * Expected output:
 *   [==========] Running X tests from 1 test suite.
 *   [----------] Global test environment set-up.
 *   [----------] X tests from UtilTest
 *   [ RUN      ] UtilTest.NoteNameToNumberBasic
 *   [       OK ] UtilTest.NoteNameToNumberBasic (0 ms)
 *   ...
 *   [==========] X tests from 1 test suite ran. (Y ms total)
 *   [  PASSED  ] X tests.
 *
 * Notes:
 *   - Tests cover full MIDI note range (0-127)
 *   - Validates OMC (Octave for Middle C) variations
 *   - Round-trip tests ensure bidirectional consistency
 *   - All tests should pass for compliant implementation
 */
