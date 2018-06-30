#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include <output/output_direct3d.h>

#include "sdlmain.h"

using namespace std;

#if (HAVE_D3D9_H) && defined(WIN32)

CDirect3D* d3d = NULL;

// output API below

void OUTPUT_DIRECT3D_Initialize()
{

}

void OUTPUT_DIRECT3D_Select()
{
    sdl.desktop.want_type = SCREEN_DIRECT3D;
    render.aspectOffload = true;
}

Bitu OUTPUT_DIRECT3D_GetBestMode(Bitu flags)
{
    flags |= GFX_SCALING;
    if (GCC_UNLIKELY(d3d->bpp16))
        flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_32);
    else
        flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    return flags;
}

#endif /*(HAVE_D3D9_H) && defined(WIN32)*/