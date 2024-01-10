/*
 *  I/O port pass-through for DOSBox-X
 *  Copyright (C) 2023-2024 Daniël Hörchner

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

#pragma once

#include "config.h"
#include <stdint.h>

/*
  For MinGW and MinGW-w64, _M_IX86 and _M_X64 are not defined by the compiler,
  but in a header file, which has to be (indirectly) included, usually through
  a C (not C++) standard header file. For MinGW it is sdkddkver.h and for
  MinGW-w64 it is _mingw_mac.h. Do not rely on constants that may not be
  defined, depending on what was included before these lines.
*/
#if (defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64) && \
    (defined _WIN32 || defined BSD || defined LINUX || defined __CYGWIN__) // _WIN32 is not defined by default on Cygwin
bool initPassthroughIO(void);
#if defined BSD || defined LINUX
bool dropPrivileges(void);
bool dropPrivilegesTemp(void);
bool regainPrivileges(void);
#endif
#endif

uint8_t inportb(uint16_t port);
void outportb(uint16_t port, uint8_t value);

uint16_t inportw(uint16_t port);
void outportw(uint16_t port, uint16_t value);

uint32_t inportd(uint16_t port);
void outportd(uint16_t port, uint32_t value);
