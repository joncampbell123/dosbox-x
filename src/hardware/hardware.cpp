/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dosbox.h"
#include "control.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "mem.h"
#include "mapper.h"
#include "pic.h"
#include "mixer.h"
#include "render.h"
#include "cross.h"

#include "riff_wav_writer.h"
#include "avi_writer.h"
#include "rawint.h"

#include <map>

/* FIXME: This needs to be an enum */
bool native_zmbv = false;

std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState = 0;

#define WAVE_BUF 16*1024
#define MIDI_BUF 4*1024
#define AVI_HEADER_SIZE	500

static struct {
	struct {
		riff_wav_writer *writer;
		Bit16s buf[WAVE_BUF][2];
		Bitu used;
		Bit32u length;
		Bit32u freq;
	} wave;
    struct {
        avi_writer  *writer;
		Bitu		audiorate;
        std::map<std::string,size_t> name_to_stream_index;
    } multitrack_wave;
	struct {
		FILE * handle;
		Bit8u buffer[MIDI_BUF];
		Bitu used,done;
		Bit32u last;
	} midi;
	struct {
		Bitu rowlen;
	} image;
} capture;

std::string GetCaptureFilePath(const char * type,const char * ext) {
	if(capturedir.empty()) {
		LOG_MSG("Please specify a capture directory");
		return "";
	}

	Bitu last=0;
	char file_start[16];
	dir_information * dir;
	/* Find a filename to open */
	dir = open_directory(capturedir.c_str());
	if (!dir) {
		//Try creating it first
		Cross::CreateDir(capturedir);
		dir=open_directory(capturedir.c_str());
		if(!dir) {
			LOG_MSG("Can't open dir %s for capturing %s",capturedir.c_str(),type);
			return 0;
		}
	}
	strcpy(file_start,RunningProgram);
	lowcase(file_start);
	strcat(file_start,"_");
	bool is_directory;
	char tempname[CROSS_LEN];
	bool testRead = read_directory_first(dir, tempname, is_directory );
	for ( ; testRead; testRead = read_directory_next(dir, tempname, is_directory) ) {
		char * test=strstr(tempname,ext);
		if (!test || strlen(test)!=strlen(ext)) 
			continue;
		*test=0;
		if (strncasecmp(tempname,file_start,strlen(file_start))!=0) continue;
		Bitu num=(Bitu)atoi(&tempname[strlen(file_start)]);
		if (num>=last) last=num+1;
	}
	close_directory( dir );
	char file_name[CROSS_LEN];
	sprintf(file_name,"%s%c%s%03d%s",capturedir.c_str(),CROSS_FILESPLIT,file_start,(int)last,ext);
	return file_name;
}

FILE * OpenCaptureFile(const char * type,const char * ext) {
	if(capturedir.empty()) {
		LOG_MSG("Please specify a capture directory");
		return 0;
	}

	Bitu last=0;
	char file_start[16];
	dir_information * dir;
	/* Find a filename to open */
	dir = open_directory(capturedir.c_str());
	if (!dir) {
		//Try creating it first
		Cross::CreateDir(capturedir);
		dir=open_directory(capturedir.c_str());
		if(!dir) {
			LOG_MSG("Can't open dir %s for capturing %s",capturedir.c_str(),type);
			return 0;
		}
	}
	strcpy(file_start,RunningProgram);
	lowcase(file_start);
	strcat(file_start,"_");
	bool is_directory;
	char tempname[CROSS_LEN];
	bool testRead = read_directory_first(dir, tempname, is_directory );
	for ( ; testRead; testRead = read_directory_next(dir, tempname, is_directory) ) {
		char * test=strstr(tempname,ext);
		if (!test || strlen(test)!=strlen(ext)) 
			continue;
		*test=0;
		if (strncasecmp(tempname,file_start,strlen(file_start))!=0) continue;
		Bitu num=(Bitu)atoi(&tempname[strlen(file_start)]);
		if (num>=last) last=num+1;
	}
	close_directory( dir );
	char file_name[CROSS_LEN];
	sprintf(file_name,"%s%c%s%03d%s",capturedir.c_str(),CROSS_FILESPLIT,file_start,(int)last,ext);
	/* Open the actual file */
	FILE * handle=fopen(file_name,"wb");
	if (handle) {
		LOG_MSG("Capturing %s to %s",type,file_name);
	} else {
		LOG_MSG("Failed to open %s for capturing %s",file_name,type);
	}
	return handle;
}

void CAPTURE_WaveEvent(bool pressed);

void CAPTURE_StartWave(void) {
	if (!(CaptureState & CAPTURE_WAVE))
        CAPTURE_WaveEvent(true);
}

void CAPTURE_StopWave(void) {
	if (CaptureState & CAPTURE_WAVE)
        CAPTURE_WaveEvent(true);
}

void CAPTURE_MTWaveEvent(bool pressed);

void CAPTURE_StartMTWave(void) {
	if (!(CaptureState & CAPTURE_MULTITRACK_WAVE))
        CAPTURE_MTWaveEvent(true);
}

void CAPTURE_StopMTWave(void) {
	if (CaptureState & CAPTURE_MULTITRACK_WAVE)
        CAPTURE_MTWaveEvent(true);
}

unsigned int GFX_GetBShift();

void CAPTURE_AddImage(Bitu width, Bitu height, Bitu bpp, Bitu pitch, Bitu flags, float fps, Bit8u * data, Bit8u * pal) {
	return;
}


MixerChannel * MIXER_FirstChannel(void);

void CAPTURE_MultiTrackAddWave(Bit32u freq, Bit32u len, Bit16s * data,const char *name) {
    if (CaptureState & CAPTURE_MULTITRACK_WAVE) {
		if (capture.multitrack_wave.writer == NULL) {
            unsigned int streams = 0;

            {
                MixerChannel *c = MIXER_FirstChannel();
                while (c != NULL) {
                    streams++;
                    c = c->next;
                }
            }

            if (streams == 0) {
                LOG_MSG("Not starting multitrack wave, no streams");
                goto skip_mt_wav;
            }

			std::string path = GetCaptureFilePath("Multitrack Wave",".mt.avi");
			if (path == "") {
                LOG_MSG("Cannot determine capture path");
				goto skip_mt_wav;
            }

		    capture.multitrack_wave.audiorate = freq;

			capture.multitrack_wave.writer = avi_writer_create();
			if (capture.multitrack_wave.writer == NULL)
				goto skip_mt_wav;

			if (!avi_writer_open_file(capture.multitrack_wave.writer,path.c_str()))
				goto skip_mt_wav;

            if (!avi_writer_set_stream_writing(capture.multitrack_wave.writer))
                goto skip_mt_wav;

			riff_avih_AVIMAINHEADER *mheader = avi_writer_main_header(capture.multitrack_wave.writer);
			if (mheader == NULL)
				goto skip_mt_wav;

			memset(mheader,0,sizeof(*mheader));
			__w_le_u32(&mheader->dwMicroSecPerFrame,(uint32_t)(1000000 / 30)); /* NTS: int divided by double FIXME GUESS */
			__w_le_u32(&mheader->dwMaxBytesPerSec,0);
			__w_le_u32(&mheader->dwPaddingGranularity,0);
			__w_le_u32(&mheader->dwFlags,0x110);                     /* Flags,0x10 has index, 0x100 interleaved */
			__w_le_u32(&mheader->dwTotalFrames,0);			/* AVI writer updates this automatically on finish */
			__w_le_u32(&mheader->dwInitialFrames,0);
			__w_le_u32(&mheader->dwStreams,streams);
			__w_le_u32(&mheader->dwSuggestedBufferSize,0);
			__w_le_u32(&mheader->dwWidth,0);
			__w_le_u32(&mheader->dwHeight,0);

            capture.multitrack_wave.name_to_stream_index.clear();
            {
                MixerChannel *c = MIXER_FirstChannel();
                while (c != NULL) {
                    /* for each channel in the mixer now, make a stream in the AVI file */
                    avi_writer_stream *astream = avi_writer_new_stream(capture.multitrack_wave.writer);
                    if (astream == NULL)
                        goto skip_mt_wav;

                    riff_strh_AVISTREAMHEADER *asheader = avi_writer_stream_header(astream);
                    if (asheader == NULL)
                        goto skip_mt_wav;

                    memset(asheader,0,sizeof(*asheader));
                    __w_le_u32(&asheader->fccType,avi_fccType_audio);
                    __w_le_u32(&asheader->fccHandler,0);
                    __w_le_u32(&asheader->dwFlags,0);
                    __w_le_u16(&asheader->wPriority,0);
                    __w_le_u16(&asheader->wLanguage,0);
                    __w_le_u32(&asheader->dwInitialFrames,0);
                    __w_le_u32(&asheader->dwScale,1);
                    __w_le_u32(&asheader->dwRate,capture.multitrack_wave.audiorate);
                    __w_le_u32(&asheader->dwStart,0);
                    __w_le_u32(&asheader->dwLength,0);			/* AVI writer updates this automatically */
                    __w_le_u32(&asheader->dwSuggestedBufferSize,0);
                    __w_le_u32(&asheader->dwQuality,~0u);
                    __w_le_u32(&asheader->dwSampleSize,2*2);
                    __w_le_u16(&asheader->rcFrame.left,0);
                    __w_le_u16(&asheader->rcFrame.top,0);
                    __w_le_u16(&asheader->rcFrame.right,0);
                    __w_le_u16(&asheader->rcFrame.bottom,0);

                    windows_WAVEFORMAT fmt;

                    memset(&fmt,0,sizeof(fmt));
                    __w_le_u16(&fmt.wFormatTag,windows_WAVE_FORMAT_PCM);
                    __w_le_u16(&fmt.nChannels,2);			/* stereo */
                    __w_le_u32(&fmt.nSamplesPerSec,capture.multitrack_wave.audiorate);
                    __w_le_u16(&fmt.wBitsPerSample,16);		/* 16-bit/sample */
                    __w_le_u16(&fmt.nBlockAlign,2*2);
                    __w_le_u32(&fmt.nAvgBytesPerSec,capture.multitrack_wave.audiorate*2*2);

                    if (!avi_writer_stream_set_format(astream,&fmt,sizeof(fmt)))
                        goto skip_mt_wav;

                    if (c->name != NULL && *(c->name) != 0) {
			            LOG_MSG("multitrack audio, mixer channel '%s' is AVI stream %d",c->name,astream->index);
                        capture.multitrack_wave.name_to_stream_index[c->name] = (size_t)astream->index;
                        astream->name = c->name;
                    }

                    c = c->next;
                }
            }

			if (!avi_writer_begin_header(capture.multitrack_wave.writer) || !avi_writer_begin_data(capture.multitrack_wave.writer))
				goto skip_mt_wav;

			LOG_MSG("Started capturing multitrack audio (%u channels).",streams);
		}

        if (capture.multitrack_wave.writer != NULL) {
            std::map<std::string,size_t>::iterator ni = capture.multitrack_wave.name_to_stream_index.find(name);
            if (ni != capture.multitrack_wave.name_to_stream_index.end()) {
                size_t index = ni->second;

                if (index < (size_t)capture.multitrack_wave.writer->avi_stream_alloc) {
                    avi_writer_stream *os = capture.multitrack_wave.writer->avi_stream + index;
			        avi_writer_stream_write(capture.multitrack_wave.writer,os,data,len * 2 * 2,/*keyframe*/0x10);
                }
                else {
                    LOG_MSG("Multitrack: Ignoring unknown track '%s', out of range\n",name);
                }
            }
            else {
                LOG_MSG("Multitrack: Ignoring unknown track '%s'\n",name);
            }
        }
    }

    return;
skip_mt_wav:
	capture.multitrack_wave.writer = avi_writer_destroy(capture.multitrack_wave.writer);
}

void CAPTURE_AddWave(Bit32u freq, Bit32u len, Bit16s * data) {
	if (CaptureState & CAPTURE_WAVE) {
		if (capture.wave.writer == NULL) {
			std::string path = GetCaptureFilePath("Wave Output",".wav");
			if (path == "") {
				CaptureState &= ~((unsigned int)CAPTURE_WAVE);
				return;
			}

			capture.wave.writer = riff_wav_writer_create();
			if (capture.wave.writer == NULL) {
				CaptureState &= ~((unsigned int)CAPTURE_WAVE);
				capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
				return;
			}

			windows_WAVEFORMAT fmt;

			memset(&fmt,0,sizeof(fmt));
			__w_le_u16(&fmt.wFormatTag,windows_WAVE_FORMAT_PCM);
			__w_le_u16(&fmt.nChannels,2);			/* stereo */
			__w_le_u32(&fmt.nSamplesPerSec,freq);
			__w_le_u16(&fmt.wBitsPerSample,16);		/* 16-bit/sample */
			__w_le_u16(&fmt.nBlockAlign,2*2);
			__w_le_u32(&fmt.nAvgBytesPerSec,freq*2*2);

			if (!riff_wav_writer_open_file(capture.wave.writer,path.c_str())) {
				CaptureState &= ~((unsigned int)CAPTURE_WAVE);
				capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
				return;
			}
			if (!riff_wav_writer_set_format(capture.wave.writer,&fmt) ||
				!riff_wav_writer_begin_header(capture.wave.writer) ||
				!riff_wav_writer_begin_data(capture.wave.writer)) {
				CaptureState &= ~((unsigned int)CAPTURE_WAVE);
				capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
				return;
			}

			capture.wave.length = 0;
			capture.wave.used = 0;
			capture.wave.freq = freq;
			LOG_MSG("Started capturing wave output.");
		}
		Bit16s * read = data;
		while (len > 0 ) {
			Bitu left = WAVE_BUF - capture.wave.used;
			if (!left) {
				riff_wav_writer_data_write(capture.wave.writer,capture.wave.buf,2*2*WAVE_BUF);
				capture.wave.length += 4*WAVE_BUF;
				capture.wave.used = 0;
				left = WAVE_BUF;
			}
			if (left > len)
				left = len;
			memcpy( &capture.wave.buf[capture.wave.used], read, left*4);
			capture.wave.used += left;
			read += left*2;
			len -= left;
		}
	}
}

void CAPTURE_MTWaveEvent(bool pressed) {
	if (!pressed)
		return;

    if (CaptureState & CAPTURE_MULTITRACK_WAVE) {
        if (capture.multitrack_wave.writer != NULL) {
            LOG_MSG("Stopped capturing multitrack wave output.");
            capture.multitrack_wave.name_to_stream_index.clear();
            avi_writer_end_data(capture.multitrack_wave.writer);
            avi_writer_finish(capture.multitrack_wave.writer);
            avi_writer_close_file(capture.multitrack_wave.writer);
            capture.multitrack_wave.writer = avi_writer_destroy(capture.multitrack_wave.writer);
            CaptureState &= ~((unsigned int)CAPTURE_MULTITRACK_WAVE);
        }
    }
    else {
        CaptureState |= CAPTURE_MULTITRACK_WAVE;
    }

	mainMenu.get_item("mapper_recmtwave").check(!!(CaptureState & CAPTURE_MULTITRACK_WAVE)).refresh_item(mainMenu);
}

void CAPTURE_WaveEvent(bool pressed) {
	if (!pressed)
		return;

    if (CaptureState & CAPTURE_WAVE) {
        /* Check for previously opened wave file */
        if (capture.wave.writer != NULL) {
            LOG_MSG("Stopped capturing wave output.");
            /* Write last piece of audio in buffer */
            riff_wav_writer_data_write(capture.wave.writer,capture.wave.buf,2*2*capture.wave.used);
            capture.wave.length+=capture.wave.used*4;
            riff_wav_writer_end_data(capture.wave.writer);
            capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
            CaptureState &= ~((unsigned int)CAPTURE_WAVE);
        }
    }
    else {
        CaptureState |= CAPTURE_WAVE;
    }

	mainMenu.get_item("mapper_recwave").check(!!(CaptureState & CAPTURE_WAVE)).refresh_item(mainMenu);
}

/* MIDI capturing */

static Bit8u midi_header[]={
	'M','T','h','d',			/* Bit32u, Header Chunk */
	0x0,0x0,0x0,0x6,			/* Bit32u, Chunk Length */
	0x0,0x0,					/* Bit16u, Format, 0=single track */
	0x0,0x1,					/* Bit16u, Track Count, 1 track */
	0x01,0xf4,					/* Bit16u, Timing, 2 beats/second with 500 frames */
	'M','T','r','k',			/* Bit32u, Track Chunk */
	0x0,0x0,0x0,0x0,			/* Bit32u, Chunk Length */
	//Track data
};


static void RawMidiAdd(Bit8u data) {
	capture.midi.buffer[capture.midi.used++]=data;
	if (capture.midi.used >= MIDI_BUF ) {
		capture.midi.done += capture.midi.used;
		fwrite(capture.midi.buffer,1,MIDI_BUF,capture.midi.handle);
		capture.midi.used = 0;
	}
}

static void RawMidiAddNumber(Bit32u val) {
	if (val & 0xfe00000) RawMidiAdd((Bit8u)(0x80|((val >> 21) & 0x7f)));
	if (val & 0xfffc000) RawMidiAdd((Bit8u)(0x80|((val >> 14) & 0x7f)));
	if (val & 0xfffff80) RawMidiAdd((Bit8u)(0x80|((val >> 7) & 0x7f)));
	RawMidiAdd((Bit8u)(val & 0x7f));
}

void CAPTURE_AddMidi(bool sysex, Bitu len, Bit8u * data) {
	if (!capture.midi.handle) {
		capture.midi.handle=OpenCaptureFile("Raw Midi",".mid");
		if (!capture.midi.handle) {
			return;
		}
		fwrite(midi_header,1,sizeof(midi_header),capture.midi.handle);
		capture.midi.last=PIC_Ticks;
	}
	Bit32u delta=PIC_Ticks-capture.midi.last;
	capture.midi.last=PIC_Ticks;
	RawMidiAddNumber(delta);
	if (sysex) {
		RawMidiAdd( 0xf0 );
		RawMidiAddNumber( len );
	}
	for (Bitu i=0;i<len;i++) 
		RawMidiAdd(data[i]);
}

void CAPTURE_MidiEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.midi.handle) {
		LOG_MSG("Stopping raw midi saving and finalizing file.");
		//Delta time
		RawMidiAdd(0x00);
		//End of track event
		RawMidiAdd(0xff);
		RawMidiAdd(0x2F);
		RawMidiAdd(0x00);
		/* clear out the final data in the buffer if any */
		fwrite(capture.midi.buffer,1,capture.midi.used,capture.midi.handle);
		capture.midi.done+=capture.midi.used;
		fseek(capture.midi.handle,18, SEEK_SET);
		Bit8u size[4];
		size[0]=(Bit8u)(capture.midi.done >> 24);
		size[1]=(Bit8u)(capture.midi.done >> 16);
		size[2]=(Bit8u)(capture.midi.done >> 8);
		size[3]=(Bit8u)(capture.midi.done >> 0);
		fwrite(&size,1,4,capture.midi.handle);
		fclose(capture.midi.handle);
		capture.midi.handle=0;
		CaptureState &= ~((unsigned int)CAPTURE_MIDI);
		return;
	} 
	CaptureState ^= CAPTURE_MIDI;
	if (CaptureState & CAPTURE_MIDI) {
		LOG_MSG("Preparing for raw midi capture, will start with first data.");
		capture.midi.used=0;
		capture.midi.done=0;
		capture.midi.handle=0;
	} else {
		LOG_MSG("Stopped capturing raw midi before any data arrived.");
	}

	mainMenu.get_item("mapper_caprawmidi").check(!!(CaptureState & CAPTURE_MIDI)).refresh_item(mainMenu);
}

void CAPTURE_Destroy(Section *sec) {
    (void)sec;//UNUSED
	// if capture is active, fake mapper event to "toggle" it off for each capture case.
    if (capture.multitrack_wave.writer) CAPTURE_MTWaveEvent(true);
	if (capture.wave.writer) CAPTURE_WaveEvent(true);
	if (capture.midi.handle) CAPTURE_MidiEvent(true);
}

void CAPTURE_Init() {
	DOSBoxMenu::item *item;

	LOG(LOG_MISC,LOG_DEBUG)("Initializing screenshot and A/V capture system");

	Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(section != NULL);

	// grab and store capture path
	Prop_path *proppath = section->Get_path("captures");
	assert(proppath != NULL);
	capturedir = proppath->realpath;

	std::string capfmt = section->Get_string("capture format");
	if (capfmt == "avi-zmbv" || capfmt == "default") {
		LOG_MSG("USING AVI+ZMBV");
		native_zmbv = true;
	}
	else {
		LOG_MSG("Unknown capture format, using AVI+ZMBV");
		native_zmbv = true;
	}

	CaptureState = 0; // make sure capture is off

	// mapper shortcuts for capture
	MAPPER_AddHandler(CAPTURE_WaveEvent,MK_w,MMOD3|MMODHOST,"recwave","Rec Wave", &item);
	item->set_text("Record audio to WAV");

	MAPPER_AddHandler(CAPTURE_MTWaveEvent,MK_nothing,0,"recmtwave","Rec MTWav", &item);
	item->set_text("Record audio to multi-track AVI");

	MAPPER_AddHandler(CAPTURE_MidiEvent,MK_nothing,0,"caprawmidi","Cap MIDI", &item);
	item->set_text("Record MIDI output");

	AddExitFunction(AddExitFunctionFuncPair(CAPTURE_Destroy),true);
}

void HARDWARE_Destroy(Section * sec) {
    (void)sec;//UNUSED
}

void HARDWARE_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("HARDWARE_Init: initializing");

	/* TODO: Hardware init. We moved capture init to it's own function. */
	AddExitFunction(AddExitFunctionFuncPair(HARDWARE_Destroy),true);
}

