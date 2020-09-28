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

#include "cpu.h"


uint8_t * lookupRMregb[]=
{
	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,

	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,

	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,

	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh
};

uint16_t * lookupRMregw[]={
	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,

	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,

	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,

	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di
};

Bit32u * lookupRMregd[256]={
	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,

	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,

	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,

	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi
};


uint8_t * lookupRMEAregb[256]={
/* 12 lines of 16*0 should give nice errors when used */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh
};

uint16_t * lookupRMEAregw[256]={
/* 12 lines of 16*0 should give nice errors when used */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di
};

Bit32u * lookupRMEAregd[256]={
/* 12 lines of 16*0 should give nice errors when used */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi
};


