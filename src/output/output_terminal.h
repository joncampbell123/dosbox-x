#ifndef DOSBOX_OUTPUT_TERMINAL_H
#define DOSBOX_OUTPUT_TERMINAL_H

#include "dosbox.h"
#include "video.h"

#include <cstdint>

void OUTPUT_TERMINAL_Select();
Bitu OUTPUT_TERMINAL_GetBestMode(Bitu flags);
Bitu OUTPUT_TERMINAL_SetSize();
bool OUTPUT_TERMINAL_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_TERMINAL_EndUpdate(const uint16_t *changedLines);
void OUTPUT_TERMINAL_Shutdown();
void OUTPUT_TERMINAL_InputEvent();

#endif /* DOSBOX_OUTPUT_TERMINAL_H */
