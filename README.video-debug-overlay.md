
**The video debug overlay: How to guide**

## What is the Video Debug Overlay?
The video debug overlay is an extension to the video output of DOSBox-X that puts VGA state information on the bottom and right sides of the DOS screen. The information updates constantly so that state changes are immediately visible.

## Why do the scalers and video mode affect it?
The video debug overlay is rendered as part of the DOS screen so that, when enabled, it appears in your screen captures and video recordings as well. This makes it easy to provide screenshots for video debugging purposes, and to analyze the information frame-by-frame in a video player or editor.

## What is provided in the video debug overlay?
The exact information provided depends entirely on what video hardware is being emulated. Even when registers and state are common across hardware, exact state varies from video type to video type.

## What is all the space on the right side of the overlay?
That information is used to show at least two things. First, the video palette on a scanline by scanline basis so it is easy to tell when per-scanline palette effects and/or "copper bars" are in effect during active display. The second purpose is to show tags per scanline of other state changes. When emulating EGA/VGA for example, tags indicating line compare (split screen) or changes to the offset register (old demoscene effect to make the screen "waver" for example) may be shown when the guest code uses them.

## What are examples of per-scanline palette effects?
1. Lemmings: Uses 16-color EGA mode, but uses a color palette change between the game board above and the controls below. At the initial screen of a level, uses a palette change between the "visual" of the level at the top and the descriptive text below.
2. Tony and Friends in Kellogs Land: Mid-screen palette change to draw "water" level.

## Common mode information (at the top)
Directly below the DOS screen, the video mode is shown. The first, in green where possible, is the display mode as an M_ constant used in the DOSBox/DOSBox-X source code.

Then, the source video resolution following T (text) or G (graphics), and then ">" followed by the video resolution rasterized to the VGA output (active display area only). The height of the video resolution for depends on the video mode and hardware. VGA emulation will always rasterize video output to a height of 400 or 480 for standard VGA modes unless changed by the guest.

Then, an at (@) sign followed by the video start address in hexadecimal. This reflects the Start Address register including the extended value beyond standard VGA registers in SVGA emulation. This is followed by "+" and then the Offset register, a value responsible for telling the video hardware how far to skip forward after every character row. The value usually represents character columns of video memory, not bytes. For example in text mode where character and attribute data is paired into 16-bit values, this value represents the number of text character cells, and therefore bytes * 2, per character row.

For EGA/VGA emulation, the offset register value is followed by -B, -W, or -D to indicate whether the CRTC is in byte, word, or DWORD mode. This affects how how far each "step" through video memory goes per character column. For 256-color mode, an additional "ch4" is appended to indicate normal "chained" 256-color more, or "uch" to mean unchained 256-color mode as commonly used by DOS games (byte mode). Nothing is shown for any other case. See common documentation including FreeVGA for more details about CRTC byte/word/dword mode. EGA does not support dword mode.

VGA emulation may show "SMZX" to indicate "Super MegaZeux" 256-color text mode.

This format may change as needed for some machine types.

For EGA emulation, the mode information will show "mono", "16c", or "64c" to indicate the mode of the EGA monitor. All EGA modes other than 640x350 use 16c, CGA-style RGBI compatible output, 640x350 uses 64c, which uses all 6 pins to tranmit R, G, and B as two-bit values (4 levels) to select 16 colors from a palette of 64 possible colors.

# M_ video modes
| M_*               | What                                                                                                              |
| ----------------- | ----------------------------------------------------------------------------------------------------------------- |
| M_CGA2            | 640x200 2-color CGA graphics mode. EGA/VGA emulation will never show this value.                                  |
| M_CGA4            | 320x200 4-color CGA graphics mode. EGA/VGA emulation will never show this value.                                  |
| M_EGA             | EGA/VGA 16-color planar graphics mode. Not SVGA modes.                                                            |
| M_EGA (CGA4)      | EGA/VGA emulation of CGA 320x200 4-color graphics mode.                                                           |
| M_EGA (CGA2)      | EGA/VGA emulation of CGA 640x200 2-color graphics mode.                                                           |
| M_VGA             | VGA 256-color graphics mode. Not SVGA modes.                                                                      |
| M_LIN4            | SVGA 16-color planar graphics mode. This usually means the 256K memory wraparound is disabled.                    |
| M_LIN8            | SVGA 256-color graphics mode. This usually means the 256K memory wraparound is disabled.                          |
| M_LIN15           | Highcolor 15bpp graphics mode. 16 bits per pixel, only the low 15 bits are used for 5-bit R/G/B.                  |
| M_LIN16           | Highcolor 16bpp graphics mode. 16 bits per pixel. 5-bit Red and Blue, 6-bit Green.                                |
| M_LIN24           | Truecolor 24bpp graphics mode. 24 bits per pixel. 8-bit R/G/B. Once used in the 1990s, no longer common since.    |
| M_LIN32           | Truecolor 32bpp graphics mode. 32 bits per pixel. 8-bit R/G/B/X. X is ignored. Very common since the late 1990s.  |
| M_TEXT            | Alphanumeric text mode.                                                                                           |
| M_HERC_GFX        | Hercules graphics mode.                                                                                           |
| M_HERC_TEXT       | Hercules/MDA text mode.                                                                                           |
| M_CGA16           | CGA graphics mode with composite video emulation.                                                                 |
| M_TANDY2          | Tandy/PCjr 2-color graphics mode, monochrome.                                                                     |
| M_TANDY4          | Tandy/PCjr 4-color graphics mode.                                                                                 |
| M_TANDY16         | Tandy/PCjr 16-color graphics mode.                                                                                |
| M_TANDY_TEXT      | Tandy/PCjr text mode.                                                                                             |
| M_AMSTRAD         | Amstrad-specific graphics mode.                                                                                   |
| M_PC98            | Video mode used by NEC PC-98 emulation.                                                                           |
| M_FM_TOWNS        | Placeholder mode (stub) for future development.                                                                   |
| M_PACKED4         | SVGA packed 4bpp mode (not planar). Somewhat common in the mid 1990s on some SVGA chipsets, not seen since.       |
| M_DCGA            | Olivetti M24 DCGA.                                                                                                |

# common descriptions for reference
| Mode                               | Description                                                                                         |
| ---------------------------------- | --------------------------------------------------------------------------------------------------- |
| VGA 80x25 text                     | M_TEXT T80x25>720x400 @00000+050-W                                                                  |
| VGA 80x25 text (8-pixel)           | M_TEXT T80x25>640x400 @00000+050-W                                                                  |
| VGA 40x25 text                     | M_TEXT T40x25>360x400 @00000+028-W                                                                  |
| CGA 320x200 4-color                | M_EGA (CGA4) G320x200>320x400 @00000+028-B                                                          |
| CGA 640x200 2-color                | M_EGA (CGA2) G640x200>640x400 @00000+050-B                                                          |
| EGA 320x200 16-color               | M_EGA G320x200>320x400 @00000+028-B                                                                 |
| EGA 640x200 16-color               | M_EGA G640x200>640x400 @00000+050-B                                                                 |
| EGA 640x350 monochrome             | M_EGA G640x350>640x350 @00000+050-B (shows 8 colors only)                                           |
| EGA 640x350 color                  | M_EGA G640x350>640x350 @00000+050-B                                                                 |
| MCGA 640x480 2-color               | M_EGA G640x480>640x480 @00000+050-B                                                                 |
| VGA 640x480 16-color               | M_EGA G640x480>640x480 @00000+050-B                                                                 |
| VGA 320x200 256-color              | M_VGA G320x200>320x400 @00000+050-Dch4                                                              |
| SVGA 640x400 256-color             | M_LIN8 G640x400>640x400 @00000+0A0-Dch4                                                             |
| SVGA 640x480 256-color             | M_LIN8 G640x480>640x480 @00000+0A0-Dch4                                                             |
| SVGA 320x200 15bpp                 | M_LIN15 G320x200>320x400 @00000+0A0-D                                                               |
| SVGA 132x43 text                   | M_TEXT T132x43>1056x688 @00000+084-W                                                                |

## VGA debug status
# Bottom of the screen: palette
RPAL shows the exact contents of the VGA hardware palette, as would be seen through I/O ports 3C7h-3C9h.

EPAL shows the palette in effect for the display mode. The number of colors shown is 256 if 256-color mode. Otherwise it is 16 colors. In CGA/EGA/VGA graphics modes other than 256-color, less than 16 colors may be displayed according to the Color Plane Enable register.

No palette is shown for highcolor/truecolor (15/16/24/32bpp) graphics modes.

A 6 or 8 is shown after RPAL and EPAL to indicate DAC width, the number of bits used to represent the R/G/B values sent to the DAC and out the VGA connector when a color palette is involved. Standard VGA and early SVGA cards use a 6-bit DAC width. Later SVGA hardware allows switching between 6 and 8 bits per RGB. There is a VESA BIOS call to set DAC width, which DOSBox-X does emulate for S3 hardware.

ACPAL shows the color palette according to the Attribute Controller. In modes other than 256-color mode, this is used to map pixels rendered from video memory to the VGA palette. In 256-color mode, but not SVGA, this has an effect on the pixels as if the 8-bit pixel were split into two 4-bit parts and remapped, except for Tseng ET4000 which remaps only partially due to a hardware bug.

The original purpose of the Attribute Controller palette on EGA hardware was to determine how to convert 16-color data to the 6-bit TTL video output on the back of the card. VGA hardware retains this EGA behavior for backwards compatibility, but instead uses it as a 4-bit to 6-bit translation in the process of determining what to look up in the color palette. The BIOS default VGA palette is set up to produce the same colors on the monitor if running a DOS program written originally for the EGA.

CSPAL shows the final palette mapping after other registers including Color Plane Enable, Color Select and DAC PEL mask are considered after the attribute controller palette. This determines the final color map sent to the display.

# Bottom of the screen: VGA register state
CPE*x*: Color Plane Enable register. *x* is a hexadecimal digit of the 4-bit value that determines which bitplanes are "enabled" and sent to the display. Applies only to 16-color planar graphics modes. CGA and MCGA graphics modes on EGA/VGA are just variations of 16-color planar graphics with bitplanes disabled to reduce the color depth and other register changes for backwards compatibility. CGA modes use the Mode Control register to direct the CRTC to emulate the two-way interleave of the original hardware. No known effect on text, 256-color mode, and SVGA modes.

HPL*x*: Horizontal PEL panning. *x* is a hexadecimal digit. This register allows for smooth horizontal panning in VGA modes including 256-color and SVGA modes. If a DOS program is horizontally panning this value will constantly change.

YP*xx*: Y-panning, according to the Preset Row Scan Register. *xx* is two hexadecimal digits. In text modes, as well as graphics modes with a nonzero Maxmimum Scan Line Register, this allows smooth vertical panning when combined with changing the Start Address registers. If a DOS program is vertically panning this value may constantly change.

PM*xx*: PEL mask, *xx* is two hexadecimal digits. Before the 8-bit color value is sent to the DAC, it is masked by this value. It is normally FF.

MD*xx*: Attribute controller mode control register. *xx* is two hexadecimal digits. This register is responsible for controlling whether graphics or text mode is active, whether character clocks are 8 or 9 pixels, whether text mode bit 7 attribute bytes blink or not, whether line compare resets horizontal panning, and is part of the mechanism to enable 256-color mode. See online documentation like FreeVGA for more details.

CS*xx*: Attribute controller color select. *xx* is two hexadecimal digits. Does not affect 256-color mode, except for Tseng ET4000 due to hardware bug. Can affect bits 4-5 of the color index out of the Attribute Controller, and can provide bits 6-7 for the color index after the 4-to-6 conversion through the Attribute Controller. Though not often used, it may be used for color palette changes and demo effects.

# On the right: palette per scanline
The color palette rendered per scanline on the right is based on CSPAL in case of per-scanline changes to do palette tricks or demoscene tricks, such as showing more than 16 colors in a 16-color mode.

## EGA debug status
# Bottom of the screen: palette
ACPAL shows the color palette according to the Attribute Controller. On EGA hardware this determines how to map the 16 colors to the 6-bit TTL video connector on the back.

MDPAL shows the Attribute Controller color palette according to the Color Plane Enable register. If some bitplanes are hidden by the CPE register, this palette will differ from ACPAL.

OVC shows the current overscan border register. YPN shows the Row Start register which is used for vertical panning. CPE shows the contents of the Color Plane Enable register. HPEL shows the horizontal PEL panning register.

# On the right: palette per scanline
The color palette rendered per scanline on the right is based on MDPAL, rendered per scanline. Next to MDPAL is CPE, 4 pixels indicating visually, per scanline, which bitplanes are enabled on the display. Some demo effects may change MDPAL and CSPAL mid-frame.

## On the right: Hardware change event tags
If a change or event that is notable occurs on a scanline, a tag will appear with the top right on that scanline noting that event.

| event                                                                 | tag                     |
| --------------------------------------------------------------------- | ----------------------- |
| EGA/VGA line compare match (split screen)                             | SPLIT LNCMP             |
| Change of Maximum Scan Line register                                  | MXS                     |
| Change of Offset register                                             | OFS                     |
| Change of horizontal retrace                                          | HRT                     |
| Change of color plane enable                                          | CPE                     |
| Change of color select                                                | CSL                     |
| Start of PC-98 display partition *n* (text)                           | TPART*n*                |
| Start of PC-98 display partition *n* (graphics)                       | GPART*n*                |

