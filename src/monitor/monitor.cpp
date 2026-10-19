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

static std::string CONFIG_USAGE_MONITOR= {
"The DOSBox-X socket pipeline configurator\n"\
"**Warning this feature is mostly untested and may cause the program to crash.**\n"\
"**                           PROCEED WITH CAUTION                            **\n"\
"-get \"section property\" returns the value of the property (also to %%CONFIG%%).\n"\
"-set (-setf for force) \"section property=value\" sets the value of the property.\n"\
};

bool Monitor::Configurator::getProperty( const cmd_tokens_t& tUser ) {/* {{{ */
	Monitor& m= Monitor::OpenMonitor();
	if( tUser.size() <3 ) {
		m.messageClient( std::string( MSG_Get( "PROGRAM_CONFIG_GET_SYNTAX" ) ) );
		return EXIT_FAILURE;
	}
	Section_prop *config_section= static_cast< Section_prop* >( control->GetSection( tUser.at( 2 ) ) );
	if( nullptr ==config_section ) {
		m.messageClient( std::string( MSG_Get( "PROGRAM_CONFIG_PROPERTY_ERROR" ) ) );
		return EXIT_FAILURE;
	}
	if( tUser.size() <4 ) {
		for( auto prop:config_section->properties ) {
			m.messageClient( prop->propname + "=" + prop->GetValue().ToString()+"\n" );
		}
		return EXIT_SUCCESS;
	}
	Property *property= config_section->Get_prop( tUser.at( 3 ) );
	if( nullptr ==property ) {
		m.messageClient( std::string( MSG_Get( "PROGRAM_CONFIG_PROPERTY_ERROR" ) ) );
	}
	
	m.messageClient( property->GetValue().ToString()+"\n" );

	return EXIT_SUCCESS;
};/* }}} */

bool Monitor::Configurator::setProperty( const cmd_tokens_t& tUser ) {/* {{{ */
	Monitor& m= Monitor::OpenMonitor();
	if( tUser.size() <5 ) {
		m.messageClient( std::string( MSG_Get( "PROGRAM_CONFIG_SET_SYNTAX" ) ) );
		return EXIT_FAILURE;
	}
	if( EXIT_SUCCESS ==tUser.at( 2 ).compare( "sdl" ) ) {
		/* Changes the SDL must be done on main thread.  */
		m.messageClient( std::string( "Changes to sdl section not permitted.\n" ) );
		return EXIT_FAILURE;
	}
	Section_prop *config_section= static_cast< Section_prop* >( control->GetSection( tUser.at( 2 ) ) );
	if( nullptr ==config_section ) {
		m.messageClient( std::string( MSG_Get( "PROGRAM_CONFIG_SET_SYNTAX" ) ) );
		return EXIT_FAILURE;
	}
	Property *property= config_section->Get_prop( tUser.at( 3 ) );
	if( nullptr ==property ) {
		m.messageClient( std::string( MSG_Get( "PROGRAM_CONFIG_PROPERTY_ERROR" ) ) );
	}

	std::string inputline= tUser.at( 3 )+"="+tUser.at( 4 );
	config_section->HandleInputline( inputline );

	return EXIT_SUCCESS;
};/* }}} */

bool Monitor::Programs::quitMainInstance( const cmd_tokens_t& ){/* {{{ */
	DoKillSwitch();
	return EXIT_SUCCESS;
}/* }}} */

bool	Monitor::Programs::displayInfo( const cmd_tokens_t& ) {/* {{{ */
	Section* sec=control->GetSection( "cpu" );
		
	ddesc_t info= {};
	info.emplace_back( dinfo_t{ std::string( "CPU:" ),sec->GetPropValue( "cputype" ) } );
	info.emplace_back( dinfo_t{ std::string( "Core:" ),sec->GetPropValue( "core" ) } );
	info.emplace_back( dinfo_t{ std::string( "Cycles:" ),sec->GetPropValue( "cycles" ) } );

	messageClient( "\nSystem Info:\n" );
	for( auto i:info ) {
		messageClient( i.at( 0 )+"\t\t"+i.at( 1 )+"\n" );
	}
	sec= control->GetSection( "dosbox" );
	messageClient( "\nMemory:\nRAM:\t\t" +sec->GetPropValue( "memsize" )+"Mb\n" );
		
	messageClient( "\nDrive(s):\n" );
	ddesc_t drive_list= {};
	std::string sstr_token;

	for( int i= 0; i <26; ++i )
		if( Drives[ i ] ) {
			dinfo_t drive= {};
			drive.push_back( std::string( 1,char( 'A'+i ) ) );

			std::string drive_info( Drives[ i ]->GetInfo() );
			std::size_t dir_pos= drive_info.find_first_of( "/\\" );
				
			if( dir_pos !=std::string::npos ) {
				drive.push_back( drive_info.substr( 0,dir_pos ) );
				drive.push_back( drive_info.substr( dir_pos ) );
			}
			drive.push_back( drive_info );					
			drive_list.push_back( drive );
		}

	std::string message= {};
	for( auto drv:drive_list ) {
		messageClient(  drv.at( 0 )+":\\\t" );
		if( drv.size() >2 ) message+= drv.at( 2 )+"\t\t";
		message+= "("+drv.at( 1 )+")\n";
		messageClient( message );
	}
	messageClient( "\n" );
	return EXIT_SUCCESS;
}	/* }}} */

bool	Monitor::Programs::changeRemovableMedia( const cmd_tokens_t &tUser ) {/* {{{ */
		bool multiple_discs= false;
		if( tUser.size() <2 ) {
			return EXIT_FAILURE;
		} else if ( tUser.size()>2 ) multiple_discs= true;

		std::vector< int > availableDrives= {};
		Get_IDECD_drives( availableDrives );

		if( availableDrives.empty() ) {
			Monitor::messageClient( "No available drives.\n" );
			return EXIT_FAILURE;
		}
					
		char drive_letter= tUser.at( 1 ).at( 0 );
		drive_letter= std::toupper( drive_letter );
		bool drive_check= false;
		for( int drive_no:availableDrives )
			if ( char( 'A'+drive_no ) ==drive_letter ) drive_check =true;

		if ( false ==drive_check ) {
			Monitor::messageClient( "Drive letter not recognized.\n" );
			return EXIT_FAILURE;
		}

		std::vector< std::string > image_list= { tUser.at( 2 ) };

		if( multiple_discs )
			for( size_t i{ 3 }; i <tUser.size(); ++i ) { 
				image_list.push_back( tUser.at( i ) );
			}

		runImgmountISO( drive_letter,image_list,multiple_discs );

		return EXIT_SUCCESS;
}/* }}} */

bool	Monitor::Programs::configure( const cmd_tokens_t &tUser ) {/* {{{ */
	Monitor& m= Monitor::OpenMonitor();
	if( tUser.size() <=1 ) {
		m.messageClient( std::string( CONFIG_USAGE_MONITOR ) );	
		return EXIT_FAILURE;
	}

	struct command {
		std::string	param;
		bool ( Monitor::Configurator::*execute )( const cmd_tokens_t& );
	};

	Monitor::Configurator execute;
	bool (Monitor::Configurator::*get)( const cmd_tokens_t& )= &Monitor::Configurator::getProperty;
	bool (Monitor::Configurator::*set)( const cmd_tokens_t& )= &Monitor::Configurator::setProperty;
	const std::array< command,3 > params= { {
		{ "-get",	get },
		{ "-set",	set },
	} };

	auto p= std::find_if( params.begin(),params.end(),
		[ &tUser ]( const command& user_req ) {
		return !user_req.param.compare( tUser.at( 1 ) ); } );

	if( p ==params.end() ) {
		m.messageClient( CONFIG_USAGE_MONITOR );	
		return EXIT_FAILURE;
	}

	return ( execute.*( p->execute ) )( tUser );
};/* }}} */

bool Monitor::Programs::helpMe( const cmd_tokens_t& cmdline ){/* {{{ */
		Monitor& m= Monitor::OpenMonitor();
		if( cmdline.size() >1 ) {
			auto command= std::find_if( m._commands->begin(),m._commands->end(),
				[ & ]( const CommandExec &line ) {
					return line.command ==cmdline.at( 1 ); } );

			if( m._commands->end()==command ) m.messageClient( "Unrecognized command.\n" );
			else m.messageClient( command->description+"\n" );

		} else {
				std::string help= "Commands: ";
				for( auto cmd:*m._commands )
					help+= cmd.command+", ";

				help.erase( help.size()-1 );
				help.at( help.size()-1 )= '\n';

				m.messageClient( help+"\nType \'help\' <command> for more information.\n" );
		}
	return EXIT_SUCCESS;
}/* }}} */

cmd_tokens_t Monitor::CommandLine::processCommandLine( std::string tBuffer ) {/* {{{ */
	cmd_tokens_t	command_line= {};
	std::istringstream	buffer( tBuffer );
	std::string	token;

	while( buffer >>std::quoted( token )) command_line.push_back( token );
	return command_line;
};/* }}} */

#endif // C_MONITOR 
