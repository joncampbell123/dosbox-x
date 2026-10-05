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
 */

/* UTF-8 file names for DOS programs: the AMIS provider "DOS-UTF8" "NAMES" and the escape form of the names that the guest
 * code page cannot hold (config option "utf8 file names" of [dos]).
 *
 * The directory cache and the DOS file layer of DOSBox-X keep every name in the guest code page. A host file whose name has a
 * character that the code page lacks used to be skipped. With the option on, such a character is kept in the name as the escape
 * "{U+XXXX}" (4 to 6 hexadecimal digits of the code point): the conversion from the host to the guest turns it into the escape,
 * the conversion back turns the escape into the character, so the file is visible to every program (under the escaped name) and
 * can be opened, created, renamed and deleted. A host name that has the text "{U+" itself gets its "{" escaped as "{U+007B}".
 *
 * A DOS program that knows UTF-8 asks for it with the AMIS provider (AL=10h BX=65001; the specification is UTF8NAMES.md of
 * github.com/unxed/go2dos): the long names of the INT 21h AH=71h functions are then UTF-8 for this process (the names that the
 * program gives are turned into the guest form, the names that it gets are turned from it). The mode belongs to the process
 * (the current PSP) and ends with it. Short names are not changed: they stay in the OEM code page, so that they keep meaning the
 * same file for every program. */

#include "dosbox.h"
#include "dos_inc.h"
#include "cross.h"
#include "control.h"
#include "regs.h"
#include "support.h"
#include "bios.h"
#include "mem.h"

#define DOSNAMEBUF_UTF8 256

#include <set>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

bool dos_utf8names = false;                       /* [dos] utf8 file names */

static std::set<uint16_t> utf8_names_psp;

bool DOS_UTF8Names_Available(void) {
	return dos_utf8names && !control->SecureMode();
}

bool DOS_UTF8NamesMode(void) {
	return dos_utf8names && utf8_names_psp.count(dos.psp()) != 0;
}

void DOS_UTF8Names_ProcessEnded(uint16_t psp) {
	utf8_names_psp.erase(psp);
}

bool DOS_UTF8Names_Call(uint8_t fn) {
	const uint16_t cur = DOS_UTF8NamesMode() ? 65001 : 0;
	switch (fn) {
		case 0x10:
			if (reg_bx == 65001) utf8_names_psp.insert(dos.psp());
			else if (reg_bx == 0) utf8_names_psp.erase(dos.psp());
			else { reg_al = 0x00; return true; }   /* not supported, nothing changed */
			reg_al = 0xFF;
			reg_bx = cur;                          /* the previous setting */
			return true;
		case 0x11:
			reg_al = 0xFF;
			reg_bx = cur;
			return true;
	}
	return false;
}

/* ---- UTF-8 and UTF-16 ---------------------------------------------------------------------------------------------------- */

/* one code point of the UTF-8 text at s (NUL terminated); the number of bytes in *used; a byte that is not valid UTF-8 is 0xFFFD */
static uint32_t Utf8At(const char *s, size_t *used) {
	const unsigned char c = (unsigned char)s[0];
	size_t n = 0;
	uint32_t cp = 0;
	if (c < 0x80) { *used = 1; return c; }
	else if (c >= 0xC2 && c <= 0xDF) { n = 1; cp = c & 0x1F; }
	else if (c >= 0xE0 && c <= 0xEF) { n = 2; cp = c & 0x0F; }
	else if (c >= 0xF0 && c <= 0xF4) { n = 3; cp = c & 0x07; }
	else { *used = 1; return 0xFFFD; }
	for (size_t k = 1; k <= n; k++) {
		const unsigned char t = (unsigned char)s[k];
		if ((t & 0xC0) != 0x80) { *used = 1; return 0xFFFD; }
		cp = (cp << 6) | (t & 0x3F);
	}
	if ((n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) { *used = 1; return 0xFFFD; }
	*used = n + 1;
	return cp;
}

static void Utf8Put(std::string &out, uint32_t cp) {
	if (cp < 0x80) out += (char)cp;
	else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
	else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
	else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

bool DOS_Utf16ToUtf8(const uint16_t *s, std::string &out) {
	out.clear();
	for (; *s; s++) {
		uint32_t cp = *s;
		if (cp >= 0xD800 && cp <= 0xDBFF && s[1] >= 0xDC00 && s[1] <= 0xDFFF) {
			cp = 0x10000 + ((cp - 0xD800) << 10) + (s[1] - 0xDC00);
			s++;
		} else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
		Utf8Put(out, cp);
	}
	return true;
}

bool DOS_Utf8ToUtf16(const char *s, uint16_t *d, size_t dmax) {
	size_t n = 0;
	while (*s) {
		size_t used;
		uint32_t cp = Utf8At(s, &used);
		s += used;
		if (cp >= 0x10000) {
			if (n + 2 >= dmax) return false;
			cp -= 0x10000;
			d[n++] = (uint16_t)(0xD800 + (cp >> 10));
			d[n++] = (uint16_t)(0xDC00 + (cp & 0x3FF));
		} else {
			if (n + 1 >= dmax) return false;
			d[n++] = (uint16_t)cp;
		}
	}
	d[n] = 0;
	return true;
}

/* ---- the escape form ------------------------------------------------------------------------------------------------------- */

static int HexVal(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

/* the escape "{U+XXXX}" at s: the code point and the length, else 0 */
static size_t EscapeAt(const char *s, uint32_t *cp) {
	if (s[0] != '{' || (s[1] != 'U' && s[1] != 'u') || s[2] != '+') return 0;
	uint32_t v = 0;
	size_t n = 0;
	while (n < 6 && HexVal(s[3 + n]) >= 0) { v = (v << 4) | (uint32_t)HexVal(s[3 + n]); n++; }
	if (n < 4 || s[3 + n] != '}') return 0;
	if (v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF) || v == 0) return 0;
	*cp = v;
	return 3 + n + 1;
}

static const size_t NAME_MAX_LEN = CROSS_LEN - 1;

typedef bool (*DOS_CpConv)(char *d, const char *s);

/* the conversions of drive_local.cpp for the guest code page; they fail on a character that the code page does not have */
bool CodePageHostToGuestUTF8_Core(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
bool CodePageGuestToHostUTF8_Core(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
bool CodePageHostToGuestUTF16_Core(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);
bool CodePageGuestToHostUTF16_Core(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);

static bool HostToGuest(char *d, const char *s, DOS_CpConv core) {
	std::string out;
	char one[8], conv[CROSS_LEN + 8];
	if (!strstr(s, "{U+") && !strstr(s, "{u+") && core(conv, s)) {      /* the usual case: nothing to escape */
		memcpy(d, conv, strlen(conv) + 1);
		return true;
	}
	while (*s) {
		size_t used;
		const uint32_t cp = Utf8At(s, &used);
		if (cp == 0xFFFD && used == 1) return false;                   /* not UTF-8 at all */
		if (cp == '{' && (s[1] == 'U' || s[1] == 'u') && s[2] == '+') out += "{U+007B}";   /* the text of an escape: escape it */
		else if (cp < 0x80) out += (char)cp;
		else {
			memcpy(one, s, used);
			one[used] = 0;
			if (core(conv, one)) out += conv;
			else {
				char esc[16];
				snprintf(esc, sizeof(esc), cp > 0xFFFF ? "{U+%05X}" : "{U+%04X}", (unsigned)cp);
				out += esc;
			}
		}
		s += used;
		if (out.size() > NAME_MAX_LEN) return false;
	}
	memcpy(d, out.c_str(), out.size() + 1);
	return true;
}

static bool GuestToHost(char *d, const char *s, DOS_CpConv core) {
	std::string out, seg;
	char conv[CROSS_LEN + 8];
	while (true) {
		uint32_t cp = 0;
		size_t esc = *s ? EscapeAt(s, &cp) : 0;
		if (esc == 0 && *s) { seg += *s++; continue; }
		if (!seg.empty()) {
			if (seg.size() > NAME_MAX_LEN || !core(conv, seg.c_str())) return false;
			out += conv;
			seg.clear();
		}
		if (!*s) break;
		Utf8Put(out, cp);
		s += esc;
		if (out.size() > NAME_MAX_LEN) return false;
	}
	memcpy(d, out.c_str(), out.size() + 1);
	return true;
}

/* the conversions that the rest of DOSBox-X calls (they were in drive_local.cpp): with the option on, the characters that the
 * code page lacks are kept in the escape form, so no file is lost */
bool CodePageHostToGuestUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
	if (!dos_utf8names) return CodePageHostToGuestUTF8_Core(d, s);
	return HostToGuest(d, s, CodePageHostToGuestUTF8_Core);
}

bool CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
	if (!dos_utf8names || !strstr(s, "{")) return CodePageGuestToHostUTF8_Core(d, s);
	return GuestToHost(d, s, CodePageGuestToHostUTF8_Core);
}

bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/) {
	if (!dos_utf8names) return CodePageHostToGuestUTF16_Core(d, s);
	std::string u8;
	DOS_Utf16ToUtf8(s, u8);
	return CodePageHostToGuestUTF8(d, u8.c_str());
}

bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/) {
	if (!dos_utf8names) return CodePageGuestToHostUTF16_Core(d, s);
	char u8[CROSS_LEN + 8];
	return CodePageGuestToHostUTF8(u8, s) && DOS_Utf8ToUtf16(u8, d, CROSS_LEN);
}

/* ---- the border of the INT 21h AH=71h functions for a process in the UTF-8 mode -------------------------------------------- */

/* the name that the program gave (UTF-8) -> the guest form; the name for the program -> UTF-8 */
bool DOS_UTF8_NameIn(char *name, size_t size) {
	char conv[CROSS_LEN + 8];
	if (!HostToGuest(conv, name, CodePageHostToGuestUTF8_Core) || strlen(conv) >= size) return false;
	strcpy(name, conv);
	return true;
}

bool DOS_UTF8_NameOut(char *name, size_t size) {
	char conv[CROSS_LEN + 8];
	if (!GuestToHost(conv, name, CodePageGuestToHostUTF8_Core) || strlen(conv) >= size) return false;
	strcpy(name, conv);
	return true;
}

/* ---- the long file name functions: the names that go in and out of the guest memory -------------------------------------------- */

/* the name at src (the guest memory) as the DOS file layer wants it: the guest form */
void DOS_LFN_StrCopy(PhysPt src, char *dst, Bitu max) {
	MEM_StrCopy(src, dst, max);
	if (DOS_UTF8NamesMode()) DOS_UTF8_NameIn(dst, max);          /* too long or not UTF-8: left as it is, the lookup then fails */
}

/* the name for the program: the whole of it, with the NUL */
void DOS_LFN_NameWrite(PhysPt dst, const char *name, Bitu len) {
	if (DOS_UTF8NamesMode()) {
		char conv[CROSS_LEN + 8];
		strncpy(conv, name, CROSS_LEN - 1);
		conv[CROSS_LEN - 1] = 0;
		if (DOS_UTF8_NameOut(conv, CROSS_LEN)) { MEM_BlockWrite(dst, conv, strlen(conv) + 1); return; }
	}
	MEM_BlockWrite(dst, name, len);
}

/* the long name of a find data record (offset 44, 260 bytes) */
void DOS_LFN_FindData(char *finddata) {
	if (!DOS_UTF8NamesMode()) return;
	DOS_UTF8_NameOut(finddata + 44, 260);
	for (char *p = finddata + 304; *p; p++)                      /* short names are ASCII in this mode (a name from the OEM page could be mistaken for UTF-8) */
		if ((unsigned char)*p >= 0x80) *p = '_';
}

/* the input names of a long file name function must be valid UTF-8 (the specification: else error 2) */
static bool Utf8Valid(const char *s) {
	while (*s) {
		size_t used;
		if (Utf8At(s, &used) == 0xFFFD && used == 1) return false;
		s += used;
	}
	return true;
}

bool DOS_LFN_InputOK(void) {
	if (!DOS_UTF8NamesMode()) return true;
	char name[DOSNAMEBUF_UTF8 + 2];
	PhysPt first = 0, second = 0;
	switch (reg_al) {
		case 0x39: case 0x3a: case 0x3b: case 0x41: case 0x43: case 0x4e: first = SegPhys(ds) + reg_dx; break;
		case 0x56: first = SegPhys(ds) + reg_dx; second = SegPhys(es) + reg_di; break;
		case 0x60: case 0x6c: case 0xa8: first = SegPhys(ds) + reg_si; break;
		default: return true;
	}
	MEM_StrCopy(first, name, DOSNAMEBUF_UTF8);
	if (!Utf8Valid(name)) return false;
	if (second) {
		MEM_StrCopy(second, name, DOSNAMEBUF_UTF8);
		if (!Utf8Valid(name)) return false;
	}
	return true;
}

/* the upper-case table for 6502h/6504h in the UTF-8 mode: 80h-FFh map to themselves */
uint32_t DOS_UTF8Names_IdentityUpcase(void) {
	static uint32_t where = 0;
	if (where == 0) {
		const PhysPt p = ROMBIOS_GetMemory(2 + 128, "UTF-8 identity upcase table");
		mem_writew(p, 128);
		for (unsigned i = 0; i < 128; i++) mem_writeb(p + 2 + i, (uint8_t)(0x80 + i));
		where = RealMake((uint16_t)(p >> 4), (uint16_t)(p & 15));
	}
	return where;
}
