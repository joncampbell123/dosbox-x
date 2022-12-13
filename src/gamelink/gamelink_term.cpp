
// Game Link - Console

//------------------------------------------------------------------------------
// Dependencies
//------------------------------------------------------------------------------

#include "config.h"

#if C_GAMELINK

// External Dependencies
#ifdef WIN32
#else // WIN32
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#endif // WIN32

// Local Dependencies
#include "dosbox.h"
#include "gamelink.h"
#include "sdlmain.h"
#include "../resource.h"
#include <stdio.h>
#include "mem.h"

typedef GameLink::sSharedMMapBuffer_R1	Buffer;

//==============================================================================

//------------------------------------------------------------------------------
// Local Data
//------------------------------------------------------------------------------

static Buffer* g_p_outbuf;


//------------------------------------------------------------------------------
// Local Functions
//------------------------------------------------------------------------------

//
// out_char
//
// Copy a string into the output buffer.
//
static void out_char( const char ch )
{
	// Overflow?
	if ( g_p_outbuf->payload >= Buffer::BUFFER_SIZE - 1 ) {
		return;
	}

	// Copy character
	g_p_outbuf->data[ g_p_outbuf->payload++ ] = ch;

	// terminate
	g_p_outbuf->data[ g_p_outbuf->payload ] = 0;
}

//
// out_strcpy
//
// Copy a string into the output buffer.
//
static void out_strcpy( const char* p )
{
	while ( *p )
	{
		// Overflow?
		if ( g_p_outbuf->payload >= Buffer::BUFFER_SIZE - 1 ) {
			break;
		}

		// Copy character
		g_p_outbuf->data[ g_p_outbuf->payload++ ] = *p++;
	}

	// terminate
	g_p_outbuf->data[ g_p_outbuf->payload ] = 0;
}

//
// out_strint
//
// Copy an integer as plain text into the output buffer.
//
static void out_strint( const int v )
{
	char buf[ 32 ];
	sprintf( buf, "%d", v );
	out_strcpy( buf );
}

//
// out_strhex
//
// Copy a hex value as plain text into the output buffer.
// Zero prefixed.
//
static void out_strhex( const int v, const int wide )
{
	char buf[ 32 ], fmt[ 32 ] = "%08X";

	if ( wide >= 1 && wide <= 9 ) {
		fmt[ 2 ] = '0' + wide;
		sprintf( buf, fmt, v );
		out_strcpy( buf );
	}
}

//
// out_strhexspc
//
// Copy a hex value as plain text into the output buffer.
//
static void out_strhexspc( const int v, const int wide )
{
	char buf[ 32 ], fmt[ 32 ] = "%8X";

	if ( wide >= 1 && wide <= 9 ) {
		fmt[ 1 ] = '0' + wide;
		sprintf( buf, fmt, v );
		out_strcpy( buf );
	}
}

//
// out_memcpy
//
// Copy a block of memory into the output buffer.
//
static void out_memcpy( const void* p, const uint16_t len )
{
	const uint8_t* src = (const uint8_t*)p;

	for ( uint16_t i = 0; i < len; ++i )
	{
		// Overflow?
		if ( g_p_outbuf->payload >= Buffer::BUFFER_SIZE - 1 ) {
			break;
		}

		// Copy data
		g_p_outbuf->data[ g_p_outbuf->payload++ ] = *src++;
	}
}

//==============================================================================

//
// proc_mech
//
// Process a mechanical command - encoded form for computer-computer communication. Minimal feedback.
//
static void proc_mech( Buffer* cmd, uint16_t payload )
{
	// Ignore NULL commands.
	if ( payload <= 1 || payload > 128 )
		return;

	cmd->payload = 0;
	char* com = (char*)(cmd->data);
	com[ payload ] = 0;

//	printf( "command = %s; payload = %d\n", com, payload );

	//
	// Decode

	if ( strcmp( com, ":reset" ) == 0 )
	{
//		printf( "do reset\n" );

		ResetSystem( true );
	}
	else if ( strcmp( com, ":pause" ) == 0 )
	{
//		printf( "do pause\n" );

		PauseDOSBox( true );
	}
	else if ( strcmp( com, ":shutdown" ) == 0 )
	{
//		printf( "do shutdown\n" );

		DoKillSwitch();
	}
}

//------------------------------------------------------------------------------
// Global Functions
//------------------------------------------------------------------------------

void GameLink::InitTerminal()
{
	g_p_outbuf = 0;
}

void GameLink::ExecTerminalMech( Buffer* p_procbuf )
{
	proc_mech( p_procbuf, p_procbuf->payload );
}

void GameLink::ExecTerminal( Buffer* p_inbuf, 
							 Buffer* p_outbuf,
							 Buffer* p_procbuf )
{
	// Nothing from the host, or host hasn't acknowledged our last message.
	if ( p_inbuf->payload == 0 ) {
		return;
	}
	if ( p_outbuf->payload > 0 ) {
		return;
	}

	// Store output pointer
	g_p_outbuf = p_outbuf;

	// Process mode select ...
	if ( p_inbuf->data[ 0 ] == ':' )
	{
		// Acknowledge now, to avoid loops.
		uint16_t payload = p_inbuf->payload;
		p_inbuf->payload = 0;

		// Copy out.
		memcpy( p_procbuf->data, p_inbuf->data, payload );
		p_procbuf->payload = payload;
	}
	else
	{
		// Human command
		char buf[ Buffer::BUFFER_SIZE + 1 ], *b = buf;

		// Convert into printable ASCII
		for ( uint32_t i = 0; i < p_inbuf->payload; ++i )
		{
			uint8_t u8 = p_inbuf->data[ i ];
			if ( u8 < 32 || u8 > 127 ) {
				*b++ = '?';
			} else {
				*b++ = (char)u8;
			}
		}

		// ... terminate
		*b++ = 0;

		// Acknowledge
		p_inbuf->payload = 0;

		// proc_human( buf ); // <-- deprecated
	}
}

//==============================================================================
#endif // C_GAMELINK

