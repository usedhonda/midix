/*
 * test_parser.cpp - CLI Parser Unit Tests
 *
 * Tests for SendMIDI-compatible command line parser.
 * Validates note name parsing, number parsing, and command identification.
 *
 * License: MIT
 */

#include <gtest/gtest.h>
#include <string>
#include <cstdint>

// Forward declarations for parser utility functions
// These should be implemented in cli/parser.cpp
namespace midix_parser {
    int parse_note_name(const char* name, int omc);
    bool parse_number(const char* str, int* value, int default_base);
    bool is_command(const char* str);
}

// ============================================================================
// Note Name Parsing Tests
// ============================================================================

TEST(ParserTest, NoteNameMiddleC) {
    // Middle C (C4 with omc=3, C3 with omc=2)
    EXPECT_EQ(midix_parser::parse_note_name("C4", 3), 60);
    EXPECT_EQ(midix_parser::parse_note_name("C3", 2), 60);
    EXPECT_EQ(midix_parser::parse_note_name("C5", 4), 60);
}

TEST(ParserTest, NoteNameSharpsAndFlats) {
    // C#4 and Db4 should be the same (61)
    EXPECT_EQ(midix_parser::parse_note_name("C#4", 3), 61);
    EXPECT_EQ(midix_parser::parse_note_name("Db4", 3), 61);

    // F#4 and Gb4
    EXPECT_EQ(midix_parser::parse_note_name("F#4", 3), 66);
    EXPECT_EQ(midix_parser::parse_note_name("Gb4", 3), 66);
}

TEST(ParserTest, NoteNameOctaveRange) {
    // C-2 = 0, G8 = 127
    EXPECT_EQ(midix_parser::parse_note_name("C-2", 3), 0);
    EXPECT_EQ(midix_parser::parse_note_name("G8", 3), 127);

    // A0 = 21 (common piano lowest)
    EXPECT_EQ(midix_parser::parse_note_name("A0", 3), 21);

    // C8 = 120
    EXPECT_EQ(midix_parser::parse_note_name("C8", 3), 120);
}

TEST(ParserTest, NoteNameAllNaturals) {
    // All natural notes in octave 4 (omc=3)
    EXPECT_EQ(midix_parser::parse_note_name("C4", 3), 60);
    EXPECT_EQ(midix_parser::parse_note_name("D4", 3), 62);
    EXPECT_EQ(midix_parser::parse_note_name("E4", 3), 64);
    EXPECT_EQ(midix_parser::parse_note_name("F4", 3), 65);
    EXPECT_EQ(midix_parser::parse_note_name("G4", 3), 67);
    EXPECT_EQ(midix_parser::parse_note_name("A4", 3), 69);
    EXPECT_EQ(midix_parser::parse_note_name("B4", 3), 71);
}

TEST(ParserTest, NoteNameCaseInsensitive) {
    // Parser should accept lowercase
    EXPECT_EQ(midix_parser::parse_note_name("c4", 3), 60);
    EXPECT_EQ(midix_parser::parse_note_name("C4", 3), 60);
    EXPECT_EQ(midix_parser::parse_note_name("c#4", 3), 61);
    EXPECT_EQ(midix_parser::parse_note_name("C#4", 3), 61);
}

TEST(ParserTest, NoteNameInvalid) {
    // Invalid note names should return -1
    EXPECT_EQ(midix_parser::parse_note_name("H4", 3), -1);  // H is not a note
    EXPECT_EQ(midix_parser::parse_note_name("C#b4", 3), -1);  // Can't be both sharp and flat
    EXPECT_EQ(midix_parser::parse_note_name("C99", 3), -1);  // Octave too high
    EXPECT_EQ(midix_parser::parse_note_name("C", 3), -1);  // Missing octave
}

TEST(ParserTest, NoteNameOMCVariations) {
    // Test OMC (Octave for Middle C) variations
    // Middle C = 60 in all cases
    EXPECT_EQ(midix_parser::parse_note_name("C3", 2), 60);  // omc=2: C3 is middle C
    EXPECT_EQ(midix_parser::parse_note_name("C4", 3), 60);  // omc=3: C4 is middle C (default)
    EXPECT_EQ(midix_parser::parse_note_name("C5", 4), 60);  // omc=4: C5 is middle C
}

// ============================================================================
// Number Parsing Tests
// ============================================================================

TEST(ParserTest, NumberDecimal) {
    int value;

    EXPECT_TRUE(midix_parser::parse_number("0", &value, 10));
    EXPECT_EQ(value, 0);

    EXPECT_TRUE(midix_parser::parse_number("127", &value, 10));
    EXPECT_EQ(value, 127);

    EXPECT_TRUE(midix_parser::parse_number("16383", &value, 10));
    EXPECT_EQ(value, 16383);
}

TEST(ParserTest, NumberHexadecimal) {
    int value;

    EXPECT_TRUE(midix_parser::parse_number("0x00", &value, 16));
    EXPECT_EQ(value, 0);

    EXPECT_TRUE(midix_parser::parse_number("0x7F", &value, 16));
    EXPECT_EQ(value, 127);

    EXPECT_TRUE(midix_parser::parse_number("0xFF", &value, 16));
    EXPECT_EQ(value, 255);

    // Without 0x prefix in hex mode
    EXPECT_TRUE(midix_parser::parse_number("7F", &value, 16));
    EXPECT_EQ(value, 127);
}

TEST(ParserTest, NumberSuffixes) {
    int value;

    // M suffix = decimal (force)
    EXPECT_TRUE(midix_parser::parse_number("127M", &value, 16));
    EXPECT_EQ(value, 127);  // Should be decimal despite base=16

    // H suffix = hexadecimal (force)
    EXPECT_TRUE(midix_parser::parse_number("7FH", &value, 10));
    EXPECT_EQ(value, 127);  // Should be hex despite base=10
}

TEST(ParserTest, NumberInvalid) {
    int value;

    EXPECT_FALSE(midix_parser::parse_number("", &value, 10));
    EXPECT_FALSE(midix_parser::parse_number("abc", &value, 10));  // Not a number
    EXPECT_FALSE(midix_parser::parse_number("-1", &value, 10));  // Negative
    EXPECT_FALSE(midix_parser::parse_number("99999", &value, 10));  // Too large
}

// ============================================================================
// Command Identification Tests
// ============================================================================

TEST(ParserTest, CommandIdentification) {
    // Valid commands
    EXPECT_TRUE(midix_parser::is_command("dev"));
    EXPECT_TRUE(midix_parser::is_command("list"));
    EXPECT_TRUE(midix_parser::is_command("on"));
    EXPECT_TRUE(midix_parser::is_command("off"));
    EXPECT_TRUE(midix_parser::is_command("cc"));
    EXPECT_TRUE(midix_parser::is_command("pc"));
    EXPECT_TRUE(midix_parser::is_command("pb"));
    EXPECT_TRUE(midix_parser::is_command("syx"));
    EXPECT_TRUE(midix_parser::is_command("panic"));

    // Not commands (numbers, note names)
    EXPECT_FALSE(midix_parser::is_command("60"));
    EXPECT_FALSE(midix_parser::is_command("C4"));
    EXPECT_FALSE(midix_parser::is_command("127"));
}

TEST(ParserTest, CommandCaseInsensitive) {
    // Commands should be case-insensitive
    EXPECT_TRUE(midix_parser::is_command("ON"));
    EXPECT_TRUE(midix_parser::is_command("On"));
    EXPECT_TRUE(midix_parser::is_command("on"));
    EXPECT_TRUE(midix_parser::is_command("PANIC"));
    EXPECT_TRUE(midix_parser::is_command("Panic"));
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST(ParserTest, BoundaryValues) {
    int value;

    // MIDI note range: 0-127
    EXPECT_EQ(midix_parser::parse_note_name("C-2", 3), 0);
    EXPECT_EQ(midix_parser::parse_note_name("G8", 3), 127);

    // 7-bit values: 0-127
    EXPECT_TRUE(midix_parser::parse_number("0", &value, 10));
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(midix_parser::parse_number("127", &value, 10));
    EXPECT_EQ(value, 127);

    // 14-bit values: 0-16383
    EXPECT_TRUE(midix_parser::parse_number("0", &value, 10));
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(midix_parser::parse_number("16383", &value, 10));
    EXPECT_EQ(value, 16383);
}

TEST(ParserTest, WhitespaceHandling) {
    int value;

    // Leading/trailing whitespace should be trimmed
    EXPECT_TRUE(midix_parser::parse_number(" 127 ", &value, 10));
    EXPECT_EQ(value, 127);

    // Note names with whitespace
    EXPECT_EQ(midix_parser::parse_note_name(" C4 ", 3), 60);
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
 *   make test_parser
 *
 * Run:
 *   ./test_parser
 *   # or
 *   ctest -R test_parser -V
 *
 * Expected output:
 *   [==========] Running X tests from 1 test suite.
 *   [----------] Global test environment set-up.
 *   [----------] X tests from ParserTest
 *   [ RUN      ] ParserTest.NoteNameMiddleC
 *   [       OK ] ParserTest.NoteNameMiddleC (0 ms)
 *   ...
 *   [==========] X tests from 1 test suite ran. (Y ms total)
 *   [  PASSED  ] X tests.
 *
 * Notes:
 *   - Requires parser utility functions to be implemented
 *   - Tests cover SendMIDI compatibility edge cases
 *   - All tests should pass for compliant parser implementation
 */
