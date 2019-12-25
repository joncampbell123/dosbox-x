
#include "dos_inc.h"

static const unsigned char bin_25_com[] = { 0xB8, 0x14, 0x11, 0x30, 0xDB, 0xCD, 0x10, 0xC3 };

struct BuiltinFileBlob bfb_25_COM = {
	/*recommended file name*/	"25.COM",
	/*data*/			bin_25_com,
	/*length*/			sizeof(bin_25_com)
};

