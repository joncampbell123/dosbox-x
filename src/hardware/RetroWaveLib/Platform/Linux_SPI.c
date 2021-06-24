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

#include "Linux_SPI.h"

#ifdef __linux

static const char log_tag[] = "retrowave platform linux_spi";


static inline void __attribute__((always_inline)) set_cs(RetroWavePlatform_LinuxSPI *ctx, int value) {
	struct gpiohandle_data gpio_data;

	gpio_data.values[0] = value;

	if (ioctl(ctx->fd_gpioline, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &gpio_data)) {
		fprintf(stderr, "%s: FATAL: failed to set GPIO value: %s\n", log_tag, strerror(errno));
		abort();
	}
}

static inline void set_device_lock(RetroWavePlatform_LinuxSPI *ctx, int value) {
	errno = 0;

	int rc_lock;

	do {
		rc_lock = flock(ctx->fd_spi, value ? LOCK_EX : LOCK_UN);
	} while (errno == EINTR);

	if (rc_lock < 0) {
		fprintf(stderr, "%s: FATAL: failed to change lock state on SPI device node: %s\n", log_tag, strerror(errno));
		abort();
	}
}

static struct spi_ioc_transfer spi_tr = {0};

static void io_callback(void *userp, uint32_t data_rate, const void *tx_buf, void *rx_buf, uint32_t len) {
	RetroWavePlatform_LinuxSPI *ctx = userp;

	spi_tr.tx_buf = (__u64)tx_buf;
	spi_tr.rx_buf = (__u64)rx_buf;
	spi_tr.len = len;
	spi_tr.speed_hz = data_rate;
//	spi_tr.bits_per_word = 8;

//	set_device_lock(ctx, 1);

	set_cs(ctx, 0);

	if (ioctl(ctx->fd_spi, SPI_IOC_MESSAGE(1), &spi_tr) < 0) {
		fprintf(stderr, "%s: FATAL: failed to do SPI transfer: %s\n", log_tag, strerror(errno));
		abort();
	}

//	printf("SPI TX: ");
//
//	for (int i=0; i<len; i++) {
//		printf("%02x ", ((uint8_t *)tx_buf)[i]);
//	}
//
//	puts("");
//
//	printf("SPI RX: ");
//
//	for (int i=0; i<len; i++) {
//		printf("%02x ", ((uint8_t *)rx_buf)[i]);
//	}
//
//	puts("");

	set_cs(ctx, 1);

//	set_device_lock(ctx, 0);
}

int retrowave_init_linux_spi(RetroWaveContext *ctx, const char *spi_dev, int cs_gpio_chip, int cs_gpio_line) {
	retrowave_init(ctx);

	ctx->user_data = malloc(sizeof(RetroWavePlatform_LinuxSPI));

	RetroWavePlatform_LinuxSPI *pctx = ctx->user_data;

	// SPI Init
	pctx->fd_spi = open(spi_dev, O_RDWR);
	if (pctx->fd_spi < 0) {
		fprintf(stderr, "%s: failed to open SPI device `%s': %s\n", log_tag, spi_dev, strerror(errno));
		free(pctx);
		return -1;
	}

	int spi_mode = SPI_NO_CS;
	if (ioctl(pctx->fd_spi, SPI_IOC_WR_MODE32, &spi_mode) < 0) {
		fprintf(stderr, "%s: failed to set SPI mode 0x%02x: %s\n", log_tag, spi_mode, strerror(errno));
		free(pctx);
		return -1;
	}

	int spi_speed = 1200000;
	if (ioctl(pctx->fd_spi, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0) {
		fprintf(stderr, "%s: failed to set SPI speed to %d: %s\n", log_tag, spi_speed, strerror(errno));
		free(pctx);
		return -1;
	}

	// GPIO Init
	char pathbuf[24];
	snprintf(pathbuf, sizeof(pathbuf)-1, "/dev/gpiochip%d", cs_gpio_chip);
	pctx->fd_gpiochip = open(pathbuf, O_RDWR);
	if (pctx->fd_gpiochip < 0) {
		fprintf(stderr, "%s: failed to open GPIO device `%s': %s\n", log_tag, pathbuf, strerror(errno));
		free(pctx);
		return -1;
	}

	struct gpiohandle_request gl_req = {0};
	gl_req.lineoffsets[0] = cs_gpio_line;
	gl_req.default_values[0] = 1; // CS should be high if not used
	gl_req.flags = GPIOHANDLE_REQUEST_OUTPUT;
	gl_req.lines = 1;
	strcpy(gl_req.consumer_label, "RetroWave SPI CS");

retry_lock:
	if (ioctl(pctx->fd_gpiochip, GPIO_GET_LINEHANDLE_IOCTL, &gl_req)) {
		if (errno == EBUSY) {
			goto retry_lock;
		}
		fprintf(stderr, "%s: failed to request GPIO line %d: %s\n", log_tag, cs_gpio_line, strerror(errno));
		abort();
	}

	pctx->fd_gpioline = gl_req.fd;

	ctx->callback_io = io_callback;

	return 0;
}

void retrowave_deinit_linux_spi(RetroWaveContext *ctx) {
	RetroWavePlatform_LinuxSPI *pctx = ctx->user_data;

	close(pctx->fd_spi);
	close(pctx->fd_gpioline);
	close(pctx->fd_gpiochip);

	free(pctx);
}

#endif