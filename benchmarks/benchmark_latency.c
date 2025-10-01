/*
 * benchmark_latency.c - MIDI latency benchmark
 *
 * Measures round-trip latency using IAC Driver (macOS loopback)
 *
 * Methodology:
 * 1. Send Note On to IAC Driver
 * 2. Receive via CoreMIDI callback
 * 3. Calculate timestamp difference
 * 4. Repeat 1000 times, compute statistics
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>

#define NUM_ITERATIONS 1000
#define MIDI_CHANNEL 1
#define MIDI_NOTE 60
#define MIDI_VELOCITY 100

// Globals for receiving MIDI
static MIDIClientRef g_midi_client = 0;
static MIDIPortRef g_midi_input_port = 0;
static MIDIEndpointRef g_iac_source = 0;

// Timing data
static uint64_t g_send_timestamps[NUM_ITERATIONS];
static uint64_t g_recv_timestamps[NUM_ITERATIONS];
static int g_recv_count = 0;

// Mach timebase
static mach_timebase_info_data_t g_timebase;

// Convert mach absolute time to nanoseconds
static uint64_t mach_to_ns(uint64_t mach_time) {
    return (mach_time * g_timebase.numer) / g_timebase.denom;
}

// MIDI receive callback
static void midi_read_callback(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon) {
    (void)readProcRefCon;
    (void)srcConnRefCon;

    const MIDIPacket *packet = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; i++) {
        // Check if it's our Note On message
        if (packet->length >= 3 &&
            (packet->data[0] & 0xF0) == 0x90 &&  // Note On
            packet->data[1] == MIDI_NOTE &&
            packet->data[2] == MIDI_VELOCITY) {

            if (g_recv_count < NUM_ITERATIONS) {
                g_recv_timestamps[g_recv_count] = mach_absolute_time();
                g_recv_count++;
            }
        }

        packet = MIDIPacketNext(packet);
    }
}

// Setup MIDI input
static int setup_midi_input(void) {
    OSStatus status;

    // Get timebase
    mach_timebase_info(&g_timebase);

    // Create MIDI client
    status = MIDIClientCreate(CFSTR("midix-bench-input"), NULL, NULL, &g_midi_client);
    if (status != noErr) {
        fprintf(stderr, "Failed to create MIDI client: %d\n", (int)status);
        return -1;
    }

    // Create input port
    status = MIDIInputPortCreate(g_midi_client, CFSTR("Input"), midi_read_callback, NULL, &g_midi_input_port);
    if (status != noErr) {
        fprintf(stderr, "Failed to create input port: %d\n", (int)status);
        return -1;
    }

    // Find IAC Driver source
    ItemCount num_sources = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < num_sources; i++) {
        MIDIEndpointRef source = MIDIGetSource(i);
        CFStringRef name = NULL;
        MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name);

        if (name) {
            char name_buf[256];
            CFStringGetCString(name, name_buf, sizeof(name_buf), kCFStringEncodingUTF8);
            CFRelease(name);

            // Look for IAC Driver (Japanese: "バス1" or English: "IAC Driver Bus 1")
            if (strstr(name_buf, "バス1") || strstr(name_buf, "IAC")) {
                g_iac_source = source;
                printf("Found IAC source: %s\n", name_buf);
                break;
            }
        }
    }

    if (!g_iac_source) {
        fprintf(stderr, "IAC Driver not found. Please enable it in Audio MIDI Setup.\n");
        return -1;
    }

    // Connect to IAC source
    status = MIDIPortConnectSource(g_midi_input_port, g_iac_source, NULL);
    if (status != noErr) {
        fprintf(stderr, "Failed to connect to IAC source: %d\n", (int)status);
        return -1;
    }

    printf("MIDI input setup complete\n");
    return 0;
}

// Cleanup MIDI input
static void cleanup_midi_input(void) {
    if (g_midi_input_port) {
        MIDIPortDisconnectSource(g_midi_input_port, g_iac_source);
        MIDIPortDispose(g_midi_input_port);
    }
    if (g_midi_client) {
        MIDIClientDispose(g_midi_client);
    }
}

// Calculate statistics
static void calculate_stats(double *mean, double *stddev, double *min, double *max) {
    double sum = 0.0;
    *min = 1e9;
    *max = 0.0;

    for (int i = 0; i < g_recv_count; i++) {
        double latency_ns = (double)(g_recv_timestamps[i] - g_send_timestamps[i]);
        latency_ns = mach_to_ns((uint64_t)latency_ns);
        double latency_us = latency_ns / 1000.0;

        sum += latency_us;
        if (latency_us < *min) *min = latency_us;
        if (latency_us > *max) *max = latency_us;
    }

    *mean = sum / g_recv_count;

    // Calculate standard deviation
    double variance = 0.0;
    for (int i = 0; i < g_recv_count; i++) {
        double latency_ns = (double)(g_recv_timestamps[i] - g_send_timestamps[i]);
        latency_ns = mach_to_ns((uint64_t)latency_ns);
        double latency_us = latency_ns / 1000.0;
        double diff = latency_us - *mean;
        variance += diff * diff;
    }
    *stddev = sqrt(variance / g_recv_count);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("=== midix Latency Benchmark ===\n\n");
    printf("Configuration:\n");
    printf("  Iterations: %d\n", NUM_ITERATIONS);
    printf("  Method: IAC Driver loopback (round-trip)\n");
    printf("  Message: Note On (ch=%d, note=%d, vel=%d)\n\n", MIDI_CHANNEL, MIDI_NOTE, MIDI_VELOCITY);

    // Setup MIDI input
    if (setup_midi_input() != 0) {
        return 1;
    }

    // Setup midix output
    midix_ctx* ctx = midix_create("midix-bench");
    if (!ctx) {
        fprintf(stderr, "Failed to create midix context\n");
        cleanup_midi_input();
        return 1;
    }

    // Open IAC output
    if (midix_open_output_by_name(ctx, "IAC") != MIDIX_OK &&
        midix_open_output_by_name(ctx, "バス1") != MIDIX_OK) {
        fprintf(stderr, "Failed to open IAC Driver output\n");
        midix_destroy(ctx);
        cleanup_midi_input();
        return 1;
    }

    printf("Starting benchmark...\n");

    // Warmup
    for (int i = 0; i < 10; i++) {
        midix_note_on(ctx, MIDI_CHANNEL, MIDI_NOTE, MIDI_VELOCITY, 0);
        usleep(10000);  // 10ms
        midix_note_off(ctx, MIDI_CHANNEL, MIDI_NOTE, 0, 0);
        usleep(10000);
    }

    g_recv_count = 0;

    // Benchmark loop
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        g_send_timestamps[i] = mach_absolute_time();
        midix_note_on(ctx, MIDI_CHANNEL, MIDI_NOTE, MIDI_VELOCITY, 0);

        // Wait for receive (with timeout)
        int timeout = 0;
        while (g_recv_count <= i && timeout < 1000) {
            usleep(100);  // 0.1ms
            timeout++;
        }

        if (g_recv_count <= i) {
            fprintf(stderr, "Warning: Message %d not received\n", i);
        }

        // Send Note Off
        midix_note_off(ctx, MIDI_CHANNEL, MIDI_NOTE, 0, 0);

        // Small delay between iterations
        usleep(1000);  // 1ms

        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d\n", i + 1, NUM_ITERATIONS);
        }
    }

    printf("\nBenchmark complete. Received %d/%d messages.\n\n", g_recv_count, NUM_ITERATIONS);

    // Calculate statistics
    if (g_recv_count > 0) {
        double mean, stddev, min, max;
        calculate_stats(&mean, &stddev, &min, &max);

        printf("Results:\n");
        printf("  Mean latency:   %.2f µs\n", mean);
        printf("  Std deviation:  %.2f µs\n", stddev);
        printf("  Min latency:    %.2f µs\n", min);
        printf("  Max latency:    %.2f µs\n", max);
        printf("  Success rate:   %.1f%%\n", 100.0 * g_recv_count / NUM_ITERATIONS);
    }

    // Cleanup
    midix_destroy(ctx);
    cleanup_midi_input();

    return 0;
}
