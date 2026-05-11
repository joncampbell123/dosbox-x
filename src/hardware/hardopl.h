#pragma once

#include "config.h"

#if (defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64) && \
    (defined _WIN32 || defined BSD || defined LINUX || defined __CYGWIN__) // _WIN32 is not defined by default on Cygwin
#define HAS_HARDOPL 1

extern void HARDOPL_Init(Bitu hardwareaddr, Bitu sbbase, bool isCMS);
extern void HARDOPL_Cleanup();

#else
#define HAS_HARDOPL 0
#endif
