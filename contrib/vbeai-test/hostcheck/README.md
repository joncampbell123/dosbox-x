# hostcheck — host-side white-box check of the VBE/AI provider

Compiles `src/ints/int10_vesa_ai.cpp` against a set of stub headers that fake
DOSBox-X's guest memory, register file, callback allocator and mixer, then
drives the provider directly and asserts on the results.

```bash
sh ./run.sh          # CXX=... to override the compiler
```

Runs on the host in under a second, needs no emulator, and checks the parts
that are otherwise only observable by ear:

- the ten-byte ROM trampolines — `MOV AX,index` / callback / `RETF imm16` —
  and that every `RETF` operand equals that function's Pascal argument count;
- `INT 10h AX=4F13h` subfunctions 0–4, including the `SI:DI` vs `SI:CX`
  return asymmetry between query and open;
- byte offsets of `GeneralDeviceClass`, `WAVEInfo` and `WAVEService`;
- Pascal argument decoding, i.e. that the last declared parameter is read
  from `SP+4` and the list runs upward in reverse;
- playback stepping through the mixer, sample for sample, against the source
  bytes in fake guest memory;
- **the completion-callback frame**: that `wsTimerTick` leaves the guest stack
  holding exactly the frame `wsApplPSyncCB(int, void far *, long, long)`
  expects, with the sync-return stub as its return address, and that the
  application's `RETF 14` lands back on the original caller's `CS:IP`.

That last one is the least obvious part of the implementation and the one most
likely to fail silently — a wrong frame shows up as a hang or a crash in the
guest, not as an error. The check pins it numerically.

The stub headers deliberately mirror the real DOSBox-X signatures
(`MixerChannel::AddSamples_*`, `CPU_Push16`'s `cpu.stack.mask` arithmetic,
`SegPhys`, `real_readd`); if any of those change upstream this will stop
compiling, which is the intended signal.
