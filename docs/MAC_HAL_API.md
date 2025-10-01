# macOS HAL API Reference

## Overview

The macOS Hardware Abstraction Layer (HAL) provides low-level MIDI I/O using Apple's CoreMIDI framework.

**File**: `src/mac/midix_hal_mac.mm`
**Language**: Objective-C++
**Frameworks**: CoreMIDI, CoreAudio, Foundation
**License**: MIT

## Architecture

```
┌─────────────────────────────────────┐
│   High-Level API (midix.h)          │
│   (note_on, cc, pc, etc.)           │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│   Scheduler & Worker Thread          │
│   (timestamp queue, async sends)    │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│   HAL Interface (midix_hal_ops_t)   │  ← Platform-independent
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│   macOS HAL (midix_hal_mac.mm)      │  ← This implementation
│   ┌─────────────────────────────┐   │
│   │ CoreMIDI Framework          │   │
│   │ - MIDIClientCreate          │   │
│   │ - MIDIOutputPortCreate      │   │
│   │ - MIDISend / MIDISendSysex  │   │
│   │ - MIDISourceCreate (virtual)│   │
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

## Data Structures

### `midix_mac_data_t`

Platform-specific state maintained by HAL:

```c
typedef struct {
    MIDIClientRef client;           // CoreMIDI client handle
    MIDIPortRef output_port;        // Output port for sending
    MIDIEndpointRef destination;    // Connected physical device
    MIDIEndpointRef virtual_source; // Virtual output port (if created)

    pthread_mutex_t sysex_mutex;    // SysEx completion synchronization
    int pending_sysex_count;        // Number of pending async SysEx sends

    mach_timebase_info_data_t timebase; // For nanosecond conversion
} midix_mac_data_t;
```

## HAL Operations

### Initialization

#### `mac_init(hal, client_name)`

Creates CoreMIDI client and output port.

**Implementation**:
```c
- MIDIClientCreate(client_name) → client
- MIDIOutputPortCreate(client, "Output") → output_port
- mach_timebase_info() → timebase
- pthread_mutex_init() → sysex_mutex
```

**Returns**: `MIDIX_OK` or `MIDIX_ERR_PORT_FAILURE`

**Resources Allocated**:
- CoreMIDI client
- CoreMIDI output port
- Mutex for SysEx tracking

---

### Cleanup

#### `mac_cleanup(hal)`

Disposes all CoreMIDI resources.

**Implementation**:
```c
- Wait for pending SysEx (up to 1 second)
- MIDIEndpointDispose(virtual_source) if exists
- MIDIPortDispose(output_port)
- MIDIClientDispose(client)
- pthread_mutex_destroy(sysex_mutex)
- free(platform_data)
```

**Note**: Blocks briefly if SysEx sends are pending to ensure clean shutdown.

---

### Device Management

#### `mac_list_outputs(names, count)`

Enumerates available MIDI destinations.

**Implementation**:
```c
- MIDIGetNumberOfDestinations() → num_dest
- for each destination:
    - MIDIGetDestination(i) → endpoint
    - MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName) → name
- return array of names
```

**Returns**: Array of device name strings (caller must free)

**Example Output**:
```
IAC Driver Bus 1
Network Session 1
My MIDI Device
```

---

#### `mac_open_output_by_name(hal, name_substring)`

Connects to MIDI device matching name (case-insensitive substring).

**Implementation**:
```c
- Close existing destination (if any)
- Search all destinations for case-insensitive match
- Store matching endpoint in hal->destination
```

**Returns**:
- `MIDIX_OK` if match found
- `MIDIX_ERR_NO_DEVICE` if no match

**Matching Examples**:
- `"IAC"` matches `"IAC Driver Bus 1"`
- `"network"` matches `"Network Session 1"`
- `"midi"` matches `"My MIDI Device"`

---

#### `mac_close_output(hal)`

Disconnects from current device.

**Implementation**:
```c
hal->destination = 0;
```

**Note**: Does not dispose resources, just clears reference.

---

### Virtual Ports

#### `mac_create_virtual_output(hal, port_name)`

Creates virtual MIDI source visible to other apps.

**Implementation**:
```c
- Dispose existing virtual_source (if any)
- MIDISourceCreate(client, port_name) → virtual_source
```

**Returns**: `MIDIX_OK` or `MIDIX_ERR_PORT_FAILURE`

**Visibility**: Port immediately appears in:
- Audio MIDI Setup
- DAW MIDI input lists
- MIDI Monitor apps

**Use Case**: Create virtual output for routing MIDI to other apps.

---

#### `mac_drop_virtual_output(hal)`

Destroys virtual MIDI source.

**Implementation**:
```c
- MIDIEndpointDispose(virtual_source)
- virtual_source = 0
```

---

### MIDI Sending

#### `mac_send_short(hal, data, len)`

Sends 1-3 byte MIDI message (Note, CC, PC, PB, etc.).

**Implementation**:
```c
- Build MIDIPacketList with current timestamp
- If destination: MIDISend(output_port, destination, packet_list)
- If virtual_source: MIDIReceived(virtual_source, packet_list)
```

**Parameters**:
- `data`: MIDI bytes (1-3 bytes)
- `len`: Message length (1-3)

**Returns**: `MIDIX_OK` or error code

**Supported Messages**:
- Note On/Off: `[0x90-0x9F, note, velocity]`
- Control Change: `[0xB0-0xBF, cc_num, value]`
- Program Change: `[0xC0-0xCF, program]`
- Channel Pressure: `[0xD0-0xDF, pressure]`
- Pitch Bend: `[0xE0-0xEF, lsb, msb]`
- Polyphonic Pressure: `[0xA0-0xAF, note, pressure]`

**Example**:
```c
uint8_t note_on[] = {0x90, 60, 100}; // Ch1, C4, vel=100
mac_send_short(hal, note_on, 3);
```

---

#### `mac_send_sysex(hal, data, len)`

Sends System Exclusive message (any length).

**Implementation**:

**Small SysEx (≤64KB)**:
```c
- Validate F0/F7 framing
- Allocate buffer copy
- Create MIDISysexSendRequest:
    - data = buffer
    - bytesToSend = len
    - completionProc = sysex_completion_proc
- MIDISendSysex(request) (async)
- If virtual_source: MIDIReceived() (sync)
```

**Large SysEx (>64KB)**:
```c
- Split into 64KB chunks
- Send each chunk via MIDISendSysex
- Log chunk progress
```

**Parameters**:
- `data`: SysEx data including F0 and F7
- `len`: Total length

**Returns**:
- `MIDIX_OK` if send initiated
- `MIDIX_ERR_SYSEX_INCOMPLETE` if F0/F7 missing
- `MIDIX_ERR_PORT_FAILURE` if send fails

**Async Completion**:
- Callback frees buffer when send completes
- Decrements pending counter
- Thread-safe tracking

**Example**:
```c
// GM System On
uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
mac_send_sysex(hal, sysex, 6);

// Large firmware dump (auto-chunked)
uint8_t firmware[200000];
firmware[0] = 0xF0;
// ... fill data ...
firmware[199999] = 0xF7;
mac_send_sysex(hal, firmware, 200000); // Sends in 4 chunks
```

---

### Timing

#### `mac_get_time()`

Returns current monotonic time in nanoseconds.

**Implementation**:
```c
- mach_absolute_time() → host_time
- Convert using timebase: (host_time * numer) / denom
- Return nanoseconds
```

**Returns**: `uint64_t` nanoseconds since boot

**Precision**: Typically <1µs

**Use Case**: Timestamping events for scheduler

---

## Utility Functions

### `str_contains_ci(haystack, needle)`

Case-insensitive substring search.

```cpp
std::string h(haystack);
std::string n(needle);
// Convert both to lowercase
for (auto& c : h) c = tolower(c);
for (auto& c : n) c = tolower(c);
return h.find(n) != std::string::npos;
```

---

### `get_midi_property(obj, property)`

Extracts string property from MIDI object.

```c
MIDIObjectGetStringProperty(obj, property, &name_ref);
CFStringGetCString(name_ref, buffer, size, kCFStringEncodingUTF8);
```

Returns: Allocated C string (caller must free)

---

### `ns_to_host_time(data, ns)`

Converts nanoseconds to MIDITimeStamp.

```c
return (ns * timebase.denom) / timebase.numer;
```

**Note**: Currently unused - immediate sends use `mach_absolute_time()` directly.

---

### `sysex_completion_proc(request)`

Async callback for SysEx completion.

**Actions**:
1. Free data buffer
2. Decrement pending counter (thread-safe)
3. Free request structure
4. Log completion

---

## Thread Safety

### Mutex Protection

- `sysex_mutex`: Protects `pending_sysex_count`
- Lock held during increment (send) and decrement (completion)

### Concurrent Operations

**Safe**:
- Multiple short message sends
- Concurrent send_short + send_sysex
- Send to both physical + virtual simultaneously

**Not Thread-Safe** (caller must synchronize):
- Concurrent device open/close
- Concurrent virtual port create/drop
- Concurrent cleanup

---

## Error Handling

### OSStatus Mapping

| CoreMIDI Error | MIDIX Error | Meaning |
|----------------|-------------|---------|
| `noErr` (0) | `MIDIX_OK` | Success |
| `-10842` (kMIDIServerStartErr) | `MIDIX_ERR_PORT_FAILURE` | MIDI server not running |
| `-10844` (kMIDIObjectNotFound) | `MIDIX_ERR_NO_DEVICE` | Device disconnected |
| Any other | `MIDIX_ERR_PORT_FAILURE` | Generic failure |

### Validation Errors

- Missing F0/F7 → `MIDIX_ERR_SYSEX_INCOMPLETE`
- Invalid message length → `MIDIX_ERR_INVALID_MSG`
- No device open → `MIDIX_ERR_NO_DEVICE`

---

## Performance Characteristics

### Latency

- **Short messages**: <1ms (typically <100µs)
- **Small SysEx**: <5ms
- **Large SysEx**: Async, doesn't block

### Throughput

- Tested with 10,000+ messages/sec
- CoreMIDI handles queuing internally
- No artificial rate limiting

### Memory

- Per-message overhead: ~100 bytes (packet structures)
- SysEx buffers copied (freed on completion)
- No memory pools or caching

---

## Limitations

### Current Implementation

1. **No timestamp scheduling**: Uses immediate send (timestamp = now)
   - Scheduler layer will handle delayed sends

2. **64KB chunk size**: Conservative for compatibility
   - Could be tuned per-device

3. **No running status**: Each message is self-contained
   - Standard MIDI practice

4. **No MIDI 2.0 / UMP**: Uses MIDI 1.0 protocol
   - CoreMIDI supports MIDI 2.0 on macOS 11+
   - Future enhancement possible

### CoreMIDI Constraints

- Virtual ports limited to ~16 per process
- SysEx max size: ~1MB practical limit
- No direct access to MIDI thru connections

---

## Debugging

### Logging

Set `MIDIX_LOG_LEVEL=DEBUG`:

```
[DEBUG] CoreMIDI initialized: client='MyApp'
[DEBUG] Found 2 MIDI destinations
[INFO] Found matching device: 'IAC Driver Bus 1'
[DEBUG] Sent short message: 90 3C 64 (len=3)
[DEBUG] SysEx send completed (status: 0, 6 bytes)
```

### Common Issues

**"MIDIClientCreate failed: -10842"**
- MIDI server not running
- Restart: `sudo killall coreaudiod`

**"No device matching 'X' found"**
- Device offline or removed
- Check Audio MIDI Setup
- Try substring match (e.g., "IAC" not "IAC Driver Bus 1")

**"MIDISend failed: -10844"**
- Device disconnected during send
- Re-open device

---

## Testing Strategies

### Unit Tests

1. **Initialization**: Create/destroy multiple contexts
2. **Enumeration**: List devices (handle 0 devices)
3. **Connect**: Open valid/invalid device names
4. **Send**: Each message type
5. **SysEx**: Small, medium, large, invalid framing
6. **Virtual**: Create, send, destroy

### Integration Tests

1. **Loopback**: Virtual → Physical (IAC Driver)
2. **DAW**: Send to DAW, verify in piano roll
3. **Monitor**: MIDI Monitor app verification

### Stress Tests

1. **Burst**: 10,000 rapid note events
2. **Large SysEx**: 1MB+ firmware dumps
3. **Concurrent**: Multiple contexts, devices
4. **Long-running**: 24 hour stability test

---

## Future Enhancements

### MIDI 2.0 Support

```c
// Potential UMP send function
int mac_send_ump(midix_hal_t* hal, const uint32_t* ump, size_t word_count);
```

Requires:
- macOS 11+ (`MIDIEventListAdd`, `MIDISendEventList`)
- UMP packet formatting
- Protocol negotiation

### Timestamp Scheduling

```c
// Add timestamp parameter
int mac_send_short(midix_hal_t* hal, const uint8_t* data, size_t len,
                   midix_time_ns timestamp);
```

Implementation:
- Convert `midix_time_ns` to `MIDITimeStamp`
- Use `MIDIPacketListAdd` with explicit timestamp
- Requires scheduler coordination

### Device Notifications

```c
// Callback for device add/remove
typedef void (*device_notify_cb)(bool added, const char* device_name);
```

Implementation:
- `MIDIClientCreate` with notification callback
- Parse `MIDIObjectAddRemoveNotification`
- Notify upper layers

---

## References

### Apple Documentation

- [CoreMIDI Framework](https://developer.apple.com/documentation/coremidi)
- [MIDI Services](https://developer.apple.com/documentation/coremidi/midi_services)
- [MIDISend](https://developer.apple.com/documentation/coremidi/1495197-midisend)
- [MIDISendSysex](https://developer.apple.com/documentation/coremidi/1495264-midisendsysex)
- [Mach Absolute Time](https://developer.apple.com/documentation/kernel/1462446-mach_absolute_time)

### MIDI Specification

- [MIDI 1.0 Specification](https://www.midi.org/specifications)
- [MIDI 2.0 Specification](https://www.midi.org/midi-articles/details-about-midi-2-0-midi-ci-profiles-and-property-exchange)

---

## License

MIT License - See project LICENSE file
