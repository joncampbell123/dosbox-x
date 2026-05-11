
enum {
    GDC_CMD_RESET = 0x00,                       // 0   0   0   0   0   0   0   0
    GDC_CMD_DISPLAY_BLANK = 0x0C,               // 0   0   0   0   1   1   0   DE
    GDC_CMD_SYNC = 0x0E,                        // 0   0   0   0   1   1   1   DE
    GDC_CMD_MODE_REPLACE = 0x20,                // 0   0   1   0   0   0   0   0
    GDC_CMD_MODE_COMPLEMENT = 0x21,             // 0   0   1   0   0   0   0   1
    GDC_CMD_MODE_CLEAR = 0x22,                  // 0   0   1   0   0   0   1   0
    GDC_CMD_MODE_SET = 0x23,                    // 0   0   1   0   0   0   1   1
    GDC_CMD_CURSOR_POSITION = 0x49,             // 0   1   0   0   1   0   0   1
    GDC_CMD_CSRW = 0x49,                        // 0   1   0   0   1   0   0   1
    GDC_CMD_CURSOR_CHAR_SETUP = 0x4B,           // 0   1   0   0   1   0   1   1
    GDC_CMD_ZOOM = 0x46,                        // 0   1   0   0   0   1   1   0
    GDC_CMD_PITCH_SPEC = 0x47,                  // 0   1   0   0   0   1   1   1
    GDC_CMD_VECTW = 0x4C,                       // 0   1   0   0   1   1   0   0
    GDC_CMD_TEXTE = 0x68,                       // 0   1   1   0   1   0   0   0
    GDC_CMD_START_DISPLAY = 0x6B,               // 0   1   1   0   1   0   1   1
    GDC_CMD_VECTE = 0x6c,                       // 0   1   1   0   1   1   0   0
    GDC_CMD_VERTICAL_SYNC_MODE = 0x6E,          // 0   1   1   0   1   1   1   M
    GDC_CMD_PARAMETER_RAM_LOAD = 0x70,          // 0   1   1   1   S   S   S   S    S[3:0] = starting address in parameter RAM
    GDC_CMD_TEXTW = 0x78,                       // 0   1   1   1   1   0   0   0    This is the same as GDC_CMD_PARAMETER_RAM_LOAD+8 aka loading at the 8th byte
    GDC_CMD_CURSOR_ADDRESS_READ = 0xE0          // 1   1   1   0   0   0   0   0
};
