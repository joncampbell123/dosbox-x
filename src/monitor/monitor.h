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

#ifdef LINUX
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#endif
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
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
#define UDS_BACKLOG 5

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
	TCPsocket tcp;
	operator TCPsocket&() { return tcp; }
	SocketUnion &operator=( TCPsocket lhs ){ tcp= lhs; return *this; }
	#ifdef LINUX
	int uds;
	operator int&() { return uds; }
	SocketUnion &operator=( int lhs ){ uds= lhs; return *this; }
	#endif
};
union SocketAddrUnion {
	IPaddress ip;
	operator IPaddress &() { return ip; }
	SocketAddrUnion &operator=( IPaddress lhs ){ ip= lhs; return *this; }
	#ifdef LINUX
	sockaddr_un domain;
	operator sockaddr_un &() {return domain; }
	SocketAddrUnion &operator=( sockaddr_un lhs ){ domain= lhs; return *this; }
	#endif
};

/* Type struct is flattened as each branch (TCP/UDS) requires completely different 
   types meaning this should be safe, for the moment. Hacky? Yes. */
struct socket_t{
	int type; SocketUnion sock; SocketAddrUnion address;

	operator bool () { return sock !=0; }
	operator TCPsocket &() { return sock.tcp; }
	operator IPaddress &() { return address.ip; }

	void reset() {
		memset( &sock,0,sizeof( sock ) );
		memset( &address,0,sizeof( address ) );
	}
	TCPsocket &as_tcp() { return sock.tcp; }
	socket_t &operator=( TCPsocket lhs ) {
		reset();
		type= 0;
		sock.tcp= lhs;
		return *this;
	}
	IPaddress &ip= address.ip;
	TCPsocket &tcp= sock.tcp;

	#ifdef LINUX
	int &as_int() { return sock.uds; }
	socket_t &operator=( int lhs ) {
		reset();
		type= 1;
		sock.uds= lhs;
		return *this;
	}
	sa_family_t &sun_family= address.domain.sun_family;
	char ( &sun_path )[ sizeof( address.domain.sun_path ) ]= address.domain.sun_path;
	sockaddr_un &fd= address.domain;
	#endif
};
// }}} 

class Monitor { /* {{{ */
	// Parser and programs constructs {{{
	struct	Configurator {/* {{{ */
		void	run( Monitor &,const cmd_tokens_t& );
		bool	getProperty( Monitor &,const cmd_tokens_t& );
		bool	setProperty( Monitor &,const cmd_tokens_t& );
	};/* }}} */

	struct	Programs {/* {{{ */
		bool	quitMainInstance( Monitor&,const cmd_tokens_t& );
		bool	changeRemovableMedia( Monitor&,const cmd_tokens_t& );
		bool	configure( Monitor&,const cmd_tokens_t& );
		bool	displayInfo( Monitor&,const cmd_tokens_t& );
		bool	helpMe( Monitor&,const cmd_tokens_t& );
	};/* }}} */

	struct	CommandLine {/* {{{ */
		cmd_tokens_t	processCommandLine( const std::string );
		void				getInfo();
	};// }}} }}} 

	// Vars {{{
	static	Monitor		*ptr_monitor;
	const std::string	_prompt;

	socket_t			_server,_client;
	SDL_Thread			*_pThread;
	Programs			_run;

	std::atomic< bool >	_isRunning;

	typedef bool ( Monitor::Programs::*p_programs_fn )( Monitor &,const cmd_tokens_t& );
	typedef bool ( Monitor::Configurator::*p_configurator_fn )( Monitor &,const cmd_tokens_t& );
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
		#ifdef LINUX
		send( tClient,_prompt.c_str(),_prompt.length(),MSG_EOR );
		#endif
	}/* }}} */
	
	bool	startServer( std::string tSockPath ) {/* {{{ */
		#ifdef LINUX
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
		if ( listen( _server.sock,UDS_BACKLOG ) ==-1 ) {
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
		#else
		return EXIT_FAILURE;
		#endif
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
		/* the port may be in use, for example by another instance */
		if( !_server ) {
			LOG_MSG( "MONITOR: cannot listen on TCP port %d: %s\n",tPort,SDLNet_GetError() );
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}/* }}} */

	/* The TCP monitor accepts only local connections. SDL_net cannot bind a server to the
	 * loopback address (it listens on all interfaces), so check the peer of each connection. */
	static bool	isLoopbackPeer( TCPsocket tSocket ) {/* {{{ */
		IPaddress *peer= SDLNet_TCP_GetPeerAddress( tSocket );
		if( nullptr ==peer ) return false;

		const Uint32 host= SDLNet_Read32( &peer->host );
		if( ( host >>24 ) ==127 ) return true;

		LOG_MSG( "MONITOR: refused a connection from %u.%u.%u.%u, only local connections are accepted.\n",
			(unsigned int)( host >>24 ),(unsigned int)( ( host >>16 ) &0xFF ),
			(unsigned int)( ( host >>8 ) &0xFF ),(unsigned int)( host &0xFF ) );
		return false;
	}/* }}} */

	void	closeSocket( TCPsocket& tSocket ) {/* {{{ */
		SDLNet_TCP_Close( tSocket );
		tSocket= nullptr;
	}/* }}} */

	void	closeClientSocket() {/* {{{ */
		if( _client.sock.tcp ) closeSocket( _client.sock.tcp );
	}/* }}} */

	bool updateUDSServer() {/* {{{ */
		#ifdef LINUX
		int bytes_received= 0;
		while ( _isRunning ) {
			/* the descriptor itself: socket_t converts to int through operator bool */
			_client= accept( _server.sock.uds,nullptr,nullptr );
			char buffer[ INPUT_LENGTH ];

			messageClient( _client,std::string( "DOSBox-X " )+VERSION+" monitor - type \'help\' for a list of commands\n" );

			prompt( _client.as_int() );

			memset( buffer,'\0',sizeof( char )*INPUT_LENGTH );
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
					( _run.*( processCommand->exec ) )( *ptr_monitor,user_input );

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
		#else
		return EXIT_FAILURE;
		#endif
	}/* }}} */

	bool	updateTCPServer( ) {/* {{{ */
		while( _isRunning ) {
			if( nullptr ==_client ) {
				/* Blocking loop while we wait for a client to connect.*/
				while( nullptr ==_client ){
					_client= SDLNet_TCP_Accept( _server );			
					if( nullptr !=_client ) {
						if( !isLoopbackPeer( _client.as_tcp() ) ) {
							closeSocket( _client.as_tcp() );
							continue;
						}
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
					( _run.*( processCommand->exec ) )( *ptr_monitor,user_input );
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
		#ifdef LINUX
		else if( DBX_SOCKET_UDS ==_client.type ) {
			if( _server.sock !=-1 ) {
				shutdown( _server.sock,SHUT_RDWR );
				close( _server.sock );
				_server.sock= -1;
			}
		}
		#endif
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
		#ifdef LINUX
		if( tClient.type >0 ) {
			send( tClient.sock.uds,tMessage.c_str(),tMessage.size(),0 );
			return;
		}
		#endif
		SDLNet_TCP_Send( tClient,tMessage.c_str(),tMessage.size() );
	}/* }}} */

	static	Monitor& OpenMonitor( std::string tLocation="" ) {/* {{{ */
		static Monitor m;

		if( false ==m._isRunning ) {
			/* "on" is the default port; otherwise a port number, or a socket path */
			if( !tLocation.compare( "on" ) ) tLocation= "4444";
			bool isPort= !tLocation.empty();
			for( char c:tLocation )
				if( !std::isdigit( (unsigned char)c ) ) {
					isPort=false;
					break;
				}
			int ( *run )( void* )= nullptr;

			if( m._pThread ) m.onServerClose();

			if( isPort ) {
				const unsigned long port= strtoul( tLocation.c_str(),nullptr,10 );
				if( port ==0 || port >65535 )
					LOG_MSG( "MONITOR: invalid port number %s.\n",tLocation.c_str() );
				else if( EXIT_SUCCESS ==m.startServer( (int)port ) )
					run= CB_ListenTCP;
			}
			#ifdef LINUX
			else if( EXIT_SUCCESS ==m.startServer( tLocation ) ) {
				run= CB_ListenUDP;
			}
			#else
			else {
				LOG_MSG( "MONITOR: a socket path is only supported on Linux, use a port number.\n" );
			}
			#endif

			/* no listening thread without a server to listen on */
			if( nullptr !=run ) {
				m._isRunning= true;
				#if defined(C_SDL2) && C_SDL2
				m._pThread= SDL_CreateThread( run,"dosbox-x monitor",&m );
				SDL_DetachThread( m._pThread );
				#else
				m._pThread= SDL_CreateThread( run,(void*)&m );
				#endif
			}
		}

		m.ptr_monitor= &m;
		return m;
	}/* }}} */

	static bool CloseMonitor() {/* {{{ */
		Monitor &m= OpenMonitor();
		return m._isRunning= false;
	}/* }}} */
}; /* }}} */

#endif // C_MONITOR
#endif // _MONITOR_CONSOLE_H
