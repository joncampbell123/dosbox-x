/*
 *  Copyright (C) 2011-2023  The DOSBox-X Team
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

#if WIN32 && !defined(HX_DOS)
#ifdef _MSC_VER
#if defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86)
#if _MSC_VER <= 1916
#define OS_PLATFORM "VS XP"
#define OS_PLATFORM_LONG "Visual Studio XP"
#else
#define OS_PLATFORM "VS"
#define OS_PLATFORM_LONG "Visual Studio"
#endif
#else
#define OS_PLATFORM "VS ARM"
#define OS_PLATFORM_LONG "Visual Studio ARM"
#endif
#elif defined(__MINGW32__)
#define OS_PLATFORM "MinGW"
#define OS_PLATFORM_LONG "MinGW"
#else
#define OS_PLATFORM "Windows"
#define OS_PLATFORM_LONG "Windows"
#endif
#elif defined(HX_DOS)
#define OS_PLATFORM "DOS"
#define OS_PLATFORM_LONG "DOS"
#elif defined(LINUX)
#define OS_PLATFORM "Linux"
#define OS_PLATFORM_LONG "Linux"
#elif defined(MACOSX)
#ifdef __arm__
#define OS_PLATFORM "ARM mac"
#define OS_PLATFORM_LONG "macOS ARM"
#else
#define OS_PLATFORM "x86 mac"
#define OS_PLATFORM_LONG "macOS Intel"
#endif

#else
#define OS_PLATFORM ""
#define OS_PLATFORM_LONG ""
#endif

#if defined(_M_X64) || defined (_M_AMD64) || defined (_M_ARM64) || defined (_M_IA64) || defined(__ia64__) || defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__powerpc64__)
#define OS_BIT "64"
#define OS_BIT_INT 64
#else
#define OS_BIT "32"
#define OS_BIT_INT 32
#endif
