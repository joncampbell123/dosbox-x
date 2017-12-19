
enum {
    GDC_CMD_RESET = 0x00,                       // 0   0   0   0   0   0   0   0
    GDC_CMD_DISPLAY_BLANK = 0x0C,               // 0   0   0   0   1   1   0   DE
    GDC_CMD_SYNC = 0x0E,                        // 0   0   0   0   1   1   1   DE
    GDC_CMD_CURSOR_POSITION = 0x49,             // 0   1   0   0   1   0   0   1
    GDC_CMD_CURSOR_CHAR_SETUP = 0x4B,           // 0   1   0   0   1   0   1   1
    GDC_CMD_PITCH_SPEC = 0x47,                  // 0   1   0   0   0   1   1   1
    GDC_CMD_START_DISPLAY = 0x6B,               // 0   1   1   0   1   0   1   1
    GDC_CMD_VERTICAL_SYNC_MODE = 0x6E,          // 0   1   1   0   1   1   1   M
    GDC_CMD_PARAMETER_RAM_LOAD = 0x70           // 0   1   1   1   S   S   S   S    S[3:0] = starting address in parameter RAM
};

