// midix_context.c - Context management
// Copyright (c) 2025 libmidix contributors
// SPDX-License-Identifier: MIT

#include "midix_internal.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Context Creation / Destruction
// ============================================================================

midix_ctx* midix_create(const char* client_name) {
    midix_log_init();

    MIDIX_LOG_DEBUG("Creating MIDIX context: %s", client_name ? client_name : "(default)");

    midix_ctx* ctx = (midix_ctx*)calloc(1, sizeof(midix_ctx));
    if (!ctx) {
        MIDIX_LOG_ERROR("Failed to allocate context");
        return NULL;
    }

    // Initialize state mutex
    if (pthread_mutex_init(&ctx->state_mutex, NULL) != 0) {
        MIDIX_LOG_ERROR("Failed to initialize state mutex");
        free(ctx);
        return NULL;
    }

    // Initialize HAL (platform-specific)
#ifdef __APPLE__
    if (midix_hal_mac_init(&ctx->hal, client_name) != MIDIX_OK) {
        MIDIX_LOG_ERROR("Failed to initialize macOS HAL");
        pthread_mutex_destroy(&ctx->state_mutex);
        free(ctx);
        return NULL;
    }
#elif defined(_WIN32)
    if (midix_hal_win_init(&ctx->hal, client_name) != MIDIX_OK) {
        MIDIX_LOG_ERROR("Failed to initialize Windows HAL");
        pthread_mutex_destroy(&ctx->state_mutex);
        free(ctx);
        return NULL;
    }
#else
    MIDIX_LOG_ERROR("Unsupported platform");
    pthread_mutex_destroy(&ctx->state_mutex);
    free(ctx);
    return NULL;
#endif

    // Initialize scheduler
    if (midix_scheduler_init(ctx) != MIDIX_OK) {
        MIDIX_LOG_ERROR("Failed to initialize scheduler");
        if (ctx->hal.ops && ctx->hal.ops->cleanup) {
            ctx->hal.ops->cleanup(&ctx->hal);
        }
        pthread_mutex_destroy(&ctx->state_mutex);
        free(ctx);
        return NULL;
    }

    ctx->has_output = false;
    ctx->has_virtual_output = false;
    ctx->last_error = MIDIX_OK;

    MIDIX_LOG_INFO("MIDIX context created successfully");
    return ctx;
}

void midix_destroy(midix_ctx* ctx) {
    if (!ctx) {
        return;
    }

    MIDIX_LOG_DEBUG("Destroying MIDIX context");

    // Shutdown scheduler and worker thread
    midix_scheduler_shutdown(ctx);

    // Close any open ports
    pthread_mutex_lock(&ctx->state_mutex);

    if (ctx->has_virtual_output && ctx->hal.ops->drop_virtual_output) {
        ctx->hal.ops->drop_virtual_output(&ctx->hal);
    }

    if (ctx->has_output && ctx->hal.ops->close_output) {
        ctx->hal.ops->close_output(&ctx->hal);
    }

    pthread_mutex_unlock(&ctx->state_mutex);

    // Cleanup HAL
    if (ctx->hal.ops && ctx->hal.ops->cleanup) {
        ctx->hal.ops->cleanup(&ctx->hal);
    }

    // Destroy mutex
    pthread_mutex_destroy(&ctx->state_mutex);

    // Free context
    free(ctx);

    MIDIX_LOG_INFO("MIDIX context destroyed");
}

// ============================================================================
// Error Management
// ============================================================================

int midix_get_last_error(midix_ctx* ctx) {
    if (!ctx) {
        return MIDIX_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&ctx->state_mutex);
    int err = ctx->last_error;
    pthread_mutex_unlock(&ctx->state_mutex);

    return err;
}

void midix_set_last_error(midix_ctx* ctx, int err) {
    if (!ctx) {
        return;
    }

    pthread_mutex_lock(&ctx->state_mutex);
    ctx->last_error = err;
    pthread_mutex_unlock(&ctx->state_mutex);
}
