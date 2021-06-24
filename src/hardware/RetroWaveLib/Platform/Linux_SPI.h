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

#ifdef __linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/spi/spidev.h>

#include "../RetroWave.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int fd_spi, fd_gpiochip;
	int fd_gpioline;
} RetroWavePlatform_LinuxSPI;

extern int retrowave_init_linux_spi(RetroWaveContext *ctx, const char *spi_dev, int cs_gpio_chip, int cs_gpio_line);
extern void retrowave_deinit_linux_spi(RetroWaveContext *ctx);

#ifdef __cplusplus
};
#endif

#endif
