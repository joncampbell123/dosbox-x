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

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	Retrowave_Serial_Transfer_End = 0,
	Retrowave_Serial_Transfer_Start = 1,
} RetroWaveProtocol_Serial_ControlFlags;

typedef struct {
	uint8_t data : 7;
	uint8_t is_data : 1;
} __attribute__((__packed__, gcc_struct)) RetroWaveProtocol_Serial_Byte;

extern uint32_t retrowave_protocol_serial_packed_length(uint32_t len_in);
extern uint32_t retrowave_protocol_serial_pack(const void *_buf_in, uint32_t len_in, void *_buf_out);

#ifdef __cplusplus
};
#endif