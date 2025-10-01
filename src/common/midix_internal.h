// midix_internal.h - Internal shared definitions for libmidix
// Copyright (c) 2025 libmidix contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "midix.h"
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Logging
// ============================================================================

typedef enum {
    MIDIX_LOG_ERROR = 0,
    MIDIX_LOG_WARN  = 1,
    MIDIX_LOG_INFO  = 2,
    MIDIX_LOG_DEBUG = 3
} midix_log_level_t;

void midix_log_init(void);
int  midix_log_get_level(void);
void midix_log_write(midix_log_level_t level, const char* fmt, ...);

#define MIDIX_LOG_ERROR(...) midix_log_write(MIDIX_LOG_ERROR, __VA_ARGS__)
#define MIDIX_LOG_WARN(...)  midix_log_write(MIDIX_LOG_WARN,  __VA_ARGS__)
#define MIDIX_LOG_INFO(...)  midix_log_write(MIDIX_LOG_INFO,  __VA_ARGS__)
#define MIDIX_LOG_DEBUG(...) midix_log_write(MIDIX_LOG_DEBUG, __VA_ARGS__)

// ============================================================================
// Scheduler Event Queue
// ============================================================================

typedef enum {
    MIDIX_EVENT_SHORT,   // 1-3 byte MIDI message
    MIDIX_EVENT_SYSEX,   // SysEx message
    MIDIX_EVENT_SHUTDOWN // Shutdown signal
} midix_event_type_t;

typedef struct midix_event {
    midix_event_type_t type;
    midix_time_ns timestamp;
    union {
        struct {
            uint8_t data[3];
            size_t len;
        } short_msg;
        struct {
            uint8_t* data;
            size_t len;
        } sysex;
    };
    struct midix_event* next;
} midix_event_t;

// Priority queue (sorted by timestamp)
typedef struct {
    midix_event_t* head;
    size_t count;
    size_t max_size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} midix_queue_t;

int  midix_queue_init(midix_queue_t* q, size_t max_size);
void midix_queue_destroy(midix_queue_t* q);
int  midix_queue_push(midix_queue_t* q, midix_event_t* event);
midix_event_t* midix_queue_pop(midix_queue_t* q, midix_time_ns now);
midix_event_t* midix_queue_peek(midix_queue_t* q);
void midix_queue_clear(midix_queue_t* q);

// ============================================================================
// Platform HAL
// ============================================================================

typedef struct midix_hal midix_hal_t;

typedef struct {
    int (*init)(midix_hal_t* hal, const char* client_name);
    void (*cleanup)(midix_hal_t* hal);

    int (*open_output_by_name)(midix_hal_t* hal, const char* name);
    void (*close_output)(midix_hal_t* hal);

    int (*create_virtual_output)(midix_hal_t* hal, const char* port_name);
    void (*drop_virtual_output)(midix_hal_t* hal);

    int (*send_short)(midix_hal_t* hal, const uint8_t* data, size_t len);
    int (*send_sysex)(midix_hal_t* hal, const uint8_t* data, size_t len);

    midix_time_ns (*get_time)(void);
} midix_hal_ops_t;

struct midix_hal {
    const midix_hal_ops_t* ops;
    void* platform_data;
};

// Platform-specific HAL initialization
#ifdef __APPLE__
int midix_hal_mac_init(midix_hal_t* hal, const char* client_name);
#endif

#ifdef _WIN32
int midix_hal_win_init(midix_hal_t* hal, const char* client_name);
#endif

// ============================================================================
// Main Context
// ============================================================================

struct midix_ctx {
    // HAL (platform-specific)
    midix_hal_t hal;

    // Scheduler
    midix_queue_t queue;
    pthread_t worker_thread;
    bool worker_running;
    bool worker_should_stop;

    // State
    pthread_mutex_t state_mutex;
    bool has_output;
    bool has_virtual_output;

    // Error state
    int last_error;
};

// ============================================================================
// Scheduler
// ============================================================================

int  midix_scheduler_init(midix_ctx* ctx);
void midix_scheduler_shutdown(midix_ctx* ctx);
int  midix_scheduler_enqueue_short(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts);
int  midix_scheduler_enqueue_sysex(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts);
int  midix_scheduler_flush(midix_ctx* ctx);

// ============================================================================
// Utility
// ============================================================================

// Note name parsing
int midix_parse_note(const char* name, int omc);  // returns MIDI note number or -1

// Number parsing
int midix_parse_int(const char* str, int* out_value);  // handles dec/hex, returns 0 on success

// MIDI message validation
bool midix_validate_channel(int ch);
bool midix_validate_note(uint8_t note);
bool midix_validate_velocity(uint8_t vel);
bool midix_validate_cc(uint8_t ccno);
bool midix_validate_value7(uint8_t val);
bool midix_validate_value14(uint16_t val);

// Error codes are defined in midix.h (public header)

#ifdef __cplusplus
}
#endif
