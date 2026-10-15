/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _MONITOR_CONSOLE_
#define _MONITOR_CONSOLE_

#include <string>
#include <vector>
#include <atomic>
#include <cctype>
#if defined(C_SDL2_NET) && C_SDL2_NET
#include <SDL2/SDL_net.h>
#elif defined(C_SDL_NET) && C_SDL_NET
#include "SDL_net.h"
#endif

#include "cpu.h"
#include "dosbox.h"
#include "ide.h"
#include "logging.h"
#include "../dos/drives.h"

#define INPUT_LENGTH 1024

extern bool CPU_CycleAutoAdjust;

extern void Get_IDECD_drives( std::vector< int >& );
extern std::string GetIDEInfo();
extern void runImgmountISO(const char drive,const std::vector<std::string> &paths,const bool replace);
extern void RDTSC_rebase(void);
extern void menu_update_autocycle(void);
extern void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);

typedef std::vector< std::string > dinfo_t;
typedef std::vector< dinfo_t > ddesc_t;

typedef void (*p_monFunc)(void*);

typedef struct {
	std::string	command;
	p_monFunc	exec;
	void		*data;
} CommandExec;

typedef std::vector< CommandExec > CommandList;

class Monitor{

	Monitor( const Monitor& )= delete;
	Monitor& operator=( const Monitor& )= delete;

	dinfo_t			_userInput;
	TCPsocket		_server,_client;
	IPaddress		_hostIP;
	SDL_Thread		*_pThread;
	std::string		_message;

	std::atomic< bool >	_isRunning;

	const std::string	_prompt;
	const CommandList	_commands= {
		{ "close",	CB_CloseServer },
		{ "exit",	CB_ExitClient },
		{ "list",	CB_ListIDE },
		{ "change",	CB_ChangeIDE },
		{ "cycles",	CB_Cycles },
	};

	/* Callbacks are paired with simple string */

	static void CB_CloseServer( void *data ){
		Monitor& m= *reinterpret_cast< Monitor* >( data );
		m.onServerClose();
		LOG_MSG( "MONITOR server successfully closed." );
	}

	static void CB_ExitClient( void *data ) {
		Monitor& m= *reinterpret_cast< Monitor* >( data );
		m.closeClientSocket();
		LOG_MSG( "MONITOR client successfully disconnected." );
	}

	static void CB_ChangeIDE( void *data ) {	
		LOG_MSG( "MONITOR attempting to change CD." );
		Monitor& m= *reinterpret_cast< Monitor* >( data );
		m.changeIDE();
	}

	static void CB_ListIDE( void *data ) {
		LOG_MSG( "MONITOR searching for attached IDE-CD." );
		Monitor& m= *reinterpret_cast< Monitor* >( data );
		m.listIDE();
	}

	static void CB_Cycles( void *data ) {
		LOG_MSG( "MONITOR setting cycles to %d.",20000 );
		Monitor& m= *reinterpret_cast< Monitor* >( data );
		m.setCycles();
	}

	/* Callback for threading. */
	static int CB_Listen( void *tData ) {
		auto& m= *reinterpret_cast< Monitor* >( tData );
		
		m.updateServer();

		return EXIT_SUCCESS;
	}

	/* Main program functions */
	void	prompt( TCPsocket tClient ){
		SDLNet_TCP_Send( tClient,_prompt.c_str(),_prompt.length() );
	}

	std::vector< std::string >	processCLI( std::string &tBuffer ){
		std::vector< std::string > vCommands ={};
		std::istringstream info_stream( tBuffer );
		std::string sstr_token;

		while( info_stream >>sstr_token ) {
			vCommands.push_back( sstr_token );
		}
		return vCommands;
	}

	/* TODO:
	 *		Rewrite this function to behave more like the mainmenu counter part.
	 *		Where it should accep and processt Auto, Fixed and Max.
	 */
	void	setCycles() {
		if( _userInput.size() <2 ) {
			_message= "Format: cycles <value>\n";
			SDLNet_TCP_Send( _client,_message.c_str(),_message.size() );
			return;
		}

		int newCycleCount= std::stoi( _userInput.at( 1 ) );

		RDTSC_rebase();
		CPU_CycleAutoAdjust= false;
		CPU_CycleLeft= 0;
		CPU_Cycles= 0;
		CPU_CycleMax= CPU_OldCycleMax= newCycleCount;
		GFX_SetTitle( (int32_t)CPU_OldCycleMax,-1,-1,false );
		menu_update_autocycle();
	}

	bool	startServer( int tPort ) {
		if( int retval= SDLNet_Init() )
			LOG_MSG( "SDLNet returned: %d\n",retval );

		if( _server )
			onServerClose();

		if ( SDLNet_ResolveHost( &_hostIP,nullptr,tPort ) ) {
	       		fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
			return false;
		}

		_server= SDLNet_TCP_Open( &_hostIP );

		return true;
	}

	TCPsocket	connectClient() {
		TCPsocket client= nullptr;
		
		while( nullptr ==client ){
				client= SDLNet_TCP_Accept( _server );

			if ( client ) {
				LOG_MSG( "MONITOR client connected on %d.%d.%d.%d:%d.\n",
						( _hostIP.host )&0xff,( _hostIP.host >>8 )&0xff,
						( _hostIP.host >>16 )&0xff,( _hostIP.host >>24 )&0xff,
						SDLNet_Read16( &_hostIP.port ) );
				break;
			}
			SDL_Delay( 50 );
		}

		return client;
	}

	void	closeSocket( TCPsocket& tSocket ) {
		SDLNet_TCP_Close( tSocket );
		tSocket= nullptr;
	}

	void	closeClientSocket() {
		if( _client ) closeSocket( _client );
	}

	void	formatIDEMessageAndSend( dinfo_t& tDriveString ) {
			std::string info= std::string( "\t" ) + tDriveString.at( 0 ) + ":\\\t(" + tDriveString.at( 1 ) + ")\n";
			LOG_MSG( "%s",info.c_str() );
			SDLNet_TCP_Send( _client,info.c_str(),info.size() );	
	}

	/* Look for all available CD-IDE drives. */
	ddesc_t	lookupIDE() {
		std::vector< int > dds={};

		Get_IDECD_drives( dds );
		ddesc_t drives_available= {};
		
		for( auto n:dds ){
			std::string drive_info=Drives[ n ]->GetInfo();
				
			dinfo_t vInfo= {};
			std::istringstream info_stream( drive_info );
			std::string info_token;

			while( info_stream >> info_token ) {
				vInfo.push_back( info_token );
			}

			drives_available.emplace_back( dinfo_t{ std::string( 1,char( 'A'+n ) ),vInfo.at( 1 ) } );
		}
		
		std::sort( drives_available.begin(),drives_available.end(),
				[]( const dinfo_t& a,const dinfo_t& b ){
			return a[0] < b[0];} );

		return drives_available;
	}
	
	/* Format and print available drives to log and send to client. */
	void	listIDE() {
		ddesc_t drives= lookupIDE();

		_message= "No drive(s) mounted.\n";

		if( drives.empty() ) {
			SDLNet_TCP_Send( _client,_message.c_str(),_message.size() );
			return;
		}

		_message= "Available CD IDE drives:\n";
		SDLNet_TCP_Send( _client,_message.c_str(),_message.size() );

		for( auto drive:drives ) {
			formatIDEMessageAndSend( drive );
		}
	}

	void	changeIDE() {		
		if( _userInput.size() <3 ) {
			_message= "Format: change <drive_letter> <image_path>\n";
			SDLNet_TCP_Send( _client,_message.c_str(),_message.size() );
			return;
		}

		ddesc_t drives= lookupIDE();
					
		_message=  "No drive(s) mounted.\n";
		if( drives.empty() ) {
			SDLNet_TCP_Send( _client,_message.c_str(),_message.size() );
			return;
		}

		_userInput.at( 1 )= std::toupper( _userInput.at( 1 ).at( 0 ) );
					
		std::string drivePath= _userInput.at( 1 );
		std::string imagePath= _userInput.at( 2 );

		auto iDrive= std::find_if( drives.begin(),drives.end(),
			[ & ]( const std::vector< std::string >& drive ){		
				return drive[0] ==drivePath; } );

		if( iDrive ==drives.end() ) {
			_message= "Drive is unavailable.\n";
			SDLNet_TCP_Send( _client,_message.c_str(),_message.size() );
			return;
		}

		runImgmountISO( *drivePath.c_str(),std::vector< std::string >{ imagePath },true );

		drives=	lookupIDE();

		formatIDEMessageAndSend( *iDrive );	
	}

	void	updateServer( ) {
		while( _isRunning ) {
			if( nullptr ==_client ) {
				_client= connectClient();
			}

			if( _client )
				prompt( _client );

			char buffer[ INPUT_LENGTH ];

			memset(buffer,'\0',sizeof( char )*INPUT_LENGTH);
			int bytes_received=SDLNet_TCP_Recv( _client,buffer,INPUT_LENGTH );

			if( '\0' !=buffer[ bytes_received-1 ] )
				buffer[ bytes_received-1 ] = '\0';

			if( bytes_received <=0  ){
				LOG_MSG( "MONITOR client disconnected unexpectedly or connection lost.\n" );
				SDLNet_TCP_Close( _client );
				_client= nullptr;
				continue;
			} else {
				std::string inputRecv{ buffer };

				_userInput= processCLI( inputRecv );

				if( _userInput.empty() ){ continue; }

				std::string command= _userInput.at( 0 );
				auto processCommand= std::find_if( _commands.begin(),_commands.end(),
					[ & ]( const CommandExec& item ){	
							return item.command == command;});
				
				if( processCommand !=_commands.end() )
					processCommand->exec( this );	
			}	
		}
		onServerClose();
	}

	void	onServerClose() {
		if( _isRunning )
			_isRunning= false;

		if( _client ) closeSocket( _client );
		if( _server ) closeSocket( _server ); 

		if( _pThread ){
			
			#if defined(C_SDL_NET) && C_SDL_NET
			int status;
			SDL_WaitThread( _pThread,&status );
			#endif

			_pThread= nullptr;
		}
	
		SDLNet_Quit();
	}

	/* CTOR */
	Monitor():
	_userInput( 8,"\0" ),
   _server( nullptr ),_client( nullptr ),
	_hostIP( IPaddress{ 0,0 } ),_pThread( nullptr ),
	_message( std::string( 128,'\0' ) ),
	_isRunning( false ),_prompt( "(dosbox-x) " ) {
	}

	public:

	~Monitor() {
		onServerClose();
	}

	static	Monitor& OpenMonitor( int tPort) {
		static Monitor m;

		if( false ==m._isRunning ) {
			if( m._pThread ) m.onServerClose();

			m._isRunning= m.startServer( tPort );

			if ( true ==m._isRunning ){

				#if defined(C_SDL2) && C_SDL
				m._pThread= SDL_CreateThread( CB_Listen,"dosbox-x monitor",&m );
				SDL_DetachThread( m._pThread );
				#else
				m._pThread= SDL_CreateThread( CB_Listen,(void*)&m );
				#endif
			}
		}
		return m;
	}
};

#endif  // _MONITOR_CONSOLE_
