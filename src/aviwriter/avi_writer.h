/*
 *  Copyright (C) 2018-2020 Jon Campbell
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

#ifndef __ISP_UTILS_AVI_WRITER_H
#define __ISP_UTILS_AVI_WRITER_H

#include "avi.h"

enum {
	AVI_WRITER_STATE_INIT=0,
	AVI_WRITER_STATE_HEADER,
	AVI_WRITER_STATE_BODY,
	AVI_WRITER_STATE_FOOTER,
	AVI_WRITER_STATE_DONE
};

typedef struct avi_writer_stream_index {
	uint64_t			stream_offset;
	uint64_t			offset;
	uint32_t			length;
	uint32_t			dwFlags;	/* flags in AVIOLDINDEX. In OpenDML 2.0 indexes, only the keyframe bit has meaning */
} avi_writer_stream_index;

typedef struct avi_writer_stream {
	int				index;
	riff_strh_AVISTREAMHEADER	header;
	riff_indx_AVISUPERINDEX		superindex;
    const char*         name;
	void*				format;
	size_t				format_len;
	riff_chunk			strh,indx,indx_junk;
	avi_writer_stream_index*	sample_index;
	unsigned int			sample_index_alloc,sample_index_max;
	unsigned int			sample_write_offset;
	unsigned int			sample_write_chunk;
	unsigned int			indx_entryofs;
	uint32_t			chunk_fourcc;	/* such as: 00dc, 00db, 00wb, etc. */
	uint32_t			group0_len;
} avi_writer_stream;

typedef struct avi_writer {
	int				fd,own_fd;	/* file descriptor, and whether we retain ownership of it */
	riff_stack*			riff;
	riff_chunk			movi,avih;
	int				state;
	int				avi_stream_max;
	int				avi_stream_alloc;
	avi_writer_stream*		avi_stream;
	riff_avih_AVIMAINHEADER		main_header;
	unsigned char			enable_opendml_index;	/* 1=write OpenDML index */
	unsigned char			enable_avioldindex;	/* 1=write AVIOLDINDEX */
	unsigned char			enable_opendml;		/* 1=enable RIFF:AVIX extensions to allow files >= 2GB */
	unsigned char			enable_stream_writing;	/* 1=try to always write as a stream to disk at the expense of some AVI structure integrity */
	unsigned char			wrote_idx1;
	unsigned int			group;
} avi_writer;

int avi_writer_stream_check_samplecount(avi_writer_stream *s,unsigned int len);
int avi_writer_stream_set_format(avi_writer_stream *s,void *data,size_t len);
riff_strh_AVISTREAMHEADER *avi_writer_stream_header(avi_writer_stream *s);
riff_avih_AVIMAINHEADER *avi_writer_main_header(avi_writer *w);
avi_writer_stream *avi_writer_new_stream(avi_writer *w); /* reminder: you are not required to free this pointer, the writer does it for you on close_file() */
void avi_writer_free_stream(avi_writer_stream *s);
void avi_writer_free_streams(avi_writer *w);
avi_writer *avi_writer_create();
void avi_writer_close_file(avi_writer *w);
int avi_writer_open_file(avi_writer *w,const char *path);
avi_writer *avi_writer_destroy(avi_writer *w);
int avi_writer_begin_data(avi_writer *w);
int avi_writer_begin_header(avi_writer *w);
int avi_writer_stream_write(avi_writer *w,avi_writer_stream *s,void *data,size_t len,uint32_t flags);
int avi_writer_stream_repeat_last_chunk(avi_writer *w,avi_writer_stream *s);
int avi_writer_set_stream_writing(avi_writer *w);
int avi_writer_end_data(avi_writer *w);
int avi_writer_finish(avi_writer *w);

/* TODO: remove from header, these functions are private */
uint64_t avi_writer_stream_alloc_superindex(avi_writer *w,avi_writer_stream *s);
int avi_writer_emit_avioldindex(avi_writer *w);
int avi_writer_emit_opendml_indexes(avi_writer *w);
int avi_writer_update_avi_and_stream_headers(avi_writer *w);

#endif /* __ISP_UTILS_AVI_WRITER_H */

