#pragma once

#include "config.h"

// NOTE: In order to make the code work on Linux and the BSDs, disable the call to dropPrivileges() in parport.cpp.
#if 0 && /* Replace the 0 with 1 in order to enable the OPL pass-through code. */ \
    (defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64) && \
    (defined _WIN32 || defined BSD || defined LINUX || defined __CYGWIN__) // _WIN32 is not defined by default on Cygwin
#define HAS_HARDOPL 1

extern void HARDOPL_Init(Bitu hardwareaddr, Bitu sbbase, bool isCMS);
extern void HWOPL_Cleanup();

#else
#define HAS_HARDOPL 0
#endif
