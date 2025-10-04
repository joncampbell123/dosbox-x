
#include "dos_inc.h"

static const unsigned char bin_28_com[] = { 0xB8, 0x11, 0x11, 0x30, 0xDB, 0xCD, 0x10, 0xC3 };

struct BuiltinFileBlob bfb_28_COM = {
	/*recommended file name*/	"28.COM",
	/*data*/			bin_28_com,
	/*length*/			sizeof(bin_28_com)
};

static const unsigned char bin_28_com_ega[] = { 0xB8, 0x12, 0x11, 0x30, 0xDB, 0xCD, 0x10, 0xC3 };

struct BuiltinFileBlob bfb_28_COM_ega = {
	/*recommended file name*/	"28.COM",
	/*data*/			bin_28_com_ega,
	/*length*/			sizeof(bin_28_com_ega)
};

