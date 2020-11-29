#include "../serialport/libserial.h"

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
			COMPORT comport;
	};
#endif