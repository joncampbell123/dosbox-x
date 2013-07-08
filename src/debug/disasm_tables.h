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

char * RegTable_16[8]=	{"ax","cx","dx","bx","sp","bp","si","di"};
char * RegTable_8[8]=	{"al","cl","dl","bl","ah","ch","dh","bh"};
char * SegTable[8]=		{"es","cs","ss","ds","fs","gs","iseg","iseg"};


#define MAX_INFO 3

enum {
	Eb,Ev,Ew,Ep,
	Gb,Gv,
	Rb,Rw,Rv,
	Ob,Ov,
	Sw,
	Mp,
	Ib,Ibs,Iw,Iv,Ap,
	Jb,Jv,
	Bd,Bw,
	XBnd,Xlea,
/* specials */
	b2,p_es,p_ss,p_cs,p_ds,p_fs,p_gs,p_size,p_addr,p_rep,
	s_ax,s_cx,s_dx,s_bx,s_sp,s_bp,s_si,s_di,
	s_al,s_cl,s_dl,s_bl,s_ah,s_ch,s_dh,s_bh,
	s_1,
	Cj,
	G1,G2,G3b,G3v,G4,G5,
	no=0xff
};

enum {
	s_jo,	s_jno,	s_jc,	s_jnc,	s_je,	s_jne,	s_jbe,	s_jnbe,
	s_js,	s_jns,	s_jp,	s_jnp,	s_jl,	s_jnl,	s_jle,	s_jnle
};


struct Dentry {
	char * start;
	Bit8u info[MAX_INFO];
};

static char * G1_Table[8]={"add ","or ","adc ","sbb ","and ","sub ","xor ","cmp "};
static char * G2_Table[8]={"rol ","ror ","rcl ","rcr ","shl ","shr ","sal ","sar "};

static Dentry G3b_Table[8]={
	"test ",Eb,Ib,no,
	"test ",Eb,Ib,no,
	"not ",Eb,no,no,
	"neg ",Eb,no,no,	 
	"mul al,",Eb,no,no,
	"imul al,",Eb,no,no,
	"div ax,",Eb,no,no,
	"idiv ax,",Eb,no,no
};

static Dentry G3v_Table[8]={
	"test ",Ev,Iv,no,
	"test ",Ev,Iv,no,
	"not ",Ev,no,no,
	"neg ",Ev,no,no,	 
	"mul ax,",Ev,no,no,
	"imul ax,",Ev,no,no,
	"div dx:ax,",Ev,no,no,
	"idiv dx:ax,",Ev,no,no
};

static char * G4_Table[8]={
	"inc ",
	"dec ",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal"
};

static Dentry G5_Table[8]={
	"inc ",Ev,no,no,
	"dec ",Ev,no,no,
	"call ",Ev,no,no,
	"call ",Ep,no,no,	 
	"jmp ",Ev,no,no,
	"jmp ",Ep,no,no,
	"push ,",Ev,no,no,
	"illegal",no,no,no
};



static Dentry DTable[256]={
/* 0 */
	"add ",Eb,Gb,no,	"add ",Ev,Gv,no,	"add ",Gb,Eb,no,	"add ",Gv,Ev,no,
	"add ",s_al,Ib,no,	"add ",s_ax,Iv,no,	"push es",no,no,no,	"pop es",no,no,no,
	"or ",Eb,Gb,no,		"or ",Ev,Gv,no,		"or ",Gb,Eb,no,		"or ",Gv,Ev,no,
	"or ",s_al,Ib,no,	"or ",s_ax,Iv,no,	"push cs",no,no,no,	"",b2,no,no,
/* 1 */
	"adc ",Eb,Gb,no,	"adc ",Ev,Gv,no,	"adc ",Gb,Eb,no,	"adc ",Gv,Ev,no,
	"adc ",s_al,Ib,no,	"adc ",s_ax,Iv,no,	"push ss",no,no,no,	"pop ss",no,no,no,
	"sbb ",Eb,Gb,no,	"sbb ",Ev,Gv,no,	"sbb ",Gb,Eb,no,	"sbb ",Gv,Ev,no,
	"sbb ",s_al,Ib,no,	"sbb ",s_ax,Iv,no,	"push ds",no,no,no,	"pop ds",no,no,no,
/* 2 */
	"and ",Eb,Gb,no,	"and ",Ev,Gv,no,	"and ",Gb,Eb,no,	"and ",Gv,Ev,no,
	"and ",s_al,Ib,no,	"and ",s_ax,Iv,no,	"",p_es,no,no,		"daa",no,no,no,
	"sub ",Eb,Gb,no,	"sub ",Ev,Gv,no,	"sub ",Gb,Eb,no,	"sub ",Gv,Ev,no,
	"sub ",s_al,Ib,no,	"sub ",s_ax,Iv,no,	"",p_ss,no,no,		"das",no,no,no,
/* 3 */
	"xor ",Eb,Gb,no,	"xor ",Ev,Gv,no,	"xor ",Gb,Eb,no,	"xor ",Gv,Ev,no,
	"xor ",s_al,Ib,no,	"xor ",s_ax,Iv,no,	"",p_ss,no,no,		"aaa",no,no,no,
	"cmp ",Eb,Gb,no,	"cmp ",Ev,Gv,no,	"cmp ",Gb,Eb,no,	"cmp ",Gv,Ev,no,
	"cmp ",s_al,Ib,no,	"cmp ",s_ax,Iv,no,	"",p_ds,no,no,		"aas",no,no,no,
/* 4 */
	"inc ",s_ax,no,no,	"inc ",s_cx,no,no,	"inc ",s_dx,no,no,	"inc ",s_bx,no,no,
	"inc ",s_sp,no,no,	"inc ",s_bp,no,no,	"inc ",s_si,no,no,	"inc ",s_di,no,no,
	"dec ",s_ax,no,no,	"dec ",s_cx,no,no,	"dec ",s_dx,no,no,	"dec ",s_bx,no,no,
	"dec ",s_sp,no,no,	"dec ",s_bp,no,no,	"dec ",s_si,no,no,	"dec ",s_di,no,no,
/* 5 */
	"push ",s_ax,no,no,	"push ",s_cx,no,no,	"push ",s_dx,no,no,	"push ",s_bx,no,no,
	"push ",s_sp,no,no,	"push ",s_bp,no,no,	"push ",s_si,no,no,	"push ",s_di,no,no,
	"pop ",s_ax,no,no,	"pop ",s_cx,no,no,	"pop ",s_dx,no,no,	"pop ",s_bx,no,no,
	"pop ",s_sp,no,no,	"pop ",s_bp,no,no,	"pop ",s_si,no,no,	"pop ",s_di,no,no,
/* 6 */
	"pusha",Bd,no,no,	"popa",Bd,no,no,	"bound",XBnd,no,no,	"arpl",Ew,Rw,no,
	"",p_fs,no,no,		"",p_gs,no,no,		"",p_size,no,no,	"",p_addr,no,no,
	"push ",Iv,no,no,	"imul ",Gv,Ev,Iv,	"push ",Ibs,no,no,	"imul ",Gv,Ev,Ib,
	"insb",no,no,no,	"ins",Bw,no,no,		"oustb",no,no,no,	"outs",Bw,no,no,
/* 7 */
	"jo ",Cj,s_jo,no,	"jno ",Cj,s_jno,no,	"jc ",Cj,s_jc,no,	"jnc ",Cj,s_jnc,no,
	"je ",Cj,s_je,no,	"jne ",Cj,s_jne,no,	"jbe ",Cj,s_jbe,no,	"jnbe ",Cj,s_jnbe,no,
	"js ",Cj,s_js,no,	"jns ",Cj,s_jns,no,	"jp ",Cj,s_jp,no,	"jnp ",Cj,s_jnp,no,
	"jl ",Cj,s_jl,no,	"jnl ",Cj,s_jnl,no,	"jle ",Cj,s_jle,no,	"jnle ",Cj,s_jnle,no,
/* 8 */
	"",G1,Eb,Ib,		"",G1,Ev,Iv,		"",G1,Eb,Ib,		"",G1,Ev,Ibs,
	"test ",Eb,Gb,no,	"test ",Ev,Gv,no,	"xchg ",Eb,Gb,no,	"xchg ",Ev,Gv,no,
	"mov ",Eb,Gb,no,	"mov ",Ev,Gv,no,	"mov ",Gb,Eb,no,	"mov ",Gv,Ev,no,
	"mov ",Ew,Sw,no,	"lea ",Gv,Xlea,no,	"mov ",Sw,Ew,no,	"pop ",Ev,no,no,
/* 9 */
	"nop",no,no,no,		 "xchg ",s_ax,s_cx,no,"xchg ",s_ax,s_dx,no,"xchg ",s_ax,s_bx,no,
	"xchg ",s_ax,s_sp,no,"xchg ",s_ax,s_bp,no,"xchg ",s_ax,s_si,no,"xchg ",s_ax,s_di,no, 
	"cbw",no,no,no,		"cwd",no,no,no,		"call ",Ap,no,no,	"fwait",no,no,no,
	"pushf",Bd,no,no,	"popf",Bd,no,no,	"sahf",no,no,no,	"lahf",no,no,no,
/* a */
	"mov ",s_al,Ob,no,	"mov ",s_ax,Ov,no,	"mov ",Ob,s_al,no,	"mov ",Ov,s_ax,no,
	"movsb",no,no,no,	"movs",Bw,no,no,	"cmpsb",no,no,no,	"cmps",Bw,no,no,
	"test ",s_al,Ib,no,	"test ",s_ax,Iv,no,	"stosb",no,no,no,	"stos",Bw,no,no,	
	"lodsb",no,no,no,	"lods",Bw,no,no,	"scasb",no,no,no,	"scas",Bw,no,no,
/* b */
	"mov ",s_al,Ib,no,	"mov ",s_cl,Ib,no,	"mov ",s_dl,Ib,no,	"mov ",s_bl,Ib,no,
	"mov ",s_ah,Ib,no,	"mov ",s_ch,Ib,no,	"mov ",s_dh,Ib,no,	"mov ",s_bh,Ib,no,
	"mov ",s_ax,Iv,no,	"mov ",s_cx,Iv,no,	"mov ",s_dx,Iv,no,	"mov ",s_bx,Iv,no,	
	"mov ",s_sp,Iv,no,	"mov ",s_bp,Iv,no,	"mov ",s_si,Iv,no,	"mov ",s_di,Iv,no,	
/* c */
  	"",G2,Eb,Ib,		"",G2,Ev,Ib,		"ret ",Iw,no,no,	"ret",no,no,no,
	"les ",Gv,Mp,no,	"lds ",Gv,Mp,no,	"mov ",Eb,Ib,no,	"mov ",Ev,Iv,no,
	"enter ",Iw,Ib,no,	"leave",no,no,no,	"retf ",Iw,no,no,	"retf",no,no,no,
	"int 03",no,no,no,	"int ",Ib,no,no,	"into",no,no,no,	"iret",Bd,no,no,
/* d */
	"",G2,Eb,s_1,		"",G2,Ev,s_1,		"",G2,Eb,s_cl,		"",G2,Ev,s_cl,
	"aam",no,no,no,		"aad",no,no,no,		"setalc",no,no,no,	"xlat",no,no,no,
	"esc 0",Ib,no,no,	"esc 1",Ib,no,no,	"esc 2",Ib,no,no,	"esc 3",Ib,no,no,
	"esc 4",Ib,no,no,	"esc 5",Ib,no,no,	"esc 6",Ib,no,no,	"esc 7",Ib,no,no,
/* e */
	"loopne ",Jb,no,no,	"loope ",Jb,no,no,	"loop ",Jb,no,no,	"jcxz ",Jb,no,no,
	"in ",s_al,Ib,no,	"in ",s_ax,Ib,no,	"out ",Ib,s_al,no,	"out ",Ib,s_ax,no,
	"call ",Jv,no,no,	"jmp ",Jv,no,no,	"jmp",Ap,no,no,		"jmp ",Jb,no,no,
	"in ",s_al,s_dx,no,	"in ",s_ax,s_dx,no, "out ",s_dx,s_al,no,"out ",s_dx,s_ax,no,
/* f */
	"lock",no,no,no,	"cb ",Iw,no,no,		"repne ",p_rep,no,no,"repe ",p_rep,no,no,
	"hlt",no,no,no,		"cmc",no,no,no,		"",G3b,no,no,		"",G3v,no,no,
	"clc",no,no,no,		"stc",no,no,no,		"cli",no,no,no,		"sti",no,no,no,
	"cld",no,no,no,		"std",no,no,no,		"",G4,Eb,no,		"",G5,no,no,
};



