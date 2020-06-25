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
#include "avi.h"
#include "avi_writer.h"
#include "avi_rw_iobuf.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
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

int avi_writer_stream_check_samplecount(avi_writer_stream *s,unsigned int len) {
    if (s == NULL) return 0;

    if (s->sample_index == NULL) {
        s->sample_index_alloc = len + 256;
        s->sample_index = (avi_writer_stream_index*)
            malloc(sizeof(avi_writer_stream_index) * s->sample_index_alloc);
        if (s->sample_index == NULL) {
            s->sample_index_alloc = 0;
            s->sample_index_max = 0;
            return 0;
        }
        s->sample_index_max = 0;
    }
    else if (len > s->sample_index_alloc) {
        unsigned int na = len + 8192;
        avi_writer_stream_index *n = (avi_writer_stream_index*)
            realloc((void*)(s->sample_index),
                sizeof(avi_writer_stream_index) * na);
        if (n == NULL) return 0;
        s->sample_index = n;
        s->sample_index_alloc = na;
    }

    return 1;
}

int avi_writer_stream_set_format(avi_writer_stream *s,void *data,size_t len) {
    if (s == NULL) return 0;
    if (s->format) free(s->format);
    s->format_len = 0;
    s->format = NULL;

    if (len == 0)
        return 1;

    if ((s->format = malloc(len)) == NULL)
        return 0;

    s->format_len = len;
    if (data) memcpy(s->format,data,len);
    else      memset(s->format,0,len);
    return 1;
}

riff_strh_AVISTREAMHEADER *avi_writer_stream_header(avi_writer_stream *s) {
    if (s == NULL) return NULL;
    return &s->header;
}

riff_avih_AVIMAINHEADER *avi_writer_main_header(avi_writer *w) {
    if (w == NULL) return NULL;
    return &w->main_header;
}

avi_writer_stream *avi_writer_new_stream(avi_writer *w) { /* reminder: you are not required to free this pointer, the writer does it for you on close_file() */
    avi_writer_stream *s;

    if (w == NULL) return NULL;
    if (w->state != AVI_WRITER_STATE_INIT) return NULL;

    if (w->avi_stream == NULL) {
        w->avi_stream_max = 1;
        w->avi_stream_alloc = 8;
        w->avi_stream = (avi_writer_stream*)
            malloc(sizeof(avi_writer_stream) * (unsigned int)w->avi_stream_alloc);
        if (w->avi_stream == NULL) {
            w->avi_stream_max = 0;
            w->avi_stream_alloc = 0;
            return NULL;
        }
        s = w->avi_stream;
    }
    else if (w->avi_stream_max >= w->avi_stream_alloc) {
        unsigned int na = (unsigned int)w->avi_stream_alloc + 64U;
        avi_writer_stream *n = (avi_writer_stream*)
            realloc((void*)(w->avi_stream),sizeof(avi_writer_stream) * (unsigned int)na);
        if (n == NULL)
            return NULL;
        w->avi_stream = n;
        w->avi_stream_alloc = (int)na;
        s = w->avi_stream + (w->avi_stream_max++);
    }
    else {
        s = w->avi_stream + (w->avi_stream_max++);
    }

    memset(s,0,sizeof(*s));
    s->index = (int)(s - w->avi_stream); /* NTS: C compiler converts pointer math to # of elements since w->avi_stream */
    return s;
}

void avi_writer_free_stream(avi_writer_stream *s) {
    if (s->sample_index) free(s->sample_index);
    s->sample_index_max = 0;
    s->sample_index_alloc = 0;
    if (s->format) free(s->format);
    s->format_len = 0;
    s->format = NULL;
}

void avi_writer_free_streams(avi_writer *w) {
    if (w->avi_stream) {
        for (int i=0;i < w->avi_stream_max;i++)
            avi_writer_free_stream(&w->avi_stream[i]);
        free(w->avi_stream);
    }

    w->avi_stream = NULL;
    w->avi_stream_max = 0;
    w->avi_stream_alloc = 0;
}

avi_writer *avi_writer_create() {
    avi_writer *w = (avi_writer*)malloc(sizeof(avi_writer));
    if (w == NULL) return NULL;
    memset(w,0,sizeof(*w));
    w->enable_opendml_index = 1;
    w->enable_stream_writing = 0;
    w->enable_avioldindex = 1;
    w->enable_opendml = 1;
    w->fd = -1;
    return w;
}

void avi_writer_close_file(avi_writer *w) {
    avi_writer_free_streams(w);
    if (w->riff) {
        riff_stack_writing_sync(w->riff);
        w->riff = riff_stack_destroy(w->riff);
    }
    if (w->fd >= 0) {
        if (w->own_fd) close(w->fd);
        w->fd = -1;
    }
    w->state = AVI_WRITER_STATE_DONE;
}

int avi_writer_open_file(avi_writer *w,const char *path) {
    avi_writer_close_file(w);

    w->own_fd = 1;
    if ((w->fd = open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644)) < 0)
        return 0;

    if ((w->riff = riff_stack_create(256)) == NULL)
        goto errout;

    assert(riff_stack_assign_fd(w->riff,w->fd));
    assert(riff_stack_empty(w->riff));
    assert(riff_stack_prepare_for_writing(w->riff,1));

    w->state = AVI_WRITER_STATE_INIT;
    return 1;
errout:
    avi_writer_close_file(w);
    return 0;
}

avi_writer *avi_writer_destroy(avi_writer *w) {
    if (w) {
        avi_writer_close_file(w);
        free(w);
    }

    return NULL;
}

int avi_writer_begin_data(avi_writer *w) {
    riff_chunk chunk;

    if (w == NULL) return 0;
    if (w->state != AVI_WRITER_STATE_HEADER) return 0;

    /* in case additional headers inserted are off-track, pop them.
     * AND pop out of the LIST:hdrl chunk too */
    while (w->riff->current > 0)
        riff_stack_pop(w->riff);

    /* start the movi chunk */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_list_type(&chunk,riff_LIST,riff_fourcc_const('m','o','v','i')));
    if (w->enable_stream_writing) {
        assert(riff_stack_enable_placeholder(w->riff,&chunk));
        chunk.disable_sync = 1;
    }
    assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
    w->movi = chunk;

    w->state = AVI_WRITER_STATE_BODY;
    riff_stack_header_sync_all(w->riff);
    return 1;
}

int avi_writer_begin_header(avi_writer *w) {
    riff_chunk chunk;
    int stream;

    if (w == NULL) return 0;
    if (w->state != AVI_WRITER_STATE_INIT) return 0;

    /* update the main header */
    __w_le_u32(&w->main_header.dwStreams,(unsigned int)w->avi_stream_max);

    /* [1] RIFF:AVI */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_list_type(&chunk,riff_RIFF,riff_fourcc_const('A','V','I',' ')));
    if (w->enable_stream_writing) {
        /* if stream writing we encourage (up until OpenDML AVIX chunk is written)
         * the RIFF library to keep the size field set to the 0x7FFFFFFF placeholder
         * so that if this AVI file is left incomplete it still remains (mostly)
         * playable. Most media players including VLC player will not read an AVI
         * file past the length given in the LIST:AVI chunk and forcing the placeholder
         * ensure that VLC player will gracefully handle incomplete files written
         * by this library. */
        assert(riff_stack_enable_placeholder(w->riff,&chunk));
        chunk.disable_sync = 1;
    }
    assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */

    /* [2] LIST:hdrl */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_list_type(&chunk,riff_LIST,riff_fourcc_const('h','d','r','l')));
    assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */

    /* [3] avih */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('a','v','i','h')));
    assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
    assert(riff_stack_write(w->riff,riff_stack_top(w->riff),&w->main_header,sizeof(w->main_header)) ==
        (int)sizeof(w->main_header));
    w->avih = *riff_stack_top(w->riff);
    riff_stack_pop(w->riff); /* end avih */

    /* write out the streams */
    for (stream=0;stream < w->avi_stream_max;stream++) {
        avi_writer_stream *s = w->avi_stream + stream;

        /* Auto-generate chunk fourcc */
        if (s->chunk_fourcc == 0) {
            if (s->header.fccType == avi_fccType_video || s->header.fccType == avi_fccType_iavs) {
                if (s->format != NULL && s->format_len >= sizeof(windows_BITMAPINFOHEADER)) {
                    windows_BITMAPINFOHEADER *h = (windows_BITMAPINFOHEADER*)(s->format);
                    if (h->biCompression == 0)
                        s->chunk_fourcc = riff_fourcc_const(0,0,'d','b');
                    else
                        s->chunk_fourcc = riff_fourcc_const(0,0,'d','c');
                }
            }
            else if (s->header.fccType == avi_fccType_audio) {
                s->chunk_fourcc = riff_fourcc_const(0,0,'w','b');
            }
            else {
                /* TODO */
            }

            s->chunk_fourcc |= ((((unsigned int)s->index / 10U) % 10U) + (unsigned char)('0')) << 0UL;
            s->chunk_fourcc |= ( ((unsigned int)s->index % 10U)        + (unsigned char)('0')) << 8UL;
        }

        /* [3] LIST:strl */
        assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
        assert(riff_stack_set_chunk_list_type(&chunk,riff_LIST,riff_fourcc_const('s','t','r','l')));
        assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */

        /* [4] strh */
        assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
        assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('s','t','r','h')));
        assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
        assert(riff_stack_write(w->riff,riff_stack_top(w->riff),&s->header,sizeof(s->header)) ==
            (int)sizeof(s->header));
        s->strh = *riff_stack_top(w->riff);
        riff_stack_pop(w->riff);

        /* [4] strf */
        assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
        assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('s','t','r','f')));
        assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
        if (s->format && s->format_len > 0)
            assert((int)riff_stack_write(w->riff,riff_stack_top(w->riff),s->format,(size_t)s->format_len) == (int)s->format_len);
        riff_stack_pop(w->riff);

        /* [5] strn (if name given) */
        if (s->name != NULL) {
            size_t len = strlen(s->name) + 1; /* must include NUL */

            assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
            assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('s','t','r','n')));
            assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
            assert((int)riff_stack_write(w->riff,riff_stack_top(w->riff),s->name,(size_t)len) == (int)len);
            riff_stack_pop(w->riff);
        }

        if (w->enable_opendml_index) {
            unsigned char tmp[512];
            int i;

            /* JUNK indx */
            assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
            assert(riff_stack_set_chunk_data_type(&chunk,riff_fourcc_const('J','U','N','K')));
            assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
            memset(tmp,0,sizeof(tmp));
            for (i=0;i < (16384/512);i++) assert(riff_stack_write(w->riff,riff_stack_top(w->riff),tmp,512) == 512);
            s->indx_junk = *riff_stack_top(w->riff);
            riff_stack_pop(w->riff);
        }

        /* end strl */
        riff_stack_pop(w->riff);
    }

    riff_stack_header_sync_all(w->riff);
    w->state = AVI_WRITER_STATE_HEADER;
    return 1;
}

int avi_writer_stream_repeat_last_chunk(avi_writer *w,avi_writer_stream *s) {
    avi_writer_stream_index *si,*psi;
    riff_chunk chunk;

    if (w == NULL || s == NULL)
        return 0;
    if (w->state != AVI_WRITER_STATE_BODY)
        return 0;
    if (s->sample_write_chunk == 0) /* if there *IS* no previous chunk, then bail */
        return 0;

    /* make sure we're down into the 'movi' chunk */
    while (w->riff->current > 1)
        riff_stack_pop(w->riff);

    /* make sure this is the movi chunk */
    if (w->riff->current != 1)
        return 0;
    if (w->riff->top->fourcc != avi_riff_movi)
        return 0;

    if (w->enable_opendml) {
        /* if we're writing an OpenDML 2.0 compliant file, and we're approaching a movi size of 1GB,
         * then split the movi chunk and start another RIFF:AVIX */
        if ((unsigned long long)(w->riff->top->write_offset + 8) >= 0x3FF00000ULL) { /* 1GB - 16MB */
            riff_stack_writing_sync(w->riff); /* sync all headers and pop all chunks */
            assert(w->riff->current == -1); /* should be at top level */

            /* at the first 1GB boundary emit AVIOLDINDEX for older AVI applications */
            if (w->group == 0 && w->enable_avioldindex)
                avi_writer_emit_avioldindex(w);

            /* [1] RIFF:AVIX */
            assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
            assert(riff_stack_set_chunk_list_type(&chunk,riff_RIFF,riff_fourcc_const('A','V','I','X')));
            if (w->enable_stream_writing) {
                assert(riff_stack_enable_placeholder(w->riff,&chunk));
                chunk.disable_sync = 1;
            }
            assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
            if (w->enable_stream_writing) riff_stack_header_sync(w->riff,riff_stack_top(w->riff));

            /* start the movi chunk */
            assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
            assert(riff_stack_set_chunk_list_type(&chunk,riff_LIST,riff_fourcc_const('m','o','v','i')));
            if (w->enable_stream_writing) {
                assert(riff_stack_enable_placeholder(w->riff,&chunk));
                chunk.disable_sync = 1;
            }
            assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
            if (w->enable_stream_writing) riff_stack_header_sync(w->riff,riff_stack_top(w->riff));
            w->movi = chunk;

            w->group++;
        }
    }
    else {
        /* else, if we're about to pass 2GB, then stop allowing any more data, because the traditional
         * AVI format uses 32-bit integers and most implementations treat them as signed. */
        if ((w->movi.absolute_data_offset + w->riff->top->write_offset + 8) >= 0x7FF00000LL) /* 2GB - 16MB */
            return 0;
    }

    /* write chunk into movi (for consistent timekeeping with older AVI apps that don't read the index) */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_data_type(&chunk,s->chunk_fourcc));
    assert(riff_stack_push(w->riff,&chunk));
    riff_stack_pop(w->riff);

    /* put the data into the index */
    if (!avi_writer_stream_check_samplecount(s,s->sample_write_chunk+16))
        return 0;

    /* lookup the previous chunk */
    /* NTS: this must come after the check_samplecount() because check_samplecount()
     *      uses realloc() to extend the array and realloc() may move the data around
     *      to fullfill the request */
    assert(s->sample_index != NULL);
    assert(s->sample_index_max >= s->sample_write_chunk);
    psi = s->sample_index + s->sample_write_chunk - 1;

    s->sample_index_max = s->sample_write_chunk+1;
    assert(s->sample_index_max < s->sample_index_alloc);
    si = s->sample_index + s->sample_write_chunk;

    *si = *psi;
    si->stream_offset = s->sample_write_offset;

    s->sample_write_offset += si->length;
    s->sample_write_chunk++;
    riff_stack_header_sync_all(w->riff);
    return 1;
}

/* NTS: this code makes no attempt to optimize chunk sizes and combine samples/frames---if you're
 *      stupid enough to call this routine with 4-byte long data then 4-byte long chunks is what
 *      you'll get, don't blame me if doing that overruns the allocated space set aside for the
 *      indexes. */
int avi_writer_stream_write(avi_writer *w,avi_writer_stream *s,void *data,size_t len,uint32_t flags) {
    avi_writer_stream_index *si;
    riff_chunk chunk;

    if (w == NULL || s == NULL)
        return 0;
    if (w->state != AVI_WRITER_STATE_BODY)
        return 0;

    /* calling this function with data == NULL is perfectly valid, it simply means no data */
    if (data == NULL)
        len = 0;

    /* make sure we're down into the 'movi' chunk */
    while (w->riff->current > 1)
        riff_stack_pop(w->riff);

    /* make sure this is the movi chunk */
    if (w->riff->current != 1)
        return 0;
    if (w->riff->top->fourcc != avi_riff_movi)
        return 0;

    if (w->enable_opendml) {
        /* if we're writing an OpenDML 2.0 compliant file, and we're approaching a movi size of 1GB,
         * then split the movi chunk and start another RIFF:AVIX */
        if (((unsigned long long)w->riff->top->write_offset + (unsigned long long)len) >= 0x3FF00000ULL) { /* 1GB - 16MB */
            riff_stack_writing_sync(w->riff); /* sync all headers and pop all chunks */
            assert(w->riff->current == -1); /* should be at top level */

            /* at the first 1GB boundary emit AVIOLDINDEX for older AVI applications */
            if (w->group == 0 && w->enable_avioldindex)
                avi_writer_emit_avioldindex(w);

            /* [1] RIFF:AVIX */
            assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
            assert(riff_stack_set_chunk_list_type(&chunk,riff_RIFF,riff_fourcc_const('A','V','I','X')));
            if (w->enable_stream_writing) {
                assert(riff_stack_enable_placeholder(w->riff,&chunk));
                chunk.disable_sync = 1;
            }
            assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
            if (w->enable_stream_writing) riff_stack_header_sync(w->riff,riff_stack_top(w->riff));

            /* start the movi chunk */
            assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
            assert(riff_stack_set_chunk_list_type(&chunk,riff_LIST,riff_fourcc_const('m','o','v','i')));
            if (w->enable_stream_writing) {
                assert(riff_stack_enable_placeholder(w->riff,&chunk));
                chunk.disable_sync = 1;
            }
            assert(riff_stack_push(w->riff,&chunk)); /* NTS: we can reuse chunk, the stack copies it here */
            if (w->enable_stream_writing) riff_stack_header_sync(w->riff,riff_stack_top(w->riff));
            w->movi = chunk;

            w->group++;
        }
    }
    else {
        /* else, if we're about to pass 2GB, then stop allowing any more data, because the traditional
         * AVI format uses 32-bit integers and most implementations treat them as signed. */
        if (((unsigned long long)w->movi.absolute_data_offset +
             (unsigned long long)w->riff->top->write_offset +
             (unsigned long long)len) >= 0x7FF00000ULL) /* 2GB - 16MB */
            return 0;
    }

    /* write chunk into movi */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_data_type(&chunk,s->chunk_fourcc));
    assert(riff_stack_push(w->riff,&chunk));
    if (w->enable_stream_writing) {
        /* use an optimized version of riff_stack_write() that blasts the RIFF chunk header + data in one go */
        if (data != NULL && len > 0)
            assert((int)riff_stack_streamwrite(w->riff,riff_stack_top(w->riff),data,(size_t)len) == (int)len);
        else
            assert((int)riff_stack_streamwrite(w->riff,riff_stack_top(w->riff),NULL,(size_t)0) == (int)0);
    }
    else {
        if (data != NULL && len > 0)
            assert((int)riff_stack_write(w->riff,riff_stack_top(w->riff),data,(size_t)len) == (int)len);
    }
    riff_stack_pop(w->riff);

    /* put the data into the index */
    if (!avi_writer_stream_check_samplecount(s,s->sample_write_chunk+16))
        return 0;

    s->sample_index_max = s->sample_write_chunk+1;
    assert(s->sample_index_max < s->sample_index_alloc);
    si = s->sample_index + s->sample_write_chunk;

    si->stream_offset = s->sample_write_offset;
    si->offset = (uint64_t)chunk.absolute_data_offset;
    si->length = (uint32_t)len;
    si->dwFlags = flags;

    s->sample_write_offset += (unsigned int)len;
    s->sample_write_chunk++;

    /* if stream writing is not enabled, then rewrite all RIFF parent chunks to reflect the new data */
    if (!w->enable_stream_writing)
        riff_stack_header_sync_all(w->riff);

    return 1;
}

/* caller must ensure that we're at the RIFF:AVI(X) level, not anywhere else! */
int avi_writer_emit_avioldindex(avi_writer *w) {
    riff_idx1_AVIOLDINDEX *ie;
    unsigned int i,c,co;
    riff_chunk chunk;

    if (w == NULL) return 0;
    if (w->wrote_idx1) return 0;
    if (w->group != 0) return 0;
    if (avi_io_buffer_init(sizeof(*ie)) == NULL) return 0;

    /* write chunk into movi */
    assert(riff_stack_begin_new_chunk_here(w->riff,&chunk));
    assert(riff_stack_set_chunk_data_type(&chunk,avi_riff_idx1));
    assert(riff_stack_push(w->riff,&chunk));

    /* scan all frames, stopping when all streams have been scanned */
    i=0U;
    do {
        co=0U;
        for (c=0U;c < (unsigned int)w->avi_stream_max;c++) {
            avi_writer_stream *s = w->avi_stream + c;
            if (i < s->sample_index_max) {
                avi_writer_stream_index *sie = s->sample_index + i;
                /* AVIOLDINDEX: offsets are relative to movi chunk header offset + 8 */
                long long ofs = ((long long)sie->offset - 8LL) - ((long long)w->movi.absolute_header_offset + 8LL);
                assert(ofs >= 0);
                if (ofs < 0x80000000LL) { /* the AVIOLDINDEX can only support 32-bit offsets */
                    if ((avi_io_write+sizeof(*ie)) > avi_io_fence) {
                        size_t sz = (size_t)(avi_io_write - avi_io_buf);
                        /* flush to disk */
                        assert(riff_stack_write(w->riff,riff_stack_top(w->riff),avi_io_buf,sz) == (int)sz);
                        /* reset pointer */
                        avi_io_write = avi_io_buf;
                    }

                    /* TODO: FIXME This needs to use the rawint.h macros to set the variables! */
                    ie = (riff_idx1_AVIOLDINDEX*)avi_io_write;
                    avi_io_write += sizeof(*ie);
                    ie->dwChunkId = s->chunk_fourcc;
                    ie->dwFlags = sie->dwFlags;
                    ie->dwOffset = (uint32_t)(ofs);
                    ie->dwSize = sie->length;
                    co++;
                }
            }
        }
        if (co != 0) i++;
    } while (co != 0);

    if (avi_io_write != avi_io_fence) {
        size_t sz = (size_t)(avi_io_write - avi_io_buf);
        /* flush to disk */
        assert(riff_stack_write(w->riff,riff_stack_top(w->riff),avi_io_buf,sz) == (int)sz);
        /* reset pointer */
        avi_io_write = avi_io_buf;
    }

    riff_stack_pop(w->riff);
    avi_io_buffer_free();
    w->wrote_idx1 = 1;
    return 1;
}

int avi_writer_update_avi_and_stream_headers(avi_writer *w) {
    avi_writer_stream *s;
    int stream;

    if (w == NULL) return 0;
    if (w->state == AVI_WRITER_STATE_INIT || w->state == AVI_WRITER_STATE_DONE) return 0;

    if (w->enable_avioldindex || w->enable_opendml_index) {
        /* FIXME: This needs to use the rawint.h macros to read and set the value */
        w->main_header.dwFlags |=
            riff_avih_AVIMAINHEADER_flags_HASINDEX |
            riff_avih_AVIMAINHEADER_flags_MUSTUSEINDEX |
            riff_avih_AVIMAINHEADER_flags_ISINTERLEAVED;
    }

    /* NTS: As of 2013/02/06 we now allow the caller to pre-set
     *      the dwLength value, and we update it with our own if
     *      the actual frame count is larger */
    /* FIXME: When writing OpenDML files, we're actually supposed to set the stream header's
     *        dwLength to what would be the maximum length for older programs that do not
     *        read OpenDML extensions, and then we write an extended chunk into the stream
     *        header LIST that holds the true length. When will we implement this? */
    for (stream=0;stream < w->avi_stream_max;stream++) {
        s = w->avi_stream + stream;
        if (s->header.fccType == avi_fccType_video || s->header.fccType == avi_fccType_iavs) {
            if (s->header.dwLength < s->sample_index_max)
                s->header.dwLength = s->sample_index_max;
        }
        else if (s->header.fccType == avi_fccType_audio) {
            unsigned int nlength=0;

            if (s->format != NULL && s->format_len >= sizeof(windows_WAVEFORMAT)) {
                windows_WAVEFORMAT *wf = (windows_WAVEFORMAT*)(s->format);
                unsigned int nss = __le_u16(&wf->nBlockAlign);
                if (nss != 0) nlength = s->sample_write_offset / nss;
            }
            else {
                nlength = s->sample_write_offset;
            }

            if (s->header.dwLength < nlength)
                s->header.dwLength = nlength;
        }

        if (s->strh.absolute_data_offset != 0LL && s->strh.data_length >= sizeof(riff_strh_AVISTREAMHEADER)) {
            riff_chunk r = s->strh;
            assert(riff_stack_seek(w->riff,&r,0) == 0);
            assert(riff_stack_write(w->riff,&r,&s->header,sizeof(s->header)) == (int)sizeof(s->header));
        }
    }

    /* FIXME: This needs to use the rawint.h macros to set the value */
    w->main_header.dwTotalFrames = 0;
    for (stream=0;stream < w->avi_stream_max;stream++) {
        s = w->avi_stream + stream;
        if (s->header.fccType == avi_fccType_video || s->header.fccType == avi_fccType_iavs) {
            /* FIXME: This needs to use the rawint.h macros to set the value */
            w->main_header.dwTotalFrames = s->header.dwLength;
            break;
        }
    }

    if (w->avih.absolute_data_offset != 0LL && w->avih.data_length >= sizeof(riff_avih_AVIMAINHEADER)) {
        riff_chunk r = w->avih;
        assert(riff_stack_seek(w->riff,&r,0) == 0);
        assert(riff_stack_write(w->riff,&r,&w->main_header,sizeof(w->main_header)) == (int)sizeof(w->main_header));
    }

    return 1;
}

/* NTS: this writer keeps the AVISUPERINDEXes in the header, therefore the file offset
 *      returned is never larger than about 2MB or so */
uint64_t avi_writer_stream_alloc_superindex(avi_writer *w,avi_writer_stream *s) {
    riff_chunk chunk;

    if (w == NULL || s == NULL)
        return 0ULL;

    if (s->indx_junk.absolute_data_offset != 0LL) {
        if (s->indx_junk.data_length < 256) return 0ULL;

        /* convert the JUNK chunk into an indx chunk */
        {
            assert(riff_stack_seek(w->riff,NULL,s->indx_junk.absolute_header_offset) == s->indx_junk.absolute_header_offset);
            assert(riff_stack_write(w->riff,NULL,"indx",4) == 4);
        }

        /* emit the header */
        {
            s->superindex.wLongsPerEntry = 4;
            s->superindex.bIndexType = riff_indx_type_AVI_INDEX_OF_INDEXES;
            s->superindex.nEntriesInUse = 1;
            s->superindex.dwChunkId = s->chunk_fourcc;
            chunk = s->indx_junk;
            assert(riff_stack_seek(w->riff,&chunk,0) == 0);
            assert(riff_stack_write(w->riff,&chunk,&s->superindex,sizeof(s->superindex)) == (int)sizeof(s->superindex));
            s->indx_entryofs = sizeof(s->superindex);
        }

        /* now it's an indx chunk we treat it as such */
        s->indx = s->indx_junk;
        s->indx_junk.absolute_data_offset = 0LL;
    }
    else if (s->indx.absolute_data_offset != 0LL) {
        if ((s->indx_entryofs + sizeof(riff_indx_AVISUPERINDEX_entry)) > s->indx.data_length)
            return 0ULL;

        chunk = s->indx;
        s->superindex.nEntriesInUse++;
        assert(riff_stack_seek(w->riff,&chunk,0) == 0);
        assert(riff_stack_write(w->riff,&chunk,&s->superindex,sizeof(s->superindex)) == (int)sizeof(s->superindex));
    }

    if (s->indx.absolute_data_offset != 0LL) {
        if ((s->indx_entryofs + sizeof(riff_indx_AVISUPERINDEX_entry)) > s->indx.data_length)
            return 0ULL;

        uint64_t ofs = (uint64_t)s->indx.absolute_data_offset + (uint64_t)s->indx_entryofs;
        s->indx_entryofs += (unsigned int)sizeof(riff_indx_AVISUPERINDEX_entry);
        return ofs;
    }

    return 0ULL;
}

/* caller must ensure we're at the movi chunk level */
int avi_writer_emit_opendml_indexes(avi_writer *w) {
    unsigned long long chunk_ofs=0,chunk_max;
    riff_indx_AVISUPERINDEX_entry suie;
    avi_writer_stream_index *si;
    unsigned int chunk,chks,out_chunks;
    riff_indx_AVISTDINDEX_entry *stdie;
    unsigned long long superindex;
    riff_indx_AVISTDINDEX stdh;
    avi_writer_stream *s;
    riff_chunk newchunk;
    int stream,in1,in2;
    long long offset;

    if (w == NULL) return 0;
    if (!w->enable_opendml_index) return 0;
    if (avi_io_buffer_init(sizeof(*stdie)) == NULL) return 0;

    for (stream=0;stream < w->avi_stream_max;stream++) {
        s = w->avi_stream + stream;
        if (s->indx_entryofs != 0) break;
        if (s->sample_index == NULL) continue;
        in1 = ((stream / 10) % 10) + '0';
        in2 = (stream % 10) + '0';

        for (chunk=0;chunk < s->sample_index_max;) {
            /* scan up to 2000 samples, and determine a good base offset for them */
            si = s->sample_index + chunk;
            chunk_ofs = chunk_max = si->offset;
            chks = chunk + 1; si++;
            while (chks < (chunk + 2000) && chks < s->sample_index_max) {
                if (chunk_max < si->offset) {
                    if (si->offset > (chunk_ofs + 0x7FFF0000ULL))
                        break;

                    chunk_max = si->offset;
                }
                else if (chunk_ofs > si->offset) {
                    if ((si->offset + 0x7FFF0000ULL) <= chunk_max)
                        break;

                    chunk_ofs = si->offset;
                }

                chks++;
                si++;
            }

            /* make sure the above loop does it's job */
            assert((chunk_ofs + 0x7FFF0000ULL) > chunk_max);

            /* start an AVISUPERINDEX */
            out_chunks = 0;
            if ((superindex = avi_writer_stream_alloc_superindex(w,s)) == 0ULL) {
                fprintf(stderr,"Cannot alloc superindex for %d\n",s->index);
                break;
            }

            /* start an index chunk */
            assert(riff_stack_begin_new_chunk_here(w->riff,&newchunk));
            assert(riff_stack_set_chunk_data_type(&newchunk,riff_fourcc_const('i','x',in1,in2)));
            assert(riff_stack_push(w->riff,&newchunk)); /* NTS: we can reuse chunk, the stack copies it here */

            memset(&stdh,0,sizeof(stdh));
            stdh.wLongsPerEntry = 2;
            stdh.bIndexType = riff_indx_type_AVI_INDEX_OF_CHUNKS;
            stdh.dwChunkId = s->chunk_fourcc;
            stdh.qwBaseOffset = chunk_ofs;
            assert(riff_stack_write(w->riff,riff_stack_top(w->riff),&stdh,sizeof(stdh)) == (int)sizeof(stdh));

            avi_io_write = avi_io_buf;
            while (chunk < s->sample_index_max) {
                si = s->sample_index + chunk;

                offset = (long long)si->offset - (long long)chunk_ofs;
                if (offset < 0LL || offset >= 0x7FFF0000LL)
                    break;

                if ((avi_io_write+sizeof(*stdie)) > avi_io_fence) {
                    size_t sz = (size_t)(avi_io_write - avi_io_buf);
                    /* flush to disk */
                    assert(riff_stack_write(w->riff,riff_stack_top(w->riff),avi_io_buf,sz) == (int)sz);
                    /* reset pointer */
                    avi_io_write = avi_io_buf;
                }

                stdie = (riff_indx_AVISTDINDEX_entry*)avi_io_write;
                avi_io_write += sizeof(*stdie);

                stdie->dwOffset = (uint32_t)offset;
                stdie->dwSize = si->length;
                if ((si->dwFlags & riff_idx1_AVIOLDINDEX_flags_KEYFRAME) == 0) stdie->dwSize |= (1UL << 31UL);
                out_chunks++;
                chunk++;
            }

            if (avi_io_write != avi_io_fence) {
                size_t sz = (size_t)(avi_io_write - avi_io_buf);
                /* flush to disk */
                assert(riff_stack_write(w->riff,riff_stack_top(w->riff),avi_io_buf,sz) == (int)sz);
                /* reset pointer */
                avi_io_write = avi_io_buf;
            }

            assert(out_chunks != 0);
            stdh.nEntriesInUse = out_chunks;
            assert(riff_stack_seek(w->riff,riff_stack_top(w->riff),0) == 0);
            assert(riff_stack_write(w->riff,riff_stack_top(w->riff),&stdh,sizeof(stdh)) == (int)sizeof(stdh));

            newchunk = *riff_stack_top(w->riff);
            riff_stack_pop(w->riff); /* end ix## */

            suie.qwOffset = (uint64_t)newchunk.absolute_header_offset;
            suie.dwDuration = out_chunks;
            suie.dwSize = newchunk.data_length + 8;
            assert(riff_stack_seek(w->riff,NULL,(int64_t)superindex) == (int64_t)superindex);
            assert(riff_stack_write(w->riff,NULL,&suie,sizeof(suie)) == (int)sizeof(suie));
        }
    }

    avi_io_buffer_free();
    return 1;
}

int avi_writer_end_data(avi_writer *w) {
    if (w == NULL) return 0;
    if (w->state != AVI_WRITER_STATE_BODY) return 0;

    /* if we're still in the movi chunk, and we're asked to write the OpenDML 2.0
     * AVI index, do it now */
    while (w->riff->current > 1) riff_stack_pop(w->riff);
    if (w->riff->current == 1 && w->riff->top->fourcc == avi_riff_movi)
        avi_writer_emit_opendml_indexes(w);

    /* in case additional headers inserted are off-track, pop them.
     * AND pop out of the LIST:movi chunk too */
    while (w->riff->current > 0)
        riff_stack_pop(w->riff);

    /* now, while at this level, emit the AVIOLDINDEX if this is still the first movi */
    if (w->group == 0 && w->enable_avioldindex)
        avi_writer_emit_avioldindex(w);

    /* stay at this level, if the caller wants to add more chunks of his own */
    w->state = AVI_WRITER_STATE_FOOTER;
    riff_stack_header_sync_all(w->riff);
    avi_writer_update_avi_and_stream_headers(w);
    return 1;
}

int avi_writer_finish(avi_writer *w) {
    if (w == NULL) return 0;
    if (w->state != AVI_WRITER_STATE_FOOTER) return 0;

    /* pop it all down */
    while (w->riff->current > 0)
        riff_stack_pop(w->riff);

    riff_stack_header_sync_all(w->riff);
    w->state = AVI_WRITER_STATE_DONE;
    return 1;
}

/* if the caller is daring he can set stream writing mode where
 * this code minimizes disk seeking and enforces writing the
 * AVI file in a continuous stream.
 *
 * - Be aware that enabling this may make the AVI structure less
 *   coherent if this code is never given the chance to complete
 *   the writing process (i.e. because you crashed...)
 *
 * - Once you start the writing process, you cannot change stream
 *   writing mode */
int avi_writer_set_stream_writing(avi_writer *w) {
    if (w == NULL) return 0;
    if (w->state != AVI_WRITER_STATE_INIT) return 0;
    w->enable_stream_writing = 1;
    return 1;
}

