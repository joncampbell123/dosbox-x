#ifndef __GAMELINK_H___
#define __GAMELINK_H___

#include "config.h"

#if C_GAMELINK

#include "dosbox.h"

#ifdef WIN32
#include <Windows.h>
#endif // WIN32

//------------------------------------------------------------------------------
// Namespace Declaration
//------------------------------------------------------------------------------

namespace GameLink
{

	//--------------------------------------------------------------------------
	// Global Declarations
	//--------------------------------------------------------------------------

#pragma pack( push, 1 )

	//
	// sSharedMMapFrame_R1
	//
	// Server -> Client Frame. 32-bit RGBA up to MAX_WIDTH x MAX_HEIGHT
	//
	struct sSharedMMapFrame_R1
	{
		uint16_t seq;
		uint16_t width;
		uint16_t height;

		uint8_t image_fmt; // 0 = no frame; 1 = 32-bit 0xAARRGGBB
		uint8_t reserved0;

		uint16_t par_x; // pixel aspect ratio
		uint16_t par_y;

		enum { MAX_WIDTH = 1280 };
		enum { MAX_HEIGHT = 1024 };

		enum { MAX_PAYLOAD = MAX_WIDTH * MAX_HEIGHT * 4 };
		uint8_t buffer[ MAX_PAYLOAD ];
	};

	//
	// sSharedMMapInput_R2
	//
	// Client -> Server Input Data
	//
	struct sSharedMMapInput_R2
	{
		float mouse_dx;
		float mouse_dy;
		uint8_t ready;
		uint8_t mouse_btn;
		uint32_t keyb_state[ 8 ];
	};

	//
	// sSharedMMapPeek_R2
	//
	// Memory reading interface.
	//
	struct sSharedMMapPeek_R2
	{
		enum { PEEK_LIMIT = 16 * 1024 };

		uint32_t addr_count;
		uint32_t addr[ PEEK_LIMIT ];
		uint8_t data[ PEEK_LIMIT ];
	};

	//
	// sSharedMMapBuffer_R1
	//
	// General buffer (64Kb)
	//
	struct sSharedMMapBuffer_R1
	{
		enum { BUFFER_SIZE = ( 64 * 1024 ) };

		uint16_t payload;
		uint8_t data[ BUFFER_SIZE ];
	};

	//
	// sSharedMMapAudio_R1
	//
	// Audio control interface.
	//
	struct sSharedMMapAudio_R1
	{
		uint8_t master_vol_l;
		uint8_t master_vol_r;
	};

	//
	// sSharedMemoryMap_R4
	//
	// Memory Map (top-level object)
	//
	struct sSharedMemoryMap_R4
	{
		enum {
			FLAG_WANT_KEYB			= 1 << 0,
			FLAG_WANT_MOUSE			= 1 << 1,
			FLAG_NO_FRAME			= 1 << 2,
			FLAG_PAUSED				= 1 << 3,
		};

		enum {
			SYSTEM_MAXLEN			= 64
		};

		enum {
			PROGRAM_MAXLEN			= 260
		};

		uint8_t version; // = PROTOCOL_VER
		uint8_t flags;
		char system[ SYSTEM_MAXLEN ]; // System name.
		char program[ PROGRAM_MAXLEN ]; // Program name. Zero terminated.
		uint32_t program_hash[ 4 ]; // Program code hash (256-bits)

		sSharedMMapFrame_R1 frame;
		sSharedMMapInput_R2 input;
		sSharedMMapPeek_R2 peek;
		sSharedMMapBuffer_R1 buf_recv; // a message to us.
		sSharedMMapBuffer_R1 buf_tohost;
		sSharedMMapAudio_R1 audio;

		// added for protocol v4
		uint32_t ram_size;
	};

#pragma pack( pop )


	//--------------------------------------------------------------------------
	// Global Functions
	//--------------------------------------------------------------------------

	extern int Init( const bool trackonly_mode );
	
	extern uint8_t* AllocRAM( const uint32_t size );
	extern void FreeRAM(void *membase);

	extern void Term();

	extern int In( sSharedMMapInput_R2* p_input,
				   sSharedMMapAudio_R1* p_audio );

	extern void Out( const uint16_t frame_width,
					 const uint16_t frame_height,
					 const double source_ratio,
					 const bool need_mouse,
					 const char* p_program,
					 const uint32_t* p_program_hash,
					 const uint8_t* p_frame,
					 const uint8_t* p_sysmem );

	extern void ExecTerminal( sSharedMMapBuffer_R1* p_inbuf,
							  sSharedMMapBuffer_R1* p_outbuf,
							  sSharedMMapBuffer_R1* p_mechbuf );

	extern void ExecTerminalMech( sSharedMMapBuffer_R1* p_mechbuf );

	extern void InitTerminal();


}; // namespace GameLink

//==============================================================================
#endif

#endif // __GAMELINK_HDR__
