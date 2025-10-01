// midix_util.c - Utility functions
// Copyright (c) 2025 libmidix contributors
// SPDX-License-Identifier: MIT

#include "midix_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

// ============================================================================
// Logging
// ============================================================================

static int g_log_level = MIDIX_LOG_WARN;
static bool g_log_initialized = false;

void midix_log_init(void) {
    if (g_log_initialized) {
        return;
    }

    const char* env = getenv("MIDIX_LOG_LEVEL");
    if (env) {
        if (strcmp(env, "DEBUG") == 0) {
            g_log_level = MIDIX_LOG_DEBUG;
        } else if (strcmp(env, "INFO") == 0) {
            g_log_level = MIDIX_LOG_INFO;
        } else if (strcmp(env, "WARN") == 0) {
            g_log_level = MIDIX_LOG_WARN;
        } else if (strcmp(env, "ERROR") == 0) {
            g_log_level = MIDIX_LOG_ERROR;
        }
    }

    g_log_initialized = true;
}

int midix_log_get_level(void) {
    return g_log_level;
}

void midix_log_write(midix_log_level_t level, const char* fmt, ...) {
    if (level > g_log_level) {
        return;
    }

    const char* level_str = "UNKNOWN";
    switch (level) {
        case MIDIX_LOG_ERROR: level_str = "ERROR"; break;
        case MIDIX_LOG_WARN:  level_str = "WARN "; break;
        case MIDIX_LOG_INFO:  level_str = "INFO "; break;
        case MIDIX_LOG_DEBUG: level_str = "DEBUG"; break;
    }

    fprintf(stderr, "[MIDIX:%s] ", level_str);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

// ============================================================================
// Note Name Parsing
// ============================================================================

static const char* note_names[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static const char* note_names_flat[] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

int midix_parse_note(const char* name, int omc) {
    if (!name || !*name) {
        return -1;
    }

    // Parse note name (C, C#, Db, etc.)
    int note_offset = -1;
    size_t name_len = strlen(name);
    size_t idx = 0;

    // Try sharp notation
    for (int i = 0; i < 12; i++) {
        size_t len = strlen(note_names[i]);
        if (name_len >= len && strncasecmp(name, note_names[i], len) == 0) {
            note_offset = i;
            idx = len;
            break;
        }
    }

    // Try flat notation if not found
    if (note_offset == -1) {
        for (int i = 0; i < 12; i++) {
            size_t len = strlen(note_names_flat[i]);
            if (name_len >= len && strncasecmp(name, note_names_flat[i], len) == 0) {
                note_offset = i;
                idx = len;
                break;
            }
        }
    }

    if (note_offset == -1) {
        return -1;
    }

    // Parse octave (supports negative octaves like C-2)
    if (idx >= name_len) {
        return -1;
    }

    int octave;
    if (name[idx] == '-') {
        // Negative octave
        if (sscanf(&name[idx], "%d", &octave) != 1) {
            return -1;
        }
    } else {
        if (sscanf(&name[idx], "%d", &octave) != 1) {
            return -1;
        }
    }

    // Calculate MIDI note number
    // omc defines which octave contains middle C (60)
    // For omc=3, C3=60, C4=72, etc.
    // For omc=4, C4=60, C5=72, etc.
    int midi_note = note_offset + (octave - omc) * 12 + 60;

    // Validate range (0-127)
    if (midi_note < 0 || midi_note > 127) {
        return -1;
    }

    return midi_note;
}

// ============================================================================
// Number Parsing
// ============================================================================

int midix_parse_int(const char* str, int* out_value) {
    if (!str || !*str || !out_value) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    // Check for hex suffix (H)
    size_t len = strlen(str);
    bool force_hex = false;
    bool force_dec = false;
    char* buffer = NULL;

    if (len > 1) {
        char last = toupper(str[len - 1]);
        if (last == 'H') {
            force_hex = true;
            buffer = strdup(str);
            buffer[len - 1] = '\0';
            str = buffer;
        } else if (last == 'M') {
            force_dec = true;
            buffer = strdup(str);
            buffer[len - 1] = '\0';
            str = buffer;
        }
    }

    // Parse as hex if 0x prefix or H suffix
    if (force_hex || (strncmp(str, "0x", 2) == 0) || (strncmp(str, "0X", 2) == 0)) {
        char* endptr;
        long val = strtol(str, &endptr, 16);
        if (*endptr != '\0') {
            free(buffer);
            return MIDIX_ERR_INVALID_PARAM;
        }
        *out_value = (int)val;
        free(buffer);
        return MIDIX_OK;
    }

    // Parse as decimal
    char* endptr;
    long val = strtol(str, &endptr, 10);
    if (*endptr != '\0') {
        free(buffer);
        return MIDIX_ERR_INVALID_PARAM;
    }

    *out_value = (int)val;
    free(buffer);
    return MIDIX_OK;
}

// ============================================================================
// MIDI Message Validation
// ============================================================================

bool midix_validate_channel(int ch) {
    return ch >= 1 && ch <= 16;
}

bool midix_validate_note(uint8_t note) {
    return note <= 127;
}

bool midix_validate_velocity(uint8_t vel) {
    return vel <= 127;
}

bool midix_validate_cc(uint8_t ccno) {
    return ccno <= 127;
}

bool midix_validate_value7(uint8_t val) {
    return val <= 127;
}

bool midix_validate_value14(uint16_t val) {
    return val <= 16383;
}
