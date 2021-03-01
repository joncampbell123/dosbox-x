#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
#include <thread>
#endif
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

#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
            std::thread thread;
            bool stopOPL3DuoThread = false;
		
#endif
            COMPORT comport;
            uint8_t sendBuffer[OPL3_DUO_BUFFER_SIZE];
            uint16_t bufferRdPos = 0;
            uint16_t bufferWrPos = 0;
	};
#endif
