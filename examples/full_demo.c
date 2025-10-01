/*
 * full_demo.c - Comprehensive MIDI message demonstration
 *
 * Demonstrates:
 * - Channel Voice messages (CC, PB, PC)
 * - Timestamp scheduling (arpeggio)
 * - SysEx messages (Identity Request)
 * - Panic (All Notes Off)
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void demo_control_change(midix_ctx* ctx) {
    printf("\n--- Control Change Demo ---\n");

    // Modulation Wheel (CC#1)
    printf("Setting Modulation Wheel to 64...\n");
    midix_cc(ctx, 1, 1, 64, 0);
    usleep(200000);

    // Volume (CC#7)
    printf("Setting Volume to 100...\n");
    midix_cc(ctx, 1, 7, 100, 0);
    usleep(200000);

    // Sustain Pedal On (CC#64)
    printf("Sustain Pedal ON...\n");
    midix_cc(ctx, 1, 64, 127, 0);
    usleep(500000);

    // Sustain Pedal Off
    printf("Sustain Pedal OFF...\n");
    midix_cc(ctx, 1, 64, 0, 0);
    usleep(200000);
}

static void demo_pitch_bend(midix_ctx* ctx) {
    printf("\n--- Pitch Bend Demo ---\n");

    // Center (8192)
    printf("Pitch Bend: Center (8192)...\n");
    midix_pb(ctx, 1, 8192, 0);
    usleep(300000);

    // Maximum up (16383)
    printf("Pitch Bend: Maximum Up (16383)...\n");
    midix_pb(ctx, 1, 16383, 0);
    usleep(300000);

    // Maximum down (0)
    printf("Pitch Bend: Maximum Down (0)...\n");
    midix_pb(ctx, 1, 0, 0);
    usleep(300000);

    // Back to center
    printf("Pitch Bend: Back to Center...\n");
    midix_pb(ctx, 1, 8192, 0);
    usleep(200000);
}

static void demo_program_change(midix_ctx* ctx) {
    printf("\n--- Program Change Demo ---\n");

    // Piano (0)
    printf("Program Change: 0 (Acoustic Grand Piano)...\n");
    midix_pc(ctx, 1, 0, 0);
    usleep(200000);

    // Strings (48)
    printf("Program Change: 48 (String Ensemble)...\n");
    midix_pc(ctx, 1, 48, 0);
    usleep(200000);

    // Synth Lead (80)
    printf("Program Change: 80 (Lead 1 - Square)...\n");
    midix_pc(ctx, 1, 80, 0);
    usleep(200000);
}

static void demo_scheduled_arpeggio(midix_ctx* ctx) {
    printf("\n--- Scheduled Arpeggio Demo ---\n");
    printf("Playing C major arpeggio with precise timing...\n");

    // C major arpeggio: C4, E4, G4, C5
    const uint8_t notes[] = {60, 64, 67, 72};
    const int num_notes = sizeof(notes) / sizeof(notes[0]);
    const uint64_t interval_ns = 150000000;  // 150ms

    midix_time_ns start_time = midix_now();

    // Schedule note on events
    for (int i = 0; i < num_notes; i++) {
        midix_time_ns note_time = start_time + (i * interval_ns);
        midix_note_on(ctx, 1, notes[i], 100, note_time);
        printf("  Scheduled Note On: %d at +%dms\n", notes[i], i * 150);
    }

    // Schedule note off events
    for (int i = 0; i < num_notes; i++) {
        midix_time_ns note_time = start_time + ((i + 1) * interval_ns);
        midix_note_off(ctx, 1, notes[i], 0, note_time);
    }

    // Wait for all notes to complete
    usleep((num_notes + 1) * 150000);
    printf("Arpeggio complete!\n");
}

static void demo_sysex(midix_ctx* ctx) {
    printf("\n--- SysEx Demo ---\n");

    // Universal Device Inquiry (Identity Request)
    // F0 7E 7F 06 01 F7
    // This asks all devices to report their identity
    uint8_t identity_request[] = {
        0xF0,  // SysEx start
        0x7E,  // Universal Non-Realtime
        0x7F,  // All devices
        0x06,  // General Information
        0x01,  // Identity Request
        0xF7   // SysEx end
    };

    printf("Sending Universal Device Identity Request...\n");
    int result = midix_send_sysex(ctx, identity_request, sizeof(identity_request), 0);
    if (result == MIDIX_OK) {
        printf("✓ SysEx sent successfully\n");
    } else {
        printf("✗ SysEx send failed (error %d)\n", result);
    }

    usleep(500000);

    // GM System On
    // F0 7E 7F 09 01 F7
    uint8_t gm_system_on[] = {
        0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7
    };

    printf("Sending General MIDI System On...\n");
    result = midix_send_sysex(ctx, gm_system_on, sizeof(gm_system_on), 0);
    if (result == MIDIX_OK) {
        printf("✓ GM System On sent successfully\n");
    } else {
        printf("✗ GM System On failed (error %d)\n", result);
    }

    usleep(200000);
}

static void demo_panic(midix_ctx* ctx) {
    printf("\n--- Panic Demo ---\n");
    printf("Sending All Notes Off + Sustain Off to all channels...\n");

    int result = midix_panic(ctx);
    if (result == MIDIX_OK) {
        printf("✓ Panic sent successfully\n");
    } else {
        printf("✗ Panic failed (error %d)\n", result);
    }

    usleep(200000);
}

int main(int argc, char* argv[]) {
    const char* device_name = (argc > 1) ? argv[1] : "IAC";

    printf("=== midix Full Feature Demo ===\n");
    printf("Device: %s\n", device_name);
    printf("Library version: %s\n", midix_version());

    // Create context
    midix_ctx* ctx = midix_create("FullDemo");
    if (!ctx) {
        fprintf(stderr, "ERROR: Failed to create MIDI context\n");
        return 1;
    }

    // Open device
    int result = midix_open_output_by_name(ctx, device_name);
    if (result != MIDIX_OK) {
        fprintf(stderr, "ERROR: Failed to open device '%s' (error %d)\n",
                device_name, result);
        fprintf(stderr, "HINT: Run 'list_devices' to see available devices\n");
        midix_destroy(ctx);
        return 1;
    }

    printf("✓ Connected to device matching '%s'\n", device_name);

    // Run demonstrations
    demo_control_change(ctx);
    demo_pitch_bend(ctx);
    demo_program_change(ctx);
    demo_scheduled_arpeggio(ctx);
    demo_sysex(ctx);
    demo_panic(ctx);

    // Flush and cleanup
    printf("\n--- Cleanup ---\n");
    printf("Flushing send queue...\n");
    midix_flush(ctx);

    midix_destroy(ctx);
    printf("✓ Context destroyed\n");

    printf("\n=== Demo Complete ===\n");
    return 0;
}

/*
 * COMPILATION & EXECUTION:
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   make full_demo
 *
 * Run:
 *   ./full_demo                  # Uses IAC Driver
 *   ./full_demo "loopMIDI"       # Specify device
 *
 * Monitor with:
 *   receivemidi dev "IAC"
 *   # or open a DAW with a MIDI monitor plugin
 *
 * Expected behavior:
 *   - Control Change messages (Modulation, Volume, Sustain)
 *   - Pitch Bend sweep (down, center, up)
 *   - Program Change (Piano, Strings, Synth Lead)
 *   - Scheduled C major arpeggio with precise timing
 *   - SysEx messages (Identity Request, GM System On)
 *   - Panic (All Notes Off on all channels)
 *
 * Notes:
 *   - Some devices may ignore certain messages (e.g., Program Change)
 *   - SysEx responses depend on connected devices
 *   - Timing precision depends on OS scheduler and device latency
 */
