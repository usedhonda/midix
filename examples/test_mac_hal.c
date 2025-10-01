/*
 * test_mac_hal.c - Basic macOS HAL functionality test
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("=== midix macOS HAL Test ===\n\n");

    // Create context
    midix_ctx* ctx = midix_create("midix-test");
    if (!ctx) {
        fprintf(stderr, "Failed to create MIDI context\n");
        return 1;
    }

    printf("✓ Created MIDI context\n");

    // List available outputs
    printf("\nAvailable MIDI outputs:\n");
    char** names = NULL;
    int count = 0;

    if (midix_list_outputs(&names, &count) == MIDIX_OK) {
        for (int i = 0; i < count; i++) {
            printf("  %d: %s\n", i, names[i]);
            free(names[i]);
        }
        free(names);
    } else {
        printf("  (none found)\n");
    }

    // Try to open IAC Driver (macOS built-in virtual MIDI bus)
    printf("\nAttempting to open 'IAC' device...\n");
    int result = midix_open_output_by_name(ctx, "IAC");

    if (result == MIDIX_OK) {
        printf("✓ Opened IAC device\n");

        // Send a test note
        printf("\nSending test note (C4, 100ms)...\n");
        midix_note_on(ctx, 1, 60, 100, 0);
        usleep(100000); // 100ms
        midix_note_off(ctx, 1, 60, 0, 0);

        printf("✓ Note sent\n");

        // Send test CC
        printf("\nSending test CC (Modulation)...\n");
        midix_cc(ctx, 1, 1, 64, 0);
        printf("✓ CC sent\n");

        // Send test SysEx (GM System On)
        printf("\nSending GM System On SysEx...\n");
        uint8_t gm_on[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
        midix_send_sysex(ctx, gm_on, sizeof(gm_on), 0);
        printf("✓ SysEx sent\n");

        midix_close_output(ctx);
    } else {
        printf("✗ Could not open IAC device (result: %d)\n", result);
        printf("  Tip: Enable IAC Driver in Audio MIDI Setup\n");
    }

    // Test virtual port creation
    printf("\nCreating virtual output 'midix-test-out'...\n");
    result = midix_create_virtual_output(ctx, "midix-test-out");

    if (result == MIDIX_OK) {
        printf("✓ Virtual port created\n");
        printf("  Check Audio MIDI Setup to see the port\n");
        printf("  Waiting 5 seconds...\n");

        // Send some notes to the virtual port
        for (int i = 0; i < 5; i++) {
            midix_note_on(ctx, 1, 60 + i, 100, 0);
            usleep(200000);
            midix_note_off(ctx, 1, 60 + i, 0, 0);
            usleep(200000);
        }

        midix_drop_virtual_output(ctx);
        printf("✓ Virtual port dropped\n");
    } else {
        printf("✗ Could not create virtual port (result: %d)\n", result);
    }

    // Test panic
    printf("\nSending panic (All Notes Off)...\n");
    midix_panic(ctx);
    printf("✓ Panic sent\n");

    // Get timing info
    printf("\nCurrent time: %llu ns\n", midix_now());

    // Cleanup
    printf("\nCleaning up...\n");
    midix_destroy(ctx);
    printf("✓ Context destroyed\n");

    printf("\n=== Test completed successfully ===\n");
    return 0;
}
