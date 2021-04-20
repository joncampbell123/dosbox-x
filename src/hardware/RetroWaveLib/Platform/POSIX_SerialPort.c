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

#include "POSIX_SerialPort.h"
#include "assert.h"

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))

static const char log_tag[] = "retrowave platform posix_serialport";

int set_tty(int fd) {
	struct termios tio;

#ifdef __linux__
	if (ioctl(fd, TCGETS, &tio)) {
#else
	if (tcgetattr(fd, &tio)) {
#endif
		fprintf(stderr, "%s: FATAL: failed to get tty attributes: %s\n", log_tag, strerror(errno));
		return -1;
	}

	tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tio.c_oflag &= ~OPOST;
	tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tio.c_cflag &= ~(CSIZE | PARENB);
	tio.c_cflag |= CS8;

#if defined (__linux__) || defined (__CYGWIN__)
	tio.c_cflag &= ~CBAUD;
	tio.c_cflag |= B2000000;
#endif

#if defined (BSD) || defined(__FreeBSD__)
	cfsetispeed(&tio, 2000000);
        cfsetospeed(&tio, 2000000);
#endif

#ifdef __linux__
	if (ioctl(fd, TCSETS, &tio)) {
#else
	if (tcsetattr(fd, 0, &tio)) {
#endif
		fprintf(stderr, "%s: FATAL: failed to set tty attributes: %s\n", log_tag, strerror(errno));
		return -1;
	}

#ifdef __APPLE__
	int speed = 2000000;

	if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
		fprintf(stderr, "%s: FATAL: failed to set MacOS specific tty attributes: %s", log_tag, strerror(errno));
		return -1;
	}
#endif

	return 0;
}

static inline void set_device_lock(RetroWavePlatform_POSIXSerialPort *ctx, int value) {
	errno = 0;

	int rc_lock;

	do {
		rc_lock = flock(ctx->fd_tty, value ? LOCK_EX : LOCK_UN);
	} while (errno == EINTR);

	if (rc_lock < 0) {
		fprintf(stderr, "%s: FATAL: failed to change lock state on tty device node: %s\n", log_tag, strerror(errno));
		abort();
	}
}

static void io_callback(void *userp, uint32_t data_rate, const void *tx_buf, void *rx_buf, uint32_t len) {
	RetroWavePlatform_POSIXSerialPort *ctx = userp;

	set_device_lock(ctx, 1);

	uint32_t packed_len = retrowave_protocol_serial_packed_length(len);

	uint8_t *packed_data;

	if (packed_len > 128)
		packed_data = malloc(packed_len);
	else
		packed_data = alloca(packed_len);


	assert(retrowave_protocol_serial_pack(tx_buf, len, packed_data) == packed_len);

	size_t written = 0;

	while (written < len) {
		ssize_t rc = write(ctx->fd_tty, packed_data + written, packed_len - written);
		if (rc > 0) {
			written += rc;
		} else {
			fprintf(stderr, "%s: FATAL: failed to write to tty: %s\n", log_tag, strerror(errno));
			abort();
		}
	}

	if (packed_len > 128)
		free(packed_data);

	set_device_lock(ctx, 0);
}

int retrowave_init_posix_serialport(RetroWaveContext *ctx, const char *tty_path) {
	retrowave_init(ctx);

	ctx->user_data = malloc(sizeof(RetroWavePlatform_POSIXSerialPort));

	RetroWavePlatform_POSIXSerialPort *pctx = ctx->user_data;

	pctx->fd_tty = open(tty_path, O_RDWR);
	if (pctx->fd_tty < 0) {
		fprintf(stderr, "%s: failed to open tty device `%s': %s\n", log_tag, tty_path, strerror(errno));
		free(pctx);
		return -1;
	}

	if (set_tty(pctx->fd_tty)) {
#ifdef __CYGWIN__
		puts("Workaround activated: ignoring all termios errors on Cygwin.");
#else
		return -1;
#endif
	}

	ctx->callback_io = io_callback;

	return 0;
}

void retrowave_deinit_posix_serialport(RetroWaveContext *ctx) {
	RetroWavePlatform_POSIXSerialPort *pctx = ctx->user_data;
	close(pctx->fd_tty);
	free(pctx);
}

#endif