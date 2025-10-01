# midix Library API Reference

Version: 0.1.0

## Table of Contents

- [Overview](#overview)
- [Core Types](#core-types)
- [Context Management](#context-management)
- [Device Management](#device-management)
- [Message Sending](#message-sending)
  - [Channel Voice Messages](#channel-voice-messages)
  - [Raw Messages](#raw-messages)
  - [SysEx Messages](#sysex-messages)
- [Timing and Scheduling](#timing-and-scheduling)
- [Utility Functions](#utility-functions)
- [Error Codes](#error-codes)
- [Usage Examples](#usage-examples)

---

## Overview

The midix library provides a clean C API for sending MIDI messages with optional timestamp-based scheduling. All functions are thread-safe and designed for low-latency, non-blocking operation.

### Header

```c
#include <midix.h>
```

### Linking

- **macOS**: `-lmidix -framework CoreMIDI -framework CoreAudio -framework CoreFoundation`
- **Windows**: `midix.lib winmm.lib`

---

## Core Types

### `midix_ctx`

```c
typedef struct midix_ctx midix_ctx;
```

Opaque handle to a MIDI context. Represents a MIDI client with associated worker thread and message queue.

### `midix_time_ns`

```c
typedef uint64_t midix_time_ns;
```

Monotonic timestamp in nanoseconds. Used for scheduling MIDI messages.

- `0` = send immediately
- Non-zero = send at specified time (from `midix_now()`)

---

## Context Management

### `midix_create`

```c
midix_ctx* midix_create(const char* client_name);
```

Creates a new MIDI context with the specified client name.

**Parameters:**
- `client_name`: Name visible in MIDI routing applications (null-terminated string)

**Returns:**
- Pointer to `midix_ctx` on success
- `NULL` on failure

**Example:**
```c
midix_ctx* ctx = midix_create("MyApp");
if (!ctx) {
    fprintf(stderr, "Failed to create MIDI context\n");
    return 1;
}
```

---

### `midix_destroy`

```c
void midix_destroy(midix_ctx* ctx);
```

Destroys a MIDI context and releases all associated resources. Automatically closes any open outputs and flushes pending messages.

**Parameters:**
- `ctx`: Context to destroy (may be `NULL`)

**Example:**
```c
midix_destroy(ctx);
ctx = NULL;
```

---

## Device Management

### `midix_open_output_by_name`

```c
int midix_open_output_by_name(midix_ctx* ctx, const char* name_substring);
```

Opens a MIDI output port by partial name match (case-insensitive).

**Parameters:**
- `ctx`: MIDI context
- `name_substring`: Substring to match against available port names

**Returns:**
- `0` on success
- `MIDIX_ERR_NO_DEVICE` if no matching device found
- `MIDIX_ERR_PORT_FAILURE` if port creation failed

**Example:**
```c
// Matches "IAC Driver Bus 1" or "IAC Driver Port"
if (midix_open_output_by_name(ctx, "IAC") != 0) {
    fprintf(stderr, "Failed to open MIDI output\n");
}
```

---

### `midix_close_output`

```c
void midix_close_output(midix_ctx* ctx);
```

Closes the currently open MIDI output port.

**Parameters:**
- `ctx`: MIDI context

**Example:**
```c
midix_close_output(ctx);
```

---

### `midix_create_virtual_output`

```c
int midix_create_virtual_output(midix_ctx* ctx, const char* port_name);
```

Creates a virtual MIDI output port (macOS only).

**Parameters:**
- `ctx`: MIDI context
- `port_name`: Name for the virtual port

**Returns:**
- `0` on success
- `MIDIX_ERR_PORT_FAILURE` on failure
- Not supported on Windows (requires external virtual MIDI driver)

**Example:**
```c
if (midix_create_virtual_output(ctx, "My Virtual Port") == 0) {
    printf("Virtual port created\n");
}
```

---

### `midix_drop_virtual_output`

```c
void midix_drop_virtual_output(midix_ctx* ctx);
```

Destroys the virtual MIDI output port.

**Parameters:**
- `ctx`: MIDI context

---

## Message Sending

### Channel Voice Messages

#### `midix_note_on`

```c
int midix_note_on(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts);
```

Sends a Note On message.

**Parameters:**
- `ctx`: MIDI context
- `ch`: MIDI channel (1-16)
- `note`: Note number (0-127)
- `vel`: Velocity (0-127)
- `ts`: Timestamp (0 = immediate)

**Returns:**
- `0` on success
- Error code on failure

**Example:**
```c
// Send middle C at full velocity immediately
midix_note_on(ctx, 1, 60, 127, 0);

// Schedule note 100ms in the future
midix_time_ns future = midix_now() + 100000000;
midix_note_on(ctx, 1, 64, 100, future);
```

---

#### `midix_note_off`

```c
int midix_note_off(midix_ctx* ctx, int ch, uint8_t note, uint8_t vel, midix_time_ns ts);
```

Sends a Note Off message.

**Parameters:**
- Same as `midix_note_on`

**Example:**
```c
midix_note_off(ctx, 1, 60, 0, 0);
```

---

#### `midix_cc`

```c
int midix_cc(midix_ctx* ctx, int ch, uint8_t ccno, uint8_t val, midix_time_ns ts);
```

Sends a Control Change message.

**Parameters:**
- `ctx`: MIDI context
- `ch`: MIDI channel (1-16)
- `ccno`: Controller number (0-127)
- `val`: Controller value (0-127)
- `ts`: Timestamp (0 = immediate)

**Example:**
```c
// Set volume (CC 7) to maximum
midix_cc(ctx, 1, 7, 127, 0);

// Set sustain pedal off
midix_cc(ctx, 1, 64, 0, 0);
```

---

#### `midix_pb`

```c
int midix_pb(midix_ctx* ctx, int ch, uint16_t val14, midix_time_ns ts);
```

Sends a Pitch Bend message.

**Parameters:**
- `ctx`: MIDI context
- `ch`: MIDI channel (1-16)
- `val14`: 14-bit pitch bend value (0-16383, 8192 = center)
- `ts`: Timestamp (0 = immediate)

**Example:**
```c
// Center pitch bend
midix_pb(ctx, 1, 8192, 0);

// Bend up maximum
midix_pb(ctx, 1, 16383, 0);
```

---

#### `midix_pc`

```c
int midix_pc(midix_ctx* ctx, int ch, uint8_t program, midix_time_ns ts);
```

Sends a Program Change message.

**Parameters:**
- `ctx`: MIDI context
- `ch`: MIDI channel (1-16)
- `program`: Program number (0-127)
- `ts`: Timestamp (0 = immediate)

**Example:**
```c
// Select piano (program 0)
midix_pc(ctx, 1, 0, 0);
```

---

#### `midix_cp`

```c
int midix_cp(midix_ctx* ctx, int ch, uint8_t val, midix_time_ns ts);
```

Sends a Channel Pressure (aftertouch) message.

**Parameters:**
- `ctx`: MIDI context
- `ch`: MIDI channel (1-16)
- `val`: Pressure value (0-127)
- `ts`: Timestamp (0 = immediate)

**Example:**
```c
midix_cp(ctx, 1, 64, 0);
```

---

### Raw Messages

#### `midix_send_bytes`

```c
int midix_send_bytes(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts);
```

Sends arbitrary MIDI bytes (non-SysEx).

**Parameters:**
- `ctx`: MIDI context
- `data`: Pointer to MIDI message bytes
- `len`: Number of bytes (typically 1-3)
- `ts`: Timestamp (0 = immediate)

**Returns:**
- `0` on success
- `MIDIX_ERR_INVALID_MSG` if message format is invalid

**Example:**
```c
// Send raw MIDI Clock (0xF8)
uint8_t clock = 0xF8;
midix_send_bytes(ctx, &clock, 1, 0);

// Send note on (channel 1)
uint8_t noteon[] = {0x90, 60, 100};
midix_send_bytes(ctx, noteon, 3, 0);
```

---

### SysEx Messages

#### `midix_send_sysex`

```c
int midix_send_sysex(midix_ctx* ctx, const uint8_t* data, size_t len, midix_time_ns ts);
```

Sends a System Exclusive message.

**Parameters:**
- `ctx`: MIDI context
- `data`: Pointer to SysEx data (must start with 0xF0 and end with 0xF7)
- `len`: Total length including F0 and F7
- `ts`: Timestamp (0 = immediate)

**Returns:**
- `0` on success
- `MIDIX_ERR_SYSEX_INCOMPLETE` if F0/F7 are missing
- `MIDIX_ERR_TIMEOUT` if send times out

**Example:**
```c
// Send Identity Request
uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
midix_send_sysex(ctx, sysex, sizeof(sysex), 0);

// Large SysEx (automatically chunked if needed)
uint8_t large_sysex[65536];
large_sysex[0] = 0xF0;
// ... fill data ...
large_sysex[65535] = 0xF7;
midix_send_sysex(ctx, large_sysex, sizeof(large_sysex), 0);
```

---

## Timing and Scheduling

### `midix_now`

```c
midix_time_ns midix_now(void);
```

Returns the current monotonic time in nanoseconds.

**Returns:**
- Current time suitable for use with timestamp parameters

**Example:**
```c
midix_time_ns now = midix_now();
midix_time_ns future = now + 500000000; // 500ms in the future
midix_note_on(ctx, 1, 60, 100, future);
```

---

## Utility Functions

### `midix_panic`

```c
int midix_panic(midix_ctx* ctx);
```

Sends All Notes Off and Sustain Off on all 16 channels.

**Parameters:**
- `ctx`: MIDI context

**Returns:**
- `0` on success

**Example:**
```c
// Emergency: silence all sound
midix_panic(ctx);
```

---

### `midix_flush`

```c
int midix_flush(midix_ctx* ctx);
```

Blocks until all queued messages have been sent (optional drain operation).

**Parameters:**
- `ctx`: MIDI context

**Returns:**
- `0` on success

**Example:**
```c
// Send multiple messages
midix_note_on(ctx, 1, 60, 100, 0);
midix_note_on(ctx, 1, 64, 100, 0);
midix_note_on(ctx, 1, 67, 100, 0);

// Wait for all to complete
midix_flush(ctx);
```

---

## Error Codes

```c
#define MIDIX_ERR_NO_DEVICE         -1  // Device not found or not selected
#define MIDIX_ERR_PORT_FAILURE      -2  // Port creation/connection failed
#define MIDIX_ERR_INVALID_MSG       -3  // Invalid MIDI message format
#define MIDIX_ERR_SCHED_OVERFLOW    -4  // Send queue is full
#define MIDIX_ERR_SYSEX_INCOMPLETE  -5  // SysEx missing F0 or F7
#define MIDIX_ERR_TIMEOUT           -6  // Operation timed out
```

---

## Usage Examples

### Example 1: Simple Note Sequence

```c
#include <midix.h>
#include <unistd.h>

int main() {
    midix_ctx* ctx = midix_create("NotePlayer");
    midix_open_output_by_name(ctx, "IAC");

    // Play C major chord
    midix_note_on(ctx, 1, 60, 100, 0); // C
    midix_note_on(ctx, 1, 64, 100, 0); // E
    midix_note_on(ctx, 1, 67, 100, 0); // G

    sleep(1);

    midix_note_off(ctx, 1, 60, 0, 0);
    midix_note_off(ctx, 1, 64, 0, 0);
    midix_note_off(ctx, 1, 67, 0, 0);

    midix_destroy(ctx);
    return 0;
}
```

---

### Example 2: Scheduled Arpeggio

```c
#include <midix.h>

int main() {
    midix_ctx* ctx = midix_create("Arpeggiator");
    midix_open_output_by_name(ctx, "IAC");

    midix_time_ns now = midix_now();
    uint64_t interval = 200000000; // 200ms

    // Schedule notes in the future
    uint8_t notes[] = {60, 64, 67, 72};
    for (int i = 0; i < 4; i++) {
        midix_time_ns t_on = now + (i * interval);
        midix_time_ns t_off = t_on + (interval - 10000000); // 10ms before next

        midix_note_on(ctx, 1, notes[i], 100, t_on);
        midix_note_off(ctx, 1, notes[i], 0, t_off);
    }

    // Wait for completion
    sleep(2);
    midix_destroy(ctx);
    return 0;
}
```

---

### Example 3: SysEx Bulk Dump

```c
#include <midix.h>
#include <stdio.h>

int main() {
    midix_ctx* ctx = midix_create("SysExSender");
    midix_open_output_by_name(ctx, "MIDI Device");

    // Load .syx file
    FILE* f = fopen("patch.syx", "rb");
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* sysex = malloc(size);
    fread(sysex, 1, size, f);
    fclose(f);

    // Send SysEx
    if (midix_send_sysex(ctx, sysex, size, 0) == 0) {
        printf("SysEx sent successfully\n");
    } else {
        fprintf(stderr, "SysEx send failed\n");
    }

    free(sysex);
    midix_destroy(ctx);
    return 0;
}
```

---

### Example 4: Real-time Clock

```c
#include <midix.h>
#include <unistd.h>

void send_clock_at_bpm(midix_ctx* ctx, double bpm, int beats) {
    uint64_t interval_ns = (uint64_t)((60.0 / bpm / 24.0) * 1000000000.0);
    uint8_t clock_msg = 0xF8;

    midix_time_ns t = midix_now();
    for (int i = 0; i < beats * 24; i++) {
        midix_send_bytes(ctx, &clock_msg, 1, t);
        t += interval_ns;
    }
}

int main() {
    midix_ctx* ctx = midix_create("ClockMaster");
    midix_open_output_by_name(ctx, "IAC");

    // Send start
    uint8_t start = 0xFA;
    midix_send_bytes(ctx, &start, 1, 0);

    // Send 4 beats at 120 BPM
    send_clock_at_bpm(ctx, 120.0, 4);

    sleep(3);

    // Send stop
    uint8_t stop = 0xFC;
    midix_send_bytes(ctx, &stop, 1, 0);

    midix_destroy(ctx);
    return 0;
}
```

---

## Thread Safety

All public API functions are thread-safe and can be called from multiple threads concurrently. The library uses internal lock-free queues for message dispatch.

## Performance Considerations

- Use `midix_flush()` sparingly - it blocks until all messages are sent
- For high-throughput scenarios, schedule messages in batches with timestamps
- SysEx messages larger than 1KB are automatically chunked (transparent to caller)
- Queue overflow (`MIDIX_ERR_SCHED_OVERFLOW`) only occurs under extreme load (>10k messages/sec)

## Platform-Specific Notes

### macOS
- CoreMIDI requires "Bluetooth & Peripherals" permission on newer macOS versions
- Virtual outputs appear immediately in MIDI routing applications

### Windows
- WinMM has slightly higher latency than CoreMIDI (typically 1-5ms)
- Virtual outputs require third-party drivers (loopMIDI, virtualMIDI, etc.)
- Consider `timeBeginPeriod(1)` for sub-millisecond precision (optional)

---

For CLI usage, see [CLI.md](CLI.md).
