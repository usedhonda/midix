/*
 * midix.h - SendMIDI-compatible MIDI transmission library
 *
 * A lightweight, JUCE-free MIDI sending library with SendMIDI CLI compatibility.
 * Supports macOS (CoreMIDI) and Windows (WinMM).
 *
 * License: MIT
 * Version: 0.1.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * Opaque context handle for MIDI operations.
 * Each context maintains its own output port, scheduler, and worker thread.
 */
typedef struct midix_ctx midix_ctx;

/**
 * Monotonic timestamp in nanoseconds.
 * Use midix_now() to get current time, or 0 for immediate send.
 */
typedef uint64_t midix_time_ns;

/* ============================================================================
 * Error Codes
 * ========================================================================== */

#define MIDIX_OK                    0   /**< Success */
#define MIDIX_ERR_NO_DEVICE        -1   /**< No device selected or found */
#define MIDIX_ERR_PORT_FAILURE     -2   /**< Port creation/connection failed */
#define MIDIX_ERR_INVALID_MSG      -3   /**< Invalid MIDI message */
#define MIDIX_ERR_SCHED_OVERFLOW   -4   /**< Send queue full */
#define MIDIX_ERR_SYSEX_INCOMPLETE -5   /**< SysEx missing F0/F7 */
#define MIDIX_ERR_TIMEOUT          -6   /**< Send completion timeout */
#define MIDIX_ERR_INVALID_PARAM    -7   /**< Invalid parameter value */
#define MIDIX_ERR_OUT_OF_MEMORY    -8   /**< Memory allocation failed */
#define MIDIX_ERR_NO_MEMORY        -8   /**< Alias for MIDIX_ERR_OUT_OF_MEMORY */
#define MIDIX_ERR_NOT_SUPPORTED    -9   /**< Feature not supported on this OS */

/* ============================================================================
 * Context Management
 * ========================================================================== */

/**
 * Create a new MIDI context.
 *
 * @param client_name Name for the MIDI client (displayed in system MIDI setup)
 * @return Context handle, or NULL on failure
 *
 * Example:
 *   midix_ctx* ctx = midix_create("MyApp");
 *   if (!ctx) { handle_error(); }
 */
midix_ctx* midix_create(const char* client_name);

/**
 * Destroy a MIDI context and release all resources.
 * Automatically flushes pending messages and joins worker thread.
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void midix_destroy(midix_ctx* ctx);

/* ============================================================================
 * Device Management
 * ========================================================================== */

/**
 * List available MIDI output devices.
 *
 * @param names Output array of device name strings (caller must free)
 * @param count Output number of devices found
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   char** names = NULL;
 *   int count = 0;
 *   if (midix_list_outputs(&names, &count) == MIDIX_OK) {
 *     for (int i = 0; i < count; i++) {
 *       printf("%d: %s\n", i, names[i]);
 *       free(names[i]);
 *     }
 *     free(names);
 *   }
 */
int midix_list_outputs(char*** names, int* count);

/**
 * Open MIDI output device by name (substring match, case-insensitive).
 *
 * @param ctx Context handle
 * @param name_substring Device name or partial match (e.g., "IAC", "loopMIDI")
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Note: Automatically closes previously opened device.
 *
 * Example:
 *   if (midix_open_output_by_name(ctx, "IAC Driver") != MIDIX_OK) {
 *     fprintf(stderr, "Failed to open device\n");
 *   }
 */
int midix_open_output_by_name(midix_ctx* ctx, const char* name_substring);

/**
 * Close currently open output device.
 *
 * @param ctx Context handle
 */
void midix_close_output(midix_ctx* ctx);

/**
 * Create a virtual MIDI output port (macOS only).
 * On macOS, creates a MIDISource visible to other applications.
 * On Windows, returns MIDIX_ERR_NOT_SUPPORTED (requires external loopMIDI/virtualMIDI).
 *
 * @param ctx Context handle
 * @param port_name Name for the virtual port
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   if (midix_create_virtual_output(ctx, "MyApp Output") == MIDIX_OK) {
 *     printf("Virtual port created\n");
 *   }
 */
int midix_create_virtual_output(midix_ctx* ctx, const char* port_name);

/**
 * Destroy virtual MIDI output port.
 *
 * @param ctx Context handle
 */
void midix_drop_virtual_output(midix_ctx* ctx);

/* ============================================================================
 * Timing
 * ========================================================================== */

/**
 * Get current monotonic time in nanoseconds.
 * Uses AudioGetCurrentHostTime (macOS) or QueryPerformanceCounter (Windows).
 *
 * @return Current time in nanoseconds
 *
 * Example:
 *   midix_time_ns now = midix_now();
 *   midix_time_ns future = now + 1000000000; // +1 second
 *   midix_note_on(ctx, 1, 60, 100, future);
 */
midix_time_ns midix_now(void);

/* ============================================================================
 * Channel Voice Messages
 * ========================================================================== */

/**
 * Send Note On message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param note Note number (0-127)
 * @param vel Velocity (0-127, 0 = note off on some devices)
 * @param ts Timestamp (0 = immediate, or value from midix_now())
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_note_on(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts);

/**
 * Send Note Off message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param note Note number (0-127)
 * @param vel Release velocity (0-127)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_note_off(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts);

/**
 * Send Polyphonic Aftertouch (Poly Pressure) message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param note Note number (0-127)
 * @param pressure Pressure value (0-127)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_poly_pressure(midix_ctx* ctx, int ch, uint8_t note, uint8_t pressure, midix_time_ns ts);

/**
 * Send Control Change (CC) message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param ccno Controller number (0-127)
 * @param val Controller value (0-127)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   midix_cc(ctx, 1, 64, 127, 0); // Sustain pedal on
 */
int midix_cc(midix_ctx* ctx, int ch, uint8_t ccno, uint8_t val, midix_time_ns ts);

/**
 * Send 14-bit Control Change (CC) message.
 * Sends MSB (ccno) and LSB (ccno+32) as two separate CC messages.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param ccno Controller number MSB (0-31)
 * @param val14 14-bit value (0-16383)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   midix_cc14(ctx, 1, 1, 8192, 0); // Modulation wheel center
 */
int midix_cc14(midix_ctx* ctx, int ch, uint8_t ccno, uint16_t val14, midix_time_ns ts);

/**
 * Send Program Change message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param program Program number (0-127)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_pc(midix_ctx* ctx, int ch, uint8_t program, midix_time_ns ts);

/**
 * Send Channel Pressure (Aftertouch) message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param val Pressure value (0-127)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_cp(midix_ctx* ctx, int ch, uint8_t val, midix_time_ns ts);

/**
 * Send Pitch Bend message.
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param val14 14-bit pitch bend value (0-16383, 8192 = center)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   midix_pb(ctx, 1, 8192, 0); // Center position
 *   midix_pb(ctx, 1, 16383, 0); // Maximum up
 */
int midix_pb(midix_ctx* ctx, int ch, uint16_t val14, midix_time_ns ts);

/* ============================================================================
 * RPN/NRPN
 * ========================================================================== */

/**
 * Send Registered Parameter Number (RPN) message.
 * Sends CC#101 (MSB), CC#100 (LSB), CC#6 (data MSB), CC#38 (data LSB).
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param number RPN number (0-16383)
 * @param value RPN value (0-16383)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   midix_rpn(ctx, 1, 0, 256, 0); // Pitch bend sensitivity = 2 semitones
 */
int midix_rpn(midix_ctx* ctx, int ch, uint16_t number, uint16_t value, midix_time_ns ts);

/**
 * Send Non-Registered Parameter Number (NRPN) message.
 * Sends CC#99 (MSB), CC#98 (LSB), CC#6 (data MSB), CC#38 (data LSB).
 *
 * @param ctx Context handle
 * @param ch MIDI channel (1-16)
 * @param number NRPN number (0-16383)
 * @param value NRPN value (0-16383)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_nrpn(midix_ctx* ctx, int ch, uint16_t number, uint16_t value, midix_time_ns ts);

/* ============================================================================
 * System Real-Time Messages
 * ========================================================================== */

/**
 * Send MIDI Clock (0xF8) message.
 * Typically sent 24 times per quarter note.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example (120 BPM = 2 beats/sec = 48 clocks/sec):
 *   for (int i = 0; i < 24; i++) {
 *     midix_clock(ctx, midix_now() + i * 20833333); // ~20.83ms intervals
 *   }
 */
int midix_clock(midix_ctx* ctx, midix_time_ns ts);

/**
 * Send Start (0xFA) message.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_start(midix_ctx* ctx, midix_time_ns ts);

/**
 * Send Stop (0xFC) message.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_stop(midix_ctx* ctx, midix_time_ns ts);

/**
 * Send Continue (0xFB) message.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_continue(midix_ctx* ctx, midix_time_ns ts);

/**
 * Send Active Sensing (0xFE) message.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_active_sensing(midix_ctx* ctx, midix_time_ns ts);

/* ============================================================================
 * System Common Messages
 * ========================================================================== */

/**
 * Send MIDI Time Code (MTC) Quarter Frame messages.
 * Sends 8 consecutive quarter-frame messages for full timecode.
 *
 * @param ctx Context handle
 * @param type Frame type (0=24fps, 1=25fps, 2=30fps drop, 3=30fps)
 * @param hour Hours (0-23)
 * @param min Minutes (0-59)
 * @param sec Seconds (0-59)
 * @param frame Frame number (0-29 depending on type)
 * @param ts Timestamp for first quarter frame (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_time_code(midix_ctx* ctx, uint8_t type, uint8_t hour, uint8_t min,
                    uint8_t sec, uint8_t frame, midix_time_ns ts);

/**
 * Send Song Position Pointer (0xF2) message.
 *
 * @param ctx Context handle
 * @param position Position in MIDI beats (14-bit, 0-16383)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_song_position(midix_ctx* ctx, uint16_t position, midix_time_ns ts);

/**
 * Send Song Select (0xF3) message.
 *
 * @param ctx Context handle
 * @param song Song number (0-127)
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_song_select(midix_ctx* ctx, uint8_t song, midix_time_ns ts);

/**
 * Send Tune Request (0xF6) message.
 * Requests analog synthesizers to retune oscillators.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_tune_request(midix_ctx* ctx, midix_time_ns ts);

/**
 * Send System Reset (0xFF) message.
 * Resets all receivers to power-up status.
 *
 * @param ctx Context handle
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 */
int midix_reset(midix_ctx* ctx, midix_time_ns ts);

/* ============================================================================
 * SysEx
 * ========================================================================== */

/**
 * Send System Exclusive (SysEx) message.
 * Data must start with 0xF0 and end with 0xF7.
 * Library handles large messages with OS-specific APIs (MIDISendSysex, midiOutLongMsg).
 *
 * @param ctx Context handle
 * @param data SysEx data including F0 and F7
 * @param len Length of data in bytes
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, MIDIX_ERR_SYSEX_INCOMPLETE if F0/F7 missing
 *
 * Example:
 *   uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7}; // GM System On
 *   midix_send_sysex(ctx, sysex, sizeof(sysex), 0);
 */
int midix_send_sysex(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts);

/* ============================================================================
 * Raw Send
 * ========================================================================== */

/**
 * Send raw MIDI byte sequence.
 * For complete control over MIDI stream (Running Status, etc.).
 * No validation performed - caller must ensure valid MIDI data.
 *
 * @param ctx Context handle
 * @param data Raw MIDI bytes
 * @param len Length of data in bytes
 * @param ts Timestamp (0 = immediate)
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   uint8_t raw[] = {0x90, 0x3C, 0x64}; // Note On, Ch1, C4, vel=100
 *   midix_send_bytes(ctx, raw, 3, 0);
 */
int midix_send_bytes(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * Send panic message - All Notes Off + Sustain Off on all channels.
 * Sends CC#123 (All Notes Off) and CC#64 (Sustain=0) on channels 1-16.
 *
 * @param ctx Context handle
 * @return MIDIX_OK on success, negative error code on failure
 *
 * Example:
 *   midix_panic(ctx); // Silence all stuck notes
 */
int midix_panic(midix_ctx* ctx);

/**
 * Flush send queue and wait for all messages to complete.
 * Blocks until worker thread has sent all pending messages.
 * Optional - messages are automatically sent asynchronously.
 *
 * @param ctx Context handle
 * @return MIDIX_OK on success, MIDIX_ERR_TIMEOUT if queue doesn't drain
 *
 * Example:
 *   midix_note_on(ctx, 1, 60, 100, 0);
 *   midix_flush(ctx); // Ensure note sent before exiting
 */
int midix_flush(midix_ctx* ctx);

/* ============================================================================
 * Version Information
 * ========================================================================== */

#define MIDIX_VERSION_MAJOR 0
#define MIDIX_VERSION_MINOR 1
#define MIDIX_VERSION_PATCH 0

/**
 * Get library version string.
 *
 * @return Version string (e.g., "0.1.0")
 */
const char* midix_version(void);

#ifdef __cplusplus
}
#endif
