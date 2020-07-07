/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#if (C_SSHOT)
#include <zlib.h>
#include <png.h>
#include "../libs/zmbv/zmbv.h"
#endif

#include "riff_wav_writer.h"
#include "avi_writer.h"
#include "rawint.h"

#include <map>

#if (C_AVCODEC)
extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

/* This code now requires FFMPEG 4.0.2 or higher */
# if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,18,100)
#  error Your libavcodec is too old. Update FFMPEG.
# endif

#endif

bool            skip_encoding_unchanged_frames = false;

#if (C_AVCODEC)
bool ffmpeg_init = false;
AVFormatContext*	ffmpeg_fmt_ctx = NULL;
AVCodec*		ffmpeg_vid_codec = NULL;
AVCodecContext*		ffmpeg_vid_ctx = NULL;
AVCodec*		ffmpeg_aud_codec = NULL;
AVCodecContext*		ffmpeg_aud_ctx = NULL;
AVStream*		ffmpeg_vid_stream = NULL;
AVStream*		ffmpeg_aud_stream = NULL;
AVFrame*		ffmpeg_aud_frame = NULL;
AVFrame*		ffmpeg_vid_frame = NULL;
AVFrame*		ffmpeg_vidrgb_frame = NULL;
SwsContext*		ffmpeg_sws_ctx = NULL;
bool			ffmpeg_avformat_began = false;
unsigned int		ffmpeg_aud_write = 0;
Bit64u			ffmpeg_audio_sample_counter = 0;
Bit64u			ffmpeg_video_frame_time_offset = 0;
Bit64u			ffmpeg_video_frame_last_time = 0;

int             ffmpeg_yuv_format_choice = -1;  // -1 default  4 = 444   2 = 422   0 = 420

AVPixelFormat ffmpeg_choose_pixfmt(const int x) {
    (void)x;//UNUSED
    if (ffmpeg_yuv_format_choice == 4)
        return AV_PIX_FMT_YUV444P;
    else if (ffmpeg_yuv_format_choice == 2)
        return AV_PIX_FMT_YUV422P;
    else if (ffmpeg_yuv_format_choice == 0)
        return AV_PIX_FMT_YUV420P;

    // default
    // TODO: but this default should also be affected by what the codec supports!
    return AV_PIX_FMT_YUV444P;
}

void ffmpeg_closeall() {
	if (ffmpeg_fmt_ctx != NULL) {
		if (ffmpeg_avformat_began) {
			av_write_trailer(ffmpeg_fmt_ctx);
			ffmpeg_avformat_began = false;
		}
		avio_close(ffmpeg_fmt_ctx->pb);
		avformat_free_context(ffmpeg_fmt_ctx);
		ffmpeg_fmt_ctx = NULL;
	}
	if (ffmpeg_vid_ctx != NULL) {
		avcodec_close(ffmpeg_vid_ctx);
		ffmpeg_vid_ctx = NULL;
	}
	if (ffmpeg_aud_ctx != NULL) {
		avcodec_close(ffmpeg_aud_ctx);
		ffmpeg_aud_ctx = NULL;
	}
	if (ffmpeg_vidrgb_frame != NULL) {
		av_frame_free(&ffmpeg_vidrgb_frame);
		ffmpeg_vidrgb_frame = NULL;
	}
	if (ffmpeg_vid_frame != NULL) {
		av_frame_free(&ffmpeg_vid_frame);
		ffmpeg_vid_frame = NULL;
	}
	if (ffmpeg_aud_frame != NULL) {
		av_frame_free(&ffmpeg_aud_frame);
		ffmpeg_aud_frame = NULL;
	}
	if (ffmpeg_sws_ctx != NULL) {
		sws_freeContext(ffmpeg_sws_ctx);
		ffmpeg_sws_ctx = NULL;
	}
	ffmpeg_video_frame_time_offset = 0;
	ffmpeg_video_frame_last_time = 0;
	ffmpeg_aud_codec = NULL;
	ffmpeg_vid_codec = NULL;
	ffmpeg_vid_stream = NULL;
	ffmpeg_aud_stream = NULL;
	ffmpeg_aud_write = 0;
}

void ffmpeg_audio_frame_send() {
	AVPacket pkt;
	int gotit;

	av_init_packet(&pkt);
	if (av_new_packet(&pkt,65536) == 0) {
		pkt.flags |= AV_PKT_FLAG_KEY;

		if (avcodec_encode_audio2(ffmpeg_aud_ctx,&pkt,ffmpeg_aud_frame,&gotit) == 0) {
			if (gotit) {
				pkt.pts = (int64_t)ffmpeg_audio_sample_counter;
				pkt.dts = (int64_t)ffmpeg_audio_sample_counter;
				pkt.stream_index = ffmpeg_aud_stream->index;
				av_packet_rescale_ts(&pkt,ffmpeg_aud_ctx->time_base,ffmpeg_aud_stream->time_base);

				if (av_interleaved_write_frame(ffmpeg_fmt_ctx,&pkt) < 0)
					LOG_MSG("WARNING: av_interleaved_write_frame failed");
			}
			else {
				LOG_MSG("DEBUG: avcodec_encode_audio2() delayed output");
			}
		}
		else {
			LOG_MSG("WARNING: avcodec_encode_audio2() failed to encode");
		}
	}
	av_packet_unref(&pkt);

	ffmpeg_audio_sample_counter += (Bit64u)ffmpeg_aud_frame->nb_samples;
}

void ffmpeg_take_audio(Bit16s *raw,unsigned int samples) {
	if (ffmpeg_aud_codec == NULL || ffmpeg_aud_frame == NULL || ffmpeg_fmt_ctx == NULL) return;

	if ((unsigned long)ffmpeg_aud_write >= (unsigned long)ffmpeg_aud_frame->nb_samples) {
		ffmpeg_audio_frame_send();
		ffmpeg_aud_write = 0;
	}

	if (ffmpeg_aud_frame->format == AV_SAMPLE_FMT_FLTP) {
		while (samples > 0) {
			unsigned int op = (unsigned int)ffmpeg_aud_frame->nb_samples - ffmpeg_aud_write;

			if (op > samples) op = samples;

			while (op > 0) {
				/* fixed stereo 16-bit -> two channel planar float */
				((float*)ffmpeg_aud_frame->data[0])[ffmpeg_aud_write] = (float)raw[0] / 32768;
				((float*)ffmpeg_aud_frame->data[1])[ffmpeg_aud_write] = (float)raw[1] / 32768;
				ffmpeg_aud_write++;
				samples--;
				raw += 2;
				op--;
			}

			if ((unsigned long)ffmpeg_aud_write >= (unsigned long)ffmpeg_aud_frame->nb_samples) {
				ffmpeg_audio_frame_send();
				ffmpeg_aud_write = 0;
			}
		}
	}
	else if (ffmpeg_aud_frame->format == AV_SAMPLE_FMT_S16) {
		while (samples > 0) {
			unsigned int op = (unsigned int)ffmpeg_aud_frame->nb_samples - ffmpeg_aud_write;

			if (op > samples) op = samples;

			while (op > 0) {
				/* 16-bit stereo -> 16-bit stereo conversion */
				((int16_t*)ffmpeg_aud_frame->data[0])[(ffmpeg_aud_write*2)+0] = raw[0];
				((int16_t*)ffmpeg_aud_frame->data[0])[(ffmpeg_aud_write*2)+1] = raw[1];
				ffmpeg_aud_write++;
				samples--;
				raw += 2;
				op--;
			}

			if ((unsigned long)ffmpeg_aud_write >= (unsigned long)ffmpeg_aud_frame->nb_samples) {
				ffmpeg_audio_frame_send();
				ffmpeg_aud_write = 0;
			}
		}
	}
	else {
		LOG_MSG("WARNING: Audio encoder expects unknown format %u",ffmpeg_aud_frame->format);
	}
}

void ffmpeg_reopen_video(double fps);

void ffmpeg_flush_video() {
	if (ffmpeg_fmt_ctx != NULL) {
		if (ffmpeg_vid_frame != NULL) {
			bool again;
			AVPacket pkt;

			do {
				again=false;
				av_init_packet(&pkt);
				if (av_new_packet(&pkt,50000000/8) == 0) {
					int gotit=0;
					if (avcodec_encode_video2(ffmpeg_vid_ctx,&pkt,NULL,&gotit) == 0) {
						if (gotit) {
							Bit64u tm;

							again = true;
							tm = (uint64_t)pkt.pts;
							pkt.stream_index = ffmpeg_vid_stream->index;
							av_packet_rescale_ts(&pkt,ffmpeg_vid_ctx->time_base,ffmpeg_vid_stream->time_base);
							pkt.pts += (int64_t)ffmpeg_video_frame_time_offset;
							pkt.dts += (int64_t)ffmpeg_video_frame_time_offset;

							if (av_interleaved_write_frame(ffmpeg_fmt_ctx,&pkt) < 0)
								LOG_MSG("WARNING: av_interleaved_write_frame failed");

							pkt.pts = (int64_t)tm + (int64_t)1;
							pkt.dts = (int64_t)tm + (int64_t)1;
							av_packet_rescale_ts(&pkt,ffmpeg_vid_ctx->time_base,ffmpeg_vid_stream->time_base);
							ffmpeg_video_frame_last_time = (uint64_t)pkt.pts;
						}
					}
				}
				av_packet_unref(&pkt);
			} while (again);
		}
	}
}

void ffmpeg_flushout() {
	/* audio */
	if (ffmpeg_fmt_ctx != NULL) {
		if (ffmpeg_aud_frame != NULL) {
			bool again;
			AVPacket pkt;

			if (ffmpeg_aud_write != 0)
				ffmpeg_aud_frame->nb_samples = (int)ffmpeg_aud_write;

			ffmpeg_audio_frame_send();
			ffmpeg_aud_write = 0;
			ffmpeg_aud_frame->nb_samples = 0;

			do {
				again=false;
				av_init_packet(&pkt);
				if (av_new_packet(&pkt,65536) == 0) {
					int gotit=0;
					if (avcodec_encode_audio2(ffmpeg_aud_ctx,&pkt,NULL,&gotit) == 0) {
						if (gotit) {
							again = true;
							pkt.stream_index = ffmpeg_aud_stream->index;
							av_packet_rescale_ts(&pkt,ffmpeg_aud_ctx->time_base,ffmpeg_aud_stream->time_base);

							if (av_interleaved_write_frame(ffmpeg_fmt_ctx,&pkt) < 0)
								LOG_MSG("WARNING: av_interleaved_write_frame failed");
						}
					}
				}
				av_packet_unref(&pkt);
			} while (again);
		}
	}

	ffmpeg_flush_video();
}
#endif

/* FIXME: This needs to be an enum */
bool native_zmbv = false;
bool export_ffmpeg = false;

std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState = 0;

#define WAVE_BUF 16*1024
#define MIDI_BUF 4*1024

static struct {
	struct {
		riff_wav_writer *writer;
		Bit16s buf[WAVE_BUF][2];
		Bitu used;
		Bit32u length;
		Bit32u freq;
    } wave = {};
    struct {
        avi_writer  *writer;
		Bitu		audiorate;
        std::map<std::string,size_t> name_to_stream_index;
    } multitrack_wave = {};
	struct {
		FILE * handle;
		Bit8u buffer[MIDI_BUF];
		Bitu used,done;
		Bit32u last;
    } midi = {};
	struct {
		Bitu rowlen;
    } image = {};
#if (C_SSHOT) || (C_AVCODEC)
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
    } video = {};
#endif
} capture;

#if (C_AVCODEC)
unsigned int GFX_GetBShift();

int ffmpeg_bpp_pick_rgb_format(int bpp) {
	// NTS: This requires some explanation. FFMPEG's pixfmt.h documents RGB555 as (msb)5R 5G 5B(lsb).
	//      So when we typecast two bytes in video ram as uint16_t we get XRRRRRGGGGGBBBBB.
	//
	//      But, RGB24 means that, byte by byte, we get R G B R G B ... which when typecast as uint32_t
	//      on little endian hosts becomes 0xXXBBGGRR.
	//
	//      RGBA would mean byte order R G B A R G B A ... which when typecast as uint32_t little endian
	//      becomes AABBGGRR.
	//
	//      Most of the platforms DOSBox-X runs on tend to arrange 24-bit bpp as uint32_t(0xRRGGBB) little
	//      endian, and 32-bit bpp as uint32_t(0xAARRGGBB).
	//
	//      So if the above text makes any sense, the RGB/BGR switch-up between 15/16bpp and 24/32bpp is
	//      NOT a coding error or a typo.

	// FIXME: Given the above explanation, this code needs to factor RGBA byte order vs host byte order
	//        to correctly specify the color format on any platform.
	if (bpp == 8)
		return (GFX_GetBShift() == 0) ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA; // I can't get FFMPEG to do PAL8 -> YUV444P, so we'll just convert to RGBA on the fly
	else if (bpp == 15)
		return (GFX_GetBShift() == 0) ? AV_PIX_FMT_RGB555 : AV_PIX_FMT_BGR555; // <- not a typo! not a mistake!
	else if (bpp == 16)
		return (GFX_GetBShift() == 0) ? AV_PIX_FMT_RGB565 : AV_PIX_FMT_BGR565; // <- not a typo! not a mistake!
	else if (bpp == 24)
		return (GFX_GetBShift() == 0) ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24;
	else if (bpp == 32)
		return (GFX_GetBShift() == 0) ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA;

	abort();//whut?
	return 0;
}

void ffmpeg_reopen_video(double fps,const int bpp) {
	if (ffmpeg_vid_ctx != NULL) {
		avcodec_close(ffmpeg_vid_ctx);
		ffmpeg_vid_ctx = NULL;
	}
	if (ffmpeg_vidrgb_frame != NULL) {
		av_frame_free(&ffmpeg_vidrgb_frame);
		ffmpeg_vidrgb_frame = NULL;
	}
	if (ffmpeg_vid_frame != NULL) {
		av_frame_free(&ffmpeg_vid_frame);
		ffmpeg_vid_frame = NULL;
	}
	if (ffmpeg_sws_ctx != NULL) {
		sws_freeContext(ffmpeg_sws_ctx);
		ffmpeg_sws_ctx = NULL;
	}

	LOG_MSG("Restarting video codec");

	// FIXME: This is copypasta! Consolidate!
	ffmpeg_vid_ctx = ffmpeg_vid_stream->codec = avcodec_alloc_context3(ffmpeg_vid_codec);
	if (ffmpeg_vid_ctx == NULL) E_Exit("Error: Unable to reopen vid codec");
	avcodec_get_context_defaults3(ffmpeg_vid_ctx,ffmpeg_vid_codec);
	ffmpeg_vid_ctx->bit_rate = 25000000; // TODO: make configuration option!
	ffmpeg_vid_ctx->keyint_min = 15; // TODO: make configuration option!
	ffmpeg_vid_ctx->time_base.num = 1000000;
	ffmpeg_vid_ctx->time_base.den = (int)(1000000 * fps);
	ffmpeg_vid_ctx->width = (int)capture.video.width;
	ffmpeg_vid_ctx->height = (int)capture.video.height;
	ffmpeg_vid_ctx->gop_size = 15; // TODO: make config option
	ffmpeg_vid_ctx->max_b_frames = 0;
	ffmpeg_vid_ctx->pix_fmt = ffmpeg_choose_pixfmt(ffmpeg_yuv_format_choice);
	ffmpeg_vid_ctx->thread_count = 0;		// auto-choose
	ffmpeg_vid_ctx->flags2 = AV_CODEC_FLAG2_FAST;
	ffmpeg_vid_ctx->qmin = 1;
	ffmpeg_vid_ctx->qmax = 63;
	ffmpeg_vid_ctx->rc_max_rate = ffmpeg_vid_ctx->bit_rate;
	ffmpeg_vid_ctx->rc_min_rate = ffmpeg_vid_ctx->bit_rate;
	ffmpeg_vid_ctx->rc_buffer_size = (4*1024*1024);

	/* 4:3 aspect ratio. FFMPEG thinks in terms of Pixel Aspect Ratio not Display Aspect Ratio */
	ffmpeg_vid_ctx->sample_aspect_ratio.num = 4 * (int)capture.video.height;
	ffmpeg_vid_ctx->sample_aspect_ratio.den = 3 * (int)capture.video.width;

	{
		AVDictionary *opts = NULL;

		av_dict_set(&opts,"preset","superfast",1);
		av_dict_set(&opts,"aud","1",1);

		if (avcodec_open2(ffmpeg_vid_ctx,ffmpeg_vid_codec,&opts) < 0) {
			E_Exit("Unable to open H.264 codec");
		}

		av_dict_free(&opts);
	}

	ffmpeg_vid_frame = av_frame_alloc();
	ffmpeg_vidrgb_frame = av_frame_alloc();
	if (ffmpeg_aud_frame == NULL || ffmpeg_vid_frame == NULL || ffmpeg_vidrgb_frame == NULL)
		E_Exit(" ");

	av_frame_set_colorspace(ffmpeg_vidrgb_frame,AVCOL_SPC_RGB);
	ffmpeg_vidrgb_frame->width = (int)capture.video.width;
	ffmpeg_vidrgb_frame->height = (int)capture.video.height;
	ffmpeg_vidrgb_frame->format = ffmpeg_bpp_pick_rgb_format(bpp);
	if (av_frame_get_buffer(ffmpeg_vidrgb_frame,64) < 0) {
		E_Exit(" ");
	}

	av_frame_set_colorspace(ffmpeg_vid_frame,AVCOL_SPC_SMPTE170M);
	av_frame_set_color_range(ffmpeg_vidrgb_frame,AVCOL_RANGE_MPEG);
	ffmpeg_vid_frame->width = (int)capture.video.width;
	ffmpeg_vid_frame->height = (int)capture.video.height;
	ffmpeg_vid_frame->format = ffmpeg_vid_ctx->pix_fmt;
	if (av_frame_get_buffer(ffmpeg_vid_frame,64) < 0)
		E_Exit(" ");

	ffmpeg_sws_ctx = sws_getContext(
			// source
			ffmpeg_vidrgb_frame->width,
			ffmpeg_vidrgb_frame->height,
			(AVPixelFormat)ffmpeg_vidrgb_frame->format,
			// dest
			ffmpeg_vid_frame->width,
			ffmpeg_vid_frame->height,
			(AVPixelFormat)ffmpeg_vid_frame->format,
			// and the rest
			((ffmpeg_vid_frame->width == ffmpeg_vidrgb_frame->width && ffmpeg_vid_frame->height == ffmpeg_vidrgb_frame->height) ? SWS_POINT : SWS_BILINEAR),
			NULL,NULL,NULL);
	if (ffmpeg_sws_ctx == NULL)
		E_Exit(" ");
}
#endif

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
    char tempname[CROSS_LEN], sname[15];
    bool testRead = read_directory_first(dir, tempname, sname, is_directory );
    for ( ; testRead; testRead = read_directory_next(dir, tempname, sname, is_directory) ) {
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
    char tempname[CROSS_LEN], sname[15];
    bool testRead = read_directory_first(dir, tempname, sname, is_directory );
    for ( ; testRead; testRead = read_directory_next(dir, tempname, sname, is_directory) ) {
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

#if (C_SSHOT)
static void CAPTURE_AddAviChunk(const char * tag, Bit32u size, void * data, Bit32u flags, unsigned int streamindex) {
    (void)tag;//UNUSED
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
		CaptureState &= ~((unsigned int)CAPTURE_VIDEO);
		LOG_MSG("Stopped capturing video.");	

		if (capture.video.writer != NULL) {
			if ( capture.video.audioused ) {
				CAPTURE_AddAviChunk( "01wb", (Bit32u)(capture.video.audioused * 4), capture.video.audiobuf, 0x10, 1);
				capture.video.audiowritten = capture.video.audioused*4;
				capture.video.audioused = 0;
			}

			avi_writer_end_data(capture.video.writer);
			avi_writer_finish(capture.video.writer);
			avi_writer_close_file(capture.video.writer);
			capture.video.writer = avi_writer_destroy(capture.video.writer);
		}
#if (C_AVCODEC)
		if (ffmpeg_fmt_ctx != NULL) {
			ffmpeg_flushout();
			ffmpeg_closeall();
		}
#endif

		if (capture.video.buf != NULL) {
			free( capture.video.buf );
			capture.video.buf = NULL;
		}

		if (capture.video.codec != NULL) {
			delete capture.video.codec;
			capture.video.codec = NULL;
		}
	} else {
		CaptureState |= CAPTURE_VIDEO;
	}

	mainMenu.get_item("mapper_video").check(!!(CaptureState & CAPTURE_VIDEO)).refresh_item(mainMenu);
}
#endif

void CAPTURE_StartCapture(void) {
#if (C_SSHOT)
	if (!(CaptureState & CAPTURE_VIDEO))
        CAPTURE_VideoEvent(true);
#endif
}

void CAPTURE_StopCapture(void) {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO)
        CAPTURE_VideoEvent(true);
#endif
}

#if !defined(C_EMSCRIPTEN)
void CAPTURE_WaveEvent(bool pressed);
#endif

void CAPTURE_StartWave(void) {
#if !defined(C_EMSCRIPTEN)
	if (!(CaptureState & CAPTURE_WAVE))
        CAPTURE_WaveEvent(true);
#endif
}

void CAPTURE_StopWave(void) {
#if !defined(C_EMSCRIPTEN)
	if (CaptureState & CAPTURE_WAVE)
        CAPTURE_WaveEvent(true);
#endif
}

#if !defined(C_EMSCRIPTEN)
void CAPTURE_MTWaveEvent(bool pressed);
#endif

void CAPTURE_StartMTWave(void) {
#if !defined(C_EMSCRIPTEN)
	if (!(CaptureState & CAPTURE_MULTITRACK_WAVE))
        CAPTURE_MTWaveEvent(true);
#endif
}

void CAPTURE_StopMTWave(void) {
#if !defined(C_EMSCRIPTEN)
	if (CaptureState & CAPTURE_MULTITRACK_WAVE)
        CAPTURE_MTWaveEvent(true);
#endif
}

#if (C_SSHOT)
extern uint32_t GFX_palette32bpp[256];
#endif

unsigned int GFX_GetBShift();

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

		CaptureState &= ~((unsigned int)CAPTURE_IMAGE);
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
			png_set_IHDR(png_ptr, info_ptr, (png_uint_32)width, (png_uint_32)height,
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
			png_set_IHDR(png_ptr, info_ptr, (png_uint_32)width, (png_uint_32)height,
				8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		}
#ifdef PNG_TEXT_SUPPORTED
		int fields = 1;
		png_text text[1] = {};
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
						doubleRow[x*6+1] = doubleRow[x*6+4] = (Bit8u)(((pixel& 0x03e0) * 0x21) >> 7);
						doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0x7c00) * 0x21) >>  12;
					}
				} else {
					for (Bitu x=0;x<countWidth;x++) {
						Bitu pixel = ((Bit16u *)srcLine)[x];
						doubleRow[x*3+0] = ((pixel& 0x001f) * 0x21) >>  2;
						doubleRow[x*3+1] = (Bit8u)(((pixel& 0x03e0) * 0x21) >> 7);
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
		if (capture.video.width != width ||
			capture.video.height != height ||
			capture.video.bpp != bpp ||
			capture.video.fps != fps) {
			if (native_zmbv && capture.video.writer != NULL)
				CAPTURE_VideoEvent(true);
#if (C_AVCODEC)
			else if (export_ffmpeg && ffmpeg_fmt_ctx != NULL) {
				ffmpeg_flush_video();
				ffmpeg_video_frame_time_offset += ffmpeg_video_frame_last_time;
				ffmpeg_video_frame_last_time = 0;

				capture.video.width = width;
				capture.video.height = height;
				capture.video.bpp = bpp;
				capture.video.fps = fps;
				capture.video.frames = 0;

				ffmpeg_reopen_video(fps,(int)bpp);
//				CAPTURE_VideoEvent(true);
			}
#endif
		}

		CaptureState &= ~((unsigned int)CAPTURE_VIDEO);

		switch (bpp) {
		case 8:format = ZMBV_FORMAT_8BPP;break;
		case 15:format = ZMBV_FORMAT_15BPP;break;
		case 16:format = ZMBV_FORMAT_16BPP;break;
		case 32:format = ZMBV_FORMAT_32BPP;break;
		default:
			goto skip_video;
		}

		if (native_zmbv && capture.video.writer == NULL) {
			std::string path = GetCaptureFilePath("Video",".avi");
			if (path == "")
				goto skip_video;

			capture.video.writer = avi_writer_create();
			if (capture.video.writer == NULL)
				goto skip_video;

			if (!avi_writer_open_file(capture.video.writer,path.c_str()))
				goto skip_video;

            if (!avi_writer_set_stream_writing(capture.video.writer))
                goto skip_video;

			capture.video.codec = new VideoCodec();
			if (!capture.video.codec)
				goto skip_video;
			if (!capture.video.codec->SetupCompress( (int)width, (int)height)) 
				goto skip_video;
			capture.video.bufSize = capture.video.codec->NeededSize((int)width, (int)height, format);
			capture.video.buf = malloc( (size_t)capture.video.bufSize );
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
			__w_le_u32(&mheader->dwWidth,(uint32_t)capture.video.width);
			__w_le_u32(&mheader->dwHeight,(uint32_t)capture.video.height);



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
			__w_le_u32(&vsheader->dwQuality,~0u);
			__w_le_u32(&vsheader->dwSampleSize,0u);
			__w_le_u16(&vsheader->rcFrame.left,0);
			__w_le_u16(&vsheader->rcFrame.top,0);
			__w_le_u16(&vsheader->rcFrame.right,(uint16_t)capture.video.width);
			__w_le_u16(&vsheader->rcFrame.bottom,(uint16_t)capture.video.height);

			windows_BITMAPINFOHEADER vbmp;

			memset(&vbmp,0,sizeof(vbmp));
			__w_le_u32(&vbmp.biSize,sizeof(vbmp)); /* 40 */
			__w_le_u32(&vbmp.biWidth,(uint32_t)capture.video.width);
			__w_le_u32(&vbmp.biHeight,(uint32_t)capture.video.height);
			__w_le_u16(&vbmp.biPlanes,0);		/* FIXME: Only repeating what the original DOSBox code did */
			__w_le_u16(&vbmp.biBitCount,0);		/* FIXME: Only repeating what the original DOSBox code did */
			__w_le_u32(&vbmp.biCompression,avi_fourcc_const('Z','M','B','V'));
			__w_le_u32(&vbmp.biSizeImage,(uint32_t)(capture.video.width * capture.video.height * 4));

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
			__w_le_u32(&asheader->dwRate,(uint32_t)capture.video.audiorate);
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
			__w_le_u32(&fmt.nSamplesPerSec,(uint32_t)capture.video.audiorate);
			__w_le_u16(&fmt.wBitsPerSample,16);		/* 16-bit/sample */
			__w_le_u16(&fmt.nBlockAlign,2*2);
			__w_le_u32(&fmt.nAvgBytesPerSec,(uint32_t)(capture.video.audiorate*2*2));

			if (!avi_writer_stream_set_format(astream,&fmt,sizeof(fmt)))
				goto skip_video;

			if (!avi_writer_begin_header(capture.video.writer) || !avi_writer_begin_data(capture.video.writer))
				goto skip_video;

			LOG_MSG("Started capturing video.");
		}
#if (C_AVCODEC)
		else if (export_ffmpeg && ffmpeg_fmt_ctx == NULL) {
			std::string path = GetCaptureFilePath("Video",".mts"); // Use widely recognized .MTS extension
			if (path == "")
				goto skip_video;

			capture.video.width = width;
			capture.video.height = height;
			capture.video.bpp = bpp;
			capture.video.fps = fps;
			capture.video.frames = 0;
			capture.video.written = 0;
			capture.video.audioused = 0;
			capture.video.audiowritten = 0;
			ffmpeg_audio_sample_counter = 0;

			if (!ffmpeg_init) {
				LOG_MSG("Attempting to initialize FFMPEG library");
				ffmpeg_init = true;
			}

			ffmpeg_aud_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
			ffmpeg_vid_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (ffmpeg_aud_codec == NULL || ffmpeg_vid_codec == NULL) {
				LOG_MSG("H.264 or AAC encoder not available");
				goto skip_video;
			}

			if (avformat_alloc_output_context2(&ffmpeg_fmt_ctx,NULL,"mpegts",NULL) < 0) {
				LOG_MSG("Failed to allocate format context (mpegts)");
				goto skip_video;
			}
			snprintf(ffmpeg_fmt_ctx->filename,sizeof(ffmpeg_fmt_ctx->filename),"%s",path.c_str());

			if (ffmpeg_fmt_ctx->oformat == NULL)
				goto skip_video;

			if (avio_open(&ffmpeg_fmt_ctx->pb,ffmpeg_fmt_ctx->filename,AVIO_FLAG_WRITE) < 0) {
				LOG_MSG("Failed to avio_open");
				goto skip_video;
			}

			ffmpeg_vid_stream = avformat_new_stream(ffmpeg_fmt_ctx,ffmpeg_vid_codec);
			if (ffmpeg_vid_stream == NULL) {
				LOG_MSG("failed to open audio stream");
				goto skip_video;
			}
			ffmpeg_vid_ctx = ffmpeg_vid_stream->codec;
			avcodec_get_context_defaults3(ffmpeg_vid_ctx,ffmpeg_vid_codec);
			ffmpeg_vid_ctx->bit_rate = 25000000; // TODO: make configuration option!
			ffmpeg_vid_ctx->keyint_min = 15; // TODO: make configuration option!
			ffmpeg_vid_ctx->time_base.num = 1000000;
			ffmpeg_vid_ctx->time_base.den = (int)(1000000 * fps);
			ffmpeg_vid_ctx->width = (int)capture.video.width;
			ffmpeg_vid_ctx->height = (int)capture.video.height;
			ffmpeg_vid_ctx->gop_size = 15; // TODO: make config option
			ffmpeg_vid_ctx->max_b_frames = 0;
			ffmpeg_vid_ctx->pix_fmt = ffmpeg_choose_pixfmt(ffmpeg_yuv_format_choice); // TODO: auto-choose according to what codec says is supported, and let user choose as well
			ffmpeg_vid_ctx->thread_count = 0;		// auto-choose
			ffmpeg_vid_ctx->flags2 = AV_CODEC_FLAG2_FAST;
			ffmpeg_vid_ctx->qmin = 1;
			ffmpeg_vid_ctx->qmax = 63;
			ffmpeg_vid_ctx->rc_max_rate = ffmpeg_vid_ctx->bit_rate;
			ffmpeg_vid_ctx->rc_min_rate = ffmpeg_vid_ctx->bit_rate;
			ffmpeg_vid_ctx->rc_buffer_size = (4*1024*1024);

			/* 4:3 aspect ratio. FFMPEG thinks in terms of Pixel Aspect Ratio not Display Aspect Ratio */
			ffmpeg_vid_ctx->sample_aspect_ratio.num = 4 * (int)capture.video.height;
			ffmpeg_vid_ctx->sample_aspect_ratio.den = 3 * (int)capture.video.width;

			{
				AVDictionary *opts = NULL;

				av_dict_set(&opts,"preset","superfast",1);
				av_dict_set(&opts,"aud","1",1);

				if (avcodec_open2(ffmpeg_vid_ctx,ffmpeg_vid_codec,&opts) < 0) {
					LOG_MSG("Unable to open H.264 codec");
					goto skip_video;
				}

				av_dict_free(&opts);
			}

			ffmpeg_vid_stream->time_base.num = (int)1000000;
			ffmpeg_vid_stream->time_base.den = (int)(1000000 * fps);

			ffmpeg_aud_stream = avformat_new_stream(ffmpeg_fmt_ctx,ffmpeg_aud_codec);
			if (ffmpeg_aud_stream == NULL) {
				LOG_MSG("failed to open audio stream");
				goto skip_video;
			}
			ffmpeg_aud_ctx = ffmpeg_aud_stream->codec;
			avcodec_get_context_defaults3(ffmpeg_aud_ctx,ffmpeg_aud_codec);
			ffmpeg_aud_ctx->sample_rate = (int)capture.video.audiorate;
			ffmpeg_aud_ctx->channels = 2;
			ffmpeg_aud_ctx->flags = 0; // do not use global headers
			ffmpeg_aud_ctx->bit_rate = 320000;
			ffmpeg_aud_ctx->profile = FF_PROFILE_AAC_LOW;
			ffmpeg_aud_ctx->channel_layout = AV_CH_LAYOUT_STEREO;

			if (ffmpeg_aud_codec->sample_fmts != NULL)
				ffmpeg_aud_ctx->sample_fmt = (ffmpeg_aud_codec->sample_fmts)[0];
			else
				ffmpeg_aud_ctx->sample_fmt = AV_SAMPLE_FMT_FLT;

			if (avcodec_open2(ffmpeg_aud_ctx,ffmpeg_aud_codec,NULL) < 0) {
				LOG_MSG("Failed to open audio codec");
				goto skip_video;
			}

			ffmpeg_aud_stream->time_base.num = 1;
			ffmpeg_aud_stream->time_base.den = ffmpeg_aud_ctx->sample_rate;

			/* Note whether we started the header.
			 * Writing the trailer out of turn seems to cause segfaults in libavformat */
			ffmpeg_avformat_began = true;

			if (avformat_write_header(ffmpeg_fmt_ctx,NULL) < 0) {
				LOG_MSG("Failed to write header");
				goto skip_video;
			}

			ffmpeg_aud_write = 0;
			ffmpeg_aud_frame = av_frame_alloc();
			ffmpeg_vid_frame = av_frame_alloc();
			ffmpeg_vidrgb_frame = av_frame_alloc();
			if (ffmpeg_aud_frame == NULL || ffmpeg_vid_frame == NULL || ffmpeg_vidrgb_frame == NULL)
				goto skip_video;

			av_frame_set_channels(ffmpeg_aud_frame,2);
			av_frame_set_sample_rate(ffmpeg_aud_frame,(int)capture.video.audiorate);
			av_frame_set_channel_layout(ffmpeg_aud_frame,AV_CH_LAYOUT_STEREO);
			ffmpeg_aud_frame->nb_samples = ffmpeg_aud_ctx->frame_size;
			ffmpeg_aud_frame->format = ffmpeg_aud_ctx->sample_fmt;
			if (av_frame_get_buffer(ffmpeg_aud_frame,16) < 0) {
				LOG_MSG("Failed to alloc audio frame buffer");
				goto skip_video;
			}

			av_frame_set_colorspace(ffmpeg_vidrgb_frame,AVCOL_SPC_RGB);
			ffmpeg_vidrgb_frame->width = (int)capture.video.width;
			ffmpeg_vidrgb_frame->height = (int)capture.video.height;
			ffmpeg_vidrgb_frame->format = ffmpeg_bpp_pick_rgb_format((int)bpp);
			if (av_frame_get_buffer(ffmpeg_vidrgb_frame,64) < 0) {
				LOG_MSG("Failed to alloc videorgb frame buffer");
				goto skip_video;
			}

			av_frame_set_colorspace(ffmpeg_vid_frame,AVCOL_SPC_SMPTE170M);
			av_frame_set_color_range(ffmpeg_vidrgb_frame,AVCOL_RANGE_MPEG);
			ffmpeg_vid_frame->width = (int)capture.video.width;
			ffmpeg_vid_frame->height = (int)capture.video.height;
			ffmpeg_vid_frame->format = ffmpeg_vid_ctx->pix_fmt;
			if (av_frame_get_buffer(ffmpeg_vid_frame,64) < 0) {
				LOG_MSG("Failed to alloc video frame buffer");
				goto skip_video;
			}

			ffmpeg_sws_ctx = sws_getContext(
				// source
				ffmpeg_vidrgb_frame->width,
				ffmpeg_vidrgb_frame->height,
				(AVPixelFormat)ffmpeg_vidrgb_frame->format,
				// dest
				ffmpeg_vid_frame->width,
				ffmpeg_vid_frame->height,
				(AVPixelFormat)ffmpeg_vid_frame->format,
				// and the rest
				((ffmpeg_vid_frame->width == ffmpeg_vidrgb_frame->width && ffmpeg_vid_frame->height == ffmpeg_vidrgb_frame->height) ? SWS_POINT : SWS_BILINEAR),
				NULL,NULL,NULL);
			if (ffmpeg_sws_ctx == NULL) {
				LOG_MSG("Failed to init colorspace conversion");
				goto skip_video;
			}

			LOG_MSG("Started capturing video (FFMPEG)");
		}
#endif

		if (native_zmbv) {
			int codecFlags;

			if (capture.video.frames % 300 == 0)
				codecFlags = 1;
			else
				codecFlags = 0;

            if ((flags & CAPTURE_FLAG_NOCHANGE) && skip_encoding_unchanged_frames) {
                /* advance unless at keyframe */
                if (codecFlags == 0) capture.video.frames++;

                /* write null non-keyframe */
                CAPTURE_AddAviChunk( "00dc", (Bit32u)0, capture.video.buf, (Bit32u)(0x0), 0u);
            }
            else {
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

                CAPTURE_AddAviChunk( "00dc", (Bit32u)written, capture.video.buf, (Bit32u)(codecFlags & 1 ? 0x10 : 0x0), 0u);
                capture.video.frames++;
            }

			if ( capture.video.audioused ) {
				CAPTURE_AddAviChunk( "01wb", (Bit32u)(capture.video.audioused * 4u), capture.video.audiobuf, /*keyframe*/0x10u, 1u);
				capture.video.audiowritten = capture.video.audioused*4;
				capture.video.audioused = 0;
			}
		}
#if (C_AVCODEC)
		else if (export_ffmpeg && ffmpeg_fmt_ctx != NULL) {
			AVPacket pkt;

			// video
			av_init_packet(&pkt);
			if (av_new_packet(&pkt,50000000/8) == 0) {
				unsigned char *srcline,*dstline;
				Bitu x;

				// copy from source to vidrgb frame
				if (bpp == 8 && ffmpeg_vidrgb_frame->format != AV_PIX_FMT_PAL8) {
					for (i=0;i<height;i++) {
						dstline = ffmpeg_vidrgb_frame->data[0] + ((unsigned int)i * (unsigned int)ffmpeg_vidrgb_frame->linesize[0]);

						if (flags & CAPTURE_FLAG_DBLH)
							srcline=(data+(i >> 1)*pitch);
						else
							srcline=(data+(i >> 0)*pitch);

						if (flags & CAPTURE_FLAG_DBLW) {
							for (x=0;x < width;x++)
								((Bit32u *)dstline)[(x*2)+0] =
									((Bit32u *)dstline)[(x*2)+1] = GFX_palette32bpp[srcline[x]];
						}
						else {
							for (x=0;x < width;x++)
								((Bit32u *)dstline)[x] = GFX_palette32bpp[srcline[x]];
						}
					}
				}
				else {
					for (i=0;i<height;i++) {
						dstline = ffmpeg_vidrgb_frame->data[0] + ((unsigned int)i * (unsigned int)ffmpeg_vidrgb_frame->linesize[0]);

						if (flags & CAPTURE_FLAG_DBLW) {
							if (flags & CAPTURE_FLAG_DBLH)
								srcline=(data+(i >> 1)*pitch);
							else
								srcline=(data+(i >> 0)*pitch);

							switch (bpp) {
								case 8:
									for (x=0;x<countWidth;x++)
										((Bit8u *)dstline)[x*2+0] =
											((Bit8u *)dstline)[x*2+1] = ((Bit8u *)srcline)[x];
									break;
								case 15:
								case 16:
									for (x=0;x<countWidth;x++)
										((Bit16u *)dstline)[x*2+0] =
											((Bit16u *)dstline)[x*2+1] = ((Bit16u *)srcline)[x];
									break;
								case 32:
									for (x=0;x<countWidth;x++)
										((Bit32u *)dstline)[x*2+0] =
											((Bit32u *)dstline)[x*2+1] = ((Bit32u *)srcline)[x];
									break;
							}
						} else {
							if (flags & CAPTURE_FLAG_DBLH)
								srcline=(data+(i >> 1)*pitch);
							else
								srcline=(data+(i >> 0)*pitch);

							memcpy(dstline,srcline,width*((bpp+7)/8));
						}
					}
				}

				// convert colorspace
				if (sws_scale(ffmpeg_sws_ctx,
					// source
					ffmpeg_vidrgb_frame->data,
					ffmpeg_vidrgb_frame->linesize,
					0,ffmpeg_vidrgb_frame->height,
					// dest
					ffmpeg_vid_frame->data,
					ffmpeg_vid_frame->linesize) <= 0)
					LOG_MSG("WARNING: sws_scale() failed");

				// encode it
				int gotit=0;
				pkt.pts = (int64_t)capture.video.frames;
				pkt.dts = (int64_t)capture.video.frames;
				ffmpeg_vid_frame->pts = (int64_t)capture.video.frames; // or else libx264 complains about non-monotonic timestamps
				ffmpeg_vid_frame->key_frame = ((capture.video.frames % 15) == 0)?1:0;
				if (avcodec_encode_video2(ffmpeg_vid_ctx,&pkt,ffmpeg_vid_frame,&gotit) == 0) {
					if (gotit) {
						Bit64u tm;

						tm = (Bit64u)pkt.pts;
						pkt.stream_index = ffmpeg_vid_stream->index;
						av_packet_rescale_ts(&pkt,ffmpeg_vid_ctx->time_base,ffmpeg_vid_stream->time_base);
						pkt.pts += (int64_t)ffmpeg_video_frame_time_offset;
						pkt.dts += (int64_t)ffmpeg_video_frame_time_offset;

						if (av_interleaved_write_frame(ffmpeg_fmt_ctx,&pkt) < 0)
							LOG_MSG("WARNING: av_interleaved_write_frame failed");

						pkt.pts = (int64_t)tm + (int64_t)1;
						pkt.dts = (int64_t)tm + (int64_t)1;
						av_packet_rescale_ts(&pkt,ffmpeg_vid_ctx->time_base,ffmpeg_vid_stream->time_base);
						ffmpeg_video_frame_last_time = (uint64_t)pkt.pts;
					}
					else {
						LOG_MSG("DEBUG: avcodec_encode_video2 delayed frame");
						/* delayed frame */
					}
				}
				else {
					LOG_MSG("WARNING: avcodec_encode_video2() failed");
				}
			}
			av_packet_unref(&pkt);
			capture.video.frames++;

			if ( capture.video.audioused ) {
				ffmpeg_take_audio((Bit16s*)capture.video.audiobuf/*NTS: Ewwwwww.... what if the compiler pads the 2-dimensional array?*/,capture.video.audioused);
				capture.video.audiowritten = capture.video.audioused*4;
				capture.video.audioused = 0;
			}
		}
#endif
		else {
			capture.video.audiowritten = capture.video.audioused*4;
			capture.video.audioused = 0;
		}

		/* Everything went okay, set flag again for next frame */
		CaptureState |= CAPTURE_VIDEO;

        mainMenu.get_item("mapper_video").check(!!(CaptureState & CAPTURE_VIDEO)).refresh_item(mainMenu);
    }

	return;
skip_video:
	capture.video.writer = avi_writer_destroy(capture.video.writer);
# if (C_AVCODEC)
	ffmpeg_flushout();
	ffmpeg_closeall();
# endif
#endif
	return;
}


#if (C_SSHOT)
void CAPTURE_ScreenShotEvent(bool pressed) {
	if (!pressed)
		return;
#if !defined(C_EMSCRIPTEN)
	CaptureState |= CAPTURE_IMAGE;
#endif
}
#endif

MixerChannel * MIXER_FirstChannel(void);

void CAPTURE_MultiTrackAddWave(Bit32u freq, Bit32u len, Bit16s * data,const char *name) {
#if !defined(C_EMSCRIPTEN)
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
                    __w_le_u32(&asheader->dwRate,(uint32_t)capture.multitrack_wave.audiorate);
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
                    __w_le_u32(&fmt.nSamplesPerSec,(uint32_t)capture.multitrack_wave.audiorate);
                    __w_le_u16(&fmt.wBitsPerSample,16);		/* 16-bit/sample */
                    __w_le_u16(&fmt.nBlockAlign,2*2);
                    __w_le_u32(&fmt.nAvgBytesPerSec,(uint32_t)(capture.multitrack_wave.audiorate*2*2));

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
#endif
}

void CAPTURE_AddWave(Bit32u freq, Bit32u len, Bit16s * data) {
#if !defined(C_EMSCRIPTEN)
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
			len -= (Bit32u)left;
		}
	}
#endif
}

void CAPTURE_MTWaveEvent(bool pressed) {
	if (!pressed)
		return;

#if !defined(C_EMSCRIPTEN)
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
#endif
}

void CAPTURE_WaveEvent(bool pressed) {
	if (!pressed)
		return;

#if !defined(C_EMSCRIPTEN)
    if (CaptureState & CAPTURE_WAVE) {
        /* Check for previously opened wave file */
        if (capture.wave.writer != NULL) {
            LOG_MSG("Stopped capturing wave output.");
            /* Write last piece of audio in buffer */
            riff_wav_writer_data_write(capture.wave.writer,capture.wave.buf,2*2*capture.wave.used);
            capture.wave.length+=(Bit32u)(capture.wave.used*4);
            riff_wav_writer_end_data(capture.wave.writer);
            capture.wave.writer = riff_wav_writer_destroy(capture.wave.writer);
            CaptureState &= ~((unsigned int)CAPTURE_WAVE);
        }
    }
    else {
        CaptureState |= CAPTURE_WAVE;
    }

	mainMenu.get_item("mapper_recwave").check(!!(CaptureState & CAPTURE_WAVE)).refresh_item(mainMenu);
#endif
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
		capture.midi.last=(Bit32u)PIC_Ticks;
	}
	Bit32u delta=(Bit32u)(PIC_Ticks-capture.midi.last);
	capture.midi.last=(Bit32u)PIC_Ticks;
	RawMidiAddNumber(delta);
	if (sysex) {
		RawMidiAdd( 0xf0 );
		RawMidiAddNumber((Bit32u)len);
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
#if (C_SSHOT)
	if (capture.video.writer != NULL) CAPTURE_VideoEvent(true);
#endif
    if (capture.multitrack_wave.writer) CAPTURE_MTWaveEvent(true);
	if (capture.wave.writer) CAPTURE_WaveEvent(true);
	if (capture.midi.handle) CAPTURE_MidiEvent(true);
}

void OPL_SaveRawEvent(bool pressed);
void CAPTURE_Init() {
	DOSBoxMenu::item *item;

	LOG(LOG_MISC,LOG_DEBUG)("Initializing screenshot and A/V capture system");

	Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(section != NULL);

	// grab and store capture path
	Prop_path *proppath = section->Get_path("captures");
	assert(proppath != NULL);
	capturedir = proppath->realpath;

    skip_encoding_unchanged_frames = section->Get_bool("skip encoding unchanged frames");

    std::string ffmpeg_pixfmt = section->Get_string("capture chroma format");

#if (C_AVCODEC)
    if (ffmpeg_pixfmt == "4:4:4")
        ffmpeg_yuv_format_choice = 4;
    else if (ffmpeg_pixfmt == "4:2:2")
        ffmpeg_yuv_format_choice = 2;
    else if (ffmpeg_pixfmt == "4:2:0")
        ffmpeg_yuv_format_choice = 0;
    else
        ffmpeg_yuv_format_choice = -1;
#endif

	std::string capfmt = section->Get_string("capture format");
	if (capfmt == "mpegts-h264") {
#if (C_AVCODEC)
		LOG_MSG("Capture format is MPEGTS H.264+AAC");
		native_zmbv = false;
		export_ffmpeg = true;
#else
		LOG_MSG("MPEGTS H.264+AAC not compiled in to this DOSBox-X binary. Using AVI+ZMBV");
		native_zmbv = true;
		export_ffmpeg = false;
#endif
	}
	else if (capfmt == "avi-zmbv" || capfmt == "default") {
		LOG_MSG("USING AVI+ZMBV");
		native_zmbv = true;
		export_ffmpeg = false;
	}
	else {
		LOG_MSG("Unknown capture format, using AVI+ZMBV");
		native_zmbv = true;
		export_ffmpeg = false;
	}

	CaptureState = 0; // make sure capture is off

#if !defined(C_EMSCRIPTEN)
	// mapper shortcuts for capture
	MAPPER_AddHandler(CAPTURE_WaveEvent,MK_w,MMOD3|MMODHOST,"recwave","Rec Wave", &item);
	item->set_text("Record audio to WAV");

	MAPPER_AddHandler(CAPTURE_MTWaveEvent,MK_nothing,0,"recmtwave","Rec MTWav", &item);
	item->set_text("Record audio to multi-track AVI");

	MAPPER_AddHandler(CAPTURE_MidiEvent,MK_nothing,0,"caprawmidi","Cap MIDI", &item);
	item->set_text("Record MIDI output");

	MAPPER_AddHandler(OPL_SaveRawEvent,MK_nothing,0,"caprawopl","Cap OPL",&item);
	item->set_text("Record FM (OPL) output");
#if (C_SSHOT)
	MAPPER_AddHandler(CAPTURE_ScreenShotEvent,MK_s,MMOD3|MMODHOST,"scrshot","Screenshot", &item);
	item->set_text("Take screenshot");

	MAPPER_AddHandler(CAPTURE_VideoEvent,MK_v,MMOD3|MMODHOST,"video","Video", &item);
	item->set_text("Record video to AVI");
#endif
#endif

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

#if !defined(C_EMSCRIPTEN)
void update_capture_fmt_menu(void) {
# if (C_SSHOT)
    mainMenu.get_item("capture_fmt_avi_zmbv").check(native_zmbv).refresh_item(mainMenu);
#  if (C_AVCODEC)
    mainMenu.get_item("capture_fmt_mpegts_h264").check(export_ffmpeg).refresh_item(mainMenu);
#  endif
# endif
}
#endif

bool capture_fmt_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;

    const char *ts = menuitem->get_name().c_str();
    Bitu old_CaptureState = CaptureState;
    bool new_native_zmbv = native_zmbv;
    bool new_export_ffmpeg = export_ffmpeg;

    if (!strncmp(ts,"capture_fmt_",12))
        ts += 12;

#if (C_AVCODEC)
    if (!strcmp(ts,"mpegts_h264")) {
        new_native_zmbv = false;
        new_export_ffmpeg = true;
    }
    else
#endif
    {
        new_native_zmbv = true;
        new_export_ffmpeg = false;
    }

    if (native_zmbv != new_native_zmbv || export_ffmpeg != new_export_ffmpeg) {
        void CAPTURE_StopCapture(void);
        CAPTURE_StopCapture();

        native_zmbv = new_native_zmbv;
        export_ffmpeg = new_export_ffmpeg;
    }

    if (old_CaptureState & CAPTURE_VIDEO) {
        void CAPTURE_StartCapture(void);
        CAPTURE_StartCapture();
    }

#if !defined(C_EMSCRIPTEN)
    update_capture_fmt_menu();
#endif
    return true;
}

