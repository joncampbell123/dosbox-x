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

#include "MiniBlaster.h"

static const int transfer_speed = 0.8e6;

void retrowave_miniblaster_queue(RetroWaveContext *ctx, uint8_t reg, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_MiniBlaster, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	if (reg < 0x80) {
		reg &= 0x7f;
		ctx->cmd_buffer_used += 12;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 12] = 0xfd;        // A0 = 1, CS# = 0, WR# = 1
		ctx->cmd_buffer[ctx->cmd_buffer_used - 11] = reg;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 10] = 0xf9;        // A0 = 1, CS# = 0, WR# = 0
		ctx->cmd_buffer[ctx->cmd_buffer_used - 9] = reg;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 8] = 0xff;        //
		ctx->cmd_buffer[ctx->cmd_buffer_used - 7] = val;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0xfc;        // A0 = 0, CS# = 0, WR# = 1
		ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = val;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xf8;        // A0 = 0, CS# = 0, WR# = 0
		ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xfe;        // A0 = 0
		ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = val;
	} else {
		reg &= 0x7f;
		ctx->cmd_buffer_used += 12;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 12] = 0xdf;        // A0 = 1, CS# = 0, WR# = 1
		ctx->cmd_buffer[ctx->cmd_buffer_used - 11] = reg;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 10] = 0x9f;        // A0 = 1, CS# = 0, WR# = 0
		ctx->cmd_buffer[ctx->cmd_buffer_used - 9] = reg;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 8] = 0xff;        //
		ctx->cmd_buffer[ctx->cmd_buffer_used - 7] = val;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0xcf;        // A0 = 0, CS# = 0, WR# = 1
		ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = val;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0x8f;        // A0 = 0, CS# = 0, WR# = 0
		ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
		ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xef;        // A0 = 0
		ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = val;
	}
}