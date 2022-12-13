#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_GAMELINK_H
#define DOSBOX_OUTPUT_GAMELINK_H

#if C_GAMELINK
#include "../gamelink/gamelink.h"
#endif // C_GAMELINK

// output API
void OUTPUT_GAMELINK_Initialize();
void OUTPUT_GAMELINK_Select();
Bitu OUTPUT_GAMELINK_GetBestMode(Bitu flags);
Bitu OUTPUT_GAMELINK_SetSize();
bool OUTPUT_GAMELINK_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_GAMELINK_EndUpdate(const uint16_t *changedLines);
void OUTPUT_GAMELINK_Shutdown();

// specific additions
void OUTPUT_GAMELINK_InputEvent();
void OUTPUT_GAMELINK_Transfer();
void OUTPUT_GAMELINK_TrackingMode();

#endif /*DOSBOX_OUTPUT_GAMELINK_H*/