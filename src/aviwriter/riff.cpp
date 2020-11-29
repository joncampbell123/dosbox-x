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

#include "rawint.h"
#include "riff.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _MSC_VER
# include <io.h>
#endif

int riff_std_read(void *a,void *b,size_t c) {
	riff_stack *rs;
	int rd;
	
	rs = (riff_stack*)a;
	if (rs->fd < 0) return -1;
	if (rs->trk_file_pointer < (int64_t)0) return -1;
	rd = (int)read(rs->fd,b,(unsigned int)c);

	if (rd < 0) rs->trk_file_pointer = -1LL;
	else rs->trk_file_pointer += rd;

	return rd;
}

int riff_std_write(void *a,const void *b,size_t c) {
	riff_stack *rs;
	int rd;

	rs = (riff_stack*)a;
	if (rs->fd < 0) return -1;
	if (rs->trk_file_pointer < (int64_t)0) return -1;
	rd = (int)write(rs->fd,b,(unsigned int)c);

	if (rd < 0) rs->trk_file_pointer = -1LL;
	else rs->trk_file_pointer += rd;

	return rd;
}

int64_t riff_std_seek(void *a,int64_t offset) {
	riff_stack *rs = (riff_stack*)a;
	if (rs->fd < 0) return -1;
	if (!rs->always_lseek && offset == rs->trk_file_pointer) return offset;
#if defined(_MSC_VER)
	rs->trk_file_pointer = _lseeki64(rs->fd, offset, SEEK_SET);
#else
	rs->trk_file_pointer = lseek(rs->fd, offset, SEEK_SET);
#endif
	return rs->trk_file_pointer;
}

int riff_buf_read(void *a,void *b,size_t c) {
	riff_stack *rs = (riff_stack*)a;
	if (rs->buffer == NULL) return -1;
	if (rs->bufofs >= rs->buflen) return 0;
	if ((rs->bufofs+c) > rs->buflen) c = (size_t)(rs->buflen - rs->bufofs);
	memcpy(b,(char*)rs->buffer+rs->bufofs,c);
	rs->bufofs += (size_t)c;
	return (int)c;
}

int riff_buf_write(void *a,const void *b,size_t c) {
	riff_stack *rs = (riff_stack*)a;
	if (rs->buffer == NULL) return -1;
	if (rs->bufofs >= rs->buflen) return 0;
	if ((rs->bufofs+c) > rs->buflen) c = (size_t)(rs->buflen - rs->bufofs);
	memcpy((char*)rs->buffer+rs->bufofs,b,c);
	rs->bufofs += (size_t)c;
	return (int)c;
}

int64_t riff_buf_seek(void *a,int64_t offset) {
	riff_stack *rs = (riff_stack*)a;
	if (rs->buffer == NULL) return -1;
	if (offset < 0LL) offset = 0LL;
	else if (offset > (int64_t)(rs->buflen)) offset = (int64_t)(rs->buflen);
	rs->bufofs = (size_t)offset;
	return (int64_t)rs->bufofs;
}

/* NTS: By design, this code focuses only on closing the file descriptor.
 *      If you forgot to write sync headers or pop all off the stack,
 *      then the incomplete and invalid RIFF structure is your problem. */
void riff_stack_close_source(riff_stack *s) {
	if (s->fd >= 0 && s->fd_owner) {
		close(s->fd);
		s->fd = -1;
	}
}

/* NTS: By default, does NOT take ownership of the file descriptor.
 *      If you want the RIFF stack to take ownership, and automatically
 *      free the descriptor on destruction/close, then you would call
 *      this function then call riff_stack_assign_fd_ownershop() */
int riff_stack_assign_fd(riff_stack *s,int fd) {
	if (fd != s->fd) {
		riff_stack_close_source(s);
		s->fd_owner = 0;
		s->fd = fd;
	}
	s->buffer = NULL;
	s->read = riff_std_read;
	s->seek = riff_std_seek;
	s->write = riff_std_write;
	s->trk_file_pointer = -1LL;
	return 1;
}

int riff_stack_assign_fd_ownership(riff_stack *s) {
	s->fd_owner = 1;
	return 1;
}

int riff_stack_assign_buffer(riff_stack *s,void *buffer,size_t len) {
	s->fd = -1;
	s->buflen = len;
	s->buffer = buffer;
	s->bufofs = (size_t)(0UL);
	s->read = riff_buf_read;
	s->seek = riff_buf_seek;
	s->write = riff_buf_write;
	s->trk_file_pointer = -1LL;
	return 1;
}

riff_stack *riff_stack_create(int depth) {
	riff_stack *s = (riff_stack*)malloc(sizeof(riff_stack));
	if (!s) return NULL;
	memset(s,0,sizeof(*s));

	if (depth == 0) depth = 32;
	else if (depth < 16) depth = 16;
	else if (depth > 512) depth = 512;

	s->fd = -1;
	s->current = -1;
	s->depth = depth;
	s->stack = (riff_chunk*)malloc(sizeof(riff_chunk)*(unsigned int)depth);
	if (!s->stack) {
		free(s);
		return NULL;
	}
	memset(s->stack,0,sizeof(riff_chunk)*(unsigned int)depth);
	return s;
}

riff_stack *riff_stack_destroy(riff_stack *s) {
	if (s) {
		riff_stack_close_source(s);
		if (s->stack) free(s->stack);
		free(s);
	}
	return NULL;
}

int riff_stack_is_empty(riff_stack *s) {
	return (s->current == -1);
}

int riff_stack_empty(riff_stack *s) {
	s->current = -1;
	s->next_read = 0;
	s->next_write = 0;
	s->top = NULL;
	s->eof = 0;
	return 1;
}

riff_chunk *riff_stack_top(riff_stack *s) {
	if (!s) return NULL;
	if (s->current == -1) return NULL;
	return (s->top = (s->stack + s->current));
}

riff_chunk *riff_stack_pop(riff_stack *s) {
	riff_chunk *pc,*c;

	if (!s) return NULL;
	if (s->current == -1) return NULL;

	c = s->top;
	s->current--;
	if (s->current == -1) {
		if (s->wmode) {
			if (!c->disable_sync) riff_stack_header_sync(s,c);
			if (c->data_length < (int64_t)c->write_offset)
				c->data_length = (uint32_t)c->write_offset;

			c->absolute_data_length = (c->data_length + 1) & (~1UL);
			s->next_write = c->absolute_data_offset + c->absolute_data_length;
		}
		s->top = NULL;
		return NULL;
	}
	s->top = (s->stack + s->current);

	/* if we're writing a RIFF structure, we need to make sure
	 * the parent chunk's data offsets are properly set up.
	 * the above conditions ensure we don't get here unless
	 * there was a parent chunk involved */
	if (s->wmode) {
		unsigned char no_sync = c->disable_sync;

		if (c->placeholder) {
			c->placeholder = 0; /* caller pops a chunk off the stack to complete it, therefore the placeholder must be rewritten */
			c->disable_sync = 0; /* it MUST be rewritten, disabling sync is no longer an option */
			no_sync = 0; /* <--- ditto */
		}

		if (!no_sync) riff_stack_header_sync(s,c);
		pc = s->top;
		while (pc && c) {
			if ((c->absolute_data_offset + c->absolute_data_length) >
				(pc->absolute_data_offset + pc->data_length)) {
				pc->data_length = (uint32_t)((c->absolute_data_offset + c->absolute_data_length) -
					pc->absolute_data_offset);
				pc->absolute_data_length = pc->data_length; /* FIXME: right? */
			}

			if (c->absolute_data_offset >= pc->absolute_data_offset) {
				int64_t limit = c->absolute_data_offset + c->absolute_data_length -
					pc->absolute_data_offset;

				if (pc->write_offset < limit)
					pc->write_offset = limit;
			}

			if (pc == s->stack)
				break;

			c = pc--; /* NTS: remember pc = s->top = s->stack + <index> */
		}
		if (!no_sync) riff_stack_header_sync(s,s->top);
	}

	return s->top;
}

int riff_stack_push(riff_stack *s,riff_chunk *c) {
	riff_chunk *to;
	if (s == NULL || c == NULL) return 0;
	if (s->current < -1) return 0;
	if ((s->current+1) >= s->depth) return 0;
	to = s->stack + (++s->current);
	memcpy(to,c,sizeof(riff_chunk));
	s->top = to;
	return 1;
}

int64_t riff_stack_current_chunk_offset(riff_stack *s) {
	int64_t o;
	if (s == NULL) return -1LL;
	if (s->top->read_offset > (s->top->data_length-8LL)) return -1LL;
	o = s->top->absolute_data_offset+s->top->read_offset;
	return o;
}

int64_t riff_stack_seek(riff_stack *s,riff_chunk *c,int64_t of) {
	if (c) {
		if (of > c->data_length) of = c->data_length;
		else if (of < 0) of = 0;
		c->read_offset = c->write_offset = of;
	}
	else {
		if (of < 0) of = 0;
		s->next_write = s->next_read = of;
	}

	return of;
}

int riff_stack_streamwrite(riff_stack *s,riff_chunk *c,const void *buf,size_t len) {
	if (s->write == NULL)
		return -1;

	if (c) {
		if (!c->wmode)
			return -1;

		/* once the data is written, you are not allowed to write any more */
		if (c->write_offset != 0)
			return -1;

		/* AVI chunks are limited to 2GB or less */
		if (((unsigned long long)c->write_offset+len) >= 0x80000000ULL)
			return -1;

		/* assume the write will complete and setup pointers now */
		c->read_offset = c->write_offset = (int64_t)len;
		c->data_length = (uint32_t)c->write_offset;
		c->absolute_data_length = (c->data_length + 1UL) & (~1UL);

		/* write the header NOW */
		riff_stack_header_sync(s,c);

		/* then write the data.
		 * the consequence of this design is that unlike riff_stack_write() the
		 * call is considered an absolute failure IF we were not able to write all
		 * the data to disk. We have to, the design of this code bets on it!
		 *
		 * NTS: We allow the caller to streamwrite NULL-length packets with buf=NULL and len=0 */
		if (buf != NULL) {
			int rd;

			if (s->seek(s,c->absolute_data_offset) != c->absolute_data_offset) return 0;
			rd = s->write(s,buf,len);

			/* if we were not able to write all data, well, too bad. update the header */
			if (rd < (int)len) {
				if (rd < 0) rd = 0;
				c->read_offset = c->write_offset = rd;
				c->data_length = (uint32_t)c->write_offset;
				c->absolute_data_length = (c->data_length + 1UL) & (~1UL);

				/* write the header NOW */
				riff_stack_header_sync(s,c);

				/* fail */
				return -1;
			}
		}

		/* we already wrote the header, and the caller is SUPPOSED to
		 * use this function ONCE for a RIFF chunk. Don't let riff_stack_pop()
		 * waste it's time lseek()'ing back to rewrite the header */
		c->disable_sync = 1;
	}
	else {
		abort(); /* TODO */
	}

	return (int)len;
}

int riff_stack_write(riff_stack *s,riff_chunk *c,const void *buf,size_t len) {
	int rd;

	if (s->write == NULL)
		return -1;

	if (c) {
		if (!c->wmode)
			return -1;
		if (c->absolute_data_offset == ((int64_t)(-1)))
			return -1;

		/* AVI chunks are limited to 2GB or less */
		if (((unsigned long long)c->write_offset+len) >= 0x80000000ULL)
			return -1;

		if (s->seek(s,c->absolute_data_offset+c->write_offset) !=
			(c->absolute_data_offset+c->write_offset)) return 0;
		rd = s->write(s,buf,len);
		if (rd > 0) c->read_offset = (c->write_offset += rd);
		if (c->data_length < (int64_t)c->write_offset)
			c->data_length = (uint32_t)c->write_offset;
		c->absolute_data_length = (c->data_length + 1UL) & (~1UL);
	}
	else {
		if (s->seek(s,s->next_write) != s->next_write) return 0;
		rd = s->write(s,buf,len);
		if (rd > 0) s->next_read = (s->next_write += rd);
	}

	return rd;
}

int riff_stack_read(riff_stack *s,riff_chunk *c,void *buf,size_t len) {
	int rd;

	if (c) {
		int64_t rem = c->data_length - c->read_offset;
		if (rem > (int64_t)len) rem = (int64_t)len;
		len = (size_t)rem;
		if (len == 0) return 0;
		if (c->absolute_data_offset == ((int64_t)(-1))) return 0;
		if (s->seek(s,c->absolute_data_offset+c->read_offset) != (c->absolute_data_offset+c->read_offset)) return 0;
		rd = s->read(s,buf,len);
		if (rd > 0) c->write_offset = (c->read_offset += rd);
	}
	else {
		if (s->seek(s,s->next_read) != s->next_read) return 0;
		rd = s->read(s,buf,len);
		if (rd > 0) s->next_write = (s->next_read += rd);
	}

	return rd;
}

/* given a chunk (pc), read a subchunk.
 * to read at the top (file) level, set pc == NULL.
 * 
 * if a subchunk is present, function returns 1 and the subchunk in (c).
 * else, (c) is not changed and function returns 0.
 *
 * The function needs the stack object to read, but does not automatically
 * push the chunk to the top of the stack. if you want to enter the
 * chunk to examine it, then you must push it to the stack yourself.
 *
 * this function DOES NOT check for you whether the chunk you are reading
 * should have subchunks. YOU are responsible for using
 * riff_stack_chunk_contains_subchunks() to determine that first. if you
 * use this function on a chunk that contains only data, you will get
 * garbage chunks and that's your fault.
 *
 * Anatomy of an AVI chunk (data only, always little endian):
 *
 * Offset       size / type     contents
 * -------------------------------------
 * +0x00 +0     4 / DWORD       FOURCC (4-byte ASCII identifying the chunk)
 * +0x04 +4     4 / DWORD       data length (not including header)
 * =0x08 +8     8
 *
 * Anatomy of an AVI chunk (list chunk containing subchunks):
 * 
 * Offset       size / type     contents
 * -------------------------------------
 * +0x00 +0     4 / DWORD       FOURCC ("RIFF" or "LIST")
 * +0x04 +4     4 / DWORD       data length (not including header but including 4-byte subchunk header that follows)
 * +0x08 +8     4 / DWORD       FOURCC (4-byte ASCII identifying the subchunk)
 * =0x0C +12    12
 * 
 * A word of caution about top-level chunks: Most RIFF formats are based on the top level
 * containing one major top-level chunk, and most (all) chunks are within that top level
 * chunk. If the file extends past 2GB, then the file at the top level is a series of
 * very large top-level RIFF chunks. It is very unlikely that you'll ever get a non-list
 * chunk at top level, so if this function returns one at toplevel, it's probably unrelated
 * junk at the end of the file.
 *
 * In most cases, your code should simply read the one chunk at the start of the file and
 * generally stay within the chunk, unless for example OpenDML AVI 2.0, in which you must
 * make sure you read the series of RIFF:AVI and RIFF:AVIX chunks until you reach the end
 * of the file or encounter any other chunk. Again, this code will NOT prevent you from
 * reading junk at the end of the file as an AVI chunk!
 */
int riff_stack_readchunk(riff_stack *s,riff_chunk *pc,riff_chunk *c) {
	unsigned char buf[8];

	if (s == NULL) return 0; /* caller must provide (s) stack */
	if (c == NULL) return 0; /* caller must provide (c) chunk structure to fill in */
	if (s->wmode) return 0;	/* no reading chunks while writing RIFF */

	if (pc != NULL) {
		/* if parent chunk provided, read new chunk from parent chunk */
		if (c->absolute_data_offset == ((int64_t)(-1))) return 0;
		if ((pc->read_offset+8) > pc->data_length) return 0;
		if (s->seek(s,pc->absolute_data_offset+pc->read_offset) != (pc->absolute_data_offset+pc->read_offset)) return 0;
		c->absolute_header_offset = pc->absolute_data_offset+pc->read_offset;
	}
	else {
		/* if parent chunk NOT provided, read from read pointer in file.
		 * return now if read pointer has signalled eof.
		 * signal eof and return if seek fails. */
		if (s->eof) return 0;
		if (s->seek(s,s->next_read) != s->next_read) {
			s->eof = 1;
			return 0;
		}
		c->absolute_header_offset = s->next_read;
	}

	/* read 8 bytes corresponding to the FOURCC and data length.
	 * failure to read is the end of the chunk.
	 * signal eof if the failure is at the top level. */
	if (s->read(s,buf,8) < 8) {
		if (pc == NULL) s->eof = 1;
		return 0;
	}

	c->wmode = 0;
	c->read_offset = 0;
	c->write_offset = 0;
	c->absolute_data_offset = c->absolute_header_offset + 8LL;
	c->list_fourcc = (riff_fourcc_t)(0UL);
	c->fourcc = __le_u32(buf+0);
	c->data_length = __le_u32(buf+4); /* <-- NOTE this is why AVI files have a 2GB limit */
	c->absolute_data_length = (c->data_length + 1ULL) & (~1ULL); /* <-- RIFF chunks are WORD aligned */
	c->absolute_offset_next_chunk = c->absolute_data_offset + c->absolute_data_length;

	/* consider it the end of the chunks if we read in a NULL fourcc, and mark EOF */
	if (c->fourcc == 0UL) {
		if (pc) pc->read_offset = pc->write_offset = pc->data_length;
		else s->eof = 1;
		return 0;
	}

	/* some chunks like 'RIFF' or 'LIST' are actually 12 byte headers, with the real FOURCC
	 * following the length, then subchunks follow in the data area. */
	if (c->fourcc == riff_RIFF || c->fourcc == riff_LIST) {
		c->list_fourcc = c->fourcc;
		if (c->data_length >= 4) {
			if (s->read(s,buf,4) < 4) return 0;
			c->fourcc = __le_u32(buf+0);
			c->absolute_data_offset += 4;
			c->absolute_data_length -= 4;
			c->data_length -= 4;
		}
		else {
			memset(buf,0,4);
			if ((int)s->read(s,buf,c->absolute_data_length) < (int)c->absolute_data_length) return 0;
			c->fourcc = __le_u32(buf+0);
			c->absolute_data_offset += c->absolute_data_length;
			c->absolute_data_length = 0;
			c->data_length = 0;
		}
	}

	if (pc)	pc->read_offset = pc->write_offset = c->absolute_offset_next_chunk - pc->absolute_data_offset;
	else	s->next_read = s->next_write = c->absolute_offset_next_chunk;
	return 1;
}

int riff_stack_chunk_contains_subchunks(riff_chunk *c) {
	if (c == NULL) return 0;
	if (c->list_fourcc == riff_RIFF) return 1;
	if (c->list_fourcc == riff_LIST) return 1;
	return 0;
}

void riff_stack_fourcc_to_str(riff_fourcc_t t,char *tmp) {
	tmp[0] = (char)((t >>  0) & 0xFF);
	tmp[1] = (char)((t >>  8) & 0xFF);
	tmp[2] = (char)((t >> 16) & 0xFF);
	tmp[3] = (char)((t >> 24) & 0xFF);
	tmp[4] = (char)0;
}

void riff_stack_debug_print_indent(FILE *fp,int level) {
	while (level-- > 0) fprintf(fp,"  ");
}

void riff_stack_debug_print(FILE *fp,int level,riff_chunk *chunk) {
	char tmp[8];

	riff_stack_debug_print_indent(fp,level);
	fprintf(fp,"[%d] ",level);
	if (riff_stack_chunk_contains_subchunks(chunk)) {
		riff_stack_fourcc_to_str(chunk->list_fourcc,tmp);
		fprintf(fp,"'%s:",tmp);
		riff_stack_fourcc_to_str(chunk->fourcc,tmp);
		fprintf(fp,"%s' ",tmp);
	}
	else {
		riff_stack_fourcc_to_str(chunk->fourcc,tmp);
		fprintf(fp,"'%s' ",tmp);
	}
	fprintf(fp,"hdr=%llu data=%llu len=%lu data-end=%llu",
		(unsigned long long)(chunk->absolute_header_offset),
		(unsigned long long)(chunk->absolute_data_offset),
		(unsigned long)(chunk->data_length),
		(unsigned long long)(chunk->absolute_data_offset+chunk->data_length));
	fprintf(fp,"\n");
}

void riff_stack_debug_chunk_dump(FILE *fp,riff_stack *riff,riff_chunk *chunk) {
	unsigned char tmp[64];
	int rd,i,col=0,o;

	if (riff_stack_seek(riff,chunk,0LL) != 0LL)
		return;

	rd = (int)riff_stack_read(riff,chunk,tmp,sizeof(tmp));
	for (i=0;i < ((rd+15)&(~15));) {
		if (col == 0) fprintf(fp,"        ");

		col++;
		if (i < rd)	fprintf(fp,"%02X ",tmp[i]);
		else		fprintf(fp,"   ");
		i++;

		if (col >= 16 || i == ((rd+15)&(~15))) {
			for (col=0;col < 16;col++) {
				o = i+col-16;
				if (o >= rd)
					fwrite(" ",1,1,fp);
				else if (tmp[o] >= 32 && tmp[o] < 127)
					fwrite(tmp+o,1,1,fp);
				else
					fwrite(".",1,1,fp);
			}

			col = 0;
			fprintf(fp,"\n");
		}
	}
}

int riff_stack_eof(riff_stack *r) {
	if (r == NULL) return 0;
	if (r->current == -1) return r->eof;
	return 0;
}

void riff_chunk_improvise(riff_chunk *c,uint64_t ofs,uint32_t size) {
	c->absolute_header_offset = (int64_t)ofs;
	c->absolute_data_offset = (int64_t)ofs;
	c->absolute_offset_next_chunk = (int64_t)(ofs + size);
	c->fourcc = 0U;
	c->data_length = size;
	c->absolute_data_length = size;
	c->list_fourcc = 0U;
	c->read_offset = 0ULL;
	c->write_offset = 0ULL;
	c->wmode = 0;
}

int riff_stack_prepare_for_writing(riff_stack *r,int wmode) {
	if (r == NULL) return 0;
	if (r->wmode == wmode) return 1;

	/* no state changes while anything is stacked on */
	if (r->current >= 0) return 0;

	/* no state changes while reading an AVI chunk */
	if (r->next_read != 0LL && !r->eof) return 0;

	r->wmode = wmode > 0 ? 1 : 0;
	return 1;
}

int riff_stack_begin_new_chunk_here(riff_stack *s,riff_chunk *c) {
	riff_chunk *p = riff_stack_top(s);

	/* you can't begin new chunks if you're not writing the file */
	if (!s->wmode)
		return 0;

	/* sanity check.
	 * if we're currently in a subchunk, make sure the subchunk allows for subchunks within.
	 * you can't just put chunks inside data. */
	if (p != NULL) {
		if (!riff_stack_chunk_contains_subchunks(p)) {
			fprintf(stderr,"BUG: riff_stack_begin_new_chunk_here() caller attempting to start new RIFF chunks inside a chunk that does not contain subchunks\n");
			return 0;
		}
	}

	/* zero the chunk and pick the header offset.
	 * for debugging purposes, we do not assign the data offset until the
	 * host program calls set_chunk_list_type/set_chunk_data_type.
	 * the chunk itself must also remember that it's in write mode. */
	memset(c,0,sizeof(*c));
	if (p != NULL)
		c->absolute_header_offset = p->absolute_data_offset + p->write_offset;
	else
		c->absolute_header_offset = s->next_write;

	c->absolute_data_offset = (int64_t)(-1LL);
	c->wmode = 1;
	return 1;
}

int riff_stack_header_sync(riff_stack *s,riff_chunk *c) {
	unsigned char tmp[12];

	if (!c->wmode || !s->wmode)
		return 0;

	if (s->seek(s,c->absolute_header_offset) != c->absolute_header_offset)
		return 0;

	/* NTS: A placeholder value of 0xFFFFFFFF would technically violate the AVI standard
	 *      because it is well known older programs treat the length as signed. We use
	 *      0x7FFFFFFF instead. */
	if (c->list_fourcc != (riff_fourcc_t)0) {
		__w_le_u32(tmp+0,c->list_fourcc);
		if (c->placeholder)
			__w_le_u32(tmp+4,0x7FFFFFFFUL);
		else
			__w_le_u32(tmp+4,c->data_length + 4UL); /* for some reason the length covers the fourcc */
		__w_le_u32(tmp+8,c->fourcc);
		if (s->write(s,tmp,12) < 12)
			return 0;
	}
	else {
		__w_le_u32(tmp+0,c->fourcc);
		if (c->placeholder)
			__w_le_u32(tmp+4,0x7FFFFFFFUL);
		else
			__w_le_u32(tmp+4,c->data_length);
		if (s->write(s,tmp,8) < 8)
			return 0;
	}

	return 1;
}

int riff_stack_header_sync_all(riff_stack *s) {
	int i = s->current;

	while (i >= 0) {
		riff_chunk *c = s->stack + (i--);
		if (!riff_stack_header_sync(s,c))
			return 0;
	}

	return 1;
}

/* start a data chunk (no subchunks) with FOURCC (fcc) */
int riff_stack_set_chunk_data_type(riff_chunk *c,riff_fourcc_t fcc) {
	if (!c->wmode)
		return 0;
	if (c->write_offset != 0LL) {
		fprintf(stderr,"BUG: riff_stack_set_chunk_data_type() caller attempted to set type after writing data!\n");
		return 0;
	}

	c->fourcc = fcc;
	c->list_fourcc = (riff_fourcc_t)0;
	c->absolute_data_offset = c->absolute_header_offset + 8LL; /* <fourcc> <len> */
	return 1;
}

/* start a list chunk (with subchunks) with list type (list) usually "RIFF" and "LIST" and FOURCC (fcc).
 * For example: RIFF:AVI would be list = "RIFF" fcc = "AVI " */
int riff_stack_set_chunk_list_type(riff_chunk *c,riff_fourcc_t list,riff_fourcc_t fcc) {
	if (!c->wmode)
		return 0;
	if (c->write_offset != 0LL) {
		fprintf(stderr,"BUG: riff_stack_set_chunk_list_type() caller attempted to set type after writing data!\n");
		return 0;
	}

	c->fourcc = fcc;
	c->list_fourcc = list;
	c->absolute_data_offset = c->absolute_header_offset + 12LL; /* RIFF <len> <fourcc> */
	return 1;
}

void riff_stack_writing_sync(riff_stack *s) {
	int64_t noffset = 0;

	while (s->current >= 0) {
		/* the caller uses this function when all chunks are to be completed,
		 * and the writing process may or may not be done. the AVI writer
		 * may uses this for instance when writing the next 'AVIX' chunk
		 * in an OpenDML file which requires all chunks to be completed.
		 *
		 * as part of the process we must clear all disable_sync and
		 * placeholder markings so the true state can be written back */
		riff_chunk *t = riff_stack_top(s);
		t->disable_sync = 0;
		t->placeholder = 0;
		riff_stack_header_sync_all(s);
		assert(s->top->absolute_data_offset >= (int64_t)0);
		int64_t x = s->top->absolute_data_offset + s->top->absolute_data_length;
		if (noffset < x) noffset = x;
		riff_stack_pop(s);
	}

	s->next_write = noffset;
}

/* if I wrote "len" bytes, would I hit or cross the AVI 2GB chunk limit at any level? */
/* if so, the caller must pop down all AVI chunks, emptying the stack, and then create
 * new chunks to continue on */
int riff_stack_chunk_limit(riff_stack *s,int len) {
	riff_chunk *lowest,*highest;
	unsigned long long base;

	if (s->current < 0) return 0;

	lowest = s->stack;
	assert(lowest->absolute_data_offset >= (int64_t)0);
	highest = riff_stack_top(s);
	assert(highest != NULL);
	assert(highest->absolute_data_offset >= (int64_t)0);
	base = (unsigned long long)lowest->absolute_data_offset;

	for (;highest >= lowest;highest--) {
		signed long long rel = (signed long long)((unsigned long long)highest->absolute_data_offset - base);
		/* subchunks should not be all over the place---sanity check! */
		assert(rel >= 0);

		if ((rel + highest->write_offset + len) >= 0x40000000LL) /* 1GB mark */
			return 1;
	}

	return 0;
}

int riff_stack_enable_placeholder(riff_stack *s,riff_chunk *c) {
	if (s == NULL) return 0;
	if (c == NULL) return 0;
	c->placeholder = 1;
	return 1;
}

