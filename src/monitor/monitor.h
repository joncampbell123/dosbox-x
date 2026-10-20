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

#include "dosbox.h"

#if C_MONITOR

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
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

#include "logging.h"

#define INPUT_LENGTH 1024
#define BUFFER_SIZE 1024

enum { DBX_SOCKET_TCP= 0,DBX_SOCKET_UDS };

// extern {{{
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
// }}}

// global typedef {{{
typedef std::vector< std::string > dinfo_t;
typedef std::vector< std::string > cmd_tokens_t;
typedef std::vector< dinfo_t > ddesc_t;
// }}}

/* type socket_t declaration {{{ */
union SocketUnion { 
	TCPsocket tcp; int uds;

	operator TCPsocket&() { return tcp; }
	operator int&() { return uds; }
	SocketUnion &operator=( TCPsocket lhs ){ tcp= lhs; return *this; }
	SocketUnion &operator=( int lhs ){ uds= lhs; return *this; }
};
union SocketAddrUnion {
	IPaddress ip; sockaddr_un domain;

	operator IPaddress &() { return ip; }
	operator sockaddr_un &() {return domain; }
	SocketAddrUnion &operator=( IPaddress lhs ){ ip= lhs; return *this; }
	SocketAddrUnion &operator=( sockaddr_un lhs ){ domain= lhs; return *this; }
};

/* Type struct is flattened as each branch (TCP/UDS) requires completely different 
   types meaning this should be safe, for the moment. Hacky? Yes. */
struct socket_t{
	int type; SocketUnion sock; SocketAddrUnion address;

	operator bool () { return static_cast< int >( sock ) >0; }
	operator TCPsocket &() { return sock.tcp; }
	operator int &() { return sock.uds; }
	operator IPaddress &() { return address.ip; }

	void reset() {
		memset( &sock,0,sizeof( sock ) );
		memset( &address,0,sizeof( address ) );
	}

	TCPsocket &as_tcp() { return sock.tcp; }
	int &as_int() { return sock.uds; }
	socket_t &operator=( TCPsocket lhs ) {
		reset();
		type= 0;
		sock.tcp= lhs;
		return *this;
	}
	socket_t &operator=( int lhs ) {
		reset();
		type= 1;
		sock.uds= lhs;
		return *this;
	}

	sa_family_t &sun_family= address.domain.sun_family;
	char ( &sun_path )[ sizeof( address.domain.sun_path ) ]= address.domain.sun_path;
	sockaddr_un &fd= address.domain;
	IPaddress &ip= address.ip;
	TCPsocket &tcp= sock.tcp;
};
// }}} 

class Monitor { /* {{{ */
	// Parser and programs constructs {{{
	struct	Configurator {/* {{{ */
		void	run( const cmd_tokens_t& );
		bool	getProperty( const cmd_tokens_t& );
		bool	setProperty( const cmd_tokens_t& );
	};/* }}} */

	struct	Programs {/* {{{ */
		bool	quitMainInstance( const cmd_tokens_t& );
		bool	changeRemovableMedia( const cmd_tokens_t& );
		bool	configure( const cmd_tokens_t& );
		bool	displayInfo( const cmd_tokens_t& );
		bool	helpMe( const cmd_tokens_t& );
	};/* }}} */

	struct	CommandLine {/* {{{ */
		cmd_tokens_t	processCommandLine( const std::string );
		void				getInfo();
	};// }}} }}} 

	// Vars {{{
	const std::string	_prompt;

	socket_t			_server,_client;
	SDL_Thread		*_pThread;
	Programs			_run;

	std::atomic< bool >	_isRunning;

	typedef bool ( Monitor::Programs::*p_programs_fn )( const cmd_tokens_t& );
	typedef bool ( Monitor::Configurator::*p_configurator_fn )( const cmd_tokens_t& );
	typedef int ( Monitor::*p_listener_fn )( void* );
	typedef struct {
		std::string		command;
		std::string		description;
		p_programs_fn	exec;
	} CommandExec;
	typedef std::vector< CommandExec > CommandList;
	const CommandList	*_commands;
	// }}}

	/* Callbacks for threading. {{{ */
	static int CB_ListenUDP( void *tData ) { /* {{{ */
		auto& m= *reinterpret_cast< Monitor* >( tData );
		
		return m.updateUDSServer();
	}/* }}} */
	
static int CB_ListenTCP( void *tData ) {/* {{{ */
		auto& m= *reinterpret_cast< Monitor* >( tData );
		
		return m.updateTCPServer();
	}/* }}} }}} */

	void	prompt( TCPsocket &tClient ){/* {{{ */
			SDLNet_TCP_Send( tClient,_prompt.c_str(),_prompt.length() );
	}/* }}} */
	
	void	prompt( int tClient ){/* {{{ */
			send( tClient,_prompt.c_str(),_prompt.length(),MSG_EOR );
	}/* }}} */
	
	bool	startServer( std::string tSockPath ) {/* {{{ */
#define BACKLOG 5
		_server.sock= socket( AF_UNIX,SOCK_STREAM,0 );
		if( -1 ==_server.sock ) {
			LOG_MSG( "MONITOR: issue with socket domain addressing.\n");
			return EXIT_FAILURE;
		}
		if ( tSockPath.length()  >sizeof( _server.sun_path )-1 ) {
			LOG_MSG( "MONITOR: Cannot open server because the socket path is too long: %s.\n",tSockPath.c_str() );
			return EXIT_FAILURE;
		}
		if ( remove( tSockPath.c_str() ) ==-1 && errno !=ENOENT ) {
			LOG_MSG( "MONITOR: remove %s\n",tSockPath.c_str() );
			return EXIT_FAILURE;
		}

		memset( &_server.fd,0,sizeof( _server.fd ) );
		_server.sun_family= AF_UNIX;
		strncpy( _server.sun_path,tSockPath.c_str(),sizeof( _server.sun_path ) - 1);
		if ( bind( _server.sock,( struct sockaddr* ) &_server.fd,sizeof( _server.fd ) ) ==-1) {
			LOG_MSG( "MONITOR: Could not bind server to socket." );
			return EXIT_FAILURE;
		}
		if ( listen( _server.sock,BACKLOG ) ==-1 ) {
			LOG_MSG( "MONITOR: listen failure" );
			return EXIT_FAILURE;
		}
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
//		_server.type= DBX_SOCKET_UDS;
		return EXIT_SUCCESS;
	}/* }}} */

	bool	startServer( int tPort ) {/* {{{ */
		if( int retval= SDLNet_Init() ) {
			LOG_MSG( "SDLNet returned: %d\n",retval );
			return EXIT_FAILURE;
		}
		if( _server )
			onServerClose();

		if ( ( SDLNet_ResolveHost( &_server.ip,nullptr,tPort ) ) ) {
			LOG_MSG( "SDLNet_ResolveHost: %s\n", SDLNet_GetError() );
			return EXIT_FAILURE;
		}
		_server= SDLNet_TCP_Open( &_server.ip );
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
		return EXIT_SUCCESS;
	}/* }}} */

	void	closeSocket( TCPsocket& tSocket ) {/* {{{ */
		SDLNet_TCP_Close( tSocket );
		tSocket= nullptr;
	}/* }}} */

	void	closeClientSocket() {/* {{{ */
		if( _client.sock.tcp ) closeSocket( _client.sock.tcp );
	}/* }}} */

	bool updateUDSServer() {/* {{{ */
		int bytes_received= 0;
		while ( _isRunning ) {
			_client= accept( _server,nullptr,nullptr );
			char buffer[ INPUT_LENGTH ];

			messageClient( _client,std::string( "DOSBox-X " )+VERSION+" monitor - type \'help\' for a list of commands\n" );

			prompt( _client.as_int() );

			memset(buffer,'\0',sizeof( char )*INPUT_LENGTH);
			while ( ( bytes_received= read( _client.sock,buffer,INPUT_LENGTH ) ) >0 ) {
				if( bytes_received && '\0' !=buffer[ bytes_received-1 ] )
					buffer[ bytes_received-1 ] = '\0';

				prompt( _client.as_int() );		
				
				CommandLine cli;
				cmd_tokens_t user_input= cli.processCommandLine( buffer );

				if( user_input.empty() ) continue;

				auto processCommand= std::find_if( _commands->begin(),_commands->end(),
					[ & ]( const CommandExec& item ){	
							return item.command == user_input.at( 0 );});

				if( processCommand !=_commands->end() ){
					( _run.*( processCommand->exec ) )( user_input );

					prompt( _client.as_int() );		
				}
			}

			if( bytes_received ==-1 ) {
				return EXIT_FAILURE;
			}

			if( close( _client.sock ) ==-1 ) {
				return EXIT_FAILURE;
			}
		}
		onServerClose();
		return EXIT_SUCCESS;
	}/* }}} */

	bool	updateTCPServer( ) {/* {{{ */
		while( _isRunning ) {
			if( nullptr ==_client ) {
				/* Blocking loop while we wait for a client to connect.*/
				while( nullptr ==_client ){
					_client= SDLNet_TCP_Accept( _server );			
					if( nullptr !=_client ) {
						LOG_MSG( "MONITOR client connected.\n" );
						break;
					}
					SDL_Delay( 50 );
				}
				messageClient( _client,std::string( "DOSBox-X " )+VERSION+" monitor - type \'help\' for a list of commands\n" );
			}
			prompt( _client.as_tcp() );

			char buffer[ INPUT_LENGTH ];
			memset(buffer,'\0',sizeof( char )*INPUT_LENGTH);

			int bytes_received= SDLNet_TCP_Recv( _client,buffer,INPUT_LENGTH );	

			if( bytes_received && '\0' !=buffer[ bytes_received-1 ] )
				buffer[ bytes_received-1 ] = '\0';
			
			if( bytes_received <=0 ) {
				if( _client ) closeSocket( _client );
				LOG_MSG( "MONITOR client disconnected unexpectedly or connection lost.\n" );
			}
			else {

				CommandLine  cli;
				cmd_tokens_t user_input= cli.processCommandLine( buffer );

				if( user_input.empty() ){ continue; }

				auto processCommand= std::find_if( _commands->begin(),_commands->end(),
					[ & ]( const CommandExec& item ){	
							return item.command == user_input.at( 0 );});

				if( processCommand !=_commands->end() )
					( _run.*( processCommand->exec ) )( user_input );
			}
		}
		onServerClose();
		return EXIT_SUCCESS;
	}/* }}} */

	void	initCommands() {/* {{{ */
		using Prog= Monitor::Programs;

		_commands= new CommandList{
			{ "kill","kill DOSBox-X instance ungracefully.\n",p_programs_fn( &Prog::quitMainInstance ) },
			{ "info","show current system information.",p_programs_fn( &Prog::displayInfo ) },
			{ "config",	"get and set machine values." ,p_programs_fn( &Prog::configure ) },
			{ "change",	"change CD, format: change <drive_letter> <image_filepath>.",p_programs_fn( &Prog::changeRemovableMedia ) },
			{ "help","list available commands.",p_programs_fn( &Prog::helpMe ) },
		};
	}/* }}} */

	void	onServerClose() {/* {{{ */
		if( DBX_SOCKET_TCP ==_client.type ) {
			if( _isRunning )
				_isRunning= false;

			if( _client ) SDLNet_TCP_Close( _client );
			if( _server ) SDLNet_TCP_Close( _server ); 
		}
		else if( DBX_SOCKET_UDS ==_client.type ) {
			if( _server.sock !=-1 ) {
				shutdown( _server.sock,SHUT_RDWR );
				close( _server.sock );
				_server.sock= -1;
			}
		}

		if( _commands ){  delete _commands; _commands=nullptr; };
		if( _pThread ){
			
			#if defined(C_SDL_NET) && C_SDL_NET
			int status;
			SDL_WaitThread( _pThread,&status );
			#endif

			_pThread= nullptr;
		}
	}/* }}} */

	// Constructors {{{
	Monitor():
	_prompt( "(dosbox-x) " ),
	_server{ },_client{ },
	_pThread( nullptr ),
	_run(),_isRunning( false ) {
		initCommands();
	}

	Monitor( const Monitor& )= delete;
	Monitor& operator=( const Monitor& )= delete;
	// }}}

	public:

	static void	messageClient( socket_t &tClient, std::string tMessage ) {/* {{{ */
		if( tClient.type >0 ) {
			send( tClient,tMessage.c_str(),tMessage.size(),0 );
			return;
		}
		SDLNet_TCP_Send( tClient,tMessage.c_str(),tMessage.size() );
	}/* }}} */

	static	Monitor& OpenMonitor( std::string tLocation="" ) {/* {{{ */
		static Monitor m;

		if( false ==m._isRunning ) {
			bool isPort= true;
			if( tLocation.compare( "on" ) ) tLocation= "4444"; /* If the monitor is not running we'll give it a default state on localhost. */
			else {
				for( char c:tLocation )
					if( !std::isdigit( c ) ) {
						isPort=false;
						break;
					}
			}	
			int ( *run )( void* );

			if( m._pThread ) m.onServerClose();

			#ifdef WIN32
			if( false ==isPort &&  EXIT_SUCCESS ==m.startServer( 4444  ) )
				run=CB_ListenTCP;
			#else
			if( isPort && ( EXIT_SUCCESS ==m.startServer( std::stoi( tLocation ) ) ) ) {
				run= CB_ListenTCP;
			}
			else if( !isPort && EXIT_SUCCESS ==m.startServer( tLocation ) ) {
				run= CB_ListenUDP;
			}
			else
				return m;
			#endif

			m._isRunning= true;
			#if defined(C_SDL2) && C_SDL2
			m._pThread= SDL_CreateThread( run,"dosbox-x monitor",&m );
			SDL_DetachThread( m._pThread );
			#else
			m._pThread= SDL_CreateThread( run,(void*)&m );
			#endif	
		}
		return m;
	}/* }}} */

	static bool CloseMonitor() {/* {{{ */
		Monitor &m= OpenMonitor();
		return m._isRunning= false;
	}/* }}} */
}; /* }}} */

#endif // C_MONITOR
#endif // _MONITOR_CONSOLE_H
