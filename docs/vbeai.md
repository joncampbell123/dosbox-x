# VESA VBE/AI 1.0 (VESA Audio Interface) — consolidated reference

Working reference for the DOSBox-X built-in VBE/AI provider (`src/ints/int10_vesa_ai.cpp`).
Everything below is taken from primary sources; where a source is ambiguous or
self-contradictory the conflict is called out explicitly rather than smoothed over.

## Sources actually used

| Source | Status | Notes |
| --- | --- | --- |
| **`VBEAI100.pdf`** — *VESA VBE/AI 1.0 Standard*, 02/04/94 | **Primary, complete** | Found in the `volkertb/vbe-ai-sdk` mirror. This is the full 90-page standard, not an abstract. All register-level and structure-level detail below comes from it. |
| **`VBEAI.H` / `VBEAI.INC`** (same SDK) | Primary | Machine-readable structure and constant definitions. Agrees with the PDF on the WAVE side; **disagrees on the MIDI feature bits**, where the two headers agree with each other against the prose (see §8). |
| **`OPL2.COM`, `MPU.COM`, `SBWAVE.COM`…** (same SDK) | Primary | The shipped driver binaries. Consulted to confirm the four-character structure tags (`"VESA"`, `"WAVI"`, `"WAVS"`, `"MIDI"`, `"MIDS"`) rather than guessing them. |
| **`VESA.C` / `VESA.H`** (same SDK) | Primary | VESA's own mid-level helper layer. Contains the actual inline-asm `INT 10h` call sites — the authoritative statement of which register holds what. |
| **`PLAY.C`, `TESTW.C`** (same SDK) | Primary | Reference applications. Settle the buffer/streaming model by demonstration. |
| Ralf Brown's Interrupt List | **Not needed** | The SDK spec turned out to be complete, so RBIL was not used as evidence. Its known `???` markers are moot. |
| Miles AIL `VESADIG.ADV`, DIGPAK `DIGVESA.COM` | **Not needed** | Consulting period consumers was contingency for an incomplete spec. The spec is not incomplete. |

The SDK's `README.txt` claims an OPL3 MIDI driver is included; it is not. Only `OPL2.COM`
and `MPU.COM` ship. `MPU.COM`, the MPU-401 transmitter driver, is the closest analogue to
what this provider's MIDI device does.

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

## 5. Volume (not implemented; recorded for completeness)

MIDI is implemented -- see sections 8 and 9.

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

**Configuration.** A dedicated `[vbeai]` section with `vbeai = true|false` and
`midimode` (section 9),
matching how `[sblaster]`, `[gus]` and friends are structured. Default **on**: the interface
is purely additive — `AX=4F13h` currently returns "unsupported" — and the whole point is that
software finds it without setup, exactly as VESA intended drivers to be preloaded before an
application runs.

## 7. Verification

Three layers, because "it made a noise" is not evidence that the right samples came out.

**Host-side white-box check** — `contrib/vbeai-test/hostcheck/`. Compiles the provider against
stub headers faking DOSBox-X's guest memory, registers, callbacks and mixer, then drives it
directly: 187 assertions covering the trampoline encoding, the `INT 10h` subfunctions, the
structure byte offsets, Pascal argument decoding, block and continuous playback stepping, and
the exact shape of the completion-callback frame left on the guest stack.

**End-to-end under the emulator** — `contrib/vbeai-test/vbeaiwav.c` run inside the modified
DOSBox-X. Its `Playback complete.` line is printed only when the driver's `wsApplPSyncCB`
callback actually fired, so it exercises the whole chain, guest stack frame included.

**The audio itself.** DOSBox-X's bundled SDL 1.x includes the disk audio driver, so the mixer
output can be captured to a file rather than a sound card:

```bash
SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=mix.raw dosbox-x -conf dosbox-x.conf
```

With `[mixer] rate = 22050` matching the sample, `mix.raw` is raw 16-bit stereo at the source
rate. Cross-correlating each captured 1024-frame block against the source WAV gives:

```
block 14 -> source offset   2746   correlation 0.99995
block 15 -> source offset   6076   correlation 1.00000
...
block 25 -> source offset  39305   correlation 0.99999
```

Every correlation ≥ 0.9998, offsets strictly increasing and evenly spaced from the start of the
file to near its end (41727 bytes), with the mono source correctly duplicated to both channels.
The disk driver's pacing drops buffers between captures, which is why the blocks are sampled
rather than contiguous; what matters is that each one *is* the source waveform at the expected
position. Correlation rather than exact equality because the mixer interpolates.

One analysis caveat: **skip any block that is not fully filled**. Playback generally begins
partway through a block, so the first one is part silence, and correlating that against the
source scores around 0.79 — an artefact of the measurement, not of the audio. Only blocks with
1024 non-zero frames are meaningful.

### Bugs this caught

Worth recording, since both would have presented as a hang rather than an error:

- **Continuous mode spun forever at the end of a lap.** The division-callback loop reset
  `divdone` to zero *inside* the loop, so `playpos >= (divdone+1)*divlen` became true again
  immediately. `divdone` must only be reset by the wrap that follows the loop. This ran in
  DOSBox-X's mixer, so it would have frozen the emulator.
- **A block whose length was not a whole number of frames never completed.** The sub-frame
  tail could never be played, `playpos` never reached `playlen`, the device stayed busy
  forever and the completion callback never fired.

## 8. MIDI device

Implemented as a **MIDI transmitter/receiver** (`MIDIFXMITR`) whose downstream already holds the
General MIDI patches (`MIDIFPRELD`), because that is exactly what DOSBox-X's MIDI output is:
whatever the user configured in `[midi]` — MT-32, FluidSynth, or the host synthesiser.

That framing removes most of chapter 5's complexity. There is no synthesis here, no voice
allocation, and no patch library: `msMIDImsg` hands bytes to `MIDI_RawOutByte`, which already
implements running status, sysex framing and realtime messages — precisely what the spec
requires of a driver ("the drivers are required to handle the data as if it came in one byte at
a time… handle running status, and process the MIDI protocol").

> **Header/spec conflict.** §5.4's prose puts *patches preloaded* at `0x40` and *internal time
> stamping* at `0x80`, leaving `0x20` unassigned. `VBEAI.H` **and** `VBEAI.INC` both put them at
> `0x20` and `0x40` with no gap. Two independent machine-readable headers from the same SDK
> agree with each other and disagree with the PDF, and the headers are what period drivers and
> applications actually compiled against — so the implementation follows the headers.

The `"MIDI"` / `"MIDS"` structure tags were taken from the SDK's own shipped drivers
(`OPL2.COM`, `MPU.COM`) rather than guessed; the same check confirmed `"VESA"`, `"WAVI"` and
`"WAVS"` on the WAVE side.

### What is provided

| | |
| --- | --- |
| `msDeviceCheck` | All nine messages. `MIDITONES` and `MIDIVOICESTEAL` return `0xFFFF`, which the spec explicitly sanctions for transmitter/receivers that cannot know the answer. |
| `msGlobalReset` | All Notes Off + Reset All Controllers on all 16 channels. |
| `msMIDImsg` | Forwards the block verbatim, byte by byte. |
| `msPreLoadPatch` | Forwards sysex to the downstream device, as the spec requires of a transmitter — but only if the block really begins `F0`, rather than spraying an OPL patch blob at the synthesiser. Anything else fails with `MID_UNKNOWNPATCH`. |
| `msUnloadPatch` | Succeeds; nothing is retained on this side. |
| `msPollMIDI`, `msTimerTick` | No-ops. `mitimerticks` is 0 and no input bits are advertised, so neither is needed. |

**Not provided:** MIDI input. Neither `MIDIINTR` nor `MIDIPOLL` is advertised, so
`msApplMIDIIn` is never called. `miactivetones` reports `0xFFFF` because the downstream device
cannot be interrogated.

**Presence.** `MIDI_RawOutByte` dereferences DOSBox-X's MIDI handler without checking it, and
that handler is null when no MIDI output is configured. So the MIDI device is **not enumerated
at all** unless `MIDI_Available()`, and every byte the provider emits additionally goes through
a guarded helper — availability can change if the user reconfigures `[midi]` between
enumeration and use.

**Handles.** WAVE is 1, MIDI is 2, and subfunction 1 walks them in ascending order, returning
the first past the caller's previous handle that matches the requested class.

### Verification

The host check covers enumeration (including that the MIDI device disappears when no output is
configured), the `MIDIInfo`/`MIDIService` byte offsets, verbatim byte forwarding, the device
checks, `msGlobalReset`'s exact 96-byte output, and both `msPreLoadPatch` paths.

End to end, `vbeaiwav` plays a Standard MIDI File through the device. Because MIDI leaves
through the MIDI handler rather than the mixer, the audio-correlation trick used for WAVE does
not apply; instead the provider counts the application bytes it forwards and logs the total at
close, which can be checked against an independent parse of the file:

```
SAKURA2A.MID (format 1, 10 tracks)  expected 4828 bytes
VBE/AI: MIDI forwarded 4828 application bytes
```

An exact match means the DOS sequencer parsed the file, merged all ten tracks and delivered
every event, and the provider forwarded all of it.

## 9. `midimode` and the FM synthesiser

VBE/AI's MIDI class covers two quite different kinds of driver, and the provider implements
both. `[vbeai] midimode` selects which one a guest sees:

| Value | Device presented |
| --- | --- |
| `auto` (default) | `transmitter` if a MIDI output is configured, otherwise no MIDI device |
| `transmitter` | MIDI transmitter/receiver forwarding to `[midi] mididevice` |
| `opl2` | FM synthesiser on a private OPL2, 9 two-operator voices |
| `opl3` | FM synthesiser on a private OPL3, 18 two-operator voices |
| `none` | No MIDI device; only the WAVE device is offered |

The setting lives under `[vbeai]` rather than `[midi]` because the choice is a VBE/AI-level
one: it changes `mifeatures`, `michip`, `miactivetones`, and whether a patch library is
meaningful. It is *not* named `mididevice`, deliberately — a `[vbeai] mididevice` sitting next
to `[midi] mididevice` would be a support trap.

Note what the mode does **not** do: VBE/AI's enumeration and `midevpref` exist precisely so that
several MIDI devices can coexist and the application picks one, which is how a period machine
with both `MPU.COM` and `OPL2.COM` resident would have behaved. An exclusive switch forecloses
that. It is still the better trade here — coexistence relies on every application honouring
`midevpref`, and a user debugging "why is the music coming out of the wrong thing" is far
better served by a setting that says plainly which device exists.

### Which OPL

The FM modes use a **private** `DBOPL::Handler` and their own `VBEAIFM` mixer channel, not the
chip `[sblaster] oplmode` drives. Sharing would have been more faithful to a real 1994 machine,
which had exactly one OPL and in which `OPL2.COM` programmed it directly. It was rejected
because it makes `midimode = opl2` silently produce nothing whenever `oplmode = none`, and
because a game's own Adlib writes and the VBE/AI stream would then fight over the same
registers. A private chip also preserves the provider's "claims no hardware" property.

### Device personality

An interpreting driver is not a transmitter, so in FM mode:

- `mifeatures` advertises `MIDIFPRELD` only — **not** `MIDIFXMITR`.
- `michip` reads `Yamaha OPL2` / `Yamaha OPL3`.
- `miactivetones` and `MIDITONES` report real numbers: the total voice count, and the count
  currently free. The spec lets a transmitter answer `0xFFFF` to both because it cannot know;
  a synthesiser can.
- `MIDIVOICESTEAL` is honoured per channel rather than being a stub. With stealing disabled on
  a channel and no free voice, the note is dropped instead.
- `MIDIPATCHTYPE` answers true for `0x10` (OPL2), and for `0x11` (OPL3) in `opl3` mode.

### Patches

`msPreLoadPatch` installs a patch for one GM program. The OPL2 form (§7.2.1) arrives as
thirteen one-byte fields per operator, which are packed back into the five registers the chip
wants. One subtlety: the spec's `opl2fm` field is 1 for *frequency* modulation, while the
chip's connection bit is 1 for *additive*, so it inverts. The OPL3 form (§7.2.2) is raw
four-operator register images; since the provider runs two-operator voices, the first operator
pair is taken. `msUnloadPatch` restores the built-in.

**The built-in bank is original and deliberately plain**, and `midibank` below exists to
replace it: one rough voice per General MIDI
family, plus six percussion voices for channel 10. The obvious candidate for a real bank — The
Fat Man's `FATV10.BNK`, which ships with the VESA SDK — turns out **not to be redistributable**.
Its `TERMS` file requires a per-product licence fee, an on-screen credit, and a copy of the
finished product sent to Fat Labs, none of which is compatible with bundling into DOSBox-X. The
alternative to writing originals was to ship nothing and force every application down the
patch-library path, which would have made `midimode = opl2` useless out of the box.

Pitch comes from an F-number table computed directly from `fnum = freq * 2^(20-block) / 49716`,
which is within 0.03% of equal temperament. The table most period drivers used is a systematic
9 cents flat; there was no reason to reproduce that.

### Loading a real bank: `midibank`

`[vbeai] midibank` points the FM modes at an instrument bank file, because the built-in
timbres are a stopgap and should be treated as one. Two formats are understood, told apart by
signature rather than extension:

| Format | Signature | Layout |
| --- | --- | --- |
| DMX `GENMIDI` | `#OPL_II#` at 0 | 175 instruments of 36 bytes — 128 melodic then 47 percussion for notes 35–81 — followed by 175 names of 32 bytes. Each instrument holds two voices; the first is used, since this synthesiser runs single two-operator voices. |
| Ad Lib `.BNK` | `ADLIB-` at 2 | Header, name index, then 30-byte timbres. The timbre is field for field the VBE/AI OPL2 patch minus its leading type word, so the same packing applies. Names follow the SDK convention: `AM000`–`AM127` melodic, `APO035`… percussion by GM note. |

A bank that cannot be read is reported and ignored, leaving the built-in instruments in place.
Malformed input is expected here — these files come from the user — so the loader checks the
signature, the length, and every offset it follows, and the host check feeds it a bad
signature, a truncated file and a missing file to prove the built-in bank survives each.

The GENMIDI `base_note_offset` becomes the patch transpose; the fixed-pitch flag and note
become the percussion patch's fixed note. Waveforms are masked to what the chip supports —
two bits on OPL2, three on OPL3 — since banks written for an OPL3 do use the wider set.

### How the built-in bank measures up

Loading each bank through the same loader and counting how many genuinely distinct timbres
come out the other side:

| Bank | Melodic, of 128 | Percussion, of 47 |
| --- | --- | --- |
| Built-in | **16** (12%) | **6** (13%) |
| Freedoom `genmidi.lmp` | **128** (100%) | **47** (100%) |
| Fat Man `FATV10.BNK` | 127 (99%) | 27 (57%) |

The built-in bank maps eight consecutive GM programs onto one family voice, so a piece that
changes instrument inside a family hears no change at all. That is the cost of writing sixteen
voices by hand instead of a hundred and seventy-five, and it is the single strongest reason to
point `midibank` at something better.

Rendering the same file through OPL3 and measuring the captured mixer output:

| | Built-in | Freedoom |
| --- | --- | --- |
| RMS level | 2813 | 1439 |
| Spectral centroid | 1428 Hz | 990 Hz |
| Dynamic variation (σ/mean of the envelope) | 0.13 | **0.25** |

Freedoom's is quieter, considerably darker, and has roughly twice the dynamic movement — its
notes decay and breathe where the built-in voices sit at a more uniform level, which is the
same over-sustaining that made the envelope look suspiciously flat during verification.

**Freedoom's bank is the one to use.** Its provenance is clean: the instruments derive from
OpenBSD's kernel and the project requires original content, with the build scripts under
BSD-3-Clause. Its own README is candid that the set "isn't so good" compared with Doom's
proprietary one — but it is comprehensively better than sixteen hand-written voices, and it is
genuinely free, which `FATV10.BNK` is not.

### Verification

The host check drives the synthesiser through the stub `DBOPL::Handler`, recording every
register write: enumeration under each mode, the mode-dependent personality, note on/off,
running status, voice exhaustion and stealing, `MIDIVOICESTEAL`, both patch formats field by
field, the OPL3 second register bank, and pitch accuracy decoded back out of the F-number and
block actually written.

End to end, `SAKURA2A.MID` plays through `midimode = opl2` with `[midi] mididevice = none`,
proving the FM device needs nothing from `[midi]`, and the captured mixer output is tonal with
a shifting dominant pitch.

#### A bug this found

Feeding the file's real byte stream through the synthesiser on the host and comparing voice
usage against an independent count of the music's polyphony exposed a genuine fault. The piece
peaks at 8 keys held, or **14** once the sustain pedal is accounted for — but the synthesiser
was using every voice it had, 9 of 9 and 18 of 18.

The cause: re-striking a key while the pedal is down went through the normal note-off path,
which *sustains* the old voice rather than freeing it, and then allocated a second voice for
the same pitch. Under a heavily pedalled piece — and `SAKURA2A.MID` presses the pedal 85 times,
the only controller it uses at all — those accumulate until the chip is starved and starts
stealing notes that should still be sounding. A re-strike now hard-releases the old voice
first, after which OPL3 peaks at exactly 14 of 18, matching the computed demand.

Worth noting how it presented: nothing crashed, nothing hung, no note was stuck at the end, and
every byte was accounted for. It was audible only as music that was subtly wrong.
