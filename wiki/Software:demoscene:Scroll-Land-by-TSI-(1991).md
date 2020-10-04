<img src="images/Demoscene:Scroll-Land-by-TSI-(1991).gif" width="640" height="400"><br>
[[1991|Guide:MS‐DOS:demoscene:1991]] demoscene entry.

# Demo description

A demo that plays music in a loop with cue points combined with color palette cycling effects to scroll text messages. If music is playing, additional color palette work is used to flash white text to the beat of the music.

# Recommended DOSBox-X configuration

    [dosbox]
    memsize=1

    [cpu]
    core=normal
    cputype=386
    cycles=4000

    [sblaster]
    sbtype=sbpro2
    sbbase=220
    irq=3
    dma=1

# Sound Blaster support oddity

This demo appears to use undocumented [[Sound Blaster DSP commands|Hardware:Sound-Blaster:DSP-commands]] [[0xE2 (DMA identification)|Hardware:Sound-Blaster:DSP-commands:0xE2]], [[0xE4 (write test register)|Hardware:Sound-Blaster:DSP-commands:0xE4]], and [[0xE8 (read test register)|Hardware:Sound-Blaster:DSP-commands:0xE8]] in addition to [[0xE0 (SB2.0 identification)|Hardware:Sound-Blaster:DSP-commands:0xE0]] and [[0xE1 (Get DSP version)|Hardware:Sound-Blaster:DSP-commands:0xE1]].

For reasons unknown at this time, it also appears to assume the Sound Blaster is on IRQ 3.

# Problems

This demo appears to have a problem exiting back to DOS. Hitting space only causes the demo to freeze (but the music keeps going).

# More information

[More information (Pouet)](http://www.pouet.net/prod.php?which=5261)
