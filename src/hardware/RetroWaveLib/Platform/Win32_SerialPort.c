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

#include "Win32_SerialPort.h"

#ifdef _WIN32

static const char log_tag[] = "retrowave platform win32_serialport";

static void io_callback(void *userp, uint32_t data_rate, const void *tx_buf, void *rx_buf, uint32_t len) {
	RetroWavePlatform_Win32SerialPort *ctx = userp;

	uint32_t packed_len = retrowave_protocol_serial_packed_length(len);

	uint8_t *packed_data;

	if (packed_len > 128)
		packed_data = malloc(packed_len);
	else
		packed_data = alloca(packed_len);

	retrowave_protocol_serial_pack(tx_buf, len, packed_data);


	DWORD bytesWritten;

	WriteFile(ctx->porthandle, packed_data, packed_len, &bytesWritten, NULL);

	if (packed_len > 128)
		free(packed_data);
}

int retrowave_init_win32_serialport(RetroWaveContext *ctx, const char *com_path) {
	retrowave_init(ctx);

	ctx->user_data = malloc(sizeof(RetroWavePlatform_Win32SerialPort));

	RetroWavePlatform_Win32SerialPort *pctx = ctx->user_data;

	if (strlen(com_path) > 240) {
		printf("%s: error: COM path too long!\n", log_tag);
		SetLastError(ERROR_BUFFER_OVERFLOW);
		free(ctx->user_data);
		return -1;
	}

	char extended_portname[256];

	snprintf(extended_portname, sizeof(extended_portname)-1, "\\\\.\\%s", com_path);

	pctx->porthandle = CreateFile(extended_portname,
				     GENERIC_READ | GENERIC_WRITE, 0,
				     NULL,          // no security attributes
				     OPEN_EXISTING, // must use OPEN_EXISTING
				     0,             // non overlapped I/O
				     NULL           // hTemplate must be NULL for comm devices
	);

	if (pctx->porthandle == INVALID_HANDLE_VALUE) {
		printf("%s: CreateFile failed with error %d.\n", log_tag, GetLastError());
		free(ctx->user_data);
		return -1;
	}

	DCB dcb;
	BOOL fSuccess;

	//  Initialize the DCB structure.
	SecureZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);

	//  Build on the current configuration by first retrieving all current
	//  settings.
	fSuccess = GetCommState(pctx->porthandle, &dcb);

	if (!fSuccess) {
		// Handle the error.
		printf("%s: GetCommState failed with error %d.\n", log_tag, GetLastError());
		free(ctx->user_data);
		return -1;
	}

	//  Fill in some DCB values and set the com state:
	//  57,600 bps, 8 data bits, no parity, and 1 stop bit.
	dcb.BaudRate = CBR_115200;     //  baud rate
	dcb.ByteSize = 8;             //  data size, xmit and rcv
	dcb.Parity   = NOPARITY;      //  parity bit
	dcb.StopBits = ONESTOPBIT;    //  stop bit

	fSuccess = SetCommState(pctx->porthandle, &dcb);

	if (!fSuccess) {
		//  Handle the error.
		printf("%s: SetCommState failed with error %d.\n", log_tag, GetLastError());
		free(ctx->user_data);
		return -1;
	}

	ctx->callback_io = io_callback;

	return 0;
}

void retrowave_deinit_win32_serialport(RetroWaveContext *ctx) {
	RetroWavePlatform_Win32SerialPort *pctx = ctx->user_data;

	if (pctx->porthandle != INVALID_HANDLE_VALUE) {
		CloseHandle(pctx->porthandle);
	}

	free(pctx);
}

#endif