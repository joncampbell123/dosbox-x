#include "../serialport/libserial.h"
#include "setup.h"
#include "opl3duoboard.h"
#include <thread>

Opl3DuoBoard::Opl3DuoBoard() {
}

void Opl3DuoBoard::connect(const char* port) {
	printf("OPL3 Duo! Board: Connecting to port %s... \n", port);

	comport = 0;
	if (SERIAL_open(port, &comport)) {
		SERIAL_setCommParameters(comport, 115200, 'n', SERIAL_1STOP, 8);
		printf("OK\n");

        resetBuffer();
        stopOPL3DuoThread = false;
        thread = std::thread(&Opl3DuoBoard::writeBuffer, this);
	} else {
		printf("FAIL\n");
	}
}

void Opl3DuoBoard::disconnect() {
	#if OPL3_DUO_BOARD_DEBUG
    	printf("OPL3 Duo! Board: Disconnect\n");
	#endif

    // Stop buffer thread after resetting the OPL3 Duo board.
    if(thread.joinable()) {
        reset();
        stopOPL3DuoThread = true;
        thread.join();
    }

    // Once buffer thread has stopped close the port.
    if(comport) {
        SERIAL_close(comport);
    }
}

void Opl3DuoBoard::reset() {
	#if OPL3_DUO_BOARD_DEBUG
		printf("OPL3 Duo! Board: Reset\n");
	#endif

    resetBuffer();
    write(0x105, 0x01);     // Enable OPL3 mode
    write(0x104, 0x00);     // Disable all 4-op channels

    // Clear all registers of banks 0 and 1
    for (uint16_t i = 0x000; i <= 0x100; i += 0x100) {
        for(uint16_t j = 0x20; j <= 0xF5; j++) {
            write(i + j, j >= 0x40 && j <= 0x55 ? 0xFF : 0x00);
        }
    }

    write(0x105, 0x00);     // Disable OPL3 mode
    write(0x01, 0x00);      // Clear waveform select
    write(0x08, 0x00);      // Clear CSW and N-S
}

void Opl3DuoBoard::resetBuffer() {
    bufferWrPos = 0;
    bufferRdPos = 0;
}

void Opl3DuoBoard::write(uint32_t reg, uint8_t val) {
	if (comport) {
		#if OPL3_DUO_BOARD_DEBUG
			printf("OPL3 Duo! Board: Write %d --> %d\n", val, reg);
		#endif

		sendBuffer[bufferWrPos] = (reg >> 6) | 0x80;
		sendBuffer[bufferWrPos + 1] = ((reg & 0x3f) << 1) | (val >> 7);
		sendBuffer[bufferWrPos + 2] = (val & 0x7f);
        bufferWrPos = (bufferWrPos + 3) % OPL3_DUO_BUFFER_SIZE;
	}
}

void Opl3DuoBoard::writeBuffer() {
    do {
        while(bufferRdPos != bufferWrPos) {
            SERIAL_sendchar(comport, sendBuffer[bufferRdPos]);
            bufferRdPos = (bufferRdPos + 1) % OPL3_DUO_BUFFER_SIZE;
        }
    } while(!stopOPL3DuoThread);
}
