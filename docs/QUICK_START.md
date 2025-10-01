# Quick Start Guide

This guide will help you get started with midix in 5 minutes.

## Installation

### macOS

```bash
git clone https://github.com/usedhonda/midix.git
cd midix
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Binaries will be in `build/`:
- `midix` - CLI tool
- `libmidix.a` - Static library
- `libmidix.dylib` - Shared library

### Windows

```bash
git clone https://github.com/usedhonda/midix.git
cd midix
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## CLI Quick Start

### 1. List MIDI Devices

```bash
./midix list
```

Output:
```
Available MIDI outputs:
  IAC Driver Bus 1
  MIDI Monitor Input
  My Synth
```

### 2. Send a Note

```bash
# Send Middle C (note 60) to IAC Driver
./midix dev "IAC" ch 1 on 60 100

# Wait a bit
sleep 0.5

# Turn off the note
./midix dev "IAC" ch 1 off 60 0
```

### 3. Play a Melody

```bash
# Play C-D-E-F-G
./midix dev "IAC" ch 1 on C4 100 off C4 0 on D4 100 off D4 0 on E4 100 off E4 0
```

### 4. Send Control Changes

```bash
# Set volume (CC7) to max
./midix dev "IAC" ch 1 cc 7 127

# Set modulation (CC1) to half
./midix dev "IAC" ch 1 cc 1 64
```

### 5. Interactive Mode

```bash
./midix dev "IAC" --
```

Then type commands interactively:
```
ch 1
on 60 100
off 60 0
cc 7 127
```

Press Ctrl+D to exit.

## Library Quick Start

### 1. Simple C Program

Create `test.c`:

```c
#include <midix.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    // Create context
    midix_ctx* ctx = midix_create("TestApp");
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    // Open MIDI device (partial name match)
    if (midix_open_output_by_name(ctx, "IAC") != 0) {
        fprintf(stderr, "Failed to open MIDI device\n");
        midix_destroy(ctx);
        return 1;
    }

    printf("Sending Middle C...\n");

    // Send Note On
    midix_note_on(ctx, 1, 60, 100, 0);

    // Wait 500ms
    usleep(500000);

    // Send Note Off
    midix_note_off(ctx, 1, 60, 0, 0);

    printf("Done!\n");

    // Cleanup
    midix_close_output(ctx);
    midix_destroy(ctx);
    return 0;
}
```

### 2. Compile and Run

```bash
# Compile (macOS)
clang test.c -o test -I../include -L. -lmidix -framework CoreMIDI -framework CoreAudio -framework CoreFoundation

# Run
./test
```

### 3. C++ Example

```cpp
#include <midix.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    auto ctx = midix_create("CppApp");
    if (!ctx) {
        std::cerr << "Failed to create context\n";
        return 1;
    }

    if (midix_open_output_by_name(ctx, "IAC") != 0) {
        std::cerr << "Failed to open device\n";
        midix_destroy(ctx);
        return 1;
    }

    // Play C major arpeggio
    int notes[] = {60, 64, 67, 72};  // C, E, G, C
    
    for (int note : notes) {
        std::cout << "Playing note " << note << "\n";
        midix_note_on(ctx, 1, note, 100, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        midix_note_off(ctx, 1, note, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    midix_close_output(ctx);
    midix_destroy(ctx);
    return 0;
}
```

Compile:
```bash
clang++ -std=c++17 test.cpp -o test -I../include -L. -lmidix -framework CoreMIDI -framework CoreAudio -framework CoreFoundation
```

## Common Tasks

### Send SysEx

```bash
# Send GM System On
./midix dev "IAC" syx F0 7E 7F 09 01 F7
```

### Create Virtual Port (macOS)

```bash
# Create a virtual MIDI output named "MyPort"
./midix virt "MyPort" ch 1 on 60 100
```

Other apps can now receive MIDI from "MyPort".

### Scheduled Events (Library)

```c
// Get current time
midix_time_ns now = midix_now();

// Schedule notes 100ms apart
midix_note_on(ctx, 1, 60, 100, now + 100000000);   // +100ms
midix_note_on(ctx, 1, 64, 100, now + 200000000);   // +200ms
midix_note_on(ctx, 1, 67, 100, now + 300000000);   // +300ms
```

## Troubleshooting

### "No device found"

- Check device name with `./midix list`
- Device names are case-insensitive and support partial matching
- Example: `"IAC"` matches `"IAC Driver Bus 1"`

### Command hangs

- Close Audio MIDI Setup.app if open on macOS
- It can cause MIDI feedback loops with IAC Driver

### Notes don't sound

- Make sure you're sending Note Off after Note On
- Check that a MIDI-capable app is receiving (DAW, MIDI Monitor, etc.)
- Verify the device is not muted

## Next Steps

- Read [API.md](API.md) for complete API reference
- See [CLI.md](CLI.md) for all CLI commands
- Check `examples/` directory for more code samples
- Read [PERFORMANCE_COMPARISON.md](PERFORMANCE_COMPARISON.md) for benchmarks

## Getting Help

- GitHub Issues: https://github.com/usedhonda/midix/issues
- Check existing examples in `examples/` directory
- Read the source: `include/midix.h` has detailed comments
