#include "../serialport/libserial.h"

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
			COMPORT comport;
	};
#endif
