#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_SURFACE_H
#define DOSBOX_OUTPUT_SURFACE_H

void OUTPUT_SURFACE_Select();
Bitu OUTPUT_SURFACE_GetBestMode(Bitu flags);
Bitu OUTPUT_SURFACE_SetSize();
bool OUTPUT_SURFACE_StartUpdate(Bit8u* &pixels, Bitu &pitch);
void OUTPUT_SURFACE_EndUpdate(const Bit16u *changedLines);

#endif /*DOSBOX_OUTPUT_SURFACE_H*/