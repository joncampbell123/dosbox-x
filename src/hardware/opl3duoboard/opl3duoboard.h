#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
#include <thread>
#endif
#include "../serialport/libserial.h"
#include <queue>

#ifndef OPL3_DUO_BOARD
	#define OPL3_DUO_BOARD


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
			std::queue<uint8_t> sendBuffer;
	};
#endif
