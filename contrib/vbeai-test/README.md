# vbeaiwav — VBE/AI WAVE playback test

A minimal real-mode DOS program that plays a RIFF/WAVE file through
`INT 10h, AX=4F13h`. It exists to prove the DOSBox-X built-in VBE/AI provider
(`src/ints/int10_vesa_ai.cpp`) end to end — nothing more.

## Build

Open Watcom, 16-bit real mode, large model:

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
        features=3004A529 memreq=256 ticks/sec=100
GUPPY.WAV: 1 ch, 11025 Hz, 8 bit, ... bytes
Playing -- ESC to stop.
Playback complete.
```

…and the file should be audible.

## What it exercises

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

It does not exercise `wsPlayCont`, recording, MIDI or Volume. Recording, MIDI
and Volume are not implemented by the provider; `wsPlayCont` is, but a single
block is enough to prove the path.
