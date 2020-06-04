/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "mem.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "pic.h"
#include <cstring>
#include <math.h>

#include "mame/emu.h"
#include "mame/saa1099.h"

#define MASTER_CLOCK 7159090

//My mixer channel
static MixerChannel * cms_chan;
//Timer to disable the channel after a while
static Bit32u lastWriteTicks;
static Bit32u cmsBase;
static saa1099_device* device[2];

static void write_cms(Bitu port, Bitu val, Bitu /* iolen */) {
	if(cms_chan && (!cms_chan->enabled)) cms_chan->Enable(true);
	lastWriteTicks = (Bit32u)PIC_Ticks;
	switch ( port - cmsBase ) {
	case 1:
		device[0]->control_w(0, 0, (u8)val);
		break;
	case 0:
		device[0]->data_w(0, 0, (u8)val);
		break;
	case 3:
		device[1]->control_w(0, 0, (u8)val);
		break;
	case 2:
		device[1]->data_w(0, 0, (u8)val);
		break;
	}
}

static void CMS_CallBack(Bitu len) {
	enum {
		BUFFER_SIZE = 2048
	};

	if ( len > BUFFER_SIZE )
		return;

	if ( cms_chan ) {

		//Have there been 10 seconds of no commands, disable channel
		if ( lastWriteTicks + 10000 < PIC_Ticks ) {
			cms_chan->Enable( false );
			return;
		}
		Bit32s result[BUFFER_SIZE][2];
		Bit16s work[2][BUFFER_SIZE];
		Bit16s* buffers[2] = { work[0], work[1] };
		device_sound_interface::sound_stream stream;
		device[0]->sound_stream_update(stream, 0, buffers, (int)len);
		for (Bitu i = 0; i < len; i++) {
			result[i][0] = work[0][i];
			result[i][1] = work[1][i];
		}
		device[1]->sound_stream_update(stream, 0, buffers, (int)len);
		for (Bitu i = 0; i < len; i++) {
			result[i][0] += work[0][i];
			result[i][1] += work[1][i];
		}
		cms_chan->AddSamples_s32( len, result[0] );
	}
}

// The Gameblaster detection
static Bit8u cms_detect_register = 0xff;

static void write_cms_detect(Bitu port, Bitu val, Bitu /* iolen */) {
	switch ( port - cmsBase ) {
	case 0x6:
	case 0x7:
		cms_detect_register = (Bit8u)val;
		break;
	}
}

static Bitu read_cms_detect(Bitu port, Bitu /* iolen */) {
	Bit8u retval = 0xff;
	switch ( port - cmsBase ) {
	case 0x4:
		retval = 0x7f;
		break;
	case 0xa:
	case 0xb:
		retval = cms_detect_register;
		break;
	}
	return retval;
}


class CMS:public Module_base {
private:
	IO_WriteHandleObject WriteHandler;
	IO_WriteHandleObject DetWriteHandler;
	IO_ReadHandleObject DetReadHandler;
	MixerObject MixerChan;

public:
	CMS(Section* configuration):Module_base(configuration) {
		Section_prop * section = static_cast<Section_prop *>(configuration);
		Bitu sampleRate = (Bitu)section->Get_int( "oplrate" );
		cmsBase = (Bit32u)section->Get_hex("sbbase");
		WriteHandler.Install( cmsBase, write_cms, IO_MB, 4 );

		// A standalone Gameblaster has a magic chip on it which is
		// sometimes used for detection.
		const char * sbtype=section->Get_string("sbtype");
		if (!strcasecmp(sbtype,"gb")) {
			DetWriteHandler.Install( cmsBase + 4, write_cms_detect, IO_MB, 12 );
			DetReadHandler.Install(cmsBase,read_cms_detect,IO_MB,16);
		}

		/* Register the Mixer CallBack */
		cms_chan = MixerChan.Install(CMS_CallBack,sampleRate,"CMS");
	
		lastWriteTicks = (Bit32u)PIC_Ticks;

#if 0 // unused?
		Bit32u freq = 7159000;		//14318180 isa clock / 2
#endif

		machine_config config;
		device[0] = new saa1099_device(config, "", 0, 7159090);
		device[1] = new saa1099_device(config, "", 0, 7159090);

		device[0]->device_start();
		device[1]->device_start();
	}

	~CMS() {
		cms_chan = 0;
		delete device[0];
		delete device[1];
	}
};


static CMS* test = NULL;
   
void CMS_Init(Section* sec) {
    if (test == NULL)
    	test = new CMS(sec);
}
void CMS_ShutDown(Section* sec) {
    (void)sec;//UNUSED
    if (test) {
        delete test;
        test = NULL;
    }
}

// save state support
void POD_Save_Gameblaster( std::ostream& stream )
{
	const char pod_name[32] = "CMS";

    if( stream.fail() ) return;
    if( !test ) return;
    if( !cms_chan ) return;

    WRITE_POD( &pod_name, pod_name );

    //*******************************************
    //*******************************************
    //*******************************************

    // - pure data
    WRITE_POD( &lastWriteTicks, lastWriteTicks );
    WRITE_POD( &cmsBase, cmsBase );
    WRITE_POD( &cms_detect_register, cms_detect_register );

    //************************************************
    //************************************************
    //************************************************

    for (int i=0; i<2; i++) {
        device[i]->SaveState(stream);
    }

    cms_chan->SaveState(stream);
}


void POD_Load_Gameblaster( std::istream& stream )
{
	char pod_name[32] = {0};

	if( stream.fail() ) return;
	if( !test ) return;
	if( !cms_chan ) return;


	// error checking
	READ_POD( &pod_name, pod_name );
	if( strcmp( pod_name, "CMS" ) ) {
		stream.clear( std::istream::failbit | std::istream::badbit );
		return;
	}

	//*******************************************
	//*******************************************
	//*******************************************

    // - pure data
    READ_POD( &lastWriteTicks, lastWriteTicks );
    READ_POD( &cmsBase, cmsBase );
    READ_POD( &cms_detect_register, cms_detect_register );

	//************************************************
	//************************************************
	//************************************************

    for (int i=0; i<2; i++) {
        device[i]->LoadState(stream);
   }

	cms_chan->LoadState(stream);
}
