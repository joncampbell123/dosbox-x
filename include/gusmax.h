/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef DOSBOX_GUSMAX_H
#define DOSBOX_GUSMAX_H

#include "dosbox.h"

class Section;

#define GUSMAX_CODEC_OFFSET 0x10C
#define GUSMAX_CODEC_PORTS 4
#define GUSMAX_CODEC_PORT_MASK 3

#define GUSMAX_RELOC_BASE 0x30C
#define GUSMAX_RELOC_SHIFT 4
#define GUSMAX_RELOC_NIBBLE 0x0F

#define GUSMAX_CTRL_PORT_LO 0x306
#define GUSMAX_CTRL_PORT_HI 0x706
#define GUSMAX_CTRL_ENABLE 0x40
#define GUSMAX_CTRL_ENABLE_SHIFT 6
#define GUSMAX_CTRL_DMA1_16 0x10
#define GUSMAX_CTRL_DMA2_16 0x20
#define GUSMAX_DMA16_MIN 4

#define GUSMAX_REV_ID 0x0A
#define GUSMAX_OPEN_BUS 0xFF

#define GUSMAX_DEFAULT_DMA 3
#define GUSMAX_DEFAULT_IRQ 5

#define GUSMAX_MIXER_NAME "GUSMAX"

void GUSMAX_Init();
void GUSMAX_OnReset(Section *sec);
void GUSMAX_Relocate(Bitu addr);
void GUSMAX_SetPlaybackDMA(uint8_t ch);
void GUSMAX_SetDma1(uint8_t ch);
void GUSMAX_SetIRQ(uint8_t irq);
void GUSMAX_SetCodecEnable(bool on);
void GUSMAX_WriteControl(Bitu val);
Bitu GUSMAX_ReadControl();

#endif
