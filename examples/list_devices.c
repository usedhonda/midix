/*
 * list_devices.c - MIDI device enumeration example
 *
 * Demonstrates how to list all available MIDI output devices.
 * Useful for discovering device names for use with midix_open_output_by_name().
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== midix Device List Example ===\n\n");

    char** names = NULL;
    int count = 0;

    // List all available MIDI output devices
    int result = midix_list_outputs(&names, &count);
    if (result != MIDIX_OK) {
        fprintf(stderr, "ERROR: Failed to list devices (error %d)\n", result);
        return 1;
    }

    if (count == 0) {
        printf("No MIDI output devices found.\n");
        printf("\nmacOS: Ensure IAC Driver is enabled in Audio MIDI Setup\n");
        printf("Windows: Install loopMIDI or virtualMIDI for virtual ports\n");
        return 0;
    }

    printf("Found %d MIDI output device%s:\n\n", count, count == 1 ? "" : "s");

    for (int i = 0; i < count; i++) {
        printf("  [%d] %s\n", i, names[i]);
        free(names[i]);  // Free individual device name
    }
    free(names);  // Free array

    printf("\nUsage with midix CLI:\n");
    printf("  midix dev \"<device name>\" on 60 100\n");
    printf("\nUsage with midix API:\n");
    printf("  midix_open_output_by_name(ctx, \"<device name>\");\n");

    return 0;
}

/*
 * COMPILATION & EXECUTION:
 *
 * Build with CMake:
 *   mkdir build && cd build
 *   cmake ..
 *   make list_devices
 *
 * Run:
 *   ./list_devices
 *
 * Expected output (macOS with IAC Driver enabled):
 *   Found 1 MIDI output device:
 *     [0] IAC Driver Bus 1
 *
 * Expected output (Windows with loopMIDI):
 *   Found 2 MIDI output devices:
 *     [0] Microsoft GS Wavetable Synth
 *     [1] loopMIDI Port
 *
 * Notes:
 *   - Device names are platform and configuration specific
 *   - Virtual MIDI ports (IAC/loopMIDI) must be enabled/installed
 *   - Some DAWs create their own virtual ports when running
 */
