# Windows 3.1 framebuffer publication

This optional integration-device extension lets a cooperating Windows 3.1
display driver submit a complete 8-bit framebuffer without exposing its
intermediate redraws during VGA scanout. It is inactive until a guest client
explicitly submits a frame. It does not alter the emulated VGA programming model.

## Registers and transport

Use the existing DOSBox integration device at index port 0x28, data port 0x29
and command/status port 0x2a. Select a register with a DWORD index write; use
DWORD data accesses. Preserve the device latches with PUSH_STATE (command
0x20) and POP_STATE (0x21). A failed push must not pop another client's state.

| Register | Operation | Value |
| --- | --- | --- |
| 0x00684582 | Read capability | 0x57333101 when supported |
| 0x00684583 | Write submission | Guest linear address of descriptor |
| 0x00684583 | Read result | Status of the preceding submission |
| 0x00684584 | Write zero | Cancel pending publication; result is zero |
| 0x00684584 | Read result | Retains the cancellation result |

A result read does not submit or cancel again. Capability discovery does not
guarantee that the current mode or caller context is supported.

## Descriptor

The descriptor is ten little-endian DWORDs, in this order:

| DWORD | Meaning |
| --- | --- |
| 0 | Descriptor size, 40 |
| 1 | Protocol version, 1 |
| 2 | Guest linear address of the back buffer |
| 3 | Reserved, zero |
| 4 | Guest linear address of the VBE driver cursor structure |
| 5 | Framebuffer size in bytes |
| 6 | Stride in bytes |
| 7 | Width in pixels |
| 8 | Height in pixels |
| 9 | Bits per pixel, 8 |

Only protected-mode ring-zero callers outside virtual-8086 mode are accepted.
The current mode must be M_LIN8 with no resize in progress, display start,
panning or skipped bytes. Descriptor geometry must match the current VGA
width, height and stride. Width and height must be positive, width must not
exceed stride, and byte size must equal stride times height, at most 16 MiB
and at most the emulated video-memory size.

All descriptor, framebuffer and cursor ranges must fit in the 32-bit linear
address space and translate to directly backed ordinary RAM. ROM, MMIO and
unmapped pages are rejected. Readable canonical RAM wrapped by the dynamic
code cache remains acceptable. Reads occur synchronously in the submitting
caller's address space. The host retains only copied bytes, with no deferred
guest pointers or guest-memory writes.

The cursor format is the existing Windows VBE driver's shared structure:
16 bytes of metadata, a 16,384-byte mask workspace, then a flags WORD. Bit 2
of the flags indicates visibility. Metadata WORDs at offsets 4, 6, 8 and 10
contain clipped width, height, source x and source y. A DWORD at offset 12
contains the framebuffer byte offset. The first 32-by-32 mask entries are
interleaved 8-bit AND/XOR pairs, indexed by source coordinates. Composition
is `(pixel & AND) ^ XOR`. Both source and destination bounds are validated.
A hidden or entirely clipped cursor requires no composition.

## Publication and failure

| Status | Meaning |
| --- | --- |
| 0 | Snapshot accepted |
| 1 | Unsupported caller context or display mode |
| 2 | Invalid descriptor or geometry |
| 3 | Invalid memory or allocation failure |
| 4 | Invalid cursor bounds |

The most recent accepted submission replaces any pending snapshot. Acceptance
does not immediately write VRAM: after the previous render update ends, the
next VGA frame receives the complete snapshot. The frame is discarded if its
geometry or mode generation no longer matches. Explicit cancellation, failed
submission, mode changes, resize, reset and save-state load discard pending
pixels, preventing stale publication over a guest fallback or restored state.
Cancellation preserves the last committed frame. A client can cancel a pending
snapshot when drawing resumes, without reverting the displayed pixels. It must
track which submissions can be revoked so that starting another operation does
not repeatedly discard completed frames before they can be displayed.

The service does not copy pixels into the guest driver's front-buffer shadow.
A client returning to its ordinary copy path must invalidate that shadow.
Drivers must also prevent submission during an incomplete drawing operation
and preserve their existing fallback when the capability is absent or fails.

## Verification

Debug builds run descriptor, cursor, ownership and service checks with the
project's `dosbox-x -tests` command. A complete guest test additionally needs
a cooperating driver: compare large text redraws at the same resolution and
CPU budget, verify cursor clipping, exercise continuous DCI drawing, exit and
restart Windows, and change modes or load a save state while frames are pending.
