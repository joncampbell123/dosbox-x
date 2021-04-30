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
#include "../Protocol/Serial.h"

#ifdef _WIN32

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	HANDLE porthandle;
} RetroWavePlatform_Win32SerialPort;

extern int retrowave_init_win32_serialport(RetroWaveContext *ctx, const char *com_path);
extern void retrowave_deinit_win32_serialport(RetroWaveContext *ctx);

#ifdef __cplusplus
};
#endif

#endif