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

#ifndef _MONITOR_CONSOLE_H
#define _MONITOR_CONSOLE_H


#include <algorithm>
#include <string>
#include <vector>
#include <atomic>
#include <cctype>
#include <iomanip>
#if defined(C_SDL2_NET) && C_SDL2_NET
#include <SDL2/SDL_net.h>
#else
#include "SDL_net.h"
#endif

#include "dosbox.h"
#include "logging.h"

#define INPUT_LENGTH 1024

extern bool save_dynamic_rec;

extern void Get_IDECD_drives( std::vector< int >& );
extern std::string GetIDEInfo();
extern void runImgmountISO(const char drive,const std::vector<std::string> &paths,const bool replace);
extern void RDTSC_rebase(void);
extern void menu_update_autocycle(void);
extern void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
#if (C_DYNAMIC_X86)
extern void CPU_Core_Dyn_X86_Cache_Init(bool enable_cache);
#endif
#if (C_DYNREC)
extern void CPU_Core_Dynrec_Cache_Init(bool enable_cache);
#endif
extern void DoKillSwitch( void );

typedef std::vector< std::string > dinfo_t;
typedef std::vector< std::string > cmd_tokens_t;
typedef std::vector< dinfo_t > ddesc_t;
typedef void (*p_monFunc)(void*);
typedef struct {
	std::string	command;
	p_monFunc	exec;
	std::string description;
} CommandExec;
typedef std::vector< CommandExec > CommandList;

class Monitor {
	Monitor( const Monitor& )= delete;
	Monitor& operator=( const Monitor& )= delete;

	struct	Configurator {/* {{{ */
		bool	getProperty( const cmd_tokens_t& );
		bool	setProperty( const cmd_tokens_t& );
		bool	forceSetProperty( const cmd_tokens_t& );
	};/* }}} */

	struct	Handler {/* {{{ */
		bool	changeRemovableMedia( const cmd_tokens_t& );
		bool	configure( const cmd_tokens_t& );
	};/* }}} */

	struct	CommandLine {/* {{{ */
		cmd_tokens_t	processCommandLine( const std::string );
		void				getInfo();
	};/* }}} */

	TCPsocket		_server,_client;
	IPaddress		_hostIP;
	SDL_Thread		*_pThread;

	std::atomic< bool >	_isRunning;

	const std::string	_prompt;
	const CommandList	_commands= {/* {{{ */
		{ "kill",		CB_Quit,				"kill DOSBox-X instance ungracefully."},
		{ "info",		CB_GetInfo,			"show current system information." },
		{ "config",		CB_Config,			"get and set machine values." },
		{ "change",		CB_ChangeMedia,	"change CD, format: change <drive_letter> <image_filepath>." },
		{ "help",		CB_Help,				"list available commands." },
	};/* }}} */

	/* Callbacks are paired with simple string */
	static void CB_Quit( void *data ){/* {{{ */
		cmd_tokens_t& cmdline= *reinterpret_cast< cmd_tokens_t* >( data );
		DoKillSwitch();
	}/* }}} */

	static void CB_GetInfo( void *data ) {/* {{{ */
		cmd_tokens_t& cmdline= *reinterpret_cast< cmd_tokens_t* >( data );
		CommandLine cli;
		cli.getInfo();
	}/* }}} */

	static void CB_Config( void *data ) {/* {{{ */
		cmd_tokens_t& cmdline= *reinterpret_cast< cmd_tokens_t* >( data );

		Handler the_hand;
		the_hand.configure( cmdline );
	}/* }}} */

	static void CB_ChangeMedia( void *data ) {	/* {{{ */
		cmd_tokens_t& cmdline= *reinterpret_cast< cmd_tokens_t* >( data );
		Handler the_hand;
		the_hand.changeRemovableMedia( cmdline );
	}/* }}} */

	static void CB_Help( void *data ) {     /* {{{ */
		cmd_tokens_t& cmdline= *reinterpret_cast< cmd_tokens_t* >( data );
		Monitor& m= Monitor::OpenMonitor();
		if( cmdline.size() >1 ) {
			auto command= std::find_if( m._commands.begin(),m._commands.end(),
				[ & ]( const CommandExec &line ) {
					return line.command ==cmdline.at( 1 ); } );

			if( m._commands.end()==command ) m.messageClient( "Unrecognized command.\n" );
			else m.messageClient( command->description+"\n" );

		} else {
				std::string help= "Commands: ";
				for( auto cmd:m._commands )
					help+= cmd.command+", ";

				help.erase( help.size()-1 );
				help.at( help.size()-1 )= '\n';

				m.messageClient( help+"\nType \'help\' <command> for more information.\n" );
		}
	}/* }}} */

	/* Callback for threading. */
	static int CB_Listen( void *tData ) {/* {{{ */
		auto& m= *reinterpret_cast< Monitor* >( tData );
		
		m.updateServer();

		return EXIT_SUCCESS;
	}/* }}} */

	/* Main program functions */
	void	prompt( TCPsocket tClient ){/* {{{ */
		SDLNet_TCP_Send( tClient,_prompt.c_str(),_prompt.length() );
	}/* }}} */
	
	bool	startServer( int tPort ) {/* {{{ */
		if( int retval= SDLNet_Init() )
			LOG_MSG( "SDLNet returned: %d\n",retval );

		if( _server )
			onServerClose();

		if ( SDLNet_ResolveHost( &_hostIP,nullptr,tPort ) ) {
	       		fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
			return false;
		}

		_server= SDLNet_TCP_Open( &_hostIP );

		/* This is likely not the best place to put this "fix"
         but it sorts an issue where DOSBox-X would crash
			if selecting the dynamic core if DOSBox-X was 
			started with any other. */
		#if (C_DYNAMIC_X86)
		CPU_Core_Dyn_X86_Cache_Init( true );
		#endif
		#if (C_DYNREC)
		CPU_Core_Dynrec_Cache_Init( true );
		#endif

		return true;
	}/* }}} */

	TCPsocket	connectClient() {/* {{{ */
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
	}/* }}} */

	void	closeSocket( TCPsocket& tSocket ) {/* {{{ */
		SDLNet_TCP_Close( tSocket );
		tSocket= nullptr;
	}/* }}} */

	void	closeClientSocket() {/* {{{ */
		if( _client ) closeSocket( _client );
	}/* }}} */

	void	updateServer( ) {/* {{{ */
		while( _isRunning ) {
			if( nullptr ==_client ) {
				_client= connectClient();
				messageClient( std::string( "DOSBox-X " )+VERSION+" monitor - type \'help\' for a list of commands\n" );
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
				CommandLine  cli;
				cmd_tokens_t user_input= cli.processCommandLine( buffer );

				if( user_input.empty() ){ continue; }

				auto processCommand= std::find_if( _commands.begin(),_commands.end(),
					[ & ]( const CommandExec& item ){	
							return item.command == user_input.at( 0 );});
				
				if( processCommand !=_commands.end() )
					processCommand->exec( &user_input );
			}	
		}
		onServerClose();
	}/* }}} */

	void	onServerClose() {/* {{{ */
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
	}/* }}} */

	/* CTOR */
	Monitor():
	_server( nullptr ),_client( nullptr ),
	_hostIP( IPaddress{ 0,0 } ),_pThread( nullptr ),
	_isRunning( false ),_prompt( "(dosbox-x) " ) {
		/* Empty */
	}

	public:

	~Monitor() {/* {{{ */
		onServerClose();
	}/* }}} */

	static void	messageClient( std::string tMessage ) {/* {{{ */
		Monitor &m= OpenMonitor( );
		SDLNet_TCP_Send( m._client,tMessage.c_str(),tMessage.size() );
	}/* }}} */

	static	Monitor& OpenMonitor( int tPort=4444 ) {/* {{{ */
		static Monitor m;

		if( false ==m._isRunning ) {
			if( m._pThread ) m.onServerClose();

			m._isRunning= m.startServer( tPort );

			if ( true ==m._isRunning ){

				#if defined(C_SDL2) && C_SDL2
				m._pThread= SDL_CreateThread( CB_Listen,"dosbox-x monitor",&m );
				SDL_DetachThread( m._pThread );
				#else
				m._pThread= SDL_CreateThread( CB_Listen,(void*)&m );
				#endif
			}
		}
		return m;
	}/* }}} */
};

#endif  // _MONITOR_CONSOLE_H
