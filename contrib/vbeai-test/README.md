# vbeaiwav — VBE/AI playback test

A minimal real-mode DOS program that plays a RIFF/WAVE file or a Standard MIDI
File through `INT 10h, AX=4F13h`. It exists to prove the DOSBox-X built-in
VBE/AI provider (`src/ints/int10_vesa_ai.cpp`) end to end — nothing more.

It sniffs the file's magic rather than its extension: `RIFF`/`WAVE` goes to the
WAVE device, `MThd` to the MIDI device.

## Build

Open Watcom, 16-bit real mode, large model:

```bash
WATCOM=/path/to/open-watcom sh ./build.sh
```

which is just:

```bash
wcl -0 -ml -bcl=dos -fe=vbeaiwav.exe vbeaiwav.c
```

Large model matters: the services structure and the sample buffer are both
reached through far pointers. `__pascal` and `__loadds` are Open Watcom
keywords; with another compiler use its equivalents (`pascal` / `_loadds` on
Microsoft C, which is what the original VESA SDK sample code was built with).

## Run

Make sure `dosbox-x.conf` has the provider enabled:

```ini
[vbeai]
vbeai = true
```

Then, inside DOSBox-X:

```
VBEAIWAV GUPPY.WAV
```

With no argument it looks for `TEST.WAV`. `GUPPY.WAV` from the VESA VBE/AI SDK
is a convenient known-good 8-bit mono file.

Expected output:

```
VBE/AI version 1.0 present.
Device: DOSBox-X / VBE/AI Provider (DOSBox-X Mixer)
        features=3004A529 memreq=128 ticks/sec=100
GUPPY.WAV: 1 ch, 11025 Hz, 8 bit, ... bytes
Playing -- ESC to stop.
Playback complete.
```

…and the file should be audible.

### MIDI

```
VBEAIWAV SAKURA2A.MID
```

needs a MIDI output configured, since the provider does not offer a MIDI device
without one:

```ini
[midi]
mididevice = default
```

`ALFRE.MID` (format 0) and `SAKURA2A.MID` (format 1, ten tracks) both ship with
the VESA SDK. Expected output:

```
VBE/AI version 1.0 present.
Device: DOSBox-X / VBE/AI Provider (DOSBox-X MIDI Out)
        features=00000030 memreq=128 tones=65535
sakura2a.mid: 6578 bytes, format 1, 10 track(s), 192 ticks/quarter
Playing -- ESC to stop.
Playback complete.
```

VBE/AI puts tempo and scheduling on the *application* — the driver only ever
sees events that are already due — so this contains a small sequencer: it reads
the file, merges the tracks, follows tempo meta events, and feeds each event to
`msMIDImsg` at the right moment. Timing comes from the BIOS tick combined with
a live read of PIT channel 0, because the 55 ms tick alone is far too coarse
for music.

## What it exercises

### WAVE

| Step | Call |
| --- | --- |
| Presence check | `AX=4F13h BX=0000h` |
| Locate a WAVE device | `AX=4F13h BX=0001h` |
| Query the device class | `AX=4F13h BX=0002h`, `DL=2` |
| Open, donating `wimemreq` bytes | `AX=4F13h BX=0003h` |
| Register the app's sync callback | `wsApplPSyncCB` |
| Set the PCM format | `wsPCMInfo` |
| Register the sample block | `wsWaveRegister` |
| Start playback | `wsPlayBlock` |
| Drive the driver, take the callback | `wsTimerTick` → `wsApplPSyncCB` |
| Poll as a backstop | `wsDeviceCheck(WAVEDRIVERSTATE)` |
| Stop early on ESC | `wsStopIO` |
| Unregister and close | `wsWaveRegister(NULL,h)`, `AX=4F13h BX=0004h` |

### MIDI

| Step | Call |
| --- | --- |
| Locate a MIDI device | `AX=4F13h BX=0001h`, `DL=2` |
| Query the device class | `AX=4F13h BX=0002h`, `DL=2` |
| Open, donating `mimemreq` bytes | `AX=4F13h BX=0003h` |
| Silence before and after | `msGlobalReset` |
| Feed each event at delta time 0 | `msMIDImsg` |
| Close | `AX=4F13h BX=0004h` |

It does not exercise `wsPlayCont`, recording, MIDI input, `msPreLoadPatch` or
Volume. Recording, MIDI input and Volume are not implemented by the provider;
`wsPlayCont` and `msPreLoadPatch` are, and are covered by `hostcheck/` instead.
