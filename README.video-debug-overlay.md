
**The video debug overlay: How to guide**

## What is the Video Debug Overlay?
The video debug overlay is an extension to the video output of DOSBox-X that puts VGA state information on the bottom and right sides of the DOS screen. The information updates constantly so that state changes are immediately visible.

## Why do the scalers and video mode affect it?
The video debug overlay is rendered as part of the DOS screen so that, when enabled, it appears in your screen captures and video recordings as well, making it easy to provide screenshots for video debugging purposes, and a video recording where it is possible to analyze the information frame-by-frame in a video editor.

## What is provided in the video debug overlay?
The exact information provided depends entirely on what video hardware is being emulated. Even when registers and state are common across hardware, exact state varies from video type to video type.

## What is all the space on the right side of the overlay?
That information is used to show at least two things. First, the video palette on a scanline by scanline basis so it is easy to tell when per-scanline palette effects and/or "copper bars" are in effect during active display. The second purpose is to show tags per scanline of other state changes. When emulating EGA/VGA for example, tags indicating line compare (split screen) or changes to the offset register (old demoscene effect to make the screen "waver" for example) may be shown when the guest code uses them.

## What are examples of per-scanline palette effects?
1. Lemmings: Uses 16-color EGA mode, but uses a color palette change between the game board above and the controls below. At the initial screen of a level, uses a palette change between the "visual" of the level at the top and the descriptive text below.
2. Tony and Friends in Kellogs Land: Mid-screen palette change to draw "water" level.

