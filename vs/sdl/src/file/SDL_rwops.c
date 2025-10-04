/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#if defined(_MSC_VER)
# pragma warning(disable:4996) /* Nobody cares that GetVersionExA() is deprecated */
#endif

/* This file provides a general interface for SDL to read and write
   data sources.  It can easily be extended to files, memory, etc.
*/

#include "SDL_endian.h"
#include "SDL_rwops.h"


#if defined(__WIN32__) && !defined(__SYMBIAN32__)

/* Functions to read/write Win32 API file pointers */
/* Will not use it on WinCE because stdio is buffered, it means
   faster, and all stdio functions anyway are embedded in coredll.dll - 
   the main wince dll*/

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

#define READAHEAD_BUFFER_SIZE	1024

static int SDLCALL win32_file_open(SDL_RWops *context, const char *filename, const char *mode)
{
#ifndef _WIN32_WCE
	UINT	old_error_mode;
#endif
	HANDLE	h;
	DWORD	r_right, w_right;
	DWORD	must_exist, truncate;
	int		a_mode;

	if (!context)
		return -1; /* failed (invalid call) */
		
	context->hidden.win32io.h = INVALID_HANDLE_VALUE; /* mark this as unusable */
	context->hidden.win32io.buffer.data = NULL;
	context->hidden.win32io.buffer.size = 0;
	context->hidden.win32io.buffer.left = 0;

	/* "r" = reading, file must exist */
	/* "w" = writing, truncate existing, file may not exist */
	/* "r+"= reading or writing, file must exist            */
	/* "a" = writing, append file may not exist             */
	/* "a+"= append + read, file may not exist              */
	/* "w+" = read, write, truncate. file may not exist    */
	
	must_exist = ( SDL_strchr(mode,'r') != NULL ) ? OPEN_EXISTING : 0;
	truncate   = ( SDL_strchr(mode,'w') != NULL ) ? CREATE_ALWAYS : 0;
	r_right    = ( SDL_strchr(mode,'+') != NULL || must_exist ) ? GENERIC_READ : 0;
	a_mode     = ( SDL_strchr(mode,'a') != NULL ) ? OPEN_ALWAYS : 0;
	w_right    = ( a_mode || SDL_strchr(mode,'+') || truncate ) ? GENERIC_WRITE : 0;

	if (!r_right && !w_right) /* inconsistent mode */
		return -1; /* failed (invalid call) */

	context->hidden.win32io.buffer.data = (char *)SDL_malloc(READAHEAD_BUFFER_SIZE);
	if (!context->hidden.win32io.buffer.data) {
		SDL_OutOfMemory();
		return -1;
	}

#ifdef _WIN32_WCE
	{
		size_t size = SDL_strlen(filename)+1;
		wchar_t *filenameW = SDL_stack_alloc(wchar_t, size);

		if ( MultiByteToWideChar(CP_UTF8, 0, filename, -1, filenameW, size) == 0 ) {
			SDL_stack_free(filenameW);
			SDL_free(context->hidden.win32io.buffer.data);
			context->hidden.win32io.buffer.data = NULL;
			SDL_SetError("Unable to convert filename to Unicode");
			return -1;
		}
		h = CreateFile(filenameW, (w_right|r_right), (w_right)? 0 : FILE_SHARE_READ, 
					   NULL, (must_exist|truncate|a_mode), FILE_ATTRIBUTE_NORMAL,NULL);
		SDL_stack_free(filenameW);
	}
#else
	{

	/* handle Unicode filenames.  We do some tapdancing here to make sure this
	   works on Win9x, which doesn't support anything but 1-byte codepages. */
	const size_t size = SDL_strlen(filename)+1;
	static int unicode_support = -1;

	if (unicode_support == -1) {
		OSVERSIONINFO osVerInfo;     /* Information about the OS */
		osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
		if (!GetVersionEx(&osVerInfo)) {
			unicode_support = 0;
		} else if (osVerInfo.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS) {
			unicode_support = 1;  /* Not Win95/98/ME. */
		} else {
			unicode_support = 0;
		}
	}

	if (unicode_support) {  /* everything but Win95/98/ME. */
		wchar_t *filenameW = SDL_stack_alloc(wchar_t, size);
		if ( MultiByteToWideChar(CP_UTF8, 0, filename, -1, filenameW, (int)size) == 0 ) {
			SDL_stack_free(filenameW);
			SDL_free(context->hidden.win32io.buffer.data);
			context->hidden.win32io.buffer.data = NULL;
			SDL_SetError("Unable to convert filename to Unicode");
			return -1;
		}

		/* Do not open a dialog box if failure */
		old_error_mode = SetErrorMode(SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS);
		h = CreateFileW(filenameW, (w_right|r_right), (w_right)? 0 : FILE_SHARE_READ,
					   NULL, (must_exist|truncate|a_mode), FILE_ATTRIBUTE_NORMAL,NULL);
		/* restore old behaviour */
		SetErrorMode(old_error_mode);

		SDL_stack_free(filenameW);
	} else {
		/* CP_UTF8 might not be supported (Win95), so use SDL_iconv to get wchars. */
		/* Use UCS2: no UTF-16 support here. Try again in SDL 1.3.  :) */
		char *utf16 = SDL_iconv_string("UCS2", "UTF8", filename, SDL_strlen(filename) + 1);
		char *filenameA = SDL_stack_alloc(char, size * 6);  /* 6, just in case. */
		BOOL bDefCharUsed = FALSE;

		/* Dither down to a codepage and hope for the best. */
		if (!utf16 ||
			!WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)utf16, -1, filenameA, (int)(size*6), 0, &bDefCharUsed) ||
			bDefCharUsed) {
			SDL_stack_free(filenameA);
			SDL_free(utf16);
			SDL_free(context->hidden.win32io.buffer.data);
			context->hidden.win32io.buffer.data = NULL;
			SDL_SetError("Unable to convert filename to Unicode");
			return -1;
		}

		/* Do not open a dialog box if failure */
		old_error_mode = SetErrorMode(SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS);
		h = CreateFile(filenameA, (w_right|r_right), (w_right)? 0 : FILE_SHARE_READ,
		               NULL, (must_exist|truncate|a_mode), FILE_ATTRIBUTE_NORMAL,NULL);
		/* restore old behaviour */
		SetErrorMode(old_error_mode);

		SDL_stack_free(filenameA);
		SDL_free(utf16);
	}

	}
#endif /* _WIN32_WCE */

	if (h==INVALID_HANDLE_VALUE) {
		SDL_free(context->hidden.win32io.buffer.data);
		context->hidden.win32io.buffer.data = NULL;
		SDL_SetError("Couldn't open %s",filename);
		return -2; /* failed (CreateFile) */
	}
	context->hidden.win32io.h = h;
	context->hidden.win32io.append = a_mode;

	return 0; /* ok */
}
static int SDLCALL win32_file_seek(SDL_RWops *context, int offset, int whence)
{
	DWORD win32whence;
	int   file_pos;
	
	if (!context || context->hidden.win32io.h == INVALID_HANDLE_VALUE) {
		SDL_SetError("win32_file_seek: invalid context/file not opened");
		return -1;
	}

	/* FIXME: We may be able to satisfy the seek within buffered data */
	if (whence == RW_SEEK_CUR && context->hidden.win32io.buffer.left) {
		offset -= context->hidden.win32io.buffer.left;
    }
    context->hidden.win32io.buffer.left = 0;

	switch (whence) {
		case RW_SEEK_SET:		
			win32whence = FILE_BEGIN; break;
		case RW_SEEK_CUR:
			win32whence = FILE_CURRENT; break;
		case RW_SEEK_END:
			win32whence = FILE_END; break;
		default:
			SDL_SetError("win32_file_seek: Unknown value for 'whence'");			
			return -1;
	}

	file_pos = SetFilePointer(context->hidden.win32io.h,offset,NULL,win32whence);

	if ( file_pos != INVALID_SET_FILE_POINTER )
		return file_pos; /* success */
	
	SDL_Error(SDL_EFSEEK);
	return -1; /* error */
}
static int SDLCALL win32_file_read(SDL_RWops *context, void *ptr, int size, int maxnum)
{
	int		total_need; 
	int		total_read = 0; 
    int     read_ahead;
	DWORD	byte_read;
	
	total_need = size*maxnum;
	
	if (!context || context->hidden.win32io.h == INVALID_HANDLE_VALUE || total_need<=0 || !size) 	
		return 0;

    if (context->hidden.win32io.buffer.left > 0) {
        void *data = (char *)context->hidden.win32io.buffer.data +
                             context->hidden.win32io.buffer.size -
                             context->hidden.win32io.buffer.left;
        read_ahead = SDL_min(total_need, context->hidden.win32io.buffer.left); 
        SDL_memcpy(ptr, data, read_ahead);
        context->hidden.win32io.buffer.left -= read_ahead;

        if (read_ahead == total_need) {
            return maxnum;
        }
        ptr = (char *)ptr + read_ahead;
        total_need -= read_ahead;       
		total_read += read_ahead;
    }

    if (total_need < READAHEAD_BUFFER_SIZE) {
        if (!ReadFile(context->hidden.win32io.h,context->hidden.win32io.buffer.data,READAHEAD_BUFFER_SIZE,&byte_read,NULL)) {
            SDL_Error(SDL_EFREAD);
            return 0;
        }
        read_ahead = SDL_min(total_need, (int)byte_read);
        SDL_memcpy(ptr, context->hidden.win32io.buffer.data, read_ahead);
        context->hidden.win32io.buffer.size = byte_read;
        context->hidden.win32io.buffer.left = byte_read-read_ahead;
        total_read += read_ahead;
    } else {
        if (!ReadFile(context->hidden.win32io.h,ptr,total_need,&byte_read,NULL)) {
            SDL_Error(SDL_EFREAD);
            return 0;
        }
        total_read += byte_read;
    }
	return (total_read/size);
}
static int SDLCALL win32_file_write(SDL_RWops *context, const void *ptr, int size, int num)
{
	
	int		total_bytes; 
	DWORD	byte_written,nwritten;
	
	total_bytes = size*num;

	if (!context || context->hidden.win32io.h==INVALID_HANDLE_VALUE || total_bytes<=0 || !size) 	
		return 0;

	if (context->hidden.win32io.buffer.left) {
		SetFilePointer(context->hidden.win32io.h,-context->hidden.win32io.buffer.left,NULL,FILE_CURRENT);
		context->hidden.win32io.buffer.left = 0;
	}

	/* if in append mode, we must go to the EOF before write */
	if (context->hidden.win32io.append) {
		if ( SetFilePointer(context->hidden.win32io.h,0L,NULL,FILE_END) == INVALID_SET_FILE_POINTER ) {
			SDL_Error(SDL_EFWRITE);
			return 0;
		}
	}
	
	if (!WriteFile(context->hidden.win32io.h,ptr,total_bytes,&byte_written,NULL)) {
		SDL_Error(SDL_EFWRITE);
		return 0;
	}
	
	nwritten = byte_written/size;
	return nwritten;
}
static int SDLCALL win32_file_close(SDL_RWops *context)
{
	
	if ( context ) {								
		if (context->hidden.win32io.h != INVALID_HANDLE_VALUE) {
			CloseHandle(context->hidden.win32io.h);
			context->hidden.win32io.h = INVALID_HANDLE_VALUE; /* to be sure */
		}
		if (context->hidden.win32io.buffer.data) {
			SDL_free(context->hidden.win32io.buffer.data);
			context->hidden.win32io.buffer.data = NULL;
		}
		SDL_FreeRW(context);
	}
	return(0);
}
#endif /* __WIN32__ */

#ifdef HAVE_STDIO_H

/* Functions to read/write stdio file pointers */

static int SDLCALL stdio_seek(SDL_RWops *context, int offset, int whence)
{
	if ( fseek(context->hidden.stdio.fp, offset, whence) == 0 ) {
		return(ftell(context->hidden.stdio.fp));
	} else {
		SDL_Error(SDL_EFSEEK);
		return(-1);
	}
}
static int SDLCALL stdio_read(SDL_RWops *context, void *ptr, int size, int maxnum)
{
	size_t nread;

	nread = fread(ptr, size, maxnum, context->hidden.stdio.fp); 
	if ( nread == 0 && ferror(context->hidden.stdio.fp) ) {
		SDL_Error(SDL_EFREAD);
	}
	return (int)nread;
}
static int SDLCALL stdio_write(SDL_RWops *context, const void *ptr, int size, int num)
{
	size_t nwrote;

	nwrote = fwrite(ptr, size, num, context->hidden.stdio.fp);
	if ( nwrote == 0 && ferror(context->hidden.stdio.fp) ) {
		SDL_Error(SDL_EFWRITE);
	}
	return (int)nwrote;
}
static int SDLCALL stdio_close(SDL_RWops *context)
{
	if ( context ) {
		if ( context->hidden.stdio.autoclose ) {
			/* WARNING:  Check the return value here! */
			fclose(context->hidden.stdio.fp);
		}
		SDL_FreeRW(context);
	}
	return(0);
}
#endif /* !HAVE_STDIO_H */

/* Functions to read/write memory pointers */

static int SDLCALL mem_seek(SDL_RWops *context, int offset, int whence)
{
	Uint8 *newpos;

	switch (whence) {
		case RW_SEEK_SET:
			newpos = context->hidden.mem.base+offset;
			break;
		case RW_SEEK_CUR:
			newpos = context->hidden.mem.here+offset;
			break;
		case RW_SEEK_END:
			newpos = context->hidden.mem.stop+offset;
			break;
		default:
			SDL_SetError("Unknown value for 'whence'");
			return(-1);
	}
	if ( newpos < context->hidden.mem.base ) {
		newpos = context->hidden.mem.base;
	}
	if ( newpos > context->hidden.mem.stop ) {
		newpos = context->hidden.mem.stop;
	}
	context->hidden.mem.here = newpos;
	return (int)(context->hidden.mem.here-context->hidden.mem.base);
}
static int SDLCALL mem_read(SDL_RWops *context, void *ptr, int size, int maxnum)
{
	size_t total_bytes;
	size_t mem_available;

	total_bytes = (maxnum * size);
	if ( (maxnum <= 0) || (size <= 0) || ((total_bytes / maxnum) != (size_t) size) ) {
		return 0;
	}

	mem_available = (context->hidden.mem.stop - context->hidden.mem.here);
	if (total_bytes > mem_available) {
		total_bytes = mem_available;
	}

	SDL_memcpy(ptr, context->hidden.mem.here, total_bytes);
	context->hidden.mem.here += total_bytes;

	return (int)(total_bytes / size);
}
static int SDLCALL mem_write(SDL_RWops *context, const void *ptr, int size, int num)
{
	if ( (context->hidden.mem.here + (num*size)) > context->hidden.mem.stop ) {
		num = (int)((context->hidden.mem.stop-context->hidden.mem.here)/size);
	}
	SDL_memcpy(context->hidden.mem.here, ptr, num*size);
	context->hidden.mem.here += num*size;
	return(num);
}
static int SDLCALL mem_writeconst(SDL_RWops *context, const void *ptr, int size, int num)
{
	SDL_SetError("Can't write to read-only memory");
	return(-1);
}
static int SDLCALL mem_close(SDL_RWops *context)
{
	if ( context ) {
		SDL_FreeRW(context);
	}
	return(0);
}


/* Functions to create SDL_RWops structures from various data sources */

#ifdef __MACOS__
/*
 * translate unix-style slash-separated filename to mac-style colon-separated
 * name; return malloced string
 */
static char *unix_to_mac(const char *file)
{
	int flen = SDL_strlen(file);
	char *path = SDL_malloc(flen + 2);
	const char *src = file;
	char *dst = path;
	if(*src == '/') {
		/* really depends on filesystem layout, hope for the best */
		src++;
	} else {
		/* Check if this is a MacOS path to begin with */
		if(*src != ':')
			*dst++ = ':';   /* relative paths begin with ':' */
	}
	while(src < file + flen) {
		const char *end = SDL_strchr(src, '/');
		int len;
		if(!end)
			end = file + flen; /* last component */
		len = end - src;
		if(len == 0 || (len == 1 && src[0] == '.')) {
			/* remove repeated slashes and . */
		} else {
			if(len == 2 && src[0] == '.' && src[1] == '.') {
				/* replace .. with the empty string */
			} else {
				SDL_memcpy(dst, src, len);
				dst += len;
			}
			if(end < file + flen)
				*dst++ = ':';
		}
		src = end + 1;
	}
	*dst++ = '\0';
	return path;
}
#endif /* __MACOS__ */

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode)
{
	SDL_RWops *rwops = NULL;
#ifdef HAVE_STDIO_H
	FILE *fp = NULL;
	(void) fp;
#endif
	if ( !file || !*file || !mode || !*mode ) {
		SDL_SetError("SDL_RWFromFile(): No file or no mode specified");
		return NULL;
	}

#if defined(__WIN32__) && !defined(__SYMBIAN32__)
	rwops = SDL_AllocRW();
	if (!rwops)
		return NULL; /* SDL_SetError already setup by SDL_AllocRW() */
	if (win32_file_open(rwops,file,mode) < 0) {
		SDL_FreeRW(rwops);
		return NULL;
	}	
	rwops->seek  = win32_file_seek;
	rwops->read  = win32_file_read;
	rwops->write = win32_file_write;
	rwops->close = win32_file_close;

#elif HAVE_STDIO_H

#ifdef __MACOS__
	{
		char *mpath = unix_to_mac(file);
		fp = fopen(mpath, mode);
		SDL_free(mpath);
	}
#else
	fp = fopen(file, mode);
#endif
	if ( fp == NULL ) {
		SDL_SetError("Couldn't open %s", file);
	} else {
		rwops = SDL_RWFromFP(fp, 1);
	}
#else
	SDL_SetError("SDL not compiled with stdio support");
#endif /* !HAVE_STDIO_H */

	return(rwops);
}

#ifdef HAVE_STDIO_H
SDL_RWops *SDL_RWFromFP(FILE *fp, int autoclose)
{
	SDL_RWops *rwops = NULL;

	rwops = SDL_AllocRW();
	if ( rwops != NULL ) {
		rwops->seek = stdio_seek;
		rwops->read = stdio_read;
		rwops->write = stdio_write;
		rwops->close = stdio_close;
		rwops->hidden.stdio.fp = fp;
		rwops->hidden.stdio.autoclose = autoclose;
	}
	return(rwops);
}
#endif /* HAVE_STDIO_H */

SDL_RWops *SDL_RWFromMem(void *mem, int size)
{
	SDL_RWops *rwops;

	rwops = SDL_AllocRW();
	if ( rwops != NULL ) {
		rwops->seek = mem_seek;
		rwops->read = mem_read;
		rwops->write = mem_write;
		rwops->close = mem_close;
		rwops->hidden.mem.base = (Uint8 *)mem;
		rwops->hidden.mem.here = rwops->hidden.mem.base;
		rwops->hidden.mem.stop = rwops->hidden.mem.base+size;
	}
	return(rwops);
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, int size)
{
	SDL_RWops *rwops;

	rwops = SDL_AllocRW();
	if ( rwops != NULL ) {
		rwops->seek = mem_seek;
		rwops->read = mem_read;
		rwops->write = mem_writeconst;
		rwops->close = mem_close;
		rwops->hidden.mem.base = (Uint8 *)mem;
		rwops->hidden.mem.here = rwops->hidden.mem.base;
		rwops->hidden.mem.stop = rwops->hidden.mem.base+size;
	}
	return(rwops);
}

SDL_RWops *SDL_AllocRW(void)
{
	SDL_RWops *area;

	area = (SDL_RWops *)SDL_malloc(sizeof *area);
	if ( area == NULL ) {
		SDL_OutOfMemory();
	}
	return(area);
}

void SDL_FreeRW(SDL_RWops *area)
{
	SDL_free(area);
}

/* Functions for dynamically reading and writing endian-specific values */

Uint16 SDL_ReadLE16 (SDL_RWops *src)
{
	Uint16 value;

	SDL_RWread(src, &value, (sizeof value), 1);
	return(SDL_SwapLE16(value));
}
Uint16 SDL_ReadBE16 (SDL_RWops *src)
{
	Uint16 value;

	SDL_RWread(src, &value, (sizeof value), 1);
	return(SDL_SwapBE16(value));
}
Uint32 SDL_ReadLE32 (SDL_RWops *src)
{
	Uint32 value;

	SDL_RWread(src, &value, (sizeof value), 1);
	return(SDL_SwapLE32(value));
}
Uint32 SDL_ReadBE32 (SDL_RWops *src)
{
	Uint32 value;

	SDL_RWread(src, &value, (sizeof value), 1);
	return(SDL_SwapBE32(value));
}
Uint64 SDL_ReadLE64 (SDL_RWops *src)
{
	Uint64 value;

	SDL_RWread(src, &value, (sizeof value), 1);
	return(SDL_SwapLE64(value));
}
Uint64 SDL_ReadBE64 (SDL_RWops *src)
{
	Uint64 value;

	SDL_RWread(src, &value, (sizeof value), 1);
	return(SDL_SwapBE64(value));
}

int SDL_WriteLE16 (SDL_RWops *dst, Uint16 value)
{
	value = SDL_SwapLE16(value);
	return(SDL_RWwrite(dst, &value, (sizeof value), 1));
}
int SDL_WriteBE16 (SDL_RWops *dst, Uint16 value)
{
	value = SDL_SwapBE16(value);
	return(SDL_RWwrite(dst, &value, (sizeof value), 1));
}
int SDL_WriteLE32 (SDL_RWops *dst, Uint32 value)
{
	value = SDL_SwapLE32(value);
	return(SDL_RWwrite(dst, &value, (sizeof value), 1));
}
int SDL_WriteBE32 (SDL_RWops *dst, Uint32 value)
{
	value = SDL_SwapBE32(value);
	return(SDL_RWwrite(dst, &value, (sizeof value), 1));
}
int SDL_WriteLE64 (SDL_RWops *dst, Uint64 value)
{
	value = SDL_SwapLE64(value);
	return(SDL_RWwrite(dst, &value, (sizeof value), 1));
}
int SDL_WriteBE64 (SDL_RWops *dst, Uint64 value)
{
	value = SDL_SwapBE64(value);
	return(SDL_RWwrite(dst, &value, (sizeof value), 1));
}
