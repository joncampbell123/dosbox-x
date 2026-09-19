# VESA VBE/AI 1.0 (VESA Audio Interface) — consolidated reference

Working reference for the DOSBox-X built-in VBE/AI provider (`src/ints/int10_vesa_ai.cpp`).
Everything below is taken from primary sources; where a source is ambiguous or
self-contradictory the conflict is called out explicitly rather than smoothed over.

## Sources actually used

| Source | Status | Notes |
| --- | --- | --- |
| **`VBEAI100.pdf`** — *VESA VBE/AI 1.0 Standard*, 02/04/94 | **Primary, complete** | Found in the `volkertb/vbe-ai-sdk` mirror. This is the full 90-page standard, not an abstract. All register-level and structure-level detail below comes from it. |
| **`VBEAI.H` / `VBEAI.INC`** (same SDK) | Primary | Machine-readable structure and constant definitions. Agrees with the PDF throughout. |
| **`VESA.C` / `VESA.H`** (same SDK) | Primary | VESA's own mid-level helper layer. Contains the actual inline-asm `INT 10h` call sites — the authoritative statement of which register holds what. |
| **`PLAY.C`, `TESTW.C`** (same SDK) | Primary | Reference applications. Settle the buffer/streaming model by demonstration. |
| Ralf Brown's Interrupt List | **Not needed** | The SDK spec turned out to be complete, so RBIL was not used as evidence. Its known `???` markers are moot. |
| Miles AIL `VESADIG.ADV`, DIGPAK `DIGVESA.COM` | **Not needed** | Consulting period consumers was contingency for an incomplete spec. The spec is not incomplete. |

The SDK's `README.txt` claims an OPL3 MIDI driver is included; it is not. Only `OPL2.COM`
and `MPU.COM` ship. This does not affect the WAVE path.

## 1. The shape of the interface (and why it matters)

VBE/AI is **not** a pure register-passing BIOS interface. It is a two-layer design:

1. **A register-based discovery layer** — `INT 10h, AX=4F13h`, subfunctions 0–7.
   Used only to find, interrogate, open and close devices. This layer is the *only*
   part that uses registers, and is explicitly *not* Pascal convention.
2. **A vector-table service layer** — opening a device returns a far pointer to a
   structure full of **`pascal far` function pointers**. All actual audio work
   (set format, play, stop, register buffers) happens through far `CALL`s into those
   pointers, not through `INT 10h`.

> *"This is the only interface that does not use the Pascal calling convention. All
> parameters pass to these subfunctions are passed in registers…"* — spec §2, design notes

**Consequence for DOSBox-X:** answering `AX=4F13h` alone is not enough. The provider must
also publish real, callable real-mode entry points in emulated memory for every service
function. That is what DOSBox-X's `CALLBACK_*` machinery is for.

### Pascal calling convention, precisely

For every service-layer function:

- Arguments are pushed **left to right**, so the **last** declared argument sits at the
  **lowest** address above the return address.
- A `long` occupies two words with the **low word at the lower address**.
- A `far *` occupies two words with the **offset at the lower address**, segment above.
- The **callee cleans the stack** — `RETF <total arg bytes>`.
- Return values: `int` → `AX`; `long` and `far *` → `DX:AX`.

So for `wsPCMInfo(int channels, long rate, int comp, int blocking, int pcmsize)`, on entry
(SP relative, after the far call):

```
SP+0  return IP        SP+8   comp
SP+2  return CS        SP+10  rate  (low word)
SP+4  pcmsize          SP+12  rate  (high word)
SP+6  blocking         SP+14  channels
```

…and the function must end with `RETF 12`.

### Success convention

Every `AX=4F13h` subfunction returns `AX == 0x004F` on success — i.e. `AL=0x4Fh`,
`AH=0x00h` — matching VESA VBE video. Any other `AX` is failure. `VESA.C` tests this by
`sub ax,004Fh`.

## 2. `INT 10h, AX=4F13h` subfunctions

### Subfunction 0 — Driver Check

```
In:   AX=4F13h  BX=0000h  CX=0 (reserved)  ES:DI=0 (reserved)
Out:  AX=004Fh on success
      BL = version, nibble-packed (high nibble major, low nibble minor) → 0x10 for 1.0
      BH, CX, DX, SI undefined.  ES:DI unchanged.
```

### Subfunction 1 — Get Next Device Handle

```
In:   AX=4F13h  BX=0001h
      CX = 0 for the first call, else the handle from the previous call
      DL = device class (1=WAVE, 2=MIDI, 3=Volume), or 0 for "any class"
Out:  AX=004Fh on success
      CX = handle of the next matching device, or 0 if there are no more
      BX, DX clobbered.
```

Enumeration is a walk of the driver chain: a driver seeing a null handle claims the call by
returning its own handle; a driver seeing *its own* handle zeroes it and passes the call on,
so the next driver claims it. The application keeps calling until `CX` comes back 0.

> **Spec inconsistency.** §2.2's *Driver Internal Operation* prose says the driver
> "will load it's own handle into **BX**". Both the Input/Output register lists in the same
> section and VESA's own `VESAFindADevice()` in `VESA.C` use **CX**. **CX is correct**; the
> BX mention is an editing error.

### Subfunction 2 — Query Device Class Info

```
In:   AX=4F13h  BX=0002h
      CX = device handle
      DL = query number
      DH = 0 → 16-bit real-mode structures; 1 → 32-bit flat model
      SI:DI = caller's buffer (for the copy queries), or a 32-bit parameter
              (for device-check queries)
Out:  AX=004Fh on success; BX, CX, DX clobbered
      SI:DI = 32-bit result (SI = high word, DI = low word)
```

General queries (`DL` = 1…6):

| `DL` | Meaning |
| --- | --- |
| 1 | Return **length** of the `GeneralDeviceClass` structure in `SI:DI` |
| 2 | **Copy** the `GeneralDeviceClass` structure into `SI:DI` |
| 3 | Return length of the device's `VolumeInfo` structure |
| 4 | Copy the device's `VolumeInfo` structure |
| 5 | Return length of the device's `VolumeService` structure |
| 6 | Copy the device's `VolumeService` structure |

For queries 3–6, **`DX` returns NULL if the device has no volume control**. Queries 7–15 are
reserved.

`DL` ≥ 0x10 selects a **device-check message** instead — the same message numbers the
opened device's `wsDeviceCheck`/`msDeviceCheck`/`vsDeviceCheck` accept, with `SI:DI`
carrying the 32-bit parameter and returning the 32-bit result. Crucially:

> *"Since the device check functions are being executed without requiring the device to be
> opened, the driver device checks must not have any dependencies upon an opened state!"*

### Subfunction 3 — Open Device

```
In:   AX=4F13h  BX=0003h
      CX = device handle
      DX = 0 for the 16-bit interface, 1 for the 32-bit interface
      SI = segment/selector of a caller-allocated block of `memreq` bytes
           (the offset is assumed to be zero)
Out:  AX=004Fh on success
      SI:CX = far pointer to the device's services structure
              (SI = segment, CX = offset) — NULL if unavailable
      BX, DX clobbered.
```

Note the **asymmetry**: subfunction 2 returns a value in `SI:DI`, subfunction 3 returns a
pointer in `SI:CX`. Confirmed by `VESAOpenADevice()` in `VESA.C`:

```asm
mov si,word ptr [memptr+2]   ; pass in the segment
int 10h
mov word ptr [cc+2],si       ; segment of result
mov word ptr [cc+0],cx       ; offset of result
```

The driver writes its services structure **into the caller's block** and returns a pointer to
it. The rationale is ROM residency: a driver in ROM has nowhere to keep per-open state, so the
application donates the RAM. Size comes from the `memreq` field of the device's Info structure.

**Volume devices cannot be opened.** An open against a Volume device must fail; volume access
is entirely through subfunction 2 queries 3–6, because opening implies exclusive ownership and
that is inappropriate for a mixer control.

### Subfunction 4 — Close Device

```
In:   AX=4F13h  BX=0004h  CX = device handle
Out:  AX=004Fh on success; BX, CX, DX clobbered
```

On close a WAVE driver must stop audio, release DMA/IRQ, and return all queued buffers.
After the call returns the driver **must not touch the application's memory block again**.
Close on a Volume device is ignored.

### Subfunction 5 — Driver Unload Request

```
In:   AX=4F13h  BX=0005h  CX = device handle
Out:  AX=004Fh on success, BX = PSP segment of the unloading TSR
```

Only meaningful for TSR drivers. A ROM/BIOS-resident provider cannot unload and must fail
this call.

### Subfunction 6 — Driver Chaining (drivers only, never applications)

```
Step 1 (chain in):   AX=4F13h  BL=06h  BH=0   DX:CX = this driver's entry point
                     Out: AX=004Fh, DX:CX = next-in-chain address to call
Step 2 (chain out):  AX=4F13h  BL=06h  BH=1   DX:CX = this driver's entry point
                     SI:DI = the next-in-chain address received in step 1
                     Out: AX=004Fh on success
```

The first driver to chain becomes permanent and can never unload, because the whole scheme
depends on at least one driver being present to service chain requests.

### Subfunction 7 — 32-bit interface loading

*"reserved for appending 32 bit services to the driver query capabilities - to be defined
later"*. The spec defines nothing. Fail it.

## 3. Data structures

All structures below are laid out with 2-byte fields at even offsets and no interior padding.
**Byte-packing versus word-packing is not ambiguous here**: every field in every structure
already falls on an even offset naturally (note how each Info structure pads `boardid` with a
3-byte `unused` array), so `/Zp1` and `/Zp2` produce identical layouts. There is no
compiler-dependent layout risk.

### `GeneralDeviceClass` — 138 bytes with a `WAVEInfo` payload

| Off | Size | Field |
| --- | --- | --- |
| 0 | 4 | `gdname[4]` — `"VESA"` |
| 4 | 4 | `gdlength` — structure length |
| 8 | 2 | `gdclassid` — 1 WAVE / 2 MIDI / 3 Volume |
| 10 | 2 | `gdvbever` — VBE/AI version |
| 12 | … | union of `WAVEInfo` / `MIDIInfo` / `VolumeInfo` |

### `WAVEInfo` — 126 bytes (at offset 12 of the above)

| Off | Size | Field | Meaning |
| --- | --- | --- | --- |
| 0 | 4 | `winame[4]` | `"WAVI"` |
| 4 | 4 | `wilength` | structure length |
| 8 | 4 | `wiversion` | driver version, BCD |
| 12 | 32 | `wivname[32]` | vendor name, ASCIIZ |
| 44 | 32 | `wiprod[32]` | product name, ASCIIZ |
| 76 | 32 | `wichip[32]` | chip/hardware description, ASCIIZ |
| 108 | 1 | `wiboardid` | board number |
| 109 | 3 | `wiunused[3]` | padding |
| 112 | 4 | `wifeatures` | feature bits (below) |
| 116 | 2 | `widevpref` | user preference; **0 = highest** |
| 118 | 2 | `wimemreq` | bytes the app must donate at open |
| 120 | 2 | `witimerticks` | callbacks/sec the app must deliver; 0 = none |
| 122 | 2 | `wiChannels` | 1 = mono device, 2 = stereo device |
| 124 | 2 | `wiSampleSize` | 0x01 8-bit play, 0x02 16-bit play, 0x10 8-bit rec, 0x20 16-bit rec |

`wifeatures` bits:

```
0x00000001 8000 Hz mono play      0x00000020 11025 Hz mono play
0x00000002 8000 Hz mono rec       0x00000040 11025 Hz mono rec
0x00000004 8000 Hz stereo rec     0x00000080 11025 Hz stereo rec
0x00000008 8000 Hz stereo play    0x00000100 11025 Hz stereo play
0x00000010 8000 Hz full duplex    0x00000200 11025 Hz full duplex

0x00000400 22050 Hz mono play     0x00008000 44100 Hz mono play
0x00000800 22050 Hz mono rec      0x00010000 44100 Hz mono rec
0x00001000 22050 Hz stereo rec    0x00020000 44100 Hz stereo rec
0x00002000 22050 Hz stereo play   0x00040000 44100 Hz stereo play
0x00004000 22050 Hz full duplex   0x00080000 44100 Hz full duplex

0x08000000 wsWavePrepare required before playback
0x10000000 variable mono play rates     0x20000000 variable stereo play rates
0x40000000 variable mono rec rates      0x80000000 variable stereo rec rates
```

Note the bit ordering is irregular — *stereo record* comes before *stereo playback* at every
rate. This is not a transcription slip; `VBEAI.H` and the spec text agree.

### `WAVEService` — 84 bytes, written into the application's block

| Off | Size | Field | Args | `RETF n` |
| --- | --- | --- | --- | --- |
| 0 | 4 | `wsname[4]` = `"WAVS"` | | |
| 4 | 4 | `wslength` | | |
| 8 | 16 | `wsfuture[16]` | | |
| 24 | 4 | `wsDeviceCheck(int, long)` | 6 | 6 |
| 28 | 4 | `wsPCMInfo(int, long, int, int, int)` | 12 | 12 |
| 32 | 4 | `wsPlayBlock(int, long)` | 6 | 6 |
| 36 | 4 | `wsPlayCont(void far*, long, long)` | 12 | 12 |
| 40 | 4 | `wsRecordBlock(int, long)` | 6 | 6 |
| 44 | 4 | `wsRecordCont(void far*, long, long)` | 12 | 12 |
| 48 | 4 | `wsPauseIO(int)` | 2 | 2 |
| 52 | 4 | `wsResumeIO(int)` | 2 | 2 |
| 56 | 4 | `wsStopIO(int)` | 2 | 2 |
| 60 | 4 | `wsWavePrepare(int, int, int, void far*, long)` | 14 | 14 |
| 64 | 4 | `wsWaveRegister(void huge*, long)` | 8 | 8 |
| 68 | 4 | `wsGetLastError(void)` | 0 | 0 |
| 72 | 4 | `wsTimerTick(void)` | 0 | 0 |
| 76 | 4 | `wsApplPSyncCB(int, void far*, long, long)` | 14 | **written by the app** |
| 80 | 4 | `wsApplRSyncCB(int, void far*, long, long)` | 14 | **written by the app** |

The two callback slots are the **only** fields the application may write.

The `wsname`/`msname` tag is load-bearing, not decorative: `VESAOpenADevice()` in `VESA.C`
memcmp's the first four bytes against `"WAVS"` / `"MIDS"` to decide which structure it just
received. Getting the tag wrong makes the helper silently skip timer registration.

## 4. The WAVE playback model — settled

This was the main open question going in. It is **not** a pull model and there is no callback
the driver uses to request data. It is a **push model with pre-registration**, in two flavours.

### Flavour A — single block (`wsWaveRegister` + `wsPlayBlock`)

```
wsPCMInfo(channels, rate, compression, blocking, pcmsize)   → best-match rate, or 0 on error
handle = wsWaveRegister(far_ptr_to_pcm, length_in_bytes)    → block handle, or 0 on error
wsPlayBlock(handle, 0)                                      → nonzero if started
    …plays to completion in the background…
wsApplPSyncCB(driver_handle, ptr, length, 0)                ← driver calls the app
```

Exactly the sequence `PLAY.C` performs. Rules from the spec:

- Up to **32 blocks** may be registered; only one plays at a time.
- A registered block's format is frozen at the **`wsPCMInfo` state current at registration
  time**.
- While registered, the application must not touch the buffer, and the buffer must stay put.
- Unregister by calling `wsWaveRegister(NULL, handle)`.
- A new `wsPlayBlock` while busy **stops the current block and fires its callback first**.
- **There is no queuing.** The hardware goes idle at the end of every block. Gapless playback
  is the application's problem — which is exactly why `wsPlayCont` exists.

### Flavour B — continuous / circular (`wsPlayCont`)

```
wsPlayCont(far_ptr_to_buffer, buffer_length, division_length)
```

Models an auto-init DMA ring. The application owns the ring, pre-fills it (at least two
divisions' worth), and gets `wsApplPSyncCB` after each division drains, with a pointer to the
division that just completed. Constraints:

- Buffer must not cross a 64 KB boundary — the XT DMA limit, adopted as the lowest common
  denominator for *all* devices, DMA or not.
- Maximum buffer 64 KB; suggested 4096 bytes with a 2048-byte division.
- Returns TRUE if running, FALSE on error.

### Callback timing and delivery

> *"the callback to the application is expected to occur when the data has played out the DAC,
> i.e., has been heard by the user."*

Not when the buffer is handed to hardware. A FIFO device must withhold the callback until the
data has actually passed the DAC.

`wsApplPSyncCB(int handle, void far *ptr, long len, long reserved)` — 14 bytes of arguments,
`RETF 14`. The spec's parameter table is garbled by the PDF's column layout; the correct
reading, cross-checked against `PLAY.C`'s `OurCallback(han, fptr, len, filler)`, is: driver
handle; pointer to the block (or NULL when returning a *block handle* instead, in which case
the handle arrives in the third parameter); length in bytes; reserved, zero.

> *"The application is responsible for saving all registers. The CPU Flags register may be
> modified."* — and *"No assumptions can be made about the segment registers"* (`PLAY.C`).

**How the driver gets control to make that call** is the design-critical part. VBE/AI is
explicit that a driver may be entirely timer-driven — the Disney Sound Source and Media Vision
AudioPort drivers in this very SDK work that way:

> *"Non-DMA Block oriented devices can be supported as Block I/O devices that use timer tick
> callbacks to keep data moving… If the device driver requires timer tick callbacks, then the
> application will be responsible for calling the driver at the specified rate."*

The application learns the rate from `witimerticks` and calls `wsTimerTick()` that often.
`VESA.C` automates this: `VESAQueryDevice(…, VESAQUERY2, …)` stashes `witimerticks`, and the
subsequent `VESAOpenADevice()` hooks the 8253 and drives `wsTimerTick` automatically. The
practical ceiling is **500 ticks/sec** (one per 2 ms); `wsTimerTick` is explicitly **not
reentrant**, and the application is responsible for guarding against reentry.

This is the mechanism the DOSBox-X provider uses to deliver completion callbacks — see
"Implementation notes" below.

### `wsDeviceCheck` messages (WAVE)

| Msg | Name | Parameter | Returns |
| --- | --- | --- | --- |
| 0x11 | `WAVECOMPRESSION` | LO: compression type, HI: block size | TRUE if supported |
| 0x12 | `WAVEDRIVERSTATE` | — | −1 not open, 0 idle, 1 busy; OR 0x80 if paused |
| 0x13 | `WAVEGETCURRENTPOS` | — | bytes played/recorded in the current block |
| 0x14 | `WAVESAMPLERATE` | mono sample rate | 0 if unsupported, else closest match (≤5% delta) |
| 0x15 | `WAVESETPREFERENCE` | new preference, or −1 to query only | old preference |
| 0x16 | `WAVEGETDMAIRQ` | — | HI word: DMA channels, LO word: IRQs; 0xFF = none |
| 0x17 | `WAVEGETIOADDRESS` | — | base I/O address |
| 0x18 | `WAVEGETMEMADDRESS` | — | LO: base segment, HI: size; −1 = not memory mapped |
| 0x19 | `WAVEGETMEMFREE` | — | free on-board memory, 0 if none |
| 0x1A | `WAVEFULLDUPLEX` | 0 disable / 1 enable / 2 query | current state |
| 0x1B | `WAVEGETBLOCKSIZE` | LO: PCM size, HI: compression | smallest transferable unit, in samples |
| 0x1C | `WAVEGETPCMFORMAT` | — | LO: supported format bits, HI: currently enabled |
| 0x1D | `WAVEENAPCMFORMAT` | format bits to enable | — |
| ≥0x80 | vendor-specific | | |

PCM format bits for 0x1C/0x1D: `0x01` 8-bit signed, `0x02` 8-bit **unsigned**, `0x10` 16-bit
**signed**, `0x20` 16-bit unsigned. The spec's baseline pair — the formats every driver is
expected to handle without `wsWavePrepare` — is **unsigned 8-bit and signed 16-bit**, i.e.
exactly what a RIFF/WAVE file contains.

> The spec's worked examples for `WAVEENAPCMFORMAT` are visibly wrong: `0x00000012` is listed
> as both a legal and an illegal value on consecutive lines. The intent is clear enough — one
> bit set per sample size — and the error does not affect implementation.

### WAVE error codes (`wsGetLastError`)

```
1 unsupported feature/function   4 bad block address
2 bad sample rate                5 application missed an IRQ
3 bad block length               6 unrecognised PCM size/format
                              0x80 vendor-specific hardware failure
```

The driver zeroes the stored error once it has been read.

## 5. MIDI and Volume (not implemented; recorded for completeness)

**MIDI** — `MIDIService` is tagged `"MIDS"`. Services: `msDeviceCheck`, `msGlobalReset`,
`msMIDImsg(char far*, int)`, `msPollMIDI(int)`, `msPreLoadPatch`, `msUnloadPatch`,
`msTimerTick`, `msGetLastError`, plus app callbacks `msApplFreeCB` and `msApplMIDIIn`. The
structure carries a 16-entry `mspatches` bitfield of loaded patches and a `milibrary[14]`
patch-library filename. Registered patch types: `0x10` OPL2, `0x11` OPL3. A future MIDI device
would map `msMIDImsg` onto DOSBox-X's existing MIDI/OPL handling.

**Volume** — `VolumeInfo` and `VolumeService` (`"VOLS"`), reached only through subfunction 2
queries 3–6. Services: `vsDeviceCheck`, `vsSetVolume(int,int,int)`, `vsSetFieldVol`,
`vsToneControl`, `vsFilterControl`, `vsOutputPath`, `vsResetChannel`, `vsGetLastError`.
`VOL_USERSETTING`/`VOL_APPSETTING` distinguish the user's master setting from the
application's. This maps cleanly onto `MixerChannel::SetVolume` if implemented later.

## 6. Implementation notes — DOSBox-X provider

Decisions taken for `src/ints/int10_vesa_ai.cpp`, and why.

**Dispatch.** `AX=4F13h` is intercepted in `src/ints/int10.cpp` **before** the existing
`case 0x4f` guards that require an S3/VGA card. VBE/AI is an audio interface; gating it on the
emulated video card would be wrong.

**Service entry points.** Each of the 13 WAVE service functions gets a DOSBox-X callback. The
stock `CB_RETF` stub ends in a plain `RETF`, which is caller-cleanup — wrong for Pascal. Each
stub's return instruction is therefore patched to `RETF imm16` (`0xCA`) with that function's
argument byte count.

**Format negotiation.** Deliberately **not** limited to one fixed format. The provider accepts
1 or 2 channels, 8- or 16-bit, at any rate from 4000 to 48000 Hz, because DOSBox-X's mixer
resamples anyway and restricting it would buy nothing. `wsPCMInfo` returns the rate it will
actually use. The advertised `wifeatures` includes the four standard rates plus the
variable-rate bits, which is truthful.

**Audio path.** Output goes to a dedicated `MixerChannel` named `VBEAI`, independent of any
emulated Sound Blaster/GUS/OPL. No I/O port, IRQ or DMA channel is claimed, so `WAVEGETDMAIRQ`
reports `0xFF` for every channel and `WAVEGETIOADDRESS` reports 0 — truthfully, since there is
no hardware.

**Callback delivery.** The provider advertises `witimerticks = 100` and delivers
`wsApplPSyncCB` from inside its `wsTimerTick` handler, by building a Pascal frame on the guest
stack and letting the stub's `RETF` land in the application's callback — the same technique
`src/ints/mouse.cpp` uses to enter a user mouse handler from `INT 74h`. This is the
spec-sanctioned path for a non-DMA device, and `VESA.C` drives it automatically for any
application that queries the device before opening it.

Delivery happens **only** from `wsTimerTick` and from the sync-return stub — both of which
return `void`. It deliberately does not happen on entry to the other service calls: the
application's callback runs *after* our handler returns, so it would overwrite the `AX` /
`DX:AX` those functions are returning.

*Known limitation:* an application that ignores `witimerticks` and never calls `wsTimerTick`
will receive no completion callbacks. It can still track playback by polling the
`WAVEDRIVERSTATE` and `WAVEGETCURRENTPOS` device checks, which the provider also answers
without an open device as the spec requires. Hooking `INT 08h` internally would remove the
limitation entirely and is the obvious next step if a real-world consumer turns out to need
it.

**Callback delivery mechanics.** `wsApplPSyncCB` is entered by pushing its Pascal frame onto
the guest stack and letting the `wsTimerTick` stub's own `RETF` land on the application's
callback, with the internal sync-return stub as that callback's return address. When the
application's `RETF 14` returns into the sync-return stub, its handler drains the next queued
callback the same way, so a backlog is delivered in one pass before control goes back to the
caller of `wsTimerTick`.

**Configuration.** A dedicated `[vbeai]` section with a single `vbeai = true|false` key,
matching how `[sblaster]`, `[gus]` and friends are structured. Default **on**: the interface
is purely additive — `AX=4F13h` currently returns "unsupported" — and the whole point is that
software finds it without setup, exactly as VESA intended drivers to be preloaded before an
application runs.
