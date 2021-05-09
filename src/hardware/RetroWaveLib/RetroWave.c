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

#include "RetroWave.h"

void retrowave_init(RetroWaveContext *ctx) {
	memset(ctx, 0, sizeof(RetroWaveContext));
	ctx->cmd_buffer = malloc(8192);
}

void retrowave_deinit(RetroWaveContext *ctx) {
	free(ctx->cmd_buffer);
}

void retrowave_io_init(RetroWaveContext *ctx) {
	// Sync CS state
	uint8_t empty_byte = 0;
	ctx->callback_io(ctx->user_data, (uint32_t)1e6, &empty_byte, NULL, 1);

	uint8_t init_sequence_1[] = {
		0x00,
		0x0a,	// IOCON register
		0x28	// Enable: HAEN, SEQOP
	};

	uint8_t init_sequence_2[] = {
		0x00,
		0x00,	// IODIRA register
		0x00,	// Set output
		0x00	// Set output
	};

	uint8_t init_sequence_3[] = {
		0x00,
		0x12,	// GPIOA register
		0xff,	// Set all HIGH
		0xff	// Set all HIGH
	};

	for (uint8_t i=0x20; i<0x28; i++) {
		uint8_t addr = i << 1;

		init_sequence_1[0] = init_sequence_2[0] = init_sequence_3[0] = addr;
		ctx->callback_io(ctx->user_data, (uint32_t)1e6, init_sequence_1, NULL, sizeof(init_sequence_1));
		ctx->callback_io(ctx->user_data, (uint32_t)1e6, init_sequence_2, NULL, sizeof(init_sequence_2));
		ctx->callback_io(ctx->user_data, (uint32_t)1e6, init_sequence_3, NULL, sizeof(init_sequence_3));
	}
}

void retrowave_cmd_buffer_init(RetroWaveContext *ctx, RetroWaveBoardType board_type, uint8_t first_reg) {
	if (ctx->cmd_buffer_used) {
		if (ctx->cmd_buffer[0] != board_type) {
			retrowave_flush(ctx);
		}
	}

	if (!ctx->cmd_buffer_used) {
		ctx->cmd_buffer[0] = board_type;
		ctx->cmd_buffer[1] = first_reg;
		ctx->cmd_buffer_used = 2;
	}
}

static inline void cmd_buffer_deinit(RetroWaveContext *ctx) {
	ctx->cmd_buffer_used = 0;
}

void retrowave_flush(RetroWaveContext *ctx) {
	if (ctx->cmd_buffer_used) {
//		puts("flush");

		ctx->callback_io(ctx->user_data, ctx->transfer_speed_hint, ctx->cmd_buffer, NULL, ctx->cmd_buffer_used);

		cmd_buffer_deinit(ctx);
	}
}

uint8_t retrowave_invert_byte(uint8_t val) {
	uint8_t ret;

	for (uint8_t i=0; i<8; i++) {
		uint8_t bit = (val >> i) & 1U;
		ret ^= (-bit ^ ret) & (1UL << (7-i));
	}

	return ret;
}
