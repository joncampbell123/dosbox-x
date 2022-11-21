#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
  #include <thread>
#endif
#include "../serialport/libserial.h"
#include <queue>

#ifndef OPL2_AUDIO_BOARD
	#define OPL2_AUDIO_BOARD

	// Output debug information to the DosBox console if set to 1
	#define OPL2_AUDIO_BOARD_DEBUG 0

	class OPL2AudioBoard {
		public:
			OPL2AudioBoard();
			void connect(const char* port);
			void disconnect();
			void reset();
			void write(uint8_t reg, uint8_t val);

		private:
			void resetBuffer();
			void writeBuffer();

#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
			std::thread thread;
			bool stopThread = false;
#endif
			COMPORT comport;
			std::queue<uint8_t> sendBuffer;
	};
#endif
