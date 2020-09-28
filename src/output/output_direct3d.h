#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_DIRECT3D_H
#define DOSBOX_OUTPUT_DIRECT3D_H

#if C_DIRECT3D

#include "direct3d/direct3d.h"
extern CDirect3D* d3d;

// output API
void OUTPUT_DIRECT3D_Initialize();
void OUTPUT_DIRECT3D_Select();
Bitu OUTPUT_DIRECT3D_GetBestMode(Bitu flags);
Bitu OUTPUT_DIRECT3D_SetSize();
bool OUTPUT_DIRECT3D_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_DIRECT3D_EndUpdate(const Bit16u *changedLines);
void OUTPUT_DIRECT3D_Shutdown();

#endif /*C_DIRECT3D*/

#endif /*DOSBOX_OUTPUT_DIRECT3D_H*/