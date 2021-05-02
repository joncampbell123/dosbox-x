#include "dos_inc.h"

static const unsigned char bin_132_com[] = { 0xB0, 0x54, 0xB4, 0x00, 0xCD, 0x10, 0xCD, 0x20 };

struct BuiltinFileBlob bfb_132_COM = {
        /*recommended file name*/       "132.COM",
        /*data*/                        bin_132_com,
        /*length*/                      sizeof(bin_132_com)
};
