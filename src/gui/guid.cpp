
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _MSC_VER
# include <inttypes.h>
#endif
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "guid.h"

unsigned char windows_IsEqualGUID(const windows_GUID *a,const windows_GUID *b) {
	return (memcmp(a,b,sizeof(windows_GUID)) == 0) ? 1 : 0;
}

