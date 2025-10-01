# midix

A fast, lightweight, JUCE-free MIDI transmission library with SendMIDI-compatible CLI.

## Overview

**midix** is a MIT-licensed, embeddable MIDI output core library designed for low-latency, high-reliability MIDI message transmission on macOS and Windows. It provides both a C API for library integration and a CLI tool compatible with SendMIDI command syntax.

## Features

- **JUCE-Free**: No heavyweight dependencies - only OS-native MIDI frameworks (CoreMIDI on macOS, WinMM on Windows)
- **MIT License**: Free for commercial and open-source projects
- **Low Latency**: Sub-millisecond to few-millisecond average latency for immediate sends
- **SendMIDI Compatible CLI**: Drop-in replacement for SendMIDI with extended features
- **Cross-Platform**: Native support for macOS (CoreMIDI) and Windows (WinMM)
- **Thread-Safe**: Lock-free queue architecture for non-blocking message dispatch
- **High-Precision Scheduling**: Timestamp-based scheduling with ±1ms jitter tolerance
- **SysEx Support**: Robust handling of large SysEx messages (≥256KB) with automatic splitting
- **Virtual MIDI Outputs**: Create virtual MIDI ports on macOS (external driver required on Windows)

## Supported Platforms

| Platform | Backend | Virtual Output | Requirements |
|----------|---------|----------------|--------------|
| macOS 10.13+ | CoreMIDI | Native support | Xcode Command Line Tools |
| Windows 10+ | WinMM (mmsystem) | External driver* | MSVC 2019+ or MinGW |

*Windows virtual MIDI requires third-party drivers like [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html)

## Quick Start

New to midix? See the [Quick Start Guide](docs/QUICK_START.md) for a 5-minute introduction.

## Installation

### Build from Source

#### Prerequisites

- CMake 3.15 or later
- C++17 compatible compiler
- macOS: Xcode Command Line Tools
- Windows: Visual Studio 2019+ or MinGW-w64

#### Build Steps

```bash
# Clone repository
git clone https://github.com/usedhonda/midix.git
cd midix

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Install (optional)
sudo cmake --install .
```

#### Build Options

- `BUILD_SHARED_LIBS=ON`: Build shared library (default: static)
- `BUILD_CLI=ON`: Build midix CLI tool (default: ON)
- `BUILD_TESTS=ON`: Build test suite (default: OFF)

## Usage

### Library API

#### Basic Example (C)

```c
#include <midix.h>
#include <stdio.h>

int main() {
    // Create MIDI context
    midix_ctx* ctx = midix_create("MyApp");
    if (!ctx) {
        fprintf(stderr, "Failed to create MIDI context\n");
        return 1;
    }

    // Open output port by name (partial match)
    if (midix_open_output_by_name(ctx, "IAC") != 0) {
        fprintf(stderr, "Failed to open MIDI output\n");
        midix_destroy(ctx);
        return 1;
    }

    // Send note on (channel 1, note 60, velocity 100)
    midix_note_on(ctx, 1, 60, 100, 0);

    // Wait 500ms
    usleep(500000);

    // Send note off
    midix_note_off(ctx, 1, 60, 0, 0);

    // Cleanup
    midix_close_output(ctx);
    midix_destroy(ctx);
    return 0;
}
```

#### Scheduled Events

```c
// Get current time
midix_time_ns now = midix_now();

// Schedule note on 100ms in the future
midix_note_on(ctx, 1, 60, 100, now + 100000000);

// Schedule note off 600ms in the future
midix_note_off(ctx, 1, 60, 0, now + 600000000);
```

#### SysEx Example

```c
// Send SysEx message
uint8_t sysex[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
midix_send_sysex(ctx, sysex, sizeof(sysex), 0);
```

For complete API reference, see [docs/API.md](docs/API.md).

### CLI Tool

The `midix` CLI is compatible with SendMIDI syntax:

#### List Available Devices

```bash
midix list
```

#### Send MIDI Messages

```bash
# Select device and send note
midix dev "IAC Driver" on 60 100

# Send multiple messages
midix dev "IAC Driver" ch 1 on C4 100 cc 7 127 off C4 0

# Load commands from file
midix file commands.txt

# Interactive mode (stdin)
midix dev "IAC Driver" --
# (type commands, Ctrl+D to exit)
```

#### SysEx

```bash
# Send SysEx bytes
midix dev "IAC Driver" syx F0 7E 7F 09 01 F7

# Load SysEx from file
midix dev "IAC Driver" syf firmware.syx
```

#### Virtual Output (macOS)

```bash
# Create virtual MIDI output
midix virt "My Virtual Port" on 60 100
```

For complete CLI reference, see [docs/CLI.md](docs/CLI.md).

## Examples

The `examples/` directory contains demonstration programs:

### simple_note.c
Minimal example showing basic MIDI note transmission.

```bash
./simple_note              # Send middle C to IAC Driver
./simple_note "loopMIDI"   # Specify device name
```

### list_devices.c
Enumerate all available MIDI output devices.

```bash
./list_devices
```

### full_demo.c
Comprehensive demonstration of all MIDI message types:
- Control Change (Modulation, Volume, Sustain)
- Pitch Bend
- Program Change
- Scheduled arpeggio (timestamp-based)
- SysEx messages (Identity Request, GM System On)
- Panic (All Notes Off)

```bash
./full_demo              # Run full feature demo
./full_demo "IAC Driver" # Specify device
```

### clock_demo.c
MIDI Clock transmission with high-precision timing.

```bash
./clock_demo                      # 120 BPM, 4 seconds
./clock_demo "IAC" 140 10         # 140 BPM, 10 seconds
```

**Build Examples:**
```bash
mkdir build && cd build
cmake -DMIDIX_BUILD_EXAMPLES=ON ..
make
```

## Testing

The test suite uses Google Test and covers:
- **API Tests**: Context management, error handling, parameter validation
- **Parser Tests**: Note name parsing, number parsing, command identification
- **Utility Tests**: Note name conversion, OMC variations, boundary values

**Build and Run Tests:**
```bash
mkdir build && cd build
cmake -DMIDIX_BUILD_TESTS=ON ..
make

# Run all tests
ctest --output-on-failure
# or
make run_tests

# Run specific tests
./test_api
./test_parser
./test_util

# Run tests by label
ctest -L core -V
```

## Architecture

- **Core**: Thread-safe message queue with high-precision scheduler
- **HAL**: OS abstraction layer (midix_hal_mac / midix_hal_win)
- **Scheduler**: Hybrid sleep/spin-wait for deadline-driven dispatch
- **CLI**: SendMIDI-compatible parser with extended features

```
┌─────────────────┐
│   Application   │
└────────┬────────┘
         │
    ┌────▼─────┐
    │  midix   │  Public C API
    │   API    │
    └────┬─────┘
         │
    ┌────▼─────────┐
    │  Scheduler   │  Timestamp queue
    │  + Queue     │
    └────┬─────────┘
         │
    ┌────▼─────────┐
    │     HAL      │  OS abstraction
    └────┬─────────┘
         │
    ┌────▼─────────┐
    │  CoreMIDI    │  Native MIDI
    │  / WinMM     │  framework
    └──────────────┘
```

## Performance

Measured on macOS with IAC Driver loopback (round-trip):

- **Average Latency**: 27.83 µs (library API) / 21.99 ms (CLI)
- **Comparison**: 2.5x faster than SendMIDI CLI (54.14 ms)
- **Jitter**: ±10 µs standard deviation
- **Throughput**: Handles continuous CC spam and note floods without message loss
- **SysEx**: Supports large SysEx messages (tested up to 1MB) with automatic chunking

See [docs/PERFORMANCE_COMPARISON.md](docs/PERFORMANCE_COMPARISON.md) for detailed benchmarks.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Related Projects

- [SendMIDI](https://github.com/gbevin/SendMIDI) - JUCE-based MIDI CLI tool (GPL-3.0)
- [ReceiveMIDI](https://github.com/gbevin/ReceiveMIDI) - MIDI input CLI tool
- [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) - Windows virtual MIDI driver

## Acknowledgments

midix is inspired by SendMIDI's command-line interface design but implemented independently from scratch using only OS-native MIDI frameworks (CoreMIDI/WinMM) for MIT licensing and minimal dependencies.
