#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_DIRECT3D_H
#define DOSBOX_OUTPUT_DIRECT3D_H

#if (HAVE_D3D9_H) && defined(WIN32)

#include "direct3d/direct3d.h"
extern CDirect3D* d3d;

// output API
void OUTPUT_DIRECT3D_Initialize();
void OUTPUT_DIRECT3D_Select();
Bitu OUTPUT_DIRECT3D_GetBestMode(Bitu flags);

#endif /*(HAVE_D3D9_H) && defined(WIN32)*/

#endif /*DOSBOX_OUTPUT_DIRECT3D_H*/