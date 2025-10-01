// midix_api.c - Public API implementation
// Copyright (c) 2025 libmidix contributors
// SPDX-License-Identifier: MIT

#include "midix_internal.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Port Management
// ============================================================================

int midix_open_output_by_name(midix_ctx* ctx, const char* name_substring) {
    if (!ctx || !name_substring) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!ctx->hal.ops || !ctx->hal.ops->open_output_by_name) {
        return MIDIX_ERR_NOT_SUPPORTED;
    }

    MIDIX_LOG_DEBUG("Opening output port: %s", name_substring);

    int result = ctx->hal.ops->open_output_by_name(&ctx->hal, name_substring);
    if (result == MIDIX_OK) {
        pthread_mutex_lock(&ctx->state_mutex);
        ctx->has_output = true;
        pthread_mutex_unlock(&ctx->state_mutex);
        MIDIX_LOG_INFO("Output port opened successfully");
    } else {
        MIDIX_LOG_ERROR("Failed to open output port: %d", result);
    }

    return result;
}

void midix_close_output(midix_ctx* ctx) {
    if (!ctx) {
        return;
    }

    if (!ctx->hal.ops || !ctx->hal.ops->close_output) {
        return;
    }

    MIDIX_LOG_DEBUG("Closing output port");

    pthread_mutex_lock(&ctx->state_mutex);
    if (ctx->has_output) {
        ctx->hal.ops->close_output(&ctx->hal);
        ctx->has_output = false;
    }
    pthread_mutex_unlock(&ctx->state_mutex);

    MIDIX_LOG_INFO("Output port closed");
}

int midix_create_virtual_output(midix_ctx* ctx, const char* port_name) {
    if (!ctx || !port_name) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!ctx->hal.ops || !ctx->hal.ops->create_virtual_output) {
        MIDIX_LOG_WARN("Virtual output not supported on this platform");
        return MIDIX_ERR_NOT_SUPPORTED;
    }

    MIDIX_LOG_DEBUG("Creating virtual output: %s", port_name);

    int result = ctx->hal.ops->create_virtual_output(&ctx->hal, port_name);
    if (result == MIDIX_OK) {
        pthread_mutex_lock(&ctx->state_mutex);
        ctx->has_virtual_output = true;
        pthread_mutex_unlock(&ctx->state_mutex);
        MIDIX_LOG_INFO("Virtual output created successfully");
    } else {
        MIDIX_LOG_ERROR("Failed to create virtual output: %d", result);
    }

    return result;
}

void midix_drop_virtual_output(midix_ctx* ctx) {
    if (!ctx) {
        return;
    }

    if (!ctx->hal.ops || !ctx->hal.ops->drop_virtual_output) {
        return;
    }

    MIDIX_LOG_DEBUG("Dropping virtual output");

    pthread_mutex_lock(&ctx->state_mutex);
    if (ctx->has_virtual_output) {
        ctx->hal.ops->drop_virtual_output(&ctx->hal);
        ctx->has_virtual_output = false;
    }
    pthread_mutex_unlock(&ctx->state_mutex);

    MIDIX_LOG_INFO("Virtual output dropped");
}

// ============================================================================
// Time
// ============================================================================

midix_time_ns midix_now(void) {
    // This will be implemented by HAL, but we need a default
    // In case HAL is not initialized, return 0
    return 0;
}

// ============================================================================
// Raw Send Functions
// ============================================================================

int midix_send_bytes(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts) {
    if (!ctx || !data || len == 0 || len > 3) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&ctx->state_mutex);
    bool has_output = ctx->has_output || ctx->has_virtual_output;
    pthread_mutex_unlock(&ctx->state_mutex);

    if (!has_output) {
        MIDIX_LOG_ERROR("No output port open");
        return MIDIX_ERR_NO_DEVICE;
    }

    return midix_scheduler_enqueue_short(ctx, data, len, ts);
}

int midix_send_sysex(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts) {
    if (!ctx || !data || len == 0) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&ctx->state_mutex);
    bool has_output = ctx->has_output || ctx->has_virtual_output;
    pthread_mutex_unlock(&ctx->state_mutex);

    if (!has_output) {
        MIDIX_LOG_ERROR("No output port open");
        return MIDIX_ERR_NO_DEVICE;
    }

    // Validate SysEx framing
    if (data[0] != 0xF0 || data[len - 1] != 0xF7) {
        MIDIX_LOG_ERROR("Invalid SysEx: missing F0/F7 framing");
        return MIDIX_ERR_SYSEX_INCOMPLETE;
    }

    return midix_scheduler_enqueue_sysex(ctx, data, len, ts);
}

// ============================================================================
// Channel Messages
// ============================================================================

int midix_note_on(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_note(note)) {
        MIDIX_LOG_ERROR("Invalid note: %d (must be 0-127)", note);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_velocity(vel)) {
        MIDIX_LOG_ERROR("Invalid velocity: %d (must be 0-127)", vel);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[3] = {
        0x90 | ((ch - 1) & 0x0F),
        note & 0x7F,
        vel & 0x7F
    };

    return midix_send_bytes(ctx, data, 3, ts);
}

int midix_note_off(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_note(note)) {
        MIDIX_LOG_ERROR("Invalid note: %d (must be 0-127)", note);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_velocity(vel)) {
        MIDIX_LOG_ERROR("Invalid velocity: %d (must be 0-127)", vel);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[3] = {
        0x80 | ((ch - 1) & 0x0F),
        note & 0x7F,
        vel & 0x7F
    };

    return midix_send_bytes(ctx, data, 3, ts);
}

int midix_cc(midix_ctx* ctx, int ch, uint8_t ccno, uint8_t val, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_cc(ccno)) {
        MIDIX_LOG_ERROR("Invalid CC number: %d (must be 0-127)", ccno);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_value7(val)) {
        MIDIX_LOG_ERROR("Invalid CC value: %d (must be 0-127)", val);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[3] = {
        0xB0 | ((ch - 1) & 0x0F),
        ccno & 0x7F,
        val & 0x7F
    };

    return midix_send_bytes(ctx, data, 3, ts);
}

int midix_pb(midix_ctx* ctx, int ch, uint16_t val14, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_value14(val14)) {
        MIDIX_LOG_ERROR("Invalid pitch bend value: %d (must be 0-16383)", val14);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[3] = {
        0xE0 | ((ch - 1) & 0x0F),
        val14 & 0x7F,         // LSB
        (val14 >> 7) & 0x7F   // MSB
    };

    return midix_send_bytes(ctx, data, 3, ts);
}

int midix_cp(midix_ctx* ctx, int ch, uint8_t val, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_value7(val)) {
        MIDIX_LOG_ERROR("Invalid channel pressure value: %d (must be 0-127)", val);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[2] = {
        0xD0 | ((ch - 1) & 0x0F),
        val & 0x7F
    };

    return midix_send_bytes(ctx, data, 2, ts);
}

int midix_pc(midix_ctx* ctx, int ch, uint8_t program, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_value7(program)) {
        MIDIX_LOG_ERROR("Invalid program number: %d (must be 0-127)", program);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[2] = {
        0xC0 | ((ch - 1) & 0x0F),
        program & 0x7F
    };

    return midix_send_bytes(ctx, data, 2, ts);
}

// ============================================================================
// Special Functions
// ============================================================================

int midix_panic(midix_ctx* ctx) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    MIDIX_LOG_INFO("Sending panic (All Notes Off + Sustain Off)");

    // Send All Notes Off (CC 123) and Sustain Off (CC 64) on all channels
    for (int ch = 1; ch <= 16; ch++) {
        // All Notes Off
        int result = midix_cc(ctx, ch, 123, 0, 0);
        if (result != MIDIX_OK) {
            MIDIX_LOG_ERROR("Failed to send All Notes Off on channel %d: %d", ch, result);
            return result;
        }

        // Sustain Off
        result = midix_cc(ctx, ch, 64, 0, 0);
        if (result != MIDIX_OK) {
            MIDIX_LOG_ERROR("Failed to send Sustain Off on channel %d: %d", ch, result);
            return result;
        }
    }

    return MIDIX_OK;
}

int midix_flush(midix_ctx* ctx) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    MIDIX_LOG_DEBUG("Flushing send queue");
    return midix_scheduler_flush(ctx);
}

// ============================================================================
// Additional Channel Voice Messages
// ============================================================================

int midix_poly_pressure(midix_ctx* ctx, int ch, uint8_t note, uint8_t pressure, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch)) {
        MIDIX_LOG_ERROR("Invalid channel: %d", ch);
        return MIDIX_ERR_INVALID_MSG;
    }

    if (!midix_validate_note(note) || !midix_validate_value7(pressure)) {
        MIDIX_LOG_ERROR("Invalid note or pressure value");
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[3] = {
        0xA0 | ((ch - 1) & 0x0F),
        note & 0x7F,
        pressure & 0x7F
    };

    return midix_send_bytes(ctx, data, 3, ts);
}

int midix_cc14(midix_ctx* ctx, int ch, uint8_t ccno, uint16_t val14, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_channel(ch) || ccno > 31 || !midix_validate_value14(val14)) {
        MIDIX_LOG_ERROR("Invalid parameters for CC14");
        return MIDIX_ERR_INVALID_MSG;
    }

    // Send MSB (CC ccno)
    int result = midix_cc(ctx, ch, ccno, (val14 >> 7) & 0x7F, ts);
    if (result != MIDIX_OK) {
        return result;
    }

    // Send LSB (CC ccno+32)
    return midix_cc(ctx, ch, ccno + 32, val14 & 0x7F, ts);
}

// ============================================================================
// RPN/NRPN
// ============================================================================

int midix_rpn(midix_ctx* ctx, int ch, uint16_t number, uint16_t value, midix_time_ns ts) {
    if (!ctx || !midix_validate_channel(ch)) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_value14(number) || !midix_validate_value14(value)) {
        MIDIX_LOG_ERROR("Invalid RPN number or value");
        return MIDIX_ERR_INVALID_MSG;
    }

    // CC 101 (RPN MSB), CC 100 (RPN LSB), CC 6 (Data Entry MSB), CC 38 (Data Entry LSB)
    int result;
    result = midix_cc(ctx, ch, 101, (number >> 7) & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    result = midix_cc(ctx, ch, 100, number & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    result = midix_cc(ctx, ch, 6, (value >> 7) & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    result = midix_cc(ctx, ch, 38, value & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    // Reset RPN to null
    result = midix_cc(ctx, ch, 101, 127, ts);
    if (result != MIDIX_OK) return result;

    return midix_cc(ctx, ch, 100, 127, ts);
}

int midix_nrpn(midix_ctx* ctx, int ch, uint16_t number, uint16_t value, midix_time_ns ts) {
    if (!ctx || !midix_validate_channel(ch)) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_value14(number) || !midix_validate_value14(value)) {
        MIDIX_LOG_ERROR("Invalid NRPN number or value");
        return MIDIX_ERR_INVALID_MSG;
    }

    // CC 99 (NRPN MSB), CC 98 (NRPN LSB), CC 6 (Data Entry MSB), CC 38 (Data Entry LSB)
    int result;
    result = midix_cc(ctx, ch, 99, (number >> 7) & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    result = midix_cc(ctx, ch, 98, number & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    result = midix_cc(ctx, ch, 6, (value >> 7) & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    result = midix_cc(ctx, ch, 38, value & 0x7F, ts);
    if (result != MIDIX_OK) return result;

    // Reset NRPN to null
    result = midix_cc(ctx, ch, 99, 127, ts);
    if (result != MIDIX_OK) return result;

    return midix_cc(ctx, ch, 98, 127, ts);
}

// ============================================================================
// System Real-Time Messages
// ============================================================================

int midix_clock(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xF8;  // MIDI Clock
    return midix_send_bytes(ctx, &data, 1, ts);
}

int midix_start(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xFA;  // Start
    return midix_send_bytes(ctx, &data, 1, ts);
}

int midix_stop(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xFC;  // Stop
    return midix_send_bytes(ctx, &data, 1, ts);
}

int midix_continue(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xFB;  // Continue
    return midix_send_bytes(ctx, &data, 1, ts);
}

int midix_active_sensing(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xFE;  // Active Sensing
    return midix_send_bytes(ctx, &data, 1, ts);
}

// ============================================================================
// System Common Messages
// ============================================================================

int midix_time_code(midix_ctx* ctx, uint8_t type, uint8_t hour, uint8_t min, uint8_t sec, uint8_t frame, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    // Send MIDI Time Code Quarter Frame messages
    // This is a simplified implementation
    uint8_t data = 0xF1;  // Time Code Quarter Frame
    return midix_send_bytes(ctx, &data, 1, ts);
}

int midix_song_position(midix_ctx* ctx, uint16_t position, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_value14(position)) {
        MIDIX_LOG_ERROR("Invalid song position: %d", position);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[3] = {
        0xF2,  // Song Position Pointer
        position & 0x7F,
        (position >> 7) & 0x7F
    };

    return midix_send_bytes(ctx, data, 3, ts);
}

int midix_song_select(midix_ctx* ctx, uint8_t song, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!midix_validate_value7(song)) {
        MIDIX_LOG_ERROR("Invalid song number: %d", song);
        return MIDIX_ERR_INVALID_MSG;
    }

    uint8_t data[2] = {
        0xF3,  // Song Select
        song & 0x7F
    };

    return midix_send_bytes(ctx, data, 2, ts);
}

int midix_tune_request(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xF6;  // Tune Request
    return midix_send_bytes(ctx, &data, 1, ts);
}

int midix_reset(midix_ctx* ctx, midix_time_ns ts) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t data = 0xFF;  // System Reset
    return midix_send_bytes(ctx, &data, 1, ts);
}

// ============================================================================
// Utility
// ============================================================================

const char* midix_version(void) {
    return "0.1.0";
}
