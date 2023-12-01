/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *
 *  Heavy improvements by the DOSBox-X Team
 *  With major works from joncampbell123 and Wengier
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <cassert>
#if defined(MACOSX)
#define _DARWIN_C_SOURCE
#endif
#ifndef WIN32
#include <utime.h>
#include <sys/file.h>
#else
#include <fcntl.h>
#include <sys/utime.h>
#include <sys/locking.h>
#endif
#include <sys/stat.h>

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "logging.h"
#include "support.h"
#include "cross.h"
#include "inout.h"
#include "callback.h"
#include "regs.h"
#include "timer.h"
#include "render.h"
#include "jfont.h"

#if defined(EMSCRIPTEN) || defined(HAIKU)
#include <fcntl.h>
#endif

#include "cp437_uni.h"
#include "cp737_uni.h"
#include "cp775_uni.h"
#include "cp808_uni.h"
#include "cp850_uni.h"
#include "cp852_uni.h"
#include "cp853_uni.h"
#include "cp855_uni.h"
#include "cp856_uni.h"
#include "cp857_uni.h"
#include "cp858_uni.h"
#include "cp859_uni.h"
#include "cp860_uni.h"
#include "cp861_uni.h"
#include "cp862_uni.h"
#include "cp863_uni.h"
#include "cp864_uni.h"
#include "cp865_uni.h"
#include "cp866_uni.h"
#include "cp867_uni.h"
#include "cp868_uni.h"
#include "cp869_uni.h"
#include "cp872_uni.h"
#include "cp874_uni.h"
#include "cp932_uni.h"
#include "cp936_uni.h"
#include "cp949_uni.h"
#include "cp950_uni.h"
#include "cp951_uni.h"
#include "cp1250_uni.h"
#include "cp1251_uni.h"
#include "cp1252_uni.h"
#include "cp1253_uni.h"
#include "cp1254_uni.h"
#include "cp1255_uni.h"
#include "cp1256_uni.h"
#include "cp1257_uni.h"
#include "cp1258_uni.h"
#include "cp3021_uni.h"

#if defined (WIN32)
#include <Shellapi.h>
#else
#include <glob.h>
#endif

#if defined(PATH_MAX) && !defined(MAX_PATH)
#define MAX_PATH PATH_MAX
#endif

uint16_t ldid[256];
std::string ldir[256];
static host_cnv_char_t cpcnv_temp[4096];
static host_cnv_char_t cpcnv_ltemp[4096];
host_cnv_char_t *CodePageGuestToHost(const char *s);
extern bool isDBCSCP(), isKanji1(uint8_t chr), shiftjis_lead_byte(int c);
extern bool rsize, morelen, force_sfn, enable_share_exe, chinasea, uao, halfwidthkana, dbcs_sbcs, inmsg, forceswk;
extern int lfn_filefind_handle, freesizecap, file_access_tries;
extern unsigned long totalc, freec;
uint16_t customcp_to_unicode[256], altcp_to_unicode[256];
extern uint16_t cpMap_AX[32];
extern uint16_t cpMap_PC98[256];
extern std::map<int, int> lowboxdrawmap, pc98boxdrawmap;
int tryconvertcp = 0;
bool cpwarn_once = false, ignorespecial = false, notrycp = false;
std::string prefix_local = ".DBLOCALFILE";

char* GetCrossedName(const char *basedir, const char *dir) {
	static char crossname[CROSS_LEN];
	strcpy(crossname, basedir);
	strcat(crossname, dir);
	CROSS_FILENAME(crossname);
	return crossname;
}

bool String_ASCII_TO_HOST_UTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    const uint16_t* df = d + CROSS_LEN * (morelen?4:1) - 1;
	const char *sf = s + CROSS_LEN * (morelen?4:1) - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic < 32 || ic > 127) return false; // non-representable

        *d++ = (host_cnv_char_t)ic;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

bool String_ASCII_TO_HOST_UTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    const char* df = d + CROSS_LEN * (morelen?6:1) - 1;
	const char *sf = s + CROSS_LEN * (morelen?6:1) - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic < 32 || ic > 127) return false; // non-representable

        if (utf8_encode(&d,df,(uint32_t)ic) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_SBCS_TO_HOST_UTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const uint16_t* df = d + CROSS_LEN * (morelen?4:1) - 1;
	const char *sf = s + CROSS_LEN * (morelen?4:1) - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic >= map_max) return false; // non-representable
        MT wc;
#if defined(USE_TTF)
        if (dos.loaded_codepage==437&&forceswk) {
            if (ic>=0xA1&&ic<=0xDF) wc = cpMap_PC98[ic];
            else {
                std::map<int, int>::iterator it = lowboxdrawmap.find(ic);
                wc = map[it==lowboxdrawmap.end()?ic:it->second];
            }
        } else
#endif
            wc = map[ic]; // output: unicode character

        *d++ = (uint16_t)wc;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_SBCS_TO_HOST_UTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const char* df = d + CROSS_LEN * (morelen?6:1) - 1;
	const char *sf = s + CROSS_LEN * (morelen?6:1) - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic >= map_max) return false; // non-representable
        MT wc = map[ic]; // output: unicode character

        if (morelen&&ic==10&&wc==0x25D9) *d++ = ic;
        else if (utf8_encode(&d,df,(uint32_t)wc) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
    }

    assert(d <= df);
    *d = 0;

    return true;
}

uint16_t baselen = 0;
std::list<uint16_t> bdlist = {};
/* needed for Wengier's TTF output and CJK mode */
template <class MT> bool String_DBCS_TO_HOST_UTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const uint16_t* df = d + CROSS_LEN * (morelen?4:1) - 1;
	const char *sf = s + CROSS_LEN * (morelen?4:1) - 1;
    const char *ss = s;

    while (*s != 0 && s < sf) {
        if (morelen && !(dos.loaded_codepage == 932
#if defined(USE_TTF)
        && halfwidthkana
#endif
        ) && (std::find(bdlist.begin(), bdlist.end(), (uint16_t)(baselen + s - ss)) != bdlist.end() || (isKanji1(*s) && (!(*(s+1)) || !isKanji2(*(s+1)))))) {
            *d++ = cp437_to_unicode[(uint8_t)*s++];
            continue;
        } else if (morelen && IS_JEGA_ARCH && (uint8_t)(*s) && (uint8_t)(*s)<32) {
            *d++ = cpMap_AX[(uint8_t)*s++];
            continue;
        } else if (morelen && IS_PC98_ARCH && pc98boxdrawmap.find((uint8_t)*s) != pc98boxdrawmap.end()) {
            *d++ = cp437_to_unicode[(uint8_t)*s++];
            continue;
        } else if (morelen && dos.loaded_codepage == 932
#if defined(USE_TTF)
        && halfwidthkana
#endif
        && !IS_PC98_ARCH && !IS_JEGA_ARCH && lowboxdrawmap.find(*s)!=lowboxdrawmap.end()) {
            *d++ = cp437_to_unicode[(uint8_t)lowboxdrawmap.find(*s++)->second];
            continue;
        }
        uint16_t ic = (unsigned char)(*s++);
        if ((dos.loaded_codepage==932 &&((ic & 0xE0) == 0x80 || (ic & 0xE0) == 0xE0)) || ((dos.loaded_codepage==936 || dos.loaded_codepage==949 || dos.loaded_codepage==950 || dos.loaded_codepage==951) && (ic & 0x80) == 0x80)) {
            if (*s == 0) return false;
            ic <<= 8U;
            ic += (unsigned char)(*s++);
        }

        MT rawofs = hitbl[ic >> 6];
        if (rawofs == 0xFFFF)
            return false;

        assert((size_t)(rawofs+ (Bitu)0x40) <= rawtbl_max);
        MT wc = rawtbl[rawofs + (ic & 0x3F)];
        if (wc == 0x0000)
            return false;

        *d++ = (uint16_t)wc;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_DBCS_TO_HOST_UTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const char* df = d + CROSS_LEN * (morelen?6:1) - 1;
	const char *sf = s + CROSS_LEN * (morelen?6:1) - 1;
    const char *ss = s;

    while (*s != 0 && s < sf) {
        if (morelen && !(dos.loaded_codepage == 932
#if defined(USE_TTF)
        && halfwidthkana
#endif
        ) && (std::find(bdlist.begin(), bdlist.end(), (uint16_t)(baselen + s - ss)) != bdlist.end() || (isKanji1(*s) && (!(*(s+1)) || !isKanji2(*(s+1))))) && utf8_encode(&d,df,(uint32_t)cp437_to_unicode[(uint8_t)*s]) >= 0) {
            s++;
            continue;
        } else if (morelen && IS_JEGA_ARCH && (uint8_t)(*s) && (uint8_t)(*s)<32) {
            uint16_t oc = cpMap_AX[(uint8_t)*s];
            if (utf8_encode(&d,df,(uint32_t)oc) >= 0) {
                s++;
                continue;
            }
        } else if (morelen && IS_PC98_ARCH && pc98boxdrawmap.find((uint8_t)*s) != pc98boxdrawmap.end()) {
            uint16_t oc = cp437_to_unicode[(uint8_t)*s];
            if (utf8_encode(&d,df,(uint32_t)oc) >= 0) {
                s++;
                continue;
            }
        } else if (morelen && dos.loaded_codepage == 932
#if defined(USE_TTF)
        && halfwidthkana
#endif
        && !IS_PC98_ARCH && !IS_JEGA_ARCH && lowboxdrawmap.find(*s)!=lowboxdrawmap.end()) {
            uint16_t oc = cp437_to_unicode[(uint8_t)lowboxdrawmap.find(*s)->second];
            if (utf8_encode(&d,df,(uint32_t)oc) >= 0) {
                s++;
                continue;
            }
        }

        uint16_t ic = (unsigned char)(*s++);
        if ((dos.loaded_codepage==932 &&((ic & 0xE0) == 0x80 || (ic & 0xE0) == 0xE0)) || ((dos.loaded_codepage==936 || dos.loaded_codepage==949 || dos.loaded_codepage==950 || dos.loaded_codepage==951) && (ic & 0x80) == 0x80)) {
            if (*s == 0) return false;
            if (morelen && !(dos.loaded_codepage==932 && (IS_PC98_ARCH || IS_JEGA_ARCH
#if defined(USE_TTF)
                || halfwidthkana
#endif
                )) && (ic == 179 || ic == 186) && (s < sf && (*s == 32 || *s == 13))) {
                MT wc = cp437_to_unicode[ic];
                if (utf8_encode(&d,df,(uint32_t)wc) < 0) return false;
                continue;
            } else {
                ic <<= 8U;
                ic += (unsigned char)(*s++);
            }
        }

        MT rawofs = hitbl[ic >> 6];
        if (rawofs == 0xFFFF)
            return false;

        assert((size_t)(rawofs+ (Bitu)0x40) <= rawtbl_max);
        MT wc = rawtbl[rawofs + (ic & 0x3F)];
        if (wc == 0x0000)
            return false;

        if (utf8_encode(&d,df,(uint32_t)wc) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
    }

    assert(d <= df);
    *d = 0;

    return true;
}

// TODO: This is SLOW. Optimize.
template <class MT> int SBCS_From_Host_Find(int c,const MT *map,const size_t map_max) {
    if (morelen && (MT)c<0x20 && c >= 0 && c < 256 && map[c] == cp437_to_unicode[c]) return c;
    for (size_t i=0;i < map_max;i++) {
        if ((MT)c == map[i])
            return (int)i;
    }

    return -1;
}

// TODO: This is SLOW. Optimize.
template <class MT> int DBCS_From_Host_Find(int c,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    if ((MT)c>=0x20 && (MT)c<0x80 && !(altcp && dos.loaded_codepage == altcp) && !(customcp && dos.loaded_codepage == customcp)) return c;

    for (size_t h=0;h < 1024;h++) {
        MT ofs = hitbl[h];

        if (ofs == 0xFFFF) continue;
        assert((size_t)(ofs+ (Bitu)0x40) <= rawtbl_max);

        for (size_t l=0;l < 0x40;l++) {
            if ((MT)c == rawtbl[ofs+l])
                return (int)((h << 6) + l);
        }
    }

    return -1;
}

template <class MT> bool String_HOST_TO_DBCS_UTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const uint16_t *sf = s + CROSS_LEN * (morelen?4:1) - 1;
    const char* df = d + CROSS_LEN * (morelen?4:1) - 1;

    while (*s != 0 && s < sf) {
        int ic;
        ic = (int)(*s++);
#if defined(USE_TTF)
        if (morelen && !(dos.loaded_codepage == 932 && halfwidthkana) && ((!dbcs_sbcs && !inmsg) || (ic>=0x2550 && ic<=0x2569))) {
            *d++ = SBCS_From_Host_Find<MT>(ic,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            continue;
        } else
#endif
            if (morelen && dos.loaded_codepage == 932
#if defined(USE_TTF)
            && halfwidthkana
#endif
            && !IS_PC98_ARCH && !IS_JEGA_ARCH) {
            int wc = SBCS_From_Host_Find<MT>(ic,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            bool found = false;
            for (auto it = lowboxdrawmap.begin(); it != lowboxdrawmap.end(); ++it)
                if (it->second == wc) {
                    *d++ = it->first;
                    found = true;
                    break;
                }
            if (found) continue;
        } else if (morelen && IS_PC98_ARCH && ic > 0xFF) {
            int wc = SBCS_From_Host_Find<MT>(ic,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            auto it = pc98boxdrawmap.find(wc);
            if (it != pc98boxdrawmap.end()) {
                *d++ = (char)0x86;
                *d++ = it->second;
                continue;
            }
        } else if (morelen && IS_JEGA_ARCH) {
            bool found = false;
            for (uint8_t i=1; i<32; i++)
                if (cpMap_AX[i] == ic) {
                    *d++ = i;
                    found = true;
                    break;
                }
            if (found) continue;
        }

        int oc = DBCS_From_Host_Find<MT>(ic,hitbl,rawtbl,rawtbl_max);
        if (oc < 0)
            return false; // non-representable

        if (oc >= 0x100) {
            if ((d+1) >= df) return false;
            *d++ = (char)(oc >> 8U);
            *d++ = (char)oc;
        }
        else {
            if (d >= df) return false;
            *d++ = (char)oc;
        }
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_HOST_TO_DBCS_UTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const char *sf = s + CROSS_LEN * (morelen?6:1) - 1;
    const char* df = d + CROSS_LEN * (morelen?6:1) - 1;

    while (*s != 0 && s < sf) {
        int ic;
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable
#if defined(USE_TTF)
        if (morelen && !(dos.loaded_codepage == 932 && halfwidthkana) && ((!dbcs_sbcs && !inmsg) || (ic>=0x2550 && ic<=0x2569))) {
            *d++ = SBCS_From_Host_Find<MT>(ic,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            continue;
        } else
#endif
            if (morelen && dos.loaded_codepage == 932
#if defined(USE_TTF)
            && halfwidthkana
#endif
            && !IS_PC98_ARCH && !IS_JEGA_ARCH) {
            int wc = SBCS_From_Host_Find<MT>(ic,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            uint16_t j = 0;
            for (auto it = lowboxdrawmap.begin(); it != lowboxdrawmap.end(); ++it)
                if (it->second == wc) {
                    j = it->first;
                    break;
                }
            if (j && utf8_encode(&d,df,(uint32_t)j) >= 0) {
                s++;
                continue;
            }
        } else if (morelen && IS_PC98_ARCH && ic > 0xFF) {
            int wc = SBCS_From_Host_Find<MT>(ic,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            auto it = pc98boxdrawmap.find(wc);
            if (it != pc98boxdrawmap.end()) {
                *d++ = (char)0x86;
                *d++ = it->second;
                continue;
            }
        } else if (morelen && IS_JEGA_ARCH) {
            uint8_t j = 0;
            for (uint8_t i=1; i<32; i++)
                if (cpMap_AX[i] == ic) {
                    j = i;
                    break;
                }
            if (j && utf8_encode(&d,df,(uint32_t)j) >= 0) {
                s++;
                continue;
            }
        }
        int oc = DBCS_From_Host_Find<MT>(ic,hitbl,rawtbl,rawtbl_max);
        if (oc < 0)
            return false; // non-representable

        if (oc >= 0x100) {
            if ((d+1) >= df) return false;
            *d++ = (char)(oc >> 8U);
            *d++ = (char)oc;
        }
        else {
            if (d >= df) return false;
            *d++ = (char)oc;
        }
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_HOST_TO_SBCS_UTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const uint16_t *sf = s + CROSS_LEN * (morelen?4:1) - 1;
    const char* df = d + CROSS_LEN * (morelen?4:1) - 1;

    while (*s != 0 && s < sf) {
        int ic;
        ic = (int)(*s++);

        int oc = SBCS_From_Host_Find<MT>(ic,map,map_max);
        if (oc < 0)
            return false; // non-representable

        if (d >= df) return false;
        *d++ = (char)oc;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_HOST_TO_SBCS_UTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const char *sf = s + CROSS_LEN * (morelen?6:1) - 1;
    const char* df = d + CROSS_LEN * (morelen?6:1) - 1;

    while (*s != 0 && s < sf) {
        int ic;
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable

        int oc = SBCS_From_Host_Find<MT>(ic,map,map_max);
        if (oc < 0)
            return false; // non-representable

        if (d >= df) return false;
        *d++ = (char)oc;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

bool String_HOST_TO_ASCII_UTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/) {
    const uint16_t *sf = s + CROSS_LEN * (morelen?4:1) - 1;
    const char* df = d + CROSS_LEN * (morelen?4:1) - 1;

    while (*s != 0 && s < sf) {
        int ic;
        ic = (int)(*s++);

        if (ic < 32 || ic > 127)
            return false; // non-representable

        if (d >= df) return false;
        *d++ = (char)ic;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

bool String_HOST_TO_ASCII_UTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    const char *sf = s + CROSS_LEN * (morelen?6:1) - 1;
    const char* df = d + CROSS_LEN * (morelen?6:1) - 1;

    while (*s != 0 && s < sf) {
        int ic;
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable

        if (ic < 32 || ic > 127)
            return false; // non-representable

        if (d >= df) return false;
        *d++ = (char)ic;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

bool isemptyhit(uint16_t code) {
    switch (dos.loaded_codepage) {
        case 932:
            return cp932_to_unicode_hitbl[code >> 6] == 0xffff;
        case 936:
            return cp936_to_unicode_hitbl[code >> 6] == 0xffff;
        case 949:
            return cp949_to_unicode_hitbl[code >> 6] == 0xffff;
        case 950:
            return chinasea ? cp950ext_to_unicode_hitbl[code >> 6] == 0xffff : cp950_to_unicode_hitbl[code >> 6] == 0xffff;
        case 951:
            return cp951_to_unicode_hitbl[code >> 6] == 0xffff;
        default:
            return code > 0xff;
    }
}

bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/) {
    if (altcp && dos.loaded_codepage == altcp) return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,altcp_to_unicode,sizeof(altcp_to_unicode)/sizeof(altcp_to_unicode[0]));
    if (customcp && dos.loaded_codepage == customcp) return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,customcp_to_unicode,sizeof(customcp_to_unicode)/sizeof(customcp_to_unicode[0]));
    switch (dos.loaded_codepage) {
        case 437:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 737:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp737_to_unicode,sizeof(cp737_to_unicode)/sizeof(cp737_to_unicode[0]));
        case 775:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp775_to_unicode,sizeof(cp775_to_unicode)/sizeof(cp775_to_unicode[0]));
        case 808:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 856:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp856_to_unicode,sizeof(cp856_to_unicode)/sizeof(cp856_to_unicode[0]));
        case 857:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 859:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp859_to_unicode,sizeof(cp859_to_unicode)/sizeof(cp859_to_unicode[0]));
        case 860:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 867:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp867_to_unicode,sizeof(cp867_to_unicode)/sizeof(cp867_to_unicode[0]));
        case 868:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp868_to_unicode,sizeof(cp868_to_unicode)/sizeof(cp868_to_unicode[0]));
        case 869:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        case 936:
            return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp936_to_unicode_hitbl,cp936_to_unicode_raw,sizeof(cp936_to_unicode_raw)/sizeof(cp936_to_unicode_raw[0]));
        case 949:
            return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp949_to_unicode_hitbl,cp949_to_unicode_raw,sizeof(cp949_to_unicode_raw)/sizeof(cp949_to_unicode_raw[0]));
        case 950:
            if (chinasea) return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp950ext_to_unicode_hitbl,cp950ext_to_unicode_raw,sizeof(cp950ext_to_unicode_raw)/sizeof(cp950ext_to_unicode_raw[0]));
            return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp950_to_unicode_hitbl,cp950_to_unicode_raw,sizeof(cp950_to_unicode_raw)/sizeof(cp950_to_unicode_raw[0]));
        case 951:
            if (chinasea && uao) return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uaosea_to_unicode_raw,sizeof(cp951uaosea_to_unicode_raw)/sizeof(cp951uaosea_to_unicode_raw[0]));
            if (chinasea && !uao) return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951sea_to_unicode_raw,sizeof(cp951sea_to_unicode_raw)/sizeof(cp951sea_to_unicode_raw[0]));
            if (!chinasea && uao) return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uao_to_unicode_raw,sizeof(cp951uao_to_unicode_raw)/sizeof(cp951uao_to_unicode_raw[0]));
            return String_HOST_TO_DBCS_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951_to_unicode_raw,sizeof(cp951_to_unicode_raw)/sizeof(cp951_to_unicode_raw[0]));
        case 1250:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1250_to_unicode,sizeof(cp1250_to_unicode)/sizeof(cp1250_to_unicode[0]));
        case 1251:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1251_to_unicode,sizeof(cp1251_to_unicode)/sizeof(cp1251_to_unicode[0]));
        case 1252:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1252_to_unicode,sizeof(cp1252_to_unicode)/sizeof(cp1252_to_unicode[0]));
        case 1253:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1253_to_unicode,sizeof(cp1253_to_unicode)/sizeof(cp1253_to_unicode[0]));
        case 1254:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1254_to_unicode,sizeof(cp1254_to_unicode)/sizeof(cp1254_to_unicode[0]));
        case 1255:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1255_to_unicode,sizeof(cp1255_to_unicode)/sizeof(cp1255_to_unicode[0]));
        case 1256:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1256_to_unicode,sizeof(cp1256_to_unicode)/sizeof(cp1256_to_unicode[0]));
        case 1257:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1257_to_unicode,sizeof(cp1257_to_unicode)/sizeof(cp1257_to_unicode[0]));
        case 1258:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp1258_to_unicode,sizeof(cp1258_to_unicode)/sizeof(cp1258_to_unicode[0]));
        case 3021:
            return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp3021_to_unicode,sizeof(cp3021_to_unicode)/sizeof(cp3021_to_unicode[0]));
        default: // Otherwise just use code page 437 or ASCII
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to guest) for code page %u",dos.loaded_codepage);
            }
            if (dos.loaded_codepage>=800)
                return String_HOST_TO_SBCS_UTF16<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            else
                return String_HOST_TO_ASCII_UTF16(d,s);
    }
}

bool CodePageHostToGuestUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    if (altcp && dos.loaded_codepage == altcp) return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,altcp_to_unicode,sizeof(altcp_to_unicode)/sizeof(altcp_to_unicode[0]));
    if (customcp && dos.loaded_codepage == customcp) return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,customcp_to_unicode,sizeof(customcp_to_unicode)/sizeof(customcp_to_unicode[0]));
    switch (dos.loaded_codepage) {
        case 437:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 737:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp737_to_unicode,sizeof(cp737_to_unicode)/sizeof(cp737_to_unicode[0]));
        case 775:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp775_to_unicode,sizeof(cp775_to_unicode)/sizeof(cp775_to_unicode[0]));
        case 808:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 856:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp856_to_unicode,sizeof(cp856_to_unicode)/sizeof(cp856_to_unicode[0]));
        case 857:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 859:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp859_to_unicode,sizeof(cp859_to_unicode)/sizeof(cp859_to_unicode[0]));
        case 860:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 867:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp867_to_unicode,sizeof(cp867_to_unicode)/sizeof(cp867_to_unicode[0]));
        case 868:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp868_to_unicode,sizeof(cp868_to_unicode)/sizeof(cp868_to_unicode[0]));
        case 869:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        case 936:
            return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp936_to_unicode_hitbl,cp936_to_unicode_raw,sizeof(cp936_to_unicode_raw)/sizeof(cp936_to_unicode_raw[0]));
        case 949:
            return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp949_to_unicode_hitbl,cp949_to_unicode_raw,sizeof(cp949_to_unicode_raw)/sizeof(cp949_to_unicode_raw[0]));
        case 950:
            if (chinasea) return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp950ext_to_unicode_hitbl,cp950ext_to_unicode_raw,sizeof(cp950ext_to_unicode_raw)/sizeof(cp950ext_to_unicode_raw[0]));
            return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp950_to_unicode_hitbl,cp950_to_unicode_raw,sizeof(cp950_to_unicode_raw)/sizeof(cp950_to_unicode_raw[0]));
        case 951:
            if (chinasea && uao) return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uaosea_to_unicode_raw,sizeof(cp951uaosea_to_unicode_raw)/sizeof(cp951uaosea_to_unicode_raw[0]));
            if (chinasea && !uao) return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951sea_to_unicode_raw,sizeof(cp951sea_to_unicode_raw)/sizeof(cp951sea_to_unicode_raw[0]));
            if (!chinasea && uao) return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uao_to_unicode_raw,sizeof(cp951uao_to_unicode_raw)/sizeof(cp951uao_to_unicode_raw[0]));
            return String_HOST_TO_DBCS_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951_to_unicode_raw,sizeof(cp951_to_unicode_raw)/sizeof(cp951_to_unicode_raw[0]));
        case 1250:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1250_to_unicode,sizeof(cp1250_to_unicode)/sizeof(cp1250_to_unicode[0]));
        case 1251:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1251_to_unicode,sizeof(cp1251_to_unicode)/sizeof(cp1251_to_unicode[0]));
        case 1252:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1252_to_unicode,sizeof(cp1252_to_unicode)/sizeof(cp1252_to_unicode[0]));
        case 1253:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1253_to_unicode,sizeof(cp1253_to_unicode)/sizeof(cp1253_to_unicode[0]));
        case 1254:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1254_to_unicode,sizeof(cp1254_to_unicode)/sizeof(cp1254_to_unicode[0]));
        case 1255:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1255_to_unicode,sizeof(cp1255_to_unicode)/sizeof(cp1255_to_unicode[0]));
        case 1256:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1256_to_unicode,sizeof(cp1256_to_unicode)/sizeof(cp1256_to_unicode[0]));
        case 1257:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1257_to_unicode,sizeof(cp1257_to_unicode)/sizeof(cp1257_to_unicode[0]));
        case 1258:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp1258_to_unicode,sizeof(cp1258_to_unicode)/sizeof(cp1258_to_unicode[0]));
        case 3021:
            return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp3021_to_unicode,sizeof(cp3021_to_unicode)/sizeof(cp3021_to_unicode[0]));
        default: // Otherwise just use code page 437 or ASCII
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to guest) for code page %u",dos.loaded_codepage);
            }
            if (dos.loaded_codepage>=800)
                return String_HOST_TO_SBCS_UTF8<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            else
                return String_HOST_TO_ASCII_UTF8(d,s);
    }
}

bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    if (altcp && dos.loaded_codepage == altcp) return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,altcp_to_unicode,sizeof(altcp_to_unicode)/sizeof(altcp_to_unicode[0]));
    if (customcp && dos.loaded_codepage == customcp) return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,customcp_to_unicode,sizeof(customcp_to_unicode)/sizeof(customcp_to_unicode[0]));
    switch (dos.loaded_codepage) {
        case 437:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 737:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp737_to_unicode,sizeof(cp737_to_unicode)/sizeof(cp737_to_unicode[0]));
        case 775:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp775_to_unicode,sizeof(cp775_to_unicode)/sizeof(cp775_to_unicode[0]));
        case 808:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 856:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp856_to_unicode,sizeof(cp856_to_unicode)/sizeof(cp856_to_unicode[0]));
        case 857:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 859:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp859_to_unicode,sizeof(cp859_to_unicode)/sizeof(cp859_to_unicode[0]));
        case 860:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 867:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp867_to_unicode,sizeof(cp867_to_unicode)/sizeof(cp867_to_unicode[0]));
        case 868:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp868_to_unicode,sizeof(cp868_to_unicode)/sizeof(cp868_to_unicode[0]));
        case 869:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        case 936:
            return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp936_to_unicode_hitbl,cp936_to_unicode_raw,sizeof(cp936_to_unicode_raw)/sizeof(cp936_to_unicode_raw[0]));
        case 949:
            return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp949_to_unicode_hitbl,cp949_to_unicode_raw,sizeof(cp949_to_unicode_raw)/sizeof(cp949_to_unicode_raw[0]));
        case 950:
            if (chinasea) return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp950ext_to_unicode_hitbl,cp950ext_to_unicode_raw,sizeof(cp950ext_to_unicode_raw)/sizeof(cp950ext_to_unicode_raw[0]));
            return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp950_to_unicode_hitbl,cp950_to_unicode_raw,sizeof(cp950_to_unicode_raw)/sizeof(cp950_to_unicode_raw[0]));
        case 951:
            if (chinasea && uao) return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uaosea_to_unicode_raw,sizeof(cp951uaosea_to_unicode_raw)/sizeof(cp951uaosea_to_unicode_raw[0]));
            if (chinasea && !uao) return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951sea_to_unicode_raw,sizeof(cp951sea_to_unicode_raw)/sizeof(cp951sea_to_unicode_raw[0]));
            if (!chinasea && uao) return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uao_to_unicode_raw,sizeof(cp951uao_to_unicode_raw)/sizeof(cp951uao_to_unicode_raw[0]));
            return String_DBCS_TO_HOST_UTF16<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951_to_unicode_raw,sizeof(cp951_to_unicode_raw)/sizeof(cp951_to_unicode_raw[0]));
        case 1250:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1250_to_unicode,sizeof(cp1250_to_unicode)/sizeof(cp1250_to_unicode[0]));
        case 1251:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1251_to_unicode,sizeof(cp1251_to_unicode)/sizeof(cp1251_to_unicode[0]));
        case 1252:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1252_to_unicode,sizeof(cp1252_to_unicode)/sizeof(cp1252_to_unicode[0]));
        case 1253:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1253_to_unicode,sizeof(cp1253_to_unicode)/sizeof(cp1253_to_unicode[0]));
        case 1254:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1254_to_unicode,sizeof(cp1254_to_unicode)/sizeof(cp1254_to_unicode[0]));
        case 1255:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1255_to_unicode,sizeof(cp1255_to_unicode)/sizeof(cp1255_to_unicode[0]));
        case 1256:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1256_to_unicode,sizeof(cp1256_to_unicode)/sizeof(cp1256_to_unicode[0]));
        case 1257:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1257_to_unicode,sizeof(cp1257_to_unicode)/sizeof(cp1257_to_unicode[0]));
        case 1258:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp1258_to_unicode,sizeof(cp1258_to_unicode)/sizeof(cp1258_to_unicode[0]));
        case 3021:
            return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp3021_to_unicode,sizeof(cp3021_to_unicode)/sizeof(cp3021_to_unicode[0]));
        default: // Otherwise just use code page 437 or ASCII
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to host) for code page %u",dos.loaded_codepage);
            }
            if (dos.loaded_codepage>=800)
                return String_SBCS_TO_HOST_UTF16<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            else
                return String_ASCII_TO_HOST_UTF16(d,s);
    }
}

bool CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    if (altcp && dos.loaded_codepage == altcp) return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,altcp_to_unicode,sizeof(altcp_to_unicode)/sizeof(altcp_to_unicode[0]));
    if (customcp && dos.loaded_codepage == customcp) return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,customcp_to_unicode,sizeof(customcp_to_unicode)/sizeof(customcp_to_unicode[0]));
    switch (dos.loaded_codepage) {
        case 437:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 737:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp737_to_unicode,sizeof(cp737_to_unicode)/sizeof(cp737_to_unicode[0]));
        case 775:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp775_to_unicode,sizeof(cp775_to_unicode)/sizeof(cp775_to_unicode[0]));
        case 808:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 856:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp856_to_unicode,sizeof(cp856_to_unicode)/sizeof(cp856_to_unicode[0]));
        case 857:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 859:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp859_to_unicode,sizeof(cp859_to_unicode)/sizeof(cp859_to_unicode[0]));
        case 860:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 867:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp867_to_unicode,sizeof(cp867_to_unicode)/sizeof(cp867_to_unicode[0]));
        case 868:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp868_to_unicode,sizeof(cp868_to_unicode)/sizeof(cp868_to_unicode[0]));
        case 869:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        case 936:
            return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp936_to_unicode_hitbl,cp936_to_unicode_raw,sizeof(cp936_to_unicode_raw)/sizeof(cp936_to_unicode_raw[0]));
        case 949:
            return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp949_to_unicode_hitbl,cp949_to_unicode_raw,sizeof(cp949_to_unicode_raw)/sizeof(cp949_to_unicode_raw[0]));
        case 950:
            if (chinasea) return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp950ext_to_unicode_hitbl,cp950ext_to_unicode_raw,sizeof(cp950ext_to_unicode_raw)/sizeof(cp950ext_to_unicode_raw[0]));
            return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp950_to_unicode_hitbl,cp950_to_unicode_raw,sizeof(cp950_to_unicode_raw)/sizeof(cp950_to_unicode_raw[0]));
        case 951:
            if (chinasea && uao) return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uaosea_to_unicode_raw,sizeof(cp951uaosea_to_unicode_raw)/sizeof(cp951uaosea_to_unicode_raw[0]));
            if (chinasea && !uao) return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951sea_to_unicode_raw,sizeof(cp951sea_to_unicode_raw)/sizeof(cp951sea_to_unicode_raw[0]));
            if (!chinasea && uao) return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951uao_to_unicode_raw,sizeof(cp951uao_to_unicode_raw)/sizeof(cp951uao_to_unicode_raw[0]));
            return String_DBCS_TO_HOST_UTF8<uint16_t>(d,s,cp951_to_unicode_hitbl,cp951_to_unicode_raw,sizeof(cp951_to_unicode_raw)/sizeof(cp951_to_unicode_raw[0]));
        case 1250:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1250_to_unicode,sizeof(cp1250_to_unicode)/sizeof(cp1250_to_unicode[0]));
        case 1251:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1251_to_unicode,sizeof(cp1251_to_unicode)/sizeof(cp1251_to_unicode[0]));
        case 1252:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1252_to_unicode,sizeof(cp1252_to_unicode)/sizeof(cp1252_to_unicode[0]));
        case 1253:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1253_to_unicode,sizeof(cp1253_to_unicode)/sizeof(cp1253_to_unicode[0]));
        case 1254:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1254_to_unicode,sizeof(cp1254_to_unicode)/sizeof(cp1254_to_unicode[0]));
        case 1255:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1255_to_unicode,sizeof(cp1255_to_unicode)/sizeof(cp1255_to_unicode[0]));
        case 1256:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1256_to_unicode,sizeof(cp1256_to_unicode)/sizeof(cp1256_to_unicode[0]));
        case 1257:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1257_to_unicode,sizeof(cp1257_to_unicode)/sizeof(cp1257_to_unicode[0]));
        case 1258:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp1258_to_unicode,sizeof(cp1258_to_unicode)/sizeof(cp1258_to_unicode[0]));
        case 3021:
            return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp3021_to_unicode,sizeof(cp3021_to_unicode)/sizeof(cp3021_to_unicode[0]));
        default: // Otherwise just use code page 437 or ASCII
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to host) for code page %u",dos.loaded_codepage);
            }
            if (dos.loaded_codepage>=800)
                return String_SBCS_TO_HOST_UTF8<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
            else
                return String_ASCII_TO_HOST_UTF8(d,s);
    }
}

host_cnv_char_t *CodePageGuestToHost(const char *s) {
#if defined(host_cnv_use_wchar)
    uint16_t cp = GetACP(), cpbak = dos.loaded_codepage;
    if (tryconvertcp && !notrycp && cpbak == 437 && (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951)) {
        dos.loaded_codepage = cp;
        if (CodePageGuestToHostUTF16((uint16_t *)cpcnv_temp,s)) {
            dos.loaded_codepage = cpbak;
            return cpcnv_temp;
        } else
            dos.loaded_codepage = cpbak;
    }
    if (!CodePageGuestToHostUTF16((uint16_t *)cpcnv_temp,s))
#else
    if (!CodePageGuestToHostUTF8((char *)cpcnv_temp,s))
#endif
        return NULL;

    return cpcnv_temp;
}

char *CodePageHostToGuest(const host_cnv_char_t *s) {
#if defined(host_cnv_use_wchar)
    uint16_t cp = GetACP(), cpbak = dos.loaded_codepage;
    if (tryconvertcp && !notrycp && cpbak == 437 && (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951)) {
        dos.loaded_codepage = cp;
        if (CodePageHostToGuestUTF16((char *)cpcnv_temp,(const uint16_t *)s)) {
            dos.loaded_codepage = cpbak;
            return (char *)cpcnv_temp;
        } else
            dos.loaded_codepage = cpbak;
    }
    if (!CodePageHostToGuestUTF16((char *)cpcnv_temp,(const uint16_t *)s))
#else
    if (!CodePageHostToGuestUTF8((char *)cpcnv_temp,(char *)s))
#endif
        return NULL;

    return (char*)cpcnv_temp;
}

char *CodePageHostToGuestL(const host_cnv_char_t *s) {
#if defined(host_cnv_use_wchar)
    uint16_t cp = GetACP(), cpbak = dos.loaded_codepage;
    if (tryconvertcp && !notrycp && cpbak == 437 && (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951)) {
        dos.loaded_codepage = cp;
        if (CodePageHostToGuestUTF16((char *)cpcnv_ltemp,(const uint16_t *)s)) {
            dos.loaded_codepage = cpbak;
            return (char *)cpcnv_ltemp;
        } else
            dos.loaded_codepage = cpbak;
    }
    if (!CodePageHostToGuestUTF16((char *)cpcnv_ltemp,(const uint16_t *)s))
#else
    if (!CodePageHostToGuestUTF8((char *)cpcnv_ltemp,(char *)s))
#endif
        return NULL;

    return (char*)cpcnv_ltemp;
}

int FileDirExistCP(const char *name) {
    struct stat st;
    if (stat(name, &st)) return 0;
    if ((st.st_mode & S_IFREG) == S_IFREG) return 1;
    else if (st.st_mode & S_IFDIR) return 2;
    return 0;
}

int FileDirExistUTF8(std::string &localname, const char *name) {
    int cp = dos.loaded_codepage;
#ifdef WIN32
    dos.loaded_codepage = GetACP();
#else
    return 0;
#endif
    if (dos.loaded_codepage == 950 && !chinasea) makestdcp950table();
    if (dos.loaded_codepage == 951 && chinasea) makeseacp951table();
    if (!CodePageHostToGuestUTF8((char *)cpcnv_temp, (char *)name)) {
        dos.loaded_codepage = cp;
        return 0;
    }
    dos.loaded_codepage = cp;
    localname = (char *)cpcnv_temp;
    return FileDirExistCP(localname.c_str());
}

extern uint16_t fztime, fzdate;
extern bool force_conversion, InitCodePage();
std::string GetDOSBoxXPath(bool withexe=false);
void getdrivezpath(std::string &path, std::string const& dirname) {
    const host_cnv_char_t* host_name = CodePageGuestToHost(path.c_str());
    if (host_name == NULL) {path = "";return;}
    struct stat cstat;
    ht_stat_t hstat;
    int res=host_name == NULL?stat(path.c_str(),&cstat):ht_stat(host_name,&hstat);
    bool ret=res==-1?false:((host_name == NULL?cstat.st_mode:hstat.st_mode) & S_IFDIR);
    if (!ret) {
        path = GetDOSBoxXPath();
        if (path.size()) {
            path += dirname;
            host_name = CodePageGuestToHost(path.c_str());
            res=host_name == NULL?stat(path.c_str(),&cstat):ht_stat(host_name,&hstat);
            ret=res==-1?false:((host_name == NULL?cstat.st_mode:hstat.st_mode) & S_IFDIR);
        }
        if (!path.size() || !ret) {
            path = "";
            Cross::GetPlatformConfigDir(path);
            path += dirname;
            host_name = CodePageGuestToHost(path.c_str());
            res=host_name == NULL?stat(path.c_str(),&cstat):ht_stat(host_name,&hstat);
            ret=res==-1?false:((host_name == NULL?cstat.st_mode:hstat.st_mode) & S_IFDIR);
            if (!ret) {
                path = "";
                Cross::GetPlatformResDir(path);
                path += dirname;
                host_name = CodePageGuestToHost(path.c_str());
                res=host_name == NULL?stat(path.c_str(),&cstat):ht_stat(host_name,&hstat);
                ret=res==-1?false:((host_name == NULL?cstat.st_mode:hstat.st_mode) & S_IFDIR);
                if (!ret)
                    path = "";
            }
        }
    }
}

void drivezRegister(std::string const& path, std::string const& dir, bool usecp) {
    int cp = dos.loaded_codepage;
    if (!usecp || !cp) {
        force_conversion = true;
        InitCodePage();
        force_conversion = false;
    }
    char exePath[CROSS_LEN];
    std::vector<std::string> names;
    if (path.size()) {
#if defined(WIN32)
        HANDLE hFind;
        WIN32_FIND_DATA fd;
        WIN32_FIND_DATAW fdw;
        host_cnv_char_t *host_name = CodePageGuestToHost((path+"\\*.*").c_str());
        if (host_name != NULL) hFind = FindFirstFileW(host_name, &fdw);
        else hFind = FindFirstFile((path+"\\*.*").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                const char* temp_name = NULL;
                if (host_name != NULL) temp_name = CodePageHostToGuest(fdw.cFileName);
                if (!((host_name != NULL ? fdw.dwFileAttributes : fd.dwFileAttributes) & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (host_name == NULL)
                        names.emplace_back(fd.cFileName);
                    else if (temp_name != NULL)
                        names.emplace_back(temp_name);
                } else if ((host_name == NULL || temp_name != NULL) && strcmp(host_name != NULL ? temp_name : fd.cFileName, ".") && strcmp(host_name != NULL ? temp_name : fd.cFileName, ".."))
                    names.push_back(std::string(host_name == NULL ? fd.cFileName : temp_name)+"/");
            } while(host_name != NULL ? FindNextFileW(hFind, &fdw) : FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
#else
        struct dirent *dir;
        host_cnv_char_t *host_name = CodePageGuestToHost(path.c_str());
        DIR *d = opendir(host_name != NULL ? host_name : path.c_str());
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                host_cnv_char_t *temp_name = CodePageHostToGuest(dir->d_name);
#if defined(HAIKU)
                struct stat path_stat;
                stat(dir->d_name, &path_stat);
                bool is_regular_file = S_ISREG(path_stat.st_mode);
                bool is_directory = S_ISDIR(path_stat.st_mode);
#else
                bool is_regular_file = (dir->d_type == DT_REG);
                bool is_directory = (dir->d_type == DT_DIR);
#endif
                if (is_regular_file)
                    names.push_back(temp_name!=NULL?temp_name:dir->d_name);
                else if (is_directory && strcmp(temp_name != NULL ? temp_name : dir->d_name, ".") && strcmp(temp_name != NULL ? temp_name : dir->d_name, ".."))
                    names.push_back(std::string(temp_name != NULL ? temp_name : dir->d_name) + "/");
            }
            closedir(d);
        }
#endif
    }
    int res;
    long f_size;
    uint8_t *f_data;
    struct stat temp_stat;
    const struct tm* ltime;
    const host_cnv_char_t* host_name;
    for (std::string name: names) {
        if (!name.size()) continue;
        if ((!strcasecmp(name.c_str(), "AUTOEXEC.BAT") || !strcasecmp(name.c_str(), "CONFIG.SYS")) && dir=="/") continue;
        if (name.back()=='/' && dir=="/") {
            ht_stat_t temp_stat;
            host_name = CodePageGuestToHost((path+CROSS_FILESPLIT+name).c_str());
            res = host_name == NULL ? 1 : ht_stat(host_name,&temp_stat);
            if (res) {
                host_name = CodePageGuestToHost((GetDOSBoxXPath()+path+CROSS_FILESPLIT+name).c_str());
                res = ht_stat(host_name,&temp_stat);
            }
            if (res==0&&(ltime=
#if defined(__MINGW32__) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
            _localtime64
#else
            localtime
#endif
            (&temp_stat.st_mtime))!=0) {
                fztime=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
                fzdate=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
            }
            VFILE_Register(name.substr(0, name.size()-1).c_str(), 0, 0, dir.c_str());
            fztime = fzdate = 0;
            drivezRegister(path+CROSS_FILESPLIT+name.substr(0, name.size()-1), dir+name, usecp);
            continue;
        }
        FILE * f = NULL;
        host_cnv_char_t *host_name = CodePageGuestToHost((path+CROSS_FILESPLIT+name).c_str());
        if (host_name != NULL) {
#ifdef host_cnv_use_wchar
            f = _wfopen(host_name, L"rb");
#else
            f = fopen(host_name, "rb");
#endif
        }
        if (f == NULL) {
            strcpy(exePath, GetDOSBoxXPath().c_str());
            strcat(exePath, (path+CROSS_FILESPLIT+name).c_str());
            host_name = CodePageGuestToHost(exePath);
            if (host_name != NULL) {
#ifdef host_cnv_use_wchar
                f = _wfopen(host_name, L"rb");
#else
                f = fopen(host_name, "rb");
#endif
            }
        }
        f_size = 0;
        f_data = NULL;
        if (f != NULL) {
            res=fstat(fileno(f),&temp_stat);
            if (res==0&&(ltime=localtime(&temp_stat.st_mtime))!=0) {
                fztime=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
                fzdate=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
            }
            fseek(f, 0, SEEK_END);
            f_size=ftell(f);
            f_data=(uint8_t*)malloc(f_size);
            fseek(f, 0, SEEK_SET);
            if (f_data) fread(f_data, sizeof(char), f_size, f);
            fclose(f);
        }
        if (f_data) VFILE_Register(name.c_str(), f_data, f_size, dir=="/"?"":dir.c_str());
        fztime = fzdate = 0;
    }
    dos.loaded_codepage = cp;
}

bool localDrive::FileCreate(DOS_File * * file,const char * name,uint16_t attributes) {
    if (nocachedir) EmptyCache();

    if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

    if (attributes & DOS_ATTR_VOLUME) {
        // Real MS-DOS 6.22 and earlier behavior says that creating a volume label
        // without first deleting the existing volume label does nothing but add
        // yet another volume label entry to the root directory and the reported
        // volume label does not change. MS-DOS 7.0 and later appear to have fixed
        // this considering the re-use of bits [3:0] == 0xF to carry LFN entries.
        //
        // Emulate this behavior by setting the volume label ONLY IF there is no
        // volume label. If the DOS application knows how to do it properly it will
        // first issue an FCB delete with attr = DOS_ATTR_VOLUME and ????????.???
        // OR (more common in MS-DOS 6.22 and later) an FCB delete with
        // attr = DOS_ATTR_VOLUME and an explicit copy of the volume label obtained
        // from FCB FindFirst.
        //
        // Volume label handling always affects the root directory.
        //
        // This function is called with file == NULL for volume labels.
        if (*GetLabel() == 0)
            SetLabel(name,false,true);

        return true;
    }

    assert(file != NULL);

//TODO Maybe care for attributes but not likely
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
    const char* temp_name = dirCache.GetExpandName(newname); // Can only be used until a new drive_cache action is performed
	/* Test if file exists (so we need to truncate it). don't add to dirCache then */
	bool existing_file=false;

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(temp_name);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
		DOS_SetError(DOSERR_FILE_NOT_FOUND); // FIXME
        return false;
    }

#ifdef host_cnv_use_wchar
    FILE * test=_wfopen(host_name,L"rb+");
#else
    FILE * test=fopen(host_name,"rb+");
#endif
	if(test) {
		fclose(test);
		existing_file=true;
	}

    FILE * hand;
    if (enable_share_exe && !existing_file) {
#if defined(WIN32)
        int attribs = FILE_ATTRIBUTE_NORMAL;
        if (attributes&3) attribs = attributes&3;
        HANDLE handle = CreateFileW(host_name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, attribs, NULL);
        if (handle == INVALID_HANDLE_VALUE) return false;
        int nHandle = _open_osfhandle((intptr_t)handle, _O_RDONLY);
        if (nHandle == -1) {CloseHandle(handle);return false;}
        hand = _wfdopen(nHandle, L"wb+");
#else
        int fd = open(host_name,
#if defined(O_DIRECT)
            (file_access_tries>0 ? O_DIRECT : 0) |
#endif
            O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd<0) {return false;}
#if defined(F_NOCACHE)
        if (file_access_tries>0) fcntl(fd, F_NOCACHE, 1);
#endif
        hand = fdopen(fd, "wb+");
#endif
    } else {
#ifdef host_cnv_use_wchar
        hand=_wfopen(host_name,L"wb+");
#else
        hand=fopen(host_name,"wb+");
#endif
    }
	if (!hand){
		LOG_MSG("Warning: file creation failed: %s",newname);
		return false;
	}

	if(!existing_file) {
		strcpy(newname,basedir);
		strcat(newname,name);
		CROSS_FILENAME(newname);
		dirCache.AddEntry(newname, true);
	}

	/* Make the 16 bit device information */
	*file=new localFile(name,hand);
	(*file)->flags=OPEN_READWRITE;

	return true;
}

#ifndef WIN32
int lock_file_region(int fd, int cmd, struct flock *fl, long long start, unsigned long len)
{
  fl->l_whence = SEEK_SET;
  fl->l_pid = 0;
  //if (start == 0x100000000LL) start = len = 0; //first handle magic file lock value
#ifdef F_SETLK64
  if (cmd == F_SETLK64 || cmd == F_GETLK64) {
    struct flock64 fl64;
    int result;
    LOG(LOG_DOSMISC,LOG_DEBUG)("Large file locking start=%llx, len=%lx\n", start, len);
    fl64.l_type = fl->l_type;
    fl64.l_whence = fl->l_whence;
    fl64.l_pid = fl->l_pid;
    fl64.l_start = start;
    fl64.l_len = len;
    result = fcntl( fd, cmd, &fl64 );
    fl->l_type = fl64.l_type;
    fl->l_start = (long) fl64.l_start;
    fl->l_len = (long) fl64.l_len;
    return result;
  }
#endif
  if (start == 0x100000000LL)
    start = 0x7fffffff;
  fl->l_start = start;
  fl->l_len = len;
  return fcntl( fd, cmd, fl );
}

#define COMPAT_MODE	0x00
#define DENY_ALL	0x01
#define DENY_WRITE	0x02
#define DENY_READ	0x03
#define DENY_NONE	0x04
#define FCB_MODE	0x07
bool share(int fd, int mode, uint32_t flags) {
  struct flock fl;
  int ret;
  int share_mode = ( flags >> 4 ) & 0x7;
  fl.l_type = F_WRLCK;
  /* see whatever locks are possible */

#ifdef F_GETLK64
  ret = lock_file_region( fd, F_GETLK64, &fl, 0x100000000LL, 1 );
  if ( ret == -1 && errno == EINVAL )
#endif
  ret = lock_file_region( fd, F_GETLK, &fl, 0x100000000LL, 1 );
  if ( ret == -1 ) return true;

  /* file is already locked? then do not even open */
  /* a Unix read lock prevents writing;
     a Unix write lock prevents reading and writing,
     but for DOS compatibility we allow reading for write locks */
  if ((fl.l_type == F_RDLCK && mode != O_RDONLY) || (fl.l_type == F_WRLCK && mode != O_WRONLY))
    return false;

  switch ( share_mode ) {
  case COMPAT_MODE:
    if (fl.l_type == F_WRLCK) return false;
  case DENY_NONE:
    return true;                   /* do not set locks at all */
  case DENY_WRITE:
    if (fl.l_type == F_WRLCK) return false;
    if (mode == O_WRONLY) return true; /* only apply read locks */
    fl.l_type = F_RDLCK;
    break;
  case DENY_READ:
    if (fl.l_type == F_RDLCK) return false;
    if (mode == O_RDONLY) return true; /* only apply write locks */
    fl.l_type = F_WRLCK;
    break;
  case DENY_ALL:
    if (fl.l_type == F_WRLCK || fl.l_type == F_RDLCK) return false;
    fl.l_type = mode == O_RDONLY ? F_RDLCK : F_WRLCK;
    break;
  case FCB_MODE:
    if ((flags & 0x8000) && (fl.l_type != F_WRLCK)) return true;
    /* else fall through */
  default:
    LOG(LOG_DOSMISC,LOG_WARN)("internal SHARE: unknown sharing mode %x\n", share_mode);
    return false;
    break;
  }
#ifdef F_SETLK64
  ret = lock_file_region(fd, F_SETLK64, &fl, 0x100000000LL, 1);
  if(ret == -1 && errno == EINVAL)
#endif
      lock_file_region(fd, F_SETLK, &fl, 0x100000000LL, 1);
  LOG(LOG_DOSMISC, LOG_DEBUG)("internal SHARE: locking: fd %d, type %d whence %d pid %d\n", fd, fl.l_type, fl.l_whence, fl.l_pid);

  return true;
}
#endif

bool localDrive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
    if (nocachedir) EmptyCache();

    if (readonly) {
        if ((flags&0xf) == OPEN_WRITE || (flags&0xf) == OPEN_READWRITE) {
            DOS_SetError(DOSERR_WRITE_PROTECTED);
            return false;
        }
    }

	const host_cnv_char_t * type;
	switch (flags&0xf) {
	case OPEN_READ:        type = _HT("rb");  break;
	case OPEN_WRITE:       type = _HT("rb+"); break;
	case OPEN_READWRITE:   type = _HT("rb+"); break;
	case OPEN_READ_NO_MOD: type = _HT("rb");  break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	//Flush the buffer of handles for the same file. (Betrayal in Antara)
	uint8_t i,drive=DOS_DRIVES;
	localFile *lfp;
	for (i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]==this) {
			drive=i;
			break;
		}
	}
    if(!dos_kernel_disabled)
        for(i = 0; i < DOS_FILES; i++) {
            if(Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive() == drive && Files[i]->IsName(name)) {
                lfp = dynamic_cast<localFile*>(Files[i]);
                if(lfp) lfp->Flush();
            }
        }

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
        return false;
    }

    FILE * hand;
    if (enable_share_exe) {
#if defined(WIN32)
        int ohFlag = (flags&0xf)==OPEN_READ||(flags&0xf)==OPEN_READ_NO_MOD?GENERIC_READ:((flags&0xf)==OPEN_WRITE?GENERIC_WRITE:GENERIC_READ|GENERIC_WRITE);
        int shhFlag = (flags&0x70)==0x10?0:((flags&0x70)==0x20?FILE_SHARE_READ:((flags&0x70)==0x30?FILE_SHARE_WRITE:FILE_SHARE_READ|FILE_SHARE_WRITE));
        HANDLE handle = CreateFileW(host_name, ohFlag, shhFlag, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE) return false;
        int nHandle = _open_osfhandle((intptr_t)handle, _O_RDONLY);
        if (nHandle == -1) {CloseHandle(handle);return false;}
        hand = _wfdopen(nHandle, (flags&0xf)==OPEN_WRITE?_HT("wb"):type);
#else
        uint16_t unix_mode = (flags&0xf)==OPEN_READ||(flags&0xf)==OPEN_READ_NO_MOD?O_RDONLY:((flags&0xf)==OPEN_WRITE?O_WRONLY:O_RDWR);
        int fd = open(host_name, unix_mode);
        if (fd<0 || !share(fd, unix_mode & O_ACCMODE, flags)) {if (fd >= 0) close(fd);return false;}
        hand = fdopen(fd, (flags&0xf)==OPEN_WRITE?_HT("wb"):type);
#endif
    } else {
#ifdef host_cnv_use_wchar
		hand=_wfopen(host_name,type);
#else
		hand=fopen(host_name,type);
#endif
    }
//	uint32_t err=errno;
	if (!hand) {
		if((flags&0xf) != OPEN_READ) {
#ifdef host_cnv_use_wchar
			FILE * hmm=_wfopen(host_name,L"rb");
#else
			FILE * hmm=fopen(host_name,"rb");
#endif
			if (hmm) {
				fclose(hmm);
#ifdef host_cnv_use_wchar
				LOG_MSG("Warning: file %ls exists and failed to open in write mode.\nPlease Remove write-protection",host_name);
#else
				LOG_MSG("Warning: file %s exists and failed to open in write mode.\nPlease Remove write-protection",host_name);
#endif
			}
		}
		return false;
	}

	*file=new localFile(name,hand);
	(*file)->flags=flags;  //for the inheritance flag and maybe check for others.
//	(*file)->SetFileName(host_name);
	return true;
}

FILE * localDrive::GetSystemFilePtr(char const * const name, char const * const type) {

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return NULL;
    }

#ifdef host_cnv_use_wchar
    wchar_t wtype[8];
    unsigned int tis;

    // "type" always has ANSI chars (like "rb"), nothing fancy
    for (tis=0;tis < 7 && type[tis] != 0;tis++) wtype[tis] = (wchar_t)type[tis];
    assert(tis <= 7); // guard
    wtype[tis] = 0;

	return _wfopen(host_name,wtype);
#else
	return fopen(host_name,type);
#endif
}

bool localDrive::GetSystemFilename(char *sysName, char const * const dosName) {

	strcpy(sysName, basedir);
	strcat(sysName, dosName);
	CROSS_FILENAME(sysName);
	dirCache.ExpandName(sysName);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(sysName);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,sysName);
        return false;
    }

#ifdef host_cnv_use_wchar
    // FIXME: GetSystemFilename as implemented cannot return the wide char filename
    return false;
#else
    strcpy(sysName,host_name);
    return true;
#endif
}

bool localDrive::FileUnlink(const char * name) {
    if (readonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	char *fullname = dirCache.GetExpandName(newname);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(fullname);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,fullname);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
        return false;
    }

	if (ht_unlink(host_name)) {
		//Unlink failed for some reason try finding it.
		ht_stat_t buffer;
		if(ht_stat(host_name,&buffer)) return false; // File not found.

#ifdef host_cnv_use_wchar
		FILE* file_writable = _wfopen(host_name,L"rb+");
#else
		FILE* file_writable = fopen(host_name,"rb+");
#endif
		if(!file_writable) return false; //No access ? ERROR MESSAGE NOT SET. FIXME ?
		fclose(file_writable);

		//File exists and can technically be deleted, nevertheless it failed.
		//This means that the file is probably open by some process.
		//See if We have it open.
		bool found_file = false;
		for(Bitu i = 0;i < DOS_FILES;i++){
			if(Files[i] && Files[i]->IsName(name)) {
				Bitu max = DOS_FILES;
				while(Files[i]->IsOpen() && max--) {
					Files[i]->Close();
					if (Files[i]->RemoveRef()<=0) break;
				}
				found_file=true;
			}
		}
		if(!found_file) return false;
		if (!ht_unlink(host_name)) {
#if !defined (WIN32)
			remove_special_file_from_disk(name, "ATR");
#endif
			dirCache.DeleteEntry(newname);
			return true;
		}
		return false;
	} else {
#if !defined (WIN32)
		remove_special_file_from_disk(name, "ATR");
#endif
		dirCache.DeleteEntry(newname);
		return true;
	}
}

bool localDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	char tempDir[CROSS_LEN];
	strcpy(tempDir,basedir);
	strcat(tempDir,_dir);
	CROSS_FILENAME(tempDir);

	size_t len = strlen(tempDir);
	bool lead = false;
	for (unsigned int i=0;i<len;i++) {
		if(lead) lead = false;
		else if((IS_PC98_ARCH || isDBCSCP()) && isKanji1(tempDir[i])) lead = true;
		else tempDir[i]=toupper(tempDir[i]);
	}
    if (nocachedir) EmptyCache();

	if (allocation.mediaid==0xF0 ) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}

	if (!check_last_split_char(tempDir, len, CROSS_FILESPLIT)) {
		char end[2]={CROSS_FILESPLIT,0};
		strcat(tempDir,end);
	}

	uint16_t id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) {
		dta.SetDirID(id);
		strcpy(srchInfo[id].srch_dir,tempDir);
	} else {
		ldid[lfn_filefind_handle]=id;
		ldir[lfn_filefind_handle]=tempDir;
	}

	uint8_t sAttr;
	dta.GetSearchParams(sAttr,tempDir,false);

	if (this->isRemote() && this->isRemovable()) {
		// cdroms behave a bit different than regular drives
		if (sAttr == DOS_ATTR_VOLUME) {
			dta.SetResult(dirCache.GetLabel(),dirCache.GetLabel(),0,0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	} else {
		if (sAttr == DOS_ATTR_VOLUME) {
			if ( strcmp(dirCache.GetLabel(), "") == 0 ) {
//				LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
//				dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
//				return true;
				DOS_SetError(DOSERR_NO_MORE_FILES);
				return false;
			}
            dta.SetResult(dirCache.GetLabel(),dirCache.GetLabel(),0,0,0,0,DOS_ATTR_VOLUME);
			return true;
		} else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst) { 
		//should check for a valid leading directory instead of 0
		//exists==true if the volume label matches the searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
                dta.SetResult(dirCache.GetLabel(),dirCache.GetLabel(),0,0,0,0,DOS_ATTR_VOLUME);
				return true;
			}
		}
	}
	return FindNext(dta);
}

char * DBCS_upcase(char * str);

bool localDrive::FindNext(DOS_DTA & dta) {

    char * dir_ent, *ldir_ent;
	ht_stat_t stat_block;
    char full_name[CROSS_LEN], lfull_name[LFN_NAMELENGTH+1];
    char dir_entcopy[CROSS_LEN], ldir_entcopy[CROSS_LEN];

    uint8_t srch_attr;char srch_pattern[LFN_NAMELENGTH+1];
	uint8_t find_attr;

    dta.GetSearchParams(srch_attr,srch_pattern,false);
	uint16_t id = lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():ldid[lfn_filefind_handle];

again:
    if (!dirCache.FindNext(id,dir_ent,ldir_ent)) {
		if (lfn_filefind_handle<LFN_FILEFIND_MAX) {
			ldid[lfn_filefind_handle]=0;
			ldir[lfn_filefind_handle]="";
		}
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
    if(!WildFileCmp(dir_ent,srch_pattern)&&!LWildFileCmp(ldir_ent,srch_pattern)) goto again;

	strcpy(full_name,lfn_filefind_handle>=LFN_FILEFIND_MAX?srchInfo[id].srch_dir:(ldir[lfn_filefind_handle]!=""?ldir[lfn_filefind_handle].c_str():"\\"));
	strcpy(lfull_name,full_name);

	strcat(full_name,dir_ent);
    strcat(lfull_name,ldir_ent);

	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory 
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	strcpy(dir_entcopy,dir_ent);
    strcpy(ldir_entcopy,ldir_ent);

    char *temp_name = dirCache.GetExpandName(full_name);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(temp_name);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,temp_name);
		goto again;//No symlinks and such
    }

	if (ht_stat(host_name,&stat_block)!=0)
		goto again;//No symlinks and such

	if(stat_block.st_mode & S_IFDIR) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=0;
#if defined (WIN32)
	Bitu attribs = GetFileAttributesW(host_name);
	if (attribs != INVALID_FILE_ATTRIBUTES)
		find_attr|=attribs&0x3f;
#else
	bool isdir = find_attr & DOS_ATTR_DIRECTORY;
	if (!isdir) find_attr|=DOS_ATTR_ARCHIVE;
	if(!(stat_block.st_mode & S_IWUSR)) find_attr|=DOS_ATTR_READ_ONLY;
    std::string fname = create_filename_of_special_operation(temp_name, "ATR", false);
    if (ht_stat(fname.c_str(),&stat_block)==0) {
        unsigned int len = stat_block.st_size;
        if (len & 1) {
            if (isdir)
                find_attr|=DOS_ATTR_ARCHIVE;
            else
                find_attr&=~DOS_ATTR_ARCHIVE;
        }
        if (len & 2) find_attr|=DOS_ATTR_HIDDEN;
        if (len & 4) find_attr|=DOS_ATTR_SYSTEM;
    }
#endif
 	if (~srch_attr & find_attr & DOS_ATTR_DIRECTORY) goto again;

	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII], lfind_name[LFN_NAMELENGTH+1];
    uint16_t find_date,find_time;uint32_t find_size,find_hsize;

	if(strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_entcopy);
        if (IS_PC98_ARCH || isDBCSCP())
            DBCS_upcase(find_name);
        else
            upcase(find_name);
    }
	strcpy(lfind_name,ldir_entcopy);
    lfind_name[LFN_NAMELENGTH]=0;

	find_hsize=(uint32_t) (stat_block.st_size / 0x100000000);
	find_size=(uint32_t) (stat_block.st_size % 0x100000000);
    const struct tm* time;
	if((time=
#if defined(__MINGW32__) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    _localtime64
#else
    localtime
#endif
    (&stat_block.st_mtime))!=0){
		find_date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
		find_time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,lfind_name,find_size,find_hsize,find_date,find_time,find_attr);
	return true;
}

void localDrive::remove_special_file_from_disk(const char* dosname, const char* operation) {
#if !defined (WIN32)
	std::string newname = create_filename_of_special_operation(dosname, operation, true);
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname.c_str());
	if (host_name != NULL)
		ht_unlink(host_name);
	else
		unlink(newname.c_str());
#else
    (void)dosname;
    (void)operation;
#endif
}

std::string localDrive::create_filename_of_special_operation(const char* dosname, const char* operation, bool expand) {
	std::string res(expand?dirCache.GetExpandName(GetCrossedName(basedir, dosname)):dosname);
	std::string::size_type s = std::string::npos; //CHECK DOS or host endings.... on update_cache
	bool lead = false;
	for (unsigned int i=0; i<res.size(); i++) {
		if (lead) lead = false;
		else if ((IS_PC98_ARCH && shiftjis_lead_byte(res[i])) || (isDBCSCP() && isKanji1(res[i]))) lead = true;
		else if (res[i]=='\\'||res[i]==CROSS_FILESPLIT) s = i;
	}
	if (s == std::string::npos) s = 0; else s++;
	std::string oper = special_prefix_local + "_" + operation + "_";
	res.insert(s,oper);
	return res;
}

bool localDrive::add_special_file_to_disk(const char* dosname, const char* operation, uint16_t value, bool isdir) {
#if !defined (WIN32)
	std::string newname = create_filename_of_special_operation(dosname, operation, true);
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname.c_str());
	FILE* f = fopen_wrap(host_name!=NULL?host_name:newname.c_str(),"wb+");
	if (!f) return false;
	size_t len = 0;
	if (isdir != !(value & DOS_ATTR_ARCHIVE)) len |= 1;
	if (value & DOS_ATTR_HIDDEN) len |= 2;
	if (value & DOS_ATTR_SYSTEM) len |= 4;
	char *buf = new char[len + 1];
	memset(buf,0,len);
	fwrite(buf,len,1,f);
	fclose(f);
	delete[] buf;
	return true;
#else
    (void)dosname;
    (void)operation;
    (void)value;
    (void)isdir;
	return false;
#endif
}

bool localDrive::SetFileAttr(const char * name,uint16_t attr) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	// guest to host code page translation
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
	if (host_name == NULL) {
		LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

#if defined (WIN32)
	if (!SetFileAttributesW(host_name, attr))
		{
		DOS_SetError((uint16_t)GetLastError());
		return false;
		}
	dirCache.EmptyCache();
	return true;
#else
	ht_stat_t status;
	if (ht_stat(host_name,&status)==0) {
		if (attr & DOS_ATTR_READ_ONLY)
			status.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
		else
			status.st_mode |=  S_IWUSR;
		if (chmod(host_name,status.st_mode) < 0) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		if (!(attr & (DOS_ATTR_SYSTEM|DOS_ATTR_HIDDEN)) && (status.st_mode & S_IFDIR) == !(attr & DOS_ATTR_ARCHIVE))
			remove_special_file_from_disk(name, "ATR");
		else if (!add_special_file_to_disk(name, "ATR", attr, status.st_mode & S_IFDIR))
			LOG(LOG_DOSMISC,LOG_WARN)("%s: Application attempted to set system or hidden attributes for '%s' which is ignored for local drives",__FUNCTION__,newname);

		return true;
	}

	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
#endif
}

bool localDrive::GetFileAttr(const char * name,uint16_t * attr) {
    if (nocachedir) EmptyCache();

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
        return false;
    }

#if defined (WIN32)
	Bitu attribs = GetFileAttributesW(host_name);
	if (attribs == INVALID_FILE_ATTRIBUTES)
		{
		DOS_SetError((uint16_t)GetLastError());
		return false;
		}
	*attr = attribs&0x3f;
	return true;
#else
	ht_stat_t status;
	if (ht_stat(host_name,&status)==0) {
        bool isdir = status.st_mode & S_IFDIR;
		*attr=isdir?0:DOS_ATTR_ARCHIVE;
		if(isdir) *attr|=DOS_ATTR_DIRECTORY;
		if(!(status.st_mode & S_IWUSR)) *attr|=DOS_ATTR_READ_ONLY;
		std::string fname = create_filename_of_special_operation(name, "ATR", true);
        if (ht_stat(fname.c_str(),&status)==0) {
            unsigned int len = status.st_size;
            if (len & 1) {
                if (isdir)
                    *attr|=DOS_ATTR_ARCHIVE;
                else
                    *attr&=~DOS_ATTR_ARCHIVE;
            }
            if (len & 2) *attr|=DOS_ATTR_HIDDEN;
            if (len & 4) *attr|=DOS_ATTR_SYSTEM;
        }
		return true;
	}
	*attr=0;
	return false; 
#endif
}

bool localDrive::GetFileAttrEx(char* name, struct stat *status) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	return !stat(newname,status);
}

std::string localDrive::GetHostName(const char * name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
	ht_stat_t temp_stat;
	static std::string hostname; hostname = host_name != NULL && ht_stat(host_name,&temp_stat)==0 ? newname : "";
	return hostname;
}

unsigned long localDrive::GetCompressedSize(char* name) {
    (void)name;
#if !defined (WIN32)
	return 0;
#else
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	DWORD size = GetCompressedFileSize(newname, NULL);
	if (size != INVALID_FILE_SIZE) {
		if (size != 0 && size == GetFileSize(newname, NULL)) {
			DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
			if (GetDiskFreeSpace(newname, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters)) {
				size = ((size - 1) | (sectors_per_cluster * bytes_per_sector - 1)) + 1;
			}
		}
		return size;
	} else {
		DOS_SetError((uint16_t)GetLastError());
		return -1;
	}
#endif
}

#if defined (WIN32)
HANDLE localDrive::CreateOpenFile(const char* name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	HANDLE handle=CreateFile(newname, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle==INVALID_HANDLE_VALUE)
		DOS_SetError((uint16_t)GetLastError());
	return handle;
}

unsigned long localDrive::GetSerial() {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	if (strlen(newname)>2&&newname[1]==':') {
		unsigned long serial_number=0x1234;
		char volume[] = "A:\\";
		volume[0]=newname[0];
		GetVolumeInformation(volume, NULL, 0, &serial_number, NULL, NULL, NULL, 0);
		return serial_number;
	}

	return 0x1234;
}
#endif

bool localDrive::MakeDir(const char * dir) {
    if (nocachedir) EmptyCache();

    if (readonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);

    const char* temp_name = dirCache.GetExpandName(newdir);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(temp_name);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newdir);
		DOS_SetError(DOSERR_FILE_NOT_FOUND); // FIXME
        return false;
    }

#if defined (WIN32)						/* MS Visual C++ */
	int temp=_wmkdir(host_name);
#else
	int temp=mkdir(host_name,0775);
#endif
	if (temp==0) dirCache.CacheOut(newdir,true);
	return (temp==0);// || ((temp!=0) && (errno==EEXIST));
}

bool localDrive::RemoveDir(const char * dir) {
    if (nocachedir) EmptyCache();

    if (readonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);

    const char* temp_name = dirCache.GetExpandName(newdir);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(temp_name);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newdir);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
        return false;
    }

#if defined (WIN32)						/* MS Visual C++ */
	int temp=_wrmdir(host_name);
#else
	int temp=rmdir(host_name);
#endif
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
}

bool localDrive::TestDir(const char * dir) {
    if (nocachedir) EmptyCache();

	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(newdir);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newdir);
        return false;
    }

	std::string temp_line=std::string(newdir);
	if(temp_line.size() > 4 && temp_line[0]=='\\' && temp_line[1]=='\\' && temp_line[2]!='\\' && std::count(temp_line.begin()+3, temp_line.end(), '\\')==1) strcat(newdir,"\\");

	// Skip directory test, if "\"
	size_t len = strlen(newdir);
	if (len && !check_last_split_char(newdir, len, '\\')) {
		// It has to be a directory !
		ht_stat_t test;
		if (ht_stat(host_name,&test))		return false;
		if ((test.st_mode & S_IFDIR)==0)	return false;
	}
	int temp=ht_access(host_name,F_OK);
	return (temp==0);
}

bool localDrive::Rename(const char * oldname,const char * newname) {
    if (readonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

    const host_cnv_char_t* ht;

	char newold[CROSS_LEN];
	strcpy(newold,basedir);
	strcat(newold,oldname);
	CROSS_FILENAME(newold);
    struct stat temp_stat;
    if(stat(newold,&temp_stat)) dirCache.ExpandName(newold);

	char newnew[CROSS_LEN];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);
    dirCache.ExpandName(newnew);

    // guest to host code page translation
    ht = CodePageGuestToHost(newold);
    if (ht == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newold);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
        return false;
    }
    host_cnv_char_t *o_temp_name = ht_strdup(ht);

    // guest to host code page translation
    ht = CodePageGuestToHost(newnew);
    if (ht == NULL) {
        free(o_temp_name);
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newnew);
		DOS_SetError(DOSERR_FILE_NOT_FOUND); // FIXME
        return false;
    }
    host_cnv_char_t *n_temp_name = ht_strdup(ht);

#ifdef host_cnv_use_wchar
	int temp=_wrename(o_temp_name,n_temp_name);
#else
	int temp=rename(o_temp_name,n_temp_name);
#endif

	if (temp==0) dirCache.CacheOut(newnew);

    free(o_temp_name);
    free(n_temp_name);

	return (temp==0);

}

#if !defined(WIN32)
#include <sys/statvfs.h>
#endif
bool localDrive::AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) {
	*_bytes_sector=allocation.bytes_sector;
	*_sectors_cluster=allocation.sectors_cluster;
	*_total_clusters=allocation.total_clusters;
	*_free_clusters=allocation.free_clusters;
	if ((!allocation.total_clusters && !allocation.free_clusters) || freesizecap) {
		bool res=false;
#if defined(WIN32)
		long unsigned int dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
		uint8_t drive=strlen(basedir)>1&&basedir[1]==':'?toupper(basedir[0])-'A'+1:0;
		if (drive>26) drive=0;
		char root[4]="A:\\";
		root[0]='A'+drive-1;
        if (basedir[0]=='\\' && basedir[1]=='\\')
            res = GetDiskFreeSpace(basedir, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
        else
            res = GetDiskFreeSpace(drive?root:NULL, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
		if (res) {
			unsigned long total = dwTotalClusters * dwSectPerClust;
			int ratio = total > 2097120 ? 64 : (total > 1048560 ? 32 : (total > 524280 ? 16 : (total > 262140 ? 8 : (total > 131070 ? 4 : (total > 65535 ? 2 : 1))))), ratio2 = ratio * dwBytesPerSect / 512;
			*_bytes_sector = 512;
			*_sectors_cluster = ratio;
			*_total_clusters = total > 4194240? 65535 : (uint16_t)(dwTotalClusters * dwSectPerClust / ratio2);
			*_free_clusters = dwFreeClusters ? (total > 4194240? 61440 : (uint16_t)(dwFreeClusters * dwSectPerClust / ratio2)) : 0;
			if (rsize) {
				totalc=dwTotalClusters * dwSectPerClust / ratio;
				freec=dwFreeClusters * dwSectPerClust / ratio;
			}
#else
		struct statvfs stat;
		res = statvfs(basedir, &stat) == 0;
		if (res) {
			int ratio = stat.f_blocks / 65536, tmp=ratio;
			*_bytes_sector = 512;
			*_sectors_cluster = stat.f_frsize/512 > 64? 64 : stat.f_frsize/512;
			if (ratio>1) {
				if (ratio * (*_sectors_cluster) > 64) tmp = (*_sectors_cluster+63)/(*_sectors_cluster);
				*_sectors_cluster = ratio * (*_sectors_cluster) > 64? 64 : ratio * (*_sectors_cluster);
				ratio = tmp;
			}
			*_total_clusters = stat.f_blocks > 65535? 65535 : stat.f_blocks;
			*_free_clusters = stat.f_bavail > 61440? 61440 : stat.f_bavail;
			if (rsize) {
				totalc=stat.f_blocks;
				freec=stat.f_bavail;
				if (ratio>1) {
					totalc/=ratio;
					freec/=ratio;
				}
			}
#endif
			if ((allocation.total_clusters || allocation.free_clusters) && freesizecap<3) {
                long diff = 0;
                if (freesizecap==2) diff = (freec?freec:*_free_clusters) - allocation.initfree;
				bool g1=*_bytes_sector * *_sectors_cluster * *_total_clusters > allocation.bytes_sector * allocation.sectors_cluster * allocation.total_clusters;
				bool g2=*_bytes_sector * *_sectors_cluster * *_free_clusters > allocation.bytes_sector * allocation.sectors_cluster * allocation.free_clusters;
				if (g1||g2) {
                    if (freesizecap==2) diff *= (*_bytes_sector * *_sectors_cluster) / (allocation.bytes_sector * allocation.sectors_cluster);
                    *_bytes_sector = allocation.bytes_sector;
                    *_sectors_cluster = allocation.sectors_cluster;
					if (g1) *_total_clusters = allocation.total_clusters;
					if (g2) *_free_clusters = allocation.free_clusters;
					if (freesizecap==2) {
                        if (diff<0&&(-diff)>*_free_clusters)
                            *_free_clusters=0;
                        else
                            *_free_clusters += (uint16_t)diff;
                    }
					if (*_total_clusters<*_free_clusters) {
						if (*_free_clusters>65525)
							*_total_clusters=65535;
						else
							*_total_clusters=*_free_clusters+10;
					}
					if (rsize) {
						if (g1) totalc=*_total_clusters;
						if (g2) freec=*_free_clusters;
					}
				}
			}
		} else if (!allocation.total_clusters && !allocation.free_clusters) {
			if (allocation.mediaid==0xF0) {
				*_bytes_sector = 512;
				*_sectors_cluster = 1;
				*_total_clusters = 2880;
				*_free_clusters = 2880;
            } else if (allocation.bytes_sector==2048) {
				*_bytes_sector = 2048;
				*_sectors_cluster = 1;
				*_total_clusters = 65535;
				*_free_clusters = 0;
			} else {
                // 512*32*32765==~500MB total size
                // 512*32*16000==~250MB total free size
				*_bytes_sector = 512;
				*_sectors_cluster = 32;
				*_total_clusters = 32765;
				*_free_clusters = 16000;
			}
		}
	}
	return true;
}

bool localDrive::FileExists(const char* name) {
    if (nocachedir) EmptyCache();

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }

	ht_stat_t temp_stat;
	if(ht_stat(host_name,&temp_stat)!=0) return false;
	if(temp_stat.st_mode & S_IFDIR) return false;
	return true;
}

bool localDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
    if (nocachedir) EmptyCache();

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }

	ht_stat_t temp_stat;
	if(ht_stat(host_name,&temp_stat)!=0) return false;

	/* Convert the stat to a FileStat */
    const struct tm* time;
	if((time=
#if defined(__MINGW32__) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    _localtime64
#else
    localtime
#endif
    (&temp_stat.st_mtime))!=0) {
		stat_block->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		stat_block->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	}
	stat_block->size=(uint32_t)temp_stat.st_size;
	return true;
}

uint8_t localDrive::GetMediaByte(void) {
	return allocation.mediaid;
}

bool localDrive::isRemote(void) {
	if (remote==1) return true;
	if (remote==0) return false;
	char psp_name[9];
	DOS_MCB psp_mcb(dos.psp()-1);
	psp_mcb.GetFileName(psp_name);
	if (!strcmp(psp_name, "SCANDISK") || !strcmp(psp_name, "CHKDSK")) {
		/* Check for SCANDISK.EXE (or CHKDSK.EXE) and return true (Wengier) */
		return true;
	}
	/* Automatically detect if called by SCANDISK.EXE even if it is renamed (tested with the program from MS-DOS 6.20 to Windows ME) */
	if (dos.version.major >= 5 && reg_sp >=0x4000 && mem_readw(SegPhys(ss)+reg_sp)/0x100 == 0x1 && mem_readw(SegPhys(ss)+reg_sp+2)/0x100 >= 0xB && mem_readw(SegPhys(ss)+reg_sp+2)/0x100 <= 0x12)
		return true;
	return false;
}

bool localDrive::isRemovable(void) {
	return false;
}

Bits localDrive::UnMount(void) { 
	delete this;
	return 0; 
}

/* helper functions for drive cache */
void *localDrive::opendir(const char *name) {
    // guest to host code page translation
    const host_cnv_char_t* host_name = CodePageGuestToHost(name);
    if (host_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,name);
        return NULL;
    }

	return open_directoryw(host_name);
}

void localDrive::closedir(void *handle) {
	close_directory((dir_information*)handle);
}

bool localDrive::read_directory_first(void *handle, char* entry_name, char* entry_sname, bool& is_directory) {
    host_cnv_char_t tmp[MAX_PATH+1], stmp[MAX_PATH+1];

    ignorespecial = true;
    if (::read_directory_firstw((dir_information*)handle, tmp, stmp, is_directory)) {
        ignorespecial = false;
        // guest to host code page translation
        const char* n_stemp_name = CodePageHostToGuest(stmp);
        if (n_stemp_name == NULL) {
#ifdef host_cnv_use_wchar
            LOG_MSG("%s: Filename '%ls' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,stmp);
#else
            LOG_MSG("%s: Filename '%s' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,stmp);
#endif
            return false;
        }
		{
			const char* n_temp_name = CodePageHostToGuestL(tmp);
			if (n_temp_name == NULL) {
#ifdef host_cnv_use_wchar
				LOG_MSG("%s: Filename '%ls' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,tmp);
#else
				LOG_MSG("%s: Filename '%s' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,tmp);
#endif
				strcpy(entry_name,n_stemp_name);
			} else {
				strcpy(entry_name,n_temp_name);
			}
		}
		strcpy(entry_sname,n_stemp_name);
		return true;
    }
    ignorespecial = false;

    return false;
}

bool localDrive::read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory) {
    host_cnv_char_t tmp[MAX_PATH+1], stmp[MAX_PATH+1];

next:
    ignorespecial = true;
    if (::read_directory_nextw((dir_information*)handle, tmp, stmp, is_directory)) {
        ignorespecial = false;
        // guest to host code page translation
        const char* n_stemp_name = CodePageHostToGuest(stmp);
        if (n_stemp_name == NULL) {
#ifdef host_cnv_use_wchar
            LOG_MSG("%s: Filename '%ls' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,stmp);
#else
            LOG_MSG("%s: Filename '%s' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,stmp);
#endif
            goto next;
        }
		{
			const char* n_temp_name = CodePageHostToGuestL(tmp);
			if (n_temp_name == NULL) {
#ifdef host_cnv_use_wchar
				LOG_MSG("%s: Filename '%ls' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,tmp);
#else
				LOG_MSG("%s: Filename '%s' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,tmp);
#endif
				strcpy(entry_name,n_stemp_name);
			} else {
				strcpy(entry_name,n_temp_name);
			}
		}
        strcpy(entry_sname,n_stemp_name);
        return true;
    }
    ignorespecial = false;

    return false;
}

localDrive::localDrive(const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, std::vector<std::string> &options) : special_prefix_local(prefix_local.c_str()) {
	strcpy(basedir,startdir);
	sprintf(info,"local directory %s",startdir);
	allocation.bytes_sector=_bytes_sector;
	allocation.sectors_cluster=_sectors_cluster;
	allocation.total_clusters=_total_clusters;
	allocation.free_clusters=_free_clusters;
	allocation.mediaid=_mediaid;
	allocation.initfree=0;
    if (freesizecap==2) {
        uint16_t bytes_per_sector,total_clusters,free_clusters;
        uint8_t sectors_per_cluster;
        freesizecap=3;
        rsize=true;
        freec=0;
        if (AllocationInfo(&bytes_per_sector,&sectors_per_cluster,&total_clusters,&free_clusters))
            allocation.initfree = freec?freec:free_clusters;
        rsize=false;
        freesizecap=2;
    }

	for (const auto &opt : options) {
		size_t equ = opt.find_first_of('=');
		std::string name,value;

		if (equ != std::string::npos) {
			name = opt.substr(0,equ);
			value = opt.substr(equ+1);
		}
		else {
			name = opt;
			value.clear();
		}

		for (char & c: name) c=tolower(c);
		if (name == "remote")
			remote = 1;
		else if (name == "local")
			remote = 0;
	}

	dirCache.SetBaseDir(basedir,this);
}


//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(uint8_t * data,uint16_t * size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {	// check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
#if defined(WIN32)
    if (file_access_tries>0) {
        HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
        uint32_t bytesRead;
        for (int tries = file_access_tries; tries; tries--) {							// Try specified number of times
            if (ReadFile(hFile,static_cast<LPVOID>(data), (uint32_t)*size, (LPDWORD)&bytesRead, NULL)) {
                *size = (uint16_t)bytesRead;
                return true;
            }
            Sleep(25);																	// If failed (should be region locked), wait 25 millisecs
        }
        DOS_SetError((uint16_t)GetLastError());
        *size = 0;
        return false;
    }
#endif
	if (last_action==WRITE) {
		if (file_access_tries>0) {
			off_t pos = lseek(fileno(fhandle),0,SEEK_CUR);
			if (pos>-1) lseek(fileno(fhandle),pos,SEEK_SET);
		} else
			fseek(fhandle,ftell(fhandle),SEEK_SET);
        if (!newtime) UpdateLocalDateTime();
    }
	last_action=READ;
	*size=file_access_tries>0?(uint16_t)read(fileno(fhandle),data,*size):(uint16_t)fread(data,1,*size,fhandle);
	/* Fake harddrive motion. Inspector Gadget with soundblaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as unmasking is realitively heavy to emulate */
    if (!IS_PC98_ARCH) {
        uint8_t mask = IO_Read(0x21);
        if(mask & 0x4 ) IO_Write(0x21,mask&0xfb);
    }

	return true;
}

bool localFile::Write(const uint8_t * data,uint16_t * size) {
	uint32_t lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
#if defined(WIN32)
    if (file_access_tries>0) {
        HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
        if (*size == 0) {
            if (SetEndOfFile(hFile)) {
                UpdateLocalDateTime();
                return true;
            } else {
                DOS_SetError((uint16_t)GetLastError());
                return false;
            }
        }
        uint32_t bytesWritten;
        for (int tries = file_access_tries; tries; tries--) {							// Try three times
            if (WriteFile(hFile, data, (uint32_t)*size, (LPDWORD)&bytesWritten, NULL)) {
                *size = (uint16_t)bytesWritten;
                UpdateLocalDateTime();
                return true;
            }
            Sleep(25);																	// If failed (should be region locked? (not documented in MSDN)), wait 25 millisecs
        }
        DOS_SetError((uint16_t)GetLastError());
        *size = 0;
        return false;
    }
#endif
	if (last_action==READ) {
		if (file_access_tries>0) {
			off_t pos = lseek(fileno(fhandle),0,SEEK_CUR);
			if (pos>-1) lseek(fileno(fhandle),pos,SEEK_SET);
		} else fseek(fhandle,ftell(fhandle),SEEK_SET);
	}
	last_action=WRITE;
	if (*size==0){
		uint32_t pos=file_access_tries>0?lseek(fileno(fhandle),0,SEEK_CUR):ftell(fhandle);
		return !ftruncate(fileno(fhandle),pos);
	} else {
		*size=file_access_tries>0?(uint16_t)write(fileno(fhandle),data,*size):(uint16_t)fwrite(data,1,*size,fhandle);
		return true;
	}
}

#ifndef WIN32
bool toLock(int fd, bool is_lock, uint32_t pos, uint16_t size) {
    struct flock larg;
    unsigned long mask = 0xC0000000;
    int flag = fcntl(fd, F_GETFL);
    larg.l_type = is_lock ? (flag & O_RDWR || flag & O_WRONLY ? F_WRLCK : F_RDLCK) : F_UNLCK;
    larg.l_start = pos;
    larg.l_len = size;
    larg.l_len &= ~mask;
    if ((larg.l_start & mask) != 0)
        larg.l_start = (larg.l_start & ~mask) | ((larg.l_start & mask) >> 2);
    int ret;
#ifdef F_SETLK64
    ret = lock_file_region (fd,F_SETLK64,&larg,pos,size);
    if (ret == -1 && errno == EINVAL)
#endif
    ret = lock_file_region (fd,F_SETLK,&larg,larg.l_start,larg.l_len);
    return ret != -1;
}
#endif

// ert, 20100711: Locking extensions
// Wengier, 20201230: All platforms
bool localFile::LockFile(uint8_t mode, uint32_t pos, uint16_t size) {
#if defined(WIN32)
    static bool lockWarn = true;
	HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
    if (file_access_tries>0) {
        if (mode > 1) {
            DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
            return false;
        }
        if (!mode)																		// Lock, try 3 tries
            for (int tries = file_access_tries; tries; tries--) {
                if (::LockFile(hFile, pos, 0, size, 0)) {
                    if (lockWarn && ::LockFile(hFile, pos, 0, size, 0)) {
                        lockWarn = false;
                        char caption[512];
                        strcat(strcpy(caption, "Windows reference: "), dynamic_cast<localDrive*>(Drives[GetDrive()])->getBasedir());
                        MessageBox(NULL, "Record locking seems incorrectly implemented!\nConsult ...", caption, MB_OK|MB_ICONSTOP);
                    }
                    return true;
                }
                Sleep(25);																// If failed, wait 25 millisecs
            }
        else if (::UnlockFile(hFile, pos, 0, size, 0))									// This is a somewhat permanet condition!
            return true;
        DOS_SetError((uint16_t)GetLastError());
        return false;
    }
	BOOL bRet;
#else
	bool bRet;
#endif

	switch (mode)
	{
#if defined(WIN32)
	case 0: bRet = ::LockFile (hFile, pos, 0, size, 0); break;
	case 1: bRet = ::UnlockFile(hFile, pos, 0, size, 0); break;
#else
	case 0: bRet = toLock(fileno(fhandle), true, pos, size); break;
	case 1: bRet = toLock(fileno(fhandle), false, pos, size); break;
#endif
	default: 
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	}
	//LOG_MSG("::LockFile %s", name);

	if (!bRet)
	{
#if defined(WIN32)
		switch (GetLastError())
		{
		case ERROR_ACCESS_DENIED:
		case ERROR_LOCK_VIOLATION:
		case ERROR_NETWORK_ACCESS_DENIED:
		case ERROR_DRIVE_LOCKED:
		case ERROR_SEEK_ON_DEVICE:
		case ERROR_NOT_LOCKED:
		case ERROR_LOCK_FAILED:
			DOS_SetError(0x21);
			break;
		case ERROR_INVALID_HANDLE:
			DOS_SetError(DOSERR_INVALID_HANDLE);
			break;
		case ERROR_INVALID_FUNCTION:
		default:
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			break;
		}
#else
		switch (errno)
		{
		case EINTR:
		case ENOLCK:
		case EAGAIN:
			DOS_SetError(0x21);
			break;
		case EBADF:
			DOS_SetError(DOSERR_INVALID_HANDLE);
			break;
		case EINVAL:
		default:
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			break;
		}
#endif
	}
	return bRet;
}

extern const char* RunningProgram;
bool localFile::Seek(uint32_t * pos,uint32_t type) {
	int seektype;
	switch (type) {
	case DOS_SEEK_SET:seektype=SEEK_SET;break;
	case DOS_SEEK_CUR:seektype=SEEK_CUR;break;
	case DOS_SEEK_END:seektype=SEEK_END;break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}
#if defined(WIN32)
    if (file_access_tries>0) {
        HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
        DWORD dwPtr = SetFilePointer(hFile, *pos, NULL, type);
        if (dwPtr == INVALID_SET_FILE_POINTER && !strcmp(RunningProgram, "BTHORNE"))	// Fix for Black Thorne
            dwPtr = SetFilePointer(hFile, 0, NULL, DOS_SEEK_END);
        if (dwPtr != INVALID_SET_FILE_POINTER) {										// If success
            *pos = (uint32_t)dwPtr;
            return true;
        }
        DOS_SetError((uint16_t)GetLastError());
        return false;
    }
#endif
	bool fail;
	if (file_access_tries>0) fail=lseek(fileno(fhandle),*reinterpret_cast<int32_t*>(pos),seektype)==-1;
	else fail=fseek(fhandle,*reinterpret_cast<int32_t*>(pos),seektype)!=0;
	if (fail) {
		// Out of file range, pretend everything is ok
		// and move file pointer top end of file... ?! (Black Thorne)
		if (file_access_tries>0)
			lseek(fileno(fhandle),0,SEEK_END);
		else
			fseek(fhandle,0,SEEK_END);
	}
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	uint32_t * fake_pos=(uint32_t*)&temppos;
	*pos=*fake_pos;
#endif
	*pos=file_access_tries>0?(uint32_t)lseek(fileno(fhandle),0,SEEK_CUR):(uint32_t)ftell(fhandle);
	last_action=NONE;
	return true;
}

bool localFile::Close() {
    if (!newtime && fhandle && last_action == WRITE) UpdateLocalDateTime();
	if (newtime && fhandle) {
        // force STDIO to flush buffers on this file handle, or else fclose() will write buffered data
        // and cause mtime to reset back to current time.
        fflush(fhandle);

 		// backport from DOS_PackDate() and DOS_PackTime()
		struct tm tim = { 0 };
		tim.tm_sec  = (time&0x1f)*2;
		tim.tm_min  = (time>>5)&0x3f;
		tim.tm_hour = (time>>11)&0x1f;
		tim.tm_mday = date&0x1f;
		tim.tm_mon  = ((date>>5)&0x0f)-1;
		tim.tm_year = (date>>9)+1980-1900;
        // sanitize the date in case of invalid timestamps (such as 0x0000 date/time fields)
        if (tim.tm_mon < 0) tim.tm_mon = 0;
        if (tim.tm_mday == 0) tim.tm_mday = 1;
		//  have the C run-time library code compute whether standard time or daylight saving time is in effect.
		tim.tm_isdst = -1;
		// serialize time
		mktime(&tim);

        // change file time by file handle (while we still have it open)
        // so that we do not have to duplicate guest to host filename conversion here.
        // This should help Yksoft1 with file date/time, PC-98, and Shift-JIS Japanese filenames as well on Windows.

#if defined(WIN32) /* TODO: What about MinGW? */
        struct _utimbuf ftim;
        ftim.actime = ftim.modtime = mktime(&tim);

        if (_futime(fileno(fhandle), &ftim)) {
            extern int errno; 
            LOG_MSG("Set time failed (%s)", strerror(errno));
        }
#elif !defined(RISCOS) // Linux (TODO: What about Mac OS X/Darwin?)
        // NTS: Do not attempt futime, Linux doesn't have it.
        //      Do not attempt futimes, Linux man pages LIE about having it. It's even there in the freaking header, but not recognized!
        //      Use futimens. Modern stuff should have it. [https://pubs.opengroup.org/onlinepubs/9699919799/functions/futimens.html]
        struct timespec ftsp[2];
        ftsp[0].tv_sec =  ftsp[1].tv_sec =  mktime(&tim);
        ftsp[0].tv_nsec = ftsp[1].tv_nsec = 0;

        if (futimens(fileno(fhandle), ftsp)) {
            extern int errno; 
            LOG_MSG("Set time failed (%s)", strerror(errno));
        }
#endif
	}

	// only close if one reference left
	if (refCtr==1) {
		if(fhandle) fclose(fhandle); 
		fhandle = 0;
		open = false;
	}

	return true;
}

uint16_t localFile::GetInformation(void) {
	return read_only_medium ? DeviceInfoFlags::NotWritten : 0;
}


uint32_t localFile::GetSeekPos() {
	return file_access_tries>0?(uint32_t)lseek(fileno(fhandle),0,SEEK_CUR):(uint32_t)ftell( fhandle );
}

localFile::localFile() {}

localFile::localFile(const char* _name, FILE* handle) : fhandle(handle) {
	open=true;
	localFile::UpdateDateTimeFromHost();

	attr=DOS_ATTR_ARCHIVE;
	last_action=NONE;

	name=0;
	SetName(_name);
}

void localFile::FlagReadOnlyMedium(void) {
	read_only_medium = true;
}

bool localFile::UpdateDateTimeFromHost(void) {
	if(!open) return false;
	struct stat temp_stat;
	fstat(fileno(fhandle),&temp_stat);
    const struct tm* ltime;
	if((ltime=localtime(&temp_stat.st_mtime))!=0) {
		time=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
		date=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
	} else {
		time=1;date=1;
	}
	return true;
}

bool localFile::UpdateLocalDateTime(void) {
    time_t timet = ::time(NULL);
    struct tm *tm = localtime(&timet);
    tm->tm_isdst = -1;
    uint16_t oldax=reg_ax, oldcx=reg_cx, olddx=reg_dx;

	reg_ah=0x2a; // get system date
	CALLBACK_RunRealInt(0x21);

	tm->tm_year = reg_cx-1900;
	tm->tm_mon = reg_dh-1;
	tm->tm_mday = reg_dl;

	reg_ah=0x2c; // get system time
	CALLBACK_RunRealInt(0x21);

	tm->tm_hour = reg_ch;
	tm->tm_min = reg_cl;
	tm->tm_sec = reg_dh;

    reg_ax=oldax;
    reg_cx=oldcx;
    reg_dx=olddx;

    timet = mktime(tm);
    if (timet == -1)
        return false;
    tm = localtime(&timet);
    time = ((unsigned int)tm->tm_hour << 11u) + ((unsigned int)tm->tm_min << 5u) + ((unsigned int)tm->tm_sec >> 1u);
    date = (((unsigned int)tm->tm_year - 80u) << 9u) + (((unsigned int)tm->tm_mon + 1u) << 5u) + (unsigned int)tm->tm_mday;
    newtime = true;
    return true;
}


void localFile::Flush(void) {
#if defined(WIN32)
    if (file_access_tries>0) return;
#endif
	if (last_action==WRITE) {
		if (file_access_tries>0) {
			off_t pos = lseek(fileno(fhandle),0,SEEK_CUR);
			if (pos>-1) lseek(fileno(fhandle),pos,SEEK_SET);
		} else {
			fseek(fhandle,ftell(fhandle),SEEK_SET);
			fflush(fhandle);
		}
		last_action=NONE;
        if (!newtime) UpdateLocalDateTime();
	}
}


// ********************************************
// CDROM DRIVE
// ********************************************

int  MSCDEX_RemoveDrive(char driveLetter);
int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit);
bool MSCDEX_HasMediaChanged(uint8_t subUnit);
bool MSCDEX_GetVolumeName(uint8_t subUnit, char* name);

cdromDrive::cdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,options), driveLetter(driveLetter)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	strcpy(info, "CDRom ");
	strcat(info, startdir);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

bool cdromDrive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~((unsigned int)OPEN_READWRITE);
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool retcode = localDrive::FileOpen(file,name,flags);
	if(retcode) (dynamic_cast<localFile*>(*file))->FlagReadOnlyMedium();
	return retcode;
}

bool cdromDrive::FileCreate(DOS_File * * /*file*/,const char * /*name*/,uint16_t /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::FileUnlink(const char * /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir(const char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir(const char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename(const char * /*oldname*/,const char * /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::GetFileAttr(const char * name,uint16_t * attr) {
	bool result = localDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
}

bool cdromDrive::GetFileAttrEx(char* name, struct stat *status) {
	return localDrive::GetFileAttrEx(name,status);
}

unsigned long cdromDrive::GetCompressedSize(char* name) {
	return localDrive::GetCompressedSize(name);
}

#if defined (WIN32)
HANDLE cdromDrive::CreateOpenFile(const char* name) {
		return localDrive::CreateOpenFile(name);
}
unsigned long cdromDrive::GetSerial() {
		return localDrive::GetSerial();
}
#endif

bool cdromDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool /*fcb_findfirst*/) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return localDrive::FindFirst(_dir,dta);
}

void cdromDrive::SetDir(const char* path) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	localDrive::SetDir(path);
}

bool cdromDrive::isRemote(void) {
	return localDrive::isRemote();
}

bool cdromDrive::isRemovable(void) {
	return true;
}

Bits cdromDrive::UnMount(void) {
	if(MSCDEX_RemoveDrive(driveLetter)) {
		delete this;
		return 0;
	}
	return 2;
}
