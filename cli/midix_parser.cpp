/*
 * midix_parser.cpp - Command parser for midix CLI
 *
 * Tokenization, command identification, and argument parsing
 *
 * License: MIT
 * Version: 0.1.0
 */

#include "midix_cli.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstring>

/* ============================================================================
 * Tokenizer
 * ========================================================================== */

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quote = false;
    bool in_comment = false;

    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];

        // Comment handling
        if (c == '#' && !in_quote) {
            in_comment = true;
            break;
        }

        if (in_comment) {
            break;
        }

        // Quote handling
        if (c == '"') {
            in_quote = !in_quote;
            continue;
        }

        // Whitespace handling
        if (std::isspace(c) && !in_quote) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        // Accumulate character
        current += c;
    }

    // Add final token
    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

/* ============================================================================
 * Command Identification
 * ========================================================================== */

static std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

Command identify_command(const std::string& cmd) {
    std::string lower = to_lower(cmd);

    // Device management
    if (lower == "list") return Command::LIST;
    if (lower == "dev" || lower == "device") return Command::DEV;
    if (lower == "virt" || lower == "virtual") return Command::VIRT;

    // State
    if (lower == "ch" || lower == "channel") return Command::CHANNEL;
    if (lower == "dec" || lower == "decimal") return Command::DEC;
    if (lower == "hex" || lower == "hexadecimal") return Command::HEX;
    if (lower == "omc" || lower == "octave-middle-c") return Command::OMC;

    // Channel voice
    if (lower == "on" || lower == "note-on") return Command::NOTE_ON;
    if (lower == "off" || lower == "note-off") return Command::NOTE_OFF;
    if (lower == "pp" || lower == "poly-pressure" || lower == "polypressure") return Command::POLY_PRESSURE;
    if (lower == "cc" || lower == "control-change") return Command::CC;
    if (lower == "cc14") return Command::CC14;
    if (lower == "pc" || lower == "program-change") return Command::PC;
    if (lower == "cp" || lower == "channel-pressure" || lower == "channelpressure") return Command::CP;
    if (lower == "pb" || lower == "pitch-bend" || lower == "pitchbend") return Command::PB;

    // RPN/NRPN
    if (lower == "rpn") return Command::RPN;
    if (lower == "nrpn") return Command::NRPN;

    // System real-time
    if (lower == "clock") return Command::CLOCK;
    if (lower == "mc" || lower == "midiclock") return Command::MIDI_CLOCK;
    if (lower == "start") return Command::START;
    if (lower == "stop") return Command::STOP;
    if (lower == "cont" || lower == "continue") return Command::CONTINUE;
    if (lower == "as" || lower == "active-sensing") return Command::ACTIVE_SENSING;

    // System common
    if (lower == "tc" || lower == "time-code" || lower == "timecode") return Command::TIME_CODE;
    if (lower == "spp" || lower == "song-position" || lower == "songposition") return Command::SONG_POSITION;
    if (lower == "ss" || lower == "song-select" || lower == "songselect") return Command::SONG_SELECT;
    if (lower == "tun" || lower == "tune" || lower == "tune-request") return Command::TUNE_REQUEST;
    if (lower == "rst" || lower == "reset") return Command::RESET;

    // SysEx
    if (lower == "syx" || lower == "sysex") return Command::SYSEX;
    if (lower == "syf" || lower == "sysex-file" || lower == "sysexfile") return Command::SYSEX_FILE;

    // Utility
    if (lower == "raw") return Command::RAW;
    if (lower == "file") return Command::FILE;
    if (lower == "panic") return Command::PANIC;
    if (lower == "--") return Command::STDIN;

    // Options
    if (lower == "--drain") return Command::DRAIN;
    if (lower == "--strict") return Command::STRICT;
    if (lower == "--verbose") return Command::VERBOSE;
    if (lower == "--json") return Command::JSON;

    return Command::UNKNOWN;
}

/* ============================================================================
 * Number Parsing
 * ========================================================================== */

int parse_number(const std::string& str, const MidixState& state) {
    if (str.empty()) return -1;

    std::string s = str;
    bool force_hex = false;
    bool force_dec = false;

    // Check for suffix
    if (s.length() > 1) {
        char last = std::toupper(s.back());
        if (last == 'H') {
            force_hex = true;
            s = s.substr(0, s.length() - 1);
        } else if (last == 'M') {
            force_dec = true;
            s = s.substr(0, s.length() - 1);
        }
    }

    // Determine base
    int base = 10;
    if (force_hex) {
        base = 16;
    } else if (force_dec) {
        base = 10;
    } else if (state.hex_mode) {
        base = 16;
    }

    try {
        size_t pos;
        int value = std::stoi(s, &pos, base);
        if (pos != s.length()) {
            return -1; // Invalid characters
        }
        return value;
    } catch (...) {
        return -1;
    }
}

/* ============================================================================
 * Note Name Parsing
 * ========================================================================== */

int parse_note_name(const std::string& str, const MidixState& state) {
    if (str.empty()) return -1;

    std::string s = to_lower(str);
    size_t pos = 0;

    // Parse note letter (C-G)
    char note_letter = std::toupper(s[pos++]);
    if (note_letter < 'A' || note_letter > 'G') {
        return -1;
    }

    // Base note values (C=0, D=2, E=4, F=5, G=7, A=9, B=11)
    int note_offsets[] = {9, 11, 0, 2, 4, 5, 7}; // A, B, C, D, E, F, G
    int base_note = note_offsets[note_letter - 'A'];

    // Parse accidental
    int accidental = 0;
    if (pos < s.length()) {
        if (s[pos] == '#') {
            accidental = 1;
            pos++;
        } else if (s[pos] == 'b') {
            accidental = -1;
            pos++;
        }
    }

    // Parse octave
    if (pos >= s.length()) {
        return -1; // Missing octave
    }

    std::string octave_str = s.substr(pos);
    int octave;
    try {
        octave = std::stoi(octave_str);
    } catch (...) {
        return -1;
    }

    // Calculate MIDI note number
    // Middle C (C4) = 60 when octave_middle_c = 4
    // MIDI note = (octave + 1) * 12 + base_note + accidental
    // Adjust for octave_middle_c offset
    int midi_note = (octave - state.octave_middle_c + 5) * 12 + base_note + accidental;

    if (midi_note < 0 || midi_note > 127) {
        return -1; // Out of range
    }

    return midi_note;
}

int parse_note(const std::string& str, const MidixState& state) {
    // Try note name first
    int note = parse_note_name(str, state);
    if (note >= 0) {
        return note;
    }

    // Try numeric
    note = parse_number(str, state);
    if (note >= 0 && note <= 127) {
        return note;
    }

    return -1;
}
