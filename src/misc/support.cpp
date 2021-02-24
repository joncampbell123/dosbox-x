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
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <string>
  
#include "dosbox.h"
#include "debug.h"
#include "support.h"
#include "video.h"
#include "menu.h"
#include "SDL.h"

void upcase(std::string &str) {
	int (*tf)(int) = std::toupper;
	std::transform(str.begin(), str.end(), str.begin(), tf);
}

void lowcase(std::string &str) {
	int (*tf)(int) = std::tolower;
	std::transform(str.begin(), str.end(), str.begin(), tf);
}

void trim(std::string &str) {
    const char whitespace[] = " \r\t\f\n";
	const auto empty_pfx = str.find_first_not_of(whitespace);
	if (empty_pfx == std::string::npos) {
		str.clear(); // whole string is filled with whitespace
		return;
	}
	const auto empty_sfx = str.find_last_not_of(whitespace);
	str.erase(empty_sfx + 1);
	str.erase(0, empty_pfx);
}

/* 
	Ripped some source from freedos for this one.

*/


/*
 * replaces all instances of character o with character c
 */


void strreplace(char * str,char o,char n) {
	while (*str) {
		if (*str==o) *str=n;
		str++;
	}
}
char *ltrim(char *str) { 
	while (*str && isspace(*reinterpret_cast<unsigned char*>(str))) str++;
	return str;
}

char *rtrim(char *str) {
	char *p;
	p = strchr(str, '\0');
	while (--p >= str && *reinterpret_cast<unsigned char*>(p) != '\f' && isspace(*reinterpret_cast<unsigned char*>(p))) {};

	p[1] = '\0';
	return str;
}

char *trim(char *str) {
	return ltrim(rtrim(str));
}

char * upcase(char * str) {
    for (char* idx = str; *idx ; idx++) *idx = toupper(*reinterpret_cast<unsigned char*>(idx));
    return str;
}

char * lowcase(char * str) {
	for(char* idx = str; *idx ; idx++)  *idx = tolower(*reinterpret_cast<unsigned char*>(idx));
	return str;
}



bool ScanCMDBool(char * cmd,char const * const check) {
	char * scan=cmd;size_t c_len=strlen(check);
	while ((scan=strchr(scan,'/'))) {
		/* found a / now see behind it */
		scan++;
		if (strncasecmp(scan,check,c_len)==0 && (scan[c_len]==' ' || scan[c_len]=='\t' || scan[c_len]=='/' || scan[c_len]==0)) {
		/* Found a math now remove it from the string */
			memmove(scan-1,scan+c_len,strlen(scan+c_len)+1);
			trim(scan-1);
			return true;
		}
	}
	return false;
}

/* This scans the command line for a remaining switch and reports it else returns 0*/
char * ScanCMDRemain(char * cmd) {
	char * scan,*found;
	if ((scan=found=strchr(cmd,'/'))) {
		while ( *scan && !isspace(*reinterpret_cast<unsigned char*>(scan)) ) scan++;
		*scan=0;
		return found;
	} else return 0; 
}

char * StripWord(char *&line) {
	char * scan=line;
	scan=ltrim(scan);
	if (*scan=='"') {
		char * end_quote=strchr(scan+1,'"');
		if (end_quote) {
			*end_quote=0;
			line=ltrim(++end_quote);
			return (scan+1);
		}
	}
	char * begin=scan;
	for (char c = *scan ;(c = *scan);scan++) {
		if (isspace(*reinterpret_cast<unsigned char*>(&c))) {
			*scan++=0;
			break;
		}
	}
	line=scan;
	return begin;
}

char * StripArg(char *&line) {
       char * scan=line;
       int q=0;
       scan=ltrim(scan);
       char * begin=scan;
       for (char c = *scan ;(c = *scan);scan++) {
               if (*scan=='"') {
                       q++;
               } else if (q/2*2==q && isspace(*reinterpret_cast<unsigned char*>(&c))) {
			*scan++=0;
			break;
		}
	}
	line=scan;
	return begin;
}


Bits ConvDecWord(char * word) {
	bool negative=false;Bitu ret=0;
	if (*word=='-') {
		negative=true;
		word++;
	}
	while (char c=*word) {
		ret*=10u;
		ret+=(Bitu)c-'0';
		word++;
	}
	if (negative) return 0-(Bits)ret;
	else return (Bits)ret;
}

Bits ConvHexWord(char * word) {
	Bitu ret=0;
	while (char c=toupper(*reinterpret_cast<unsigned char*>(word))) {
		ret*=16;
		if (c>='0' && c<='9') ret+=(Bitu)c-'0';
		else if (c>='A' && c<='F') ret+=10u+((Bitu)c-'A');
		word++;
	}
	return (Bits)ret;
}

double ConvDblWord(char * word) {
    (void)word;//UNUSED
	return 0.0f;
}


int utf8_encode(char **ptr, const char *fence, uint32_t code) {
    int uchar_size=1;
    char *p = *ptr;

    if (!p) return UTF8ERR_NO_ROOM;
    if (code >= (uint32_t)0x80000000UL) return UTF8ERR_INVALID;
    if (p >= fence) return UTF8ERR_NO_ROOM;

    if (code >= 0x4000000) uchar_size = 6;
    else if (code >= 0x200000) uchar_size = 5;
    else if (code >= 0x10000) uchar_size = 4;
    else if (code >= 0x800) uchar_size = 3;
    else if (code >= 0x80) uchar_size = 2;

    if ((p+uchar_size) > fence) return UTF8ERR_NO_ROOM;

    switch (uchar_size) {
        case 1: *p++ = (char)code;
            break;
        case 2: *p++ = (char)(0xC0 | (code >> 6));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 3: *p++ = (char)(0xE0 | (code >> 12));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 4: *p++ = (char)(0xF0 | (code >> 18));
            *p++ = (char)(0x80 | ((code >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 5: *p++ = (char)(0xF8 | (code >> 24));
            *p++ = (char)(0x80 | ((code >> 18) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
        case 6: *p++ = (char)(0xFC | (code >> 30));
            *p++ = (char)(0x80 | ((code >> 24) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 18) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code & 0x3F));
            break;
    }

    *ptr = p;
    return 0;
}

int utf8_decode(const char **ptr,const char *fence) {
    const char *p = *ptr;
    int uchar_size=1;
    int ret = 0,c;

    if (!p) return UTF8ERR_NO_ROOM;
    if (p >= fence) return UTF8ERR_NO_ROOM;

    ret = (unsigned char)(*p);
    if (ret >= 0xFE) { p++; return UTF8ERR_INVALID; }
    else if (ret >= 0xFC) uchar_size=6;
    else if (ret >= 0xF8) uchar_size=5;
    else if (ret >= 0xF0) uchar_size=4;
    else if (ret >= 0xE0) uchar_size=3;
    else if (ret >= 0xC0) uchar_size=2;
    else if (ret >= 0x80) { p++; return UTF8ERR_INVALID; }

    if ((p+uchar_size) > fence)
        return UTF8ERR_NO_ROOM;

    switch (uchar_size) {
        case 1: p++;
            break;
        case 2: ret = (ret&0x1F)<<6; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 3: ret = (ret&0xF)<<12; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 4: ret = (ret&0x7)<<18; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<12;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 5: ret = (ret&0x3)<<24; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<18;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<12;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
        case 6: ret = (ret&0x1)<<30; p++;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<24;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<18;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<12;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= (c&0x3F)<<6;
            c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
            ret |= c&0x3F;
            break;
    }

    *ptr = p;
    return ret;
}

int utf16le_encode(char **ptr, const char *fence, uint32_t code) {
    char *p = *ptr;

    if (!p) return UTF8ERR_NO_ROOM;
    if (code > 0x10FFFF) return UTF8ERR_INVALID;
    if (code > 0xFFFF) { /* UTF-16 surrogate pair */
        uint32_t lo = (code - 0x10000) & 0x3FF;
        uint32_t hi = ((code - 0x10000) >> 10) & 0x3FF;
        if ((p+2+2) > fence) return UTF8ERR_NO_ROOM;
        *p++ = (char)( (hi+0xD800)       & 0xFF);
        *p++ = (char)(((hi+0xD800) >> 8) & 0xFF);
        *p++ = (char)( (lo+0xDC00)       & 0xFF);
        *p++ = (char)(((lo+0xDC00) >> 8) & 0xFF);
    }
    else if ((code&0xF800) == 0xD800) { /* do not allow accidental surrogate pairs (0xD800-0xDFFF) */
        return UTF8ERR_INVALID;
    }
    else {
        if ((p+2) > fence) return UTF8ERR_NO_ROOM;
        *p++ = (char)( code       & 0xFF);
        *p++ = (char)((code >> 8) & 0xFF);
    }

    *ptr = p;
    return 0;
}

int utf16le_decode(const char **ptr,const char *fence) {
    const char *p = *ptr;
    unsigned int ret,b=2;

    if (!p) return UTF8ERR_NO_ROOM;
    if ((p+1) >= fence) return UTF8ERR_NO_ROOM;

    ret = (unsigned char)p[0];
    ret |= ((unsigned int)((unsigned char)p[1])) << 8;
    if (ret >= 0xD800U && ret <= 0xDBFFU)
        b=4;
    else if (ret >= 0xDC00U && ret <= 0xDFFFU)
        { p++; return UTF8ERR_INVALID; }

    if ((p+b) > fence)
        return UTF8ERR_NO_ROOM;

    p += 2;
    if (ret >= 0xD800U && ret <= 0xDBFFU) {
        /* decode surrogate pair */
        unsigned int hi = ret & 0x3FFU;
        unsigned int lo = (unsigned char)p[0];
        lo |= ((unsigned int)((unsigned char)p[1])) << 8;
        p += 2;
        if (lo < 0xDC00U || lo > 0xDFFFU) return UTF8ERR_INVALID;
        lo &= 0x3FFU;
        ret = ((hi << 10U) | lo) + 0x10000U;
    }

    *ptr = p;
    return (int)ret;
}

#if C_DEBUG
#include <curses.h>
#endif
#if defined(WIN32)
void DOSBox_ConsolePauseWait();
#endif
bool sdl_wait_on_error();

static char buf[1024];           //greater scope as else it doesn't always gets thrown right (linux/gcc2.95)
void E_Exit(const char * format,...) {
#if C_DEBUG && C_HEAVY_DEBUG
 	DEBUG_HeavyWriteLogInstruction();
#endif
	va_list msg;
	va_start(msg,format);
	vsnprintf(buf,sizeof(buf),format,msg);
	va_end(msg);
	buf[sizeof(buf) - 1] = '\0';
	strcat(buf,"\n");
	LOG_MSG("E_Exit: %s\n",buf);
#if defined(WIN32)
	/* Most Windows users DON'T run DOSBox-X from the command line! */
	MessageBox(GetHWND(), buf, "E_Exit", MB_OK | MB_ICONEXCLAMATION);
#endif
#if C_DEBUG
	endwin();
#endif
	fprintf(stderr, "E_Exit: %s\n", buf);
	SDL_Quit();
	if (sdl_wait_on_error()) {
#if defined(WIN32)
        DOSBox_ConsolePauseWait();
#endif
    }
	exit(0);
}

