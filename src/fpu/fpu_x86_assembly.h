#pragma once

// #define WEAK_EXCEPTIONS

#ifdef WEAK_EXCEPTIONS
static constexpr uint16_t sw_mask = FPUStatusWord::conditionMask;
#else
static constexpr uint16_t sw_mask = FPUStatusWord::conditionAndExceptionMask;
#endif

#if defined (_MSC_VER)

#ifdef WEAK_EXCEPTIONS
#define clx
#else
#define clx fclex
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_STORE(op,szI,szA)                          \
        uint16_t save_cw,cw_masked=fpu.cw.allMasked();  \
        uint32_t top = TOP;                             \
        __asm {                                         \
        __asm    fnstcw   save_cw                       \
        __asm    mov      eax, top                      \
        __asm    fldcw    cw_masked                     \
        __asm    shl      eax, 4                        \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1  \
        __asm    op       szI PTR fpu.p_regs[128].m1    \
        __asm    fldcw    save_cw                       \
        }
#else
#define FPUD_STORE(op,szI,szA)                                         \
        uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();          \
        uint32_t top = TOP;                                            \
        __asm {                                                        \
        __asm    fnstcw   save_cw                                      \
        __asm    fldcw    cw_masked                                    \
        __asm    mov      eax, top                                     \
        __asm    shl      eax, 4                                       \
        __asm    mov      ebx, 8                                       \
        __asm    shl      ebx, 4                                       \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1                 \
        __asm    clx                                                   \
        __asm    op       szI PTR fpu.p_regs[ebx].m1                   \
        __asm    fnstsw   new_sw                                       \
        __asm    fldcw    save_cw                                      \
        }                                                              \
        fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);
#endif

// handles fsin,fcos,f2xm1,fchs,fabs
#define FPUD_TRIG(op)                                      \
        uint16_t new_sw;                                   \
        uint32_t top = TOP;                                \
        __asm {                                            \
        __asm    mov     eax, top                          \
        __asm    shl     eax, 4                            \
        __asm    fld     TBYTE PTR fpu.p_regs[eax].m1      \
        __asm    clx                                       \
        __asm    op                                        \
        __asm    fnstsw  new_sw                            \
        __asm    fstp    TBYTE PTR fpu.p_regs[eax].m1      \
        }                                                  \
        fpu.sw = (new_sw & sw_mask) |                      \
                 (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fsincos
#define FPUD_SINCOS()                                                           \
        uint16_t new_sw;                                                        \
        uint32_t top = TOP;                                                     \
        __asm {                                                                 \
        __asm    mov      eax, top                                              \
        __asm    mov      ebx, eax                                              \
        __asm    dec      ebx                                                   \
        __asm    and      ebx, 7                                                \
        __asm    shl      eax, 4                                                \
        __asm    shl      ebx, 4                                                \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1                          \
        __asm    clx                                                            \
        __asm    fsincos                                                        \
        __asm    fnstsw   new_sw                                                \
        __asm    mov      cx, new_sw                                            \
        __asm    and      ch, 0x04                                              \
        __asm    jnz      argument_too_large1                                   \
        __asm    fstp     TBYTE PTR fpu.p_regs[ebx].m1                          \
        __asm    fstp     TBYTE PTR fpu.p_regs[eax].m1                          \
        __asm    jmp      end_sincos                                            \
        __asm    argument_too_large1:                                           \
        __asm    fstp     st(0)                                                 \
        __asm    end_sincos:                                                    \
        }                                                                       \
        fpu.sw = (new_sw & sw_mask) | (fpu.sw & ~FPUStatusWord::conditionMask); \
        if (!fpu.sw.C2) FPU_PREP_PUSH();

// handles fptan
#define FPUD_PTAN()                                         \
        uint16_t new_sw;                                    \
        uint32_t top = TOP;                                 \
        __asm {                                             \
        __asm    mov      eax, top                          \
        __asm    mov      ebx, eax                          \
        __asm    dec      ebx                               \
        __asm    and      ebx, 7                            \
        __asm    shl      eax, 4                            \
        __asm    shl      ebx, 4                            \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1      \
        __asm    clx                                        \
        __asm    fptan                                      \
        __asm    fnstsw   new_sw                            \
        __asm    mov      cx, new_sw                        \
        __asm    and      ch, 0x04                          \
        __asm    jnz      argument_too_large2               \
        __asm    fstp     TBYTE PTR fpu.p_regs[ebx].m1      \
        __asm    fstp     TBYTE PTR fpu.p_regs[eax].m1      \
        __asm    jmp      end_ptan                          \
        __asm    argument_too_large2:                       \
        __asm    fstp     st(0)                             \
        __asm    end_ptan:                                  \
        }                                                   \
        fpu.sw = (new_sw & sw_mask) |                       \
                 (fpu.sw & ~FPUStatusWord::conditionMask);  \
        if (!fpu.sw.C2) FPU_PREP_PUSH();

// handles fxtract
#ifdef WEAK_EXCEPTIONS
#define FPUD_XTRACT                                      \
        uint32_t top = TOP;                              \
        __asm {                                          \
        __asm    mov      eax, top                       \
        __asm    mov      ebx, eax                       \
        __asm    dec      ebx                            \
        __asm    and      ebx, 7                         \
        __asm    shl      eax, 4                         \
        __asm    shl      ebx, 4                         \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1   \
        __asm    fxtract                                 \
        __asm    fstp     TBYTE PTR fpu.p_regs[ebx].m1   \
        __asm    fstp     TBYTE PTR fpu.p_regs[eax].m1   \
        }                                                \
        FPU_PREP_PUSH();
#else
#define FPUD_XTRACT                                                    \
        uint16_t new_sw;                                               \
        uint32_t top = TOP;                                            \
        __asm {                                                        \
        __asm    mov      eax, top                                     \
        __asm    mov      ebx, eax                                     \
        __asm    dec      ebx                                          \
        __asm    and      ebx, 7                                       \
        __asm    shl      eax, 4                                       \
        __asm    shl      ebx, 4                                       \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1                 \
        __asm    fclex                                                 \
        __asm    fxtract                                               \
        __asm    fnstsw   new_sw                                       \
        __asm    fstp     TBYTE PTR fpu.p_regs[ebx].m1                 \
        __asm    fstp     TBYTE PTR fpu.p_regs[eax].m1                 \
        }                                                              \
        fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);             \
        FPU_PREP_PUSH();
#endif

// handles fadd,fmul,fsub,fsubr
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH1(op)						\
		uint16_t save_cw,cw_masked=fpu.cw.allMasked();						\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	mov		eax, op1			\
		__asm	shl		eax, 4				\
		__asm	fldcw	cw_masked           \
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
		uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	cw_masked			\
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
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);
#endif

// handles fsqrt,frndint
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH2(op)                                   \
        uint16_t save_cw,cw_masked=fpu.cw.allMasked();    \
        uint32_t top = TOP;                               \
        __asm {                                           \
        __asm    fnstcw  save_cw                          \
        __asm    mov     eax, top                         \
        __asm    fldcw   cw_masked                        \
        __asm    shl     eax, 4                           \
        __asm    fld     TBYTE PTR fpu.p_regs[eax].m1     \
        __asm    op                                       \
        __asm    fstp    TBYTE PTR fpu.p_regs[eax].m1     \
        __asm    fldcw   save_cw                          \
        }
#else
#define FPUD_ARITH2(op)                                                \
        uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();          \
        uint32_t top = TOP;                                            \
        __asm {                                                        \
        __asm    fnstcw   save_cw                                      \
        __asm    fldcw    cw_masked                                    \
        __asm    mov      eax, top                                     \
        __asm    shl      eax, 4                                       \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1                 \
        __asm    clx                                                   \
        __asm    op                                                    \
        __asm    fnstsw   new_sw                                       \
        __asm    fstp     TBYTE PTR fpu.p_regs[eax].m1                 \
        __asm    fldcw    save_cw                                      \
        }                                                              \
        fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);
#endif

// handles fdiv,fdivr
// (This is identical to FPUD_ARITH1 but without a WEAK_EXCEPTIONS variant)
#define FPUD_ARITH3(op)						\
		uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();				\
		__asm {								\
		__asm	fnstcw	save_cw				\
		__asm	fldcw	cw_masked			\
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
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fprem,fprem1,fscale
#define FPUD_REMAINDER(op)                                         \
    uint16_t new_sw, save_cw, cw_masked = fpu.cw.allMasked();      \
    uint32_t top = TOP;                                            \
    __asm {                                                        \
    __asm   fnstcw  save_cw                                        \
    __asm   fldcw   cw_masked                                      \
    __asm   mov     eax, top                                       \
    __asm   mov     ebx, eax                                       \
    __asm   inc     ebx                                            \
    __asm   and     ebx, 7                                         \
    __asm   shl     ebx, 4                                         \
    __asm   shl     eax, 4                                         \
    __asm   fld     TBYTE PTR fpu.p_regs[ebx].m1                   \
    __asm   fld     TBYTE PTR fpu.p_regs[eax].m1                   \
    __asm   fclex                                                  \
    __asm   op                                                     \
    __asm   fnstsw  new_sw                                         \
    __asm   fstp    TBYTE PTR fpu.p_regs[eax].m1                   \
    __asm   fstp    st(0)                                          \
    __asm   fldcw   save_cw                                        \
    }                                                              \
    fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fcom,fucom
#define FPUD_COMPARE(op)			\
		uint16_t new_sw;				\
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
		fpu.sw = (new_sw & sw_mask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fxam,ftst
#define FPUD_EXAMINE(op)                                   \
        uint16_t new_sw;                                   \
        uint32_t top = TOP;                                \
        __asm {                                            \
        __asm    mov      eax, top                         \
        __asm    shl      eax, 4                           \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1     \
        __asm    clx                                       \
        __asm    op                                        \
        __asm    fnstsw   new_sw                           \
        __asm    fstp     st(0)                            \
        }                                                  \
        fpu.sw = (new_sw & sw_mask) |                      \
                 (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fpatan,fyl2xp1
#ifdef WEAK_EXCEPTIONS
#define FPUD_WITH_POP(op)                               \
        uint32_t top = TOP;                             \
        __asm {                                         \
        __asm    mov    eax, top                        \
        __asm    mov    ebx, eax                        \
        __asm    inc    ebx                             \
        __asm    and    ebx, 7                          \
        __asm    shl    ebx, 4                          \
        __asm    shl    eax, 4                          \
        __asm    fld    TBYTE PTR fpu.p_regs[ebx].m1    \
        __asm    fld    TBYTE PTR fpu.p_regs[eax].m1    \
        __asm    op                                     \
        __asm    fstp   TBYTE PTR fpu.p_regs[ebx].m1    \
        }                            \
        FPU_FPOP();
#else
#define FPUD_WITH_POP(op)                                              \
        uint16_t new_sw;                                               \
        uint32_t top = TOP;                                            \
        __asm {                                                        \
        __asm    mov      eax, top                                     \
        __asm    mov      ebx, eax                                     \
        __asm    inc      ebx                                          \
        __asm    and      ebx, 7                                       \
        __asm    shl      ebx, 4                                       \
        __asm    shl      eax, 4                                       \
        __asm    fld      TBYTE PTR fpu.p_regs[ebx].m1                 \
        __asm    fld      TBYTE PTR fpu.p_regs[eax].m1                 \
        __asm    fclex                                                 \
        __asm    op                                                    \
        __asm    fnstsw   new_sw                                       \
        __asm    fstp     TBYTE PTR fpu.p_regs[ebx].m1                 \
        }                                                              \
        fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);             \
        FPU_FPOP();
#endif

// handles fyl2x
#ifdef WEAK_EXCEPTIONS
#define FPUD_FYL2X(op)                                  \
        uint32_t top = TOP;                             \
        __asm {                                         \
        __asm    mov    eax, top                        \
        __asm    mov    ebx, eax                        \
        __asm    inc    ebx                             \
        __asm    and    ebx, 7                          \
        __asm    shl    ebx, 4                          \
        __asm    shl    eax, 4                          \
        __asm    fld    TBYTE PTR fpu.p_regs[ebx].m1    \
        __asm    fld    TBYTE PTR fpu.p_regs[eax].m1    \
        __asm    op                                     \
        __asm    fstp   TBYTE PTR fpu.p_regs[ebx].m1    \
        }                                               \
        FPU_FPOP();
#else
#define FPUD_FYL2X(op)                                                 \
        uint16_t new_sw;                                               \
        uint32_t top = TOP;                                            \
        __asm {                                                        \
        __asm    mov     eax, top                                      \
        __asm    mov     ebx, eax                                      \
        __asm    inc     ebx                                           \
        __asm    and     ebx, 7                                        \
        __asm    shl     ebx, 4                                        \
        __asm    shl     eax, 4                                        \
        __asm    fld     TBYTE PTR fpu.p_regs[ebx].m1                  \
        __asm    fld     TBYTE PTR fpu.p_regs[eax].m1                  \
        __asm    fclex                                                 \
        __asm    op                                                    \
        __asm    fnstsw  new_sw                                        \
        __asm    fstp    TBYTE PTR fpu.p_regs[ebx].m1                  \
        }                                                              \
        fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);             \
        FPU_FPOP();
#endif

#else

// !defined _MSC_VER

#ifdef WEAK_EXCEPTIONS
#define clx
#else
#define clx "fclex"
#endif

#ifdef WEAK_EXCEPTIONS
#define FPUD_STORE(op,szI,szA)				\
		uint16_t save_cw,cw_masked=fpu.cw.allMasked();						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			#op #szA "	%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "=m" (fpu.p_regs[8])	\
			:	"m" (fpu.p_regs[TOP]), "m" (cw_masked)		\
		);
#else
#define FPUD_STORE(op,szI,szA)				\
		uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();				\
		__asm__ volatile (					\
			"fnstcw		%1				\n"	\
			"fldcw		%4				\n"	\
			"fldt		%3				\n"	\
			"fclex 						\n"	\
			#op #szA "	%2				\n"	\
			"fnstsw		%0				\n"	\
			"fldcw		%1				"	\
			:	"=&am" (new_sw), "=m" (save_cw), "=m" (fpu.p_regs[8])	\
			:	"m" (fpu.p_regs[TOP]), "m" (cw_masked)			\
		);									\
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);
#endif

// handles fsin,fcos,f2xm1,fchs,fabs
#define FPUD_TRIG(op)						\
		uint16_t new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstpt		%1				"	\
			:	"=&am" (new_sw), "+m" (fpu.p_regs[TOP])		\
		);									\
		fpu.sw = (new_sw & sw_mask) |       \
		         (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fsincos
#define FPUD_SINCOS()					\
		uint16_t new_sw;						\
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
		fpu.sw = (new_sw & sw_mask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask); \
		if (!fpu.sw.C2) FPU_PREP_PUSH();

// handles fptan
#define FPUD_PTAN()						\
		uint16_t new_sw;						\
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
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask); \
		if (!fpu.sw.C2) FPU_PREP_PUSH();

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
		uint16_t new_sw;						\
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
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);             \
		FPU_PREP_PUSH();
#endif

// handles fadd,fmul,fsub,fsubr
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH1(op)						\
		uint16_t save_cw,cw_masked=fpu.cw.allMasked();						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%3				\n"	\
			"fldt		%2				\n"	\
			"fldt		%1				\n"	\
			#op"						\n"	\
			"fstpt		%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "+m" (fpu.p_regs[op1])				\
			:	"m" (fpu.p_regs[op2]), "m" (cw_masked)		\
		);
#else
#define FPUD_ARITH1(op)						\
		uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();				\
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
			:	"m" (fpu.p_regs[op2]), "m" (cw_masked)		\
		);									\
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);
#endif

// handles fsqrt,frndint
#ifdef WEAK_EXCEPTIONS
#define FPUD_ARITH2(op)						\
		uint16_t save_cw,cw_masked=fpu.cw.allMasked();						\
		__asm__ volatile (					\
			"fnstcw		%0				\n"	\
			"fldcw		%2				\n"	\
			"fldt		%1				\n"	\
			#op" 						\n"	\
			"fstpt		%1				\n"	\
			"fldcw		%0				"	\
			:	"=m" (save_cw), "+m" (fpu.p_regs[TOP])		\
			:	"m" (cw_masked)		\
		);
#else
#define FPUD_ARITH2(op)						\
		uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();				\
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
			:	"m" (cw_masked)		\
		);										\
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);
#endif

// handles fdiv,fdivr
// (This is identical to FPUD_ARITH1 but without a WEAK_EXCEPTIONS variant)
#define FPUD_ARITH3(op)						\
		uint16_t new_sw,save_cw,cw_masked=fpu.cw.allMasked();				\
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
			:	"m" (fpu.p_regs[op2]), "m" (cw_masked)		\
		);									\
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fprem,fprem1,fscale
#define FPUD_REMAINDER(op)                                         \
    uint16_t new_sw, save_cw, cw_masked = fpu.cw.allMasked();      \
    __asm__ volatile (                                             \
        "fnstcw     %1              \n"                            \
        "fldcw      %4              \n"                            \
        "fldt       %3              \n"                            \
        "fldt       %2              \n"                            \
        "fclex                      \n"                            \
        #op"                        \n"                            \
        "fnstsw     %0              \n"                            \
        "fstpt      %2              \n"                            \
        "fstp       %%st(0)         \n"                            \
        "fldcw      %1                "                            \
        : "=&am" (new_sw), "=m" (save_cw), "+m" (fpu.p_regs[TOP])  \
        : "m" (fpu.p_regs[(TOP+1)&7]), "m" (cw_masked)             \
    );                                                             \
    fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
                 (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fcom,fucom
#define FPUD_COMPARE(op)					\
		uint16_t new_sw;						\
		__asm__ volatile (					\
			"fldt		%2				\n"	\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				"	\
			:	"=&am" (new_sw)				\
			:	"m" (fpu.p_regs[op1]), "m" (fpu.p_regs[op2])	\
		);									\
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);

// handles fxam,ftst
#define FPUD_EXAMINE(op)					\
		uint16_t new_sw;						\
		__asm__ volatile (					\
			"fldt		%1				\n"	\
			clx" 						\n"	\
			#op" 						\n"	\
			"fnstsw		%0				\n"	\
			"fstp		%%st(0)			"	\
			:	"=&am" (new_sw)				\
			:	"m" (fpu.p_regs[TOP])		\
		);									\
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);

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
		uint16_t new_sw;						\
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
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);             \
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
		uint16_t new_sw;						\
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
		fpu.sw = (new_sw & FPUStatusWord::conditionAndExceptionMask) | \
		         (fpu.sw & ~FPUStatusWord::conditionMask);             \
		FPU_FPOP();
#endif

#endif
