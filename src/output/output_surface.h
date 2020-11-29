#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_SURFACE_H
#define DOSBOX_OUTPUT_SURFACE_H

// output API
void OUTPUT_SURFACE_Initialize();
void OUTPUT_SURFACE_Select();
Bitu OUTPUT_SURFACE_GetBestMode(Bitu flags);
Bitu OUTPUT_SURFACE_SetSize();
bool OUTPUT_SURFACE_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_SURFACE_EndUpdate(const uint16_t *changedLines);
void OUTPUT_SURFACE_Shutdown();

#endif /*DOSBOX_OUTPUT_SURFACE_H*/