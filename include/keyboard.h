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

#ifndef DOSBOX_KEYBOARD_H
#define DOSBOX_KEYBOARD_H

enum KBD_KEYS {
	KBD_NONE,
	KBD_1,	KBD_2,	KBD_3,	KBD_4,	KBD_5,	KBD_6,	KBD_7,	KBD_8,	KBD_9,	KBD_0,		
	KBD_q,	KBD_w,	KBD_e,	KBD_r,	KBD_t,	KBD_y,	KBD_u,	KBD_i,	KBD_o,	KBD_p,	
	KBD_a,	KBD_s,	KBD_d,	KBD_f,	KBD_g,	KBD_h,	KBD_j,	KBD_k,	KBD_l,	KBD_z,
	KBD_x,	KBD_c,	KBD_v,	KBD_b,	KBD_n,	KBD_m,	
	KBD_f1,	KBD_f2,	KBD_f3,	KBD_f4,	KBD_f5,	KBD_f6,	KBD_f7,	KBD_f8,	KBD_f9,	KBD_f10,KBD_f11,KBD_f12,
	
	/*Now the weirder keys */

	KBD_esc,KBD_tab,KBD_backspace,KBD_enter,KBD_space,
	KBD_leftalt,KBD_rightalt,KBD_leftctrl,KBD_rightctrl,KBD_leftshift,KBD_rightshift,
	KBD_capslock,KBD_scrolllock,KBD_numlock,
	
	KBD_grave,KBD_minus,KBD_equals,KBD_backslash,KBD_leftbracket,KBD_rightbracket,
	KBD_semicolon,KBD_quote,KBD_period,KBD_comma,KBD_slash,KBD_extra_lt_gt,

	KBD_printscreen,KBD_pause,
	KBD_insert,KBD_home,KBD_pageup,KBD_delete,KBD_end,KBD_pagedown,
	KBD_left,KBD_up,KBD_down,KBD_right,

	KBD_kp1,KBD_kp2,KBD_kp3,KBD_kp4,KBD_kp5,KBD_kp6,KBD_kp7,KBD_kp8,KBD_kp9,KBD_kp0,
	KBD_kpdivide,KBD_kpmultiply,KBD_kpminus,KBD_kpplus,KBD_kpenter,KBD_kpperiod,

	/* Windows 95 keys */
	KBD_lwindows,KBD_rwindows,KBD_rwinmenu,

	/* other bindings */
	KBD_kpequals,

	/* F13-F24 */
	KBD_f13,KBD_f14,KBD_f15,KBD_f16,
	KBD_f17,KBD_f18,KBD_f19,KBD_f20,
	KBD_f21,KBD_f22,KBD_f23,KBD_f24,

	/* Japanese [see http://www.stanford.edu/class/cs140/projects/pintos/specs/kbd/scancodes-7.html] */
	KBD_jp_hankaku,		/* Hankaku/zenkaku (half-width/full-width) */
	KBD_jp_muhenkan,	/* Muhenkan (No conversion from kana to kanji) */
	KBD_jp_henkan,		/* Henkan/zenkouho (Conversion from kana to kanji, shifted: previous candidate, alt: all candidates) */
	KBD_jp_hiragana,	/* Hiragana/Katakana (Hiragana, shifted: Katakana, alt: romaji) */

	/* Korean */
	KBD_kor_hancha,		/* Hancha */
	KBD_kor_hanyong,	/* Han/yong */

	/* for Japanese A01 (106) key [http://www.mediafire.com/download/t968ydz6ky92myl/dosbox74.zip] */
	/* see reference image [https://upload.wikimedia.org/wikipedia/commons/thumb/b/bc/KB_Japanese.svg/1280px-KB_Japanese.svg.png] */
	KBD_jp_yen, KBD_jp_backslash, KBD_colon, KBD_caret, KBD_atsign, KBD_jp_ro, KBD_help, KBD_kpcomma,
    KBD_stop, KBD_copy, KBD_vf1, KBD_vf2, KBD_vf3, KBD_vf4, KBD_vf5, KBD_kana,
    KBD_nfer, KBD_xfer,

	KBD_LAST
};

void KEYBOARD_ClrBuffer(void);
void KEYBOARD_AddKey(KBD_KEYS keytype,bool pressed);
size_t KEYBOARD_BufferSpaceAvail();  // emendelson from dbDOS

#endif
