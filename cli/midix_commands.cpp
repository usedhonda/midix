/*
 * midix_commands.cpp - Command execution for midix CLI
 *
 * Implementation of all MIDI commands and utilities
 *
 * License: MIT
 * Version: 0.1.0
 */

#include "midix_cli.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

void error(const MidixState& state, const std::string& msg) {
    std::cerr << "Error: " << msg << std::endl;
    if (state.strict_mode) {
        std::exit(1);
    }
}

void info(const MidixState& state, const std::string& msg) {
    if (state.verbose) {
        std::cout << "[INFO] " << msg << std::endl;
    }
}

void debug(const MidixState& state, const std::string& msg) {
    if (state.verbose) {
        std::cout << "[DEBUG] " << msg << std::endl;
    }
}

int read_file_bytes(const std::string& filename, std::vector<uint8_t>& data) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        return -1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Device Management Commands
 * ========================================================================== */

static int cmd_list(const std::vector<std::string>& args, MidixState& state) {
    (void)args; // Unused

    char** names = nullptr;
    int count = 0;

    int result = midix_list_outputs(&names, &count);
    if (result != MIDIX_OK) {
        error(state, "Failed to list MIDI outputs");
        return result;
    }

    if (state.json_mode) {
        std::cout << "[" << std::endl;
        for (int i = 0; i < count; i++) {
            std::cout << "  {\"index\": " << i << ", \"name\": \"" << names[i] << "\"}";
            if (i < count - 1) std::cout << ",";
            std::cout << std::endl;
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "Available MIDI outputs:" << std::endl;
        for (int i = 0; i < count; i++) {
            std::cout << "  " << names[i] << std::endl;
        }
    }

    // Free memory
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);

    return 0;
}

static int cmd_dev(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "dev: missing device name");
        return -1;
    }

    std::string device_name = args[0];
    info(state, "Opening device: " + device_name);

    int result = midix_open_output_by_name(state.ctx, device_name.c_str());
    if (result != MIDIX_OK) {
        error(state, "Failed to open device: " + device_name);
        return result;
    }

    info(state, "Device opened successfully");
    return 0;
}

static int cmd_virt(const std::vector<std::string>& args, MidixState& state) {
    std::string port_name = args.empty() ? "midix virtual output" : args[0];
    info(state, "Creating virtual port: " + port_name);

    int result = midix_create_virtual_output(state.ctx, port_name.c_str());
    if (result != MIDIX_OK) {
        if (result == MIDIX_ERR_NOT_SUPPORTED) {
            error(state, "Virtual ports not supported on this platform (Windows requires loopMIDI)");
        } else {
            error(state, "Failed to create virtual port");
        }
        return result;
    }

    info(state, "Virtual port created successfully");
    return 0;
}

/* ============================================================================
 * State Commands
 * ========================================================================== */

static int cmd_channel(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "ch: missing channel number");
        return -1;
    }

    int ch = parse_number(args[0], state);
    if (ch < 1 || ch > 16) {
        error(state, "ch: channel must be 1-16");
        return -1;
    }

    state.current_channel = ch;
    debug(state, "Channel set to " + std::to_string(ch));
    return 0;
}

static int cmd_dec(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    state.hex_mode = false;
    debug(state, "Number mode: decimal");
    return 0;
}

static int cmd_hex(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    state.hex_mode = true;
    debug(state, "Number mode: hexadecimal");
    return 0;
}

static int cmd_omc(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "omc: missing octave number");
        return -1;
    }

    int omc = parse_number(args[0], state);
    if (omc < -2 || omc > 8) {
        error(state, "omc: octave must be -2 to 8");
        return -1;
    }

    state.octave_middle_c = omc;
    debug(state, "Middle C octave set to " + std::to_string(omc));
    return 0;
}

/* ============================================================================
 * Channel Voice Commands
 * ========================================================================== */

static int cmd_note_on(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 2) {
        error(state, "on: missing note or velocity");
        return -1;
    }

    int note = parse_note(args[0], state);
    if (note < 0 || note > 127) {
        error(state, "on: invalid note: " + args[0]);
        return -1;
    }

    int vel = parse_number(args[1], state);
    if (vel < 0 || vel > 127) {
        error(state, "on: velocity must be 0-127");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_note_on(state.ctx, state.current_channel, note, vel, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Note On");
        return result;
    }

    debug(state, "Note On: ch=" + std::to_string(state.current_channel) +
                 " note=" + std::to_string(note) + " vel=" + std::to_string(vel));
    return 0;
}

static int cmd_note_off(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "off: missing note");
        return -1;
    }

    int note = parse_note(args[0], state);
    if (note < 0 || note > 127) {
        error(state, "off: invalid note: " + args[0]);
        return -1;
    }

    int vel = 0;
    if (args.size() >= 2) {
        vel = parse_number(args[1], state);
        if (vel < 0 || vel > 127) {
            error(state, "off: velocity must be 0-127");
            return -1;
        }
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_note_off(state.ctx, state.current_channel, note, vel, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Note Off");
        return result;
    }

    debug(state, "Note Off: ch=" + std::to_string(state.current_channel) +
                 " note=" + std::to_string(note) + " vel=" + std::to_string(vel));
    return 0;
}

static int cmd_poly_pressure(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 2) {
        error(state, "pp: missing note or pressure");
        return -1;
    }

    int note = parse_note(args[0], state);
    if (note < 0 || note > 127) {
        error(state, "pp: invalid note: " + args[0]);
        return -1;
    }

    int pressure = parse_number(args[1], state);
    if (pressure < 0 || pressure > 127) {
        error(state, "pp: pressure must be 0-127");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_poly_pressure(state.ctx, state.current_channel, note, pressure, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Poly Pressure");
        return result;
    }

    debug(state, "Poly Pressure: ch=" + std::to_string(state.current_channel) +
                 " note=" + std::to_string(note) + " pressure=" + std::to_string(pressure));
    return 0;
}

static int cmd_cc(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 2) {
        error(state, "cc: missing controller or value");
        return -1;
    }

    int ccno = parse_number(args[0], state);
    if (ccno < 0 || ccno > 127) {
        error(state, "cc: controller must be 0-127");
        return -1;
    }

    int val = parse_number(args[1], state);
    if (val < 0 || val > 127) {
        error(state, "cc: value must be 0-127");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_cc(state.ctx, state.current_channel, ccno, val, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send CC");
        return result;
    }

    debug(state, "CC: ch=" + std::to_string(state.current_channel) +
                 " cc=" + std::to_string(ccno) + " val=" + std::to_string(val));
    return 0;
}

static int cmd_cc14(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 2) {
        error(state, "cc14: missing controller or value");
        return -1;
    }

    int ccno = parse_number(args[0], state);
    if (ccno < 0 || ccno > 31) {
        error(state, "cc14: controller must be 0-31");
        return -1;
    }

    int val = parse_number(args[1], state);
    if (val < 0 || val > 16383) {
        error(state, "cc14: value must be 0-16383");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_cc14(state.ctx, state.current_channel, ccno, val, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send CC14");
        return result;
    }

    debug(state, "CC14: ch=" + std::to_string(state.current_channel) +
                 " cc=" + std::to_string(ccno) + " val=" + std::to_string(val));
    return 0;
}

static int cmd_pc(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "pc: missing program number");
        return -1;
    }

    int program = parse_number(args[0], state);
    if (program < 0 || program > 127) {
        error(state, "pc: program must be 0-127");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_pc(state.ctx, state.current_channel, program, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Program Change");
        return result;
    }

    debug(state, "Program Change: ch=" + std::to_string(state.current_channel) +
                 " program=" + std::to_string(program));
    return 0;
}

static int cmd_cp(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "cp: missing pressure value");
        return -1;
    }

    int val = parse_number(args[0], state);
    if (val < 0 || val > 127) {
        error(state, "cp: pressure must be 0-127");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_cp(state.ctx, state.current_channel, val, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Channel Pressure");
        return result;
    }

    debug(state, "Channel Pressure: ch=" + std::to_string(state.current_channel) +
                 " val=" + std::to_string(val));
    return 0;
}

static int cmd_pb(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "pb: missing pitch bend value");
        return -1;
    }

    int val = parse_number(args[0], state);
    if (val < 0 || val > 16383) {
        error(state, "pb: value must be 0-16383");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_pb(state.ctx, state.current_channel, val, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Pitch Bend");
        return result;
    }

    debug(state, "Pitch Bend: ch=" + std::to_string(state.current_channel) +
                 " val=" + std::to_string(val));
    return 0;
}

/* ============================================================================
 * RPN/NRPN Commands
 * ========================================================================== */

static int cmd_rpn(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 2) {
        error(state, "rpn: missing parameter or value");
        return -1;
    }

    int param = parse_number(args[0], state);
    if (param < 0 || param > 16383) {
        error(state, "rpn: parameter must be 0-16383");
        return -1;
    }

    int val = parse_number(args[1], state);
    if (val < 0 || val > 16383) {
        error(state, "rpn: value must be 0-16383");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_rpn(state.ctx, state.current_channel, param, val, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send RPN");
        return result;
    }

    debug(state, "RPN: ch=" + std::to_string(state.current_channel) +
                 " param=" + std::to_string(param) + " val=" + std::to_string(val));
    return 0;
}

static int cmd_nrpn(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 2) {
        error(state, "nrpn: missing parameter or value");
        return -1;
    }

    int param = parse_number(args[0], state);
    if (param < 0 || param > 16383) {
        error(state, "nrpn: parameter must be 0-16383");
        return -1;
    }

    int val = parse_number(args[1], state);
    if (val < 0 || val > 16383) {
        error(state, "nrpn: value must be 0-16383");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_nrpn(state.ctx, state.current_channel, param, val, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send NRPN");
        return result;
    }

    debug(state, "NRPN: ch=" + std::to_string(state.current_channel) +
                 " param=" + std::to_string(param) + " val=" + std::to_string(val));
    return 0;
}

/* ============================================================================
 * Real-Time Commands
 * ========================================================================== */

static int cmd_clock(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "clock: missing BPM");
        return -1;
    }

    int bpm = parse_number(args[0], state);
    if (bpm < 1 || bpm > 999) {
        error(state, "clock: BPM must be 1-999");
        return -1;
    }

    info(state, "Sending MIDI clock at " + std::to_string(bpm) + " BPM (Ctrl+C to stop)");

    // Calculate interval: 24 clocks per quarter note
    // interval_ns = (60 * 1e9) / (bpm * 24)
    uint64_t interval_ns = (60000000000ULL) / (bpm * 24);

    // Send clocks continuously
    while (true) {
        midix_time_ns ts = midix_now();
        int result = midix_clock(state.ctx, ts);
        if (result != MIDIX_OK) {
            error(state, "Failed to send MIDI clock");
            return result;
        }

        std::this_thread::sleep_for(std::chrono::nanoseconds(interval_ns));
    }

    return 0;
}

static int cmd_midi_clock(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_clock(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send MIDI clock");
        return result;
    }
    debug(state, "MIDI Clock sent");
    return 0;
}

static int cmd_start(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_start(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Start");
        return result;
    }
    debug(state, "Start sent");
    return 0;
}

static int cmd_stop(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_stop(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Stop");
        return result;
    }
    debug(state, "Stop sent");
    return 0;
}

static int cmd_continue(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_continue(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Continue");
        return result;
    }
    debug(state, "Continue sent");
    return 0;
}

static int cmd_active_sensing(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_active_sensing(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Active Sensing");
        return result;
    }
    debug(state, "Active Sensing sent");
    return 0;
}

/* ============================================================================
 * System Common Commands
 * ========================================================================== */

static int cmd_time_code(const std::vector<std::string>& args, MidixState& state) {
    if (args.size() < 4) {
        error(state, "tc: missing time code arguments (hh mm ss fr)");
        return -1;
    }

    int hour = parse_number(args[0], state);
    int min = parse_number(args[1], state);
    int sec = parse_number(args[2], state);
    int frame = parse_number(args[3], state);

    if (hour < 0 || hour > 23) {
        error(state, "tc: hour must be 0-23");
        return -1;
    }
    if (min < 0 || min > 59) {
        error(state, "tc: minutes must be 0-59");
        return -1;
    }
    if (sec < 0 || sec > 59) {
        error(state, "tc: seconds must be 0-59");
        return -1;
    }
    if (frame < 0 || frame > 29) {
        error(state, "tc: frame must be 0-29");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_time_code(state.ctx, 0, hour, min, sec, frame, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Time Code");
        return result;
    }

    debug(state, "Time Code sent: " + std::to_string(hour) + ":" +
                 std::to_string(min) + ":" + std::to_string(sec) + "." + std::to_string(frame));
    return 0;
}

static int cmd_song_position(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "spp: missing position");
        return -1;
    }

    int pos = parse_number(args[0], state);
    if (pos < 0 || pos > 16383) {
        error(state, "spp: position must be 0-16383");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_song_position(state.ctx, pos, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Song Position");
        return result;
    }

    debug(state, "Song Position sent: " + std::to_string(pos));
    return 0;
}

static int cmd_song_select(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "ss: missing song number");
        return -1;
    }

    int song = parse_number(args[0], state);
    if (song < 0 || song > 127) {
        error(state, "ss: song must be 0-127");
        return -1;
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_song_select(state.ctx, song, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Song Select");
        return result;
    }

    debug(state, "Song Select sent: " + std::to_string(song));
    return 0;
}

static int cmd_tune_request(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_tune_request(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Tune Request");
        return result;
    }
    debug(state, "Tune Request sent");
    return 0;
}

static int cmd_reset(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_reset(state.ctx, ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send Reset");
        return result;
    }
    debug(state, "Reset sent");
    return 0;
}

/* ============================================================================
 * SysEx Commands
 * ========================================================================== */

static int cmd_sysex(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "syx: missing SysEx data");
        return -1;
    }

    std::vector<uint8_t> data;

    // Parse bytes
    for (const auto& arg : args) {
        int byte = parse_number(arg, state);
        if (byte < 0 || byte > 255) {
            error(state, "syx: invalid byte: " + arg);
            return -1;
        }
        data.push_back(static_cast<uint8_t>(byte));
    }

    // Auto-wrap with F0/F7 if missing
    if (data.empty() || data[0] != 0xF0) {
        data.insert(data.begin(), 0xF0);
    }
    if (data.back() != 0xF7) {
        data.push_back(0xF7);
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_send_sysex(state.ctx, data.data(), data.size(), ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send SysEx");
        return result;
    }

    debug(state, "SysEx sent: " + std::to_string(data.size()) + " bytes");
    return 0;
}

static int cmd_sysex_file(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "syf: missing file path");
        return -1;
    }

    std::string filename = args[0];
    std::vector<uint8_t> data;

    if (read_file_bytes(filename, data) != 0) {
        error(state, "syf: failed to read file: " + filename);
        return -1;
    }

    if (data.empty()) {
        error(state, "syf: file is empty: " + filename);
        return -1;
    }

    info(state, "Sending SysEx from file: " + filename + " (" + std::to_string(data.size()) + " bytes)");

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_send_sysex(state.ctx, data.data(), data.size(), ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send SysEx from file");
        return result;
    }

    info(state, "SysEx file sent successfully");
    return 0;
}

/* ============================================================================
 * Utility Commands
 * ========================================================================== */

static int cmd_raw(const std::vector<std::string>& args, MidixState& state) {
    if (args.empty()) {
        error(state, "raw: missing MIDI bytes");
        return -1;
    }

    std::vector<uint8_t> data;

    for (const auto& arg : args) {
        int byte = parse_number(arg, state);
        if (byte < 0 || byte > 255) {
            error(state, "raw: invalid byte: " + arg);
            return -1;
        }
        data.push_back(static_cast<uint8_t>(byte));
    }

    midix_time_ns ts = state.schedule_base + state.schedule_offset;
    int result = midix_send_bytes(state.ctx, data.data(), data.size(), ts);
    if (result != MIDIX_OK) {
        error(state, "Failed to send raw MIDI");
        return result;
    }

    debug(state, "Raw MIDI sent: " + std::to_string(data.size()) + " bytes");
    return 0;
}

static int cmd_panic(const std::vector<std::string>& args, MidixState& state) {
    (void)args;
    info(state, "Sending panic (All Notes Off + Sustain Off on all channels)");

    int result = midix_panic(state.ctx);
    if (result != MIDIX_OK) {
        error(state, "Failed to send panic");
        return result;
    }

    info(state, "Panic sent successfully");
    return 0;
}

/* ============================================================================
 * Command Dispatcher
 * ========================================================================== */

int execute_command(Command cmd, const std::vector<std::string>& args, MidixState& state) {
    switch (cmd) {
        // Device management
        case Command::LIST: return cmd_list(args, state);
        case Command::DEV: return cmd_dev(args, state);
        case Command::VIRT: return cmd_virt(args, state);

        // State
        case Command::CHANNEL: return cmd_channel(args, state);
        case Command::DEC: return cmd_dec(args, state);
        case Command::HEX: return cmd_hex(args, state);
        case Command::OMC: return cmd_omc(args, state);

        // Channel voice
        case Command::NOTE_ON: return cmd_note_on(args, state);
        case Command::NOTE_OFF: return cmd_note_off(args, state);
        case Command::POLY_PRESSURE: return cmd_poly_pressure(args, state);
        case Command::CC: return cmd_cc(args, state);
        case Command::CC14: return cmd_cc14(args, state);
        case Command::PC: return cmd_pc(args, state);
        case Command::CP: return cmd_cp(args, state);
        case Command::PB: return cmd_pb(args, state);

        // RPN/NRPN
        case Command::RPN: return cmd_rpn(args, state);
        case Command::NRPN: return cmd_nrpn(args, state);

        // Real-time
        case Command::CLOCK: return cmd_clock(args, state);
        case Command::MIDI_CLOCK: return cmd_midi_clock(args, state);
        case Command::START: return cmd_start(args, state);
        case Command::STOP: return cmd_stop(args, state);
        case Command::CONTINUE: return cmd_continue(args, state);
        case Command::ACTIVE_SENSING: return cmd_active_sensing(args, state);

        // System common
        case Command::TIME_CODE: return cmd_time_code(args, state);
        case Command::SONG_POSITION: return cmd_song_position(args, state);
        case Command::SONG_SELECT: return cmd_song_select(args, state);
        case Command::TUNE_REQUEST: return cmd_tune_request(args, state);
        case Command::RESET: return cmd_reset(args, state);

        // SysEx
        case Command::SYSEX: return cmd_sysex(args, state);
        case Command::SYSEX_FILE: return cmd_sysex_file(args, state);

        // Utility
        case Command::RAW: return cmd_raw(args, state);
        case Command::PANIC: return cmd_panic(args, state);

        // Options (handled in main)
        case Command::DRAIN:
            state.drain_mode = true;
            return 0;
        case Command::STRICT:
            state.strict_mode = true;
            return 0;
        case Command::VERBOSE:
            state.verbose = true;
            return 0;
        case Command::JSON:
            state.json_mode = true;
            return 0;

        case Command::FILE:
        case Command::STDIN:
            // Handled separately
            return 0;

        case Command::UNKNOWN:
        default:
            error(state, "Unknown command");
            return -1;
    }
}

/* ============================================================================
 * Token Execution
 * ========================================================================== */

int execute_tokens(const std::vector<std::string>& tokens, MidixState& state) {
    if (tokens.empty()) {
        return 0; // Empty line
    }

    size_t i = 0;
    while (i < tokens.size()) {
        const std::string& token = tokens[i];

        // Check for scheduling prefix
        if (token[0] == '@') {
            // Absolute scheduling
            std::string ms_str = token.substr(1);
            int ms = parse_number(ms_str, state);
            if (ms < 0) {
                error(state, "Invalid absolute schedule time: " + token);
                return -1;
            }
            state.schedule_base = midix_now() + (ms * 1000000ULL);
            state.schedule_offset = 0;
            i++;
            continue;
        } else if (token[0] == '+') {
            // Relative scheduling
            std::string ms_str = token.substr(1);
            int ms = parse_number(ms_str, state);
            if (ms < 0) {
                error(state, "Invalid relative schedule time: " + token);
                return -1;
            }
            state.schedule_offset += (ms * 1000000ULL);
            i++;
            continue;
        }

        // Identify command
        Command cmd = identify_command(token);

        // Special handling for file and stdin
        if (cmd == Command::FILE) {
            if (i + 1 >= tokens.size()) {
                error(state, "file: missing filename");
                return -1;
            }
            int result = execute_file(tokens[i + 1], state);
            if (result != 0) {
                return result;
            }
            i += 2;
            continue;
        }

        if (cmd == Command::STDIN) {
            int result = execute_stdin(state);
            if (result != 0) {
                return result;
            }
            i++;
            continue;
        }

        // Collect arguments for command
        std::vector<std::string> args;
        i++;

        // Determine argument count based on command
        while (i < tokens.size()) {
            const std::string& next = tokens[i];
            Command next_cmd = identify_command(next);

            // Check if next token is a new command
            if (next_cmd != Command::UNKNOWN) {
                break;
            }

            // Check for scheduling prefix
            if (!next.empty() && (next[0] == '@' || next[0] == '+')) {
                break;
            }

            args.push_back(next);
            i++;

            // Stop collecting args for commands with known arg counts
            // (This allows chaining commands on same line)
            if ((cmd == Command::NOTE_ON && args.size() >= 2) ||
                (cmd == Command::NOTE_OFF && args.size() >= 2) ||
                (cmd == Command::CC && args.size() >= 2) ||
                (cmd == Command::CC14 && args.size() >= 2) ||
                (cmd == Command::PB && args.size() >= 1) ||
                (cmd == Command::PC && args.size() >= 1) ||
                (cmd == Command::CP && args.size() >= 1) ||
                (cmd == Command::CHANNEL && args.size() >= 1) ||
                (cmd == Command::OMC && args.size() >= 1)) {
                break;
            }
        }

        // Execute command
        int result = execute_command(cmd, args, state);
        if (result != 0 && state.strict_mode) {
            return result;
        }
    }

    return 0;
}

/* ============================================================================
 * File and Stdin Execution
 * ========================================================================== */

int execute_file(const std::string& filename, MidixState& state) {
    std::ifstream file(filename);
    if (!file) {
        error(state, "Failed to open file: " + filename);
        return -1;
    }

    info(state, "Executing commands from file: " + filename);

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        std::vector<std::string> tokens = tokenize(line);
        if (!tokens.empty()) {
            int result = execute_tokens(tokens, state);
            if (result != 0) {
                error(state, "Error in " + filename + " line " + std::to_string(line_num));
                if (state.strict_mode) {
                    return result;
                }
            }
        }
    }

    return 0;
}

int execute_stdin(MidixState& state) {
    info(state, "Reading from stdin (Ctrl+D to exit)...");
    state.interactive = true;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::string> tokens = tokenize(line);
        if (!tokens.empty()) {
            int result = execute_tokens(tokens, state);
            if (result != 0 && state.strict_mode) {
                return result;
            }
        }
    }

    return 0;
}
