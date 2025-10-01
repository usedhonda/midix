# macOS HAL Build and Test Guide

## Overview

The macOS Hardware Abstraction Layer (HAL) has been implemented in `src/mac/midix_hal_mac.mm` using CoreMIDI framework.

## Prerequisites

- **macOS 10.13+** (High Sierra or later)
- **Xcode Command Line Tools** (for CoreMIDI/CoreAudio frameworks)
- **CMake 3.15+**
- **IAC Driver enabled** (optional, for testing)

### Enable IAC Driver (Recommended for Testing)

1. Open **Audio MIDI Setup** (Applications → Utilities)
2. Window → Show MIDI Studio
3. Double-click "IAC Driver"
4. Check "Device is online"
5. Click "Apply"

## Build Instructions

### 1. Create Build Directory

```bash
cd /Users/usedhonda/projects/Mac/SendMIDILibrary
mkdir -p build
cd build
```

### 2. Configure with CMake

```bash
cmake .. \
  -DMIDIX_BUILD_STATIC=ON \
  -DMIDIX_BUILD_SHARED=ON \
  -DMIDIX_BUILD_CLI=OFF \
  -DMIDIX_BUILD_EXAMPLES=ON
```

### 3. Build

```bash
cmake --build . -j4
```

This will create:
- `libmidix.a` (static library)
- `libmidix.dylib` (shared library)
- Example programs in `examples/`

### 4. Expected Output

```
[ 10%] Building CXX object CMakeFiles/midix_static.dir/src/mac/midix_hal_mac.mm.o
[ 20%] Building C object CMakeFiles/midix_static.dir/src/common/midix_context.c.o
[ 30%] Building C object CMakeFiles/midix_static.dir/src/common/midix_scheduler.c.o
...
[100%] Built target test_mac_hal
```

## Testing

### Basic HAL Test

Run the comprehensive HAL test program:

```bash
./examples/test_mac_hal
```

**Expected Output:**

```
=== midix macOS HAL Test ===

✓ Created MIDI context

Available MIDI outputs:
  0: IAC Driver Bus 1
  1: Network Session 1
  ...

Attempting to open 'IAC' device...
✓ Opened IAC device

Sending test note (C4, 100ms)...
✓ Note sent

Sending test CC (Modulation)...
✓ CC sent

Sending GM System On SysEx...
✓ SysEx sent

Creating virtual output 'midix-test-out'...
✓ Virtual port created
  Check Audio MIDI Setup to see the port
  Waiting 5 seconds...
✓ Virtual port dropped

Sending panic (All Notes Off)...
✓ Panic sent

Current time: 123456789000 ns

Cleaning up...
✓ Context destroyed

=== Test completed successfully ===
```

### List Devices

```bash
./examples/list_devices
```

### Simple Note Test

```bash
./examples/simple_note
```

### Monitor MIDI Output

Use a MIDI monitor to verify output:

**Option 1: MIDI Monitor app** (free)
- Download from snoize.com
- Select "IAC Driver" as source
- Run test programs and observe messages

**Option 2: Using DAW**
- Open GarageBand/Logic/Ableton
- Create MIDI track with "IAC Driver" as input
- Run test programs

## Verification Checklist

### ✓ HAL Functionality

- [ ] **Initialization**: Context creates successfully
- [ ] **Device List**: Shows available MIDI destinations
- [ ] **Device Open**: Can connect to IAC Driver
- [ ] **Note On/Off**: Sends correctly (visible in MIDI Monitor)
- [ ] **Control Change**: CC messages received
- [ ] **Program Change**: PC messages received
- [ ] **Pitch Bend**: PB messages received
- [ ] **SysEx**: Small SysEx (<1KB) sends correctly
- [ ] **Large SysEx**: SysEx >64KB chunks properly
- [ ] **Virtual Port**: Creates and appears in system
- [ ] **Virtual Send**: Messages route through virtual port
- [ ] **Cleanup**: No crashes on destroy

### ✓ Performance

- [ ] **Latency**: Sub-millisecond send time
- [ ] **Throughput**: Handles 1000+ rapid notes
- [ ] **Memory**: No leaks (use Instruments or `leaks` command)
- [ ] **CPU**: Low overhead during idle

## Common Issues

### 1. "No devices found"

**Problem**: `midix_list_outputs()` returns 0 devices

**Solutions**:
- Enable IAC Driver (see Prerequisites)
- Check MIDI devices in Audio MIDI Setup
- Restart MIDI subsystem: `sudo killall coreaudiod`

### 2. "Failed to open device"

**Problem**: `midix_open_output_by_name()` returns `MIDIX_ERR_NO_DEVICE`

**Solutions**:
- Verify exact device name with `list_devices`
- Device name is case-insensitive substring match
- Try shorter substring (e.g., "IAC" instead of "IAC Driver Bus 1")

### 3. Build errors - "CoreMIDI/CoreMIDI.h not found"

**Problem**: Missing Xcode Command Line Tools

**Solution**:
```bash
xcode-select --install
```

### 4. "MIDISend failed: -10844"

**Problem**: CoreMIDI error `kMIDIObjectNotFound`

**Solutions**:
- Device was unplugged/removed
- Re-open device
- Check device still exists in Audio MIDI Setup

### 5. Virtual port doesn't appear

**Problem**: `midix_create_virtual_output()` succeeds but port not visible

**Solutions**:
- Refresh MIDI Studio in Audio MIDI Setup
- Port appears immediately in most apps
- Check with MIDI Monitor app

## Advanced Testing

### Large SysEx Test

Create a test file with large SysEx data:

```c
#include "midix.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    midix_ctx* ctx = midix_create("sysex-test");
    midix_open_output_by_name(ctx, "IAC");

    // Create 100KB SysEx (will auto-chunk)
    size_t len = 100000;
    uint8_t* sysex = malloc(len);
    sysex[0] = 0xF0;
    for (size_t i = 1; i < len - 1; i++) {
        sysex[i] = i % 128; // Valid data bytes
    }
    sysex[len - 1] = 0xF7;

    printf("Sending %zu byte SysEx...\n", len);
    int result = midix_send_sysex(ctx, sysex, len, 0);

    if (result == MIDIX_OK) {
        printf("Success! Check MIDI Monitor for chunks\n");
    } else {
        printf("Failed: %d\n", result);
    }

    free(sysex);
    midix_destroy(ctx);
    return 0;
}
```

### Memory Leak Check

```bash
leaks --atExit -- ./examples/test_mac_hal
```

Should report:
```
Process 12345: 0 leaks for 0 total leaked bytes.
```

### Performance Benchmark

```c
#include "midix.h"
#include <stdio.h>
#include <time.h>

int main() {
    midix_ctx* ctx = midix_create("bench");
    midix_open_output_by_name(ctx, "IAC");

    const int COUNT = 10000;
    midix_time_ns start = midix_now();

    for (int i = 0; i < COUNT; i++) {
        midix_note_on(ctx, 1, 60, 100, 0);
        midix_note_off(ctx, 1, 60, 0, 0);
    }

    midix_flush(ctx);
    midix_time_ns end = midix_now();

    double elapsed_ms = (end - start) / 1000000.0;
    double avg_us = elapsed_ms * 1000.0 / (COUNT * 2);

    printf("Sent %d note pairs in %.2f ms\n", COUNT, elapsed_ms);
    printf("Average latency: %.2f µs per message\n", avg_us);

    midix_destroy(ctx);
    return 0;
}
```

Expected: <5µs average latency

## Debugging

### Enable Verbose Logging

Set environment variable before running:

```bash
export MIDIX_LOG_LEVEL=DEBUG
./examples/test_mac_hal
```

Output will include detailed CoreMIDI operations:

```
[DEBUG] CoreMIDI initialized: client='midix-test'
[DEBUG] Found 2 MIDI destinations
[INFO] Found matching device: 'IAC Driver Bus 1'
[DEBUG] Sent short message: 90 3C 64 (len=3)
[DEBUG] SysEx send completed (status: 0, 6 bytes)
```

### Log Levels

- `ERROR`: Critical failures only
- `WARN`: Warnings and errors
- `INFO`: Normal operations (default)
- `DEBUG`: Detailed diagnostic info

### Instruments.app

For deep profiling:

1. Open Xcode → Open Developer Tool → Instruments
2. Choose "Leaks" or "Time Profiler"
3. Select test program
4. Click Record

## Integration with Full Library

The HAL is designed to integrate with:

1. **Scheduler** (`midix_scheduler.c`): Timestamp-based event queue
2. **Worker Thread**: Asynchronous send processing
3. **High-level API** (`midix_api.c`): User-facing functions

Current status: HAL is standalone and can be tested independently.

## Next Steps

1. Implement scheduler/worker thread
2. Add timestamp support to HAL send functions
3. Implement high-level API wrappers
4. Create CLI parser
5. Full integration testing

## Files Modified/Created

### Created
- `/Users/usedhonda/projects/Mac/SendMIDILibrary/src/mac/midix_hal_mac.mm` (650 lines)
- `/Users/usedhonda/projects/Mac/SendMIDILibrary/examples/test_mac_hal.c` (test program)
- `/Users/usedhonda/projects/Mac/SendMIDILibrary/docs/BUILD_MAC_HAL.md` (this file)
- `/Users/usedhonda/projects/Mac/SendMIDILibrary/docs/log/claude/251001_120624-macOS_HAL_implementation.md` (work log)

### Modified
- `/Users/usedhonda/projects/Mac/SendMIDILibrary/CMakeLists.txt` (fixed source file names)
- `/Users/usedhonda/projects/Mac/SendMIDILibrary/examples/CMakeLists.txt` (added test_mac_hal)

## Support

For issues:
1. Check "Common Issues" section above
2. Enable DEBUG logging
3. Verify CoreMIDI is working: `ls /System/Library/Frameworks/CoreMIDI.framework`
4. Test with Apple's MIDI tools in Audio MIDI Setup

## License

MIT License - See LICENSE file in project root
