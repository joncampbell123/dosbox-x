#ifndef DOSBOX_OUTPUT_GAMELINK_H
#define DOSBOX_OUTPUT_GAMELINK_H

#include "config.h"

#if C_GAMELINK

#include "dosbox.h"

#include "../gamelink/gamelink.h"

// output API
//void OUTPUT_GAMELINK_Initialize();
void OUTPUT_GAMELINK_Select();
Bitu OUTPUT_GAMELINK_GetBestMode(Bitu flags);
Bitu OUTPUT_GAMELINK_SetSize();
bool OUTPUT_GAMELINK_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_GAMELINK_EndUpdate(const uint16_t *changedLines);
void OUTPUT_GAMELINK_Shutdown();

// specific additions
void OUTPUT_GAMELINK_InputEvent();
void OUTPUT_GAMELINK_Transfer();
bool OUTPUT_GAMELINK_InitTrackingMode();

#endif

#endif /*DOSBOX_OUTPUT_GAMELINK_H*/