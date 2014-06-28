/* (C) 2014 Castus all rights reserved.
 * Written by Jonathan Campbell
 *
 * Intended language: C (also compatible with C++) */

#ifndef __UTIL_POINTER_H
#define __UTIL_POINTER_H

#include <stdint.h>			/* need standard C library integer types */
#include <stddef.h>
#include <alloca.h>

#ifdef __cplusplus
extern "C" {
#endif

#define min_uintptr_t			((uintptr_t)0)
#define max_uintptr_t			( ~((uintptr_t)0) )

#define min_size_t			((size_t)0)
#define max_size_t			( ~((size_t)0) )

#define ptr2int(x)			((uintptr_t)((void*)(x)))
#define int2ptr(x)			((void*)(x))

#define ptr_misaligned(x,a)		(ptr2int(x) & (((uintptr_t)(a)) - (uintptr_t)1))
#define ptr_aligned(x,a)		(ptr_misaligned(x,a) == (uintptr_t)0)

#define ptr_align_down(x,a)		(ptr2int(x) & (~(((uintptr_t)(a)) - (uintptr_t)1)))
#define ptr_align_up(x,a)		((ptr2int(x) + ((uintptr_t)(a) - (uintptr_t)1)) & (~(((uintptr_t)(a)) - (uintptr_t)1)))
#define ptr_align_nearest(x,a)		((ptr2int(x) + ((uintptr_t)(a) >> (uintptr_t)1)) & (~(((uintptr_t)(a)) - (uintptr_t)1)))
#define ptr_align_next(x,a)		((ptr2int(x) + (uintptr_t)(a)) & (~(((uintptr_t)(a)) - (uintptr_t)1)))

#define alloca_aligned(s,a)		( (void*)ptr_align_up((void*)alloca((size_t)(s) + (size_t)(a) - (size_t)1),a) )

#define alignment_word			(2)
#define alignment_dword			(4)
#define alignment_qword			(8)
#define alignment_mmx			(8)
#define alignment_sse			(16)
#define alignment_avx			(32)

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_POINTER_H */

