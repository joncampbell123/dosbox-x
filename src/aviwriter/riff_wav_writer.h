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

#ifndef __ISP_UTILS_V4_AVI_RIFF_WAV_WRITER_H
#define __ISP_UTILS_V4_AVI_RIFF_WAV_WRITER_H

#include <stdint.h>
#include "riff.h"
#include "waveformatex.h"
#include "bitmapinfoheader.h"

typedef struct riff_wav_writer {
	riff_stack*		riff;
	int			state;
	int			fd,own_fd;
	void*			fmt;
	size_t			fmt_len;
} riff_wav_writer;

enum {
	RIFF_WRITER_INIT,
	RIFF_WRITER_HEADER,
	RIFF_WRITER_DATA,
	RIFF_WRITER_FOOTER,
	RIFF_WRITER_DONE
};

riff_wav_writer *riff_wav_writer_create();
int riff_wav_writer_set_format(riff_wav_writer *w,windows_WAVEFORMAT *f);
int riff_wav_writer_set_format_old(riff_wav_writer *w,windows_WAVEFORMATOLD *f);
int riff_wav_writer_set_format_ex(riff_wav_writer *w,windows_WAVEFORMATEX *f,size_t len);
int riff_wav_writer_assign_file(riff_wav_writer *w,int fd);
int riff_wav_writer_open_file(riff_wav_writer *w,const char *path);
void riff_wav_writer_fsync(riff_wav_writer *w);
int riff_wav_writer_begin_header(riff_wav_writer *w);
int riff_wav_writer_begin_data(riff_wav_writer *w);
int riff_wav_writer_end_data(riff_wav_writer *w);
riff_wav_writer *riff_wav_writer_destroy(riff_wav_writer *w);
int riff_wav_writer_data_write(riff_wav_writer *w,void *buffer,size_t len);
int64_t riff_wav_writer_data_seek(riff_wav_writer *w,int64_t offset);
int64_t riff_wav_writer_data_tell(riff_wav_writer *w);

#endif /* __ISP_UTILS_V4_AVI_RIFF_WAV_WRITER_H */

