/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: hardware.cpp,v 1.23 2009-10-11 18:09:22 qbix79 Exp $ */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dosbox.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "mem.h"
#include "mapper.h"
#include "pic.h"
#include "render.h"
#include "cross.h"

#if (C_SSHOT)
#include <png.h>
#include "../libs/zmbv/zmbv.cpp"
#endif

static std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState;

#define WAVE_BUF 16*1024
#define MIDI_BUF 4*1024
#define AVI_HEADER_SIZE	500

static struct {
	struct {
		FILE * handle;
		Bit16s buf[WAVE_BUF][2];
		Bitu used;
		Bit32u length;
		Bit32u freq;
	} wave; 
	struct {
		FILE * handle;
		Bit8u buffer[MIDI_BUF];
		Bitu used,done;
		Bit32u last;
	} midi;
	struct {
		Bitu rowlen;
	} image;
#if (C_SSHOT)
	struct {
		FILE		*handle;
		Bitu		frames;
		Bit16s		audiobuf[WAVE_BUF][2];
		Bitu		audioused;
		Bitu		audiorate;
		Bitu		audiowritten;
		VideoCodec	*codec;
		Bitu		width, height, bpp;
		Bitu		written;
		float		fps;
		int			bufSize;
		void		*buf;
		Bit8u		*index;
		Bitu		indexsize, indexused;
	} video;
#endif
} capture;

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
		Bitu num=atoi(&tempname[strlen(file_start)]);
		if (num>=last) last=num+1;
	}
	close_directory( dir );
	char file_name[CROSS_LEN];
	sprintf(file_name,"%s%c%s%03d%s",capturedir.c_str(),CROSS_FILESPLIT,file_start,last,ext);
	/* Open the actual file */
	FILE * handle=fopen(file_name,"wb");
	if (handle) {
		LOG_MSG("Capturing %s to %s",type,file_name);
	} else {
		LOG_MSG("Failed to open %s for capturing %s",file_name,type);
	}
	return handle;
}

#if (C_SSHOT)
static void CAPTURE_AddAviChunk(const char * tag, Bit32u size, void * data, Bit32u flags) {
	Bit8u chunk[8];Bit8u *index;Bit32u pos, writesize;

	chunk[0] = tag[0];chunk[1] = tag[1];chunk[2] = tag[2];chunk[3] = tag[3];
	host_writed(&chunk[4], size);   
	/* Write the actual data */
	fwrite(chunk,1,8,capture.video.handle);
	writesize = (size+1)&~1;
	fwrite(data,1,writesize,capture.video.handle);
	pos = capture.video.written + 4;
	capture.video.written += writesize + 8;
	if (capture.video.indexused + 16 >= capture.video.indexsize ) {
		capture.video.index = (Bit8u*)realloc( capture.video.index, capture.video.indexsize + 16 * 4096);
		if (!capture.video.index)
			E_Exit("Ran out of memory during AVI capturing");
		capture.video.indexsize += 16*4096;
	}
	index = capture.video.index+capture.video.indexused;
	capture.video.indexused += 16;
	index[0] = tag[0];
	index[1] = tag[1];
	index[2] = tag[2];
	index[3] = tag[3];
	host_writed(index+4, flags);
	host_writed(index+8, pos);
	host_writed(index+12, size);
}
#endif

#if (C_SSHOT)

static void CAPTURE_VideoHeader() {
		Bit8u avi_header[AVI_HEADER_SIZE];
		Bitu main_list;
		Bitu header_pos=0;
		Bitu save_pos=ftell(capture.video.handle);

#define AVIOUT4(_S_) memcpy(&avi_header[header_pos],_S_,4);header_pos+=4;
#define AVIOUTw(_S_) host_writew(&avi_header[header_pos], _S_);header_pos+=2;
#define AVIOUTd(_S_) host_writed(&avi_header[header_pos], _S_);header_pos+=4;
		/* Try and write an avi header */
		AVIOUT4("RIFF");                    // Riff header 
		AVIOUTd(AVI_HEADER_SIZE + capture.video.written - 8 + capture.video.indexused);
		AVIOUT4("AVI ");
		AVIOUT4("LIST");                    // List header
		main_list = header_pos;
		AVIOUTd(0);				            // TODO size of list
		AVIOUT4("hdrl");

		AVIOUT4("avih");
		AVIOUTd(56);                         /* # of bytes to follow */
		AVIOUTd((Bit32u)(1000000 / capture.video.fps));       /* Microseconds per frame */
		AVIOUTd(0);
		AVIOUTd(0);                         /* PaddingGranularity (whatever that might be) */
		AVIOUTd(0x110);                     /* Flags,0x10 has index, 0x100 interleaved */
		AVIOUTd(capture.video.frames);      /* TotalFrames */
		AVIOUTd(0);                         /* InitialFrames */
		AVIOUTd(2);                         /* Stream count */
		AVIOUTd(0);                         /* SuggestedBufferSize */
		AVIOUTd(capture.video.width);       /* Width */
		AVIOUTd(capture.video.height);      /* Height */
		AVIOUTd(0);                         /* TimeScale:  Unit used to measure time */
		AVIOUTd(0);                         /* DataRate:   Data rate of playback     */
		AVIOUTd(0);                         /* StartTime:  Starting time of AVI data */
		AVIOUTd(0);                         /* DataLength: Size of AVI data chunk    */

		/* Video stream list */
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 40);       /* Size of the list */
		AVIOUT4("strl");
		/* video stream header */
        AVIOUT4("strh");
		AVIOUTd(56);                        /* # of bytes to follow */
		AVIOUT4("vids");                    /* Type */
		AVIOUT4(CODEC_4CC);		            /* Handler */
		AVIOUTd(0);                         /* Flags */
		AVIOUTd(0);                         /* Reserved, MS says: wPriority, wLanguage */
		AVIOUTd(0);                         /* InitialFrames */
		AVIOUTd(1000000);                   /* Scale */
		AVIOUTd((Bit32u)(1000000 * capture.video.fps));              /* Rate: Rate/Scale == samples/second */
		AVIOUTd(0);                         /* Start */
		AVIOUTd(capture.video.frames);      /* Length */
		AVIOUTd(0);                  /* SuggestedBufferSize */
		AVIOUTd(~0);                 /* Quality */
		AVIOUTd(0);                  /* SampleSize */
		AVIOUTd(0);                  /* Frame */
		AVIOUTd(0);                  /* Frame */
		/* The video stream format */
		AVIOUT4("strf");
		AVIOUTd(40);                 /* # of bytes to follow */
		AVIOUTd(40);                 /* Size */
		AVIOUTd(capture.video.width);         /* Width */
		AVIOUTd(capture.video.height);        /* Height */
//		OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
		AVIOUTd(0);
		AVIOUT4(CODEC_4CC);          /* Compression */
		AVIOUTd(capture.video.width * capture.video.height*4);  /* SizeImage (in bytes?) */
		AVIOUTd(0);                  /* XPelsPerMeter */
		AVIOUTd(0);                  /* YPelsPerMeter */
		AVIOUTd(0);                  /* ClrUsed: Number of colors used */
		AVIOUTd(0);                  /* ClrImportant: Number of colors important */

		/* Audio stream list */
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 16);  /* Length of list in bytes */
		AVIOUT4("strl");
		/* The audio stream header */
		AVIOUT4("strh");
		AVIOUTd(56);            /* # of bytes to follow */
		AVIOUT4("auds");
		AVIOUTd(0);             /* Format (Optionally) */
		AVIOUTd(0);             /* Flags */
		AVIOUTd(0);             /* Reserved, MS says: wPriority, wLanguage */
		AVIOUTd(0);             /* InitialFrames */
		AVIOUTd(4);    /* Scale */
		AVIOUTd(capture.video.audiorate*4);             /* Rate, actual rate is scale/rate */
		AVIOUTd(0);             /* Start */
		if (!capture.video.audiorate)
			capture.video.audiorate = 1;
		AVIOUTd(capture.video.audiowritten/4);   /* Length */
		AVIOUTd(0);             /* SuggestedBufferSize */
		AVIOUTd(~0);            /* Quality */
		AVIOUTd(4);				/* SampleSize */
		AVIOUTd(0);             /* Frame */
		AVIOUTd(0);             /* Frame */
		/* The audio stream format */
		AVIOUT4("strf");
		AVIOUTd(16);            /* # of bytes to follow */
		AVIOUTw(1);             /* Format, WAVE_ZMBV_FORMAT_PCM */
		AVIOUTw(2);             /* Number of channels */
		AVIOUTd(capture.video.audiorate);          /* SamplesPerSec */
		AVIOUTd(capture.video.audiorate*4);        /* AvgBytesPerSec*/
		AVIOUTw(4);             /* BlockAlign */
		AVIOUTw(16);            /* BitsPerSample */
		int nmain = header_pos - main_list - 4;
		/* Finish stream list, i.e. put number of bytes in the list to proper pos */

		int njunk = AVI_HEADER_SIZE - 8 - 12 - header_pos;
		AVIOUT4("JUNK");
		AVIOUTd(njunk);
		/* Fix the size of the main list */
		header_pos = main_list;
		AVIOUTd(nmain);
		header_pos = AVI_HEADER_SIZE - 12;
		AVIOUT4("LIST");
		AVIOUTd(capture.video.written+4); /* Length of list in bytes */
		AVIOUT4("movi");
		/* First add the index table to the end */
		memcpy(capture.video.index, "idx1", 4);
		host_writed( capture.video.index+4, capture.video.indexused - 8 );
		fwrite( capture.video.index, 1, capture.video.indexused, capture.video.handle);
		fseek(capture.video.handle, 0, SEEK_SET);
		fwrite(&avi_header, 1, AVI_HEADER_SIZE, capture.video.handle);
		fseek(capture.video.handle, save_pos, SEEK_SET);
}

static void CAPTURE_VideoEvent(bool pressed) {
	if (!pressed)
		return;
	if (CaptureState & CAPTURE_VIDEO) {
		/* Close the video */
		CaptureState &= ~CAPTURE_VIDEO;
		LOG_MSG("Stopped capturing video.");	

		/* Adds AVI header to the file */
		CAPTURE_VideoHeader();

		fclose( capture.video.handle );
		free( capture.video.index );
		free( capture.video.buf );
		delete capture.video.codec;
		capture.video.handle = 0;
	} else {
		CaptureState |= CAPTURE_VIDEO;
	}
}
#endif

void CAPTURE_AddImage(Bitu width, Bitu height, Bitu bpp, Bitu pitch, Bitu flags, float fps, Bit8u * data, Bit8u * pal) {
#if (C_SSHOT)
	Bitu i;
	Bit8u doubleRow[SCALER_MAXWIDTH*4];
	Bitu countWidth = width;

	if (flags & CAPTURE_FLAG_DBLH)
		height *= 2;
	if (flags & CAPTURE_FLAG_DBLW)
		width *= 2;

	if (height > SCALER_MAXHEIGHT)
		return;
	if (width > SCALER_MAXWIDTH)
		return;
	
	if (CaptureState & CAPTURE_IMAGE) {
		png_structp png_ptr;
		png_infop info_ptr;
		png_color palette[256];

		CaptureState &= ~CAPTURE_IMAGE;
		/* Open the actual file */
		FILE * fp=OpenCaptureFile("Screenshot",".png");
		if (!fp) goto skip_shot;
		/* First try to alloacte the png structures */
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
		if (!png_ptr) goto skip_shot;
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr) {
			png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
			goto skip_shot;
		}
	
		/* Finalize the initing of png library */
		png_init_io(png_ptr, fp);
		png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);
		
		/* set other zlib parameters */
		png_set_compression_mem_level(png_ptr, 8);
		png_set_compression_strategy(png_ptr,Z_DEFAULT_STRATEGY);
		png_set_compression_window_bits(png_ptr, 15);
		png_set_compression_method(png_ptr, 8);
		png_set_compression_buffer_size(png_ptr, 8192);
	
		if (bpp==8) {
			png_set_IHDR(png_ptr, info_ptr, width, height,
				8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			for (i=0;i<256;i++) {
				palette[i].red=pal[i*4+0];
				palette[i].green=pal[i*4+1];
				palette[i].blue=pal[i*4+2];
			}
			png_set_PLTE(png_ptr, info_ptr, palette,256);
		} else {
			png_set_bgr( png_ptr );
			png_set_IHDR(png_ptr, info_ptr, width, height,
				8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		}
		png_write_info(png_ptr, info_ptr);
		for (i=0;i<height;i++) {
			void *rowPointer;
			void *srcLine;
			if (flags & CAPTURE_FLAG_DBLH)
				srcLine=(data+(i >> 1)*pitch);
			else
				srcLine=(data+(i >> 0)*pitch);
			rowPointer=srcLine;
			switch (bpp) {
			case 8:
				if (flags & CAPTURE_FLAG_DBLW) {
   					for (Bitu x=0;x<countWidth;x++)
						doubleRow[x*2+0] =
						doubleRow[x*2+1] = ((Bit8u *)srcLine)[x];
					rowPointer = doubleRow;
				}
				break;
			case 15:
				if (flags & CAPTURE_FLAG_DBLW) {
					for (Bitu x=0;x<countWidth;x++) {
						Bitu pixel = ((Bit16u *)srcLine)[x];
						doubleRow[x*6+0] = doubleRow[x*6+3] = ((pixel& 0x001f) * 0x21) >>  2;
						doubleRow[x*6+1] = doubleRow[x*6+4] = ((pixel& 0x03e0) * 0x21) >>  7;
						doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0x7c00) * 0x21) >>  12;
					}
				} else {
					for (Bitu x=0;x<countWidth;x++) {
						Bitu pixel = ((Bit16u *)srcLine)[x];
						doubleRow[x*3+0] = ((pixel& 0x001f) * 0x21) >>  2;
						doubleRow[x*3+1] = ((pixel& 0x03e0) * 0x21) >>  7;
						doubleRow[x*3+2] = ((pixel& 0x7c00) * 0x21) >>  12;
					}
				}
				rowPointer = doubleRow;
				break;
			case 16:
				if (flags & CAPTURE_FLAG_DBLW) {
					for (Bitu x=0;x<countWidth;x++) {
						Bitu pixel = ((Bit16u *)srcLine)[x];
						doubleRow[x*6+0] = doubleRow[x*6+3] = ((pixel& 0x001f) * 0x21) >> 2;
						doubleRow[x*6+1] = doubleRow[x*6+4] = ((pixel& 0x07e0) * 0x41) >> 9;
						doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0xf800) * 0x21) >> 13;
					}
				} else {
					for (Bitu x=0;x<countWidth;x++) {
						Bitu pixel = ((Bit16u *)srcLine)[x];
						doubleRow[x*3+0] = ((pixel& 0x001f) * 0x21) >>  2;
						doubleRow[x*3+1] = ((pixel& 0x07e0) * 0x41) >>  9;
						doubleRow[x*3+2] = ((pixel& 0xf800) * 0x21) >>  13;
					}
				}
				rowPointer = doubleRow;
				break;
			case 32:
				if (flags & CAPTURE_FLAG_DBLW) {
					for (Bitu x=0;x<countWidth;x++) {
						doubleRow[x*6+0] = doubleRow[x*6+3] = ((Bit8u *)srcLine)[x*4+0];
						doubleRow[x*6+1] = doubleRow[x*6+4] = ((Bit8u *)srcLine)[x*4+1];
						doubleRow[x*6+2] = doubleRow[x*6+5] = ((Bit8u *)srcLine)[x*4+2];
					}
				} else {
					for (Bitu x=0;x<countWidth;x++) {
						doubleRow[x*3+0] = ((Bit8u *)srcLine)[x*4+0];
						doubleRow[x*3+1] = ((Bit8u *)srcLine)[x*4+1];
						doubleRow[x*3+2] = ((Bit8u *)srcLine)[x*4+2];
					}
				}
				rowPointer = doubleRow;
				break;
			}
			png_write_row(png_ptr, (png_bytep)rowPointer);
		}
		/* Finish writing */
		png_write_end(png_ptr, 0);
		/*Destroy PNG structs*/
		png_destroy_write_struct(&png_ptr, &info_ptr);
		/*close file*/
		fclose(fp);
	}
skip_shot:
	if (CaptureState & CAPTURE_VIDEO) {
		zmbv_format_t format;
		/* Disable capturing if any of the test fails */
		if (capture.video.handle && (
			capture.video.width != width ||
			capture.video.height != height ||
			capture.video.bpp != bpp ||
			capture.video.fps != fps)) 
		{
			CAPTURE_VideoEvent(true);
		}
		CaptureState &= ~CAPTURE_VIDEO;
		switch (bpp) {
		case 8:format = ZMBV_FORMAT_8BPP;break;
		case 15:format = ZMBV_FORMAT_15BPP;break;
		case 16:format = ZMBV_FORMAT_16BPP;break;
		case 32:format = ZMBV_FORMAT_32BPP;break;
		default:
			goto skip_video;
		}
		if (!capture.video.handle) {
			capture.video.handle = OpenCaptureFile("Video",".avi");
			if (!capture.video.handle)
				goto skip_video;
			capture.video.codec = new VideoCodec();
			if (!capture.video.codec)
				goto skip_video;
			if (!capture.video.codec->SetupCompress( width, height)) 
				goto skip_video;
			capture.video.bufSize = capture.video.codec->NeededSize(width, height, format);
			capture.video.buf = malloc( capture.video.bufSize );
			if (!capture.video.buf)
				goto skip_video;
			capture.video.index = (Bit8u*)malloc( 16*4096 );
			if (!capture.video.buf)
				goto skip_video;
			capture.video.indexsize = 16*4096;
			capture.video.indexused = 8;

			capture.video.width = width;
			capture.video.height = height;
			capture.video.bpp = bpp;
			capture.video.fps = fps;
			for (i=0;i<AVI_HEADER_SIZE;i++)
				fputc(0,capture.video.handle);
			capture.video.frames = 0;
			capture.video.written = 0;
			capture.video.audioused = 0;
			capture.video.audiowritten = 0;
		}
		int codecFlags;
		if (capture.video.frames % 300 == 0)
			codecFlags = 1;
		else codecFlags = 0;
		if (!capture.video.codec->PrepareCompressFrame( codecFlags, format, (char *)pal, capture.video.buf, capture.video.bufSize))
			goto skip_video;

		for (i=0;i<height;i++) {
			void * rowPointer;
			if (flags & CAPTURE_FLAG_DBLW) {
				void *srcLine;
				Bitu x;
				Bitu countWidth = width >> 1;
				if (flags & CAPTURE_FLAG_DBLH)
					srcLine=(data+(i >> 1)*pitch);
				else
					srcLine=(data+(i >> 0)*pitch);
				switch ( bpp) {
				case 8:
					for (x=0;x<countWidth;x++)
						((Bit8u *)doubleRow)[x*2+0] =
						((Bit8u *)doubleRow)[x*2+1] = ((Bit8u *)srcLine)[x];
					break;
				case 15:
				case 16:
					for (x=0;x<countWidth;x++)
						((Bit16u *)doubleRow)[x*2+0] =
						((Bit16u *)doubleRow)[x*2+1] = ((Bit16u *)srcLine)[x];
					break;
				case 32:
					for (x=0;x<countWidth;x++)
						((Bit32u *)doubleRow)[x*2+0] =
						((Bit32u *)doubleRow)[x*2+1] = ((Bit32u *)srcLine)[x];
					break;
				}
                rowPointer=doubleRow;
			} else {
				if (flags & CAPTURE_FLAG_DBLH)
					rowPointer=(data+(i >> 1)*pitch);
				else
					rowPointer=(data+(i >> 0)*pitch);
			}
			capture.video.codec->CompressLines( 1, &rowPointer );
		}
		int written = capture.video.codec->FinishCompressFrame();
		if (written < 0)
			goto skip_video;
		CAPTURE_AddAviChunk( "00dc", written, capture.video.buf, codecFlags & 1 ? 0x10 : 0x0);
		capture.video.frames++;
//		LOG_MSG("Frame %d video %d audio %d",capture.video.frames, written, capture.video.audioused *4 );
		if ( capture.video.audioused ) {
			CAPTURE_AddAviChunk( "01wb", capture.video.audioused * 4, capture.video.audiobuf, 0);
			capture.video.audiowritten = capture.video.audioused*4;
			capture.video.audioused = 0;
		}

		/* Adds AVI header to the file */
		CAPTURE_VideoHeader();

		/* Everything went okay, set flag again for next frame */
		CaptureState |= CAPTURE_VIDEO;
	}
skip_video:
#endif
	return;
}


#if (C_SSHOT)
static void CAPTURE_ScreenShotEvent(bool pressed) {
	if (!pressed)
		return;
	CaptureState |= CAPTURE_IMAGE;
}
#endif


/* WAV capturing */
static Bit8u wavheader[]={
	'R','I','F','F',	0x0,0x0,0x0,0x0,		/* Bit32u Riff Chunk ID /  Bit32u riff size */
	'W','A','V','E',	'f','m','t',' ',		/* Bit32u Riff Format  / Bit32u fmt chunk id */
	0x10,0x0,0x0,0x0,	0x1,0x0,0x2,0x0,		/* Bit32u fmt size / Bit16u encoding/ Bit16u channels */
	0x0,0x0,0x0,0x0,	0x0,0x0,0x0,0x0,		/* Bit32u freq / Bit32u byterate */
	0x4,0x0,0x10,0x0,	'd','a','t','a',		/* Bit16u byte-block / Bit16u bits / Bit16u data chunk id */
	0x0,0x0,0x0,0x0,							/* Bit32u data size */
};

void CAPTURE_AddWave(Bit32u freq, Bit32u len, Bit16s * data) {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		Bitu left = WAVE_BUF - capture.video.audioused;
		if (left > len)
			left = len;
		memcpy( &capture.video.audiobuf[capture.video.audioused], data, left*4);
		capture.video.audioused += left;
		capture.video.audiorate = freq;
	}
#endif
	if (CaptureState & CAPTURE_WAVE) {
		if (!capture.wave.handle) {
			capture.wave.handle=OpenCaptureFile("Wave Output",".wav");
			if (!capture.wave.handle) {
				CaptureState &= ~CAPTURE_WAVE;
				return;
			}
			capture.wave.length = 0;
			capture.wave.used = 0;
			capture.wave.freq = freq;
			fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		}
		Bit16s * read = data;
		while (len > 0 ) {
			Bitu left = WAVE_BUF - capture.wave.used;
			if (!left) {
				fwrite(capture.wave.buf,1,4*WAVE_BUF,capture.wave.handle);
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
static void CAPTURE_WaveEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.wave.handle) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		fwrite(capture.wave.buf,1,capture.wave.used*4,capture.wave.handle);
		capture.wave.length+=capture.wave.used*4;
		/* Fill in the header with useful information */
		host_writed(&wavheader[0x04],capture.wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],capture.wave.freq);
		host_writed(&wavheader[0x1C],capture.wave.freq*4);
		host_writed(&wavheader[0x28],capture.wave.length);
		
		fseek(capture.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		fclose(capture.wave.handle);
		capture.wave.handle=0;
		CaptureState |= CAPTURE_WAVE;
	} 
	CaptureState ^= CAPTURE_WAVE;
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

static void CAPTURE_MidiEvent(bool pressed) {
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
		CaptureState &= ~CAPTURE_MIDI;
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
}

class HARDWARE:public Module_base{
public:
	HARDWARE(Section* configuration):Module_base(configuration){
		Section_prop * section = static_cast<Section_prop *>(configuration);
		Prop_path* proppath= section->Get_path("captures");
		capturedir = proppath->realpath;
		CaptureState = 0;
		MAPPER_AddHandler(CAPTURE_WaveEvent,MK_f6,MMOD1,"recwave","Rec Wave");
		MAPPER_AddHandler(CAPTURE_MidiEvent,MK_f8,MMOD1|MMOD2,"caprawmidi","Cap MIDI");
#if (C_SSHOT)
		MAPPER_AddHandler(CAPTURE_ScreenShotEvent,MK_f5,MMOD1,"scrshot","Screenshot");
		MAPPER_AddHandler(CAPTURE_VideoEvent,MK_f5,MMOD1|MMOD2,"video","Video");
#endif
	}
	~HARDWARE(){
		if (capture.wave.handle) CAPTURE_WaveEvent(true);
		if (capture.midi.handle) CAPTURE_MidiEvent(true);
	}
};

static HARDWARE* test;

void HARDWARE_Destroy(Section * sec) {
	delete test;
}

void HARDWARE_Init(Section * sec) {
	test = new HARDWARE(sec);
	sec->AddDestroyFunction(&HARDWARE_Destroy,true);
}
