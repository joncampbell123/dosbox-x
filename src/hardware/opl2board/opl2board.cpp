#include "../serialport/libserial.h"
#include "setup.h"
#include "opl2board.h"

OPL2AudioBoard::OPL2AudioBoard() {
}

void OPL2AudioBoard::connect(const char* port) {
	printf("OPL2 Audio Board: Connecting to port %s... ", port);

	comport = 0;
	if (SERIAL_open(port, &comport)) {
		SERIAL_setCommParameters(comport, 115200, 'n', SERIAL_1STOP, 8);
		printf("OK\n");
	} else {
		printf("FAIL\n");
	}
}

void OPL2AudioBoard::disconnect() {
	#if OPL2_AUDIO_BOARD_DEBUG
		printf("OPL2 Audio Board: Disconnect");
	#endif

	if (comport) {
		SERIAL_close(comport);
	}
}

void OPL2AudioBoard::reset() {
	#if OPL2_AUDIO_BOARD_DEBUG
		printf("OPL2 Audio Board: Reset\n");
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

void OPL2AudioBoard::write(uint8_t reg, uint8_t val) {
	if (comport) {
		#if OPL2_AUDIO_BOARD_DEBUG
			printf("OPL2 Audio Board: Write %d --> %d\n", val, reg);
		#endif

		SERIAL_sendchar(comport, reg);
		SERIAL_sendchar(comport, val);
	}
}
