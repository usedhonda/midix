/*
 * midix_hal_mac.mm - macOS CoreMIDI Hardware Abstraction Layer
 *
 * Implements MIDI send operations using Apple CoreMIDI framework.
 * Supports physical devices, virtual ports, and scheduled sends with timestamps.
 *
 * License: MIT
 * Version: 0.1.0
 */

#import <Foundation/Foundation.h>
#import <CoreMIDI/CoreMIDI.h>
#import <CoreAudio/CoreAudioTypes.h>
#import <mach/mach_time.h>

extern "C" {
#include "../common/midix_internal.h"
}

#include <string>
#include <cstring>
#include <cstdlib>

/* ============================================================================
 * Platform Data Structure
 * ========================================================================== */

typedef struct {
    MIDIClientRef client;
    MIDIPortRef output_port;
    MIDIEndpointRef destination;      // Connected device
    MIDIEndpointRef virtual_source;   // Virtual output port

    // SysEx completion tracking
    pthread_mutex_t sysex_mutex;
    int pending_sysex_count;

    // Timing conversion cache
    mach_timebase_info_data_t timebase;
} midix_mac_data_t;

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

// Case-insensitive substring search
static bool str_contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;

    std::string h(haystack);
    std::string n(needle);

    // Convert to lowercase
    for (auto& c : h) c = tolower(c);
    for (auto& c : n) c = tolower(c);

    return h.find(n) != std::string::npos;
}

// Get string property from MIDI object
static char* get_midi_property(MIDIObjectRef obj, CFStringRef property) {
    CFStringRef name_ref = NULL;
    OSStatus status = MIDIObjectGetStringProperty(obj, property, &name_ref);

    if (status != noErr || !name_ref) {
        return NULL;
    }

    CFIndex length = CFStringGetLength(name_ref);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char* name = (char*)malloc(maxSize);

    if (name && CFStringGetCString(name_ref, name, maxSize, kCFStringEncodingUTF8)) {
        CFRelease(name_ref);
        return name;
    }

    if (name) free(name);
    CFRelease(name_ref);
    return NULL;
}

// Convert nanoseconds to MIDITimeStamp (host time)
static uint64_t ns_to_host_time(midix_mac_data_t* data, midix_time_ns ns) {
    if (ns == 0) return 0; // Immediate send

    // Convert nanoseconds to host time using mach_timebase
    return (ns * data->timebase.denom) / data->timebase.numer;
}

// Get current time in nanoseconds
static midix_time_ns get_time_ns(midix_mac_data_t* data) {
    uint64_t host_time = mach_absolute_time();
    return (host_time * data->timebase.numer) / data->timebase.denom;
}

/* ============================================================================
 * SysEx Completion Callback
 * ========================================================================== */

static void sysex_completion_proc(MIDISysexSendRequest* request) {
    midix_mac_data_t* data = (midix_mac_data_t*)request->completionRefCon;

    if (request->complete) {
        MIDIX_LOG_DEBUG("SysEx send completed (status: %d, %zu bytes)",
                       (int)request->complete, request->bytesToSend);
    }

    // Free the data buffer
    if (request->data) {
        free((void*)request->data);
    }

    // Decrement pending count
    pthread_mutex_lock(&data->sysex_mutex);
    data->pending_sysex_count--;
    pthread_mutex_unlock(&data->sysex_mutex);

    // Free the request structure
    free(request);
}

/* ============================================================================
 * HAL Operations
 * ========================================================================== */

static int mac_init(midix_hal_t* hal, const char* client_name) {
    midix_mac_data_t* data = (midix_mac_data_t*)calloc(1, sizeof(midix_mac_data_t));
    if (!data) {
        MIDIX_LOG_ERROR("Failed to allocate macOS HAL data");
        return MIDIX_ERR_OUT_OF_MEMORY;
    }

    hal->platform_data = data;

    // Initialize timebase for nanosecond conversion
    mach_timebase_info(&data->timebase);

    // Initialize SysEx mutex
    pthread_mutex_init(&data->sysex_mutex, NULL);
    data->pending_sysex_count = 0;

    // Create MIDI client
    CFStringRef name = CFStringCreateWithCString(NULL, client_name, kCFStringEncodingUTF8);
    OSStatus status = MIDIClientCreate(name, NULL, NULL, &data->client);
    CFRelease(name);

    if (status != noErr) {
        MIDIX_LOG_ERROR("MIDIClientCreate failed: %d", (int)status);
        free(data);
        hal->platform_data = NULL;
        return MIDIX_ERR_PORT_FAILURE;
    }

    // Create output port
    CFStringRef port_name = CFStringCreateWithCString(NULL, "Output", kCFStringEncodingUTF8);
    status = MIDIOutputPortCreate(data->client, port_name, &data->output_port);
    CFRelease(port_name);

    if (status != noErr) {
        MIDIX_LOG_ERROR("MIDIOutputPortCreate failed: %d", (int)status);
        MIDIClientDispose(data->client);
        free(data);
        hal->platform_data = NULL;
        return MIDIX_ERR_PORT_FAILURE;
    }

    MIDIX_LOG_INFO("CoreMIDI initialized: client='%s'", client_name);
    return MIDIX_OK;
}

static void mac_cleanup(midix_hal_t* hal) {
    if (!hal->platform_data) return;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;

    // Wait for pending SysEx sends to complete (with timeout)
    pthread_mutex_lock(&data->sysex_mutex);
    int pending = data->pending_sysex_count;
    pthread_mutex_unlock(&data->sysex_mutex);

    if (pending > 0) {
        MIDIX_LOG_WARN("Waiting for %d pending SysEx sends...", pending);
        for (int i = 0; i < 100 && pending > 0; i++) {
            usleep(10000); // 10ms
            pthread_mutex_lock(&data->sysex_mutex);
            pending = data->pending_sysex_count;
            pthread_mutex_unlock(&data->sysex_mutex);
        }
    }

    // Dispose of virtual source if created
    if (data->virtual_source) {
        MIDIEndpointDispose(data->virtual_source);
        data->virtual_source = 0;
    }

    // Dispose of output port
    if (data->output_port) {
        MIDIPortDispose(data->output_port);
        data->output_port = 0;
    }

    // Dispose of client
    if (data->client) {
        MIDIClientDispose(data->client);
        data->client = 0;
    }

    pthread_mutex_destroy(&data->sysex_mutex);

    free(data);
    hal->platform_data = NULL;

    MIDIX_LOG_INFO("CoreMIDI cleaned up");
}

static int mac_list_outputs(char*** names, int* count) {
    ItemCount num_dest = MIDIGetNumberOfDestinations();

    if (num_dest == 0) {
        *names = NULL;
        *count = 0;
        return MIDIX_OK;
    }

    char** name_list = (char**)calloc(num_dest, sizeof(char*));
    if (!name_list) {
        return MIDIX_ERR_OUT_OF_MEMORY;
    }

    int valid_count = 0;
    for (ItemCount i = 0; i < num_dest; i++) {
        MIDIEndpointRef dest = MIDIGetDestination(i);
        if (dest) {
            char* name = get_midi_property(dest, kMIDIPropertyName);
            if (name) {
                name_list[valid_count++] = name;
            }
        }
    }

    *names = name_list;
    *count = valid_count;

    MIDIX_LOG_DEBUG("Found %d MIDI destinations", valid_count);
    return MIDIX_OK;
}

static int mac_open_output_by_name(midix_hal_t* hal, const char* name_substring) {
    if (!hal->platform_data) return MIDIX_ERR_PORT_FAILURE;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;

    // Close existing connection
    if (data->destination) {
        data->destination = 0;
    }

    // Search for matching destination
    ItemCount num_dest = MIDIGetNumberOfDestinations();
    MIDIEndpointRef found = 0;

    for (ItemCount i = 0; i < num_dest; i++) {
        MIDIEndpointRef dest = MIDIGetDestination(i);
        if (!dest) continue;

        char* name = get_midi_property(dest, kMIDIPropertyName);
        if (name) {
            bool match = str_contains_ci(name, name_substring);

            if (match) {
                MIDIX_LOG_INFO("Found matching device: '%s'", name);
                found = dest;
                free(name);
                break;
            }
            free(name);
        }
    }

    if (!found) {
        MIDIX_LOG_ERROR("No device matching '%s' found", name_substring);
        return MIDIX_ERR_NO_DEVICE;
    }

    data->destination = found;
    MIDIX_LOG_INFO("Opened output device");
    return MIDIX_OK;
}

static void mac_close_output(midix_hal_t* hal) {
    if (!hal->platform_data) return;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;
    data->destination = 0;

    MIDIX_LOG_INFO("Closed output device");
}

static int mac_create_virtual_output(midix_hal_t* hal, const char* port_name) {
    if (!hal->platform_data) return MIDIX_ERR_PORT_FAILURE;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;

    // Dispose of existing virtual source
    if (data->virtual_source) {
        MIDIEndpointDispose(data->virtual_source);
        data->virtual_source = 0;
    }

    // Create virtual MIDI source
    CFStringRef name = CFStringCreateWithCString(NULL, port_name, kCFStringEncodingUTF8);
    OSStatus status = MIDISourceCreate(data->client, name, &data->virtual_source);
    CFRelease(name);

    if (status != noErr) {
        MIDIX_LOG_ERROR("MIDISourceCreate failed: %d", (int)status);
        return MIDIX_ERR_PORT_FAILURE;
    }

    MIDIX_LOG_INFO("Created virtual output: '%s'", port_name);
    return MIDIX_OK;
}

static void mac_drop_virtual_output(midix_hal_t* hal) {
    if (!hal->platform_data) return;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;

    if (data->virtual_source) {
        MIDIEndpointDispose(data->virtual_source);
        data->virtual_source = 0;
        MIDIX_LOG_INFO("Dropped virtual output");
    }
}

static int mac_send_short(midix_hal_t* hal, const uint8_t* msg_data, size_t len) {
    if (!hal->platform_data) return MIDIX_ERR_PORT_FAILURE;
    if (!msg_data || len == 0 || len > 3) return MIDIX_ERR_INVALID_MSG;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;

    if (!data->destination && !data->virtual_source) {
        MIDIX_LOG_ERROR("No output device or virtual port open");
        return MIDIX_ERR_NO_DEVICE;
    }

    // Build MIDI packet
    Byte packet_buffer[1024];
    MIDIPacketList* packet_list = (MIDIPacketList*)packet_buffer;
    MIDIPacket* packet = MIDIPacketListInit(packet_list);

    // Use current time for immediate send
    MIDITimeStamp timestamp = mach_absolute_time();

    packet = MIDIPacketListAdd(packet_list, sizeof(packet_buffer), packet,
                                timestamp, len, msg_data);

    if (!packet) {
        MIDIX_LOG_ERROR("Failed to create MIDI packet");
        return MIDIX_ERR_INVALID_MSG;
    }

    OSStatus status = noErr;

    // Send to physical device if connected
    if (data->destination) {
        status = MIDISend(data->output_port, data->destination, packet_list);
        if (status != noErr) {
            MIDIX_LOG_ERROR("MIDISend failed: %d", (int)status);
            return MIDIX_ERR_PORT_FAILURE;
        }
    }

    // Send to virtual output if created
    if (data->virtual_source) {
        status = MIDIReceived(data->virtual_source, packet_list);
        if (status != noErr) {
            MIDIX_LOG_ERROR("MIDIReceived (virtual) failed: %d", (int)status);
            return MIDIX_ERR_PORT_FAILURE;
        }
    }

    MIDIX_LOG_DEBUG("Sent short message: %02X %02X %02X (len=%zu)",
                   len > 0 ? msg_data[0] : 0,
                   len > 1 ? msg_data[1] : 0,
                   len > 2 ? msg_data[2] : 0,
                   len);

    return MIDIX_OK;
}

static int mac_send_sysex(midix_hal_t* hal, const uint8_t* sysex_data, size_t len) {
    if (!hal->platform_data) return MIDIX_ERR_PORT_FAILURE;
    if (!sysex_data || len < 2) return MIDIX_ERR_INVALID_MSG;

    midix_mac_data_t* data = (midix_mac_data_t*)hal->platform_data;

    if (!data->destination && !data->virtual_source) {
        MIDIX_LOG_ERROR("No output device or virtual port open");
        return MIDIX_ERR_NO_DEVICE;
    }

    // Validate SysEx framing
    if (sysex_data[0] != 0xF0 || sysex_data[len - 1] != 0xF7) {
        MIDIX_LOG_ERROR("Invalid SysEx: missing F0/F7 framing");
        return MIDIX_ERR_SYSEX_INCOMPLETE;
    }

    // For large SysEx (>64KB), split into chunks
    const size_t CHUNK_SIZE = 65536;

    if (len <= CHUNK_SIZE) {
        // Send as single packet
        Byte* data_copy = (Byte*)malloc(len);
        if (!data_copy) {
            return MIDIX_ERR_OUT_OF_MEMORY;
        }
        memcpy(data_copy, sysex_data, len);

        MIDISysexSendRequest* request = (MIDISysexSendRequest*)calloc(1, sizeof(MIDISysexSendRequest));
        if (!request) {
            free(data_copy);
            return MIDIX_ERR_OUT_OF_MEMORY;
        }

        request->destination = data->destination;
        request->data = data_copy;
        request->bytesToSend = (UInt32)len;
        request->complete = false;
        request->completionProc = sysex_completion_proc;
        request->completionRefCon = data;

        pthread_mutex_lock(&data->sysex_mutex);
        data->pending_sysex_count++;
        pthread_mutex_unlock(&data->sysex_mutex);

        OSStatus status = MIDISendSysex(request);

        if (status != noErr) {
            MIDIX_LOG_ERROR("MIDISendSysex failed: %d", (int)status);
            pthread_mutex_lock(&data->sysex_mutex);
            data->pending_sysex_count--;
            pthread_mutex_unlock(&data->sysex_mutex);
            free(data_copy);
            free(request);
            return MIDIX_ERR_PORT_FAILURE;
        }

        // Also send to virtual source if present
        if (data->virtual_source) {
            Byte packet_buffer[65536 + 100];
            MIDIPacketList* packet_list = (MIDIPacketList*)packet_buffer;
            MIDIPacket* packet = MIDIPacketListInit(packet_list);

            MIDITimeStamp timestamp = mach_absolute_time();
            packet = MIDIPacketListAdd(packet_list, sizeof(packet_buffer),
                                       packet, timestamp, len, sysex_data);

            if (packet) {
                MIDIReceived(data->virtual_source, packet_list);
            }
        }

        MIDIX_LOG_DEBUG("Sent SysEx: %zu bytes", len);

    } else {
        // Split large SysEx into chunks
        MIDIX_LOG_INFO("Splitting large SysEx (%zu bytes) into chunks", len);

        size_t offset = 0;
        int chunk_num = 0;

        while (offset < len) {
            size_t chunk_len = (len - offset > CHUNK_SIZE) ? CHUNK_SIZE : (len - offset);

            // For middle chunks, we need to handle framing carefully
            // First chunk: 0xF0 ... (no 0xF7)
            // Middle chunks: data only (no 0xF0 or 0xF7)
            // Last chunk: ... 0xF7

            bool is_first = (offset == 0);
            bool is_last = (offset + chunk_len >= len);

            Byte* chunk_data = (Byte*)malloc(chunk_len);
            if (!chunk_data) {
                return MIDIX_ERR_OUT_OF_MEMORY;
            }

            memcpy(chunk_data, sysex_data + offset, chunk_len);

            // Send chunk using MIDISendSysex for physical device
            if (data->destination) {
                MIDISysexSendRequest* request = (MIDISysexSendRequest*)calloc(1, sizeof(MIDISysexSendRequest));
                if (!request) {
                    free(chunk_data);
                    return MIDIX_ERR_OUT_OF_MEMORY;
                }

                request->destination = data->destination;
                request->data = chunk_data;
                request->bytesToSend = (UInt32)chunk_len;
                request->complete = false;
                request->completionProc = sysex_completion_proc;
                request->completionRefCon = data;

                pthread_mutex_lock(&data->sysex_mutex);
                data->pending_sysex_count++;
                pthread_mutex_unlock(&data->sysex_mutex);

                OSStatus status = MIDISendSysex(request);

                if (status != noErr) {
                    MIDIX_LOG_ERROR("MIDISendSysex chunk %d failed: %d", chunk_num, (int)status);
                    pthread_mutex_lock(&data->sysex_mutex);
                    data->pending_sysex_count--;
                    pthread_mutex_unlock(&data->sysex_mutex);
                    free(chunk_data);
                    free(request);
                    return MIDIX_ERR_PORT_FAILURE;
                }
            } else {
                free(chunk_data);
            }

            offset += chunk_len;
            chunk_num++;
        }

        MIDIX_LOG_INFO("Sent SysEx in %d chunks (%zu bytes total)", chunk_num, len);
    }

    return MIDIX_OK;
}

static midix_time_ns mac_get_time(void) {
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.numer == 0) {
        mach_timebase_info(&timebase);
    }

    uint64_t host_time = mach_absolute_time();
    return (host_time * timebase.numer) / timebase.denom;
}

/* ============================================================================
 * HAL Operations Table
 * ========================================================================== */

static const midix_hal_ops_t mac_ops = {
    .init = mac_init,
    .cleanup = mac_cleanup,
    .open_output_by_name = mac_open_output_by_name,
    .close_output = mac_close_output,
    .create_virtual_output = mac_create_virtual_output,
    .drop_virtual_output = mac_drop_virtual_output,
    .send_short = mac_send_short,
    .send_sysex = mac_send_sysex,
    .get_time = mac_get_time
};

/* ============================================================================
 * Public HAL Initialization
 * ========================================================================== */

extern "C" int midix_hal_mac_init(midix_hal_t* hal, const char* client_name) {
    hal->ops = &mac_ops;
    hal->platform_data = NULL;

    return mac_init(hal, client_name);
}

// List outputs function (standalone, not part of HAL ops)
extern "C" int midix_list_outputs(char*** names, int* count) {
    return mac_list_outputs(names, count);
}
