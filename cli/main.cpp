/*
 * main.cpp - Main entry point for midix CLI
 *
 * SendMIDI-compatible MIDI transmission command-line tool
 *
 * License: MIT
 * Version: 0.1.0
 */

#include "midix_cli.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <csignal>

/* ============================================================================
 * Global State (for signal handling)
 * ========================================================================== */

static MidixState* g_state = nullptr;

void signal_handler(int signal) {
    (void)signal;
    if (g_state && g_state->ctx) {
        std::cerr << "\nInterrupted - cleaning up..." << std::endl;
        midix_destroy(g_state->ctx);
        g_state->ctx = nullptr;
    }
    std::exit(130); // Standard exit code for SIGINT
}

/* ============================================================================
 * Usage Information
 * ========================================================================== */

void print_usage() {
    std::cout << R"(midix - SendMIDI-compatible MIDI transmission CLI
Version 0.1.0

USAGE:
  midix [commands...]
  midix file <commands.txt>
  midix dev "Device" --

EXAMPLES:
  midix list
  midix dev "IAC Driver" ch 1 on 60 100
  midix dev IAC on C4 100 +500 off C4 0
  midix dev IAC syf patch.syx --drain

DEVICE MANAGEMENT:
  list                    List available MIDI outputs
  dev <name>             Select output device (partial match)
  virt [name]            Create virtual output (macOS only)

STATE COMMANDS:
  ch <1-16>              Set default channel
  dec / hex              Set number format (decimal/hex)
  omc <num>              Set middle C octave (default: 4)

CHANNEL VOICE:
  on <note> <vel>        Note On
  off <note> [vel]       Note Off
  pp <note> <pressure>   Poly Pressure
  cc <num> <val>         Control Change
  cc14 <num> <val>       14-bit CC
  pc <program>           Program Change
  cp <val>               Channel Pressure
  pb <val>               Pitch Bend (0-16383, 8192=center)

RPN/NRPN:
  rpn <param> <val>      Registered Parameter Number
  nrpn <param> <val>     Non-Registered Parameter Number

REAL-TIME:
  clock <bpm>            Send MIDI clock at BPM
  mc                     Send single MIDI clock
  start / stop / cont    Transport control
  as                     Active Sensing

SYSTEM COMMON:
  tc <hh> <mm> <ss> <fr> MIDI Time Code
  spp <beats>            Song Position Pointer
  ss <song>              Song Select
  tun                    Tune Request
  rst                    System Reset

SYSEX:
  syx <bytes...>         Send SysEx (F0/F7 auto-added)
  syf <file>             Send SysEx from .syx file

UTILITY:
  raw <bytes...>         Send raw MIDI bytes
  file <path>            Execute commands from file
  panic                  All Notes Off + Sustain Off
  --                     Read from stdin

SCHEDULING:
  @<ms>                  Absolute time from now
  +<ms>                  Relative time from last message

OPTIONS:
  --drain                Wait for all messages before exit
  --strict               Exit on first error
  --verbose              Detailed logging
  --json                 JSON output (for list)

NUMBER FORMATS:
  60                     Decimal
  3CH                    Hexadecimal (H suffix)
  C4, C#4, Db3           Note names

For detailed documentation, see: https://github.com/yourusername/SendMIDILibrary
)";
}

void print_version() {
    std::cout << "midix version 0.1.0" << std::endl;
    std::cout << "libmidix version " << midix_version() << std::endl;
}

/* ============================================================================
 * Main Entry Point
 * ========================================================================== */

int main(int argc, char* argv[]) {
    // Create state
    MidixState state;
    g_state = &state;

    // Setup signal handler for Ctrl+C
    std::signal(SIGINT, signal_handler);

    // Create MIDI context
    state.ctx = midix_create("midix");
    if (!state.ctx) {
        std::cerr << "Error: Failed to create MIDI context" << std::endl;
        return 1;
    }

    // Parse command-line arguments
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(argv[i]);
    }

    // Handle special cases
    if (args.empty()) {
        print_usage();
        midix_destroy(state.ctx);
        return 0;
    }

    // Check for version/help flags
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            print_usage();
            midix_destroy(state.ctx);
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            print_version();
            midix_destroy(state.ctx);
            return 0;
        }
    }

    // Check for environment variable log level
    const char* log_level = std::getenv("MIDIX_LOG_LEVEL");
    if (log_level) {
        if (std::strcmp(log_level, "DEBUG") == 0 || std::strcmp(log_level, "VERBOSE") == 0) {
            state.verbose = true;
        }
    }

    // First pass: process global options
    for (const auto& arg : args) {
        if (arg == "--drain") {
            state.drain_mode = true;
        } else if (arg == "--strict") {
            state.strict_mode = true;
        } else if (arg == "--verbose") {
            state.verbose = true;
        } else if (arg == "--json") {
            state.json_mode = true;
        }
    }

    // Build command line string (for tokenization)
    std::string cmdline;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmdline += " ";

        // Quote arguments with spaces
        if (args[i].find(' ') != std::string::npos) {
            cmdline += "\"" + args[i] + "\"";
        } else {
            cmdline += args[i];
        }
    }

    // Tokenize and execute
    std::vector<std::string> tokens = tokenize(cmdline);
    int result = execute_tokens(tokens, state);

    // Drain if requested
    if (state.drain_mode) {
        info(state, "Draining send queue...");
        int flush_result = midix_flush(state.ctx);
        if (flush_result != MIDIX_OK) {
            error(state, "Failed to flush send queue");
            if (state.strict_mode) {
                result = flush_result;
            }
        }
    }

    // Cleanup
    info(state, "Cleaning up...");
    midix_destroy(state.ctx);
    g_state = nullptr;

    // Determine exit code
    int exit_code = 0;
    if (result != 0) {
        exit_code = state.strict_mode ? 1 : 0;
    }

    return exit_code;
}
