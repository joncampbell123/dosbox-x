#ifndef DOSBOX_OUTPUT_TOOLS_H
#define DOSBOX_OUTPUT_TOOLS_H

#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "sdlmain.h"

// common headers and static routines reused in different outputs go there

static inline int int_log2(int val)
{
    int log = 0;
    while ((val >>= 1) != 0) log++;
    return log;
}

template <class WH>
inline void aspectCorrectExtend(volatile WH &width, volatile WH &height)
{
    if (width * sdl.srcAspect.y != height * sdl.srcAspect.x)
    {
        // abnormal aspect ratio detected, apply correction
        if (width * sdl.srcAspect.y > height * sdl.srcAspect.x)
        {
            // wide pixel ratio, height should be extended to fit
            height = (WH)floor((double)width * sdl.srcAspect.yToX + 0.5);
        }
        else
        {
            // long pixel ratio, width should be extended
            width = (WH)floor((double)height * sdl.srcAspect.xToY + 0.5);
        }
    }
}

template <class WH, class XY, class TWH>
inline void aspectCorrectFitClip(volatile WH &clipW, volatile WH &clipH, volatile XY &clipX, volatile XY &clipY, const TWH fullW, const TWH fullH)
{
    XY ax, ay;
    WH sh = fullH;
    WH sw = (WH)floor((double)fullH * sdl.srcAspect.xToY);

    if (sw > fullW) {
        sh = (WH)floor(((double)sh * fullW) / sw);
        sw = fullW;
    }

    ax = (XY)floor((fullW - sw) / 2);
    ay = (XY)floor((fullH - sh) / 2);
    if (ax < 0) ax = 0;
    if (ay < 0) ay = 0;

    clipX = ax; clipY = ay; clipW = sw; clipH = sh;

//    assert((sdl.clip.x + sdl.clip.w) <= sdl.desktop.full.width);
//    assert((sdl.clip.y + sdl.clip.h) <= sdl.desktop.full.height);
}

#endif
