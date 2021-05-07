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

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"
#include "inout.h"
#include "callback.h"
#include "regs.h"
#include "timer.h"
#include "../libs/physfs/physfs.h"
#include "../libs/physfs/physfs.c"
#include "../libs/physfs/physfs_archiver_7z.c"
#include "../libs/physfs/physfs_archiver_dir.c"
#include "../libs/physfs/physfs_archiver_grp.c"
#include "../libs/physfs/physfs_archiver_hog.c"
#include "../libs/physfs/physfs_archiver_iso9660.c"
#include "../libs/physfs/physfs_archiver_mvl.c"
#include "../libs/physfs/physfs_archiver_qpak.c"
#include "../libs/physfs/physfs_archiver_slb.c"
#include "../libs/physfs/physfs_archiver_unpacked.c"
#include "../libs/physfs/physfs_archiver_vdf.c"
#include "../libs/physfs/physfs_archiver_wad.c"
#include "../libs/physfs/physfs_archiver_zip.c"
#include "../libs/physfs/physfs_byteorder.c"
#include "../libs/physfs/physfs_platform_posix.c"
#include "../libs/physfs/physfs_platform_unix.c"
#include "../libs/physfs/physfs_platform_windows.c"
#include "../libs/physfs/physfs_platform_winrt.cpp"
#include "../libs/physfs/physfs_unicode.c"
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

#include "cp437_uni.h"
#include "cp808_uni.h"
#include "cp850_uni.h"
#include "cp852_uni.h"
#include "cp853_uni.h"
#include "cp855_uni.h"
#include "cp857_uni.h"
#include "cp858_uni.h"
#include "cp860_uni.h"
#include "cp861_uni.h"
#include "cp862_uni.h"
#include "cp863_uni.h"
#include "cp864_uni.h"
#include "cp865_uni.h"
#include "cp866_uni.h"
#include "cp869_uni.h"
#include "cp872_uni.h"
#include "cp874_uni.h"
#include "cp932_uni.h"

#if defined(PATH_MAX) && !defined(MAX_PATH)
#define MAX_PATH PATH_MAX
#endif

#if defined(WIN32)
// Windows: Use UTF-16 (wide char)
// TODO: Offer an option to NOT use wide char on Windows if directed by config.h
//       for people who compile this code for Windows 95 or earlier where some
//       widechar functions are missing.
typedef wchar_t host_cnv_char_t;
# define host_cnv_use_wchar
# define _HT(x) L##x
# if defined(__MINGW32__) /* TODO: Get MinGW to support 64-bit file offsets, at least targeting Windows XP! */
#  define ht_stat_t struct _stat
#  define ht_stat(x,y) _wstat(x,y)
# else
#  define ht_stat_t struct _stat64 /* WTF Microsoft?? Why aren't _stat and _wstat() consistent on stat struct type? */
#  define ht_stat(x,y) _wstat64(x,y)
# endif
# define ht_access(x,y) _waccess(x,y)
# define ht_strdup(x) _wcsdup(x)
# define ht_unlink(x) _wunlink(x)
#else
// Linux: Use UTF-8
typedef char host_cnv_char_t;
# define _HT(x) x
# define ht_stat_t struct stat
# define ht_stat(x,y) stat(x,y)
# define ht_access(x,y) access(x,y)
# define ht_strdup(x) strdup(x)
# define ht_unlink(x) unlink(x)
#endif

static host_cnv_char_t cpcnv_temp[4096];
static host_cnv_char_t cpcnv_ltemp[4096];
static uint16_t ldid[256];
static std::string ldir[256];
extern bool rsize, force_sfn, enable_share_exe;
extern int lfn_filefind_handle, freesizecap, file_access_tries;
extern unsigned long totalc, freec;

bool String_ASCII_TO_HOST(host_cnv_char_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    const host_cnv_char_t* df = d + CROSS_LEN - 1;
	const char *sf = s + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic < 32 || ic > 127) return false; // non-representable

#if defined(host_cnv_use_wchar)
        *d++ = (host_cnv_char_t)ic;
#else
        if (utf8_encode(&d,df,(uint32_t)ic) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
#endif
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_SBCS_TO_HOST_uint16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const uint16_t* df = d + CROSS_LEN - 1;
	const char *sf = s + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic >= map_max) return false; // non-representable
        MT wc = map[ic]; // output: unicode character

        *d++ = (uint16_t)wc;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_SBCS_TO_HOST(host_cnv_char_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const host_cnv_char_t* df = d + CROSS_LEN - 1;
	const char *sf = s + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        unsigned char ic = (unsigned char)(*s++);
        if (ic >= map_max) return false; // non-representable
        MT wc = map[ic]; // output: unicode character

#if defined(host_cnv_use_wchar)
        *d++ = (host_cnv_char_t)wc;
#else
        if (utf8_encode(&d,df,(uint32_t)wc) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
#endif
    }

    assert(d <= df);
    *d = 0;

    return true;
}

/* needed for Wengier's TTF output and PC-98 mode */
template <class MT> bool String_DBCS_TO_HOST_SHIFTJIS_uint16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const uint16_t* df = d + CROSS_LEN - 1;
	const char *sf = s + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        uint16_t ic = (unsigned char)(*s++);
        if ((ic & 0xE0) == 0x80 || (ic & 0xE0) == 0xE0) {
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

template <class MT> bool String_DBCS_TO_HOST_SHIFTJIS(host_cnv_char_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const host_cnv_char_t* df = d + CROSS_LEN - 1;
	const char *sf = s + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        uint16_t ic = (unsigned char)(*s++);
        if ((ic & 0xE0) == 0x80 || (ic & 0xE0) == 0xE0) {
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

#if defined(host_cnv_use_wchar)
        *d++ = (host_cnv_char_t)wc;
#else
        if (utf8_encode(&d,df,(uint32_t)wc) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
#endif
    }

    assert(d <= df);
    *d = 0;

    return true;
}

// TODO: This is SLOW. Optimize.
template <class MT> int SBCS_From_Host_Find(int c,const MT *map,const size_t map_max) {
    for (size_t i=0;i < map_max;i++) {
        if ((MT)c == map[i])
            return (int)i;
    }

    return -1;
}

// TODO: This is SLOW. Optimize.
template <class MT> int DBCS_SHIFTJIS_From_Host_Find(int c,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
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

template <class MT> bool String_HOST_TO_DBCS_SHIFTJIS(char *d/*CROSS_LEN*/,const host_cnv_char_t *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const host_cnv_char_t *sf = s + CROSS_LEN - 1;
    const char* df = d + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        int ic;
#if defined(host_cnv_use_wchar)
        ic = (int)(*s++);
#else
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable
#endif

        int oc = DBCS_SHIFTJIS_From_Host_Find<MT>(ic,hitbl,rawtbl,rawtbl_max);
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

template <class MT> bool String_HOST_TO_SBCS(char *d/*CROSS_LEN*/,const host_cnv_char_t *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const host_cnv_char_t *sf = s + CROSS_LEN - 1;
    const char* df = d + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        int ic;
#if defined(host_cnv_use_wchar)
        ic = (int)(*s++);
#else
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable
#endif

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

bool String_HOST_TO_ASCII(char *d/*CROSS_LEN*/,const host_cnv_char_t *s/*CROSS_LEN*/) {
    const host_cnv_char_t *sf = s + CROSS_LEN - 1;
    const char* df = d + CROSS_LEN - 1;

    while (*s != 0 && s < sf) {
        int ic;
#if defined(host_cnv_use_wchar)
        ic = (int)(*s++);
#else
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable
#endif

        if (ic < 32 || ic > 127)
            return false; // non-representable

        if (d >= df) return false;
        *d++ = (char)ic;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

bool cpwarn_once = false;

bool CodePageHostToGuest(char *d/*CROSS_LEN*/,const host_cnv_char_t *s/*CROSS_LEN*/) {
    switch (dos.loaded_codepage) {
        case 437:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 808:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 857:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 860:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 869:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_HOST_TO_DBCS_SHIFTJIS<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        default:
            /* at this time, it would be cruel and unusual to not allow any file I/O just because
             * our code page support is so limited. */
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to guest) for code page %u",dos.loaded_codepage);
            }
            return String_HOST_TO_ASCII(d,s);
    }

    return false;
}

bool CodePageGuestToHostUint16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    switch (dos.loaded_codepage) {
        case 437:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 808:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 857:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 860:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 869:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_DBCS_TO_HOST_SHIFTJIS_uint16<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        default: // Otherwise just use code page 437
            return String_SBCS_TO_HOST_uint16<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
    }

    return false;
}

bool CodePageGuestToHost(host_cnv_char_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
    switch (dos.loaded_codepage) {
        case 437:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 808:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp808_to_unicode,sizeof(cp808_to_unicode)/sizeof(cp808_to_unicode[0]));
        case 850:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp850_to_unicode,sizeof(cp850_to_unicode)/sizeof(cp850_to_unicode[0]));
        case 852:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp852_to_unicode,sizeof(cp852_to_unicode)/sizeof(cp852_to_unicode[0]));
        case 853:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp853_to_unicode,sizeof(cp853_to_unicode)/sizeof(cp853_to_unicode[0]));
        case 855:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp855_to_unicode,sizeof(cp855_to_unicode)/sizeof(cp855_to_unicode[0]));
        case 857:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp857_to_unicode,sizeof(cp857_to_unicode)/sizeof(cp857_to_unicode[0]));
        case 858:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp858_to_unicode,sizeof(cp858_to_unicode)/sizeof(cp858_to_unicode[0]));
        case 860:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp860_to_unicode,sizeof(cp860_to_unicode)/sizeof(cp860_to_unicode[0]));
        case 861:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp861_to_unicode,sizeof(cp861_to_unicode)/sizeof(cp861_to_unicode[0]));
        case 862:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp862_to_unicode,sizeof(cp862_to_unicode)/sizeof(cp862_to_unicode[0]));
        case 863:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp863_to_unicode,sizeof(cp863_to_unicode)/sizeof(cp863_to_unicode[0]));
        case 864:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp864_to_unicode,sizeof(cp864_to_unicode)/sizeof(cp864_to_unicode[0]));
        case 865:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp865_to_unicode,sizeof(cp865_to_unicode)/sizeof(cp865_to_unicode[0]));
        case 866:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp866_to_unicode,sizeof(cp866_to_unicode)/sizeof(cp866_to_unicode[0]));
        case 869:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp869_to_unicode,sizeof(cp869_to_unicode)/sizeof(cp869_to_unicode[0]));
        case 872:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp872_to_unicode,sizeof(cp872_to_unicode)/sizeof(cp872_to_unicode[0]));
        case 874:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp874_to_unicode,sizeof(cp874_to_unicode)/sizeof(cp874_to_unicode[0]));
        case 932:
            return String_DBCS_TO_HOST_SHIFTJIS<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        default:
            /* at this time, it would be cruel and unusual to not allow any file I/O just because
             * our code page support is so limited. */
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to host) for code page %u",dos.loaded_codepage);
            }
            return String_ASCII_TO_HOST(d,s);
    }

    return false;
}

host_cnv_char_t *CodePageGuestToHost(const char *s) {
    if (!CodePageGuestToHost(cpcnv_temp,s))
        return NULL;

    return cpcnv_temp;
}

char *CodePageHostToGuest(const host_cnv_char_t *s) {
    if (!CodePageHostToGuest((char*)cpcnv_temp,s))
        return NULL;

    return (char*)cpcnv_temp;
}

char *CodePageHostToGuestL(const host_cnv_char_t *s) {
    if (!CodePageHostToGuest((char*)cpcnv_ltemp,s))
        return NULL;

    return (char*)cpcnv_ltemp;
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
        int fd = open(host_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd<0) {close(fd);return false;}
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
  ret = lock_file_region( fd, F_SETLK64, &fl, 0x100000000LL, 1 );
  if ( ret == -1 && errno == EINVAL )
#endif
    lock_file_region( fd, F_SETLK, &fl, 0x100000000LL, 1 );
    LOG(LOG_DOSMISC,LOG_DEBUG)("internal SHARE: locking: fd %d, type %d whence %d pid %d\n", fd, fl.l_type, fl.l_whence, fl.l_pid);

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
	for (i=0;i<DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive()==drive && Files[i]->IsName(name)) {
			lfp=dynamic_cast<localFile*>(Files[i]);
			if (lfp) lfp->Flush();
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
        if (fd<0 || !share(fd, unix_mode & O_ACCMODE, flags)) {close(fd);return false;}
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

#if defined (WIN32)
#include <Shellapi.h>
#else
#include <glob.h>
#endif
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
		if(!file_writable) return false; //No acces ? ERROR MESSAGE NOT SET. FIXME ?
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
			dirCache.DeleteEntry(newname);
			return true;
		}
		return false;
	} else {
		dirCache.DeleteEntry(newname);
		return true;
	}
}

bool localDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	char tempDir[CROSS_LEN];
	strcpy(tempDir,basedir);
	strcat(tempDir,_dir);
	CROSS_FILENAME(tempDir);

	for (unsigned int i=0;i<strlen(tempDir);i++) tempDir[i]=toupper(tempDir[i]);
    if (nocachedir) EmptyCache();

	if (allocation.mediaid==0xF0 ) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}
    
	
	if (tempDir[strlen(tempDir)-1]!=CROSS_FILESPLIT) {
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
	dta.GetSearchParams(sAttr,tempDir,uselfn);

	if (this->isRemote() && this->isRemovable()) {
		// cdroms behave a bit different than regular drives
		if (sAttr == DOS_ATTR_VOLUME) {
			dta.SetResult(dirCache.GetLabel(),dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
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
            dta.SetResult(dirCache.GetLabel(),dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		} else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst) { 
		//should check for a valid leading directory instead of 0
		//exists==true if the volume label matches the searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
                dta.SetResult(dirCache.GetLabel(),dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
				return true;
			}
		}
	}
	return FindNext(dta);
}

char * shiftjis_upcase(char * str);

bool localDrive::FindNext(DOS_DTA & dta) {

    char * dir_ent, *ldir_ent;
	ht_stat_t stat_block;
    char full_name[CROSS_LEN], lfull_name[LFN_NAMELENGTH+1];
    char dir_entcopy[CROSS_LEN], ldir_entcopy[CROSS_LEN];

    uint8_t srch_attr;char srch_pattern[LFN_NAMELENGTH];
	uint8_t find_attr;

    dta.GetSearchParams(srch_attr,srch_pattern,uselfn);
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
	if (!(find_attr&DOS_ATTR_DIRECTORY)) find_attr|=DOS_ATTR_ARCHIVE;
	if(!(stat_block.st_mode & S_IWUSR)) find_attr|=DOS_ATTR_READ_ONLY;
#endif
 	if (~srch_attr & find_attr & DOS_ATTR_DIRECTORY) goto again;
	
	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII], lfind_name[LFN_NAMELENGTH+1];
    uint16_t find_date,find_time;uint32_t find_size;

	if(strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_entcopy);
        if (IS_PC98_ARCH)
            shiftjis_upcase(find_name);
        else
            upcase(find_name);
    }
	strcpy(lfind_name,ldir_entcopy);
    lfind_name[LFN_NAMELENGTH]=0;

	find_size=(uint32_t) stat_block.st_size;
    const struct tm* time;
	if((time=localtime(&stat_block.st_mtime))!=0){
		find_date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
		find_time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
	return true;
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
		if (attr & (DOS_ATTR_SYSTEM|DOS_ATTR_HIDDEN))
			LOG(LOG_DOSMISC,LOG_WARN)("%s: Application attempted to set system or hidden attributes for '%s' which is ignored for local drives",__FUNCTION__,newname);

		if (attr & DOS_ATTR_READ_ONLY)
			status.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
		else
			status.st_mode |=  S_IWUSR;

		if (chmod(host_name,status.st_mode) < 0) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}

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
		*attr=status.st_mode & S_IFDIR?0:DOS_ATTR_ARCHIVE;
		if(status.st_mode & S_IFDIR) *attr|=DOS_ATTR_DIRECTORY;
		if(!(status.st_mode & S_IWUSR)) *attr|=DOS_ATTR_READ_ONLY;
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
	if (len && (newdir[len-1]!='\\')) {
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
	dirCache.ExpandName(newold);
	
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
		res = GetDiskFreeSpace(drive?root:NULL, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
		if (res) {
			unsigned long total = dwTotalClusters * dwSectPerClust;
			int ratio = total > 2097120 ? 64 : (total > 1048560 ? 32 : (total > 524280 ? 16 : (total > 262140 ? 8 : (total > 131070 ? 4 : (total > 65535 ? 2 : 1)))));
			*_bytes_sector = (uint16_t)dwBytesPerSect;
			*_sectors_cluster = ratio;
			*_total_clusters = total > 4194240? 65535 : (uint16_t)(dwTotalClusters * dwSectPerClust / ratio);
			*_free_clusters = total > 4194240? 61440 : (uint16_t)(dwFreeClusters * dwSectPerClust / ratio);
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
			*_sectors_cluster = stat.f_bsize/512 > 64? 64 : stat.f_bsize/512;
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
                            *_free_clusters += diff;
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
	if((time=localtime(&temp_stat.st_mtime))!=0) {
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

    if (::read_directory_firstw((dir_information*)handle, tmp, stmp, is_directory)) {
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

    return false;
}

bool localDrive::read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory) {
    host_cnv_char_t tmp[MAX_PATH+1], stmp[MAX_PATH+1];

next:
    if (::read_directory_nextw((dir_information*)handle, tmp, stmp, is_directory)) {
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

    return false;
}

localDrive::localDrive(const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, std::vector<std::string> &options) {
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
        fseek(fhandle,ftell(fhandle),SEEK_SET);
        if (!newtime) UpdateLocalDateTime();
    }
	last_action=READ;
	*size=(uint16_t)fread(data,1,*size,fhandle);
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
        if (*size == 0)
            if (SetEndOfFile(hFile)) {
                UpdateLocalDateTime();
                return true;
            } else {
                DOS_SetError((uint16_t)GetLastError());
                return false;
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
	if (last_action==READ) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=WRITE;
	if(*size==0){  
        return (!ftruncate(fileno(fhandle),ftell(fhandle)));
    }
    else 
    {
		*size=(uint16_t)fwrite(data,1,*size,fhandle);
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
static bool lockWarn = true;
bool localFile::LockFile(uint8_t mode, uint32_t pos, uint16_t size) {
#if defined(WIN32)
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
        int32_t dwPtr = SetFilePointer(hFile, *pos, NULL, type);
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
	int ret=fseek(fhandle,*reinterpret_cast<int32_t*>(pos),seektype);
	if (ret!=0) {
		// Out of file range, pretend everythings ok 
		// and move file pointer top end of file... ?! (Black Thorne)
		fseek(fhandle,0,SEEK_END);
	}
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	uint32_t * fake_pos=(uint32_t*)&temppos;
	*pos=*fake_pos;
#endif
	*pos=(uint32_t)ftell(fhandle);
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
	return read_only_medium?0x40:0;
}
	

uint32_t localFile::GetSeekPos() {
	return (uint32_t)ftell( fhandle );
}

localFile::localFile() {}

localFile::localFile(const char* _name, FILE * handle) {
	fhandle=handle;
	open=true;
	localFile::UpdateDateTimeFromHost();

	attr=DOS_ATTR_ARCHIVE;
	last_action=NONE;
	read_only_medium=false;

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
		fseek(fhandle,ftell(fhandle),SEEK_SET);
        fflush(fhandle);
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
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,options),
		    subUnit(0),
		    driveLetter('\0')
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	strcpy(info, "CDRom ");
	strcat(info, startdir);
	this->driveLetter = driveLetter;
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

PHYSFS_sint64 PHYSFS_fileLength(const char *name) {
	PHYSFS_file *f = PHYSFS_openRead(name);
	if (f == NULL) return 0;
	PHYSFS_sint64 size = PHYSFS_fileLength(f);
	PHYSFS_close(f);
	return size;
}

/* Need to strip "/.." components and transform '\\' to '/' for physfs */
static char *normalize(char * name, const char *basedir) {
	int last = strlen(name)-1;
	strreplace(name,'\\','/');
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (last > 0 && name[last] == '.' && name[last-1] == '/') name[last-1] = 0;
	if (last > 1 && name[last] == '.' && name[last-1] == '.' && name[last-2] == '/') {
		name[last-2] = 0;
		char *slash = strrchr(name,'/');
		if (slash) *slash = 0;
	}
	if (strlen(basedir) > strlen(name)) { strcpy(name,basedir); strreplace(name,'\\','/'); }
	last = strlen(name)-1;
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (name[0] == 0) name[0] = '/';
	//LOG_MSG("File access: %s",name);
	return name;
}

class physfsFile : public DOS_File {
public:
	physfsFile(const char* name, PHYSFS_file * handle,uint16_t devinfo, const char* physname, bool write);
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool prepareRead();
	bool prepareWrite();
	bool Close();
	uint16_t GetInformation(void);
	bool UpdateDateTimeFromHost(void);
private:
	PHYSFS_file * fhandle;
	enum { READ,WRITE } last_action;
	uint16_t info;
	char pname[CROSS_LEN];
};

bool physfsDrive::AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) {
	const char *wdir = PHYSFS_getWriteDir();
	if (wdir) {
		bool res=false;
#if defined(WIN32)
		long unsigned int dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
		uint8_t drive=strlen(wdir)>1&&wdir[1]==':'?toupper(wdir[0])-'A'+1:0;
		if (drive>26) drive=0;
		char root[4]="A:\\";
		root[0]='A'+drive-1;
		res = GetDiskFreeSpace(drive?root:NULL, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
		if (res) {
			unsigned long total = dwTotalClusters * dwSectPerClust;
			int ratio = total > 2097120 ? 64 : (total > 1048560 ? 32 : (total > 524280 ? 16 : (total > 262140 ? 8 : (total > 131070 ? 4 : (total > 65535 ? 2 : 1)))));
			*_bytes_sector = (uint16_t)dwBytesPerSect;
			*_sectors_cluster = ratio;
			*_total_clusters = total > 4194240? 65535 : (uint16_t)(dwTotalClusters * dwSectPerClust / ratio);
			*_free_clusters = total > 4194240? 61440 : (uint16_t)(dwFreeClusters * dwSectPerClust / ratio);
			if (rsize) {
				totalc=dwTotalClusters * dwSectPerClust / ratio;
				freec=dwFreeClusters * dwSectPerClust / ratio;
			}
#else
		struct statvfs stat;
		res = statvfs(wdir, &stat) == 0;
		if (res) {
			int ratio = stat.f_blocks / 65536, tmp=ratio;
			*_bytes_sector = 512;
			*_sectors_cluster = stat.f_bsize/512 > 64? 64 : stat.f_bsize/512;
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
		} else {
            // 512*32*32765==~500MB total size
            // 512*32*16000==~250MB total free size
            *_bytes_sector = 512;
            *_sectors_cluster = 32;
            *_total_clusters = 32765;
            *_free_clusters = 16000;
		}
	} else {
        *_bytes_sector=allocation.bytes_sector;
        *_sectors_cluster=allocation.sectors_cluster;
        *_total_clusters=allocation.total_clusters;
        *_free_clusters=0;
    }
	return true;
}

bool physfsDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	return PHYSFS_exists(newname) && !PHYSFS_isDirectory(newname);
}

bool physfsDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	time_t mytime = PHYSFS_getLastModTime(newname);
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		stat_block->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		stat_block->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
		stat_block->time=DOS_PackTime(0,0,0);
		stat_block->date=DOS_PackDate(1980,1,1);
	}
	stat_block->size=(uint32_t)PHYSFS_fileLength(newname);
	return true;
}

struct opendirinfo {
	char dir[CROSS_LEN];
	char **files;
	int pos;
};

/* helper functions for drive cache */
bool physfsDrive::isdir(const char *name) {
	char myname[CROSS_LEN];
	strcpy(myname,name);
	normalize(myname,basedir);
	return PHYSFS_isDirectory(myname);
}

void *physfsDrive::opendir(const char *name) {
	char myname[CROSS_LEN];
	strcpy(myname,name);
	normalize(myname,basedir);
	if (!PHYSFS_isDirectory(myname)) return NULL;

	struct opendirinfo *oinfo = (struct opendirinfo *)malloc(sizeof(struct opendirinfo));
	strcpy(oinfo->dir, myname);
	oinfo->files = PHYSFS_enumerateFiles(myname);
	if (oinfo->files == NULL) {
		LOG_MSG("PHYSFS: nothing found for %s (%s)",myname,PHYSFS_getLastError());
		free(oinfo);
		return NULL;
	}

	oinfo->pos = (myname[4] == 0?0:-2);
	return (void *)oinfo;
}

void physfsDrive::closedir(void *handle) {
	struct opendirinfo *oinfo = (struct opendirinfo *)handle;
	if (handle == NULL) return;
	if (oinfo->files != NULL) PHYSFS_freeList(oinfo->files);
	free(oinfo);
}

bool physfsDrive::read_directory_first(void* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	return read_directory_next(dirp, entry_name, entry_sname, is_directory);
}

bool physfsDrive::read_directory_next(void* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	struct opendirinfo *oinfo = (struct opendirinfo *)dirp;
	if (!oinfo) return false;
	if (oinfo->pos == -2) {
		oinfo->pos++;
		safe_strncpy(entry_name,".",CROSS_LEN);
		safe_strncpy(entry_sname,".",DOS_NAMELENGTH+1);
		is_directory = true;
		return true;
	}
	if (oinfo->pos == -1) {
		oinfo->pos++;
		safe_strncpy(entry_name,"..",CROSS_LEN);
		safe_strncpy(entry_sname,"..",DOS_NAMELENGTH+1);
		is_directory = true;
		return true;
	}
	if (!oinfo->files || !oinfo->files[oinfo->pos]) return false;
	safe_strncpy(entry_name,oinfo->files[oinfo->pos++],CROSS_LEN);
	*entry_sname=0;
	is_directory = isdir(strlen(oinfo->dir)?(std::string(oinfo->dir)+"/"+std::string(entry_name)).c_str():entry_name);
	return true;
}

static uint8_t physfs_used = 0;
physfsDrive::physfsDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid,int &error,std::vector<std::string> &options)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,options)
{
	this->driveLetter = driveLetter;
	this->mountarc = "";
	char mp[] = "A_DRIVE";
	mp[0] = driveLetter;
	char newname[CROSS_LEN+1];
	strcpy(newname,startdir);
	CROSS_FILENAME(newname);
	if (!physfs_used) {
		PHYSFS_init("");
		PHYSFS_permitSymbolicLinks(1);
	}

	physfs_used++;
	char *lastdir = newname;
	char *dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z')?2:0),':');
	while (dir) {
		*dir++ = 0;
		if((lastdir == newname) && !strchr(dir+(((dir[0]|0x20) >= 'a' && (dir[0]|0x20) <= 'z')?2:0),':')) {
			// If the first parameter is a directory, the next one has to be the archive file,
			// do not confuse it with basedir if trailing : is not there!
			int tmp = strlen(dir)-1;
			dir[tmp++] = ':';
			dir[tmp++] = CROSS_FILESPLIT;
			dir[tmp] = '\0';
		}
		if (*lastdir) {
            if (PHYSFS_mount(lastdir,mp,true) == 0)
                LOG_MSG("PHYSFS couldn't mount '%s': %s",lastdir,PHYSFS_getLastError());
            else {
                if (mountarc.size()) mountarc+=", ";
                mountarc+= lastdir;
            }
        }
		lastdir = dir;
		dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z')?2:0),':');
	}
	error = 0;
	if (!mountarc.size()) {error = 10;return;}
	strcpy(basedir,"\\");
	strcat(basedir,mp);
	strcat(basedir,"\\");

	allocation.bytes_sector=_bytes_sector;
	allocation.sectors_cluster=_sectors_cluster;
	allocation.total_clusters=_total_clusters;
	allocation.free_clusters=_free_clusters;
	allocation.mediaid=_mediaid;

	dirCache.SetBaseDir(basedir, this);
}

physfsDrive::~physfsDrive(void) {
	if(!physfs_used) {
		LOG_MSG("PHYSFS invalid reference count!");
		return;
	}
	physfs_used--;
	if(!physfs_used) {
		LOG_MSG("PHYSFS calling PHYSFS_deinit()");
		PHYSFS_deinit();
	}
}

const char *physfsDrive::getOverlaydir(void) {
	static const char *overlaydir = PHYSFS_getWriteDir();
	return overlaydir;
}

bool physfsDrive::setOverlaydir(const char * name) {
	char newname[CROSS_LEN+1];
	strcpy(newname,name);
	CROSS_FILENAME(newname);
	const char *oldwrite = PHYSFS_getWriteDir();
	if (oldwrite) oldwrite = strdup(oldwrite);
	if (!PHYSFS_setWriteDir(newname)) {
		if (oldwrite)
			PHYSFS_setWriteDir(oldwrite);
        return false;
	} else {
        if (oldwrite) PHYSFS_removeFromSearchPath(oldwrite);
        PHYSFS_addToSearchPath(newname, 1);
        dirCache.EmptyCache();
    }
	if (oldwrite) free((char *)oldwrite);
    return true;
}

const char *physfsDrive::GetInfo() {
	sprintf(info,"PhysFS directory %s", mountarc.c_str());
	return info;
}

bool physfsDrive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);

	PHYSFS_file * hand;
	if (!PHYSFS_exists(newname)) return false;
	if ((flags&0xf) == OPEN_READ) {
		hand = PHYSFS_openRead(newname);
	} else {

		/* open for reading, deal with writing later */
		hand = PHYSFS_openRead(newname);
	}

	if (!hand) {
		if((flags&0xf) != OPEN_READ) {
			PHYSFS_file *hmm = PHYSFS_openRead(newname);
			if (hmm) {
				PHYSFS_close(hmm);
				LOG_MSG("Warning: file %s exists and failed to open in write mode.",newname);
			}
		}
		return false;
	}

	*file=new physfsFile(name,hand,0x202,newname,false);
	(*file)->flags=flags;  //for the inheritance flag and maybe check for others.
	return true;
}

bool physfsDrive::FileCreate(DOS_File * * file,const char * name,uint16_t attributes) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);

	/* Test if file exists, don't add to dirCache then */
	bool existing_file=PHYSFS_exists(newname);

	char *slash = strrchr(newname,'/');
	if (slash && slash != newname) {
		*slash = 0;
		if (!PHYSFS_isDirectory(newname)) return false;
		PHYSFS_mkdir(newname);
		*slash = '/';
	}

	PHYSFS_file * hand=PHYSFS_openWrite(newname);
	if (!hand){
		LOG_MSG("Warning: file creation failed: %s (%s)",newname,PHYSFS_getLastError());
		return false;
	}

	/* Make the 16 bit device information */
	*file=new physfsFile(name,hand,0x202,newname,true);
	(*file)->flags=OPEN_READWRITE;
	if(!existing_file) {
		strcpy(newname,basedir);
		strcat(newname,name);
		CROSS_FILENAME(newname);
		dirCache.AddEntry(newname, true);
		dirCache.EmptyCache();
	}
	return true;
}

bool physfsDrive::FileUnlink(const char * name) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	if (PHYSFS_delete(newname)) {
		CROSS_FILENAME(newname);
		dirCache.DeleteEntry(newname);
		dirCache.EmptyCache();
		return true;
	};
	return false;
}

bool physfsDrive::RemoveDir(const char * dir) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
	if (PHYSFS_isDirectory(newdir) && PHYSFS_delete(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.DeleteEntry(newdir,true);
		dirCache.EmptyCache();
		return true;
	}
	return false;
}

bool physfsDrive::MakeDir(const char * dir) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
	if (PHYSFS_mkdir(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.CacheOut(newdir,true);
		return true;
	}
	return false;
}

bool physfsDrive::TestDir(const char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
	return (PHYSFS_isDirectory(newdir));
}

bool physfsDrive::Rename(const char * oldname,const char * newname) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newold[CROSS_LEN];
	strcpy(newold,basedir);
	strcat(newold,oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandName(newold);
	normalize(newold,basedir);

	char newnew[CROSS_LEN];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);
	dirCache.ExpandName(newnew);
	normalize(newnew,basedir);
    const char *dir=PHYSFS_getRealDir(newold);
    if (dir && !strcmp(getOverlaydir(), dir)) {
        char fullold[CROSS_LEN], fullnew[CROSS_LEN];
        strcpy(fullold, dir);
        strcat(fullold, newold);
        strcpy(fullnew, dir);
        strcat(fullnew, newnew);
        if (rename(fullold, fullnew)==0) {
            dirCache.EmptyCache();
            return true;
        }
    } else LOG_MSG("PHYSFS: rename not supported (%s -> %s)",newold,newnew); // yuck. physfs doesn't have "rename".
	return false;
}

bool physfsDrive::SetFileAttr(const char * name,uint16_t attr) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfsDrive::GetFileAttr(const char * name,uint16_t * attr) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	char *last = strrchr(newname,'/');
	if (last == NULL) last = newname-1;

	*attr = 0;
	if (!PHYSFS_exists(newname)) return false;
	*attr=DOS_ATTR_ARCHIVE;
	if (PHYSFS_isDirectory(newname)) *attr|=DOS_ATTR_DIRECTORY;
    return true;
}

bool physfsDrive::GetFileAttrEx(char* name, struct stat *status) {
	return localDrive::GetFileAttrEx(name,status);
}

unsigned long physfsDrive::GetCompressedSize(char* name) {
	return localDrive::GetCompressedSize(name);
}

#if defined (WIN32)
HANDLE physfsDrive::CreateOpenFile(const char* name) {
		return localDrive::CreateOpenFile(name);
}

unsigned long physfsDrive::GetSerial() {
		return localDrive::GetSerial();
}
#endif

bool physfsDrive::FindNext(DOS_DTA & dta) {
	char * dir_ent, *ldir_ent;
	char full_name[CROSS_LEN], lfull_name[LFN_NAMELENGTH+1];

	uint8_t srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	uint8_t find_attr;

    dta.GetSearchParams(srch_attr,srch_pattern,uselfn);
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

	strcpy(full_name,lfn_filefind_handle>=LFN_FILEFIND_MAX?srchInfo[id].srch_dir:ldir[lfn_filefind_handle].c_str());
	strcpy(lfull_name,full_name);

	strcat(full_name,dir_ent);
    strcat(lfull_name,ldir_ent);

	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	dirCache.ExpandName(lfull_name);
	normalize(lfull_name,basedir);

	if (PHYSFS_isDirectory(lfull_name)) find_attr=DOS_ATTR_DIRECTORY|DOS_ATTR_ARCHIVE;
	else find_attr=DOS_ATTR_ARCHIVE;
	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;

	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII], lfind_name[LFN_NAMELENGTH+1];
	uint16_t find_date,find_time;uint32_t find_size;
	find_size=(uint32_t)PHYSFS_fileLength(lfull_name);
	time_t mytime = PHYSFS_getLastModTime(lfull_name);
	struct tm *time;
	if((time=localtime(&mytime))!=0){
		find_date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
		find_time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
	} else {
		find_time=6;
		find_date=4;
	}
	if(strlen(dir_ent)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_ent);
		upcase(find_name);
	}
	strcpy(lfind_name,ldir_ent);
	lfind_name[LFN_NAMELENGTH]=0;

	dta.SetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool physfsDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool /*fcb_findfirst*/) {
	return localDrive::FindFirst(_dir,dta);
}

bool physfsDrive::isRemote(void) {
	return localDrive::isRemote();
}

bool physfsDrive::isRemovable(void) {
	return true;
}

Bits physfsDrive::UnMount(void) {
	char mp[] = "A_DRIVE";
	mp[0] = driveLetter;
	PHYSFS_unmount(mp);
	delete this;
	return 0;
}

bool physfsFile::Read(uint8_t * data,uint16_t * size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {        // check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==WRITE) prepareRead();
	last_action=READ;
	PHYSFS_sint64 mysize = PHYSFS_read(fhandle,data,1,(PHYSFS_uint64)*size);
	//LOG_MSG("Read %i bytes (wanted %i) at %i of %s (%s)",(int)mysize,(int)*size,(int)PHYSFS_tell(fhandle),name,PHYSFS_getLastError());
	*size = (uint16_t)mysize;
	return true;
}

bool physfsFile::Write(const uint8_t * data,uint16_t * size) {
	if (!PHYSFS_getWriteDir() || (this->flags & 0xf) == OPEN_READ) { // check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==READ) prepareWrite();
	last_action=WRITE;
	if (*size==0) {
		if (PHYSFS_tell(fhandle) == 0) {
			PHYSFS_close(PHYSFS_openWrite(pname));
			//LOG_MSG("Truncate %s (%s)",name,PHYSFS_getLastError());
		} else {
			LOG_MSG("PHYSFS TODO: truncate not yet implemented (%s at %i)",pname,(int)PHYSFS_tell(fhandle));
			return false;
		}
	} else {
		PHYSFS_sint64 mysize = PHYSFS_write(fhandle,data,1,(PHYSFS_uint64)*size);
		//LOG_MSG("Wrote %i bytes (wanted %i) at %i of %s (%s)",(int)mysize,(int)*size,(int)PHYSFS_tell(fhandle),name,PHYSFS_getLastError());
		*size = (uint16_t)mysize;
		return true;
	}
}

bool physfsFile::Seek(uint32_t * pos,uint32_t type) {
	PHYSFS_sint64 mypos = (int32_t)*pos;
	switch (type) {
	case DOS_SEEK_SET:break;
	case DOS_SEEK_CUR:mypos += PHYSFS_tell(fhandle); break;
	case DOS_SEEK_END:mypos += PHYSFS_fileLength(fhandle)-mypos; break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}

	if (!PHYSFS_seek(fhandle,mypos)) {
		// Out of file range, pretend everythings ok
		// and move file pointer top end of file... ?! (Black Thorne)
		PHYSFS_seek(fhandle,PHYSFS_fileLength(fhandle));
	};
	//LOG_MSG("Seek to %i (%i at %x) of %s (%s)",(int)mypos,(int)*pos,type,name,PHYSFS_getLastError());

	*pos=(uint32_t)PHYSFS_tell(fhandle);
	return true;
}

bool physfsFile::prepareRead() {
	PHYSFS_uint64 pos = PHYSFS_tell(fhandle);
	PHYSFS_close(fhandle);
	fhandle = PHYSFS_openRead(pname);
	PHYSFS_seek(fhandle, pos);
	return true; //LOG_MSG("Goto read (%s at %i)",pname,PHYSFS_tell(fhandle));
}

#ifndef WIN32
#include <fcntl.h>
#include <errno.h>
#endif

bool physfsFile::prepareWrite() {
	const char *wdir = PHYSFS_getWriteDir();
	if (wdir == NULL) {
		LOG_MSG("PHYSFS could not fulfill write request: no write directory set.");
		return false;
	}
	//LOG_MSG("Goto write (%s at %i)",pname,PHYSFS_tell(fhandle));
	const char *fdir = PHYSFS_getRealDir(pname);
	PHYSFS_uint64 pos = PHYSFS_tell(fhandle);
	char *slash = strrchr(pname,'/');
	if (slash && slash != pname) {
		*slash = 0;
		PHYSFS_mkdir(pname);
		*slash = '/';
	}
	if (strcmp(fdir,wdir)) { /* we need COW */
		//LOG_MSG("COW",pname,PHYSFS_tell(fhandle));
		PHYSFS_file *whandle = PHYSFS_openWrite(pname);
		if (whandle == NULL) {
			LOG_MSG("PHYSFS copy-on-write failed: %s.",PHYSFS_getLastError());
			return false;
		}
		char buffer[65536];
		PHYSFS_sint64 size;
		PHYSFS_seek(fhandle, 0);
		while ((size = PHYSFS_read(fhandle,buffer,1,65536)) > 0) {
			if (PHYSFS_write(whandle,buffer,1,size) != size) {
				LOG_MSG("PHYSFS copy-on-write failed: %s.",PHYSFS_getLastError());
				PHYSFS_close(whandle);
				return false;
			}
		}
		PHYSFS_seek(whandle, pos);
		PHYSFS_close(fhandle);
		fhandle = whandle;
	} else { // megayuck - physfs on posix platforms uses O_APPEND. We illegally access the fd directly and clear that flag.
		//LOG_MSG("noCOW",pname,PHYSFS_tell(fhandle));
		PHYSFS_close(fhandle);
		fhandle = PHYSFS_openAppend(pname);
#ifndef WIN32
		int rc = fcntl(**(int**)fhandle->opaque,F_SETFL,0);
#endif
		PHYSFS_seek(fhandle, pos);
	}
	return true;
}

bool physfsFile::Close() {
	// only close if one reference left
	if (refCtr==1) {
		PHYSFS_close(fhandle);
		fhandle = 0;
		open = false;
	};
	return true;
}

uint16_t physfsFile::GetInformation(void) {
	return info;
}

physfsFile::physfsFile(const char* _name, PHYSFS_file * handle,uint16_t devinfo, const char* physname, bool write) {
	fhandle=handle;
	info=devinfo;
	strcpy(pname,physname);
	time_t mytime = PHYSFS_getLastModTime(pname);
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		this->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		this->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
		this->time=DOS_PackTime(0,0,0);
		this->date=DOS_PackDate(1980,1,1);
	}

	attr=DOS_ATTR_ARCHIVE;
	last_action=(write?WRITE:READ);

	open=true;
	name=0;
	SetName(_name);
}

bool physfsFile::UpdateDateTimeFromHost(void) {
	if(!open) return false;
	time_t mytime = PHYSFS_getLastModTime(pname);
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		this->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		this->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
		this->time=DOS_PackTime(0,0,0);
		this->date=DOS_PackDate(1980,1,1);
	}
	return true;
}

physfscdromDrive::physfscdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options)
		   :physfsDrive(driveLetter,startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,error,options)
{
	// Init mscdex
	if (!mountarc.size()) {error = 10;return;}
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	this->driveLetter = driveLetter;
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
};

const char *physfscdromDrive::GetInfo() {
	sprintf(info,"PhysFS CDRom %s", mountarc.c_str());
	return info;
}

bool physfscdromDrive::FileOpen(DOS_File * * file,const char * name,uint32_t flags)
{
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~OPEN_READWRITE;
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	return physfsDrive::FileOpen(file,name,flags);
};

bool physfscdromDrive::FileCreate(DOS_File * * file,const char * name,uint16_t attributes)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::FileUnlink(const char * name)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::RemoveDir(const char * dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::MakeDir(const char * dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::Rename(const char * oldname,const char * newname)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::GetFileAttr(const char * name,uint16_t * attr)
{
	bool result = physfsDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
};

bool physfscdromDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return physfsDrive::FindFirst(_dir,dta);
};

void physfscdromDrive::SetDir(const char* path)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	physfsDrive::SetDir(path);
};

bool physfscdromDrive::isRemote(void) {
	return true;
}

bool physfscdromDrive::isRemovable(void) {
	return true;
}

Bits physfscdromDrive::UnMount(void) {
	return true;
}

#define OVERLAY_DIR 1
bool logoverlay = false;
using namespace std;

#if defined (WIN32) || defined (OS2)				/* Win 32 & OS/2*/
#define CROSS_DOSFILENAME(blah) 
#else
//Convert back to DOS PATH 
#define	CROSS_DOSFILENAME(blah) strreplace(blah,'/','\\')
#endif

char* GetCrossedName(const char *basedir, const char *dir) {
	static char crossname[CROSS_LEN];
	strcpy(crossname, basedir);
	strcat(crossname, dir);
	CROSS_FILENAME(crossname);
	return crossname;
}

/* 
 * Wengier: Long filenames are supported in all including overlay drives.
 * Shift-JIS characters (Kana, Kanji, etc) are also supported in PC-98 mode.
 *
 * New rename for base directories (not yet supported):
 * Alter shortname in the drive_cache: take care of order and long names. 
 * update stored deleted files list in overlay. 
 */

//TODO recheck directories under linux with the filename_cache (as one adds the dos name (and runs cross_filename on the other))


//TODO Check: Maybe handle file redirection in ccc (opening the new file), (call update datetime host there ?)


/* For rename/delete(unlink)/makedir/removedir we need to rebuild the cache. (shouldn't be needed, 
 * but cacheout/delete entry currently throw away the cached folder and rebuild it on read. 
 * so we have to ensure the rebuilding is controlled through the overlay.
 * In order to not reread the overlay directory contents, the information in there is cached and updated when
 * it changes (when deleting a file or adding one)
 */


//directories that exist only in overlay can not be added to the drive_cache currently. 
//Either upgrade addentry to support directories. (without actually caching stuff in! (code in testing))
//Or create an empty directory in local drive base. 

bool Overlay_Drive::RemoveDir(const char * dir) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}
	
    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	//DOS_RemoveDir checks if directory exists.
#if OVERLAY_DIR
	if (logoverlay) LOG_MSG("Overlay: trying to remove directory: %s",dir);
#else
	E_Exit("Overlay: trying to remove directory: %s",dir);
#endif
	/* Overlay: Check if folder is empty (findfirst/next, skipping . and .. and breaking on first file found ?), if so, then it is not too tricky. */
	if (is_dir_only_in_overlay(dir)) {
		//The simple case
		char sdir[CROSS_LEN],odir[CROSS_LEN];
		strcpy(sdir,dir);
		strcpy(odir,overlaydir);
		strcat(odir,sdir);
		CROSS_FILENAME(odir);
		const host_cnv_char_t* host_name = CodePageGuestToHost(odir);
		int temp=-1;
		if (host_name!=NULL) {
#if defined (WIN32)
			temp=_wrmdir(host_name);
#else
			temp=rmdir(host_name);
#endif
		}
		if (temp) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,dir));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				strcpy(sdir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
				strcpy(odir,overlaydir);
				strcat(odir,sdir);
				CROSS_FILENAME(odir);
				host_name = CodePageGuestToHost(odir);
				if (host_name!=NULL) {
#if defined (WIN32)
					temp=_wrmdir(host_name);
#else
					temp=rmdir(host_name);
#endif
				}
			}
		}
		if (temp == 0) {
			remove_DOSdir_from_cache(dir);
			char newdir[CROSS_LEN];
			strcpy(newdir,basedir);
			strcat(newdir,sdir);
			CROSS_FILENAME(newdir);
			dirCache.DeleteEntry(newdir,true);
			dirCache.EmptyCache();
			update_cache(false);
		}
		return (temp == 0);
	} else {
		uint16_t olderror = dos.errorcode; //FindFirst/Next always set an errorcode, while RemoveDir itself shouldn't touch it if successful
		DOS_DTA dta(dos.tables.tempdta);
		char stardotstar[4] = {'*', '.', '*', 0};
		dta.SetupSearch(0,(0xff & ~DOS_ATTR_VOLUME),stardotstar); //Fake drive as we don't use it.
		bool ret = this->FindFirst(dir,dta,false);// DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			//Path not found. Should not be possible due to removedir doing a testdir, but lets be correct
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}
		bool empty = true;
		do {
			char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];uint32_t size;uint16_t date;uint16_t time;uint8_t attr;
			dta.GetResult(name,lname,size,date,time,attr);
			if (logoverlay) LOG_MSG("RemoveDir found %s",name);
			if (empty && strcmp(".",name ) && strcmp("..",name)) 
				empty = false; //Neither . or .. so directory not empty.
		} while ( (ret=this->FindNext(dta)) );
		//Always exhaust list, so drive_cache entry gets invalidated/reused.
		//FindNext is done, restore error code to old value. DOS_RemoveDir will set the right one if needed.
		dos.errorcode = olderror;

		if (!empty) return false;
		if (logoverlay) LOG_MSG("directory empty! Hide it.");
		//Directory is empty, mark it as deleted and create $DBOVERLAY file.
		//Ensure that overlap folder can not be created.
		char odir[CROSS_LEN];
		strcpy(odir,overlaydir);
		strcat(odir,dir);
		CROSS_FILENAME(odir);
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,dir));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(odir,overlaydir);
			strcat(odir,temp_name);
			CROSS_FILENAME(odir);
			rmdir(odir);
		}
		add_deleted_path(dir,true);
		return true;
	}
}

bool Overlay_Drive::MakeDir(const char * dir) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
	//DOS_MakeDir tries first, before checking if the directory already exists, so doing it here as well, so that case is handled.
	if (TestDir(dir)) return false;
	if (overlap_folder == dir) return false; //TODO Test
#if OVERLAY_DIR
	if (logoverlay) LOG_MSG("Overlay trying to make directory: %s",dir);
#else
	E_Exit("Overlay trying to make directory: %s",dir);
#endif
	/* Overlay: Create in Overlay only and add it to drive_cache + some entries else the drive_cache will try to access it. Needs an AddEntry for directories. */ 

	//Check if leading dir is marked as deleted.
	if (check_if_leading_is_deleted(dir)) return false;

	//Check if directory itself is marked as deleted
	if (is_deleted_path(dir) && localDrive::TestDir(dir)) {
		//Was deleted before and exists (last one is safety check)
		remove_deleted_path(dir,true);
		return true;
	}
	char newdir[CROSS_LEN],sdir[CROSS_LEN],pdir[CROSS_LEN];
	strcpy(sdir,dir);
	char *p=strrchr(sdir,'\\');
	if (p!=NULL) {
		*p=0;
		char *temp_name=dirCache.GetExpandName(GetCrossedName(basedir,sdir));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			strcpy(newdir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
			strcat(newdir,"\\");
			strcat(newdir,p+1);
			strcpy(sdir,newdir);
		}
	}
	strcpy(newdir,overlaydir);
	strcat(newdir,sdir);
	CROSS_FILENAME(newdir);
	p=strrchr(sdir,'\\');
	int temp=-1;
	bool madepdir=false;
	const host_cnv_char_t* host_name;
	if (p!=NULL) {
		*p=0;
		if (strlen(sdir)) {
			strcpy(pdir,overlaydir);
			strcat(pdir,sdir);
			CROSS_FILENAME(pdir);
			if (!is_deleted_path(sdir) && localDrive::TestDir(sdir)) {
				host_name = CodePageGuestToHost(pdir);
				if (host_name!=NULL) {
#if defined (WIN32)
					temp=_wmkdir(host_name);
#else
					temp=mkdir(host_name,0775);
#endif
					if (temp==0) madepdir=true;
				}
			}
		}
	}
	temp=-1;
	host_name = CodePageGuestToHost(newdir);
	if (host_name!=NULL) {
#if defined (WIN32)
		temp=_wmkdir(host_name);
#else
		temp=mkdir(host_name,0775);
#endif
	}
	if (temp==0) {
		char fakename[CROSS_LEN];
		strcpy(fakename,basedir);
		strcat(fakename,dir);
		CROSS_FILENAME(fakename);
		strcpy(sdir, dir);
		upcase(sdir);
		dirCache.AddEntryDirOverlay(fakename,sdir,true);
		add_DOSdir_to_cache(dir,sdir);
		dirCache.EmptyCache();
		update_cache(true);
	} else if (madepdir) {
		host_name = CodePageGuestToHost(pdir);
		if (host_name!=NULL) {
#if defined (WIN32)
			_wrmdir(host_name);
#else
			rmdir(host_name);
#endif
		}
	}

	return (temp == 0);// || ((temp!=0) && (errno==EEXIST));
}

bool Overlay_Drive::TestDir(const char * dir) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	//First check if directory exist exclusively in the overlay. 
	//Currently using the update_cache cache, alternatively access the directory itself.

	//Directories are stored without a trailing backslash
	char tempdir[CROSS_LEN];
	strcpy(tempdir,dir);
	size_t templen = strlen(dir);
	if (templen && tempdir[templen-1] == '\\') tempdir[templen-1] = 0;

#if OVERLAY_DIR
	if (is_dir_only_in_overlay(tempdir)) return true;
#endif

	//Next Check if the directory is marked as deleted or one of its leading directories is.
	//(it still might exists in the localDrive)

	if (is_deleted_path(tempdir)) return false; 

	// Not exclusive to overlay nor marked as deleted. Pass on to LocalDrive
	return localDrive::TestDir(dir);
}


class OverlayFile: public localFile {
public:
	OverlayFile(const char* name, FILE * handle):localFile(name,handle){
		overlay_active = false;
		if (logoverlay) LOG_MSG("constructing OverlayFile: %s",name);
	}
	bool Write(const uint8_t * data,uint16_t * size) {
		uint32_t f = flags&0xf;
		if (!overlay_active && (f == OPEN_READWRITE || f == OPEN_WRITE)) {
			if (logoverlay) LOG_MSG("write detected, switching file for %s",GetName());
			if (*data == 0) {
				if (logoverlay) LOG_MSG("OPTIMISE: truncate on switch!!!!");
			}
			uint32_t a = GetTicks();
			bool r = create_copy();
			if (GetTicks() - a > 2) {
				if (logoverlay) LOG_MSG("OPTIMISE: switching took %d",GetTicks() - a);
			}
			if (!r) return false;
			overlay_active = true;
			
		}
		return localFile::Write(data,size);
	}
	bool create_copy();
//private:
	bool overlay_active;
};

//Create leading directories of a file being overlayed if they exist in the original (localDrive).
//This function is used to create copies of existing files, so all leading directories exist in the original.

FILE* Overlay_Drive::create_file_in_overlay(const char* dos_filename, char const* mode) {
	char newname[CROSS_LEN];
	strcpy(newname,overlaydir); //TODO GOG make part of class and join in 
	strcat(newname,dos_filename); //HERE we need to convert it to Linux TODO
	CROSS_FILENAME(newname);

#ifdef host_cnv_use_wchar
    wchar_t wmode[8];
    unsigned int tis;
    for (tis=0;tis < 7 && mode[tis] != 0;tis++) wmode[tis] = (wchar_t)mode[tis];
    assert(tis < 7); // guard
    wmode[tis] = 0;
#endif
	FILE* f;
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
	if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
		f = _wfopen(host_name,wmode);
#else
		f = fopen_wrap(host_name,mode);
#endif
	}
	else {
		f = fopen_wrap(newname,mode);
	}
	//Check if a directories are part of the name:
	char* dir = strrchr((char *)dos_filename,'\\');
	if (!f && dir && *dir) {
		if (logoverlay) LOG_MSG("Overlay: warning creating a file inside a directory %s",dos_filename);
		//ensure they exist, else make them in the overlay if they exist in the original....
		Sync_leading_dirs(dos_filename);
		//try again
		char temp_name[CROSS_LEN],tmp[CROSS_LEN];
		strcpy(tmp, dos_filename);
		char *p=strrchr(tmp, '\\');
		assert(p!=NULL);
		*p=0;
		bool found=false;
		for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2)
			if ((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), tmp)) {
				found=true;
				strcpy(tmp, (*it).c_str());
				break;
			}
		if (found) {
			strcpy(temp_name,overlaydir);
			strcat(temp_name,tmp);
			strcat(temp_name,dir);
			CROSS_FILENAME(temp_name);
			const host_cnv_char_t* host_name = CodePageGuestToHost(temp_name);
			if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
				f = _wfopen(host_name,wmode);
#else
				f = fopen_wrap(host_name,mode);
#endif
			}
			else {
				f = fopen_wrap(temp_name,mode);
			}
		}
		if (!f) {
			strcpy(temp_name,dirCache.GetExpandName(GetCrossedName(basedir,dos_filename)));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				strcpy(newname,overlaydir);
				strcat(newname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
				CROSS_FILENAME(newname);
			}
			const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
			if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
				f = _wfopen(host_name,wmode);
#else
				f = fopen_wrap(host_name,mode);
#endif
			}
			else {
				f = fopen_wrap(newname,mode);
			}
		}
	}

	return f;
}

#ifndef BUFSIZ
#define BUFSIZ 2048
#endif

bool OverlayFile::create_copy() {
	//test if open/valid/etc
	//ensure file position
	FILE* lhandle = this->fhandle;
	fseek(lhandle,ftell(lhandle),SEEK_SET);
	int location_in_old_file = ftell(lhandle);
	fseek(lhandle,0L,SEEK_SET);
	
	FILE* newhandle = NULL;
	uint8_t drive_set = GetDrive();
	if (drive_set != 0xff && drive_set < DOS_DRIVES && Drives[drive_set]){
		Overlay_Drive* od = dynamic_cast<Overlay_Drive*>(Drives[drive_set]);
		if (od) {
			newhandle = od->create_file_in_overlay(GetName(),"wb+"); //todo check wb+
		}
	}
 
	if (!newhandle) return false;
	char buffer[BUFSIZ];
	size_t s;
	while ( (s = fread(buffer,1,BUFSIZ,lhandle)) != 0 ) fwrite(buffer, 1, s, newhandle);
	fclose(lhandle);
	//Set copied file handle to position of the old one 
	fseek(newhandle,location_in_old_file,SEEK_SET);
	this->fhandle = newhandle;
	//Flags ?
	return true;
}



static OverlayFile* ccc(DOS_File* file) {
	localFile* l = dynamic_cast<localFile*>(file);
	if (!l) E_Exit("overlay input file is not a localFile");
	//Create an overlayFile
	OverlayFile* ret = new OverlayFile(l->GetName(),l->fhandle);
	ret->flags = l->flags;
	ret->refCtr = l->refCtr;
	delete l;
	return ret;
}

Overlay_Drive::Overlay_Drive(const char * startdir,const char* overlay, uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid,uint8_t &error,std::vector<std::string> &options)
:localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,options),special_prefix("$DBOVERLAY") {
	optimize_cache_v1 = true; //Try to not reread overlay files on deletes. Ideally drive_cache should be improved to handle deletes properly.
	//Currently this flag does nothing, as the current behavior is to not reread due to caching everything.
#if defined (WIN32)	
	if (strcasecmp(startdir,overlay) == 0) {
#else 
	if (strcmp(startdir,overlay) == 0) {
#endif
		//overlay directory can not be the base directory
		error = 2;
		return;
	}

	std::string s(startdir);
	std::string o(overlay);
	bool s_absolute = Cross::IsPathAbsolute(s);
	bool o_absolute = Cross::IsPathAbsolute(o);
	error = 0;
	if (s_absolute != o_absolute) { 
		error = 1;
		return;
	}
	strcpy(overlaydir,overlay);
	char dirname[CROSS_LEN] = { 0 };
	//Determine if overlaydir is part of the startdir.
	convert_overlay_to_DOSname_in_base(dirname);

	size_t dirlen = strlen(dirname);
	if(dirlen && dirname[dirlen - 1] == '\\') dirname[dirlen - 1] = 0;
			
	//add_deleted_path(dirname); //update_cache will add the overlap_folder
	overlap_folder = dirname;

	update_cache(true);
}

void Overlay_Drive::convert_overlay_to_DOSname_in_base(char* dirname ) 
{
	dirname[0] = 0;//ensure good return string
	if (strlen(overlaydir) >= strlen(basedir) ) {
		//Needs to be longer at least.
#if defined (WIN32)
//OS2 ?	
		if (strncasecmp(overlaydir,basedir,strlen(basedir)) == 0) {
#else
		if (strncmp(overlaydir,basedir,strlen(basedir)) == 0) {
#endif
			//Beginning is the same.
			char t[CROSS_LEN];
			strcpy(t,overlaydir+strlen(basedir));

			char* p = t;
			char* b = t;

			while ( (p =strchr(p,CROSS_FILESPLIT)) ) {
				char directoryname[CROSS_LEN]={0};
				char dosboxdirname[CROSS_LEN]={0};
				strcpy(directoryname,dirname);
				strncat(directoryname,b,p-b);

				char d[CROSS_LEN];
				strcpy(d,basedir);
				strcat(d,directoryname);
				CROSS_FILENAME(d);
				//Try to find the corresponding directoryname in DOSBox.
				if(!dirCache.GetShortName(d,dosboxdirname) ) {
					//Not a long name, assume it is a short name instead
					strncpy(dosboxdirname,b,p-b);
					upcase(dosboxdirname);
				}


				strcat(dirname,dosboxdirname);
				strcat(dirname,"\\");

				if (logoverlay) LOG_MSG("HIDE directory: %s",dirname);

				b=++p;

			}
		}
	}
}

bool Overlay_Drive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

    if (ovlreadonly) {
        if ((flags&0xf) == OPEN_WRITE || (flags&0xf) == OPEN_READWRITE) {
            DOS_SetError(DOSERR_WRITE_PROTECTED);
            return false;
        }
    }
	const host_cnv_char_t * type;
	switch (flags&0xf) {
	case OPEN_READ:        type = _HT("rb"); break;
	case OPEN_WRITE:       type = _HT("rb+"); break;
	case OPEN_READWRITE:   type = _HT("rb+"); break;
	case OPEN_READ_NO_MOD: type = _HT("rb"); break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}

	//Flush the buffer of handles for the same file. (Betrayal in Antara)
	uint8_t i,drive = DOS_DRIVES;
	localFile *lfp;
	for (i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]==this) {
			drive=i;
			break;
		}
	}
	for (i=0;i<DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive()==drive && Files[i]->IsName(name)) {
			lfp=dynamic_cast<localFile*>(Files[i]);
			if (lfp) lfp->Flush();
		}
	}


	//Todo check name first against local tree
	//if name exists, use that one instead!
	//overlay file.
	char newname[CROSS_LEN];
	strcpy(newname,overlaydir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
#ifdef host_cnv_use_wchar
    FILE * hand = _wfopen(host_name,type);
#else
	FILE * hand = fopen_wrap(newname,type);
#endif
	if (!hand) {
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(newname,overlaydir);
			strcat(newname,temp_name);
			CROSS_FILENAME(newname);
			host_name = CodePageGuestToHost(newname);
			if (host_name != NULL) {
#ifdef host_cnv_use_wchar
				hand = _wfopen(host_name,type);
#else
				hand = fopen_wrap(newname,type);
#endif
			}
		}
	}
	bool fileopened = false;
	if (hand) {
		if (logoverlay) LOG_MSG("overlay file opened %s",newname);
		*file=new localFile(name,hand);
		(*file)->flags=flags;
		fileopened = true;
	} else {
		; //TODO error handling!!!! (maybe check if it exists and read only (should not happen with overlays)
	}
	bool overlayed = fileopened;

	//File not present in overlay, try normal drive
	//TODO take care of file being marked deleted.

	if (!fileopened && !is_deleted_file(name)) fileopened = localDrive::FileOpen(file,name, OPEN_READ);


	if (fileopened) {
		if (logoverlay) LOG_MSG("file opened %s",name);
		//Convert file to OverlayFile
		OverlayFile* f = ccc(*file);
		f->flags = flags; //ccc copies the flags of the localfile, which were not correct in this case
		f->overlay_active = overlayed; //No need to switch if already in overlayed.
		*file = f;
	}
	return fileopened;
}

bool Overlay_Drive::FileCreate(DOS_File * * file,const char * name,uint16_t /*attributes*/) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}
	
    if (ovlreadonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
	//TODO Check if it exists in the dirCache ? // fix addentry ?  or just double check (ld and overlay)
	//AddEntry looks sound to me.. 
	
	//check if leading part of filename is a deleted directory
	if (check_if_leading_is_deleted(name)) return false;

	FILE* f = create_file_in_overlay(name,"wb+");
	if(!f) {
		if (logoverlay) LOG_MSG("File creation in overlay system failed %s",name);
		return false;
	}
	*file = new localFile(name,f);
	(*file)->flags = OPEN_READWRITE;
	OverlayFile* of = ccc(*file);
	of->overlay_active = true;
	of->flags = OPEN_READWRITE;
	*file = of;
	//create fake name for the drive cache
	char fakename[CROSS_LEN];
	strcpy(fakename,basedir);
	strcat(fakename,name);
	CROSS_FILENAME(fakename);
	dirCache.AddEntry(fakename,true); //add it.
	add_DOSname_to_cache(name);
	remove_deleted_file(name,true);
	return true;
}

void Overlay_Drive::add_DOSname_to_cache(const char* name) {
	for (std::vector<std::string>::const_iterator itc = DOSnames_cache.begin(); itc != DOSnames_cache.end(); ++itc){
		if (!strcasecmp((*itc).c_str(), name)) return;
	}
	DOSnames_cache.push_back(name);
}

void Overlay_Drive::remove_DOSname_from_cache(const char* name) {
	for (std::vector<std::string>::iterator it = DOSnames_cache.begin(); it != DOSnames_cache.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)) { DOSnames_cache.erase(it); return;}
	}

}

bool Overlay_Drive::Sync_leading_dirs(const char* dos_filename){
	const char* lastdir = strrchr(dos_filename,'\\');
	//If there are no directories, return success.
	if (!lastdir) return true; 
	
	const char* leaddir = dos_filename;
	while ( (leaddir=strchr(leaddir,'\\')) != 0) {
		char dirname[CROSS_LEN] = {0};
		strncpy(dirname,dos_filename,leaddir-dos_filename);

		if (logoverlay) LOG_MSG("syncdir: %s",dirname);
		//Test if directory exist in base.
		char dirnamebase[CROSS_LEN] ={0};
		strcpy(dirnamebase,basedir);
		strcat(dirnamebase,dirname);
		CROSS_FILENAME(dirnamebase);
		struct stat basetest;
		if (stat(dirCache.GetExpandName(dirnamebase),&basetest) == 0 && basetest.st_mode & S_IFDIR) {
			if (logoverlay) LOG_MSG("base exists: %s",dirnamebase);
			//Directory exists in base folder.
			//Ensure it exists in overlay as well

			struct stat overlaytest;
			char dirnameoverlay[CROSS_LEN] ={0};
			strcpy(dirnameoverlay,overlaydir);
			strcat(dirnameoverlay,dirname);
			CROSS_FILENAME(dirnameoverlay);
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,dirname));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				strcpy(dirnameoverlay,overlaydir);
				strcat(dirnameoverlay,temp_name);
				CROSS_FILENAME(dirnameoverlay);
			}
			if (stat(dirnameoverlay,&overlaytest) == 0 ) {
				//item exist. Check if it is a folder, if not a folder =>fail!
				if ((overlaytest.st_mode & S_IFDIR) ==0) return false;
			} else {
				//folder does not exist, make it
				if (logoverlay) LOG_MSG("creating %s",dirnameoverlay);
#if defined (WIN32)						/* MS Visual C++ */
				int temp = mkdir(dirnameoverlay);
#else
				int temp = mkdir(dirnameoverlay,0775);
#endif
				if (temp != 0) return false;
			}
		}
		leaddir = leaddir + 1; //Move to next
	} 

	return true;
}

void Overlay_Drive::update_cache(bool read_directory_contents) {
	uint32_t a = GetTicks();
	std::vector<std::string> specials;
	std::vector<std::string> dirnames;
	std::vector<std::string> filenames;
	if (read_directory_contents) {
		//Clear all lists
		DOSnames_cache.clear();
		DOSdirs_cache.clear();
		deleted_files_in_base.clear();
		deleted_paths_in_base.clear();
		//Ensure hiding of the folder that contains the overlay, if it is part of the base folder.
		add_deleted_path(overlap_folder.c_str(), false);
	}

	//Needs later to support stored renames and removals of files existing in the localDrive plane.
	//and by taking in account if the file names are actually already renamed. 
	//and taking in account that a file could have gotten an overlay version and then both need to be removed. 
	//
	//Also what about sequences were a base file gets copied to a working save game and then removed/renamed...
	//copy should be safe as then the link with the original doesn't exist.
	//however the working safe can be rather complicated after a rename and delete..

	//Currently directories existing only in the overlay can not be added to drive cache:
	//1. possible workaround create empty directory in base. Drawback would break the no-touching-of-base.
	//2. double up Addentry to support directories, (and adding . and .. to the newly directory so it counts as cachedin.. and won't be recached, as otherwise
	//   cache will realize we are faking it. 
	//Working on solution 2.

	//Random TODO: Does the root drive under DOS have . and .. ? 

	//This function needs to be called after any localDrive function calling cacheout/deleteentry, as those throw away directories.
	//either do this with a parameter stating the part that needs to be rebuild,(directory) or clear the cache by default and do it all.

	std::vector<std::string>::iterator i;
	std::string::size_type const prefix_lengh = special_prefix.length();
	if (read_directory_contents) {
		void* dirp = opendir(overlaydir);
		if (dirp == NULL) return;
		// Read complete directory
		char dir_name[CROSS_LEN], dir_sname[CROSS_LEN];
		bool is_directory;
		if (read_directory_first(dirp, dir_name, dir_sname, is_directory)) {
			if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(dir_name);
			else if (is_directory) {
				dirnames.push_back(dir_name);
				if (!strlen(dir_sname)) {
					strcpy(dir_sname, dir_name);
					upcase(dir_sname);
				}
				dirnames.push_back(dir_sname);
			} else filenames.push_back(dir_name);
			while (read_directory_next(dirp, dir_name, dir_sname, is_directory)) {
				if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(dir_name);
				else if (is_directory) {
					dirnames.push_back(dir_name);
					if (!strlen(dir_sname)) {
						strcpy(dir_sname, dir_name);
						upcase(dir_sname);
					}
					dirnames.push_back(dir_sname);
				} else filenames.push_back(dir_name);
			}
		}
		closedir(dirp);
		//parse directories to add them.

		for (i = dirnames.begin(); i != dirnames.end(); i+=2) {
			if ((*i) == ".") continue;
			if ((*i) == "..") continue;
			std::string testi(*i);
			std::string::size_type ll = testi.length();
			//TODO: Use the dirname\. and dirname\.. for creating fake directories in the driveCache.
			if( ll >2 && testi[ll-1] == '.' && testi[ll-2] == CROSS_FILESPLIT) continue; 
			if( ll >3 && testi[ll-1] == '.' && testi[ll-2] == '.' && testi[ll-3] == CROSS_FILESPLIT) continue;

#if OVERLAY_DIR
			char tdir[CROSS_LEN],sdir[CROSS_LEN];
			strcpy(tdir,(*i).c_str());
			strcpy(sdir,(*(i+1)).c_str());
			CROSS_DOSFILENAME(tdir);
			bool dir_exists_in_base = localDrive::TestDir(tdir);
#endif

			char dir[CROSS_LEN];
			strcpy(dir,overlaydir);
			strcat(dir,(*i).c_str());
			char dirpush[CROSS_LEN],dirspush[CROSS_LEN];
			strcpy(dirpush,(*i).c_str());
			strcpy(dirspush,(*(i+1)).c_str());
			if (!strlen(dirspush)) {
				strcpy(dirspush, dirpush);
				upcase(dirspush);
			}
			static char end[2] = {CROSS_FILESPLIT,0};
			strcat(dirpush,end); //Linux ?
			strcat(dirspush,end);
			void* dirp = opendir(dir);
			if (dirp == NULL) continue;

#if OVERLAY_DIR
			//Good directory, add to DOSdirs_cache if not existing in localDrive. tested earlier to prevent problems with opendir
			if (!dir_exists_in_base) {
				if (!strlen(sdir)) {
					strcpy(sdir, tdir);
					upcase(sdir);
				}
				add_DOSdir_to_cache(tdir,sdir);
			}
#endif

			std::string backupi(*i);
			// Read complete directory
			char dir_name[CROSS_LEN], dir_sname[CROSS_LEN];
			bool is_directory;
			if (read_directory_first(dirp, dir_name, dir_sname, is_directory)) {
				if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(string(dirpush)+dir_name);
				else if (is_directory) {
					dirnames.push_back(string(dirpush)+dir_name);
					if (!strlen(dir_sname)) {
						strcpy(dir_sname, dir_name);
						upcase(dir_sname);
					}
					dirnames.push_back(string(dirspush)+dir_sname);
				} else filenames.push_back(string(dirpush)+dir_name);
				while (read_directory_next(dirp, dir_name, dir_sname, is_directory)) {
					if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(string(dirpush)+dir_name);
					else if (is_directory) {
						dirnames.push_back(string(dirpush)+dir_name);
						if (!strlen(dir_sname)) {
							strcpy(dir_sname, dir_name);
							upcase(dir_sname);
						}
						dirnames.push_back(string(dirspush)+dir_sname);
					} else filenames.push_back(string(dirspush)+dir_name);
				}
			}
			closedir(dirp);
			for(i = dirnames.begin(); i != dirnames.end(); i+=2) {
				if ( (*i) == backupi) break; //find current directory again, for the next round.
			}
		}
	}

	if (read_directory_contents) {
		for( i = filenames.begin(); i != filenames.end(); ++i) {
			char dosname[CROSS_LEN];
			strcpy(dosname,(*i).c_str());
			//upcase(dosname);  //Should not be really needed, as uppercase in the overlay is a requirement...
			CROSS_DOSFILENAME(dosname);
			if (logoverlay) LOG_MSG("update cache add dosname %s",dosname);
			DOSnames_cache.push_back(dosname);
		}
	}

#if OVERLAY_DIR
	for (i = DOSdirs_cache.begin(); i !=DOSdirs_cache.end(); i+=2) {
		char fakename[CROSS_LEN],sdir[CROSS_LEN],tmp[CROSS_LEN],*p;
		strcpy(fakename,basedir);
		strcat(fakename,(*i).c_str());
		CROSS_FILENAME(fakename);
		strcpy(sdir,(*(i+1)).c_str());
		dirCache.AddEntryDirOverlay(fakename,sdir,true);
		if (strlen(sdir)) {
			strcpy(tmp,(*(i+1)).c_str());
			p=strrchr(tmp, '\\');
			if (p==NULL) *(i+1)=std::string(sdir);
			else {
				*p=0;
				for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it<i && it != DOSdirs_cache.end(); it+=2) {
					if (!strcasecmp((*it).c_str(), tmp)) {
						strcpy(tmp, (*(it+1)).c_str());
						break;
					}
				}
				strcat(tmp,"\\");
				strcat(tmp,sdir);
				*(i+1)=std::string(tmp);
			}
		}
	}
#endif

	for (i = DOSnames_cache.begin(); i != DOSnames_cache.end(); ++i) {
		char fakename[CROSS_LEN];
		strcpy(fakename,basedir);
		strcat(fakename,(*i).c_str());
		CROSS_FILENAME(fakename);
		dirCache.AddEntry(fakename,true);
	}

	if (read_directory_contents) {
		for (i = specials.begin(); i != specials.end(); ++i) {
			//Specials look like this $DBOVERLAY_YYY_FILENAME.EXT or DIRNAME[\/]$DBOVERLAY_YYY_FILENAME.EXT where 
			//YYY is the operation involved. Currently only DEL is supported.
			//DEL = file marked as deleted, (but exists in localDrive!)
			std::string name(*i);
			std::string special_dir("");
			std::string special_file("");
			std::string special_operation("");
			std::string::size_type s = name.find(special_prefix);
			if (s == std::string::npos) continue;
			if (s) {
				special_dir = name.substr(0,s);
				name.erase(0,s);
			}
			name.erase(0,special_prefix.length()+1); //Erase $DBOVERLAY_
			s = name.find('_');
			if (s == std::string::npos || s == 0) continue;
			special_operation = name.substr(0,s);
			name.erase(0,s + 1);
			special_file = name;
			if (special_file.length() == 0) continue;
			if (special_operation == "DEL") {
				name = special_dir + special_file;
				//CROSS_DOSFILENAME for strings:
				while ( (s = name.find('/')) != std::string::npos) name.replace(s,1,"\\");
				
				add_deleted_file(name.c_str(),false);
			} else if (special_operation == "RMD") {
				name = special_dir + special_file;
				//CROSS_DOSFILENAME for strings:
				while ( (s = name.find('/')) != std::string::npos) name.replace(s,1,"\\");
				add_deleted_path(name.c_str(),false);

			} else {
				if (logoverlay) LOG_MSG("unsupported operation %s on %s",special_operation.c_str(),(*i).c_str());
			}

		}
	}
	if (logoverlay) LOG_MSG("OPTIMISE: update cache took %d",GetTicks()-a);
}

bool Overlay_Drive::FindNext(DOS_DTA & dta) {

	char * dir_ent, *ldir_ent;
	ht_stat_t stat_block;
	char full_name[CROSS_LEN], lfull_name[LFN_NAMELENGTH+1];
	char dir_entcopy[CROSS_LEN], ldir_entcopy[CROSS_LEN];

	uint8_t srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	uint8_t find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern,uselfn);
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
	
	//First try overlay:
	char ovname[CROSS_LEN];
	char relativename[CROSS_LEN];
	strcpy(relativename,srchInfo[id].srch_dir);
	//strip off basedir: //TODO cleanup
	strcpy(ovname,overlaydir);
	char* prel = lfull_name + strlen(basedir);

	char preldos[CROSS_LEN];
	strcpy(preldos,prel);
	CROSS_DOSFILENAME(preldos);
	strcat(ovname,prel);
	bool statok = false;
	const host_cnv_char_t* host_name=NULL;
	if (!is_deleted_file(preldos)) {
		host_name = CodePageGuestToHost(ovname);
		if (host_name != NULL) statok = ht_stat(host_name,&stat_block)==0;
		if (!statok) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,prel));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				strcpy(ovname,GetCrossedName(overlaydir,temp_name));
				host_name = CodePageGuestToHost(ovname);
				if (host_name != NULL) statok = ht_stat(host_name,&stat_block)==0;
			}
		}
	}

	if (statok) {
		if (logoverlay) LOG_MSG("using overlay data for %s : %s",uselfn?lfull_name:full_name, ovname);
	} else {
		strcpy(preldos,prel);
		CROSS_DOSFILENAME(preldos);
		if (is_deleted_file(preldos)) { //dir.. maybe lower or keep it as is TODO
			if (logoverlay) LOG_MSG("skipping deleted file %s %s %s",preldos,uselfn?lfull_name:full_name,ovname);
			goto again;
		}
		const host_cnv_char_t* host_name = CodePageGuestToHost(dirCache.GetExpandName(lfull_name));
		if (ht_stat(host_name,&stat_block)!=0) {
			if (logoverlay) LOG_MSG("stat failed for %s . This should not happen.",dirCache.GetExpandName(uselfn?lfull_name:full_name));
			goto again;//No symlinks and such
		}
	}

	if(stat_block.st_mode & S_IFDIR) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=0;
#if defined (WIN32)
	Bitu attribs = host_name==NULL?INVALID_FILE_ATTRIBUTES:GetFileAttributesW(host_name);
	if (attribs != INVALID_FILE_ATTRIBUTES)
		find_attr|=attribs&0x3f;
#else
	if (!(find_attr&DOS_ATTR_DIRECTORY)) find_attr|=DOS_ATTR_ARCHIVE;
	if(!(stat_block.st_mode & S_IWUSR)) find_attr|=DOS_ATTR_READ_ONLY;
#endif
 	if (~srch_attr & find_attr & DOS_ATTR_DIRECTORY) goto again;
	
	/* file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII], lfind_name[LFN_NAMELENGTH+1];
	uint16_t find_date,find_time;uint32_t find_size;

	if(strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_entcopy);
		upcase(find_name);
	}
	strcpy(lfind_name,ldir_entcopy);
    lfind_name[LFN_NAMELENGTH]=0;

	find_size=(uint32_t) stat_block.st_size;
	struct tm *time;
	if((time=localtime(&stat_block.st_mtime))!=0){
		find_date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
		find_time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool Overlay_Drive::FileUnlink(const char * name) {
    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

//TODO check the basedir for file existence in order if we need to add the file to deleted file list.
	uint32_t a = GetTicks();
	if (logoverlay) LOG_MSG("calling unlink on %s",name);
	char basename[CROSS_LEN];
	strcpy(basename,basedir);
	strcat(basename,name);
	CROSS_FILENAME(basename);

	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	
	bool removed=false;
	const host_cnv_char_t* host_name;
	struct stat temp_stat;
	if (stat(overlayname,&temp_stat)) {
		char* temp_name = dirCache.GetExpandName(basename);
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			char overtmpname[CROSS_LEN];
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(overtmpname,overlaydir);
			strcat(overtmpname,temp_name);
			CROSS_FILENAME(overtmpname);
			host_name = CodePageGuestToHost(overtmpname);
			if (host_name != NULL && ht_unlink(host_name)==0) removed=true;
		}
	}
//	char *fullname = dirCache.GetExpandName(newname);

	if (!removed&&unlink(overlayname)) {
		//Unlink failed for some reason try finding it.
		ht_stat_t status;
		host_name = CodePageGuestToHost(overlayname);
		if(host_name==NULL || ht_stat(host_name,&status)) {
			//file not found in overlay, check the basedrive
			//Check if file not already deleted 
			if (is_deleted_file(name)) return false;

			char *fullname = dirCache.GetExpandName(basename);
			host_name = CodePageGuestToHost(fullname);
			if (host_name==NULL || ht_stat(host_name,&status)) return false; // File not found in either, return file false.
			//File does exist in normal drive.
			//Maybe do something with the drive_cache.
			add_deleted_file(name,true);
			return true;
//			E_Exit("trying to remove existing non-overlay file %s",name);
		}
		FILE* file_writable = fopen_wrap(overlayname,"rb+");
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
		if (unlink(overlayname) == 0) { //Overlay file removed
			//Mark basefile as deleted if it exists:
			if (localDrive::FileExists(name)) add_deleted_file(name,true);
			remove_DOSname_from_cache(name); //Should be an else ? although better safe than sorry.
			//Handle this better
			dirCache.DeleteEntry(basename);
			dirCache.EmptyCache();
			update_cache(false);
			//Check if it exists in the base dir as well
			
			return true;
		}
		return false;
	} else { //Removed from overlay.
		//TODO IF it exists in the basedir: and more locations above.
		if (localDrive::FileExists(name)) add_deleted_file(name,true);
		remove_DOSname_from_cache(name);
		//TODODO remove from the update_cache cache as well
		//Handle this better
		//Check if it exists in the base dir as well
		dirCache.DeleteEntry(basename);

		dirCache.EmptyCache();
		update_cache(false);
		if (logoverlay) LOG_MSG("OPTIMISE: unlink took %d",GetTicks()-a);
		return true;
	}
}

bool Overlay_Drive::SetFileAttr(const char * name,uint16_t attr) {
	char overlayname[CROSS_LEN], tmp[CROSS_LEN], overtmpname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(tmp, name);
	char *q=strrchr(tmp, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tmp=0;
	char *p=strrchr(temp_name, '\\');
	if (p!=NULL)
		strcat(tmp,p+1);
	else
		strcat(tmp,temp_name);
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir)))
		strcpy(tmp,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
	strcpy(overtmpname,overlaydir);
	strcat(overtmpname,tmp);
	CROSS_FILENAME(overtmpname);

	ht_stat_t status;
	const host_cnv_char_t* host_name;
	bool success=false;
#if defined (WIN32)
    host_name = CodePageGuestToHost(overtmpname);
    if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
	if (!success) {
		host_name = CodePageGuestToHost(overlayname);
		if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
	}
#else
	if (ht_stat(overtmpname,&status)==0 || ht_stat(overlayname,&status)==0) {
		if (attr & (DOS_ATTR_SYSTEM|DOS_ATTR_HIDDEN))
			LOG(LOG_DOSMISC,LOG_WARN)("%s: Application attempted to set system or hidden attributes for '%s' which is ignored for local drives",__FUNCTION__,overlayname);

		if (attr & DOS_ATTR_READ_ONLY)
			status.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
		else
			status.st_mode |=  S_IWUSR;
		if (chmod(overtmpname,status.st_mode) >= 0 || chmod(overlayname,status.st_mode) >= 0)
			success=true;
	}
#endif
	if (success) {
		dirCache.EmptyCache();
		update_cache(false);
		return true;
	}

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	temp_name = dirCache.GetExpandName(newname);
	host_name = CodePageGuestToHost(temp_name);

	bool created=false;
	if (host_name != NULL && ht_stat(host_name,&status)==0 && (status.st_mode & S_IFDIR)) {
		host_name = CodePageGuestToHost(overtmpname);
		int temp=-1;
		if (host_name!=NULL) {
#if defined (WIN32)
			temp=_wmkdir(host_name);
#else
			temp=mkdir(host_name,0775);
#endif
		}
		if (temp==0) created=true;
	} else {
		FILE * hand;
		host_name = CodePageGuestToHost(temp_name);
#ifdef host_cnv_use_wchar
		if (host_name!=NULL)
			hand = _wfopen(host_name,_HT("rb"));
		else
#endif
			hand = fopen_wrap(temp_name,"rb");
		if (hand) {
			if (logoverlay) LOG_MSG("overlay file opened %s",temp_name);
			FILE * layfile=NULL;
			host_name = CodePageGuestToHost(overtmpname);
#ifdef host_cnv_use_wchar
			if (host_name!=NULL) layfile=_wfopen(host_name,_HT("wb"));
#endif
			if (layfile==NULL) layfile=fopen_wrap(overlayname,"wb");
			int numr,numw;
			char buffer[1000];
			while(feof(hand)==0) {
				if((numr=(int)fread(buffer,1,1000,hand))!=1000){
					if(ferror(hand)!=0){
						fclose(hand);
						fclose(layfile);
						return false;
					} else if(feof(hand)!=0) { }
				}
				if((numw=(int)fwrite(buffer,1,numr,layfile))!=numr){
						fclose(hand);
						fclose(layfile);
						return false;
				}
			}
			fclose(hand);
			fclose(layfile);
			created=true;
		}
	}
	if (created) {
#if defined (WIN32)
		host_name = CodePageGuestToHost(overtmpname);
		if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
		if (!success) {
			host_name = CodePageGuestToHost(overlayname);
			if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
		}
#else
		if (ht_stat(overtmpname,&status)==0 || ht_stat(overlayname,&status)==0) {
			if (attr & (DOS_ATTR_SYSTEM|DOS_ATTR_HIDDEN))
				LOG(LOG_DOSMISC,LOG_WARN)("%s: Application attempted to set system or hidden attributes for '%s' which is ignored for local drives",__FUNCTION__,overlayname);

			if (attr & DOS_ATTR_READ_ONLY)
				status.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
			else
				status.st_mode |=  S_IWUSR;
			if (chmod(overtmpname,status.st_mode) >= 0 || chmod(overlayname,status.st_mode) >= 0)
				success=true;
		}
#endif
		if (success) {
			dirCache.EmptyCache();
			update_cache(false);
			return true;
		}
	}
	return false;
}

bool Overlay_Drive::GetFileAttr(const char * name,uint16_t * attr) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	char overlayname[CROSS_LEN], tmp[CROSS_LEN], overtmpname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(tmp, name);
	char *q=strrchr(tmp, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tmp=0;
	char *p=strrchr(temp_name, '\\');
	if (p!=NULL)
		strcat(tmp,p+1);
	else
		strcat(tmp,temp_name);
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir)))
		strcpy(tmp,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
	strcpy(overtmpname,overlaydir);
	strcat(overtmpname,tmp);
	CROSS_FILENAME(overtmpname);

#if defined (WIN32)
	Bitu attribs = INVALID_FILE_ATTRIBUTES;
    const host_cnv_char_t* host_name = CodePageGuestToHost(overtmpname);
    if (host_name != NULL) attribs = GetFileAttributesW(host_name);
	if (attribs == INVALID_FILE_ATTRIBUTES) {
		host_name = CodePageGuestToHost(overlayname);
		if (host_name != NULL) attribs = GetFileAttributesW(host_name);
	}
	if (attribs != INVALID_FILE_ATTRIBUTES) {
		*attr = attribs&0x3f;
		return true;
	}
#else
	ht_stat_t status;
	if (ht_stat(overtmpname,&status)==0 || ht_stat(overlayname,&status)==0) {
		*attr=status.st_mode & S_IFDIR?0:DOS_ATTR_ARCHIVE;
		if(status.st_mode & S_IFDIR) *attr|=DOS_ATTR_DIRECTORY;
		if(!(status.st_mode & S_IWUSR)) *attr|=DOS_ATTR_READ_ONLY;
		return true;
	}
#endif
	//Maybe check for deleted path as well
	if (is_deleted_file(name)) {
		*attr = 0;
		return false;
	}
	return localDrive::GetFileAttr(name,attr);
}


void Overlay_Drive::add_deleted_file(const char* name,bool create_on_disk) {
	char tname[CROSS_LEN];
	strcpy(tname,basedir);
	strcat(tname,name);
	CROSS_FILENAME(tname);
	char* temp_name = dirCache.GetExpandName(tname);
	strcpy(tname, name);
	char *q=strrchr(tname, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tname=0;
	char *p=strrchr(temp_name, '\\');
	if (p!=NULL)
		strcat(tname,p+1);
	else
		strcat(tname,temp_name);
	if (!is_deleted_file(tname)) {
		deleted_files_in_base.push_back(tname);
		if (create_on_disk) add_special_file_to_disk(tname, "DEL");
	}
}

void Overlay_Drive::add_special_file_to_disk(const char* dosname, const char* operation) {
	std::string name = create_filename_of_special_operation(dosname, operation);
	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name.c_str());
	CROSS_FILENAME(overlayname);
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	FILE* f;
	if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
		f = _wfopen(host_name,_HT("wb+"));
#else
		f = fopen_wrap(host_name,"wb+");
#endif
	}
	else {
		f = fopen_wrap(overlayname,"wb+");
	}

	if (!f) {
		Sync_leading_dirs(dosname);
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name.c_str()));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(overlayname,overlaydir);
			strcat(overlayname,temp_name);
			CROSS_FILENAME(overlayname);
		}
		host_name = CodePageGuestToHost(overlayname);
		if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
			f = _wfopen(host_name,_HT("wb+"));
#else
			f = fopen_wrap(host_name,"wb+");
#endif
		}
		else {
			f = fopen_wrap(overlayname,"wb+");
		}
	}
	if (!f) E_Exit("Failed creation of %s",overlayname);
	char buf[5] = {'e','m','p','t','y'};
	fwrite(buf,5,1,f);
	fclose(f);
}

void Overlay_Drive::remove_special_file_from_disk(const char* dosname, const char* operation) {
	std::string name = create_filename_of_special_operation(dosname,operation);
	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name.c_str());
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name.c_str()));
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
		strcpy(overlayname,overlaydir);
		strcat(overlayname,temp_name);
		CROSS_FILENAME(overlayname);
	}
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	if ((host_name == NULL || ht_unlink(host_name) != 0) && unlink(overlayname) != 0)
		E_Exit("Failed removal of %s",overlayname);
}

std::string Overlay_Drive::create_filename_of_special_operation(const char* dosname, const char* operation) {
	std::string res(dosname);
	std::string::size_type s = res.rfind('\\'); //CHECK DOS or host endings.... on update_cache
	if (s == std::string::npos) s = 0; else s++;
	std::string oper = special_prefix + "_" + operation + "_";
	res.insert(s,oper);
	return res;
}


bool Overlay_Drive::is_dir_only_in_overlay(const char* name) {
	if (!name || !*name) return false;
	if (DOSdirs_cache.empty()) return false;
	char fname[CROSS_LEN];
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2) {
		if (!strcasecmp((*it).c_str(), name)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))||((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), name))) return true;
	}
	return false;
}

bool Overlay_Drive::is_deleted_file(const char* name) {
	if (!name || !*name) return false;
	if (deleted_files_in_base.empty()) return false;
	char tname[CROSS_LEN],fname[CROSS_LEN];
	strcpy(tname,basedir);
	strcat(tname,name);
	CROSS_FILENAME(tname);
	char* temp_name = dirCache.GetExpandName(tname);
	strcpy(tname, name);
	char *q=strrchr(tname, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tname=0;
	char *p=strrchr(temp_name, '\\');
	if (p!=NULL)
		strcat(tname,p+1);
	else
		strcat(tname,temp_name);
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = deleted_files_in_base.begin(); it != deleted_files_in_base.end(); it++) {
		if (!strcasecmp((*it).c_str(), name)||!strcasecmp((*it).c_str(), tname)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))) return true;
	}
	return false;
}

void Overlay_Drive::add_DOSdir_to_cache(const char* name, const char *sname) {
	if (!name || !*name ) return; //Skip empty file.
	if (logoverlay) LOG_MSG("Adding name to overlay_only_dir_cache %s",name);
	if (!is_dir_only_in_overlay(name)) {
		DOSdirs_cache.push_back(name);
		DOSdirs_cache.push_back(sname);
	}
}

void Overlay_Drive::remove_DOSdir_from_cache(const char* name) {
	char fname[CROSS_LEN];
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2) {
		if (!strcasecmp((*it).c_str(), name)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))||((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), name))) {
			DOSdirs_cache.erase(it+1);
			DOSdirs_cache.erase(it);
			return;
		}
	}
}

void Overlay_Drive::remove_deleted_file(const char* name,bool create_on_disk) {
	char tname[CROSS_LEN],fname[CROSS_LEN];
	strcpy(tname,basedir);
	strcat(tname,name);
	CROSS_FILENAME(tname);
	char* temp_name = dirCache.GetExpandName(tname);
	strcpy(tname, name);
	char *q=strrchr(tname, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tname=0;
	char *p=strrchr(temp_name, '\\');
	if (p!=NULL)
		strcat(tname,p+1);
	else
		strcat(tname,temp_name);
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = deleted_files_in_base.begin(); it != deleted_files_in_base.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)||!strcasecmp((*it).c_str(), tname)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))) {
			deleted_files_in_base.erase(it);
			if (create_on_disk) remove_special_file_from_disk(name, "DEL");
			return;
		}
	}
}
void Overlay_Drive::add_deleted_path(const char* name, bool create_on_disk) {
	if (!name || !*name ) return; //Skip empty file.
	if (!is_deleted_path(name)) {
		deleted_paths_in_base.push_back(name);
		//Add it to deleted files as well, so it gets skipped in FindNext. 
		//Maybe revise that.
		if (create_on_disk) add_special_file_to_disk(name,"RMD");
		add_deleted_file(name,false);
	}
}
bool Overlay_Drive::is_deleted_path(const char* name) {
	if (!name || !*name) return false;
	if (deleted_paths_in_base.empty()) return false;
	std::string sname(name);
	std::string::size_type namelen = sname.length();;
	for(std::vector<std::string>::iterator it = deleted_paths_in_base.begin(); it != deleted_paths_in_base.end(); ++it) {
		std::string::size_type blockedlen = (*it).length();
		if (namelen < blockedlen) continue;
		//See if input starts with name. 
		std::string::size_type n = sname.find(*it);
		if (n == 0 && ((namelen == blockedlen) || *(name + blockedlen) == '\\' )) return true;
	}
	return false;
}

void Overlay_Drive::remove_deleted_path(const char* name, bool create_on_disk) {
	for(std::vector<std::string>::iterator it = deleted_paths_in_base.begin(); it != deleted_paths_in_base.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)) {
			deleted_paths_in_base.erase(it);
			remove_deleted_file(name,false); //Rethink maybe.
			if (create_on_disk) remove_special_file_from_disk(name,"RMD");
			break;
		}
	}
}
bool Overlay_Drive::check_if_leading_is_deleted(const char* name){
	const char* dname = strrchr(name,'\\');
	if (dname != NULL) {
		char dirname[CROSS_LEN];
		strncpy(dirname,name,dname - name);
		dirname[dname - name] = 0;
		if (is_deleted_path(dirname)) return true;
	}
	return false;
}

bool Overlay_Drive::FileExists(const char* name) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	ht_stat_t temp_stat;
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	if (host_name != NULL && ht_stat(host_name ,&temp_stat)==0 && (temp_stat.st_mode & S_IFDIR)==0) return true;
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(overlayname,overlaydir);
		strcat(overlayname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_FILENAME(overlayname);
		host_name = CodePageGuestToHost(overlayname);
		if(host_name != NULL && ht_stat(host_name,&temp_stat)==0 && (temp_stat.st_mode & S_IFDIR)==0) return true;
	}
	
	if (is_deleted_file(name)) return false;

	return localDrive::FileExists(name);
}

bool Overlay_Drive::Rename(const char * oldname,const char * newname) {
    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	//TODO with cache function!
	//Tricky function.
	//Renaming directories is currently not fully supported, due the drive_cache not handling that smoothly.
	//So oldname is directory => exit unless it is only in overlay.
	//If oldname is on overlay => simple rename.
	//if oldname is on base => copy file to overlay with new name and mark old file as deleted. 
	//More advanced version. keep track of the file being renamed in order to detect that the file is being renamed back. 
	
	char tmp[CROSS_LEN];
	host_cnv_char_t host_nameold[CROSS_LEN], host_namenew[CROSS_LEN];
	uint16_t attr = 0;
	if (!GetFileAttr(oldname,&attr)) E_Exit("rename, but source doesn't exist, should not happen %s",oldname);
	ht_stat_t temp_stat;
	if (attr&DOS_ATTR_DIRECTORY) {
		//See if the directory exists only in the overlay, then it should be possible.
#if OVERLAY_DIR
		if (localDrive::TestDir(oldname)) {
			LOG_MSG("Overlay: renaming base directory %s to %s not yet supported", oldname,newname);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
#endif
		char overlaynameold[CROSS_LEN];
		strcpy(overlaynameold,overlaydir);
		strcat(overlaynameold,oldname);
		CROSS_FILENAME(overlaynameold);
#ifdef host_cnv_use_wchar
		wcscpy
#else
		strcpy
#endif
		(host_nameold, CodePageGuestToHost(overlaynameold));

		char overlaynamenew[CROSS_LEN];
		strcpy(overlaynamenew,overlaydir);
		strcat(overlaynamenew,newname);
		CROSS_FILENAME(overlaynamenew);
#ifdef host_cnv_use_wchar
		wcscpy
#else
		strcpy
#endif
		(host_namenew, CodePageGuestToHost(overlaynamenew));

		if (ht_stat(host_nameold,&temp_stat)) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,oldname));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				strcpy(overlaynameold,GetCrossedName(overlaydir,temp_name));
#ifdef host_cnv_use_wchar
				wcscpy
#else
				strcpy
#endif
				(host_nameold, CodePageGuestToHost(overlaynameold));
			}
			strcpy(tmp,newname);
			char *p=strrchr(tmp,'\\'), ndir[CROSS_LEN];
			if (p!=NULL) {
				*p=0;
				temp_name=dirCache.GetExpandName(GetCrossedName(basedir,tmp));
				if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
					strcpy(ndir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
					strcat(ndir,"\\");
					strcat(ndir,p+1);
					strcpy(overlaynamenew,GetCrossedName(overlaydir,ndir));
#ifdef host_cnv_use_wchar
					wcscpy
#else
					strcpy
#endif
					(host_namenew, CodePageGuestToHost(overlaynamenew));
				}
			}
		}
		int temp=-1;
#ifdef host_cnv_use_wchar
		temp = _wrename(host_nameold,host_namenew);
#else
		temp = rename(host_nameold,host_namenew);
#endif
		if (temp==0) {
			dirCache.EmptyCache();
			update_cache(true);
			return true;
		}
		LOG_MSG("Overlay: renaming overlay directory %s to %s not yet supported",oldname,newname); //TODO
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	uint32_t a = GetTicks();
	//First generate overlay names.
	char overlaynameold[CROSS_LEN];
	strcpy(overlaynameold,overlaydir);
	strcat(overlaynameold,oldname);
	CROSS_FILENAME(overlaynameold);
#ifdef host_cnv_use_wchar
	wcscpy
#else
	strcpy
#endif
	(host_nameold, CodePageGuestToHost(overlaynameold));

	char overlaynamenew[CROSS_LEN];
	strcpy(overlaynamenew,overlaydir);
	strcat(overlaynamenew,newname);
	CROSS_FILENAME(overlaynamenew);
#ifdef host_cnv_use_wchar
	wcscpy
#else
	strcpy
#endif
	(host_namenew, CodePageGuestToHost(overlaynamenew));
	if (ht_stat(host_nameold,&temp_stat)) {
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,oldname));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(overlaynameold,GetCrossedName(overlaydir,temp_name));
#ifdef host_cnv_use_wchar
			wcscpy
#else
			strcpy
#endif
			(host_nameold, CodePageGuestToHost(overlaynameold));
		}
		strcpy(tmp,newname);
		char *p=strrchr(tmp,'\\'), ndir[CROSS_LEN];
		if (p!=NULL) {
			*p=0;
			temp_name=dirCache.GetExpandName(GetCrossedName(basedir,tmp));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				strcpy(ndir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
				strcat(ndir,"\\");
				strcat(ndir,p+1);
				strcpy(overlaynamenew,GetCrossedName(overlaydir,ndir));
#ifdef host_cnv_use_wchar
				wcscpy
#else
				strcpy
#endif
				(host_namenew, CodePageGuestToHost(overlaynamenew));
			}
		}
	}

	//No need to check if the original is marked as deleted, as GetFileAttr would fail if it did.

	//Check if overlay source file exists
	int temp = -1; 
	if (ht_stat(host_nameold,&temp_stat) == 0) {
		//Simple rename
#ifdef host_cnv_use_wchar
		temp = _wrename(host_nameold,host_namenew);
#else
		temp = rename(host_nameold,host_namenew);
#endif
		//TODO CHECK if base has a file with same oldname!!!!! if it does mark it as deleted!!
		if (localDrive::FileExists(oldname)) add_deleted_file(oldname,true);
	} else {
		uint32_t aa = GetTicks();
		//File exists in the basedrive. Make a copy and mark old one as deleted.
		char newold[CROSS_LEN];
		strcpy(newold,basedir);
		strcat(newold,oldname);
		CROSS_FILENAME(newold);
		dirCache.ExpandName(newold);
		const host_cnv_char_t* host_name = CodePageGuestToHost(newold);
		FILE* o;
		if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
			o = _wfopen(host_name,_HT("rb"));
#else
			o = fopen_wrap(host_name,"rb");
#endif
		}
		else {
			o = fopen_wrap(newold,"rb");
		}
		if (!o) return false;
		FILE* n = create_file_in_overlay(newname,"wb+");
		if (!n) {fclose(o); return false;}
		char buffer[BUFSIZ];
		size_t s;
		while ( (s = fread(buffer,1,BUFSIZ,o)) ) fwrite(buffer, 1, s, n);
		fclose(o); fclose(n);

		//File copied.
		//Mark old file as deleted
		add_deleted_file(oldname,true);
		temp =0; //success
		if (logoverlay) LOG_MSG("OPTIMISE: update rename with copy took %d",GetTicks()-aa);

	}
	if (temp ==0) {
		//handle the drive_cache (a bit better)
		//Ensure that the file is not marked as deleted anymore.
		if (is_deleted_file(newname)) remove_deleted_file(newname,true);
		dirCache.EmptyCache();
		update_cache(true);
		if (logoverlay) LOG_MSG("OPTIMISE: rename took %d",GetTicks()-a);

	}
	return (temp==0);

}

bool Overlay_Drive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	if (logoverlay) LOG_MSG("FindFirst in %s",_dir);
	
	if (is_deleted_path(_dir)) {
		//No accidental listing of files in there.
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	if (*_dir) {
		char newname[CROSS_LEN], tmp[CROSS_LEN];
		strcpy(newname,overlaydir);
		strcat(newname,_dir);
		CROSS_FILENAME(newname);
		struct stat temp_stat;
		if (stat(newname,&temp_stat)) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,_dir));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				return localDrive::FindFirst(temp_name,dta,fcb_findfirst);
			}
			strcpy(tmp, _dir);
			bool found=false;
			for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2) {
				if ((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), tmp)) {
					found=true;
					strcpy(tmp, (*it).c_str());
					break;
				}
			}
			if (found) {
				return localDrive::FindFirst(tmp,dta,fcb_findfirst);
			}
		}
	}
	return localDrive::FindFirst(_dir,dta,fcb_findfirst);
}

bool Overlay_Drive::FileStat(const char* name, FileStat_Block * const stat_block) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	char overlayname[CROSS_LEN], tmp[CROSS_LEN], overtmpname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	const host_cnv_char_t* host_name;
	ht_stat_t temp_stat;
	bool success=false;
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(overtmpname,overlaydir);
		strcat(overtmpname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_FILENAME(overtmpname);
		host_name = CodePageGuestToHost(overtmpname);
		if (host_name != NULL && ht_stat(host_name,&temp_stat)==0) success=true;
	}
	if (!success) {
		host_name = CodePageGuestToHost(overlayname);
		if (host_name != NULL && ht_stat(host_name,&temp_stat) == 0) success=true;
	}
	if (!success) {
		strcpy(tmp,name);
		char *p=strrchr(tmp, '\\'), *q=strrchr(temp_name, '\\');
		if (p!=NULL&&q!=NULL) {
			*p=0;
			for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2)
				if ((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), tmp)) {
					strcpy(tmp, (*it).c_str());
					break;
				}
			strcat(tmp, "\\");
			strcat(tmp, q+1);
		}
		strcpy(overtmpname,overlaydir);
		strcat(overtmpname,tmp);
		CROSS_FILENAME(overtmpname);
		host_name = CodePageGuestToHost(overtmpname);
		if (host_name != NULL && ht_stat(host_name,&temp_stat)==0) success=true;
	}
	if(!success) {
		if (is_deleted_file(name)) return false;
		return localDrive::FileStat(name,stat_block);
	}
	
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&temp_stat.st_mtime))!=0) {
		stat_block->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		stat_block->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
			// ... But this function is not used at the moment.
	}
	stat_block->size=(uint32_t)temp_stat.st_size;
	return true;
}

Bits Overlay_Drive::UnMount(void) { 
	delete this;
	return 0; 
}
void Overlay_Drive::EmptyCache(void){
	localDrive::EmptyCache();
	update_cache(true);//lets rebuild it.
}
