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

#include "OPL3.h"

static const int transfer_speed = (const int)2e6;

void retrowave_opl3_queue_port0(RetroWaveContext *ctx, uint8_t reg, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_OPL3, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	ctx->cmd_buffer_used += 6;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0xe1;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = reg;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xe3;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xfb;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = val;
}

void retrowave_opl3_queue_port1(RetroWaveContext *ctx, uint8_t reg, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_OPL3, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	ctx->cmd_buffer_used += 6;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0xe5;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = reg;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xe7;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xfb;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = val;
}

void retrowave_opl3_emit_port0(RetroWaveContext *ctx, uint8_t reg, uint8_t val) {
	uint8_t buf[] = {RetroWave_Board_OPL3, 0x12, 0xe1, reg, 0xe3, val, 0xfb, val};
	ctx->callback_io(ctx->user_data, transfer_speed, buf, NULL, sizeof(buf));
}

void retrowave_opl3_emit_port1(RetroWaveContext *ctx, uint8_t reg, uint8_t val) {
	uint8_t buf[] = {RetroWave_Board_OPL3, 0x12, 0xe5, reg, 0xe7, val, 0xfb, val};
	ctx->callback_io(ctx->user_data, transfer_speed, buf, NULL, sizeof(buf));
}

void retrowave_opl3_reset(RetroWaveContext *ctx) {
	uint8_t buf[] = {RetroWave_Board_OPL3, 0x12, 0xfe};
	ctx->callback_io(ctx->user_data, transfer_speed / 10, buf, NULL, sizeof(buf));
	buf[2] = 0xff;
	ctx->callback_io(ctx->user_data, transfer_speed / 10, buf, NULL, sizeof(buf));
}

void retrowave_opl3_mute(RetroWaveContext *ctx) {
	for (uint8_t i = 0x20; i <= 0xF5; i++) {
		retrowave_opl3_emit_port0(ctx, i, i >= 0x40 && i <= 0x55 ? 0xFF : 0x00);
		retrowave_opl3_emit_port1(ctx, i, i >= 0x40 && i <= 0x55 ? 0xFF : 0x00);
	}
}
