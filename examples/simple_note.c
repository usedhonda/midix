/*
 * simple_note.c - Minimal MIDI note sending example
 *
 * Demonstrates basic usage of the midix library:
 * - Creating a context
 * - Opening an output device
 * - Sending a note on/off pair
 * - Cleanup
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // for usleep

int main(int argc, char* argv[]) {
    const char* device_name = (argc > 1) ? argv[1] : "IAC";

    printf("=== midix Simple Note Example ===\n");
    printf("Sending a middle C (note 60) to device matching '%s'\n\n", device_name);

    // Step 1: Create MIDI context
    midix_ctx* ctx = midix_create("SimpleNoteExample");
    if (!ctx) {
        fprintf(stderr, "ERROR: Failed to create MIDI context\n");
        return 1;
    }
    printf("✓ MIDI context created\n");

    // Step 2: Open output device by name (substring match)
    int result = midix_open_output_by_name(ctx, device_name);
    if (result != MIDIX_OK) {
        fprintf(stderr, "ERROR: Failed to open device matching '%s' (error %d)\n",
                device_name, result);
        fprintf(stderr, "HINT: Use 'midix list' to see available devices\n");
        midix_destroy(ctx);
        return 1;
    }
    printf("✓ Opened device matching '%s'\n", device_name);

    // Step 3: Send Note On (Channel 1, Middle C, velocity 100)
    result = midix_note_on(ctx, 1, 60, 100, 0);
    if (result != MIDIX_OK) {
        fprintf(stderr, "ERROR: Failed to send note on (error %d)\n", result);
        midix_destroy(ctx);
        return 1;
    }
    printf("✓ Note On sent: Ch1, C4 (60), velocity 100\n");

    // Step 4: Wait 500ms
    usleep(500000);

    // Step 5: Send Note Off (Channel 1, Middle C, velocity 0)
    result = midix_note_off(ctx, 1, 60, 0, 0);
    if (result != MIDIX_OK) {
        fprintf(stderr, "ERROR: Failed to send note off (error %d)\n", result);
        midix_destroy(ctx);
        return 1;
    }
    printf("✓ Note Off sent: Ch1, C4 (60)\n");

    // Step 6: Clean up
    midix_destroy(ctx);
    printf("✓ Context destroyed\n");

    printf("\nSuccess! Note sent and cleaned up.\n");
    return 0;
}

/*
 * COMPILATION & EXECUTION:
 *
 * Build with CMake:
 *   mkdir build && cd build
 *   cmake ..
 *   make simple_note
 *
 * Run:
 *   ./simple_note                 # Uses "IAC" as default
 *   ./simple_note "loopMIDI"      # Specify device name
 *
 * macOS Testing with IAC Driver:
 *   1. Open Audio MIDI Setup.app
 *   2. Window → Show MIDI Studio
 *   3. Double-click "IAC Driver"
 *   4. Check "Device is online"
 *   5. Run this program
 *   6. Monitor with: receivemidi dev "IAC" or a DAW
 *
 * Expected output:
 *   Note On: Channel 1, Note 60 (C4), Velocity 100
 *   (500ms pause)
 *   Note Off: Channel 1, Note 60 (C4)
 */
