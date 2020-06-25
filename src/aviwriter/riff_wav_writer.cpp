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

/* Shut up! */
#define _CRT_NONSTDC_NO_DEPRECATE

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#ifdef _MSC_VER
# include <io.h>
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

/* FIXME: I made the mistake of putting critical calls in assert() calls, which under MSVC++ may evaluate to nothing in Release builds */
#if defined(_MSC_VER) || defined (__MINGW32__)
# ifdef NDEBUG
#  undef assert
#  define assert(x) x
# endif
#endif

#include "riff_wav_writer.h"
#include "rawint.h"

riff_wav_writer *riff_wav_writer_create() {
	riff_wav_writer *w = (riff_wav_writer*)malloc(sizeof(riff_wav_writer));
	if (w == NULL) return NULL;
	memset(w,0,sizeof(*w));

	if ((w->riff = riff_stack_create(32)) == NULL)
		return riff_wav_writer_destroy(w);

	w->fd = -1;
	w->own_fd = 0;
	w->state = RIFF_WRITER_INIT;
	return w;
}

/* NOTE: for WAVEFORMATEX use set_format_ex */
int riff_wav_writer_set_format(riff_wav_writer *w,windows_WAVEFORMAT *f) {
	if (w == NULL || f == NULL)
		return 0;
	if (w->state != RIFF_WRITER_INIT)
		return 0;
	if (w->fmt != NULL)
		return 0;

	w->fmt_len = sizeof(*f);
	if ((w->fmt = malloc(w->fmt_len)) == NULL)
		return 0;

	memcpy(w->fmt,f,w->fmt_len);
	return 1;
}

int riff_wav_writer_set_format_old(riff_wav_writer *w,windows_WAVEFORMATOLD *f) {
	if (w == NULL || f == NULL)
		return 0;
	if (w->state != RIFF_WRITER_INIT)
		return 0;
	if (w->fmt != NULL)
		return 0;

	w->fmt_len = sizeof(*f);
	if ((w->fmt = malloc(w->fmt_len)) == NULL)
		return 0;

	memcpy(w->fmt,f,w->fmt_len);
	return 1;
}

int riff_wav_writer_set_format_ex(riff_wav_writer *w,windows_WAVEFORMATEX *f,size_t len) {
	if (w == NULL || f == NULL)
		return 0;
	if (w->state != RIFF_WRITER_INIT)
		return 0;
	if (w->fmt != NULL)
		return 0;

	w->fmt_len = sizeof(windows_WAVEFORMAT);
	if (__le_u16(&f->cbSize) != 0u) w->fmt_len += (size_t)2u + __le_u16(&f->cbSize);
	if (w->fmt_len > len)
		return 0;
	if ((w->fmt = malloc(w->fmt_len)) == NULL)
		return 0;

	memcpy(w->fmt,f,w->fmt_len);
	return 1;
}

int riff_wav_writer_assign_file(riff_wav_writer *w,int fd) {
	if (w->state != RIFF_WRITER_INIT)
		return 0;
	if (w->fd >= 0 && w->own_fd)
		return 0;

	w->fd = fd;
	w->own_fd = 0;
	if (!riff_stack_assign_fd(w->riff,fd))
		return 0;

	assert(riff_stack_empty(w->riff));
	assert(riff_stack_prepare_for_writing(w->riff,1));
	return 1;
}

int riff_wav_writer_open_file(riff_wav_writer *w,const char *path) {
	if (w->state != RIFF_WRITER_INIT)
		return 0;
	if (w->fd >= 0)
		return 0;
	if ((w->fd = open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644)) < 0)
		return 0;
	if (!riff_stack_assign_fd(w->riff,w->fd)) {
		close(w->fd);
		w->fd = -1;
	}
	w->own_fd = 1;
	assert(riff_stack_empty(w->riff));
	assert(riff_stack_prepare_for_writing(w->riff,1));
	return 1;
}

void riff_wav_writer_fsync(riff_wav_writer *w) {
	if (w->fd >= 0 && w->riff)
		riff_stack_header_sync_all(w->riff);
}

int riff_wav_writer_begin_header(riff_wav_writer *w) {
	riff_chunk chunk;

	if (w->state != RIFF_WRITER_INIT)
		return 0;
	if (w->fmt == NULL)
		return 0;

	/* RIFF:WAVE */
	assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
	assert(riff_stack_set_chunk_list_type(&chunk,riff_RIFF,riff_fourcc_const('W','A','V','E')));
	assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */

	/* 'fmt ' */
	assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
	assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('f','m','t',' ')));
	assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
	assert((int)riff_stack_write(w->riff,riff_stack_top(w->riff),w->fmt,w->fmt_len) == (int)w->fmt_len);
	riff_stack_pop(w->riff);

	/* state change */
	w->state = RIFF_WRITER_HEADER;
	return 1;
}

int riff_wav_writer_begin_data(riff_wav_writer *w) {
	riff_chunk chunk;

	if (w->state != RIFF_WRITER_HEADER)
		return 0;

	/* 'data' */
	assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
	assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('d','a','t','a')));
	assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
	riff_stack_header_sync_all(w->riff);

	/* state change */
	w->state = RIFF_WRITER_DATA;
	return 1;
}

riff_wav_writer *riff_wav_writer_destroy(riff_wav_writer *w) {
	if (w) {
		riff_wav_writer_fsync(w);
		if (w->fmt) free(w->fmt);
		if (w->fd >= 0 && w->own_fd) close(w->fd);
		if (w->riff) riff_stack_destroy(w->riff);
		free(w);
	}
	return NULL;
}

int riff_wav_writer_data_write(riff_wav_writer *w,void *buffer,size_t len) {
	riff_chunk *c;

	if (w == NULL)
		return 0;
	if (w->state != RIFF_WRITER_DATA)
		return 0;
	if ((c=riff_stack_top(w->riff)) == NULL)
		return 0;
	if (c->fourcc != riff_fourcc_const('d','a','t','a'))
		return 0;

	return (int)riff_stack_write(w->riff,riff_stack_top(w->riff),buffer,len);
}

int64_t riff_wav_writer_data_seek(riff_wav_writer *w,int64_t offset) {
	riff_chunk *c;

	if (w == NULL)
		return 0;
	if (w->state != RIFF_WRITER_DATA)
		return 0;
	if ((c=riff_stack_top(w->riff)) == NULL)
		return 0;
	if (c->fourcc != riff_fourcc_const('d','a','t','a'))
		return 0;

	return riff_stack_seek(w->riff,riff_stack_top(w->riff),offset);
}

int64_t riff_wav_writer_data_tell(riff_wav_writer *w) {
	riff_chunk *c;

	if (w == NULL)
		return 0;
	if (w->state != RIFF_WRITER_DATA)
		return 0;
	if ((c=riff_stack_top(w->riff)) == NULL)
		return 0;
	if (c->fourcc != riff_fourcc_const('d','a','t','a'))
		return 0;

	return c->write_offset;
}

int riff_wav_writer_end_data(riff_wav_writer *w) {
	riff_chunk *c;

	if (w == NULL)
		return 0;
	if (w->state != RIFF_WRITER_DATA)
		return 0;
	if ((c=riff_stack_top(w->riff)) == NULL)
		return 0;
	if (c->fourcc != riff_fourcc_const('d','a','t','a'))
		return 0;

	/* state change */
	riff_stack_pop(w->riff);
	w->state = RIFF_WRITER_FOOTER;
	return 1;
}

