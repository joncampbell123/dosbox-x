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

#include "riff_wav_writer.h"
#include "avi_writer.h"
#include "rawint.h"

std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState;

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
		avi_writer	*writer;
		Bitu		frames;
		Bit16s		audiobuf[WAVE_BUF][2];
		Bitu		audioused;
		Bitu		audiorate;
		Bitu		audiowritten;
		VideoCodec	*codec;
		Bitu		width, height, bpp;
		Bitu		written;
		float		fps;
		int		bufSize;
		void		*buf;
	} video;
#endif
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
		Bitu num=atoi(&tempname[strlen(file_start)]);
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
		Bitu num=atoi(&tempname[strlen(file_start)]);
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

#if (C_SSHOT)
static void CAPTURE_AddAviChunk(const char * tag, Bit32u size, void * data, Bit32u flags, unsigned int streamindex) {
	if (capture.video.writer != NULL) {
		if ((int)streamindex < capture.video.writer->avi_stream_alloc) {
			avi_writer_stream *os = capture.video.writer->avi_stream + streamindex;
			avi_writer_stream_write(capture.video.writer,os,data,size,flags);
		}
	}
}
#endif

#if (C_SSHOT)
void CAPTURE_VideoEvent(bool pressed) {
	if (!pressed)
		return;
	if (CaptureState & CAPTURE_VIDEO) {
		/* Close the video */
		CaptureState &= ~CAPTURE_VIDEO;
		LOG_MSG("Stopped capturing video.");	

		if (capture.video.writer != NULL) {
			avi_writer_end_data(capture.video.writer);
			avi_writer_finish(capture.video.writer);
			avi_writer_close_file(capture.video.writer);
			capture.video.writer = avi_writer_destroy(capture.video.writer);
		}

		free( capture.video.buf );
		delete capture.video.codec;
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
		/* First try to allocate the png structures */
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
#ifdef PNG_TEXT_SUPPORTED
		int fields = 1;
		png_text text[1];
		const char* text_s = "DOSBox " VERSION;
		size_t strl = strlen(text_s);
		char* ptext_s = new char[strl + 1];
		strcpy(ptext_s, text_s);
		char software[9] = { 'S','o','f','t','w','a','r','e',0};
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		text[0].key  = software;
		text[0].text = ptext_s;
		png_set_text(png_ptr, info_ptr, text, fields);
#endif
		png_write_info(png_ptr, info_ptr);
#ifdef PNG_TEXT_SUPPORTED
		delete [] ptext_s;
#endif
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
		if (capture.video.writer != NULL && (
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
		if (capture.video.writer == NULL) {
			std::string path = GetCaptureFilePath("Video",".avi");
			if (path == "")
				goto skip_video;

			capture.video.writer = avi_writer_create();
			if (capture.video.writer == NULL)
				goto skip_video;

			if (!avi_writer_open_file(capture.video.writer,path.c_str()))
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

			capture.video.width = width;
			capture.video.height = height;
			capture.video.bpp = bpp;
			capture.video.fps = fps;
			capture.video.frames = 0;
			capture.video.written = 0;
			capture.video.audioused = 0;
			capture.video.audiowritten = 0;

			riff_avih_AVIMAINHEADER *mheader = avi_writer_main_header(capture.video.writer);
			if (mheader == NULL)
				goto skip_video;

			memset(mheader,0,sizeof(*mheader));
			__w_le_u32(&mheader->dwMicroSecPerFrame,(uint32_t)(1000000 / fps)); /* NTS: int divided by double */
			__w_le_u32(&mheader->dwMaxBytesPerSec,0);
			__w_le_u32(&mheader->dwPaddingGranularity,0);
			__w_le_u32(&mheader->dwFlags,0x110);                     /* Flags,0x10 has index, 0x100 interleaved */
			__w_le_u32(&mheader->dwTotalFrames,0);			/* AVI writer updates this automatically on finish */
			__w_le_u32(&mheader->dwInitialFrames,0);
			__w_le_u32(&mheader->dwStreams,2);			/* audio+video */
			__w_le_u32(&mheader->dwSuggestedBufferSize,0);
			__w_le_u32(&mheader->dwWidth,capture.video.width);
			__w_le_u32(&mheader->dwHeight,capture.video.height);



			avi_writer_stream *vstream = avi_writer_new_stream(capture.video.writer);
			if (vstream == NULL)
				goto skip_video;

			riff_strh_AVISTREAMHEADER *vsheader = avi_writer_stream_header(vstream);
			if (vsheader == NULL)
				goto skip_video;

			memset(vsheader,0,sizeof(*vsheader));
			__w_le_u32(&vsheader->fccType,avi_fccType_video);
			__w_le_u32(&vsheader->fccHandler,avi_fourcc_const('Z','M','B','V'));
			__w_le_u32(&vsheader->dwFlags,0);
			__w_le_u16(&vsheader->wPriority,0);
			__w_le_u16(&vsheader->wLanguage,0);
			__w_le_u32(&vsheader->dwInitialFrames,0);
			__w_le_u32(&vsheader->dwScale,1000000);
			__w_le_u32(&vsheader->dwRate,(uint32_t)(1000000 * fps));
			__w_le_u32(&vsheader->dwStart,0);
			__w_le_u32(&vsheader->dwLength,0);			/* AVI writer updates this automatically */
			__w_le_u32(&vsheader->dwSuggestedBufferSize,0);
			__w_le_u32(&vsheader->dwQuality,~0);
			__w_le_u32(&vsheader->dwSampleSize,0);
			__w_le_u16(&vsheader->rcFrame.left,0);
			__w_le_u16(&vsheader->rcFrame.top,0);
			__w_le_u16(&vsheader->rcFrame.right,capture.video.width);
			__w_le_u16(&vsheader->rcFrame.bottom,capture.video.height);

			windows_BITMAPINFOHEADER vbmp;

			memset(&vbmp,0,sizeof(vbmp));
			__w_le_u32(&vbmp.biSize,sizeof(vbmp)); /* 40 */
			__w_le_u32(&vbmp.biWidth,capture.video.width);
			__w_le_u32(&vbmp.biHeight,capture.video.height);
			__w_le_u16(&vbmp.biPlanes,0);		/* FIXME: Only repeating what the original DOSBox code did */
			__w_le_u16(&vbmp.biBitCount,0);		/* FIXME: Only repeating what the original DOSBox code did */
			__w_le_u32(&vbmp.biCompression,avi_fourcc_const('Z','M','B','V'));
			__w_le_u32(&vbmp.biSizeImage,capture.video.width * capture.video.height * 4);

			if (!avi_writer_stream_set_format(vstream,&vbmp,sizeof(vbmp)))
				goto skip_video;


			avi_writer_stream *astream = avi_writer_new_stream(capture.video.writer);
			if (astream == NULL)
				goto skip_video;

			riff_strh_AVISTREAMHEADER *asheader = avi_writer_stream_header(astream);
			if (asheader == NULL)
				goto skip_video;

			memset(asheader,0,sizeof(*asheader));
			__w_le_u32(&asheader->fccType,avi_fccType_audio);
			__w_le_u32(&asheader->fccHandler,0);
			__w_le_u32(&asheader->dwFlags,0);
			__w_le_u16(&asheader->wPriority,0);
			__w_le_u16(&asheader->wLanguage,0);
			__w_le_u32(&asheader->dwInitialFrames,0);
			__w_le_u32(&asheader->dwScale,1);
			__w_le_u32(&asheader->dwRate,capture.video.audiorate);
			__w_le_u32(&asheader->dwStart,0);
			__w_le_u32(&asheader->dwLength,0);			/* AVI writer updates this automatically */
			__w_le_u32(&asheader->dwSuggestedBufferSize,0);
			__w_le_u32(&asheader->dwQuality,~0);
			__w_le_u32(&asheader->dwSampleSize,2*2);
			__w_le_u16(&asheader->rcFrame.left,0);
			__w_le_u16(&asheader->rcFrame.top,0);
			__w_le_u16(&asheader->rcFrame.right,0);
			__w_le_u16(&asheader->rcFrame.bottom,0);

			windows_WAVEFORMAT fmt;

			memset(&fmt,0,sizeof(fmt));
			__w_le_u16(&fmt.wFormatTag,windows_WAVE_FORMAT_PCM);
			__w_le_u16(&fmt.nChannels,2);			/* stereo */
			__w_le_u32(&fmt.nSamplesPerSec,capture.video.audiorate);
			__w_le_u16(&fmt.wBitsPerSample,16);		/* 16-bit/sample */
			__w_le_u16(&fmt.nBlockAlign,2*2);
			__w_le_u32(&fmt.nAvgBytesPerSec,capture.video.audiorate*2*2);

			if (!avi_writer_stream_set_format(astream,&fmt,sizeof(fmt)))
				goto skip_video;


			if (!avi_writer_begin_header(capture.video.writer) || !avi_writer_begin_data(capture.video.writer))
				goto skip_video;

			LOG_MSG("Started capturing video.");
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
		CAPTURE_AddAviChunk( "00dc", written, capture.video.buf, codecFlags & 1 ? 0x10 : 0x0, 0);
		capture.video.frames++;
//		LOG_MSG("Frame %d video %d audio %d",capture.video.frames, written, capture.video.audioused *4 );
		if ( capture.video.audioused ) {
			CAPTURE_AddAviChunk( "01wb", capture.video.audioused * 4, capture.video.audiobuf, 0, 1);
			capture.video.audiowritten = capture.video.audioused*4;
			capture.video.audioused = 0;
		}

		/* Everything went okay, set flag again for next frame */
		CaptureState |= CAPTURE_VIDEO;
	}

	return;
skip_video:
	capture.video.writer = avi_writer_destroy(capture.video.writer);
#endif
	return;
}


#if (C_SSHOT)
void CAPTURE_ScreenShotEvent(bool pressed) {
	if (!pressed)
		return;
	CaptureState |= CAPTURE_IMAGE;
}
#endif

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
		if (capture.wave.writer == NULL) {
			std::string path = GetCaptureFilePath("Wave Output",".wav");
			if (path == "") {
				CaptureState &= ~CAPTURE_WAVE;
				return;
			}

			capture.wave.writer = riff_wav_writer_create();
			if (capture.wave.writer == NULL) {
				CaptureState &= ~CAPTURE_WAVE;
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
				CaptureState &= ~CAPTURE_WAVE;
				capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
				return;
			}
			if (!riff_wav_writer_set_format(capture.wave.writer,&fmt) ||
				!riff_wav_writer_begin_header(capture.wave.writer) ||
				!riff_wav_writer_begin_data(capture.wave.writer)) {
				CaptureState &= ~CAPTURE_WAVE;
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
void CAPTURE_WaveEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.wave.writer != NULL) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		riff_wav_writer_data_write(capture.wave.writer,capture.wave.buf,2*2*capture.wave.used);
		capture.wave.length+=capture.wave.used*4;
		riff_wav_writer_end_data(capture.wave.writer);
		capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
		CaptureState &= ~CAPTURE_WAVE;
	} 
	else {
		CaptureState ^= CAPTURE_WAVE;
	}
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
#if (C_SSHOT)
		if (capture.video.writer != NULL) CAPTURE_VideoEvent(true);
#endif
		if (capture.wave.writer) CAPTURE_WaveEvent(true);
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
