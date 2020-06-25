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



// #define WEAK_EXCEPTIONS


#if defined (_MSC_VER)

#ifdef WEAK_EXCEPTIONS
#define clx
#else
#define clx fclex
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_LOAD(op,szI,szA)			\
		__asm {							\
		__asm	mov		ebx, store_to	\
		__asm	shl		ebx, 4			\
		__asm	op		szI PTR fpu.p_regs[128].m1		\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		}
#else
#define FPUD_LOAD(op,szI,szA)			\
		Bit16u new_sw;					\
		__asm {							\
		__asm	mov		eax, 8			\
		__asm	shl		eax, 4			\
		__asm	mov		ebx, store_to	\
		__asm	shl		ebx, 4			\
		__asm	fclex					\
		__asm	op		szI PTR fpu.p_regs[eax].m1		\
		__asm	fnstsw	new_sw			\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		}								\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_LOAD_EA(op,szI,szA)		\
		__asm {							\
		__asm	op		szI PTR fpu.p_regs[128].m1		\
		}
#else
#define FPUD_LOAD_EA(op,szI,szA)		\
		Bit16u new_sw;					\
		__asm {							\
		__asm	mov		eax, 8			\
		__asm	shl		eax, 4			\
		__asm	fclex					\
		__asm	op		szI PTR fpu.p_regs[eax].m1		\
		__asm	fnstsw	new_sw			\
		}								\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_STORE(op,szI,szA)				\
		Bit16u save_cw;						\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	mov		eax, TOP			\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	shl		eax, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	op		szI PTR fpu.p_regs[128].m1		\
		__asm	fldcw	save_cw				\
		}
#else
#define FPUD_STORE(op,szI,szA)				\
		Bit16u new_sw,save_cw;				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	mov		eax, TOP			\
		__asm	shl		eax, 4				\
		__asm	mov		ebx, 8				\
		__asm	shl		ebx, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx							\
		__asm	op		szI PTR fpu.p_regs[ebx].m1		\
		__asm	fnstsw	new_sw				\
		__asm	fldcw	save_cw				\
		}									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fsin,fcos,f2xm1,fchs,fabs
#define FPUD_TRIG(op)				\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx					\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		}							\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fsincos
#define FPUD_SINCOS()				\
		Bit16u new_sw;					\
		__asm {							\
		__asm	mov		eax, TOP		\
		__asm	mov		ebx, eax		\
		__asm	dec     ebx				\
		__asm	and     ebx, 7			\
		__asm	shl		eax, 4			\
		__asm	shl		ebx, 4			\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx						\
		__asm	fsincos					\
		__asm	fnstsw	new_sw			\
		__asm	mov		cx, new_sw		\
		__asm	and		ch, 0x04 		\
		__asm	jnz		argument_too_large1				\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	jmp		end_sincos		\
		__asm	argument_too_large1:	\
		__asm	fstp	st(0)			\
		__asm	end_sincos:				\
		}												\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		if ((new_sw&0x0400)==0) FPU_PREP_PUSH();

// handles fptan
#define FPUD_PTAN()					\
		Bit16u new_sw;					\
		__asm {							\
		__asm	mov		eax, TOP		\
		__asm	mov		ebx, eax		\
		__asm	dec     ebx				\
		__asm	and     ebx, 7			\
		__asm	shl		eax, 4			\
		__asm	shl		ebx, 4			\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx					\
		__asm	fptan					\
		__asm	fnstsw	new_sw			\
		__asm	mov		cx, new_sw		\
		__asm	and		ch, 0x04 		\
		__asm	jnz		argument_too_large2				\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	jmp		end_ptan		\
		__asm	argument_too_large2:	\
		__asm	fstp	st(0)			\
		__asm	end_ptan:				\
		}												\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		if ((new_sw&0x0400)==0) FPU_PREP_PUSH();

// handles fxtract
#ifdef WEAK_EXCEPTIONS
#define FPUD_XTRACT						\
		__asm {							\
		__asm	mov		eax, TOP		\
		__asm	mov		ebx, eax		\
		__asm	dec     ebx				\
		__asm	and     ebx, 7			\
		__asm	shl		eax, 4			\
		__asm	shl		ebx, 4			\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fxtract					\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		}												\
		FPU_PREP_PUSH();
#else
#define FPUD_XTRACT						\
		Bit16u new_sw;					\
		__asm {							\
		__asm	mov		eax, TOP		\
		__asm	mov		ebx, eax		\
		__asm	dec     ebx				\
		__asm	and     ebx, 7			\
		__asm	shl		eax, 4			\
		__asm	shl		ebx, 4			\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fclex					\
		__asm	fxtract					\
		__asm	fnstsw	new_sw			\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		}												\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);			\
		FPU_PREP_PUSH();
#endif

// handles fadd,fmul,fsub,fsubr
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH1(op)						\
		Bit16u save_cw;						\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	mov		eax, op1			\
		__asm	shl		eax, 4				\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	mov		ebx, op2			\
		__asm	shl		ebx, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	op		st(1), st(0)		\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}
#else
#define FPUD_ARITH1(op)						\
		Bit16u new_sw,save_cw;				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	mov		eax, op1			\
		__asm	shl		eax, 4				\
		__asm	mov		ebx, op2			\
		__asm	shl		ebx, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	clx							\
		__asm	op		st(1), st(0)		\
		__asm	fnstsw	new_sw				\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fadd,fmul,fsub,fsubr
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH1_EA(op)					\
		Bit16u save_cw;						\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	mov		eax, op1			\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	shl		eax, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fxch	\
		__asm	op		st(1), st(0)		\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}
#else
#define FPUD_ARITH1_EA(op)					\
		Bit16u new_sw,save_cw;				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	mov		eax, op1			\
		__asm	shl		eax, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fxch	\
		__asm	clx							\
		__asm	op		st(1), st(0)		\
		__asm	fnstsw	new_sw				\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fsqrt,frndint
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH2(op)						\
		Bit16u save_cw;						\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	mov		eax, TOP			\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	shl		eax, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	op							\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}
#else
#define FPUD_ARITH2(op)						\
		Bit16u new_sw,save_cw;				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	mov		eax, TOP			\
		__asm	shl		eax, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx							\
		__asm	op							\
		__asm	fnstsw	new_sw				\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fdiv,fdivr
// (This is identical to FPUD_ARITH1 but without a WEAK_EXCEPTIONS variant)
#define FPUD_ARITH3(op)						\
		Bit16u new_sw,save_cw;				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	mov		eax, op1			\
		__asm	shl		eax, 4				\
		__asm	mov		ebx, op2			\
		__asm	shl		ebx, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fclex						\
		__asm	op		st(1), st(0)		\
		__asm	fnstsw	new_sw				\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}									\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);

// handles fdiv,fdivr
// (This is identical to FPUD_ARITH1_EA but without a WEAK_EXCEPTIONS variant)
#define FPUD_ARITH3_EA(op)					\
		Bit16u new_sw,save_cw;				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	mov		eax, op1			\
		__asm	fldcw	fpu.cw_mask_all		\
		__asm	shl		eax, 4				\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fxch	\
		__asm	fclex						\
		__asm	op		st(1), st(0)		\
		__asm	fnstsw	new_sw				\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	 \
		__asm	fldcw	save_cw				\
		}									\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);

// handles fprem,fprem1,fscale
#define FPUD_REMAINDER(op)			\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	mov		ebx, eax	\
		__asm	inc     ebx			\
		__asm	and     ebx, 7		\
		__asm	shl		ebx, 4		\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fclex				\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fstp	st(0)		\
		}							\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);

// handles fcom,fucom
#define FPUD_COMPARE(op)			\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		ebx, op2	\
		__asm	mov		eax, op1	\
		__asm	shl		ebx, 4		\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx					\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		}							\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

#define FPUD_COMPARE_EA(op)			\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		eax, op1	\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx					\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		}							\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fxam,ftst
#define FPUD_EXAMINE(op)			\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	clx					\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		__asm	fstp	st(0)		\
		}							\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fpatan,fyl2xp1
#ifdef WEAK_EXCEPTIONS
#define FPUD_WITH_POP(op)			\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	mov		ebx, eax	\
		__asm	inc     ebx			\
		__asm	and     ebx, 7		\
		__asm	shl		ebx, 4		\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	op					\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		}							\
		FPU_FPOP();
#else
#define FPUD_WITH_POP(op)			\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	mov		ebx, eax	\
		__asm	inc     ebx			\
		__asm	and     ebx, 7		\
		__asm	shl		ebx, 4		\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fclex				\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		}								\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);	\
		FPU_FPOP();
#endif

// handles fyl2x
#ifdef WEAK_EXCEPTIONS
#define FPUD_FYL2X(op)				\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	mov		ebx, eax	\
		__asm	inc     ebx			\
		__asm	and     ebx, 7		\
		__asm	shl		ebx, 4		\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	op					\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		}								\
		FPU_FPOP();
#else
#define FPUD_FYL2X(op)				\
		Bit16u new_sw;				\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	mov		ebx, eax	\
		__asm	inc     ebx			\
		__asm	and     ebx, 7		\
		__asm	shl		ebx, 4		\
		__asm	shl		eax, 4		\
		__asm	fld		TBYTE PTR fpu.p_regs[ebx].m1	\
		__asm	fld		TBYTE PTR fpu.p_regs[eax].m1	\
		__asm	fclex				\
		__asm	op					\
		__asm	fnstsw	new_sw		\
		__asm	fstp	TBYTE PTR fpu.p_regs[ebx].m1	\
		}								\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);	\
		FPU_FPOP();
#endif

// load math constants
#define FPUD_LOAD_CONST(op)		\
		FPU_PREP_PUSH();			\
		__asm {						\
		__asm	mov		eax, TOP	\
		__asm	shl		eax, 4		\
		__asm	clx					\
		__asm	op					\
		__asm	fstp	TBYTE PTR fpu.p_regs[eax].m1	\
		}							\

#else

// !defined _MSC_VER

#ifdef WEAK_EXCEPTIONS
#define clx
#else
#define clx "fclex"
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_LOAD(op,szI,szA)				\
		__asm__ volatile (					\
			#op #szA "	%1				\n"	\
			"fstpt		%0				"	\
			:	"=m" (fpu.p_regs[store_to])	\
			:	"m" (fpu.p_regs[8])			\
		);
#else
#define FPUD_LOAD(op,szI,szA)				\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fclex						\n"	\
			#op #szA "	%2				\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%1				"	\
			:	"=&am" (new_sw), "=m" (fpu.p_regs[store_to])		\
			:	"m" (fpu.p_regs[8])			\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_LOAD_EA(op,szI,szA)			\
		__asm__ volatile (					\
			#op #szA "	%0				\n"	\
			:								\
			:	"m" (fpu.p_regs[8])			\
		);
#else
#define FPUD_LOAD_EA(op,szI,szA)			\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fclex						\n"	\
			#op #szA "	%1				\n"	\
			"fnstsw		%0				\n"	\
			:	"=&am" (new_sw)				\
			:	"m" (fpu.p_regs[8])			\
			:								\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_STORE(op,szI,szA)				\
		Bit16u save_cw;						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			#op #szA "	%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "=m" (fpu.p_regs[8])	\
			:	"m" (fpu.p_regs[TOP]), "m" (fpu.cw_mask_all)		\
		);
#else
#define FPUD_STORE(op,szI,szA)				\
		Bit16u new_sw,save_cw;				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%4				\n"	\
			"fldt		%3				\n"	\
			"fclex 						\n"	\
			#op #szA "	%2				\n"	\
			"fnstsw		%0				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "=m" (fpu.p_regs[8])	\
			:	"m" (fpu.p_regs[TOP]), "m" (fpu.cw_mask_all)			\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fsin,fcos,f2xm1,fchs,fabs
#define FPUD_TRIG(op)						\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%1				"	\
			:	"=&am" (new_sw), "+m" (fpu.p_regs[TOP])		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fsincos
#define FPUD_SINCOS()					\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			"fsincos					\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"movw		%0, %%ax		\n"	\
			"sahf						\n"	\
			"jp			1f				\n"	\
			"fstpt		%1				\n"	\
			"1:							"	\
			:	"=m" (new_sw), "+m" (fpu.p_regs[TOP]),	\
				"=m" (fpu.p_regs[(TOP-1)&7])			\
			:								\
			:	"ax", "cc"					\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		if ((new_sw&0x0400)==0) FPU_PREP_PUSH();

// handles fptan
#define FPUD_PTAN()						\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			"fptan 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"movw		%0, %%ax		\n"	\
			"sahf						\n"	\
			"jp			1f				\n"	\
			"fstpt		%1				\n"	\
			"1:							"	\
			:	"=m" (new_sw), "+m" (fpu.p_regs[TOP]),	\
				"=m" (fpu.p_regs[(TOP-1)&7])			\
			:								\
			:	"ax", "cc"					\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		if ((new_sw&0x0400)==0) FPU_PREP_PUSH();

// handles fxtract
#ifdef WEAK_EXCEPTIONS
#define FPUD_XTRACT						\
		__asm__ volatile (					\
			"fldt		%0				\n"	\
			"fxtract					\n"	\
			"fstpt		%1				\n"	\
			"fstpt		%0				"	\
			:	"+m" (fpu.p_regs[TOP]), "=m" (fpu.p_regs[(TOP-1)&7])	\
		);									\
		FPU_PREP_PUSH();
#else
#define FPUD_XTRACT						\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			"fclex						\n"	\
			"fxtract					\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"fstpt		%1				"	\
			:	"=&am" (new_sw), "+m" (fpu.p_regs[TOP]),	\
				"=m" (fpu.p_regs[(TOP-1)&7])			\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		FPU_PREP_PUSH();
#endif

// handles fadd,fmul,fsub,fsubr
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH1(op)						\
		Bit16u save_cw;						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			"fldt		%1				\n"	\
			#op"						\n"	\
			"fstpt		%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "+m" (fpu.p_regs[op1])				\
			:	"m" (fpu.p_regs[op2]), "m" (fpu.cw_mask_all)		\
		);
#else
#define FPUD_ARITH1(op)						\
		Bit16u new_sw,save_cw;				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%4				\n"	\
			"fldt		%3				\n"	\
			"fldt		%2				\n"	\
			"fclex 						\n"	\
			#op"						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "+m" (fpu.p_regs[op1])	\
			:	"m" (fpu.p_regs[op2]), "m" (fpu.cw_mask_all)		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fadd,fmul,fsub,fsubr
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH1_EA(op)					\
		Bit16u save_cw;						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%2				\n"	\
			"fldt		%1				\n"	\
			#op"						\n"	\
			"fstpt		%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "+m" (fpu.p_regs[op1])		\
			:	"m" (fpu.cw_mask_all)		\
		);
#else
#define FPUD_ARITH1_EA(op)					\
		Bit16u new_sw,save_cw;				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			"fclex 						\n"	\
			#op"						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "+m" (fpu.p_regs[op1])	\
			:	"m" (fpu.cw_mask_all)		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fsqrt,frndint
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH2(op)						\
		Bit16u save_cw;						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%2				\n"	\
			"fldt		%1				\n"	\
			#op" 						\n"	\
			"fstpt		%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "+m" (fpu.p_regs[TOP])		\
			:	"m" (fpu.cw_mask_all)		\
		);
#else
#define FPUD_ARITH2(op)						\
		Bit16u new_sw,save_cw;				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			"fclex 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "+m" (fpu.p_regs[TOP])	\
			:	"m" (fpu.cw_mask_all)		\
		);										\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);
#endif

// handles fdiv,fdivr
// (This is identical to FPUD_ARITH1 but without a WEAK_EXCEPTIONS variant)
#define FPUD_ARITH3(op)						\
		Bit16u new_sw,save_cw;				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%4				\n"	\
			"fldt		%3				\n"	\
			"fldt		%2				\n"	\
			"fclex 						\n"	\
			#op"						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "+m" (fpu.p_regs[op1])	\
			:	"m" (fpu.p_regs[op2]), "m" (fpu.cw_mask_all)		\
		);									\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);

// handles fdiv,fdivr
// (This is identical to FPUD_ARITH1_EA but without a WEAK_EXCEPTIONS variant)
#define FPUD_ARITH3_EA(op)					\
		Bit16u new_sw,save_cw;				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			"fclex 						\n"	\
			#op"						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%2				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "+m" (fpu.p_regs[op1])	\
			:	"m" (fpu.cw_mask_all)		\
		);									\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);

// handles fprem,fprem1,fscale
#define FPUD_REMAINDER(op)					\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%2				\n"	\
			"fldt		%1				\n"	\
			"fclex						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%1				\n"	\
			"fstp		%%st(0)			"	\
			:	"=&am" (new_sw), "+m" (fpu.p_regs[TOP])	\
			:	"m" (fpu.p_regs[(TOP+1)&7])				\
		);									\
		fpu.sw=(new_sw&0xffbf)|(fpu.sw&0x80ff);

// handles fcom,fucom
#define FPUD_COMPARE(op)					\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%2				\n"	\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				"	\
			:	"=&am" (new_sw)				\
			:	"m" (fpu.p_regs[op1]), "m" (fpu.p_regs[op2])	\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fcom,fucom
#define FPUD_COMPARE_EA(op)					\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				"	\
			:	"=&am" (new_sw)				\
			:	"m" (fpu.p_regs[op1])		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fxam,ftst
#define FPUD_EXAMINE(op)					\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstp		%%st(0)			"	\
			:	"=&am" (new_sw)				\
			:	"m" (fpu.p_regs[TOP])		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);

// handles fpatan,fyl2xp1
#ifdef WEAK_EXCEPTIONS
#define FPUD_WITH_POP(op)					\
		__asm__ volatile (					\
			"fldt		%0				\n"	\
			"fldt		%1				\n"	\
			#op" 						\n"	\
			"fstpt		%0				\n"	\
			:	"+m" (fpu.p_regs[(TOP+1)&7])	\
			:	"m" (fpu.p_regs[TOP])		\
		);									\
		FPU_FPOP();
#else
#define FPUD_WITH_POP(op)					\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			"fldt		%2				\n"	\
			"fclex						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%1				\n"	\
			:	"=&am" (new_sw), "+m" (fpu.p_regs[(TOP+1)&7])		\
			:	"m" (fpu.p_regs[TOP])		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		FPU_FPOP();
#endif

// handles fyl2x
#ifdef WEAK_EXCEPTIONS
#define FPUD_FYL2X(op)						\
		__asm__ volatile (					\
			"fldt		%0				\n"	\
			"fldt		%1				\n"	\
			#op" 						\n"	\
			"fstpt		%0				\n"	\
			:	"+m" (fpu.p_regs[(TOP+1)&7])	\
			:	"m" (fpu.p_regs[TOP]) 		\
		);									\
		FPU_FPOP();
#else
#define FPUD_FYL2X(op)						\
		Bit16u new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			"fldt		%2				\n"	\
			"fclex						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%1				\n"	\
			:	"=&am" (new_sw), "+m" (fpu.p_regs[(TOP+1)&7])		\
			:	"m" (fpu.p_regs[TOP]) 		\
		);									\
		fpu.sw=(new_sw&exc_mask)|(fpu.sw&0x80ff);		\
		FPU_FPOP();
#endif

// load math constants
#define FPUD_LOAD_CONST(op)				\
		FPU_PREP_PUSH();					\
		__asm__ volatile (					\
			clx" 						\n"	\
			#op" 						\n"	\
			"fstpt		%0				\n"	\
			:	"=m" (fpu.p_regs[TOP])		\
		);

#endif

#ifdef WEAK_EXCEPTIONS
const Bit16u exc_mask=0x7f00;
#else
const Bit16u exc_mask=0xffbf;
#endif

static void FPU_FINIT(void) {
	FPU_SetCW(0x37F);
	fpu.sw=0;
	TOP=FPU_GET_TOP();
	fpu.tags[0]=TAG_Empty;
	fpu.tags[1]=TAG_Empty;
	fpu.tags[2]=TAG_Empty;
	fpu.tags[3]=TAG_Empty;
	fpu.tags[4]=TAG_Empty;
	fpu.tags[5]=TAG_Empty;
	fpu.tags[6]=TAG_Empty;
	fpu.tags[7]=TAG_Empty;
	fpu.tags[8]=TAG_Valid; // is only used by us
}

static void FPU_FCLEX(void){
	fpu.sw&=0x7f00;				//should clear exceptions
}

static void FPU_FNOP(void){
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
//	if (GCC_UNLIKELY(fpu.tags[TOP] != TAG_Empty)) E_Exit("FPU stack overflow");
	fpu.tags[TOP] = TAG_Valid;
}

static void FPU_FPOP(void){
//	if (GCC_UNLIKELY(fpu.tags[TOP] == TAG_Empty)) E_Exit("FPU stack underflow");
	fpu.tags[TOP] = TAG_Empty;
	TOP = ((TOP+1)&7);
}

static void FPU_FLD_F32(PhysPt addr,Bitu store_to) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	FPUD_LOAD(fld,DWORD,s)
}

static void FPU_FLD_F32_EA(PhysPt addr) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	FPUD_LOAD_EA(fld,DWORD,s)
}

static void FPU_FLD_F64(PhysPt addr,Bitu store_to) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	fpu.p_regs[8].m2 = mem_readd(addr+4);
	FPUD_LOAD(fld,QWORD,l)
}

static void FPU_FLD_F64_EA(PhysPt addr) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	fpu.p_regs[8].m2 = mem_readd(addr+4);
	FPUD_LOAD_EA(fld,QWORD,l)
}

static void FPU_FLD_F80(PhysPt addr) {
	fpu.p_regs[TOP].m1 = mem_readd(addr);
	fpu.p_regs[TOP].m2 = mem_readd(addr+4);
	fpu.p_regs[TOP].m3 = mem_readw(addr+8);
	FPU_SET_C1(0);
}

static void FPU_FLD_I16(PhysPt addr,Bitu store_to) {
	fpu.p_regs[8].m1 = (Bit32u)mem_readw(addr);
	FPUD_LOAD(fild,WORD,s)
}

static void FPU_FLD_I16_EA(PhysPt addr) {
	fpu.p_regs[8].m1 = (Bit32u)mem_readw(addr);
	FPUD_LOAD_EA(fild,WORD,s)
}

static void FPU_FLD_I32(PhysPt addr,Bitu store_to) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	FPUD_LOAD(fild,DWORD,l)
}

static void FPU_FLD_I32_EA(PhysPt addr) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	FPUD_LOAD_EA(fild,DWORD,l)
}

static void FPU_FLD_I64(PhysPt addr,Bitu store_to) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	fpu.p_regs[8].m2 = mem_readd(addr+4);
	FPUD_LOAD(fild,QWORD,q)
}

static void FPU_FBLD(PhysPt addr,Bitu store_to) {
	fpu.p_regs[8].m1 = mem_readd(addr);
	fpu.p_regs[8].m2 = mem_readd(addr+4);
	fpu.p_regs[8].m3 = mem_readw(addr+8);
	FPUD_LOAD(fbld,TBYTE,)
}

static void FPU_FST_F32(PhysPt addr) {
	FPUD_STORE(fstp,DWORD,s)
	mem_writed(addr,fpu.p_regs[8].m1);
}

static void FPU_FST_F64(PhysPt addr) {
	FPUD_STORE(fstp,QWORD,l)
	mem_writed(addr,fpu.p_regs[8].m1);
	mem_writed(addr+4,fpu.p_regs[8].m2);
}

static void FPU_FST_F80(PhysPt addr) {
	mem_writed(addr,fpu.p_regs[TOP].m1);
	mem_writed(addr+4,fpu.p_regs[TOP].m2);
	mem_writew(addr+8,fpu.p_regs[TOP].m3);
	FPU_SET_C1(0);
}

static void FPU_FST_I16(PhysPt addr) {
	FPUD_STORE(fistp,WORD,s)
	mem_writew(addr,(Bit16u)fpu.p_regs[8].m1);
}

static void FPU_FST_I32(PhysPt addr) {
	FPUD_STORE(fistp,DWORD,l)
	mem_writed(addr,fpu.p_regs[8].m1);
}

static void FPU_FST_I64(PhysPt addr) {
	FPUD_STORE(fistp,QWORD,q)
	mem_writed(addr,fpu.p_regs[8].m1);
	mem_writed(addr+4,fpu.p_regs[8].m2);
}

static void FPU_FBST(PhysPt addr) {
	FPUD_STORE(fbstp,TBYTE,)
	mem_writed(addr,fpu.p_regs[8].m1);
	mem_writed(addr+4,fpu.p_regs[8].m2);
	mem_writew(addr+8,fpu.p_regs[8].m3);
}


static void FPU_FSIN(void){
	FPUD_TRIG(fsin)
}

static void FPU_FSINCOS(void){
	FPUD_SINCOS()
}

static void FPU_FCOS(void){
	FPUD_TRIG(fcos)
}

static void FPU_FSQRT(void){
	FPUD_ARITH2(fsqrt)
}

static void FPU_FPATAN(void){
	FPUD_WITH_POP(fpatan)
}

static void FPU_FPTAN(void){
	FPUD_PTAN()
}


static void FPU_FADD(Bitu op1, Bitu op2){
	FPUD_ARITH1(faddp)
}

static void FPU_FADD_EA(Bitu op1){
	FPUD_ARITH1_EA(faddp)
}

static void FPU_FDIV(Bitu op1, Bitu op2){
	FPUD_ARITH3(fdivp)
}

static void FPU_FDIV_EA(Bitu op1){
	FPUD_ARITH3_EA(fdivp)
}

static void FPU_FDIVR(Bitu op1, Bitu op2){
	FPUD_ARITH3(fdivrp)
}

static void FPU_FDIVR_EA(Bitu op1){
	FPUD_ARITH3_EA(fdivrp)
}

static void FPU_FMUL(Bitu op1, Bitu op2){
	FPUD_ARITH1(fmulp)
}

static void FPU_FMUL_EA(Bitu op1){
	FPUD_ARITH1_EA(fmulp)
}

static void FPU_FSUB(Bitu op1, Bitu op2){
	FPUD_ARITH1(fsubp)
}

static void FPU_FSUB_EA(Bitu op1){
	FPUD_ARITH1_EA(fsubp)
}

static void FPU_FSUBR(Bitu op1, Bitu op2){
	FPUD_ARITH1(fsubrp)
}

static void FPU_FSUBR_EA(Bitu op1){
	FPUD_ARITH1_EA(fsubrp)
}

static void FPU_FXCH(Bitu stv, Bitu other){
	FPU_Tag tag = fpu.tags[other];
	fpu.tags[other] = fpu.tags[stv];
	fpu.tags[stv] = tag;

	Bit32u m1s = fpu.p_regs[other].m1;
	Bit32u m2s = fpu.p_regs[other].m2;
	Bit16u m3s = fpu.p_regs[other].m3;
	fpu.p_regs[other].m1 = fpu.p_regs[stv].m1;
	fpu.p_regs[other].m2 = fpu.p_regs[stv].m2;
	fpu.p_regs[other].m3 = fpu.p_regs[stv].m3;
	fpu.p_regs[stv].m1 = m1s;
	fpu.p_regs[stv].m2 = m2s;
	fpu.p_regs[stv].m3 = m3s;

	FPU_SET_C1(0);
}

static void FPU_FST(Bitu stv, Bitu other){
	fpu.tags[other] = fpu.tags[stv];

	fpu.p_regs[other].m1 = fpu.p_regs[stv].m1;
	fpu.p_regs[other].m2 = fpu.p_regs[stv].m2;
	fpu.p_regs[other].m3 = fpu.p_regs[stv].m3;

	FPU_SET_C1(0);
}

static inline void FPU_FCMOV(Bitu st, Bitu other){
	fpu.p_regs[st] = fpu.p_regs[other];
	fpu.tags[st] = fpu.tags[other];
}

static void FPU_FCOM(Bitu op1, Bitu op2){
	FPUD_COMPARE(fcompp)
}

static void FPU_FUCOMI(Bitu st, Bitu other){
    LOG_MSG("FPU WARNING: FPU_FUCOMI called, needs testing");

    FPU_FCOM(st,other);

    Bitu FillFlags(void);//Why is this needed for VS2015?

	FillFlags();
	SETFLAGBIT(OF,false);

    if (fpu.sw & (1u << 14u)/*C3*/) {//if(fpu.regs[st].d == fpu.regs[other].d){
		SETFLAGBIT(ZF,true);SETFLAGBIT(PF,false);SETFLAGBIT(CF,false);return;
    }
    else if (fpu.sw & (1u << 8u)/*C0*/) {//if(fpu.regs[st].d < fpu.regs[other].d){
		SETFLAGBIT(ZF,false);SETFLAGBIT(PF,false);SETFLAGBIT(CF,true);return;
    }
	// st > other
	SETFLAGBIT(ZF,false);SETFLAGBIT(PF,false);SETFLAGBIT(CF,false);return;
}

static inline void FPU_FCOMI(Bitu st, Bitu other){
	FPU_FUCOMI(st,other);

	if(((fpu.tags[st] != TAG_Valid) && (fpu.tags[st] != TAG_Zero)) || 
		((fpu.tags[other] != TAG_Valid) && (fpu.tags[other] != TAG_Zero))){
		SETFLAGBIT(ZF,true);SETFLAGBIT(PF,true);SETFLAGBIT(CF,true);return;
	}

}

static void FPU_FCOM_EA(Bitu op1){
	FPUD_COMPARE_EA(fcompp)
}

static void FPU_FUCOM(Bitu op1, Bitu op2){
	FPUD_COMPARE(fucompp)
}

static void FPU_FRNDINT(void){
	FPUD_ARITH2(frndint)
}

static void FPU_FPREM(void){
	FPUD_REMAINDER(fprem)
}

static void FPU_FPREM1(void){
	FPUD_REMAINDER(fprem1)
}

static void FPU_FXAM(void){
	FPUD_EXAMINE(fxam)
	// handle empty registers (C1 set to sign in any way!)
	if(fpu.tags[TOP] == TAG_Empty) {
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(1);
		return;
	}
}

static void FPU_F2XM1(void){
	FPUD_TRIG(f2xm1)
}

static void FPU_FYL2X(void){
	FPUD_FYL2X(fyl2x)
}

static void FPU_FYL2XP1(void){
	FPUD_WITH_POP(fyl2xp1)
}

static void FPU_FSCALE(void){
	FPUD_REMAINDER(fscale)
}


static void FPU_FSTENV(PhysPt addr){
	FPU_SET_TOP(TOP);
	if(!cpu.code.big) {
		mem_writew(addr+0,static_cast<Bit16u>(fpu.cw));
		mem_writew(addr+2,static_cast<Bit16u>(fpu.sw));
		mem_writew(addr+4,static_cast<Bit16u>(FPU_GetTag()));
	} else { 
		mem_writed(addr+0,static_cast<Bit32u>(fpu.cw));
		mem_writed(addr+4,static_cast<Bit32u>(fpu.sw));
		mem_writed(addr+8,static_cast<Bit32u>(FPU_GetTag()));
	}
}

static void FPU_FLDENV(PhysPt addr){
	Bit16u tag;
	Bitu cw;
	if(!cpu.code.big) {
		cw     = mem_readw(addr+0);
		fpu.sw = mem_readw(addr+2);
		tag    = mem_readw(addr+4);
	} else { 
		cw     = mem_readd(addr+0);
		fpu.sw = (Bit16u)mem_readd(addr+4);
		Bit32u tagbig = mem_readd(addr+8);
		tag    = static_cast<Bit16u>(tagbig);
	}
	FPU_SetTag(tag);
	FPU_SetCW(cw);
	TOP=FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr){
	FPU_FSTENV(addr);
	Bitu start=(cpu.code.big?28:14);
	for(Bitu i=0;i<8;i++){
		mem_writed(addr+start,fpu.p_regs[STV(i)].m1);
		mem_writed(addr+start+4,fpu.p_regs[STV(i)].m2);
		mem_writew(addr+start+8,fpu.p_regs[STV(i)].m3);
		start+=10;
	}
	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr){
	FPU_FLDENV(addr);
	Bitu start=(cpu.code.big?28:14);
	for(Bitu i=0;i<8;i++){
		fpu.p_regs[STV(i)].m1 = mem_readd(addr+start);
		fpu.p_regs[STV(i)].m2 = mem_readd(addr+start+4);
		fpu.p_regs[STV(i)].m3 = mem_readw(addr+start+8);
		start+=10;
	}
}


static void FPU_FXTRACT(void) {
	FPUD_XTRACT
}

static void FPU_FCHS(void){
	FPUD_TRIG(fchs)
}

static void FPU_FABS(void){
	FPUD_TRIG(fabs)
}

static void FPU_FTST(void){
	FPUD_EXAMINE(ftst)
}

static void FPU_FLD1(void){
	FPUD_LOAD_CONST(fld1)
}

static void FPU_FLDL2T(void){
	FPUD_LOAD_CONST(fldl2t)
}

static void FPU_FLDL2E(void){
	FPUD_LOAD_CONST(fldl2e)
}

static void FPU_FLDPI(void){
	FPUD_LOAD_CONST(fldpi)
}

static void FPU_FLDLG2(void){
	FPUD_LOAD_CONST(fldlg2)
}

static void FPU_FLDLN2(void){
	FPUD_LOAD_CONST(fldln2)
}

static void FPU_FLDZ(void){
	FPUD_LOAD_CONST(fldz)
	fpu.tags[TOP]=TAG_Zero;
}
