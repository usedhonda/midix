# midix CLI Reference

Version: 0.1.0

SendMIDI-compatible command-line MIDI transmission tool.

## Table of Contents

- [Overview](#overview)
- [Command Syntax](#command-syntax)
- [Device Management](#device-management)
- [State Commands](#state-commands)
- [Channel Voice Messages](#channel-voice-messages)
- [RPN/NRPN](#rpnnrpn)
- [System Messages](#system-messages)
- [Real-Time Messages](#real-time-messages)
- [SysEx](#sysex)
- [Raw and File I/O](#raw-and-file-io)
- [Scheduling](#scheduling)
- [Options](#options)
- [Usage Examples](#usage-examples)

---

## Overview

The `midix` CLI provides SendMIDI-compatible syntax for sending MIDI messages from the command line or scripts. It supports both one-shot commands and interactive/streaming modes.

### Basic Usage

```bash
midix [commands...]
```

### Modes

1. **One-shot**: Execute commands and exit
   ```bash
   midix dev "IAC Driver" on 60 100
   ```

2. **File input**: Load commands from text file
   ```bash
   midix file commands.txt
   ```

3. **Interactive/streaming**: Read commands from stdin until EOF
   ```bash
   midix dev "IAC Driver" --
   # Type commands, press Ctrl+D to exit
   ```

---

## Command Syntax

### General Rules

- Commands are **space-separated**
- **Case-insensitive** (except file paths and device names)
- Comments: `#` to end of line
- Quotes: Use `"..."` for device names with spaces
- Numbers: Decimal by default, `H` suffix for hex (e.g., `60` or `3CH`)
- Note names: `C4`, `C#4`, `Db3`, etc. (case-insensitive)

### Number Formats

- **Decimal**: `60`, `127`, `100`
- **Hexadecimal**: `3CH`, `7FH`, `64H` (H suffix)
- **Base switching**: `dec` (decimal mode), `hex` (hex mode)
- **Note names**: `C-2` to `G8` (with `#`/`b` accidentals)

---

## Device Management

### `list`

List all available MIDI output devices.

```bash
midix list
```

**Output:**
```
Available MIDI outputs:
  IAC Driver Bus 1
  IAC Driver Port
  Network Session 1
```

**With --json flag:**
```bash
midix list --json
```
```json
[
  {"index": 0, "name": "IAC Driver Bus 1"},
  {"index": 1, "name": "IAC Driver Port"},
  {"index": 2, "name": "Network Session 1"}
]
```

---

### `dev <name>`

Select MIDI output device by name (partial match, case-insensitive).

```bash
midix dev "IAC Driver"
midix dev IAC        # Partial match
```

**Error:** If device not found, prints error and continues (unless `--strict`).

---

### `virt [name]`

Create virtual MIDI output port (macOS only).

```bash
midix virt "My Virtual Port"
midix virt   # Default name: "midix virtual output"
```

**Note:** On Windows, virtual ports require third-party drivers (loopMIDI, etc.).

---

## State Commands

### `ch <1-16>`

Set default MIDI channel for subsequent messages.

```bash
midix ch 1 on 60 100   # Send on channel 1
midix ch 10 on 36 127  # Send on channel 10
```

**Default:** Channel 1

---

### `dec` / `hex`

Set default number base for parsing.

```bash
midix dec on 60 100    # 60 decimal
midix hex on 3C 64     # 3C and 64 as hex
midix dec on 3CH 100   # 3C forced hex, 100 decimal
```

**Default:** Decimal

---

### `omc <number>`

Set middle C octave number for note name parsing.

```bash
midix omc 3 on C3 100   # C3 = MIDI note 60
midix omc 4 on C4 100   # C4 = MIDI note 60 (default)
```

**Default:** 4 (C4 = 60)

---

## Channel Voice Messages

### `on <note> <velocity>`

Send Note On message.

```bash
midix dev IAC ch 1 on 60 100
midix dev IAC on C4 100
midix dev IAC on C#4 127 on D4 127
```

**Parameters:**
- `note`: MIDI note number (0-127) or note name (C-2 to G8)
- `velocity`: 0-127

---

### `off <note> [velocity]`

Send Note Off message.

```bash
midix dev IAC off 60 0
midix dev IAC off C4
midix dev IAC off C4 64    # Note off with release velocity
```

**Parameters:**
- `note`: MIDI note number or note name
- `velocity`: 0-127 (default: 0)

---

### `pp <note> <value>`

Send Polyphonic Pressure (aftertouch) message.

```bash
midix dev IAC pp 60 80
midix dev IAC pp C4 64
```

**Parameters:**
- `note`: MIDI note number or note name
- `value`: 0-127

---

### `cc <controller> <value>`

Send Control Change message.

```bash
midix dev IAC cc 1 64       # Modulation wheel
midix dev IAC cc 7 127      # Volume max
midix dev IAC cc 64 0       # Sustain off
```

**Parameters:**
- `controller`: CC number (0-127)
- `value`: 0-127

**Common CCs:**
- 1: Modulation
- 7: Volume
- 10: Pan
- 64: Sustain
- 91: Reverb
- 93: Chorus

---

### `cc14 <controller> <value>`

Send 14-bit Control Change (MSB + LSB).

```bash
midix dev IAC cc14 1 8192   # 14-bit modulation (center)
```

**Parameters:**
- `controller`: MSB CC number (0-31)
- `value`: 0-16383 (sends CC N and CC N+32)

---

### `pc <program>`

Send Program Change message.

```bash
midix dev IAC pc 0     # Piano
midix dev IAC pc 24    # Nylon guitar
```

**Parameters:**
- `program`: 0-127

---

### `cp <value>`

Send Channel Pressure (aftertouch) message.

```bash
midix dev IAC cp 64
```

**Parameters:**
- `value`: 0-127

---

### `pb <value>`

Send Pitch Bend message.

```bash
midix dev IAC pb 8192    # Center (no bend)
midix dev IAC pb 0       # Bend down max
midix dev IAC pb 16383   # Bend up max
```

**Parameters:**
- `value`: 0-16383 (8192 = center)

---

## RPN/NRPN

### `rpn <parameter> <value>`

Send Registered Parameter Number (RPN).

```bash
midix dev IAC rpn 0 2        # Pitch bend sensitivity: 2 semitones
midix dev IAC rpn 0 12       # Pitch bend sensitivity: 12 semitones
```

**Common RPNs:**
- 0: Pitch bend sensitivity
- 1: Fine tuning
- 2: Coarse tuning
- 5: Modulation depth range

**Sequence sent:**
1. CC 101 (RPN MSB)
2. CC 100 (RPN LSB)
3. CC 6 (Data Entry MSB)
4. CC 38 (Data Entry LSB)
5. CC 101/100 = 127 (RPN reset)

---

### `nrpn <parameter> <value>`

Send Non-Registered Parameter Number (NRPN).

```bash
midix dev IAC nrpn 0 64
```

**Sequence sent:**
1. CC 99 (NRPN MSB)
2. CC 98 (NRPN LSB)
3. CC 6 (Data Entry MSB)
4. CC 38 (Data Entry LSB)
5. CC 101/100 = 127 (RPN reset to null)

---

## System Messages

### `as`

Send Active Sensing (0xFE).

```bash
midix dev IAC as
```

---

### `rst`

Send System Reset (0xFF).

```bash
midix dev IAC rst
```

---

### `tc <hh> <mm> <ss> <fr>`

Send MIDI Time Code Quarter Frame messages (full frame).

```bash
midix dev IAC tc 1 30 45 12   # 1h 30m 45s 12 frames
```

**Parameters:**
- `hh`: Hours (0-23)
- `mm`: Minutes (0-59)
- `ss`: Seconds (0-59)
- `fr`: Frames (0-29)

---

### `spp <beats>`

Send Song Position Pointer.

```bash
midix dev IAC spp 64    # Position to beat 64
```

**Parameters:**
- `beats`: 0-16383 (in 16th notes from start)

---

### `ss <song>`

Send Song Select.

```bash
midix dev IAC ss 2     # Select song 2
```

**Parameters:**
- `song`: 0-127

---

### `tun`

Send Tune Request (0xF6).

```bash
midix dev IAC tun
```

---

## Real-Time Messages

### `clock <bpm>`

Send MIDI Clock messages at specified tempo.

```bash
midix dev IAC clock 120    # Send clocks at 120 BPM
```

**Behavior:** Sends 24 clock messages per beat continuously until stopped.

**Note:** Blocking operation - use Ctrl+C to stop.

---

### `mc`

Send single MIDI Clock (0xF8).

```bash
midix dev IAC mc
```

---

### `start`

Send Start (0xFA).

```bash
midix dev IAC start
```

---

### `stop`

Send Stop (0xFC).

```bash
midix dev IAC stop
```

---

### `cont`

Send Continue (0xFB).

```bash
midix dev IAC cont
```

---

## SysEx

### `syx <bytes...>`

Send System Exclusive message.

```bash
# F0/F7 auto-added if missing
midix dev IAC syx 7E 7F 09 01

# Explicit F0/F7
midix dev IAC syx F0 7E 7F 09 01 F7

# Mixed decimal/hex
midix dev IAC hex syx F0 7E 7F 09 01 F7
```

**Auto-wrapping:** If first byte is not F0, prepends F0. If last byte is not F7, appends F7.

---

### `syf <file>`

Send SysEx from file (.syx).

```bash
midix dev IAC syf patch.syx
midix dev IAC syf firmware.syx
```

**File format:** Raw binary SysEx data (F0 ... F7).

**Error handling:** If file contains multiple SysEx messages, sends them sequentially.

---

## Raw and File I/O

### `raw <bytes...>`

Send raw MIDI bytes (any message type).

```bash
# Note on (channel 1, note 60, velocity 100)
midix dev IAC raw 90 3C 64

# Multiple messages
midix dev IAC raw 90 3C 64 80 3C 00
```

**Use case:** Debugging, non-standard messages, or precise control.

---

### `file <path>`

Load and execute commands from text file.

```bash
midix file commands.txt
```

**File format:**
```
# commands.txt
dev "IAC Driver"
ch 1

# Play chord
on C4 100
on E4 100
on G4 100
```

**Comments:** Lines starting with `#` are ignored.

---

### `panic`

Send All Notes Off and Sustain Off on all 16 channels.

```bash
midix dev IAC panic
```

**Equivalent to:**
```
cc 123 0   # All notes off
cc 64 0    # Sustain off
(repeated for channels 1-16)
```

---

### `--`

Enter interactive mode: read commands from stdin until EOF.

```bash
midix dev "IAC Driver" --
```

**Usage:**
```bash
$ midix dev IAC --
ch 1
on 60 100
# Type more commands...
# Press Ctrl+D (Unix) or Ctrl+Z (Windows) to exit
```

**Use case:** Pipe input from other programs, live performance scripting.

---

## Scheduling

### `@<ms>`

Schedule message at absolute time (milliseconds from now).

```bash
midix dev IAC @100 on 60 100   # Play note 100ms from now
```

---

### `+<ms>`

Schedule message at relative time (milliseconds from last message).

```bash
midix dev IAC on 60 100 +100 off 60 0   # Note on, then off 100ms later
```

**Cumulative:** Multiple `+` commands stack.

```bash
# Arpeggio: 0ms, 100ms, 200ms, 300ms
midix dev IAC on C4 100 +100 on E4 100 +100 on G4 100 +100 on C5 100
```

---

## Options

### `--drain`

Wait for all scheduled messages to complete before exiting.

```bash
midix dev IAC on 60 100 +500 off 60 0 --drain
```

**Default:** Without `--drain`, exits immediately (messages may be cut off).

---

### `--strict`

Exit with non-zero status on first error.

```bash
midix --strict dev "Nonexistent" on 60 100
# Exits immediately with code 1
```

**Default:** Errors are logged to stderr but execution continues.

---

### `--verbose`

Enable detailed logging (device info, message timestamps, queue status).

```bash
midix --verbose dev IAC on 60 100
```

**Output:**
```
[INFO] Opening device: IAC Driver Bus 1
[DEBUG] Queue length: 0
[DEBUG] Sending Note On: ch=1 note=60 vel=100 ts=0
```

---

### `--json`

Output machine-readable JSON (currently only for `list` command).

```bash
midix list --json
```

---

## Usage Examples

### Example 1: Simple Note

```bash
midix dev "IAC Driver" ch 1 on 60 100
```

---

### Example 2: Chord Sequence

```bash
midix dev IAC ch 1 \
  on C4 100 on E4 100 on G4 100 \
  +500 \
  off C4 0 off E4 0 off G4 0
```

---

### Example 3: CC Sweep

```bash
# Fade volume from 0 to 127
for i in {0..127}; do
  midix dev IAC cc 7 $i
  sleep 0.01
done
```

---

### Example 4: Load SysEx

```bash
midix dev "MIDI Device" syf firmware.syx --drain
```

---

### Example 5: Interactive Session

```bash
$ midix dev IAC --
ch 1
on 60 100
cc 7 127
off 60 0
^D
```

---

### Example 6: Pipe from ReceiveMIDI

```bash
# Forward MIDI from one device to another
receivemidi dev "Input" | midix dev "Output" --
```

---

### Example 7: Script with File

**commands.txt:**
```
# Setup
dev "IAC Driver"
ch 1
omc 4

# Play scale
on C4 100
+100 off C4 0
+0 on D4 100
+100 off D4 0
+0 on E4 100
+100 off E4 0
+0 on F4 100
+100 off F4 0
```

**Execute:**
```bash
midix file commands.txt --drain
```

---

### Example 8: Real-time Clock Master

```bash
# Start clock at 120 BPM
midix dev IAC start clock 120
```

---

### Example 9: Panic All Channels

```bash
midix dev IAC panic
```

---

### Example 10: Virtual Output (macOS)

```bash
# Create virtual port and send notes
midix virt "My Synth" ch 1 on C4 100 +500 off C4 0 --drain
```

---

## Environment Variables

### `MIDIX_LOG_LEVEL`

Set log verbosity.

```bash
export MIDIX_LOG_LEVEL=DEBUG
midix dev IAC on 60 100
```

**Levels:** ERROR, WARN, INFO, DEBUG

---

## Exit Codes

- `0`: Success
- `1`: Device not found (with `--strict`)
- `2`: Invalid command syntax
- `3`: MIDI send error (with `--strict`)
- `4`: File I/O error

---

## Compatibility Notes

### SendMIDI Compatibility

The `midix` CLI is designed to be a drop-in replacement for SendMIDI with the following differences:

**Compatible:**
- All core commands (`dev`, `list`, `on`, `off`, `cc`, `pc`, `pb`, etc.)
- Note name parsing
- Decimal/hex number formats
- File input and stdin modes

**Extensions:**
- `@` and `+` scheduling (not in SendMIDI)
- `--drain` for reliable script execution
- `--strict` for error handling
- `--json` for machine-readable output

**Not implemented (yet):**
- `octave-middle-c` (use `omc` instead)
- Some advanced RPN/NRPN shortcuts

---

## Platform Differences

### macOS
- Virtual outputs work natively (`virt`)
- CoreMIDI provides device list in system order

### Windows
- Virtual outputs require third-party drivers (loopMIDI, etc.)
- WinMM device enumeration may differ from macOS

---

For library API documentation, see [API.md](API.md).
