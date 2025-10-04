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

#include "MasterGear.h"

static const int transfer_speed = 1e6;


void retrowave_mastergear_queue_ym2413(RetroWaveContext *ctx, uint8_t reg, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_MasterGear, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	ctx->cmd_buffer_used += 12;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 12] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 11] = reg;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 10] = 0xf1;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 9] = reg;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 8] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 7] = val;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0xf9;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = val;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xf7;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = val;
}

void retrowave_mastergear_reset_ym2413(RetroWaveContext *ctx) {
	uint8_t buf[] = {RetroWave_Board_MasterGear, 0x12, 0xfe};
	ctx->callback_io(ctx->user_data, transfer_speed / 10, buf, NULL, sizeof(buf));
	buf[2] = 0xff;
	ctx->callback_io(ctx->user_data, transfer_speed / 10, buf, NULL, sizeof(buf));
}

void retrowave_mastergear_queue_sn76489(RetroWaveContext *ctx, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_MasterGear, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	ctx->cmd_buffer_used += 10;
	// Set data only
	ctx->cmd_buffer[ctx->cmd_buffer_used - 10] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 9] = val;
	// CS# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 8] = 0x5f;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 7] = val;
	// CS#+WR# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0x0f;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = val;
	// WR# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xaf;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
	// ALL off
	ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = 0x00;
}

void retrowave_mastergear_mute_sn76489(RetroWaveContext *ctx) {
	uint8_t mute_tone1 = 0x9f;
	uint8_t mute_tone2 = 0xdf;
	uint8_t mute_tone3 = 0xbf;
	uint8_t mute_noise = 0xff;

	uint8_t buf[] = {RetroWave_Board_MasterGear, 0x12,
			 0xff, mute_tone1, 0x5f, mute_tone1, 0x0f, mute_tone1, 0xaf, mute_tone1, 0xff, 0x00,
			 0xff, mute_tone2, 0x5f, mute_tone2, 0x0f, mute_tone2, 0xaf, mute_tone2, 0xff, 0x00,
			 0xff, mute_tone3, 0x5f, mute_tone3, 0x0f, mute_tone3, 0xaf, mute_tone3, 0xff, 0x00,
			 0xff, mute_noise, 0x5f, mute_noise, 0x0f, mute_noise, 0xaf, mute_noise, 0xff, 0x00,
	};

	ctx->callback_io(ctx->user_data, transfer_speed / 10, buf, NULL, sizeof(buf));
}

void retrowave_mastergear_queue_sn76489_left(RetroWaveContext *ctx, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_MasterGear, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	ctx->cmd_buffer_used += 10;
	// Set data only
	ctx->cmd_buffer[ctx->cmd_buffer_used - 10] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 9] = val;
	// CS# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 8] = 0xdf;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 7] = val;
	// CS#+WR# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0xcf;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = val;
	// WR# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xef;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
	// ALL off
	ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = 0x00;
}

void retrowave_mastergear_queue_sn76489_right(RetroWaveContext *ctx, uint8_t val) {
	retrowave_cmd_buffer_init(ctx, RetroWave_Board_MasterGear, 0x12);
	ctx->transfer_speed_hint = transfer_speed;

	ctx->cmd_buffer_used += 10;
	// Set data only
	ctx->cmd_buffer[ctx->cmd_buffer_used - 10] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 9] = val;
	// CS# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 8] = 0x7f;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 7] = val;
	// CS#+WR# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 6] = 0x3f;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 5] = val;
	// WR# on
	ctx->cmd_buffer[ctx->cmd_buffer_used - 4] = 0xbf;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 3] = val;
	// ALL off
	ctx->cmd_buffer[ctx->cmd_buffer_used - 2] = 0xff;
	ctx->cmd_buffer[ctx->cmd_buffer_used - 1] = 0x00;
}