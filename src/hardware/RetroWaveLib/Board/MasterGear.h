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

#include "../RetroWave.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void retrowave_mastergear_queue_ym2413(RetroWaveContext *ctx, uint8_t reg, uint8_t val);

extern void retrowave_mastergear_reset_ym2413(RetroWaveContext *ctx);

extern void retrowave_mastergear_queue_sn76489(RetroWaveContext *ctx, uint8_t val);
extern void retrowave_mastergear_queue_sn76489_left(RetroWaveContext *ctx, uint8_t val);
extern void retrowave_mastergear_queue_sn76489_right(RetroWaveContext *ctx, uint8_t val);

extern void retrowave_mastergear_mute_sn76489(RetroWaveContext *ctx);

#ifdef __cplusplus
};
#endif
