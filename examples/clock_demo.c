/*
 * clock_demo.c - MIDI Clock transmission example
 *
 * Demonstrates:
 * - MIDI Clock generation at specified BPM
 * - Start/Stop/Continue messages
 * - High-precision timing with midix scheduler
 *
 * MIDI Clock basics:
 * - 24 clock pulses per quarter note (PPQN = 24)
 * - At 120 BPM: 2 beats/sec = 48 clocks/sec = ~20.83ms/clock
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

// Global flag for graceful shutdown
static volatile bool g_running = true;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

/**
 * Calculate clock interval in nanoseconds for given BPM.
 * MIDI Clock: 24 PPQN (pulses per quarter note)
 */
static uint64_t bpm_to_clock_interval_ns(double bpm) {
    // 1 minute = 60 seconds = 60,000,000,000 nanoseconds
    // clocks_per_minute = BPM * 24 (24 clocks per beat)
    // interval = 60,000,000,000 / clocks_per_minute
    double clocks_per_minute = bpm * 24.0;
    double interval_ns = 60000000000.0 / clocks_per_minute;
    return (uint64_t)interval_ns;
}

static void run_clock_demo(midix_ctx* ctx, double bpm, int duration_sec) {
    printf("\n--- Starting MIDI Clock at %.1f BPM ---\n", bpm);

    uint64_t interval_ns = bpm_to_clock_interval_ns(bpm);
    printf("Clock interval: %.3f ms\n", interval_ns / 1000000.0);

    // Send Start message
    printf("Sending START...\n");
    midix_start(ctx, 0);
    usleep(10000);  // Small delay after start

    midix_time_ns start_time = midix_now();
    midix_time_ns end_time = start_time + ((uint64_t)duration_sec * 1000000000ULL);

    int clock_count = 0;
    int beat_count = 0;

    // Schedule initial batch of clocks
    midix_time_ns next_clock_time = start_time;

    printf("Sending clocks for %d seconds (Ctrl+C to stop early)...\n", duration_sec);

    while (g_running && midix_now() < end_time) {
        // Send clock
        midix_clock(ctx, next_clock_time);
        clock_count++;

        // Track beats (24 clocks = 1 beat)
        if (clock_count % 24 == 0) {
            beat_count++;
            printf("  Beat %d (clock %d)\n", beat_count, clock_count);
        }

        // Calculate next clock time
        next_clock_time += interval_ns;

        // Sleep until next clock (with small margin for precision)
        midix_time_ns now = midix_now();
        if (next_clock_time > now) {
            int64_t sleep_ns = next_clock_time - now;
            // Sleep 90% of remaining time to avoid overshooting
            if (sleep_ns > 1000000) {  // > 1ms
                usleep((sleep_ns * 9 / 10) / 1000);
            }
        }
    }

    // Send Stop message
    printf("\nSending STOP...\n");
    midix_stop(ctx, 0);

    printf("Clock demo complete: %d clocks sent (%d beats)\n", clock_count, beat_count);
}

static void demo_start_stop_continue(midix_ctx* ctx) {
    printf("\n--- Start/Stop/Continue Demo ---\n");

    printf("Sending START...\n");
    midix_start(ctx, 0);
    usleep(100000);

    printf("Sending 24 clocks (1 beat)...\n");
    for (int i = 0; i < 24; i++) {
        midix_clock(ctx, 0);
        usleep(20000);  // ~50 clocks/sec
    }

    printf("Sending STOP...\n");
    midix_stop(ctx, 0);
    usleep(500000);

    printf("Sending CONTINUE...\n");
    midix_continue(ctx, 0);
    usleep(100000);

    printf("Sending 24 more clocks...\n");
    for (int i = 0; i < 24; i++) {
        midix_clock(ctx, 0);
        usleep(20000);
    }

    printf("Sending STOP...\n");
    midix_stop(ctx, 0);
    usleep(200000);
}

int main(int argc, char* argv[]) {
    // Parse arguments
    const char* device_name = "IAC";
    double bpm = 120.0;
    int duration = 4;  // seconds

    if (argc > 1) device_name = argv[1];
    if (argc > 2) {
        bpm = atof(argv[2]);
        if (bpm < 20.0 || bpm > 300.0) {
            fprintf(stderr, "ERROR: BPM must be between 20 and 300\n");
            return 1;
        }
    }
    if (argc > 3) {
        duration = atoi(argv[3]);
        if (duration < 1 || duration > 60) {
            fprintf(stderr, "ERROR: Duration must be between 1 and 60 seconds\n");
            return 1;
        }
    }

    printf("=== midix MIDI Clock Demo ===\n");
    printf("Device: %s\n", device_name);
    printf("BPM: %.1f\n", bpm);
    printf("Duration: %d seconds\n", duration);

    // Setup signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create context
    midix_ctx* ctx = midix_create("ClockDemo");
    if (!ctx) {
        fprintf(stderr, "ERROR: Failed to create MIDI context\n");
        return 1;
    }

    // Open device
    int result = midix_open_output_by_name(ctx, device_name);
    if (result != MIDIX_OK) {
        fprintf(stderr, "ERROR: Failed to open device '%s' (error %d)\n",
                device_name, result);
        midix_destroy(ctx);
        return 1;
    }

    printf("✓ Connected to device matching '%s'\n", device_name);

    // Run demos
    demo_start_stop_continue(ctx);
    run_clock_demo(ctx, bpm, duration);

    // Cleanup
    printf("\n--- Cleanup ---\n");
    midix_flush(ctx);
    midix_destroy(ctx);
    printf("✓ Context destroyed\n");

    return 0;
}

/*
 * COMPILATION & EXECUTION:
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   make clock_demo
 *
 * Run:
 *   ./clock_demo                              # 120 BPM, 4 seconds, IAC Driver
 *   ./clock_demo "IAC" 140 10                 # 140 BPM, 10 seconds
 *   ./clock_demo "loopMIDI" 90 5              # 90 BPM, 5 seconds
 *
 * Monitor with:
 *   receivemidi dev "IAC"
 *   # or use a DAW with MIDI clock sync enabled
 *
 * Expected output:
 *   - START message
 *   - Regular MIDI Clock messages (0xF8) at precise intervals
 *   - Beat counter every 24 clocks
 *   - STOP message
 *
 * DAW Sync Testing:
 *   1. Configure DAW to sync to external MIDI clock
 *   2. Set DAW input to IAC Driver (or loopMIDI)
 *   3. Run this program
 *   4. DAW should start playing at specified BPM
 *
 * Timing Quality:
 *   - Jitter should be < 1ms on modern systems
 *   - Use MIDI monitoring tools to measure precision
 *   - Performance may vary under high system load
 *
 * Notes:
 *   - MIDI Clock is 24 PPQN (Pulses Per Quarter Note)
 *   - Some devices require START before accepting clocks
 *   - CONTINUE resumes from current position (vs START = reset to 0)
 *   - Press Ctrl+C to stop early
 */
