#include "../serialport/libserial.h"
#include "setup.h"
#include "opl2board.h"

OPL2AudioBoard::OPL2AudioBoard() {
}

void OPL2AudioBoard::connect(const char* port) {
	printf("OPL2 Audio Board: Connecting to port %s... \n", port);

	comport = nullptr;
	if (SERIAL_open(port, &comport)) {
		SERIAL_setCommParameters(comport, 115200, 'n', SERIAL_1STOP, 8);
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
		resetBuffer();
		stopThread = false;
		thread = std::thread(&OPL2AudioBoard::writeBuffer, this);
#endif
		printf("OPL2 Audio Board: COM Port OK.\n");
	} else {
		printf("OPL2 Audio Board: Unable to open COM port Failed. Error %d: %s\n", errno, strerror(errno));
	}
}

void OPL2AudioBoard::disconnect() {
	#if OPL2_AUDIO_BOARD_DEBUG
		printf("OPL2 Audio Board: Disconnect");
	#endif
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
	// Stop buffer thread after resetting the board.
	if(thread.joinable()) {
		reset();
		stopThread = true;
		thread.join();
	}
#else
	reset();
#endif
	if (comport) {
		SERIAL_close(comport);
	}
}

void OPL2AudioBoard::reset() {
	#if OPL2_AUDIO_BOARD_DEBUG
		printf("OPL2 Audio Board: Reset\n");
	#endif

	resetBuffer();

	for (uint8_t i = 0x00; i < 0xFF; i++) {
		if (i >= 0x40 && i <= 0x55) {
			// Set channel volumes to minimum.
			write(i, 0x3F);
		} else {
			write(i, 0x00);
		}
	}
}
void OPL2AudioBoard::resetBuffer()
{
	sendBuffer = std::queue<uint8_t>();
}
void OPL2AudioBoard::writeBuffer()
{
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))

	#if !defined(MACOS)
		/* Note:(josephillips85) This is a workaround to fix the thread stop issue presented on MacOS
		 Probably hitting this BUG https://github.com/apple/darwin-libpthread/blob/main/src/pthread.c#L2177
		 Already tested on MacOS Big Sur 11.2.1 */
		while(!stopThread) {
			if (!sendBuffer.empty()) {
				if (SERIAL_sendchar(comport, sendBuffer.front())) {
					sendBuffer.pop();
				}
			}
		}
	#else
		do {
			while(!sendBuffer.empty()) {
				if(SERIAL_sendchar(comport, sendBuffer.front())) {
					sendBuffer.pop();
				};
			}
		} while(!stopThread);
	#endif

#endif
}
void OPL2AudioBoard::write(uint8_t reg, uint8_t val) {
	if (comport) {
		#if OPL2_AUDIO_BOARD_DEBUG
			printf("OPL2 Audio Board: Write %d --> %d | buffer size %d\n", val, reg, sendBuffer.size());
		#endif
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
		sendBuffer.push(reg);
		sendBuffer.push(val);
#else
		SERIAL_sendchar(comport, reg);
		SERIAL_sendchar(comport, val);
#endif
	}
}
