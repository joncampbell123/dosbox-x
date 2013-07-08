/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

	
	KBD_LAST
};

void KEYBOARD_ClrBuffer(void);
void KEYBOARD_AddKey(KBD_KEYS keytype,bool pressed);

#endif
