# DOSBox-X CPU emulation TODO

Remaining problems found by the CPU tests in doslib-ai `testing/cpu/ins`
(8086.EXE, 286.EXE, 286P.EXE, 386.EXE). The segment limit checks are in
`src/cpu/core_normal/seglimit.h` (normal and 286 cores), `src/cpu/core_full.cpp`
and `src/cpu/core_dyn_x86/decoder.h`. The simple core skips limit checks by
design, to stay fast.

## Segment protection

- Access through a null selector (DS/ES/FS/GS = 0 in protected mode) should
  raise #GP(0). 286P.EXE: "PM limits: access through null DS".
- The upper bound of an expand-down data segment with B=0 is FFFFh, not
  FFFFFFFFh. This needs the B bit of each segment in `Segs`. The KVM core
  syncs `Segs`, so coordinate the change with it. 286P.EXE: "expand-down mov
  ax,[FFFF]".
- Type checks on memory accesses: writes through a read-only data segment or
  a code segment, and reads through an execute-only code segment, should
  raise #GP(0) (#SS for SS). 286P.EXE: "PM selectors: write through code DS",
  "write through read-only DS".
- Set the accessed bit in the descriptor when a segment register is loaded
  (GDT and LDT entries). 286P.EXE: "PM entry: DLIM accessed bit", "PM
  selectors: accessed bits".
- Pushes done while an exception or interrupt is delivered, and during far
  CALL/RET/IRET in cpu.cpp, are not limit-checked. 286P.EXE: "Ring 3: pop
  past SS limit" gives #DF (08h) instead of #SS (0Ch) on the normal and full
  cores.
- The dynamic core does not check the stack limit on PUSH/POP. 286P.EXE:
  "Ring 3: push, expand-down SS" and "pop past SS limit" give #GP instead of
  #SS.
- The 286 core's string instructions still use the FFFFh "no check" limit
  value.
- The dynamic core's limit check exists only in the x86-64 code generator;
  the 32-bit x86 code generator has none.

## Instructions

- With cputype=8086 (no count masking), SHR r8/r16 by CL=33 gives the wrong
  flags (`src/cpu/flags.cpp`, lazy flags for large shift counts). 8086.EXE:
  30 SHR failures.
- Full core, cputype=386, 286.EXE: `MOV CS,AX` (8E C8) does not raise #UD,
  and MOVSW from [FFFF] does not raise #GP in real mode (string instructions
  are not limit-checked).
- cputype=286, 286.EXE: SMSW should return 1 in MSW bits 4-15 on a 286
  (DOSBox-X returns 0).
- cputype=286, 286.EXE: "Seg wrap: movsw from [FFFF]" reads the second byte
  from offset 10000h instead of wrapping to offset 0000h.
