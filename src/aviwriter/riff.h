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

#ifndef __VIDEOMGRUTIL_RIFF_H
#define __VIDEOMGRUTIL_RIFF_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "informational.h"

typedef uint32_t riff_fourcc_t;
#define riff_fourcc_const(a,b,c,d)	( (((uint32_t)(a)) << 0U) | (((uint32_t)(b)) << 8U) | (((uint32_t)(c)) << 16U) | (((uint32_t)(d)) << 24U) )

#define riff_LIST		riff_fourcc_const('L','I','S','T')
#define riff_RIFF		riff_fourcc_const('R','I','F','F')

#define riff_wav_fmt		riff_fourcc_const('f','m','t',' ')
#define riff_wav_data		riff_fourcc_const('d','a','t','a')

typedef struct {
	int64_t			absolute_header_offset;
	int64_t			absolute_data_offset;
	int64_t			absolute_offset_next_chunk;
	riff_fourcc_t		fourcc;
	uint32_t		data_length;
	uint32_t		absolute_data_length;
	riff_fourcc_t		list_fourcc;
	int64_t			read_offset;
	int64_t			write_offset;
	int			wmode;
	unsigned char		disable_sync;		/* 1=riff_stack_pop() will NOT rewrite the header.
							   caller would use this if using riff_stack_streamwrite()
							   to blast the data to disk as fast as possible */
	unsigned char		placeholder;		/* 1=write 0xFFFFFFFF as a placeholder for the actual size
							   until the header is actually updated. you should really
							   only set this flag for LIST chunks that are unlikely to
							   be zero bytes in length. flag is cleared automatically
							   by riff_stack_pop(). do NOT set if disable_sync is set. */
} riff_chunk;

static const riff_chunk RIFF_CHUNK_INIT = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

typedef struct {
	int			current,depth;
	riff_chunk		*stack,*top;

	int			fd;
	int			eof;
	void*			user;
	int			wmode;
	int64_t			next_read;
	int64_t			next_write;
	void*			buffer;
	size_t			buflen;
	size_t			bufofs;
	int			(*read)(void *a,void *b,size_t c);
	int64_t			(*seek)(void *a,int64_t offset);
	int			(*write)(void *a,const void *b,size_t c);

	/* flags for controlling some of the optimizations used in this code */
	unsigned char		always_lseek;		/* 1=always lseek()
							   0=ignore lseek() if file pointer should be at that point anyway
							   EXPLANATION: Linux disk I/O caching is more likely to provide better
							                throughput if we avoid lseek() and write structures and
								        data using only a continuous series of read() or write()
									calls. */

	/* variables provided for the file I/O code */
	int64_t			trk_file_pointer;

	/* more flags */
	unsigned int		fd_owner:1;		/* if set, we take ownership of the file descriptor */
} riff_stack;

int riff_std_read(void *a,void *b,size_t c);
int riff_std_write(void *a,const void *b,size_t c);
int64_t riff_std_seek(void *a,int64_t offset);

riff_stack *riff_stack_create(int depth);
riff_stack *riff_stack_destroy(riff_stack *s);
int riff_stack_is_empty(riff_stack *s);
int riff_stack_empty(riff_stack *s);
riff_chunk *riff_stack_top(riff_stack *s);
riff_chunk *riff_stack_pop(riff_stack *s);
int riff_stack_push(riff_stack *s,riff_chunk *c);
int64_t riff_stack_seek(riff_stack *s,riff_chunk *c,int64_t of);
int riff_stack_read(riff_stack *s,riff_chunk *c,void *buf,size_t len);
int riff_stack_write(riff_stack *s,riff_chunk *c,const void *buf,size_t len);
int riff_stack_streamwrite(riff_stack *s,riff_chunk *c,const void *buf,size_t len);
int64_t riff_stack_current_chunk_offset(riff_stack *s);
int riff_stack_assign_fd(riff_stack *s,int fd);
int riff_stack_assign_fd_ownership(riff_stack *s);
int riff_stack_assign_buffer(riff_stack *s,void *buffer,size_t len);
int riff_stack_chunk_contains_subchunks(riff_chunk *c);
int riff_stack_readchunk(riff_stack *s,riff_chunk *pc,riff_chunk *c);
int riff_stack_set_chunk_data_type(riff_chunk *c,riff_fourcc_t fcc);
int riff_stack_set_chunk_list_type(riff_chunk *c,riff_fourcc_t list,riff_fourcc_t fcc);
void riff_stack_debug_chunk_dump(FILE *fp,riff_stack *riff,riff_chunk *chunk);
void riff_stack_debug_print(FILE *fp,int level,riff_chunk *chunk);
void riff_chunk_improvise(riff_chunk *c,uint64_t ofs,uint32_t size);
void riff_stack_fourcc_to_str(riff_fourcc_t t,char *tmp);
void riff_stack_debug_print_indent(FILE *fp,int level);
int riff_stack_eof(riff_stack *r);
int riff_stack_prepare_for_writing(riff_stack *r,int wmode);
int riff_stack_begin_new_chunk_here(riff_stack *s,riff_chunk *c);
int riff_stack_header_sync(riff_stack *s,riff_chunk *c);
int riff_stack_header_sync_all(riff_stack *s);
int riff_stack_chunk_limit(riff_stack *s,int len);
int riff_stack_enable_placeholder(riff_stack *s,riff_chunk *c);
void riff_stack_writing_sync(riff_stack *s);

#define RIFF_CHUNK_HEADER_LENGTH		8
#define RIFF_LIST_CHUNK_HEADER_LENGTH		12

#endif

