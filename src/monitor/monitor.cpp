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

Monitor *Monitor::ptr_monitor= nullptr;

#include "control.h"

// Monitor::Configurator {{{ 
static std::string CONFIG_USAGE_MONITOR= { // {{{
"The DOSBox-X socket pipeline configurator\n"\
"**Warning this feature is mostly untested and may cause the program to crash.**\n"\
"**                           PROCEED WITH CAUTION                            **\n"\
"-get \"section property\" returns the value of the property (also to %%CONFIG%%).\n"\
"-set (-setf for force) \"section property=value\" sets the value of the property.\n"\
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
};/* }}} */

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
};/* }}} */

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
	}
	if( EXIT_SUCCESS ==tUser.at( 2 ).compare( "sdl" ) ) {
		/* Changes the SDL must be done on main thread.  */
		m.messageClient( m._client,std::string( "Changes to sdl section not permitted.\n" ) );
		return EXIT_FAILURE;
	}

	std::string inputline= tUser.at( 3 )+"="+tUser.at( 4 );
	config_section->HandleInputline( inputline );

	return EXIT_SUCCESS;
};/* }}} */ /* }}} */

// Monitor::Programs {{{
bool Monitor::Programs::quitMainInstance( Monitor &tM,const cmd_tokens_t& ){/* {{{ */
	tM._isRunning= false;
	DoKillSwitch();
	return EXIT_SUCCESS;
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
		messageClient( m._client, drv.at( 0 )+":\\\t" );
		if( drv.size() >2 ) message+= drv.at( 2 )+"\t\t";
		message+= "("+drv.at( 1 )+")\n";
		messageClient( m._client,message );
	}
	messageClient( m._client,"\n" );
	return EXIT_SUCCESS;
}	/* }}} */

bool	Monitor::Programs::changeRemovableMedia( Monitor &tM,const cmd_tokens_t &tUser ) {/* {{{ */
		Monitor& m= tM;
		bool replace= false;
		if( tUser.size() <3 ) {
			return EXIT_FAILURE;
		} else if ( tUser.size()>3 ) replace= true;

		std::vector< int > availableDrives= {};
		Get_IDECD_drives( availableDrives );

		if( availableDrives.empty() ) {
			Monitor::messageClient( m._client,"No available drives.\n" );
			return EXIT_FAILURE;
		}
					
		char drive_letter= tUser.at( 1 ).at( 0 );
		drive_letter= std::toupper( drive_letter );
		bool drive_check= false;
		for( int drive_no:availableDrives )
			if ( char( 'A'+drive_no ) ==drive_letter ) drive_check =true;

		if ( false ==drive_check ) {
			Monitor::messageClient( m._client,"Drive letter not recognized.\n" );
			return EXIT_FAILURE;
		}

		std::vector< std::string > image_list= { tUser.at( 2 ) };

		if( tUser.size() > 3 )
			for( size_t i{ 3 }; i <tUser.size(); ++i ) { 
				image_list.push_back( tUser.at( i ) );
			}

		runImgmountISO( drive_letter,image_list,replace );

		return EXIT_SUCCESS;
}/* }}} */

bool	Monitor::Programs::configure( Monitor &tM,const cmd_tokens_t &tUser ) {/* {{{ */
	Monitor::Configurator configure;
	configure.run( tM,tUser );
	return EXIT_SUCCESS;
};/* }}} */

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
cmd_tokens_t Monitor::CommandLine::processCommandLine( std::string tBuffer ) { /* {{{ */
	cmd_tokens_t	command_line= {};
	std::istringstream	buffer( tBuffer );
	std::string	token;

	while( buffer >>std::quoted( token )) command_line.push_back( token );
	return command_line;
};// }}} }}}

#endif // C_MONITOR 
