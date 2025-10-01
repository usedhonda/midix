/*
 * midix_cli.h - CLI internal header for midix command-line tool
 *
 * SendMIDI-compatible MIDI transmission CLI
 *
 * License: MIT
 * Version: 0.1.0
 */

#pragma once

#include <midix.h>
#include <string>
#include <vector>
#include <cstdint>

/* ============================================================================
 * CLI State
 * ========================================================================== */

/**
 * Parser state for CLI operations
 */
struct MidixState {
    midix_ctx* ctx;                 // MIDI context
    int current_channel;            // Default MIDI channel (1-16)
    bool hex_mode;                  // true = hex default, false = decimal
    int octave_middle_c;            // Middle C octave number (default 4, C4=60)
    midix_time_ns schedule_base;    // Base time for @ scheduling
    midix_time_ns schedule_offset;  // Cumulative offset for + scheduling
    bool strict_mode;               // Exit on first error
    bool verbose;                   // Verbose logging
    bool json_mode;                 // JSON output for list
    bool drain_mode;                // Wait for completion before exit
    bool interactive;               // Interactive stdin mode

    MidixState()
        : ctx(nullptr)
        , current_channel(1)
        , hex_mode(false)
        , octave_middle_c(4)
        , schedule_base(0)
        , schedule_offset(0)
        , strict_mode(false)
        , verbose(false)
        , json_mode(false)
        , drain_mode(false)
        , interactive(false)
    {}
};

/* ============================================================================
 * Command Enumeration
 * ========================================================================== */

enum class Command {
    // Device management
    LIST,
    DEV,
    VIRT,

    // State
    CHANNEL,
    DEC,
    HEX,
    OMC,

    // Channel voice
    NOTE_ON,
    NOTE_OFF,
    POLY_PRESSURE,
    CC,
    CC14,
    PC,
    CP,
    PB,

    // RPN/NRPN
    RPN,
    NRPN,

    // System real-time
    CLOCK,
    MIDI_CLOCK,
    START,
    STOP,
    CONTINUE,
    ACTIVE_SENSING,

    // System common
    TIME_CODE,
    SONG_POSITION,
    SONG_SELECT,
    TUNE_REQUEST,
    RESET,

    // SysEx
    SYSEX,
    SYSEX_FILE,

    // Utility
    RAW,
    FILE,
    PANIC,
    STDIN,

    // Options
    DRAIN,
    STRICT,
    VERBOSE,
    JSON,

    // Unknown
    UNKNOWN
};

/* ============================================================================
 * Parser Functions
 * ========================================================================== */

/**
 * Tokenize command string into arguments
 * Handles quotes, comments, whitespace
 *
 * @param line Input line
 * @return Vector of tokens
 */
std::vector<std::string> tokenize(const std::string& line);

/**
 * Identify command from string
 *
 * @param cmd Command string
 * @return Command enum
 */
Command identify_command(const std::string& cmd);

/**
 * Parse number from string (supports decimal, hex, M/H suffix)
 *
 * @param str Input string
 * @param state Parser state (for default base)
 * @return Parsed value (0-127 typical)
 */
int parse_number(const std::string& str, const MidixState& state);

/**
 * Parse note name to MIDI note number
 * Supports: C-2 to G8, with #/b accidentals
 *
 * @param str Note name (e.g., "C4", "C#4", "Db3")
 * @param state Parser state (for octave_middle_c)
 * @return MIDI note number (0-127), or -1 on error
 */
int parse_note_name(const std::string& str, const MidixState& state);

/**
 * Parse note (number or name)
 *
 * @param str Input string
 * @param state Parser state
 * @return MIDI note number (0-127), or -1 on error
 */
int parse_note(const std::string& str, const MidixState& state);

/* ============================================================================
 * Command Execution
 * ========================================================================== */

/**
 * Execute a single command with arguments
 *
 * @param cmd Command type
 * @param args Arguments
 * @param state Parser state (modified)
 * @return 0 on success, negative on error
 */
int execute_command(Command cmd, const std::vector<std::string>& args, MidixState& state);

/**
 * Execute parsed tokens
 *
 * @param tokens Token list
 * @param state Parser state (modified)
 * @return 0 on success, negative on error
 */
int execute_tokens(const std::vector<std::string>& tokens, MidixState& state);

/**
 * Execute file contents
 *
 * @param filename File path
 * @param state Parser state (modified)
 * @return 0 on success, negative on error
 */
int execute_file(const std::string& filename, MidixState& state);

/**
 * Execute stdin until EOF
 *
 * @param state Parser state (modified)
 * @return 0 on success, negative on error
 */
int execute_stdin(MidixState& state);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * Print error message to stderr
 * Exits if strict_mode enabled
 */
void error(const MidixState& state, const std::string& msg);

/**
 * Print info message if verbose
 */
void info(const MidixState& state, const std::string& msg);

/**
 * Print debug message if verbose
 */
void debug(const MidixState& state, const std::string& msg);

/**
 * Read entire file into byte buffer
 *
 * @param filename File path
 * @param data Output buffer
 * @return 0 on success, negative on error
 */
int read_file_bytes(const std::string& filename, std::vector<uint8_t>& data);
