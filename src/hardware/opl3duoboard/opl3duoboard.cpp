#include "../serialport/libserial.h"
#include "setup.h"
#include "opl3duoboard.h"

Opl3DuoBoard::Opl3DuoBoard() {
}

void Opl3DuoBoard::connect(const char* port) {
	printf("OPL3 Duo! Board: Connecting to port %s... \n", port);

	comport = 0;
	if (SERIAL_open(port, &comport)) {
		SERIAL_setCommParameters(comport, 115200, 'n', SERIAL_1STOP, 8);
		printf("OK\n");
	} else {
		printf("FAIL\n");
	}
}

void Opl3DuoBoard::disconnect() {
	#if OPL3_DUO_BOARD_DEBUG
		printf("OPL3 Duo! Board: Disconnect\n");
	#endif

	if (comport) {
		SERIAL_close(comport);
	}
}

void Opl3DuoBoard::reset() {
	#if OPL3_DUO_BOARD_DEBUG
		printf("OPL3 Duo! Board: Reset\n");
	#endif

	for (uint8_t i = 0x00; i < 0xFF; i++) {
		if (i >= 0x40 && i <= 0x55) {
			// Set channel volumes to minimum.
			write(i, 0x3F);
		} else {
			write(i, 0x00);
		}
	}
}

void Opl3DuoBoard::write(uint32_t reg, uint8_t val) {
	if (comport) {
		#if OPL3_DUO_BOARD_DEBUG
			printf("OPL3 Duo! Board: Write %d --> %d\n", val, reg);
		#endif

		uint8_t sendBuffer[3];

		sendBuffer[0] = (reg >> 6) | 0x80;
		sendBuffer[1] = ((reg & 0x3f) << 1) | (val >> 7);
		sendBuffer[2] = (val & 0x7f);

		SERIAL_sendchar(comport, sendBuffer[0]);
		SERIAL_sendchar(comport, sendBuffer[1]);
		SERIAL_sendchar(comport, sendBuffer[2]);
	}
}