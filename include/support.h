/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#ifndef DOSBOX_SUPPORT_H
#define DOSBOX_SUPPORT_H

#include <string.h>
#include <string>
#include <ctype.h>
#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

#if defined (_MSC_VER)						/* MS Visual C++ */
#define	strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) _strnicmp(a,b,n)
#endif

#define safe_strncpy(a,b,n) do { strncpy((a),(b),(size_t)((n)-1)); (a)[(size_t)((n)-1)] = 0; } while (0)

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

void strreplace(char * str,char o,char n);
char *ltrim(char *str);
char *rtrim(char *str);
char *trim(char * str);
char * upcase(char * str);
char * lowcase(char * str);
char * StripArg(char *&cmd);
bool ScanCMDBool(char * cmd,char const * const check);
char * ScanCMDRemain(char * cmd);
char * StripWord(char *&line);
Bits ConvDecWord(char * word);
Bits ConvHexWord(char * word);

void trim(std::string& str);
void upcase(std::string &str);
void lowcase(std::string &str);

static inline bool is_power_of_2(Bitu val) {
	return (val != 0) && ((val&(val-1)) == 0);
	/* To explain: if val is a power of 2, then only one bit is set.
	 * Decrementing val would change that one bit to 0, and all bits to the right to 1.
	 * Example:
	 *
	 * Power of 2: val = 1024
	 *
	 *      1024 = 0000 0100 0000 0000
	 *  AND 1023 = 0000 0011 1111 1111
	 *  ------------------------------
	 *         0 = 0000 0000 0000 0000
	 *
	 * Non-power of 2: val = 713
	 *
	 *       713 = 0000 0010 1100 1001
	 *   AND 712 = 0000 0010 1100 1000
	 *  ------------------------------
	 *       712 = 0000 0010 1100 1000
	 *
	 * See how that works?
	 *
	 * For more info see https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2*/
}

#endif
