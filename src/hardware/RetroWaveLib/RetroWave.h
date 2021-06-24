/*
    This file is part of RetroWave.

    Copyright (C) 2021 ReimuNotMoe <reimu@sudomaker.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RetroWave_Board_Unknown = 0,
	RetroWave_Board_OPL3 = 0x21 << 1,
	RetroWave_Board_MiniBlaster = 0x20 << 1,
	RetroWave_Board_MasterGear = 0x24 << 1
} RetroWaveBoardType;

typedef struct {
	void *user_data;
	void (*callback_io)(void *, uint32_t, const void *, void *, uint32_t);
	uint8_t *cmd_buffer;
	uint32_t cmd_buffer_used, cmd_buffer_size;
	uint32_t transfer_speed_hint;
} RetroWaveContext;

extern void retrowave_init(RetroWaveContext *ctx);
extern void retrowave_deinit(RetroWaveContext *ctx);

extern void retrowave_io_init(RetroWaveContext *ctx);

extern void retrowave_cmd_buffer_init(RetroWaveContext *ctx, RetroWaveBoardType board_type, uint8_t first_reg);

extern void retrowave_flush(RetroWaveContext *ctx);

extern uint8_t retrowave_invert_byte(uint8_t val);

#ifdef __cplusplus
};
#endif
