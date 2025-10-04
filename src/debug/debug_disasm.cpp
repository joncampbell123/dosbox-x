
/*
	Ripped out some stuff from the mame release to only make it for 386's 
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

   When writing and debugging this code, I didn't have (and still don't have)
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
#include "paging.h"
#include "logging.h"
#include "cpu.h"

typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;

typedef int8_t  INT8;
typedef int16_t INT16;
typedef int32_t INT32;


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

static uint8_t last_c = 0;

static uint32_t instruction_offset;
//static UINT16 instruction_segment;

static char* ubufs;           /* start of buffer */
static char* ubufp;           /* last position of buffer */
static int invalid_opcode = 0;
static int first_space = 1;
static uint8_t last_prefix = MP_NONE;

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
	M - use MMX regs in mod/rm
	Q - qword
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
  nullptr,            "[loadall]",       "clts",           "[loadall]",
  "invd",             "wbinvd",          nullptr,          "UD2",
  nullptr,            nullptr,           nullptr,          nullptr,
/* 1 */
  "%x0",              "%x0",             "%x0",            "%x0",
  "%x0",              "%x0",             "%x0",            "%x0",
  "%g=",              nullptr,           nullptr,          nullptr,
  nullptr,            nullptr,           nullptr,          nullptr,
/* 2 */
  "mov %Rd,%Cd",      "mov %Rd,%Dd",     "mov %Cd,%Rd",    "mov %Dd,%Rd",
  "mov %Rd,%Td",      nullptr,           "mov %Td,%Rd",    nullptr,
  "%x0",              "%x0",             "%x0",            "%x0",
  "%x0",              "%x0",             "%x0",            "%x0",
/* 3 */
  "wrmsr",            "rdtsc",           "rdmsr",          nullptr,
  "sysenter",         "sysexit",         nullptr,          nullptr,
  nullptr,            nullptr,           nullptr,          nullptr,
  nullptr,            nullptr,           nullptr,          nullptr,
/* 4 */
  "cmovo %Gv,%Ev",    "cmovno %Gv,%Ev",  "cmovc %Gv,%Ev",  "cmovnc %Gv,%Ev",
  "cmovz %Gv,%Ev",    "cmovnz %Gv,%Ev",  "cmovbe %Gv,%Ev", "cmovnbe %Gv,%Ev",
  "cmovs %Gv,%Ev",    "cmovns %Gv,%Ev",  "cmovp %Gv,%Ev",  "cmovnp %Gv,%Ev",
  "cmovl %Gv,%Ev",    "cmovge %Gv,%Ev",  "cmovle %Gv,%Ev", "cmovg %Gv,%Ev",
/* 5 */
  "%x0", "%x0", "%x0",   "%x0",   "%x0", "%x0", "%x0", "%x0",
  "%x0", "%x0", nullptr, nullptr, "%x0", "%x0", "%x0", "%x0",
/* 6 */
  "punpcklbw %GM,%EM","punpcklwd %GM,%EM","punpckldq %GM,%EM","packsswb %GM,%EM",
  "pcmpgtb %GM,%EM",  "pcmpgtw %GM,%EM", "pcmpgtd %GM,%EM","packuswb %GM,%EM",
  "punpckhbw %GM,%EM","punpckhwd %GM,%EM","punpckhdq %GM,%EM","packssdw %GM,%EM",
  nullptr,            nullptr,           "movd %GM,%Ed",   "movq %GM,%EM",
/* 7 */
  "pshufw %GM,%EM,%Ib","%g;",            "%g:",            "%g9",
  "pcmpeqb %GM,%EM",  "pcmpeqw %GM,%EM", "pcmpeqd %GM,%EM","emms",
  nullptr,            nullptr,           nullptr,          nullptr,
  nullptr,            nullptr,           "movd %Ed,%GM",   "movq %EM,%GM",
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
  "shld %Ev,%Gv,%Ib", "shld %Ev,%Gv,cl", nullptr,          nullptr,
  "push gs",          "pop gs",          nullptr,          "bts %Ev,%Gv",
  "shrd %Ev,%Gv,%Ib", "shrd %Ev,%Gv,cl", "%g<",            "imul %Gv,%Ev",
/* b */
  "cmpxchg %Eb,%Gb",  "cmpxchg %Ev,%Gv", "lss %Mp",        "btr %Ev,%Gv",
  "lfs %Mp",          "lgs %Mp",         "movzx %Gv,%Eb",  "movzx %Gv,%Ew",
  nullptr,            nullptr,           "%g7 %Ev,%Ib",    "btc %Ev,%Gv",
  "bsf %Gv,%Ev",      "bsr %Gv,%Ev",     "movsx %Gv,%Eb",  "movsx %Gv,%Ew",
/* c */
  "xadd %Eb,%Gb",     "xadd %Ev,%Gv",    "%x0",            nullptr,
  "%x0",              "%x0",             "%x0",            "%g8",
  "bswap eax",        "bswap ecx",       "bswap edx",      "bswap ebx",
  "bswap esp",        "bswap ebp",       "bswap esi",      "bswap edi",
/* d */
  nullptr,            "psrlw %GM,%EM",   "psrld %GM,%EM",  "psrlq %GM,%EM",
  "paddq %GM,%EM",    "pmullw %GM,%EM",  nullptr,          "%x0",
  "psubusb %GM,%EM",  "psubusw %GM,%EM", "%x0",            "%x0",
  "paddusb %GM,%EM",  "paddusw %GM,%EM", "%x0",            "pandn %GM,%EM",
/* e */
  "%x0",              "psraw %GM,%EM",   "psrad %GM,%EM",  "%x0",
  "%x0",              "%x0",             nullptr,          "%x0",
  "%x0",              "%x0",             "%x0",            "%x0",
  "%x0",              "%x0",             "%x0",            "%x0",
/* f */
  nullptr,            "psllw %GM,%EM",   "pslld %GM,%EM",  "psllq %GM,%EM",
  nullptr,            "pmaddwd %GM,%EM", "%x0",            "%x0",
  "psubb %GM,%EM",    "psubw %GM,%EM",   "psubd %GM,%EM",  nullptr,
  "paddb %GM,%EM",    "paddw %GM,%EM",   "paddd %GM,%EM",  nullptr
};

#define NULLPTRS { nullptr, nullptr, nullptr, nullptr }
static char const *mpgroups[][256][4] = { /* mandatory prefix groups SSE instructions, then within NP, 66, F2, F3 */
/* group 0 */
  {
    /* 0x00 */ NULLPTRS, /* 0x01 */ NULLPTRS, /* 0x02 */ NULLPTRS, /* 0x03 */ NULLPTRS,
    /* 0x04 */ NULLPTRS, /* 0x05 */ NULLPTRS, /* 0x06 */ NULLPTRS, /* 0x07 */ NULLPTRS,
    /* 0x08 */ NULLPTRS, /* 0x09 */ NULLPTRS, /* 0x0A */ NULLPTRS, /* 0x0B */ NULLPTRS,
    /* 0x0C */ NULLPTRS, /* 0x0D */ NULLPTRS, /* 0x0E */ NULLPTRS, /* 0x0F */ NULLPTRS,

    /* 0x10 */ { "movups %GX,%EX", "movupd %GX,%EX", "movsd %GX,%EX", "movss %GX,%EX" },
    /* 0x11 */ { "movups %EX,%GX", "movupd %EX,%GX", "movsd %EX,%GX", "movss %EX,%GX" },
    /* 0x12 */ { "movlps %GX,%EX", nullptr, nullptr, nullptr }, // FIXME: The design of this disassembler does not allow changing opcode name based on r/m = memory or r/m = reg
    /* 0x13 */ { "movlps %EX,%GX", nullptr, nullptr, nullptr },
    /* 0x14 */ { "unpcklps %GX,%EX", nullptr, nullptr, nullptr },
    /* 0x15 */ { "unpckhps %GX,%EX", nullptr, nullptr, nullptr },
    /* 0x16 */ { "movhps %GX,%EX", nullptr, nullptr, nullptr }, // FIXME: The design of this disassembler does not allow changing opcode name based on r/m = memory or r/m = reg
    /* 0x17 */ { "movhps %EX,%GX", nullptr, nullptr, nullptr },
    /* 0x18 */ NULLPTRS, /* 0x19 */ NULLPTRS, /* 0x1A */ NULLPTRS, /* 0x1B */ NULLPTRS,
    /* 0x1C */ NULLPTRS, /* 0x1D */ NULLPTRS, /* 0x1E */ NULLPTRS, /* 0x1F */ NULLPTRS,

    /* 0x20 */ NULLPTRS, /* 0x21 */ NULLPTRS, /* 0x22 */ NULLPTRS, /* 0x23 */ NULLPTRS,
    /* 0x24 */ NULLPTRS, /* 0x25 */ NULLPTRS, /* 0x26 */ NULLPTRS, /* 0x27 */ NULLPTRS,
    /* 0x28 */ { "movaps %GX,%EX", "movapd %GX,%EX", nullptr, nullptr },
    /* 0x29 */ { "movaps %EX,%GX", "movapd %EX,%GX", nullptr, nullptr },
    /* 0x2A */ { "cvtpi2ps %GX,%EM", nullptr, nullptr, "cvtsi2ss %GX,%Ed" },
    /* 0x2B */ { "movntps %EX,%GX", nullptr, nullptr, nullptr },
    /* 0x2C */ { "cvttps2pi %GM,%EX", nullptr, nullptr, "cvttss2si %Gd,%EX" },
    /* 0x2D */ { "cvtps2pi %GM,%EX", nullptr, nullptr, "cvtss2si %Gd,%EX" },
    /* 0x2E */ { "ucomiss %GX,%EX", nullptr, nullptr, nullptr },
    /* 0x2F */ { "comiss %GX,%EX", nullptr, nullptr, nullptr },

    /* 0x30 */ NULLPTRS, /* 0x31 */ NULLPTRS, /* 0x32 */ NULLPTRS, /* 0x33 */ NULLPTRS,
    /* 0x34 */ NULLPTRS, /* 0x35 */ NULLPTRS, /* 0x36 */ NULLPTRS, /* 0x37 */ NULLPTRS,
    /* 0x38 */ NULLPTRS, /* 0x39 */ NULLPTRS, /* 0x3A */ NULLPTRS, /* 0x3B */ NULLPTRS,
    /* 0x3C */ NULLPTRS, /* 0x3D */ NULLPTRS, /* 0x3E */ NULLPTRS, /* 0x3F */ NULLPTRS,

    /* 0x40 */ NULLPTRS, /* 0x41 */ NULLPTRS, /* 0x42 */ NULLPTRS, /* 0x43 */ NULLPTRS,
    /* 0x44 */ NULLPTRS, /* 0x45 */ NULLPTRS, /* 0x46 */ NULLPTRS, /* 0x47 */ NULLPTRS,
    /* 0x48 */ NULLPTRS, /* 0x49 */ NULLPTRS, /* 0x4A */ NULLPTRS, /* 0x4B */ NULLPTRS,
    /* 0x4C */ NULLPTRS, /* 0x4D */ NULLPTRS, /* 0x4E */ NULLPTRS, /* 0x4F */ NULLPTRS,

    /* 0x50 */ { "movmskps %Gd,%EX", nullptr, nullptr, nullptr },
    /* 0x51 */ { "sqrtps %GX,%EX", "sqrtpd %GX,%EX", "sqrtsd %GX,%EX", "sqrtss %GX,%EX" },
    /* 0x52 */ { "rqsrtps %GX,%EX", nullptr, nullptr, "rsqrtss %GX,%EX" },
    /* 0x53 */ { "rcpps %GX,%EX", nullptr, nullptr, "rcpss %GX,%EX" },
    /* 0x54 */ { "andps %GX,%EX", "andpd %GX,%EX", nullptr, nullptr },
    /* 0x55 */ { "andnps %GX,%EX", "andnpd %GX,%EX", nullptr, nullptr },
    /* 0x56 */ { "orps %GX,%EX", "orpd %GX,%EX", nullptr, nullptr },
    /* 0x57 */ { "xorps %GX,%EX", "xorpd %GX,%EX", nullptr, nullptr },
    /* 0x58 */ { "addps %GX,%EX", "addpd %GX,%EX", "addsd %GX,%EX", "addss %GX,%EX" },
    /* 0x59 */ { "mulps %GX,%EX", "mulpd %GX,%EX", "mulsd %GX,%EX", "mulss %GX,%EX" },
    /* 0x5A */ NULLPTRS,
    /* 0x5B */ NULLPTRS,
    /* 0x5C */ { "subps %GX,%EX", "subpd %GX,%EX", "subsd %GX,%EX", "subss %GX,%EX" },
    /* 0x5D */ { "minps %GX,%EX", "minpd %GX,%EX", "minsd %GX,%EX", "minss %GX,%EX" },
    /* 0x5E */ { "divps %GX,%EX", "divpd %GX,%EX", "divsd %GX,%EX", "divss %GX,%EX" },
    /* 0x5F */ { "maxps %GX,%EX", "maxpd %GX,%EX", "maxsd %GX,%EX", "maxss %GX,%EX" },

    /* 0x60 */ NULLPTRS, /* 0x61 */ NULLPTRS, /* 0x62 */ NULLPTRS, /* 0x63 */ NULLPTRS,
    /* 0x64 */ NULLPTRS, /* 0x65 */ NULLPTRS, /* 0x66 */ NULLPTRS, /* 0x67 */ NULLPTRS,
    /* 0x68 */ NULLPTRS, /* 0x69 */ NULLPTRS, /* 0x6A */ NULLPTRS, /* 0x6B */ NULLPTRS,
    /* 0x6C */ NULLPTRS, /* 0x6D */ NULLPTRS, /* 0x6E */ NULLPTRS, /* 0x6F */ NULLPTRS,

    /* 0x70 */ NULLPTRS, /* 0x71 */ NULLPTRS, /* 0x72 */ NULLPTRS, /* 0x73 */ NULLPTRS,
    /* 0x74 */ NULLPTRS, /* 0x75 */ NULLPTRS, /* 0x76 */ NULLPTRS, /* 0x77 */ NULLPTRS,
    /* 0x78 */ NULLPTRS, /* 0x79 */ NULLPTRS, /* 0x7A */ NULLPTRS, /* 0x7B */ NULLPTRS,
    /* 0x7C */ NULLPTRS, /* 0x7D */ NULLPTRS, /* 0x7E */ NULLPTRS, /* 0x7F */ NULLPTRS,

    /* 0x80 */ NULLPTRS, /* 0x81 */ NULLPTRS, /* 0x82 */ NULLPTRS, /* 0x83 */ NULLPTRS,
    /* 0x84 */ NULLPTRS, /* 0x85 */ NULLPTRS, /* 0x86 */ NULLPTRS, /* 0x87 */ NULLPTRS,
    /* 0x88 */ NULLPTRS, /* 0x89 */ NULLPTRS, /* 0x8A */ NULLPTRS, /* 0x8B */ NULLPTRS,
    /* 0x8C */ NULLPTRS, /* 0x8D */ NULLPTRS, /* 0x8E */ NULLPTRS, /* 0x8F */ NULLPTRS,

    /* 0x90 */ NULLPTRS, /* 0x91 */ NULLPTRS, /* 0x92 */ NULLPTRS, /* 0x93 */ NULLPTRS,
    /* 0x94 */ NULLPTRS, /* 0x95 */ NULLPTRS, /* 0x96 */ NULLPTRS, /* 0x97 */ NULLPTRS,
    /* 0x98 */ NULLPTRS, /* 0x99 */ NULLPTRS, /* 0x9A */ NULLPTRS, /* 0x9B */ NULLPTRS,
    /* 0x9C */ NULLPTRS, /* 0x9D */ NULLPTRS, /* 0x9E */ NULLPTRS, /* 0x9F */ NULLPTRS,

    /* 0xA0 */ NULLPTRS, /* 0xA1 */ NULLPTRS, /* 0xA2 */ NULLPTRS, /* 0xA3 */ NULLPTRS,
    /* 0xA4 */ NULLPTRS, /* 0xA5 */ NULLPTRS, /* 0xA6 */ NULLPTRS, /* 0xA7 */ NULLPTRS,
    /* 0xA8 */ NULLPTRS, /* 0xA9 */ NULLPTRS, /* 0xAA */ NULLPTRS, /* 0xAB */ NULLPTRS,
    /* 0xAC */ NULLPTRS, /* 0xAD */ NULLPTRS, /* 0xAE */ NULLPTRS, /* 0xAF */ NULLPTRS,

    /* 0xB0 */ NULLPTRS, /* 0xB1 */ NULLPTRS, /* 0xB2 */ NULLPTRS, /* 0xB3 */ NULLPTRS,
    /* 0xB4 */ NULLPTRS, /* 0xB5 */ NULLPTRS, /* 0xB6 */ NULLPTRS, /* 0xB7 */ NULLPTRS,
    /* 0xB8 */ NULLPTRS, /* 0xB9 */ NULLPTRS, /* 0xBA */ NULLPTRS, /* 0xBB */ NULLPTRS,
    /* 0xBC */ NULLPTRS, /* 0xBD */ NULLPTRS, /* 0xBE */ NULLPTRS, /* 0xBF */ NULLPTRS,

    /* 0xC0 */ NULLPTRS,
    /* 0xC1 */ NULLPTRS,
    /* 0xC2 */ { "cmpps %GX,%EX,%Ib", "cmppd %GX,%EX,%Ib", "cmpsd %GX,%EX,%Ib", "cmpss %GX,%EX,%Ib" }, // FIXME: The immediate byte specifies a comparison operator
    /* 0xC3 */ NULLPTRS,
    /* 0xC4 */ { "pinsrw %GM,%Ed,%Ib", "pinsrw %GX,%Ed,%Ib", nullptr, nullptr },
    /* 0xC5 */ { "pextrw %Ed,%GM,%Ib", "pextrw %Ed,%GX,%Ib", nullptr, nullptr },
    /* 0xC6 */ { "shufps %GX,%EX,%Ib", "shufpd %GX,%EX,%Ib", nullptr, nullptr },
    /* 0xC7 */ NULLPTRS,
    /* 0xC8 */ NULLPTRS, /* 0xC9 */ NULLPTRS, /* 0xCA */ NULLPTRS, /* 0xCB */ NULLPTRS,
    /* 0xCC */ NULLPTRS, /* 0xCD */ NULLPTRS, /* 0xCE */ NULLPTRS, /* 0xCF */ NULLPTRS,

    /* 0xD0 */ NULLPTRS,
    /* 0xD1 */ NULLPTRS,
    /* 0xD2 */ NULLPTRS,
    /* 0xD3 */ NULLPTRS,
    /* 0xD4 */ NULLPTRS,
    /* 0xD5 */ NULLPTRS,
    /* 0xD6 */ NULLPTRS,
    /* 0xD7 */ { "pmovmskb %Gd,%EM", "pmovmskb %Gd,%EX", nullptr, nullptr },
    /* 0xD8 */ NULLPTRS,
    /* 0xD9 */ NULLPTRS,
    /* 0xDA */ { "pminub %GM,%EM", "pminub %GX,%EX", nullptr, nullptr },
    /* 0xDB */ { "pand %GM,%EM", "pand %GX,%EX", nullptr, nullptr },
    /* 0xDC */ NULLPTRS,
    /* 0xDD */ NULLPTRS,
    /* 0xDE */ { "pmaxub %GM,%EM", "pmaxub %GX,%EX", nullptr, nullptr },
    /* 0xDF */ NULLPTRS,

    /* 0xE0 */ { "pavgb %GM,%EM", "pavgb %GX,%EX", nullptr, nullptr },
    /* 0xE1 */ NULLPTRS,
    /* 0xE2 */ NULLPTRS,
    /* 0xE3 */ { "pavgw %GM,%EM", "pavgw %GX,%EX", nullptr, nullptr },
    /* 0xE4 */ { "pmulhuw %GM,%EM", "pmulhuw %GX,%EX", nullptr, nullptr },
    /* 0xE5 */ { "pmulhw %GM,%EM", "pmulhw %GX,%EX", nullptr, nullptr },
    /* 0xE6 */ NULLPTRS,
    /* 0xE7 */ { "movntq %EM,%GM", "movntdq %EX,%GX", nullptr, nullptr },
    /* 0xE8 */ { "psubsb %GM,%EM", "psubsb %GX,%EX", nullptr, nullptr },
    /* 0xE9 */ { "psubsw %GM,%EM", "psubsw %GX,%EX", nullptr, nullptr },
    /* 0xEA */ { "pminsw %GM,%EM", "pminsw %GX,%EX", nullptr, nullptr },
    /* 0xEB */ { "por %GM,%EM", "por %GX,%EX", nullptr, nullptr },
    /* 0xEC */ { "paddsb %GM,%EM", "paddsb %GX,%EX", nullptr, nullptr },
    /* 0xED */ { "paddsw %GM,%EM", "paddsw %GX,%EX", nullptr, nullptr },
    /* 0xEE */ { "pmaxsw %GM,%EM", "pmaxsw %GX,%EX", nullptr, nullptr },
    /* 0xEF */ { "pxor %GM,%EM", "pxor %GX,%EX", nullptr, nullptr },

    /* 0xF0 */ NULLPTRS, /* 0xF1 */ NULLPTRS, /* 0xF2 */ NULLPTRS, /* 0xF3 */ NULLPTRS,
    /* 0xF4 */ NULLPTRS,
    /* 0xF5 */ NULLPTRS,
    /* 0xF6 */ { "psadbw %GM,%EM", "psadbw %GX,%EX", nullptr, nullptr },
    /* 0xF7 */ { "maskmovq %GM,%EM", "maskmovdqu %GX,%EX", nullptr, nullptr },
    /* 0xF8 */ NULLPTRS, /* 0xF9 */ NULLPTRS, /* 0xFA */ NULLPTRS, /* 0xFB */ NULLPTRS,
    /* 0xFC */ NULLPTRS, /* 0xFD */ NULLPTRS, /* 0xFE */ NULLPTRS, /* 0xFF */ NULLPTRS
  }
};
#undef NULLPTRS

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
  { "inc %Eb",        "dec %Eb",         nullptr,          nullptr,
    nullptr,          nullptr,           nullptr,          "callback %Iw"  },
/* 4 */
  { "inc %Ev",        "dec %Ev",         "call %Kn%Ev",    "call %Kf%Ep",
    "jmp %Kn%Ev",     "jmp %Kf%Ep",      "push %Ev",       nullptr         },
/* 5 */
  { "sldt %Ew",       "str %Ew",         "lldt %Ew",       "ltr %Ew",
    "verr %Ew",       "verw %Ew",        nullptr,          nullptr         },
/* 6 */
  { "sgdt %Ms",       "sidt %Ms",        "lgdt %Ms",       "lidt %Ms",
    "smsw %Ew",       nullptr,           "lmsw %Ew",       "invlpg"        },
/* 7 */
  { nullptr,          nullptr,           nullptr,          nullptr,
    "bt",             "bts",             "btr",            "btc"           },
/* 8 */
  { nullptr,          "cmpxchg8b %EQ",   nullptr,          nullptr,
    nullptr,          nullptr,           nullptr,          nullptr         },
/* 9 */
  { nullptr,          nullptr,           "psrlq %EM,%Ib",  nullptr,
    nullptr,          nullptr,           "psllq %EM,%Ib",  nullptr         },
/* : (NTS: this is '0'+10 in ASCII) */
  { nullptr,          nullptr,           "psrld %EM,%Ib",  nullptr,
    "psrad %EM,%Ib",  nullptr,           "pslld %EM,%Ib",  nullptr         },
/* ; (NTS: this is '0'+11 in ASCII) */
  { nullptr,          nullptr,           "psrlw %EM,%Ib",  nullptr,
    "psraw %EM,%Ib",  nullptr,           "psllw %EM,%Ib",  nullptr         },
/* < (NTS: this is '0'+12 in ASCII) */
  { "fxsave %EM",     "fxrstor %EM",     "ldmxcsr %EM",    "stmxcsr %EM",
    nullptr,          nullptr,           nullptr,          "sfence"        },
/* = (NTS: this is '0'+13 in ASCII) */
  { "prefetchnta %EM","prefetch0 %EM",   "prefetch1 %EM",  "prefetch2 %EM",
    nullptr,          nullptr,           nullptr,          nullptr         }
};

/* zero here means invalid.  If first entry starts with '*', use st(i) */
/* no assumed %EFs here.  Indexed by RM(modrm())                       */
static char const *f0[]     = { nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr,};
static char const *fop_8[]  = { "*fld st,%GF" };
static char const *fop_9[]  = { "*fxch st,%GF" };
static char const *fop_10[] = { "fnop", nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr };
static char const *fop_11[] = { "*fstp st,%GF" };
static char const *fop_12[] = { "fchs", "fabs", nullptr, nullptr,
                                "ftst", "fxam", nullptr, nullptr };
static char const *fop_13[] = { "fld1", "fldl2t", "fldl2e", "fldpi",
                                "fldlg2", "fldln2", "fldz", nullptr };
static char const *fop_14[] = { "f2xm1", "fyl2x", "fptan", "fpatan",
                                "fxtract", "fprem1", "fdecstp", "fincstp" };
static char const *fop_15[] = { "fprem", "fyl2xp1", "fsqrt", "fsincos",
                                "frndint", "fscale", "fsin", "fcos" };
static char const *fop_21[] = { nullptr, "fucompp", nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr };
static char const *fop_28[] = { "[fneni]", "[fndisi]", "fclex", "finit",
                                "[fnsetpm]", "[frstpm]", nullptr, nullptr };
static char const* fop_29[] = { "*fucomi %GF" };
static char const* fop_30[] = { "*fcomi %GF" };
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
static char const *fop_51[] = { nullptr, "fcompp", nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr };
static char const *fop_52[] = { "*fsubrp %GF,st" };
static char const *fop_53[] = { "*fsubp %GF,st" };
static char const *fop_54[] = { "*fdivrp %GF,st" };
static char const *fop_55[] = { "*fdivp %GF,st" };
static char const *fop_56[] = { "*ffreep %GF" };
static char const *fop_57[] = { "*fxch %GF" };
static char const *fop_58[] = { "*fstp %GF" };
static char const *fop_59[] = { "*fstp %GF" };
static char const *fop_60[] = { "fstsw ax", nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr };
static char const* fop_61[] = { "*fucomip %GF" };
static char const* fop_62[] = { "*fcomip %GF" };

static char const **fspecial[] = { /* 0=use st(i), 1=undefined 0 in fop_* means undefined */
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  fop_8, fop_9, fop_10, fop_11, fop_12, fop_13, fop_14, fop_15,
  f0, f0, f0, f0, f0, fop_21, f0, f0,
  f0, f0, f0, f0, fop_28, fop_29, fop_30, f0,
  fop_32, fop_33, fop_34, fop_35, fop_36, fop_37, fop_38, fop_39,
  fop_40, fop_41, fop_42, fop_43, fop_44, fop_45, f0, f0,
  fop_48, fop_49, fop_50, fop_51, fop_52, fop_53, fop_54, fop_55,
  fop_56, fop_57, fop_58, fop_59, fop_60, fop_61, fop_62, f0,
};

static const char *floatops[] = { /* assumed " %EF" at end of each.  mod != 3 only */
/*00*/ "fadd", "fmul", "fcom", "fcomp",
       "fsub", "fsubr", "fdiv", "fdivr",
/*08*/ "fld", nullptr, "fst", "fstp",
       "fldenv", "fldcw", "fstenv", "fstcw",
/*16*/ "fiadd", "fimul", "ficom", "ficomp",
       "fisub", "fisubr", "fidiv", "fidivr",
/*24*/ "fild", "fisttp", "fist", "fistp",
       nullptr, "fldt", nullptr, "fstpt",
/*32*/ "faddq", "fmulq", "fcomq", "fcompq",
       "fsubq", "fsubrq", "fdivq", "fdivrq",
/*40*/ "fldq", "fisttpq", "fstq", "fstpq",
       "frstor", nullptr, "fsave", "fstsw",
/*48*/ "fiaddw", "fimulw", "ficomw", "ficompw",
       "fisubw", "fisubrw", "fidivw", "fidivrw",
/*56*/ "fildw", "fisttpw", "fistw", "fistpw",
       "fbldt", "fildq", "fbstpt", "fistpq"
};

static char *addr_to_hex(UINT32 addr, int splitup) {
  static char buffer[11];

  if (splitup) {
    if (fp_segment(addr)==0 || fp_offset(addr)==0xffff) /* 'coz of wraparound */
      sprintf(buffer, "%04X", fp_offset(addr) );
    else
      sprintf(buffer, "%04X:%04X", fp_segment(addr), fp_offset(addr) );
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

static void ua_backspace_repe(void) {
  char *s = ubufp - 1;

  while (s >= ubufs && (*s == ' ')) s--;
  while (s >= ubufs && (*s != ' ')) s--;
  s++;

  if (s < ubufs) return;

  if (!strncmp(s,"repe ",5) || !strncmp(s,"repne ",6)) {
    /* set write pointer here, to overwrite it */
    *s = 0; ubufp = s;
  }
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
		uprintf("%c%0*lX", signchar, extend, (long)delta);
    } else {
      if (extend==2)
        delta = (UINT16)delta;
	  uprintf("%0.*lX", (2*extend), (long)delta );
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
		 uprintf("%c%02lX", signchar, delta & 0xFFL);
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
		 uprintf("%c%04lX", signchar, delta & 0xFFFFL);
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
		 uprintf("%c%08lX", signchar, (unsigned long)delta & 0xFFFFFFFFL);
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
  if (size == 'M') { /* MMX register */
    uprintf("mm%d", regnum);
    return;
  }
  if (size == 'X') { /* SSE register */
    uprintf("xmm%d", regnum);
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
    if (subtype == 'Q') {
	  ua_str("qword ");
    }
    else if (wordop) {
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

  if ((MOD(modrm()) == 3) && fspecial[esc]) {
    if (fspecial[esc][0] && fspecial[esc][0][0] == '*') {
      ua_str(fspecial[esc][0]+1);
    } else {
      ua_str(fspecial[esc][RM(modrm())]);
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
       outhex(subtype, extend, 0, opsize, 0);
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
			name = addr_to_hex((UINT32)vofs+instruction_offset+(UINT32)INSTRUCTION_SIZE,0);
            break;
       case 2:
            vofs  = (INT32)((UINT32)getbyte());
            vofs |= (INT32)((UINT32)getbyte() << 8);
            vofs  = (INT16)vofs;
			name  = addr_to_hex((UINT32)vofs+instruction_offset+(UINT32)INSTRUCTION_SIZE,0);
            break;
			/* i386 */
       case 4:
            vofs  = (INT32)((UINT32)getbyte());           /* yuk! */
            vofs |= (INT32)((UINT32)getbyte() << 8);
            vofs |= (INT32)((UINT32)getbyte() << 16);
            vofs |= (INT32)((UINT32)getbyte() << 24);
			name = addr_to_hex((UINT32)vofs+instruction_offset+(UINT32)INSTRUCTION_SIZE,(addrsize == 32)?0:1);
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
       uputchar("ecsdfg??"[REG(modrm())]);
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
       last_c = c;
       wordop = c & 1;
       ua_str(second[c]);              /* instructions in 386/486   */
       break;

  case 'x':
       /* problem: for SSE opcodes with REPE/REPNE mandatory prefix this code will have already written repne/repe so that needs to be wiped out of the buffer */
       if (last_prefix == MP_F2 || last_prefix == MP_F3) ua_backspace_repe();
       ua_str(mpgroups[subtype-'0'][last_c][last_prefix]);
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
       switch (last_c) {
         case 0xF2: last_prefix = MP_F2; break;
         case 0xF3: last_prefix = MP_F3; break;
         default:   last_prefix = MP_NONE; break;
       };
       switch (subtype)  {
       case 'c':
       case 'd':
       case 'e':
       case 'f':
       case 'g':
       case 's':
            prefix = subtype;
            c = getbyte();
	    last_c = c;
	    wordop = c & 1;
            ua_str((*opmap1)[c]);
            break;
       case ':':
            if (prefix)
              uprintf("%cs:", prefix);
            break;
       case ' ':
            c = getbyte();
	    last_c = c;
            wordop = c & 1;
            ua_str((*opmap1)[c]);
            break;
       }
       break;

  case 's':                           /* size override */
       switch (last_c) {
         case 0x66: last_prefix = MP_66; break;
         default:   last_prefix = MP_NONE; break;
       };
       switch (subtype) {
       case 'a':
            addrsize = 48 - addrsize;
            c = getbyte();
	    last_c = c;
            wordop = c & 1;
            ua_str((*opmap1)[c]);
/*            ua_str(opmap1[getbyte()]); */
            break;
       case 'o':
            opsize = 48 - opsize;
            c = getbyte();
	    last_c = c;
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

  if (!str) {
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
	last_c = c;
	wordop = c & 1;
	must_do_size = 1;
	invalid_opcode = 0;
	last_prefix = MP_NONE;
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

