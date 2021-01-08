#include <thread>
#include "../serialport/libserial.h"

#ifndef OPL3_DUO_BOARD
	#define OPL3_DUO_BOARD
    #define OPL3_DUO_BUFFER_SIZE 9000

	// Output debug information to the DosBox console if set to 1
	#define OPL3_DUO_BOARD_DEBUG 0

	class Opl3DuoBoard {
		public:
			Opl3DuoBoard();
			void connect(const char* port);
			void disconnect();
			void reset();
			void write(uint32_t reg, uint8_t val);

		private:
            void resetBuffer();
            void writeBuffer();

            std::thread thread;
            COMPORT comport;
            bool stopOPL3DuoThread;
            uint8_t sendBuffer[OPL3_DUO_BUFFER_SIZE];
            uint16_t bufferRdPos = 0;
            uint16_t bufferWrPos = 0;
	};
#endif
