/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */


/**

   This header contains an implementation of atomic operations.

 */

#ifndef _FLUID_ATOMIC_H
#define _FLUID_ATOMIC_H


#if defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)

#ifdef __ATOMIC_SEQ_CST
// Use GCC's new atomic operations.

#define fluid_atomic_order __ATOMIC_SEQ_CST

#define fluid_atomic_int_exchange_and_add(_pi, _val) (__atomic_add_fetch(_pi, \
                                                        _val, fluid_atomic_order))

#define fluid_atomic_int_get(_pi) __atomic_load_n(_pi, fluid_atomic_order)
#define fluid_atomic_int_set(_pi, _val) __atomic_store_n(_pi, _val, \
                                          fluid_atomic_order)
#define fluid_atomic_int_dec_and_test(_pi) (__atomic_sub_fetch(_pi, 1, \
                                              fluid_atomic_order) == 0)

static FLUID_INLINE int
fluid_atomic_int_compare_and_exchange(volatile int* _pi, int _old, int _new)
{
  return __atomic_compare_exchange_n(_pi, &_old, _new, 0, fluid_atomic_order,
                                     fluid_atomic_order);
}


#define fluid_atomic_pointer_get(_pp) __atomic_load_n(_pp, fluid_atomic_order)
#define fluid_atomic_pointer_set(_pp, _val) __atomic_store_n(_pp, _val, \
                                              fluid_atomic_order)

static FLUID_INLINE int
fluid_atomic_pointer_compare_and_exchange(volatile void* _pi, volatile void* _old,
                                          void* _new)
{
  return __atomic_compare_exchange_n((volatile void**)_pi, &_old, _new, 0,
                                     fluid_atomic_order, fluid_atomic_order);
}

#else
// Use older __sync atomics.

#define fluid_atomic_int_exchange_and_add(_pi, _val) __sync_add_and_fetch(_pi, \
                                                      _val)

static FLUID_INLINE int
fluid_atomic_int_get(volatile int* _pi)
{
  __sync_synchronize();
  return (int)*_pi;
}

static FLUID_INLINE void
fluid_atomic_int_set(volatile int* _pi, int _val) {
  *_pi = _val;
  __sync_synchronize();
}

#define fluid_atomic_int_dec_and_test(_pi) (__sync_sub_and_fetch(_pi, 1) == 0)
#define fluid_atomic_int_compare_and_exchange(_pi, _old, _new) \
  ((int)__sync_bool_compare_and_swap(_pi, _old, _new))

static FLUID_INLINE void*
fluid_atomic_pointer_get(volatile void* _pi)
{
  __sync_synchronize();
  return *(void**)_pi;
}

static FLUID_INLINE void
fluid_atomic_pointer_set(volatile void* _pi, void* _val) {
  *(void**)_pi = _val;
  __sync_synchronize();
}

#define fluid_atomic_pointer_compare_and_exchange \
  fluid_atomic_int_compare_and_exchange

#endif // ifdef __ATOMIC_SEQ_CST

#elif defined(_WIN32)

#include <Windows.h>

#define fluid_atomic_int_exchange_and_add(_pi, _val) \
  ((LONG)InterlockedExchangeAdd((LONG volatile*)(_pi), (LONG)(_val)))

#define fluid_atomic_int_get(_pi) (*(int*)(_pi))
#define fluid_atomic_int_set(_pi, _val) (void)(InterlockedExchange (\
                                               (LONG volatile*)(_pi), \
                                               (LONG)(_val)))
#define fluid_atomic_int_dec_and_test(_pi) (InterlockedDecrement( \
                                              (LONG volatile*)(_pi)) == 0)

#define fluid_atomic_int_compare_and_exchange(_pi, _old, _new) \
  InterlockedCompareExchange((LONG volatile*)(_pi), (LONG)(_new), (LONG)(_old))

#define fluid_atomic_pointer_get(_pp) (*(void**)pp)
#define fluid_atomic_pointer_set(_pp, _val) (void)(InterlockedExchangePointer(_pp,\
                                                    _val))

#define fluid_atomic_pointer_compare_and_exchange(_pp, _old, _new) \
  InterlockedCompareExchangePointer(_pp, _new, _old)

#else

#error Unsupported system.

#endif // defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4), defined(_WIN32)

#define fluid_atomic_int_add(_pi, _val) (void)(fluid_atomic_int_exchange_and_add(\
                                                _pi, _val))
#define fluid_atomic_int_inc(_pi) fluid_atomic_int_add(_pi, 1)

static FLUID_INLINE void fluid_atomic_float_set(volatile float *fptr, float val)
{
  sint32 ival;
  memcpy (&ival, &val, 4);
  fluid_atomic_int_set ((volatile int *)fptr, ival);
}

static FLUID_INLINE float fluid_atomic_float_get(volatile float *fptr)
{
  sint32 ival;
  float fval;
  ival = fluid_atomic_int_get ((volatile int *)fptr);
  memcpy (&fval, &ival, 4);
  return fval;
}

#endif
