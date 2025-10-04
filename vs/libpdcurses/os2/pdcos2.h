/* Public Domain Curses */

#include <stdlib.h>
#include <string.h>

#define INCL_DOS
#define INCL_DOSMISC
#define INCL_WIN
#define INCL_VIO
#define INCL_KBD
#define INCL_MOU
#include <os2.h>

#include <curspriv.h>

extern ULONG pdc_last_blink;
extern short pdc_curstoreal[16];
extern int pdc_font;

extern ULONG PDC_ms_count(void);
extern void PDC_get_keyboard_info(void);
extern void PDC_set_keyboard_default(void);
extern void PDC_blink_text(void);
