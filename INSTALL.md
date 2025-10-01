# midix Build Guide

## Quick Build

### macOS
```bash
# Build in current directory
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Binaries will be created in:
# - ./midix (CLI tool)
# - ./libmidix.a (static library)
# - ./libmidix.dylib (shared library)
# - ./examples/ (example programs)
# - ./benchmarks/ (benchmark tools)
```

### Windows
```powershell
# Using Visual Studio
cmake . -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Binaries in:
# - Release\midix.exe
# - Release\midix.lib
# - Release\midix.dll
# - examples\Release\
# - benchmarks\Release\
```

---

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `MIDIX_BUILD_SHARED` | ON | Build shared library (.dylib/.dll) |
| `MIDIX_BUILD_STATIC` | ON | Build static library (.a/.lib) |
| `MIDIX_BUILD_CLI` | ON | Build `midix` CLI tool |
| `MIDIX_BUILD_EXAMPLES` | ON | Build example programs |
| `MIDIX_BUILD_TESTS` | OFF | Build test suite (requires GoogleTest) |
| `MIDIX_BUILD_BENCHMARKS` | ON | Build benchmark programs |
| `CMAKE_BUILD_TYPE` | Release | Build type (Release/Debug) |

### Examples

**Minimal CLI-only build:**
```bash
cmake . \
  -DMIDIX_BUILD_SHARED=OFF \
  -DMIDIX_BUILD_STATIC=ON \
  -DMIDIX_BUILD_CLI=ON \
  -DMIDIX_BUILD_EXAMPLES=OFF \
  -DMIDIX_BUILD_BENCHMARKS=OFF

cmake --build .
```

**Library-only (no CLI):**
```bash
cmake . \
  -DMIDIX_BUILD_SHARED=ON \
  -DMIDIX_BUILD_STATIC=ON \
  -DMIDIX_BUILD_CLI=OFF \
  -DMIDIX_BUILD_EXAMPLES=OFF \
  -DMIDIX_BUILD_BENCHMARKS=OFF

cmake --build .
```

**Debug build with tests:**
```bash
cmake . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DMIDIX_BUILD_TESTS=ON

cmake --build .
ctest --output-on-failure
```

---

## Using midix in Your Project

### Manual Linking

#### macOS
```bash
# Link with static library
clang myapp.c -o myapp \
  -I./include \
  -L. \
  -lmidix \
  -framework CoreMIDI \
  -framework CoreAudio \
  -framework CoreFoundation

# Or with shared library
clang myapp.c -o myapp \
  -I./include \
  ./libmidix.dylib \
  -framework CoreMIDI \
  -framework CoreAudio \
  -framework CoreFoundation
```

#### Windows (Visual Studio)
```powershell
cl myapp.c /I".\include" ^
  /link /LIBPATH:"." midix.lib winmm.lib
```

---

## Run Tests

```bash
# Run examples
./examples/simple_note
./examples/list_devices
./examples/full_demo

# Run benchmarks
./benchmarks/benchmark_latency
./benchmarks/benchmark_throughput

# Run CLI
./midix list
./midix dev "IAC" on 60 100
```

---

## Platform Requirements

### macOS
- macOS 10.13 (High Sierra) or later
- Xcode Command Line Tools (`xcode-select --install`)

### Windows
- Windows 10 (1809) or later
- Visual Studio 2019+ or MinGW-w64
- Virtual MIDI requires third-party driver (e.g., loopMIDI)
