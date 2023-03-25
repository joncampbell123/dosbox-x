#pragma once

#include "config.h"
#include <stdint.h>

/*
  For MinGW and MinGW-w64, _M_IX86 and _M_X64 are not defined by the compiler,
  but in a header file, which has to be (indirectly) included, usually through a
  C (not C++) standard header file. For MinGW it is sdkddkver.h and for
  MinGW-w64 it is _mingw_mac.h. Do not rely on constants that may not be
  defined, depending on what was included before these lines.
*/
#if ((defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64) && \
     (defined _WIN32 || defined BSD || defined LINUX || defined __CYGWIN__)) // _WIN32 is not defined by default on Cygwin
bool initPassthroughIO(void);
#if defined BSD || defined LINUX
bool dropPrivileges(void);
#endif
#endif

uint8_t inportb(uint16_t port);
void outportb(uint16_t port, uint8_t value);

uint16_t inportw(uint16_t port);
void outportw(uint16_t port, uint16_t value);

uint32_t inportd(uint16_t port);
void outportd(uint16_t port, uint32_t value);
