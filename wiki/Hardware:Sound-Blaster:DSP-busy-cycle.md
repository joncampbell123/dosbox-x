# Explanation

Creative Sound Blaster cards have an undocumented quirk where the DSP busy bit toggles by itself at an unknown clock rate whether or not any I/O is occuring with the card. On 486/Pentium hardware this quirk can be detected with 16 I/O reads from the DSP status port where 8 will have the busy bit set, and 8 will have the busy bit clear.

# Scope

On Sound Blaster 16 cards, this DSP busy cycle is always running.

On Sound Blaster and Sound Blaster Pro cards, the DSP busy cycle is active when DSP playback or recording is active.

Sound Blaster clones may emulate the busy cycle by toggling the busy bit (port 22Ch) on I/O read (example: Gravis Ultrasound SBOS/MEGA-EM).

# Side effects of the busy cycle
## [[Crystal Dream by Triton|Demoscene:Crystal-Dream-by-Triton-(1992)]] (1992)

[[Crystal Dream|Demoscene:Crystal-Dream-by-Triton-(1992)]] appears to use the DSP busy cycle to detect when the DSP has finished the block of audio. If it sees the busy cycle stop, then it issues DSP command 0x14 to start another DSP block. If the hardware does not have a busy cycle (SB clones), then the demo effectively [[nags|Hardware:Sound-Blaster:Nagging-the-DSP]] the DSP to keep playing continuously and audio playback works regardless. This unique method allows the demo to keep audio playback going without having to detect or worry about which IRQ the Sound Blaster is assigned to.

If the busy cycle never stops (Sound Blaster 16 case), then the demo never issues command 0x14 and music eventually stops.

## Direct DAC based playback

This can affect some games or programs that use the direct DAC (DSP command 0x10) method of playback from IRQ 0 if the game reads the DSP status first to determine if it's safe to do so. If the DSP periodically reports itself busy the timer interrupt will not be able to sustain the sample rate and will play audio slowly. At one point, the quirk [affected the DOSLIB sound blaster test program](http://youtu.be/9rCV4B4ylOs?t=4m50s) in a bad way.

A workaround for direct DAC playback is to read the DSP status port up to 8 times to wait for the busy bit to clear if the card reports DSP 4.xx.

