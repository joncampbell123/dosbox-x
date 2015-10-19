
#include "dos_inc.h"

static const unsigned char bin_50_com[] = { 0xB8, 0x12, 0x11, 0x30, 0xDB, 0xCD, 0x10, 0xC3 };

struct BuiltinFileBlob bfb_50_COM = {
	/*recommended file name*/	"50.COM",
	/*data*/			bin_50_com,
	/*length*/			sizeof(bin_50_com)
};

