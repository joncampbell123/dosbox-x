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

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/file.h>

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif

#include "../RetroWave.h"
#include "../Protocol/Serial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int fd_tty;
} RetroWavePlatform_POSIXSerialPort;

extern int retrowave_init_posix_serialport(RetroWaveContext *ctx, const char *tty_path);
extern void retrowave_deinit_posix_serialport(RetroWaveContext *ctx);

#ifdef __cplusplus
};
#endif

#endif
