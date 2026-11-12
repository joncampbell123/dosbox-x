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


#include "monitor.h"

#if C_MONITOR

#include "control.h"
#include "programs.h"
#include "drives.h"
#include "bios_disk.h"

Monitor *Monitor::ptr_monitor= nullptr;

extern void runImgmount( const char * );
extern bool wpcolon;
extern void MSCDEX_SetCDInterface( int,int );
extern const char* MSG_GetUTF8(const char* msg);
extern char lastmount;

/* Media driver adapted from ../dos/dos_programs.cpp */
#if defined(MACOSX)
extern const char* MSG_GetUTF8(const char* msg);
constexpr auto MSGX=MSG_GetUTF8;
#else
constexpr auto MSGX=MSG_Get;
#endif
constexpr int MEDIA_ID=0xF8;

extern bool wpcolon;
void MSCDEX_SetCDInterface( int,int );
extern char lastmount;
void ResetSystem( bool );
void RebootGuest( bool );
extern bool bootvm;

std::atomic< void(*)() > Monitor_RunProgram{nullptr};

// Monitor::Configurator {{{ 
static std::string CONFIG_USAGE_MONITOR= { // {{{
"The DOSBox-X socket pipeline configurator\n"\
"**Warning this feature is mostly untested and may cause the program to crash.**\n"\
"**                           PROCEED WITH CAUTION                            **\n"\
"-get \"section property\" returns the value of the property (also to %%CONFIG%%).\n"\
"-set \"section property=value\" sets the value of the property.\n"\
};// }}}

void Monitor::Configurator::run( Monitor &tM,const cmd_tokens_t& tUser ) { /* {{{ */
	using Config= Monitor::Configurator;
	Monitor& m= tM;
	if( tUser.size() <=1 ) {
		m.messageClient( m._client,std::string( CONFIG_USAGE_MONITOR ) );	
		return;
	}
	struct command {
		std::string			param;
		p_configurator_fn	execute;
	};
	const std::array< command,2 > params= { {
		{ "-get",	p_configurator_fn( &Config::getProperty ) },
		{ "-set",	p_configurator_fn( &Config::setProperty ) },
	} };

	auto p= std::find_if( params.begin(),params.end(),
		[ &tUser ]( const command& user_req ) {
		return !user_req.param.compare( tUser.at( 1 ) ); } );

	if( p ==params.end() ) {
		m.messageClient( m._client,CONFIG_USAGE_MONITOR );	
	}
	else if( ( this->*( p->execute ) )( tM,tUser ) )
		m.messageClient( m._client,"( config ) Unable to process commandline options.\n" );
}/* }}} */

bool Monitor::Configurator::getProperty( Monitor &tM,const cmd_tokens_t& tUser ) {/* {{{ */
	Monitor& m= tM;
	if( tUser.size() <3 ) {
		m.messageClient( m._client,std::string( MSG_Get( "PROGRAM_CONFIG_GET_SYNTAX" ) ) );
		return EXIT_FAILURE;
	}
	Section_prop *config_section= static_cast< Section_prop* >( control->GetSection( tUser.at( 2 ) ) );
	if( nullptr ==config_section ) {
		m.messageClient( m._client,std::string( MSG_Get( "PROGRAM_CONFIG_PROPERTY_ERROR" ) ) );
		return EXIT_FAILURE;
	}
	if( tUser.size() <4 ) {
		for( auto prop:config_section->properties ) {
			m.messageClient( m._client,prop->propname + "=" + prop->GetValue().ToString()+"\n" );
		}
		return EXIT_SUCCESS;
	}
	Property *property= config_section->Get_prop( tUser.at( 3 ) );
	if( nullptr ==property ) {
		m.messageClient( m._client,std::string( MSG_Get( "PROGRAM_CONFIG_PROPERTY_ERROR" ) ) );
	}
	
	m.messageClient( m._client,property->GetValue().ToString()+"\n" );

	return EXIT_SUCCESS;
}/* }}} */

#include "control.h"
#include "../misc/programs.cpp"

bool Monitor::Configurator::setProperty( Monitor &tM,const cmd_tokens_t& tUser ) {/* {{{ */
	Monitor& m= tM;
	if( tUser.size() <5 ) {
		m.messageClient( m._client,std::string( MSG_Get( "PROGRAM_CONFIG_SET_SYNTAX" ) ) );
		return EXIT_FAILURE;
	}
	Section_prop *config_section= static_cast< Section_prop* >( control->GetSection( tUser.at( 2 ) ) );
	if( nullptr ==config_section ) {
		m.messageClient( m._client,std::string( MSG_Get( "PROGRAM_CONFIG_SET_SYNTAX" ) ) );
		return EXIT_FAILURE;
	}
	Property *property= config_section->Get_prop( tUser.at( 3 ) );
	if( nullptr ==property ) {
		m.messageClient( m._client,std::string( MSG_Get( "PROGRAM_CONFIG_PROPERTY_ERROR" ) ) );
		return EXIT_FAILURE;
	}

	std::string inputline= tUser.at( 3 )+"="+tUser.at( 4 );

	config_section->HandleInputline( inputline );
	ApplySetting( tUser.at( 2 ),inputline,false );

	return EXIT_SUCCESS;
}/* }}} */ /* }}} */

// Monitor::Programs {{{
bool Monitor::Programs::dosboxXManagement( Monitor &tM,const cmd_tokens_t &tCommands ){/* {{{ */

	if( !tCommands.at( 0 ).compare( "system_reboot" ) ) {
		RebootGuest( true );
	}
	if( !tCommands.at( 0 ).compare( "kill" ) ) {
		tM._isRunning= false;
		DoKillSwitch();
	}
	if( !tCommands.at( 0 ).compare( "system_restart" ) ) {
		ResetSystem( true );
	}
	return EXIT_FAILURE;
}/* }}} */

bool	Monitor::Programs::displayInfo( Monitor &tM,const cmd_tokens_t& ) {/* {{{ */
	Monitor& m= tM;
	Section* sec=control->GetSection( "cpu" );
		
	ddesc_t info= {};
	info.emplace_back( dinfo_t{ std::string( "CPU:" ),sec->GetPropValue( "cputype" ) } );
	info.emplace_back( dinfo_t{ std::string( "Core:" ),sec->GetPropValue( "core" ) } );
	info.emplace_back( dinfo_t{ std::string( "Cycles:" ),sec->GetPropValue( "cycles" ) } );

	messageClient( m._client,"\nSystem Info:\n" );
	for( auto i:info ) {
		messageClient( m._client,i.at( 0 )+"\t\t"+i.at( 1 )+"\n" );
	}
	sec= control->GetSection( "dosbox" );
	messageClient( m._client,"\nMemory:\nRAM:\t\t" +sec->GetPropValue( "memsize" )+"Mb\n" );
		
	messageClient( m._client,"\nDrive(s):\n" );
	ddesc_t drive_list= {};
	std::string sstr_token;
/* Lambda functions for getting Drive Info {{{*/
	auto getDOSDriveInfo= [ & ]( int tIt ) {
		dinfo_t drive= {};
		drive.push_back( std::string( 1,char( 'A'+tIt ) ) );

		std::string drive_info( Drives[ tIt ]->GetInfo() );
		std::size_t dir_pos= drive_info.find_first_of( "/\\" );

		if( dir_pos !=std::string::npos ) {
			drive.push_back( drive_info.substr( 0,dir_pos ) );
			drive.push_back( drive_info.substr( dir_pos ) );
		}
		drive.push_back( drive_info );
		return drive;
	};
	
	auto getImageDiskInfo= [ & ]( int tIt ) {
		dinfo_t drive= {};
		drive.push_back( std::string( 1,char( 'A'+tIt ) ) );

		std::string drive_info( imageDiskList[ tIt ]->diskname );
		drive.push_back( "empty"  );

		if( !drive_info.empty() ) drive.push_back( drive_info );
		return drive;
	};
/* }}} */
	for( int i= 0; i <2; ++i )
		if ( imageDiskList[ i ] && Drives[ i ] ) {
			dinfo_t drive= getDOSDriveInfo( i );
			drive_list.push_back( drive );
		}
		else if( imageDiskList[ i ] ) {
			dinfo_t drive= getImageDiskInfo( i );
			drive_list.push_back( drive );
		}

	for( int i= 2; i <26; ++i )
		if( Drives[ i ] ) {
			dinfo_t drive= getDOSDriveInfo( i );
			drive_list.push_back( drive );
		}

	for( auto drv:drive_list ) {
		std::string message= {};
		message+=drv.at( 0 )+":\\\t";
		if( drv.size() >1 ) { if( drv.at( 1 ).at( drv.at( 1 ).size()-1 ) ==  ' ' ) drv.at( 1 ).erase( ( drv.at( 1 ).size()-1 ) );
		message+= "("+drv.at( 1 )+")"; 
		/* 32 columns is enough for any drive type divided by the size of a tab in spaces (8.)  */
		int tabwise= ( 32-drv.at( 1 ).size() )/8,i= 0;
		do {
			message+="\t";
		} while ( ++i <tabwise ); }
		if( drv.at( 1 ).size()<8 ) message+="\t";
		if( drv.size() >2 ) message+= drv.at( 2 );
		message+="\n";
		messageClient( m._client,message );
	}
	return EXIT_SUCCESS;
}	/* }}} */

int	createISODrives( const char tDriveLetter,const std::vector< std::string > tPaths,std::vector< DOS_Drive * > &tISOs ) { /* {{{ */
	MSCDEX_SetCDInterface( CDROM_USE_SDL, -1 );
	std::vector<std::string> options= { "readonly" };
	for( auto drive:tPaths ) {
		int error= -1;
		DOS_Drive* newDrive = new isoDrive( tDriveLetter,
			wpcolon&&drive.length() >1&&drive.c_str()[0]==':'?
				drive.c_str()+1 : drive.c_str(),
				MEDIA_ID,error,options );
		tISOs.push_back( newDrive );
		if( error ) {
			for( DOS_Drive *drive: tISOs ) 
				delete drive;

			return error;
		}
	}
	return EXIT_SUCCESS;
}/* }}} */

#if (defined(WIN32) || defined(LINUX) || defined(BSD)) && !defined(OSFREE)
int	createFloppyDrives( const unsigned int tDriveLetter,const std::vector< std::string >tPaths,std::vector< fatDrive *> &tDisks ) {/* {{{ */
	int index= 0;
	std::vector<std::string> fd_options= { "readonly" };
	for( auto drive:tPaths ) {
		fatDrive *newDrive = new fatDrive( drive.c_str(),0,0,0,0,fd_options );

		if( !newDrive->created_successfully ) {
			for( fatDrive *drive: tDisks ) 
				delete drive;

			return EXIT_FAILURE;
		}

		if( dos_kernel_disabled ) {	
			if( 0 ==index ) {
				if( imageDiskList[ tDriveLetter ]  )
					imageDiskList[ tDriveLetter ]->Release();
				imageDiskList[ tDriveLetter ]= newDrive->loadedDisk;
				imageDiskList[ tDriveLetter ]->Addref();
				imageDiskChange[ tDriveLetter ]= true;
			} else {

				if( diskSwap[ index-1 ] ) {
					diskSwap[ index-1 ]->Release();
				}
				diskSwap[ index-1 ]= newDrive->loadedDisk;
				diskSwap[ index-1 ]->Addref();
			}
			index++;
		}
		else {
		tDisks.push_back( newDrive );
			DriveManager::AppendDisk( tDriveLetter,newDrive );
		}
	}

	if( !dos_kernel_disabled )
		DriveManager::InitializeDrive( tDriveLetter );
	return EXIT_SUCCESS;
}/* }}} */
#endif

std::string	handleError( int tErrorCode ) { /* {{{ */
	switch( tErrorCode ) {
		case 0: return  "";
		case 1: return  MSGX("MSCDEX_ERROR_MULTIPLE_CDROMS");
		case 2: return  MSGX("MSCDEX_ERROR_NOT_SUPPORTED");
		case 3: return  MSGX("MSCDEX_ERROR_OPEN");
		case 4: return  MSGX("MSCDEX_TOO_MANY_DRIVES");
		case 5: return  MSGX("MSCDEX_LIMITED_SUPPORT");
		case 6: return  MSGX("MSCDEX_INVALID_FILEFORMAT");
	}
	return MSGX("MSCDEX_UNKNOWN_ERROR");
}/* }}} */

bool	Monitor::Programs::changeRemovableMedia( Monitor &tM,const cmd_tokens_t &tUser ) {/* {{{ */
	Monitor& m= tM;

	char drive_letter= tUser.at( 1 ).at( 0 );
	drive_letter= std::toupper( drive_letter );

	std::vector< std::string > paths= { };
	for( size_t i{ 2 }; i <tUser.size(); ++i ) 
		paths.emplace_back( tUser.at( i ) );
	if( paths.empty() ) {
		Monitor::messageClient( m._client,MSGX( "ERROR" ) );
		return EXIT_FAILURE;
	}
	/* Unlike the IMGMOUNT program we can only replace existing drives. */
	int entry= static_cast< int >( drive_letter-'A' );
	if( entry <2 ) {
	#if (defined(WIN32) || defined(LINUX) || defined(BSD)) && !defined(OSFREE)
		std::vector< fatDrive* > fatDisks= {};
		if( int retval= createFloppyDrives( entry,paths,fatDisks ) ) {
			m.messageClient( m._client,"MONITOR Drive(s) not avilable." );
			return EXIT_FAILURE;
		}
		#else
		m.messageClient( m._client,"MONITOR Drive(s) not avilable." );
		return EXIT_FAILURE;
		#endif
	}
	else {

		std::vector<DOS_Drive*> isoDisks= {};
		if( int code= createISODrives( drive_letter,paths,isoDisks ) ) {
			m.messageClient( m._client,handleError( code ) );
			return EXIT_FAILURE;
		}
		DriveManager::ClearDrive( entry );

		for( auto media:isoDisks )
			DriveManager::AppendDisk( entry,media );
		DriveManager::InitializeDrive( entry );

		if (!dos_kernel_disabled)
			mem_writeb( Real2Phys( dos.tables.mediaid ) + entry * dos.tables.dpb_size,MEDIA_ID );

		Drives[ entry ]->Activate();
		if( !dos_kernel_disabled )
			static_cast< isoDrive*>( Drives[ entry ] )->MediaChangeImmediate();
		else
			Drives[ entry ]->MediaChange();

		lastmount = entry;
	}
	return EXIT_SUCCESS;
}/* }}} */

bool	Monitor::Programs::configure( Monitor &tM,const cmd_tokens_t &tUser ) {/* {{{ */
	Monitor::Configurator configure;
	configure.run( tM,tUser );
	return EXIT_SUCCESS;
}/* }}} */

bool Monitor::Programs::helpMe( Monitor &tM,const cmd_tokens_t& cmdline ){/* {{{ */
		Monitor& m= tM;
		if( cmdline.size() >1 ) {
			auto command= std::find_if( m._commands->begin(),m._commands->end(),
				[ & ]( const CommandExec &line ) {
					return line.command ==cmdline.at( 1 ); } );

			if( m._commands->end()==command ) m.messageClient( m._client,"Unrecognized command.\n" );
			else m.messageClient( m._client,command->description+"\n" );
		} else {
				std::string help= "Commands: ";
				for( auto cmd:*m._commands )
					help+= cmd.command+", ";

				help.erase( help.size()-1 );
				help.at( help.size()-1 )= '\n';

				m.messageClient( m._client,help+"\nType \'help\' <command> for more information.\n" );
		}
	return EXIT_SUCCESS;
}/* }}} */ // }}}

// Montor::CommandLine {{{
const int Monitor::UserRequest::handleUser( const std::string &tBuffer ) { /* {{{ */
	cmd_tokens_t					command_line= {};
	std::istringstream				buffer( tBuffer );
	std::string						token;

	while( buffer >>std::quoted( token )) command_line.push_back( token );

	if( tBuffer.empty() ){
		return USER_REQ_EMPTY;
	}
	auto processCommand= std::find_if( commands->begin(),commands->end(),
		[ & ]( const CommandExec& item ){	
			return item.command == command_line.at( 0 );});

	if( processCommand ==commands->end() ) {
		return USER_REQ_UNKNOWN;
	}
	ptr_monitor->setUserInput( processCommand->exec,command_line );
	Monitor_RunProgram= Monitor::RunProgram;

	return USER_REQ_SUCCESS;
}// }}} }}}

#endif // C_MONITOR 
