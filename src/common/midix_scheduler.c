// midix_scheduler.c - Timestamp-based event scheduler
// Copyright (c) 2025 libmidix contributors
// SPDX-License-Identifier: MIT

#include "midix_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#define DEFAULT_QUEUE_SIZE 4096
#define SPIN_THRESHOLD_NS  1000000  // 1ms - switch to spin wait

// ============================================================================
// Priority Queue Implementation
// ============================================================================

int midix_queue_init(midix_queue_t* q, size_t max_size) {
    memset(q, 0, sizeof(midix_queue_t));
    q->max_size = max_size > 0 ? max_size : DEFAULT_QUEUE_SIZE;

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        return MIDIX_ERR_NO_MEMORY;
    }

    if (pthread_cond_init(&q->cond, NULL) != 0) {
        pthread_mutex_destroy(&q->mutex);
        return MIDIX_ERR_NO_MEMORY;
    }

    return MIDIX_OK;
}

void midix_queue_destroy(midix_queue_t* q) {
    if (!q) {
        return;
    }

    midix_queue_clear(q);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

int midix_queue_push(midix_queue_t* q, midix_event_t* event) {
    pthread_mutex_lock(&q->mutex);

    // Check queue size limit
    if (q->count >= q->max_size) {
        pthread_mutex_unlock(&q->mutex);
        MIDIX_LOG_ERROR("Queue overflow: %zu events", q->count);
        return MIDIX_ERR_SCHED_OVERFLOW;
    }

    // Insert in timestamp order (priority queue)
    midix_event_t** insert_point = &q->head;
    while (*insert_point && (*insert_point)->timestamp <= event->timestamp) {
        insert_point = &(*insert_point)->next;
    }

    event->next = *insert_point;
    *insert_point = event;
    q->count++;

    // Signal worker thread
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);

    return MIDIX_OK;
}

midix_event_t* midix_queue_pop(midix_queue_t* q, midix_time_ns now) {
    pthread_mutex_lock(&q->mutex);

    if (!q->head || q->head->timestamp > now) {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    midix_event_t* event = q->head;
    q->head = event->next;
    q->count--;

    pthread_mutex_unlock(&q->mutex);

    return event;
}

midix_event_t* midix_queue_peek(midix_queue_t* q) {
    pthread_mutex_lock(&q->mutex);
    midix_event_t* event = q->head;
    pthread_mutex_unlock(&q->mutex);
    return event;
}

void midix_queue_clear(midix_queue_t* q) {
    pthread_mutex_lock(&q->mutex);

    midix_event_t* current = q->head;
    while (current) {
        midix_event_t* next = current->next;

        if (current->type == MIDIX_EVENT_SYSEX && current->sysex.data) {
            free(current->sysex.data);
        }

        free(current);
        current = next;
    }

    q->head = NULL;
    q->count = 0;

    pthread_mutex_unlock(&q->mutex);
}

// ============================================================================
// Worker Thread
// ============================================================================

static void* scheduler_worker(void* arg) {
    midix_ctx* ctx = (midix_ctx*)arg;

    MIDIX_LOG_DEBUG("Scheduler worker thread started");

    while (!ctx->worker_should_stop) {
        midix_time_ns now = ctx->hal.ops->get_time();
        midix_event_t* event = midix_queue_peek(&ctx->queue);

        if (!event) {
            // Queue is empty, wait for signal
            pthread_mutex_lock(&ctx->queue.mutex);
            if (!ctx->worker_should_stop && !ctx->queue.head) {
                pthread_cond_wait(&ctx->queue.cond, &ctx->queue.mutex);
            }
            pthread_mutex_unlock(&ctx->queue.mutex);
            continue;
        }

        // Check if shutdown event
        if (event->type == MIDIX_EVENT_SHUTDOWN) {
            pthread_mutex_lock(&ctx->queue.mutex);
            if (ctx->queue.head == event) {
                ctx->queue.head = event->next;
                ctx->queue.count--;
            }
            pthread_mutex_unlock(&ctx->queue.mutex);
            free(event);
            break;
        }

        // Calculate wait time
        midix_time_ns deadline = event->timestamp;
        if (deadline > now) {
            midix_time_ns wait_ns = deadline - now;

            if (wait_ns > SPIN_THRESHOLD_NS) {
                // Sleep until close to deadline
                struct timespec ts;
                ts.tv_sec = (wait_ns - SPIN_THRESHOLD_NS) / 1000000000ULL;
                ts.tv_nsec = (wait_ns - SPIN_THRESHOLD_NS) % 1000000000ULL;
                nanosleep(&ts, NULL);
            } else {
                // Spin wait for precise timing
                while (ctx->hal.ops->get_time() < deadline) {
                    // Busy wait
                }
            }

            now = ctx->hal.ops->get_time();
        }

        // Pop and send event
        event = midix_queue_pop(&ctx->queue, now);
        if (!event) {
            continue;
        }

        int result = MIDIX_OK;

        switch (event->type) {
            case MIDIX_EVENT_SHORT:
                result = ctx->hal.ops->send_short(&ctx->hal, event->short_msg.data, event->short_msg.len);
                if (result != MIDIX_OK) {
                    MIDIX_LOG_ERROR("Failed to send short message: %d", result);
                }
                break;

            case MIDIX_EVENT_SYSEX:
                result = ctx->hal.ops->send_sysex(&ctx->hal, event->sysex.data, event->sysex.len);
                if (result != MIDIX_OK) {
                    MIDIX_LOG_ERROR("Failed to send SysEx: %d", result);
                }
                free(event->sysex.data);
                break;

            case MIDIX_EVENT_SHUTDOWN:
                // Already handled above
                break;
        }

        free(event);
    }

    MIDIX_LOG_DEBUG("Scheduler worker thread exiting");
    return NULL;
}

// ============================================================================
// Scheduler API
// ============================================================================

int midix_scheduler_init(midix_ctx* ctx) {
    if (midix_queue_init(&ctx->queue, DEFAULT_QUEUE_SIZE) != MIDIX_OK) {
        return MIDIX_ERR_NO_MEMORY;
    }

    ctx->worker_running = false;
    ctx->worker_should_stop = false;

    if (pthread_create(&ctx->worker_thread, NULL, scheduler_worker, ctx) != 0) {
        midix_queue_destroy(&ctx->queue);
        return MIDIX_ERR_NO_MEMORY;
    }

    ctx->worker_running = true;
    MIDIX_LOG_DEBUG("Scheduler initialized");

    return MIDIX_OK;
}

void midix_scheduler_shutdown(midix_ctx* ctx) {
    if (!ctx->worker_running) {
        return;
    }

    MIDIX_LOG_DEBUG("Shutting down scheduler");

    // Send shutdown event
    midix_event_t* shutdown_event = (midix_event_t*)calloc(1, sizeof(midix_event_t));
    if (shutdown_event) {
        shutdown_event->type = MIDIX_EVENT_SHUTDOWN;
        shutdown_event->timestamp = 0;
        midix_queue_push(&ctx->queue, shutdown_event);
    }

    ctx->worker_should_stop = true;

    // Wake up worker thread
    pthread_mutex_lock(&ctx->queue.mutex);
    pthread_cond_signal(&ctx->queue.cond);
    pthread_mutex_unlock(&ctx->queue.mutex);

    // Wait for worker to finish with timeout
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;  // 1 second timeout

#ifdef __APPLE__
    // macOS doesn't have pthread_timedjoin_np, use manual timeout
    int join_result = -1;
    for (int i = 0; i < 10; i++) {
        int result = pthread_kill(ctx->worker_thread, 0);
        if (result == ESRCH) {
            // Thread has exited
            pthread_join(ctx->worker_thread, NULL);
            join_result = 0;
            break;
        }
        usleep(100000);  // 100ms
    }

    if (join_result != 0) {
        MIDIX_LOG_WARN("Worker thread did not exit within timeout, forcing cleanup");
        // On macOS, we can't force-kill pthread, just detach
        pthread_detach(ctx->worker_thread);
    }
#else
    // Linux/BSD: use pthread_timedjoin_np
    int join_result = pthread_timedjoin_np(ctx->worker_thread, NULL, &ts);
    if (join_result == ETIMEDOUT) {
        MIDIX_LOG_WARN("Worker thread did not exit within timeout");
        pthread_detach(ctx->worker_thread);
    }
#endif

    // Clear remaining events
    midix_queue_destroy(&ctx->queue);

    ctx->worker_running = false;
    MIDIX_LOG_DEBUG("Scheduler shutdown complete");
}

int midix_scheduler_enqueue_short(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts) {
    if (!data || len == 0 || len > 3) {
        return MIDIX_ERR_INVALID_MSG;
    }

    // For immediate send (ts=0), send directly
    if (ts == 0) {
        return ctx->hal.ops->send_short(&ctx->hal, data, len);
    }

    // Create event
    midix_event_t* event = (midix_event_t*)calloc(1, sizeof(midix_event_t));
    if (!event) {
        return MIDIX_ERR_NO_MEMORY;
    }

    event->type = MIDIX_EVENT_SHORT;
    event->timestamp = ts;
    memcpy(event->short_msg.data, data, len);
    event->short_msg.len = len;
    event->next = NULL;

    return midix_queue_push(&ctx->queue, event);
}

int midix_scheduler_enqueue_sysex(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts) {
    if (!data || len == 0) {
        return MIDIX_ERR_INVALID_MSG;
    }

    // Validate SysEx framing
    if (data[0] != 0xF0 || data[len - 1] != 0xF7) {
        MIDIX_LOG_ERROR("Invalid SysEx: missing F0/F7 framing");
        return MIDIX_ERR_SYSEX_INCOMPLETE;
    }

    // For immediate send (ts=0), send directly
    if (ts == 0) {
        return ctx->hal.ops->send_sysex(&ctx->hal, data, len);
    }

    // Create event (copy SysEx data)
    midix_event_t* event = (midix_event_t*)calloc(1, sizeof(midix_event_t));
    if (!event) {
        return MIDIX_ERR_NO_MEMORY;
    }

    event->sysex.data = (uint8_t*)malloc(len);
    if (!event->sysex.data) {
        free(event);
        return MIDIX_ERR_NO_MEMORY;
    }

    event->type = MIDIX_EVENT_SYSEX;
    event->timestamp = ts;
    memcpy(event->sysex.data, data, len);
    event->sysex.len = len;
    event->next = NULL;

    return midix_queue_push(&ctx->queue, event);
}

int midix_scheduler_flush(midix_ctx* ctx) {
    // Wait for queue to drain
    while (true) {
        pthread_mutex_lock(&ctx->queue.mutex);
        size_t count = ctx->queue.count;
        pthread_mutex_unlock(&ctx->queue.mutex);

        if (count == 0) {
            break;
        }

        struct timespec ts = {0, 10000000};  // 10ms
        nanosleep(&ts, NULL);
    }

    return MIDIX_OK;
}
