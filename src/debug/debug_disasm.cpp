
/*
	Ripped out some stuff from the mame releae to only make it for 386's 
	Changed some variables to use the standard DOSBox data types 
	Added my callback opcode 
	
*/

/*
 * 2asm: Convert binary files to 80*86 assembler. Version 1.00
 * Adapted by Andrea Mazzoleni for use with MAME
 * HJB 990321:
 * Changed output of hex values from 0xxxxh to $xxxx format
 * Removed "ptr" from "byte ptr", "word ptr" and "dword ptr"
*/

/* 2asm comments

License:

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

Comments:

   The code was originally snaffled from the GNU C++ debugger, as ported
   to DOS by DJ Delorie and Kent Williams (williams@herky.cs.uiowa.edu).
   Extensively modified by Robin Hilliard in Jan and May 1992.

   This source compiles under Turbo C v2.01.  The disassembler is entirely
   table driven so it's fairly easy to change to suit your own tastes.

   The instruction table has been modified to correspond with that in
   `Programmer's Technical Reference: The Processor and Coprocessor',
   Robert L. Hummel, Ziff-Davis Press, 1992.  Missing (read "undocumented")
   instructions were added and many mistakes and omissions corrected.


Health warning:

   When writing and degbugging this code, I didn't have (and still don't have)
   a 32-bit disassembler to compare this guy's output with.  It's therefore
   quite likely that bugs will appear when disassembling instructions which use
   the 386 and 486's native 32 bit mode.  It seems to work fine in 16 bit mode.

Any comments/updates/bug reports to:

   Robin Hilliard, Lough Guitane, Killarney, Co. Kerry, Ireland.
   Tel:         [+353] 64-54014
   Internet:    softloft@iruccvax.ucc.ie
   Compu$erve:  100042, 1237

   If you feel like registering, and possibly get notices of updates and
   other items of software, then send me a post card of your home town.

   Thanks and enjoy!

*/
#include "dosbox.h"
#if C_DEBUG
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "mem.h"
#include "paging.h"

typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;

typedef int8_t  INT8;
typedef int16_t INT16;
typedef Bit32s INT32;


/* Little endian uint read */
#define	le_uint8(ptr) (*(UINT8*)ptr)

INLINE UINT16 le_uint16(const void* ptr) {
	const UINT8* ptr8 = (const UINT8*)ptr;
	return (UINT16)ptr8[0] | (UINT16)ptr8[1] << 8;
}
INLINE UINT32 le_uint32(const void* ptr) {
	const UINT8* ptr8 = (const UINT8*)ptr;
	return (UINT32)ptr8[0] | (UINT32)ptr8[1] << 8 |	(UINT32)ptr8[2] << 16 | (UINT32)ptr8[3] << 24;
}

/* Little endian int read */
#define le_int8(ptr) ((INT8)le_uint8(ptr))
#define le_int16(ptr) ((INT16)le_uint16(ptr))
#define le_int32(ptr) ((INT32)le_uint32(ptr))

#define fp_segment(dw) ((dw >> 16) & 0xFFFFU)
#define fp_offset(dw) (dw & 0xFFFFU)
#define fp_addr(seg, off) ( (seg<<4)+off )

static UINT8 must_do_size;   /* used with size of operand */
static int wordop;           /* dealing with word or byte operand */

static uint32_t instruction_offset;
//static UINT16 instruction_segment;

static char* ubufs;           /* start of buffer */
static char* ubufp;           /* last position of buffer */
static int invalid_opcode = 0;
static int first_space = 1;

static int prefix;            /* segment override prefix byte */
static int modrmv;            /* flag for getting modrm byte */
static int sibv;              /* flag for getting sib byte   */
static int opsize;            /* just like it says ...       */
static int addrsize;
static int addr32bit=0;

/* some defines for extracting instruction bit fields from bytes */

#define MOD(a)	  (((a)>>6)&7)
#define REG(a)	  (((a)>>3)&7)
#define RM(a)	  ((a)&7)
#define SCALE(a)  (((a)>>6)&7)
#define INDEX(a)  (((a)>>3)&7)
#define BASE(a)   ((a)&7)

/* Percent tokens in strings:
   First char after '%':
	A - direct address
	C - reg of r/m picks control register
	D - reg of r/m picks debug register
	E - r/m picks operand
	F - flags register
	G - reg of r/m picks general register
	I - immediate data
	J - relative IP offset
+       K - call/jmp distance
	M - r/m picks memory
	O - no r/m, offset only
	R - mod of r/m picks register only
	S - reg of r/m picks segment register
	T - reg of r/m picks test register
	X - DS:ESI
	Y - ES:EDI
	2 - prefix of two-byte opcode
+       e - put in 'e' if use32 (second char is part of reg name)
+           put in 'w' for use16 or 'd' for use32 (second char is 'w')
+       j - put in 'e' in jcxz if prefix==0x66
	f - floating point (second char is esc value)
	g - do r/m group 'n', n==0..7
	p - prefix
	s - size override (second char is a,o)
+       d - put d if double arg, nothing otherwise (pushfd, popfd &c)
+       w - put w if word, d if double arg, nothing otherwise (lodsw/lodsd)
+       P - simple prefix

   Second char after '%':
	a - two words in memory (BOUND)
	b - byte
	c - byte or word
	d - dword
+       f - far call/jmp
+       n - near call/jmp
        p - 32 or 48 bit pointer
+       q - byte/word thingy
	s - six byte pseudo-descriptor
	v - word or dword
        w - word
+       x - sign extended byte
	F - use floating regs in mod/rm
	1-8 - group number, esc value, etc
*/

/* watch out for aad && aam with odd operands */

static char const* (*opmap1)[256];

static char const * op386map1[256] = {
/* 0 */
  "add %Eb,%Gb",      "add %Ev,%Gv",     "add %Gb,%Eb",    "add %Gv,%Ev",
  "add al,%Ib",       "add %eax,%Iv",    "push es",        "pop es",
  "or %Eb,%Gb",       "or %Ev,%Gv",      "or %Gb,%Eb",     "or %Gv,%Ev",
  "or al,%Ib",        "or %eax,%Iv",     "push cs",        "%2 ",
/* 1 */
  "adc %Eb,%Gb",      "adc %Ev,%Gv",     "adc %Gb,%Eb",    "adc %Gv,%Ev",
  "adc al,%Ib",       "adc %eax,%Iv",    "push ss",        "pop ss",
  "sbb %Eb,%Gb",      "sbb %Ev,%Gv",     "sbb %Gb,%Eb",    "sbb %Gv,%Ev",
  "sbb al,%Ib",       "sbb %eax,%Iv",    "push ds",        "pop ds",
/* 2 */
  "and %Eb,%Gb",      "and %Ev,%Gv",     "and %Gb,%Eb",    "and %Gv,%Ev",
  "and al,%Ib",       "and %eax,%Iv",    "%pe",            "daa",
  "sub %Eb,%Gb",      "sub %Ev,%Gv",     "sub %Gb,%Eb",    "sub %Gv,%Ev",
  "sub al,%Ib",       "sub %eax,%Iv",    "%pc",            "das",
/* 3 */
  "xor %Eb,%Gb",      "xor %Ev,%Gv",     "xor %Gb,%Eb",    "xor %Gv,%Ev",
  "xor al,%Ib",       "xor %eax,%Iv",    "%ps",            "aaa",
  "cmp %Eb,%Gb",      "cmp %Ev,%Gv",     "cmp %Gb,%Eb",    "cmp %Gv,%Ev",
  "cmp al,%Ib",       "cmp %eax,%Iv",    "%pd",            "aas",
/* 4 */
  "inc %eax",         "inc %ecx",        "inc %edx",       "inc %ebx",
  "inc %esp",         "inc %ebp",        "inc %esi",       "inc %edi",
  "dec %eax",         "dec %ecx",        "dec %edx",       "dec %ebx",
  "dec %esp",         "dec %ebp",        "dec %esi",       "dec %edi",
/* 5 */
  "push %eax",        "push %ecx",       "push %edx",      "push %ebx",
  "push %esp",        "push %ebp",       "push %esi",      "push %edi",
  "pop %eax",         "pop %ecx",        "pop %edx",       "pop %ebx",
  "pop %esp",         "pop %ebp",        "pop %esi",       "pop %edi",
/* 6 */
  "pusha%d ",         "popa%d ",         "bound %Gv,%Ma",  "arpl %Ew,%Rw",
  "%pf",              "%pg",             "%so",            "%sa",
  "push %Iv",         "imul %Gv,%Ev,%Iv","push %Ix",       "imul %Gv,%Ev,%Ib",
  "insb",             "ins%ew",          "outsb",          "outs%ew",
/* 7 */
  "jo %Jb",           "jno %Jb",         "jc %Jb",         "jnc %Jb",
  "je %Jb",           "jne %Jb",         "jbe %Jb",        "ja %Jb",
  "js %Jb",           "jns %Jb",         "jpe %Jb",        "jpo %Jb",
  "jl %Jb",           "jge %Jb",         "jle %Jb",        "jg %Jb",
/* 8 */
  "%g0 %Eb,%Ib",      "%g0 %Ev,%Iv",     "%g0 %Eb,%Ib",    "%g0 %Ev,%Ix",
  "test %Eb,%Gb",     "test %Ev,%Gv",    "xchg %Eb,%Gb",   "xchg %Ev,%Gv",
  "mov %Eb,%Gb",      "mov %Ev,%Gv",     "mov %Gb,%Eb",    "mov %Gv,%Ev",
  "mov %Ew,%Sw",      "lea %Gv,%M ",     "mov %Sw,%Ew",    "pop %Ev",
/* 9 */
  "nop",              "xchg %ecx,%eax",  "xchg %edx,%eax", "xchg %ebx,%eax",
  "xchg %esp,%eax",   "xchg %ebp,%eax",  "xchg %esi,%eax", "xchg %edi,%eax",
  "cbw",              "cwd",             "call %Ap",       "fwait",
  "pushf%d ",         "popf%d ",         "sahf",           "lahf",
/* a */
  "mov al,%Oc",       "mov %eax,%Ov",    "mov %Oc,al",     "mov %Ov,%eax",
  "%P movsb",         "%P movs%w",       "%P cmpsb",       "%P cmps%w ",
  "test al,%Ib",      "test %eax,%Iv",   "%P stosb",       "%P stos%w ",
  "%P lodsb",         "%P lods%w ",      "%P scasb",       "%P scas%w ",
/* b */
  "mov al,%Ib",       "mov cl,%Ib",      "mov dl,%Ib",     "mov bl,%Ib",
  "mov ah,%Ib",       "mov ch,%Ib",      "mov dh,%Ib",     "mov bh,%Ib",
  "mov %eax,%Iv",     "mov %ecx,%Iv",    "mov %edx,%Iv",   "mov %ebx,%Iv",
  "mov %esp,%Iv",     "mov %ebp,%Iv",    "mov %esi,%Iv",   "mov %edi,%Iv",
/* c */
  "%g1 %Eb,%Ib",      "%g1 %Ev,%Ib",     "ret %Iw",        "ret",
  "les %Gv,%Mp",      "lds %Gv,%Mp",     "mov %Eb,%Ib",    "mov %Ev,%Iv",
  "enter %Iw,%Ib",    "leave",           "retf %Iw",       "retf",
  "int 03",           "int %Ib",         "into",           "iret",
/* d */
  "%g1 %Eb,1",        "%g1 %Ev,1",       "%g1 %Eb,cl",     "%g1 %Ev,cl",
  "aam ; %Ib",        "aad ; %Ib",       "setalc",         "%P xlat",
#if 0
  "esc 0,%Ib",        "esc 1,%Ib",       "esc 2,%Ib",      "esc 3,%Ib",
  "esc 4,%Ib",        "esc 5,%Ib",       "esc 6,%Ib",      "esc 7,%Ib",
#else
  "%f0",              "%f1",             "%f2",            "%f3",
  "%f4",              "%f5",             "%f6",            "%f7",
#endif
/* e */
  "loopne %Jb",       "loope %Jb",       "loop %Jb",       "j%j cxz %Jb",
  "in al,%Ib",        "in %eax,%Ib",     "out %Ib,al",     "out %Ib,%eax",
  "call %Jv",         "jmp %Jv",         "jmp %Ap",        "jmp %Ks%Jb",
  "in al,dx",         "in %eax,dx",      "out dx,al",      "out dx,%eax",
/* f */
  "lock %p ",         "icebp",           "repne %p ",      "repe %p ",
  "hlt",              "cmc",             "%g2",            "%g2",
  "clc",              "stc",             "cli",            "sti",
  "cld",              "std",             "%g3",            "%g4"
};

static char const *second[] = {
/* 0 */
  "%g5",              "%g6",             "lar %Gv,%Ew",    "lsl %Gv,%Ew",
  0,                  "[loadall]",       "clts",           "[loadall]",
  "invd",             "wbinvd",          0,                "UD2",
  0,                  0,                 0,                0,
/* 1 */
  "mov %Eb,%Gb",      "mov %Ev,%Gv",     "mov %Gb,%Eb",    "mov %Gv,%Ev",
  0,                  0,                 0,                0,
  0,                  0,                 0,                0,
  0,                  0,                 0,                0,
/* 2 */
  "mov %Rd,%Cd",      "mov %Rd,%Dd",     "mov %Cd,%Rd",    "mov %Dd,%Rd",
  "mov %Rd,%Td",      0,                 "mov %Td,%Rd",    0,
  0,                  0,                 0,                0,
  0,                  0,                 0,                0,
/* 3 */
  0,                  "rdtsc",           0,                0,
  0,                  0,                 0,                0,
  0,                  0,                 0,                0,
  0,                  0,                 0,                0,
/* 4 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
/* 6 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
/* 7 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
/* 8 */
  "jo %Jv",           "jno %Jv",         "jb %Jv",         "jnb %Jv",
  "jz %Jv",           "jnz %Jv",         "jbe %Jv",        "ja %Jv",
  "js %Jv",           "jns %Jv",         "jp %Jv",         "jnp %Jv",
  "jl %Jv",           "jge %Jv",         "jle %Jv",        "jg %Jv",
/* 9 */
  "seto %Eb",         "setno %Eb",       "setc %Eb",       "setnc %Eb",
  "setz %Eb",         "setnz %Eb",       "setbe %Eb",      "setnbe %Eb",
  "sets %Eb",         "setns %Eb",       "setp %Eb",       "setnp %Eb",
  "setl %Eb",         "setge %Eb",       "setle %Eb",      "setg %Eb",
/* a */
  "push fs",          "pop fs",          "cpuid",          "bt %Ev,%Gv",
  "shld %Ev,%Gv,%Ib", "shld %Ev,%Gv,cl", 0,                0,
  "push gs",          "pop gs",          0,                "bts %Ev,%Gv",
  "shrd %Ev,%Gv,%Ib", "shrd %Ev,%Gv,cl", 0,                "imul %Gv,%Ev",
/* b */
  "cmpxchg %Eb,%Gb",  "cmpxchg %Ev,%Gv", "lss %Mp",        "btr %Ev,%Gv",
  "lfs %Mp",          "lgs %Mp",         "movzx %Gv,%Eb",  "movzx %Gv,%Ew",
  0,                  0,                 "%g7 %Ev,%Ib",    "btc %Ev,%Gv",
  "bsf %Gv,%Ev",      "bsr %Gv,%Ev",     "movsx %Gv,%Eb",  "movsx %Gv,%Ew",
/* c */
  "xadd %Eb,%Gb",     "xadd %Ev,%Gv",    0,                0,
  0,                  0,                 0,                0,
  "bswap eax",        "bswap ecx",       "bswap edx",      "bswap ebx",
  "bswap esp",        "bswap ebp",       "bswap esi",      "bswap edi",
/* d */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
/* e */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
/* f */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
};

static char const *groups[][8] = {   /* group 0 is group 3 for %Ev set */
/* 0 */
  { "add",            "or",              "adc",            "sbb",
    "and",            "sub",             "xor",            "cmp"           },
/* 1 */
  { "rol",            "ror",             "rcl",            "rcr",
    "shl",            "shr",             "shl",            "sar"           },
/* 2 */  /* v   v*/
  { "test %Eq,%Iq",   "test %Eq,%Iq",    "not %Ec",        "neg %Ec",
    "mul %Ec",        "imul %Ec",        "div %Ec",        "idiv %Ec"      },
/* 3 */
  { "inc %Eb",        "dec %Eb",         0,                0,
    0,                0,                 0,                "callback %Iw"  },
/* 4 */
  { "inc %Ev",        "dec %Ev",         "call %Kn%Ev",  "call %Kf%Ep",
    "jmp %Kn%Ev",     "jmp %Kf%Ep",      "push %Ev",       0               },
/* 5 */
  { "sldt %Ew",       "str %Ew",         "lldt %Ew",       "ltr %Ew",
    "verr %Ew",       "verw %Ew",        0,                0               },
/* 6 */
  { "sgdt %Ms",       "sidt %Ms",        "lgdt %Ms",       "lidt %Ms",
    "smsw %Ew",       0,                 "lmsw %Ew",       "invlpg"        },
/* 7 */
  { 0,                0,                 0,                0,
    "bt",             "bts",             "btr",            "btc"           }
};

/* zero here means invalid.  If first entry starts with '*', use st(i) */
/* no assumed %EFs here.  Indexed by RM(modrm())                       */
static char const *f0[]     = { 0, 0, 0, 0, 0, 0, 0, 0};
static char const *fop_8[]  = { "*fld st,%GF" };
static char const *fop_9[]  = { "*fxch st,%GF" };
static char const *fop_10[] = { "fnop", 0, 0, 0, 0, 0, 0, 0 };
static char const *fop_11[]  = { "*fst st,%GF" };
static char const *fop_12[] = { "fchs", "fabs", 0, 0, "ftst", "fxam", 0, 0 };
static char const *fop_13[] = { "fld1", "fldl2t", "fldl2e", "fldpi",
                   "fldlg2", "fldln2", "fldz", 0 };
static char const *fop_14[] = { "f2xm1", "fyl2x", "fptan", "fpatan",
                   "fxtract", "fprem1", "fdecstp", "fincstp" };
static char const *fop_15[] = { "fprem", "fyl2xp1", "fsqrt", "fsincos",
                   "frndint", "fscale", "fsin", "fcos" };
static char const *fop_21[] = { 0, "fucompp", 0, 0, 0, 0, 0, 0 };
static char const *fop_28[] = { "[fneni]", "[fndis]", "fclex", "finit", "[fnsetpm]", "[frstpm]", 0, 0 };
static char const *fop_32[] = { "*fadd %GF,st" };
static char const *fop_33[] = { "*fmul %GF,st" };
static char const *fop_34[] = { "*fcom %GF,st" };
static char const *fop_35[] = { "*fcomp %GF,st" };
static char const *fop_36[] = { "*fsubr %GF,st" };
static char const *fop_37[] = { "*fsub %GF,st" };
static char const *fop_38[] = { "*fdivr %GF,st" };
static char const *fop_39[] = { "*fdiv %GF,st" };
static char const *fop_40[] = { "*ffree %GF" };
static char const *fop_41[] = { "*fxch %GF" };
static char const *fop_42[] = { "*fst %GF" };
static char const *fop_43[] = { "*fstp %GF" };
static char const *fop_44[] = { "*fucom %GF" };
static char const *fop_45[] = { "*fucomp %GF" };
static char const *fop_48[] = { "*faddp %GF,st" };
static char const *fop_49[] = { "*fmulp %GF,st" };
static char const *fop_50[] = { "*fcomp %GF,st" };
static char const *fop_51[] = { 0, "fcompp", 0, 0, 0, 0, 0, 0 };
static char const *fop_52[] = { "*fsubrp %GF,st" };
static char const *fop_53[] = { "*fsubp %GF,st" };
static char const *fop_54[] = { "*fdivrp %GF,st" };
static char const *fop_55[] = { "*fdivp %GF,st" };
static char const *fop_56[] = { "*ffreep %GF" };
static char const *fop_60[] = { "fstsw ax", 0, 0, 0, 0, 0, 0, 0 };

static char const **fspecial[] = { /* 0=use st(i), 1=undefined 0 in fop_* means undefined */
  0, 0, 0, 0, 0, 0, 0, 0,
  fop_8, fop_9, fop_10, fop_11, fop_12, fop_13, fop_14, fop_15,
  f0, f0, f0, f0, f0, fop_21, f0, f0,
  f0, f0, f0, f0, fop_28, f0, f0, f0,
  fop_32, fop_33, fop_34, fop_35, fop_36, fop_37, fop_38, fop_39,
  fop_40, fop_41, fop_42, fop_43, fop_44, fop_45, f0, f0,
  fop_48, fop_49, fop_50, fop_51, fop_52, fop_53, fop_54, fop_55,
  fop_56, f0, f0, f0, fop_60, f0, f0, f0,
};

static const char *floatops[] = { /* assumed " %EF" at end of each.  mod != 3 only */
/*00*/ "fadd", "fmul", "fcom", "fcomp",
       "fsub", "fsubr", "fdiv", "fdivr",
/*08*/ "fld", 0, "fst", "fstp",
       "fldenv", "fldcw", "fstenv", "fstcw",
/*16*/ "fiadd", "fimul", "ficomw", "ficompw",
       "fisub", "fisubr", "fidiv", "fidivr",
/*24*/ "fild", 0, "fist", "fistp",
       "frstor", "fldt", 0, "fstpt",
/*32*/ "faddq", "fmulq", "fcomq", "fcompq",
       "fsubq", "fsubrq", "fdivq", "fdivrq",
/*40*/ "fldq", 0, "fstq", "fstpq",
       0, 0, "fsave", "fstsw",
/*48*/ "fiaddw", "fimulw", "ficomw", "ficompw",
       "fisubw", "fisubrw", "fidivw", "fidivr",
/*56*/ "fildw", 0, "fistw", "fistpw",
       "fbldt", "fildq", "fbstpt", "fistpq"
};

static char *addr_to_hex(UINT32 addr, int splitup) {
  static char buffer[11];

  if (splitup) {
    if (fp_segment(addr)==0 || fp_offset(addr)==0xffff) /* 'coz of wraparound */
      sprintf(buffer, "%04X", (unsigned)fp_offset(addr) );
    else
      sprintf(buffer, "%04X:%04X", (unsigned)fp_segment(addr), (unsigned)fp_offset(addr) );
  } else {
#if 0
	  /* Pet outcommented, reducing address size to 4
		 when segment is 0 or 0xffff */
    if (fp_segment(addr)==0 || fp_segment(addr)==0xffff) /* 'coz of wraparound */
      sprintf(buffer, "%04X", (unsigned)fp_offset(addr) );
    else
#endif

	sprintf(buffer, "%08X", addr );
	
  }

  return buffer;
}

static PhysPt getbyte_mac;
static PhysPt startPtr;

static UINT8 getbyte(void) {
    uint8_t c;

	if (!mem_readb_checked(getbyte_mac++,&c))
        return c;

    return 0xFF;
}

/*
   only one modrm or sib byte per instruction, tho' they need to be
   returned a few times...
*/

static int modrm(void)
{
  if (modrmv == -1)
    modrmv = getbyte();
  return modrmv;
}

static int sib(void)
{
  if (sibv == -1)
    sibv = getbyte();
  return sibv;
}

/*------------------------------------------------------------------------*/

static void uprintf(char const *s, ...)
{
	va_list	arg_ptr;
	va_start (arg_ptr, s);
	vsprintf(ubufp, s, arg_ptr);
	va_end(arg_ptr);
	while (*ubufp)
		ubufp++;
}

static void uputchar(char c)
{
  *ubufp++ = c;
  *ubufp = 0;
}

/*------------------------------------------------------------------------*/

static int bytes(char c)
{
  switch (c) {
  case 'b':
       return 1;
  case 'w':
       return 2;
  case 'd':
       return 4;
  case 'v':
       if (opsize == 32)
         return 4;
       else
         return 2;
  }
  return 0;
}

/*------------------------------------------------------------------------*/
static void outhex(char subtype, int extend, int optional, int defsize, int sign)
{
  int n=0, s=0, i;
  INT32 delta = 0;
  unsigned char buff[6];
  char  signchar;

  switch (subtype) {
  case 'q':
       if (wordop) {
         if (opsize==16) {
           n = 2;
         } else {
           n = 4;
         }
       } else {
         n = 1;
       }
       break;

  case 'a':
       break;
  case 'x':
       extend = 2;
       n = 1;
       break;
  case 'b':
       n = 1;
       break;
  case 'w':
       n = 2;
       break;
  case 'd':
       n = 4;
       break;
  case 's':
       n = 6;
       break;
  case 'c':
  case 'v':
       if (defsize == 32)
         n = 4;
       else
         n = 2;
       break;
  case 'p':
       if (defsize == 32)
         n = 6;
       else
         n = 4;
       s = 1;
       break;
  }
  for (i=0; i<n; i++)
    buff[i] = getbyte();
  for (; i<extend; i++)
    buff[i] = (buff[i-1] & 0x80) ? 0xff : 0;
  if (s) {
    uprintf("%02X%02X:", (unsigned)buff[n-1], (unsigned)buff[n-2]);
    n -= 2;
  }
  switch (n) {
  case 1:
       delta = le_int8(buff);
       break;
  case 2:
       delta = le_int16(buff);
       break;
  case 4:
       delta = le_int32(buff);
       break;
  }
  if (extend > n) {
    if (subtype!='x') {
      if (delta<0) {
        delta = -delta;
        signchar = '-';
      } else
        signchar = '+';
      if (delta || !optional)
		uprintf("%c%0*lX", (char)signchar, (int)(extend), (long)delta);
    } else {
      if (extend==2)
        delta = (UINT16)delta;
	  uprintf("%0.*lX", (int)(2*extend), (long)delta );
    }
    return;
  }
  if ((n == 4) && !sign) {
    char *name = addr_to_hex((UINT32)delta, 0);
    uprintf("%s", name);
    return;
  }
  switch (n) {
  case 1:
       if (sign && (char)delta<0) {
         delta = -delta;
         signchar = '-';
       } else
         signchar = '+';
       if (sign)
		 uprintf("%c%02lX", (char)signchar, delta & 0xFFL);
       else
		 uprintf("%02lX", delta & 0xFFL);
       break;

  case 2:
       if (sign && delta<0) {
         signchar = '-';
         delta = -delta;
       } else
         signchar = '+';
       if (sign)
		 uprintf("%c%04lX", (char)signchar, delta & 0xFFFFL);
       else
		 uprintf("%04lX", delta & 0xFFFFL);
       break;

  case 4:
       if (sign && delta<0) {
         delta = -delta;
         signchar = '-';
       } else
         signchar = '+';
       if (sign)
		 uprintf("%c%08lX", (char)signchar, (unsigned long)delta & 0xFFFFFFFFL);
       else
		 uprintf("%08lX", (unsigned long)delta & 0xFFFFFFFFL);
       break;
  }
}


/*------------------------------------------------------------------------*/

static void reg_name(int regnum, char size)
{
  if (size == 'F') { /* floating point register? */
    uprintf("st(%d)", regnum);
    return;
  }
  if ((((size == 'c') || (size == 'v')) && (opsize == 32)) || (size == 'd'))
    uputchar('e');
  if ((size=='q' || size == 'b' || size=='c') && !wordop) {
    uputchar("acdbacdb"[regnum]);
    uputchar("llllhhhh"[regnum]);
  } else {
    uputchar("acdbsbsd"[regnum]);
    uputchar("xxxxppii"[regnum]);
  }
}


/*------------------------------------------------------------------------*/

static void ua_str(char const *str);

static void do_sib(int m)
{
  int s, i, b;

  s = SCALE(sib());
  i = INDEX(sib());
  b = BASE(sib());
  switch (b) {     /* pick base */
  case 0: ua_str("%p:[eax"); break;
  case 1: ua_str("%p:[ecx"); break;
  case 2: ua_str("%p:[edx"); break;
  case 3: ua_str("%p:[ebx"); break;
  case 4: ua_str("%p:[esp"); break;
  case 5:
       if (m == 0) {
         ua_str("%p:[");
         outhex('d', 4, 0, addrsize, 0);
       } else {
         ua_str("%p:[ebp");
       }
       break;
  case 6: ua_str("%p:[esi"); break;
  case 7: ua_str("%p:[edi"); break;
  }
  switch (i) {     /* and index */
  case 0: uprintf("+eax"); break;
  case 1: uprintf("+ecx"); break;
  case 2: uprintf("+edx"); break;
  case 3: uprintf("+ebx"); break;
  case 4: break;
  case 5: uprintf("+ebp"); break;
  case 6: uprintf("+esi"); break;
  case 7: uprintf("+edi"); break;
  }
  if (i != 4) {
    switch (s) {    /* and scale */
      case 0: /*uprintf("");*/ break;
      case 1: uprintf("*2"); break;
      case 2: uprintf("*4"); break;
      case 3: uprintf("*8"); break;
    }
  }
}



/*------------------------------------------------------------------------*/
static void do_modrm(char subtype)
{
  int mod = MOD(modrm());
  int rm = RM(modrm());
  int extend = (addrsize == 32) ? 4 : 2;

  if (mod == 3) { /* specifies two registers */
    reg_name(rm, subtype);
    return;
  }
  if (must_do_size) {
    if (wordop) {
      if (addrsize==32 || opsize==32) {       /* then must specify size */
		ua_str("dword ");
      } else {
		ua_str("word ");
      }
    } else {
	  ua_str("byte ");
    }
  }
  if ((mod == 0) && (rm == 5) && (addrsize == 32)) {/* mem operand with 32 bit ofs */
    ua_str("%p:[");
    outhex('d', extend, 0, addrsize, 0);
    uputchar(']');
    return;
  }
  if ((mod == 0) && (rm == 6) && (addrsize == 16)) { /* 16 bit dsplcmnt */
    ua_str("%p:[");
    outhex('w', extend, 0, addrsize, 0);
    uputchar(']');
    return;
  }
  if ((addrsize != 32) || (rm != 4))
    ua_str("%p:[");
  if (addrsize == 16) {
    switch (rm) {
    case 0: uprintf("bx+si"); break;
    case 1: uprintf("bx+di"); break;
    case 2: uprintf("bp+si"); break;
    case 3: uprintf("bp+di"); break;
    case 4: uprintf("si"); break;
    case 5: uprintf("di"); break;
    case 6: uprintf("bp"); break;
    case 7: uprintf("bx"); break;
    }
  } else {
    switch (rm) {
    case 0: uprintf("eax"); break;
    case 1: uprintf("ecx"); break;
    case 2: uprintf("edx"); break;
    case 3: uprintf("ebx"); break;
    case 4: do_sib(mod); break;
    case 5: uprintf("ebp"); break;
    case 6: uprintf("esi"); break;
    case 7: uprintf("edi"); break;
    }
  }
  switch (mod) {
  case 1:
       outhex('b', extend, 1, addrsize, 0);
       break;
  case 2:
       outhex('v', extend, 1, addrsize, 1);
       break;
  }
  uputchar(']');
}



/*------------------------------------------------------------------------*/
static void floating_point(int e1)
{
  int esc = e1*8 + REG(modrm());

  if ((MOD(modrm()) == 3)&&fspecial[esc]) {
    if (fspecial[esc][0]) {
      if (fspecial[esc][0][0] == '*') {
        ua_str(fspecial[esc][0]+1);
      } else {
        ua_str(fspecial[esc][RM(modrm())]);
      }
    } else {
      ua_str(floatops[esc]);
      ua_str(" %EF");
    }
  } else {
    ua_str(floatops[esc]);
    ua_str(" %EF");
  }
}


/*------------------------------------------------------------------------*/
/* Main table driver                                                      */

#define INSTRUCTION_SIZE ( (int)getbyte_mac - (int)startPtr )

static void percent(char type, char subtype)
{
  INT32 vofs = 0;
  char *name=NULL;
  int extend = (addrsize == 32) ? 4 : 2;
  UINT8 c;

  switch (type) {
  case 'A':                          /* direct address */
       outhex(subtype, extend, 0, addrsize, 0);
       break;

  case 'C':                          /* reg(r/m) picks control reg */
       uprintf("CR%d", REG(modrm()));
       must_do_size = 0;
       break;

  case 'D':                          /* reg(r/m) picks debug reg */
       uprintf("DR%d", REG(modrm()));
       must_do_size = 0;
       break;

  case 'E':                          /* r/m picks operand */
       do_modrm(subtype);
       break;

  case 'G':                          /* reg(r/m) picks register */
       if (subtype == 'F')                 /* 80*87 operand?   */
         reg_name(RM(modrm()), subtype);
       else
         reg_name(REG(modrm()), subtype);
       must_do_size = 0;
       break;

  case 'I':                            /* immed data */
       outhex(subtype, 0, 0, opsize, 0);
       break;

  case 'J':                            /* relative IP offset */
       switch (bytes(subtype)) {              /* sizeof offset value */
       case 1:
            vofs = (INT8)getbyte();
			name = addr_to_hex((UINT32)vofs+(UINT32)instruction_offset+(UINT32)INSTRUCTION_SIZE,0);
            break;
       case 2:
            vofs  = (INT32)((UINT32)getbyte());
            vofs |= (INT32)((UINT32)getbyte() << 8);
            vofs  = (INT16)vofs;
			name  = addr_to_hex((UINT32)vofs+(UINT32)instruction_offset+(UINT32)INSTRUCTION_SIZE,0);
            break;
			/* i386 */
       case 4:
            vofs  = (INT32)((UINT32)getbyte());           /* yuk! */
            vofs |= (INT32)((UINT32)getbyte() << 8);
            vofs |= (INT32)((UINT32)getbyte() << 16);
            vofs |= (INT32)((UINT32)getbyte() << 24);
			name = addr_to_hex((UINT32)vofs+(UINT32)instruction_offset+(UINT32)INSTRUCTION_SIZE,(addrsize == 32)?0:1);
            break;
       }
	   if (vofs<0)
		   uprintf("%s ($-%x)", name, -vofs);
	   else
		   uprintf("%s ($+%x)", name, vofs);
       break;

  case 'K':
       switch (subtype) {
       case 'f':
            ua_str("far ");
            break;
       case 'n':
            ua_str("near ");
            break;
       case 's':
            ua_str("short ");
            break;
       }
       break;

  case 'M':                            /* r/m picks memory */
       do_modrm(subtype);
       break;

  case 'O':                            /* offset only */
       ua_str("%p:[");
       outhex(subtype, extend, 0, addrsize, 0);
       uputchar(']');
       break;

  case 'P':                            /* prefix byte (rh) */
       ua_str("%p:");
       break;

  case 'R':                            /* mod(r/m) picks register */
       reg_name(RM(modrm()), subtype);      /* rh */
       must_do_size = 0;
       break;

  case 'S':                            /* reg(r/m) picks segment reg */
       uputchar("ecsdfg"[REG(modrm())]);
       uputchar('s');
       must_do_size = 0;
       break;

  case 'T':                            /* reg(r/m) picks T reg */
       uprintf("tr%d", REG(modrm()));
       must_do_size = 0;
       break;

  case 'X':                            /* ds:si type operator */
       uprintf("ds:[");
       if (addrsize == 32)
         uputchar('e');
       uprintf("si]");
       break;

  case 'Y':                            /* es:di type operator */
       uprintf("es:[");
       if (addrsize == 32)
         uputchar('e');
       uprintf("di]");
       break;

  case '2':                            /* old [pop cs]! now indexes */
       c = getbyte();
       wordop = c & 1;
       ua_str(second[c]);              /* instructions in 386/486   */
       break;

  case 'g':                            /* modrm group `subtype' (0--7) */
       ua_str(groups[subtype-'0'][REG(modrm())]);
       break;

  case 'd':                             /* sizeof operand==dword? */
       if (opsize == 32)
         uputchar('d');
       uputchar(subtype);
       break;

  case 'w':                             /* insert explicit size specifier */
       if (opsize == 32)
         uputchar('d');
       else
         uputchar('w');
       uputchar(subtype);
       break;

  case 'e':                         /* extended reg name */
       if (opsize == 32) {
         if (subtype == 'w')
           uputchar('d');
         else {
           uputchar('e');
           uputchar(subtype);
         }
       } else
         uputchar(subtype);
       break;

  case 'f':                    /* '87 opcode */
       floating_point(subtype-'0');
       break;

  case 'j':
       if (addrsize==32 || opsize==32) /* both of them?! */
         uputchar('e');
       break;

  case 'p':                    /* prefix byte */
       switch (subtype)  {
       case 'c':
       case 'd':
       case 'e':
       case 'f':
       case 'g':
       case 's':
            prefix = subtype;
            c = getbyte();
            wordop = c & 1;
            ua_str((*opmap1)[c]);
            break;
       case ':':
            if (prefix)
              uprintf("%cs:", prefix);
            break;
       case ' ':
            c = getbyte();
            wordop = c & 1;
            ua_str((*opmap1)[c]);
            break;
       }
       break;

  case 's':                           /* size override */
       switch (subtype) {
       case 'a':
            addrsize = 48 - addrsize;
            c = getbyte();
            wordop = c & 1;
            ua_str((*opmap1)[c]);
/*            ua_str(opmap1[getbyte()]); */
            break;
       case 'o':
            opsize = 48 - opsize;
            c = getbyte();
            wordop = c & 1;
            ua_str((*opmap1)[c]);
/*            ua_str(opmap1[getbyte()]); */
            break;
       }
       break;
   }
}


static void ua_str(char const *str)
{
  char c;

  if (str == 0) {
    invalid_opcode = 1;
    uprintf("?");
    return;
  }

  if (strpbrk(str, "CDFGRST")) /* specifiers for registers=>no size 2b specified */
    must_do_size = 0;

  while ((c = *str++) != 0) {
	if (c == ' ' && first_space)
	{
		first_space = 0;
		do
		{
			uputchar(' ');
		} while ( (int)(ubufp - ubufs) < 5 );
	}
	else
    if (c == '%') {
      c = *str++;
      percent(c, *str++);
    } else {
      uputchar(c);
    }
  }
}


Bitu DasmI386(char* buffer, PhysPt pc, uint32_t cur_ip, bool bit32)
{
  	Bitu c;


	instruction_offset = cur_ip;
	/* input buffer */
	startPtr	= pc;
	getbyte_mac = pc;

	/* output buffer */
	ubufs = buffer;
	ubufp = buffer;
	first_space = 1;

	addr32bit=1;

	prefix = 0;
	modrmv = sibv = -1;     /* set modrm and sib flags */
	if (bit32) opsize = addrsize = 32;
	else opsize = addrsize = 16;
	c = getbyte();
	wordop = c & 1;
	must_do_size = 1;
	invalid_opcode = 0;
	opmap1=&op386map1;
	ua_str(op386map1[c]);

  	if (invalid_opcode) {
		/* restart output buffer */
		ubufp = buffer;
		/* invalid instruction, use db xx */
    		uprintf("db %02X", (unsigned)c);
		return 1;
	}

	return getbyte_mac-pc;
}

int DasmLastOperandSize()
{
	return opsize;
}


#endif

