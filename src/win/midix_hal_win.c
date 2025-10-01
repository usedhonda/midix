/*
 * midix_hal_win.c - Windows WinMM HAL for midix library
 *
 * Windows MIDI implementation using WinMM (mmsystem) APIs.
 * Provides device enumeration, connection, sending (short/SysEx), and timing.
 *
 * License: MIT
 * Version: 0.1.0
 *
 * NOTE: This is a stub implementation for build verification.
 *       Full testing requires Windows environment with MIDI devices.
 */

#ifdef _WIN32

#include "midix.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Link with winmm.lib */
#pragma comment(lib, "winmm.lib")

/* ============================================================================
 * Internal Structures
 * ========================================================================== */

/**
 * SysEx buffer management structure for async send
 */
typedef struct {
    MIDIHDR header;
    uint8_t* data;
    size_t   len;
    bool     in_use;
    struct midix_sysex_buffer* next;
} midix_sysex_buffer;

/**
 * MIDI context structure (Windows implementation)
 */
struct midix_ctx {
    char            client_name[256];
    HMIDIOUT        midi_out;          /* WinMM output handle */
    UINT            device_id;         /* Current device ID */
    bool            is_open;           /* Device open status */

    /* SysEx buffer pool */
    midix_sysex_buffer* sysex_buffers;
    CRITICAL_SECTION    sysex_lock;

    /* Performance counter for high-precision timing */
    LARGE_INTEGER   qpc_frequency;

    /* Virtual port not supported on Windows (requires external driver) */
    bool            virtual_port_requested;
};

/* ============================================================================
 * Logging Macros
 * ========================================================================== */

#define LOG_ERROR(fmt, ...) fprintf(stderr, "[MIDIX_WIN_ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[MIDIX_WIN_WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  fprintf(stdout, "[MIDIX_WIN_INFO] " fmt "\n", ##__VA_ARGS__)

#ifdef MIDIX_DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stdout, "[MIDIX_WIN_DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

/**
 * Convert WinMM error code to error string
 */
static const char* winmm_error_string(MMRESULT err) {
    static char buf[256];
    if (midiOutGetErrorTextA(err, buf, sizeof(buf)) == MMSYSERR_NOERROR) {
        return buf;
    }
    return "Unknown MIDI error";
}

/**
 * Case-insensitive substring search
 */
static bool str_contains_nocase(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;

    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);

    if (nlen > hlen) return false;

    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h += 32; /* tolower */
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

/**
 * SysEx completion callback (called by WinMM driver)
 */
static void CALLBACK sysex_callback(HMIDIOUT hmo, UINT msg, DWORD_PTR instance,
                                   DWORD_PTR param1, DWORD_PTR param2) {
    if (msg != MOM_DONE) return;

    midix_ctx* ctx = (midix_ctx*)instance;
    MIDIHDR* hdr = (MIDIHDR*)param1;

    EnterCriticalSection(&ctx->sysex_lock);

    /* Find and free the buffer */
    midix_sysex_buffer* buf = ctx->sysex_buffers;
    midix_sysex_buffer* prev = NULL;

    while (buf) {
        if (&buf->header == hdr) {
            /* Unprepare header */
            midiOutUnprepareHeader(ctx->midi_out, &buf->header, sizeof(MIDIHDR));

            /* Remove from list */
            if (prev) {
                prev->next = buf->next;
            } else {
                ctx->sysex_buffers = buf->next;
            }

            /* Free resources */
            free(buf->data);
            free(buf);

            LOG_DEBUG("SysEx buffer completed and freed (hdr=%p)", hdr);
            break;
        }
        prev = buf;
        buf = buf->next;
    }

    LeaveCriticalSection(&ctx->sysex_lock);
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

/**
 * Create MIDI context
 */
midix_ctx* midix_create(const char* client_name) {
    if (!client_name) {
        LOG_ERROR("midix_create: client_name is NULL");
        return NULL;
    }

    midix_ctx* ctx = (midix_ctx*)calloc(1, sizeof(midix_ctx));
    if (!ctx) {
        LOG_ERROR("midix_create: Out of memory");
        return NULL;
    }

    strncpy_s(ctx->client_name, sizeof(ctx->client_name), client_name, _TRUNCATE);
    ctx->midi_out = NULL;
    ctx->device_id = 0;
    ctx->is_open = false;
    ctx->sysex_buffers = NULL;
    ctx->virtual_port_requested = false;

    InitializeCriticalSection(&ctx->sysex_lock);

    /* Initialize performance counter */
    if (!QueryPerformanceFrequency(&ctx->qpc_frequency)) {
        LOG_ERROR("midix_create: QueryPerformanceFrequency failed");
        DeleteCriticalSection(&ctx->sysex_lock);
        free(ctx);
        return NULL;
    }

    LOG_INFO("Created MIDI context '%s' (QPC freq: %lld Hz)",
             ctx->client_name, ctx->qpc_frequency.QuadPart);

    return ctx;
}

/**
 * Destroy MIDI context
 */
void midix_destroy(midix_ctx* ctx) {
    if (!ctx) return;

    LOG_INFO("Destroying MIDI context '%s'", ctx->client_name);

    /* Close output if open */
    if (ctx->is_open) {
        midix_close_output(ctx);
    }

    /* Free any remaining SysEx buffers (should be empty after close) */
    EnterCriticalSection(&ctx->sysex_lock);
    midix_sysex_buffer* buf = ctx->sysex_buffers;
    while (buf) {
        midix_sysex_buffer* next = buf->next;
        if (ctx->midi_out) {
            midiOutUnprepareHeader(ctx->midi_out, &buf->header, sizeof(MIDIHDR));
        }
        free(buf->data);
        free(buf);
        buf = next;
    }
    ctx->sysex_buffers = NULL;
    LeaveCriticalSection(&ctx->sysex_lock);

    DeleteCriticalSection(&ctx->sysex_lock);
    free(ctx);

    LOG_DEBUG("MIDI context destroyed");
}

/**
 * List available MIDI output devices
 */
int midix_list_outputs(char*** names, int* count) {
    if (!names || !count) {
        LOG_ERROR("midix_list_outputs: NULL parameter");
        return MIDIX_ERR_INVALID_PARAM;
    }

    UINT num_devs = midiOutGetNumDevs();
    LOG_INFO("Found %u MIDI output devices", num_devs);

    if (num_devs == 0) {
        *names = NULL;
        *count = 0;
        return MIDIX_OK;
    }

    char** device_names = (char**)calloc(num_devs, sizeof(char*));
    if (!device_names) {
        LOG_ERROR("midix_list_outputs: Out of memory");
        return MIDIX_ERR_OUT_OF_MEMORY;
    }

    int valid_count = 0;
    for (UINT i = 0; i < num_devs; i++) {
        MIDIOUTCAPSA caps;
        MMRESULT res = midiOutGetDevCapsA(i, &caps, sizeof(caps));

        if (res == MMSYSERR_NOERROR) {
            device_names[valid_count] = _strdup(caps.szPname);
            if (!device_names[valid_count]) {
                LOG_ERROR("midix_list_outputs: Out of memory for device name");
                /* Free previously allocated names */
                for (int j = 0; j < valid_count; j++) {
                    free(device_names[j]);
                }
                free(device_names);
                return MIDIX_ERR_OUT_OF_MEMORY;
            }
            LOG_DEBUG("Device %u: %s (wMid=%04X, wPid=%04X, vDriverVersion=%04X)",
                     i, caps.szPname, caps.wMid, caps.wPid, caps.vDriverVersion);
            valid_count++;
        } else {
            LOG_WARN("Failed to get caps for device %u: %s", i, winmm_error_string(res));
        }
    }

    *names = device_names;
    *count = valid_count;
    return MIDIX_OK;
}

/**
 * Open output device by name (substring match, case-insensitive)
 */
int midix_open_output_by_name(midix_ctx* ctx, const char* name_substring) {
    if (!ctx) {
        LOG_ERROR("midix_open_output_by_name: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!name_substring || strlen(name_substring) == 0) {
        LOG_ERROR("midix_open_output_by_name: NULL or empty name_substring");
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Close existing connection */
    if (ctx->is_open) {
        LOG_INFO("Closing existing device before opening new one");
        midix_close_output(ctx);
    }

    /* Search for matching device */
    UINT num_devs = midiOutGetNumDevs();
    UINT matched_id = (UINT)-1;

    LOG_INFO("Searching for device matching '%s' among %u devices", name_substring, num_devs);

    for (UINT i = 0; i < num_devs; i++) {
        MIDIOUTCAPSA caps;
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            LOG_DEBUG("Checking device %u: %s", i, caps.szPname);
            if (str_contains_nocase(caps.szPname, name_substring)) {
                matched_id = i;
                LOG_INFO("Matched device %u: %s", i, caps.szPname);
                break;
            }
        }
    }

    if (matched_id == (UINT)-1) {
        LOG_ERROR("No device matching '%s' found", name_substring);
        return MIDIX_ERR_NO_DEVICE;
    }

    /* Open device with callback for SysEx */
    MMRESULT res = midiOutOpen(&ctx->midi_out, matched_id,
                              (DWORD_PTR)sysex_callback,
                              (DWORD_PTR)ctx,
                              CALLBACK_FUNCTION);

    if (res != MMSYSERR_NOERROR) {
        LOG_ERROR("midiOutOpen failed: %s", winmm_error_string(res));
        return MIDIX_ERR_PORT_FAILURE;
    }

    ctx->device_id = matched_id;
    ctx->is_open = true;

    LOG_INFO("Opened MIDI output device %u successfully", matched_id);
    return MIDIX_OK;
}

/**
 * Close output device
 */
void midix_close_output(midix_ctx* ctx) {
    if (!ctx || !ctx->is_open) return;

    LOG_INFO("Closing MIDI output device %u", ctx->device_id);

    /* Reset device (stops all notes, clears buffers) */
    midiOutReset(ctx->midi_out);

    /* Wait for all SysEx buffers to complete (with timeout) */
    int timeout_ms = 5000;
    int waited_ms = 0;
    while (ctx->sysex_buffers != NULL && waited_ms < timeout_ms) {
        Sleep(10);
        waited_ms += 10;
    }

    if (ctx->sysex_buffers != NULL) {
        LOG_WARN("SysEx buffers not freed after %d ms, forcing close", timeout_ms);
    }

    /* Close device */
    MMRESULT res = midiOutClose(ctx->midi_out);
    if (res != MMSYSERR_NOERROR) {
        LOG_ERROR("midiOutClose failed: %s", winmm_error_string(res));
    }

    ctx->midi_out = NULL;
    ctx->is_open = false;

    LOG_DEBUG("MIDI output device closed");
}

/**
 * Create virtual output port (NOT SUPPORTED on Windows)
 */
int midix_create_virtual_output(midix_ctx* ctx, const char* port_name) {
    if (!ctx) {
        LOG_ERROR("midix_create_virtual_output: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    LOG_WARN("Virtual MIDI ports not supported on Windows (use loopMIDI/virtualMIDI)");
    ctx->virtual_port_requested = true;
    return MIDIX_ERR_NOT_SUPPORTED;
}

/**
 * Drop virtual output port
 */
void midix_drop_virtual_output(midix_ctx* ctx) {
    if (!ctx) return;

    if (ctx->virtual_port_requested) {
        LOG_DEBUG("Virtual port drop requested (not supported on Windows)");
        ctx->virtual_port_requested = false;
    }
}

/**
 * Get current monotonic time in nanoseconds
 */
midix_time_ns midix_now(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER freq;

    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&freq);

    /* Convert to nanoseconds: (counter * 1e9) / frequency */
    /* Avoid overflow by computing in steps */
    uint64_t ns = ((uint64_t)counter.QuadPart * 1000000000ULL) / (uint64_t)freq.QuadPart;

    return ns;
}

/**
 * Send short MIDI message (1-3 bytes, not SysEx)
 * NOTE: Windows timestamp scheduling not implemented in stub (immediate send only)
 */
static int send_short_message(midix_ctx* ctx, uint32_t msg, midix_time_ns ts) {
    if (!ctx) {
        LOG_ERROR("send_short_message: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!ctx->is_open) {
        LOG_ERROR("send_short_message: Device not open");
        return MIDIX_ERR_NO_DEVICE;
    }

    /* TODO: Implement timestamp scheduling with high-precision timer
     * For now, send immediately regardless of ts */
    if (ts != 0) {
        LOG_DEBUG("Timestamp scheduling not yet implemented, sending immediately");
    }

    MMRESULT res = midiOutShortMsg(ctx->midi_out, msg);

    if (res != MMSYSERR_NOERROR) {
        LOG_ERROR("midiOutShortMsg failed: %s (msg=0x%08X)", winmm_error_string(res), msg);
        return MIDIX_ERR_PORT_FAILURE;
    }

    LOG_DEBUG("Sent short message: 0x%08X", msg);
    return MIDIX_OK;
}

/**
 * Send Note On
 */
int midix_note_on(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_note_on: Invalid channel %d (must be 1-16)", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (note > 127 || vel > 127) {
        LOG_ERROR("midix_note_on: Invalid note/velocity (%u/%u)", note, vel);
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* WinMM short message format: status | data1 << 8 | data2 << 16 */
    uint32_t msg = (0x90 | (ch - 1)) | (note << 8) | (vel << 16);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send Note Off
 */
int midix_note_off(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_note_off: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (note > 127 || vel > 127) {
        LOG_ERROR("midix_note_off: Invalid note/velocity (%u/%u)", note, vel);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint32_t msg = (0x80 | (ch - 1)) | (note << 8) | (vel << 16);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send Polyphonic Aftertouch
 */
int midix_poly_pressure(midix_ctx* ctx, int ch, uint8_t note, uint8_t pressure, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_poly_pressure: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (note > 127 || pressure > 127) {
        LOG_ERROR("midix_poly_pressure: Invalid note/pressure (%u/%u)", note, pressure);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint32_t msg = (0xA0 | (ch - 1)) | (note << 8) | (pressure << 16);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send Control Change
 */
int midix_cc(midix_ctx* ctx, int ch, uint8_t ccno, uint8_t val, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_cc: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (ccno > 127 || val > 127) {
        LOG_ERROR("midix_cc: Invalid CC number/value (%u/%u)", ccno, val);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint32_t msg = (0xB0 | (ch - 1)) | (ccno << 8) | (val << 16);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send 14-bit Control Change
 */
int midix_cc14(midix_ctx* ctx, int ch, uint8_t ccno, uint16_t val14, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_cc14: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (ccno > 31 || val14 > 16383) {
        LOG_ERROR("midix_cc14: Invalid CC number/value (%u/%u)", ccno, val14);
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Send MSB (ccno) and LSB (ccno + 32) */
    uint8_t msb = (val14 >> 7) & 0x7F;
    uint8_t lsb = val14 & 0x7F;

    int ret = midix_cc(ctx, ch, ccno, msb, ts);
    if (ret != MIDIX_OK) return ret;

    return midix_cc(ctx, ch, ccno + 32, lsb, ts);
}

/**
 * Send Program Change
 */
int midix_pc(midix_ctx* ctx, int ch, uint8_t program, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_pc: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (program > 127) {
        LOG_ERROR("midix_pc: Invalid program %u", program);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint32_t msg = (0xC0 | (ch - 1)) | (program << 8);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send Channel Pressure
 */
int midix_cp(midix_ctx* ctx, int ch, uint8_t val, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_cp: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (val > 127) {
        LOG_ERROR("midix_cp: Invalid pressure %u", val);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint32_t msg = (0xD0 | (ch - 1)) | (val << 8);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send Pitch Bend
 */
int midix_pb(midix_ctx* ctx, int ch, uint16_t val14, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_pb: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (val14 > 16383) {
        LOG_ERROR("midix_pb: Invalid pitch bend value %u", val14);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t lsb = val14 & 0x7F;
    uint8_t msb = (val14 >> 7) & 0x7F;

    uint32_t msg = (0xE0 | (ch - 1)) | (lsb << 8) | (msb << 16);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send RPN (Registered Parameter Number)
 */
int midix_rpn(midix_ctx* ctx, int ch, uint16_t number, uint16_t value, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_rpn: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (number > 16383 || value > 16383) {
        LOG_ERROR("midix_rpn: Invalid RPN number/value (%u/%u)", number, value);
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Send CC#101 (RPN MSB), CC#100 (RPN LSB), CC#6 (Data MSB), CC#38 (Data LSB) */
    int ret;
    ret = midix_cc(ctx, ch, 101, (number >> 7) & 0x7F, ts);
    if (ret != MIDIX_OK) return ret;

    ret = midix_cc(ctx, ch, 100, number & 0x7F, ts);
    if (ret != MIDIX_OK) return ret;

    ret = midix_cc(ctx, ch, 6, (value >> 7) & 0x7F, ts);
    if (ret != MIDIX_OK) return ret;

    ret = midix_cc(ctx, ch, 38, value & 0x7F, ts);
    return ret;
}

/**
 * Send NRPN (Non-Registered Parameter Number)
 */
int midix_nrpn(midix_ctx* ctx, int ch, uint16_t number, uint16_t value, midix_time_ns ts) {
    if (ch < 1 || ch > 16) {
        LOG_ERROR("midix_nrpn: Invalid channel %d", ch);
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (number > 16383 || value > 16383) {
        LOG_ERROR("midix_nrpn: Invalid NRPN number/value (%u/%u)", number, value);
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Send CC#99 (NRPN MSB), CC#98 (NRPN LSB), CC#6 (Data MSB), CC#38 (Data LSB) */
    int ret;
    ret = midix_cc(ctx, ch, 99, (number >> 7) & 0x7F, ts);
    if (ret != MIDIX_OK) return ret;

    ret = midix_cc(ctx, ch, 98, number & 0x7F, ts);
    if (ret != MIDIX_OK) return ret;

    ret = midix_cc(ctx, ch, 6, (value >> 7) & 0x7F, ts);
    if (ret != MIDIX_OK) return ret;

    ret = midix_cc(ctx, ch, 38, value & 0x7F, ts);
    return ret;
}

/**
 * Send System Real-Time: MIDI Clock
 */
int midix_clock(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xF8;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Real-Time: Start
 */
int midix_start(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xFA;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Real-Time: Stop
 */
int midix_stop(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xFC;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Real-Time: Continue
 */
int midix_continue(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xFB;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Real-Time: Active Sensing
 */
int midix_active_sensing(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xFE;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Common: Time Code Quarter Frame
 */
int midix_time_code(midix_ctx* ctx, uint8_t type, uint8_t hour, uint8_t min,
                    uint8_t sec, uint8_t frame, midix_time_ns ts) {
    if (type > 3 || hour > 23 || min > 59 || sec > 59 || frame > 29) {
        LOG_ERROR("midix_time_code: Invalid parameters");
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Send 8 quarter-frame messages (F1 with 4-bit piece) */
    uint8_t data[8];
    data[0] = frame & 0x0F;
    data[1] = (frame >> 4) & 0x01;
    data[2] = sec & 0x0F;
    data[3] = (sec >> 4) & 0x03;
    data[4] = min & 0x0F;
    data[5] = (min >> 4) & 0x03;
    data[6] = hour & 0x0F;
    data[7] = ((hour >> 4) & 0x01) | ((type & 0x03) << 1);

    for (int i = 0; i < 8; i++) {
        uint32_t msg = 0xF1 | ((i << 4 | data[i]) << 8);
        int ret = send_short_message(ctx, msg, ts);
        if (ret != MIDIX_OK) return ret;
    }

    return MIDIX_OK;
}

/**
 * Send System Common: Song Position Pointer
 */
int midix_song_position(midix_ctx* ctx, uint16_t position, midix_time_ns ts) {
    if (position > 16383) {
        LOG_ERROR("midix_song_position: Invalid position %u", position);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint8_t lsb = position & 0x7F;
    uint8_t msb = (position >> 7) & 0x7F;

    uint32_t msg = 0xF2 | (lsb << 8) | (msb << 16);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Common: Song Select
 */
int midix_song_select(midix_ctx* ctx, uint8_t song, midix_time_ns ts) {
    if (song > 127) {
        LOG_ERROR("midix_song_select: Invalid song %u", song);
        return MIDIX_ERR_INVALID_PARAM;
    }

    uint32_t msg = 0xF3 | (song << 8);
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Common: Tune Request
 */
int midix_tune_request(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xF6;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send System Reset
 */
int midix_reset(midix_ctx* ctx, midix_time_ns ts) {
    uint32_t msg = 0xFF;
    return send_short_message(ctx, msg, ts);
}

/**
 * Send SysEx message
 */
int midix_send_sysex(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts) {
    if (!ctx) {
        LOG_ERROR("midix_send_sysex: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!ctx->is_open) {
        LOG_ERROR("midix_send_sysex: Device not open");
        return MIDIX_ERR_NO_DEVICE;
    }

    if (!data || len < 2) {
        LOG_ERROR("midix_send_sysex: Invalid data or length");
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Validate F0/F7 framing */
    if (data[0] != 0xF0 || data[len - 1] != 0xF7) {
        LOG_ERROR("midix_send_sysex: SysEx must start with F0 and end with F7");
        return MIDIX_ERR_SYSEX_INCOMPLETE;
    }

    /* Allocate buffer structure */
    midix_sysex_buffer* buf = (midix_sysex_buffer*)calloc(1, sizeof(midix_sysex_buffer));
    if (!buf) {
        LOG_ERROR("midix_send_sysex: Out of memory for buffer structure");
        return MIDIX_ERR_OUT_OF_MEMORY;
    }

    /* Copy SysEx data (must persist until callback) */
    buf->data = (uint8_t*)malloc(len);
    if (!buf->data) {
        LOG_ERROR("midix_send_sysex: Out of memory for data copy");
        free(buf);
        return MIDIX_ERR_OUT_OF_MEMORY;
    }
    memcpy(buf->data, data, len);
    buf->len = len;

    /* Prepare MIDIHDR */
    buf->header.lpData = (LPSTR)buf->data;
    buf->header.dwBufferLength = (DWORD)len;
    buf->header.dwFlags = 0;

    MMRESULT res = midiOutPrepareHeader(ctx->midi_out, &buf->header, sizeof(MIDIHDR));
    if (res != MMSYSERR_NOERROR) {
        LOG_ERROR("midiOutPrepareHeader failed: %s", winmm_error_string(res));
        free(buf->data);
        free(buf);
        return MIDIX_ERR_PORT_FAILURE;
    }

    /* Send SysEx (async, callback will free buffer) */
    res = midiOutLongMsg(ctx->midi_out, &buf->header, sizeof(MIDIHDR));
    if (res != MMSYSERR_NOERROR) {
        LOG_ERROR("midiOutLongMsg failed: %s", winmm_error_string(res));
        midiOutUnprepareHeader(ctx->midi_out, &buf->header, sizeof(MIDIHDR));
        free(buf->data);
        free(buf);
        return MIDIX_ERR_PORT_FAILURE;
    }

    /* Add to buffer list */
    EnterCriticalSection(&ctx->sysex_lock);
    buf->next = ctx->sysex_buffers;
    ctx->sysex_buffers = buf;
    LeaveCriticalSection(&ctx->sysex_lock);

    LOG_DEBUG("Sent SysEx (%zu bytes)", len);
    return MIDIX_OK;
}

/**
 * Send raw MIDI bytes
 */
int midix_send_bytes(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts) {
    if (!ctx) {
        LOG_ERROR("midix_send_bytes: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    if (!data || len == 0 || len > 3) {
        LOG_ERROR("midix_send_bytes: Invalid data or length (%zu)", len);
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Pack into WinMM short message format */
    uint32_t msg = 0;
    for (size_t i = 0; i < len; i++) {
        msg |= (data[i] << (i * 8));
    }

    return send_short_message(ctx, msg, ts);
}

/**
 * Send panic (All Notes Off + Sustain Off on all channels)
 */
int midix_panic(midix_ctx* ctx) {
    if (!ctx) {
        LOG_ERROR("midix_panic: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    LOG_INFO("Sending panic (All Notes Off + Sustain Off on all channels)");

    for (int ch = 1; ch <= 16; ch++) {
        /* CC#123 (All Notes Off) */
        int ret = midix_cc(ctx, ch, 123, 0, 0);
        if (ret != MIDIX_OK) {
            LOG_WARN("Failed to send All Notes Off on channel %d", ch);
        }

        /* CC#64 (Sustain Off) */
        ret = midix_cc(ctx, ch, 64, 0, 0);
        if (ret != MIDIX_OK) {
            LOG_WARN("Failed to send Sustain Off on channel %d", ch);
        }
    }

    return MIDIX_OK;
}

/**
 * Flush send queue (stub - no queue in current implementation)
 */
int midix_flush(midix_ctx* ctx) {
    if (!ctx) {
        LOG_ERROR("midix_flush: NULL context");
        return MIDIX_ERR_INVALID_PARAM;
    }

    /* Wait for all SysEx buffers to complete */
    int timeout_ms = 5000;
    int waited_ms = 0;

    while (ctx->sysex_buffers != NULL && waited_ms < timeout_ms) {
        Sleep(10);
        waited_ms += 10;
    }

    if (ctx->sysex_buffers != NULL) {
        LOG_WARN("Flush timeout: SysEx buffers still pending after %d ms", timeout_ms);
        return MIDIX_ERR_TIMEOUT;
    }

    LOG_DEBUG("Flush completed (all SysEx buffers sent)");
    return MIDIX_OK;
}

/**
 * Get library version string
 */
const char* midix_version(void) {
    return "0.1.0";
}

#endif /* _WIN32 */
