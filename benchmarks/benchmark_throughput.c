/*
 * benchmark_throughput.c - MIDI throughput benchmark
 *
 * Measures maximum sustained throughput (messages per second)
 *
 * Methodology:
 * 1. Send continuous stream of MIDI messages
 * 2. Measure actual send rate
 * 3. Monitor CPU and memory usage
 * 4. Test different message types and burst sizes
 *
 * License: MIT
 */

#include "midix.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <mach/mach.h>

#define TEST_DURATION_SEC 5
#define BURST_SIZE 100

// Get CPU usage
static double get_cpu_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
    double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;

    return user_time + sys_time;
}

// Get memory usage (RSS in KB)
static size_t get_memory_usage(void) {
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        return info.resident_size / 1024;  // Convert to KB
    }
    return 0;
}

// Get current time in microseconds
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// Test: Note On/Off bursts
static void test_note_bursts(midix_ctx* ctx, int duration_sec) {
    printf("\n=== Test: Note On/Off Bursts ===\n");

    uint64_t start_time = get_time_us();
    uint64_t end_time = start_time + (duration_sec * 1000000ULL);
    int count = 0;
    double start_cpu = get_cpu_usage();
    size_t start_mem = get_memory_usage();

    while (get_time_us() < end_time) {
        for (int i = 0; i < BURST_SIZE; i++) {
            int note = 60 + (i % 12);  // C4-B4
            midix_note_on(ctx, 1, note, 100, 0);
            midix_note_off(ctx, 1, note, 0, 0);
            count += 2;  // Note On + Note Off
        }
        usleep(100);  // Small delay between bursts
    }

    uint64_t actual_duration_us = get_time_us() - start_time;
    double cpu_time = get_cpu_usage() - start_cpu;
    size_t mem_used = get_memory_usage() - start_mem;

    double throughput = (count * 1000000.0) / actual_duration_us;
    double cpu_percent = (cpu_time * 100.0) / (actual_duration_us / 1000000.0);

    printf("  Messages sent:    %d\n", count);
    printf("  Duration:         %.2f sec\n", actual_duration_us / 1000000.0);
    printf("  Throughput:       %.0f msgs/sec\n", throughput);
    printf("  CPU usage:        %.1f%%\n", cpu_percent);
    printf("  Memory delta:     %zu KB\n", mem_used);
}

// Test: Control Change spam
static void test_cc_spam(midix_ctx* ctx, int duration_sec) {
    printf("\n=== Test: Control Change Spam ===\n");

    uint64_t start_time = get_time_us();
    uint64_t end_time = start_time + (duration_sec * 1000000ULL);
    int count = 0;
    double start_cpu = get_cpu_usage();
    size_t start_mem = get_memory_usage();

    while (get_time_us() < end_time) {
        for (int i = 0; i < BURST_SIZE; i++) {
            int ccno = i % 128;
            int value = (i * 7) % 128;
            midix_cc(ctx, 1, ccno, value, 0);
            count++;
        }
        usleep(50);
    }

    uint64_t actual_duration_us = get_time_us() - start_time;
    double cpu_time = get_cpu_usage() - start_cpu;
    size_t mem_used = get_memory_usage() - start_mem;

    double throughput = (count * 1000000.0) / actual_duration_us;
    double cpu_percent = (cpu_time * 100.0) / (actual_duration_us / 1000000.0);

    printf("  Messages sent:    %d\n", count);
    printf("  Duration:         %.2f sec\n", actual_duration_us / 1000000.0);
    printf("  Throughput:       %.0f msgs/sec\n", throughput);
    printf("  CPU usage:        %.1f%%\n", cpu_percent);
    printf("  Memory delta:     %zu KB\n", mem_used);
}

// Test: Mixed message types
static void test_mixed_messages(midix_ctx* ctx, int duration_sec) {
    printf("\n=== Test: Mixed Message Types ===\n");

    uint64_t start_time = get_time_us();
    uint64_t end_time = start_time + (duration_sec * 1000000ULL);
    int count = 0;
    double start_cpu = get_cpu_usage();
    size_t start_mem = get_memory_usage();

    while (get_time_us() < end_time) {
        for (int i = 0; i < BURST_SIZE / 4; i++) {
            midix_note_on(ctx, 1, 60, 100, 0);
            midix_cc(ctx, 1, 7, 127, 0);
            midix_pb(ctx, 1, 8192, 0);
            midix_pc(ctx, 1, i % 128, 0);
            count += 4;
        }
        usleep(100);
    }

    uint64_t actual_duration_us = get_time_us() - start_time;
    double cpu_time = get_cpu_usage() - start_cpu;
    size_t mem_used = get_memory_usage() - start_mem;

    double throughput = (count * 1000000.0) / actual_duration_us;
    double cpu_percent = (cpu_time * 100.0) / (actual_duration_us / 1000000.0);

    printf("  Messages sent:    %d\n", count);
    printf("  Duration:         %.2f sec\n", actual_duration_us / 1000000.0);
    printf("  Throughput:       %.0f msgs/sec\n", throughput);
    printf("  CPU usage:        %.1f%%\n", cpu_percent);
    printf("  Memory delta:     %zu KB\n", mem_used);
}

// Test: SysEx throughput
static void test_sysex(midix_ctx* ctx, int duration_sec) {
    printf("\n=== Test: SysEx Messages ===\n");

    // Test SysEx: Identity Request
    uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};

    uint64_t start_time = get_time_us();
    uint64_t end_time = start_time + (duration_sec * 1000000ULL);
    int count = 0;
    double start_cpu = get_cpu_usage();
    size_t start_mem = get_memory_usage();

    while (get_time_us() < end_time) {
        for (int i = 0; i < 10; i++) {
            midix_send_sysex(ctx, sysex, sizeof(sysex), 0);
            count++;
        }
        usleep(1000);  // 1ms delay
    }

    uint64_t actual_duration_us = get_time_us() - start_time;
    double cpu_time = get_cpu_usage() - start_cpu;
    size_t mem_used = get_memory_usage() - start_mem;

    double throughput = (count * 1000000.0) / actual_duration_us;
    double cpu_percent = (cpu_time * 100.0) / (actual_duration_us / 1000000.0);

    printf("  Messages sent:    %d\n", count);
    printf("  Duration:         %.2f sec\n", actual_duration_us / 1000000.0);
    printf("  Throughput:       %.0f msgs/sec\n", throughput);
    printf("  CPU usage:        %.1f%%\n", cpu_percent);
    printf("  Memory delta:     %zu KB\n", mem_used);
}

int main(int argc, char** argv) {
    const char* device_name = (argc > 1) ? argv[1] : "バス1";

    printf("=== midix Throughput Benchmark ===\n");
    printf("\nConfiguration:\n");
    printf("  Test duration: %d seconds per test\n", TEST_DURATION_SEC);
    printf("  Burst size: %d messages\n", BURST_SIZE);
    printf("  Target device: %s\n\n", device_name);

    // Create context
    midix_ctx* ctx = midix_create("midix-bench");
    if (!ctx) {
        fprintf(stderr, "Failed to create midix context\n");
        return 1;
    }

    // Open output
    if (midix_open_output_by_name(ctx, device_name) != MIDIX_OK) {
        fprintf(stderr, "Failed to open device: %s\n", device_name);
        fprintf(stderr, "Please enable IAC Driver in Audio MIDI Setup\n");
        midix_destroy(ctx);
        return 1;
    }

    printf("Connected to IAC Driver\n");

    // Run tests
    test_note_bursts(ctx, TEST_DURATION_SEC);
    sleep(1);

    test_cc_spam(ctx, TEST_DURATION_SEC);
    sleep(1);

    test_mixed_messages(ctx, TEST_DURATION_SEC);
    sleep(1);

    test_sysex(ctx, TEST_DURATION_SEC);

    // Cleanup
    printf("\nFlushing queue...\n");
    midix_flush(ctx);
    midix_destroy(ctx);

    printf("\n=== Benchmark Complete ===\n");

    return 0;
}
