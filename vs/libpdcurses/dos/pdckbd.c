/* Public Domain Curses */

#include "pdcdos.h"

/*man-start**************************************************************

pdckbd
------

### Synopsis

    unsigned long PDC_get_input_fd(void);

### Description

   PDC_get_input_fd() returns the file descriptor that PDCurses
   reads its input from. It can be used for select().

### Portability
                             X/Open    BSD    SYS V
    PDC_get_input_fd            -       -       -

**man-end****************************************************************/

#ifdef __DJGPP__
# include <fcntl.h>
# include <io.h>
# include <signal.h>
#endif

/************************************************************************
 *    Table for key code translation of function keys in keypad mode    *
 *    These values are for strict IBM keyboard compatibles only         *
 ************************************************************************/

static short key_table[] =
{
    -1,             ALT_ESC,        -1,             0,
    -1,             -1,             -1,             -1,
    -1,             -1,             -1,             -1,
    -1,             -1,             ALT_BKSP,       KEY_BTAB,
    ALT_Q,          ALT_W,          ALT_E,          ALT_R,
    ALT_T,          ALT_Y,          ALT_U,          ALT_I,
    ALT_O,          ALT_P,          ALT_LBRACKET,   ALT_RBRACKET,
    ALT_ENTER,      -1,             ALT_A,          ALT_S,
    ALT_D,          ALT_F,          ALT_G,          ALT_H,
    ALT_J,          ALT_K,          ALT_L,          ALT_SEMICOLON,
    ALT_FQUOTE,     ALT_BQUOTE,     -1,             ALT_BSLASH,
    ALT_Z,          ALT_X,          ALT_C,          ALT_V,
    ALT_B,          ALT_N,          ALT_M,          ALT_COMMA,
    ALT_STOP,       ALT_FSLASH,     -1,             ALT_PADSTAR,
    -1,             -1,             -1,             KEY_F(1),
    KEY_F(2),       KEY_F(3),       KEY_F(4),       KEY_F(5),
    KEY_F(6),       KEY_F(7),       KEY_F(8),       KEY_F(9),
    KEY_F(10),      -1,             -1,             KEY_HOME,
    KEY_UP,         KEY_PPAGE,      ALT_PADMINUS,   KEY_LEFT,
    KEY_B2,         KEY_RIGHT,      ALT_PADPLUS,    KEY_END,
    KEY_DOWN,       KEY_NPAGE,      KEY_IC,         KEY_DC,
    KEY_F(13),      KEY_F(14),      KEY_F(15),      KEY_F(16),
    KEY_F(17),      KEY_F(18),      KEY_F(19),      KEY_F(20),
    KEY_F(21),      KEY_F(22),      KEY_F(25),      KEY_F(26),
    KEY_F(27),      KEY_F(28),      KEY_F(29),      KEY_F(30),
    KEY_F(31),      KEY_F(32),      KEY_F(33),      KEY_F(34),
    KEY_F(37),      KEY_F(38),      KEY_F(39),      KEY_F(40),
    KEY_F(41),      KEY_F(42),      KEY_F(43),      KEY_F(44),
    KEY_F(45),      KEY_F(46),      -1,             CTL_LEFT,
    CTL_RIGHT,      CTL_END,        CTL_PGDN,       CTL_HOME,
    ALT_1,          ALT_2,          ALT_3,          ALT_4,
    ALT_5,          ALT_6,          ALT_7,          ALT_8,
    ALT_9,          ALT_0,          ALT_MINUS,      ALT_EQUAL,
    CTL_PGUP,       KEY_F(11),      KEY_F(12),      KEY_F(23),
    KEY_F(24),      KEY_F(35),      KEY_F(36),      KEY_F(47),
    KEY_F(48),      CTL_UP,         CTL_PADMINUS,   CTL_PADCENTER,
    CTL_PADPLUS,    CTL_DOWN,       CTL_INS,        CTL_DEL,
    CTL_TAB,        CTL_PADSLASH,   CTL_PADSTAR,    ALT_HOME,
    ALT_UP,         ALT_PGUP,       -1,             ALT_LEFT,
    -1,             ALT_RIGHT,      -1,             ALT_END,
    ALT_DOWN,       ALT_PGDN,       ALT_INS,        ALT_DEL,
    ALT_PADSLASH,   ALT_TAB,        ALT_PADENTER,   -1
};

unsigned long pdc_key_modifiers = 0L;

static struct {unsigned short pressed, released;} button[3];

static bool mouse_avail = FALSE, mouse_vis = FALSE, mouse_moved = FALSE,
            mouse_button = FALSE, key_pressed = FALSE;

static unsigned char mouse_scroll = 0;
static PDCREGS ms_regs, old_ms;
static unsigned short shift_status, old_shift = 0;
static unsigned char keyboard_function = 0xff, shift_function = 0xff,
                     check_function = 0xff;

static const unsigned short button_map[3] = {0, 2, 1};

unsigned long PDC_get_input_fd(void)
{
    PDC_LOG(("PDC_get_input_fd() - called\n"));

    return (unsigned long)fileno(stdin);
}

void PDC_set_keyboard_binary(bool on)
{
    PDC_LOG(("PDC_set_keyboard_binary() - called\n"));

#ifdef __DJGPP__
    setmode(fileno(stdin), on ? O_BINARY : O_TEXT);
    signal(SIGINT, on ? SIG_IGN : SIG_DFL);
#endif
}

/* check if a key or mouse event is waiting */

bool PDC_check_key(void)
{
    PDCREGS regs;

    if (shift_function == 0xff)
    {
        int scan;

        /* get shift status for all keyboards */

        regs.h.ah = 0x02;
        PDCINT(0x16, regs);
        scan = regs.h.al;

        /* get shift status for enhanced keyboards */

        regs.h.ah = 0x12;
        PDCINT(0x16, regs);

        if (scan == regs.h.al && getdosmembyte(0x496) == 0x10)
        {
            keyboard_function = 0x10;
            check_function = 0x11;
            shift_function = 0x12;
        }
        else
        {
            keyboard_function = 0;
            check_function = 1;
            shift_function = 2;
        }
    }

    regs.h.ah = shift_function;
    PDCINT(0x16, regs);

    shift_status = regs.W.ax;

    if (mouse_vis)
    {
        unsigned short i;

        ms_regs.W.ax = 3;
        PDCINT(0x33, ms_regs);

        mouse_button = FALSE;

        for (i = 0; i < 3; i++)
        {
            regs.W.ax = 6;
            regs.W.bx = button_map[i];
            PDCINT(0x33, regs);
            button[i].released = regs.W.bx;
            if (regs.W.bx)
            {
                ms_regs.W.cx = regs.W.cx;
                ms_regs.W.dx = regs.W.dx;
                mouse_button = TRUE;
            }

            regs.W.ax = 5;
            regs.W.bx = button_map[i];
            PDCINT(0x33, regs);
            button[i].pressed = regs.W.bx;
            if (regs.W.bx)
            {
                ms_regs.W.cx = regs.W.cx;
                ms_regs.W.dx = regs.W.dx;
                mouse_button = TRUE;
            }
        }

        mouse_scroll = ms_regs.h.bh;

        mouse_moved = !mouse_button && ms_regs.h.bl &&
                       ms_regs.h.bl == old_ms.h.bl &&
                    (((ms_regs.W.cx ^ old_ms.W.cx) >> 3) ||
                     ((ms_regs.W.dx ^ old_ms.W.dx) >> 3));

        if (mouse_scroll || mouse_button || mouse_moved)
            return TRUE;
    }

    if (old_shift && !shift_status)     /* modifier released */
    {
        if (!key_pressed && SP->return_key_modifiers)
            return TRUE;
    }
    else if (!old_shift && shift_status)    /* modifier pressed */
        key_pressed = FALSE;

    old_shift = shift_status;

    regs.h.ah = check_function;
    PDCINT(0x16, regs);

    return !(regs.W.flags & 64);
}

static int _process_mouse_events(void)
{
    int i;
    short shift_flags = 0;

    memset(&pdc_mouse_status, 0, sizeof(pdc_mouse_status));

    key_pressed = TRUE;
    old_shift = shift_status;
    SP->key_code = TRUE;

    /* Set shift modifiers */

    if (shift_status & 3)
        shift_flags |= BUTTON_SHIFT;

    if (shift_status & 4)
        shift_flags |= BUTTON_CONTROL;

    if (shift_status & 8)
        shift_flags |= BUTTON_ALT;

    /* Scroll wheel support for CuteMouse */

    if (mouse_scroll)
    {
        pdc_mouse_status.changes = mouse_scroll & 0x80 ?
            PDC_MOUSE_WHEEL_UP : PDC_MOUSE_WHEEL_DOWN;

        pdc_mouse_status.x = -1;
        pdc_mouse_status.y = -1;

        return KEY_MOUSE;
    }

    if (mouse_moved)
    {
        pdc_mouse_status.changes = PDC_MOUSE_MOVED;

        for (i = 0; i < 3; i++)
        {
            if (ms_regs.h.bl & (1 << button_map[i]))
            {
                pdc_mouse_status.button[i] = BUTTON_MOVED | shift_flags;
                pdc_mouse_status.changes |= (1 << i);
            }
        }
    }
    else    /* button event */
    {
        for (i = 0; i < 3; i++)
        {
            if (button[i].pressed)
            {
                /* Check for a click -- a PRESS followed
                   immediately by a release */

                if (!button[i].released)
                {
                    if (SP->mouse_wait)
                    {
                        PDCREGS regs;

                        napms(SP->mouse_wait);

                        regs.W.ax = 6;
                        regs.W.bx = button_map[i];
                        PDCINT(0x33, regs);

                        pdc_mouse_status.button[i] = regs.W.bx ?
                            BUTTON_CLICKED : BUTTON_PRESSED;
                    }
                    else
                        pdc_mouse_status.button[i] = BUTTON_PRESSED;
                }
                else
                    pdc_mouse_status.button[i] = BUTTON_CLICKED;
            }

            if (button[i].pressed || button[i].released)
            {
                pdc_mouse_status.button[i] |= shift_flags;
                pdc_mouse_status.changes |= (1 << i);
            }
        }
    }

    pdc_mouse_status.x = ms_regs.W.cx >> 3;
    pdc_mouse_status.y = ms_regs.W.dx >> 3;

    old_ms = ms_regs;

    return KEY_MOUSE;
}

/* return the next available key or mouse event */

int PDC_get_key(void)
{
    PDCREGS regs;
    int key, scan;

    pdc_key_modifiers = 0;

    if (mouse_vis && (mouse_scroll || mouse_button || mouse_moved))
        return _process_mouse_events();

    /* Return modifiers as keys? */

    if (old_shift && !shift_status)
    {
        key = -1;

        if (old_shift & 1)
            key = KEY_SHIFT_R;

        if (old_shift & 2)
            key = KEY_SHIFT_L;

        if (shift_function == 0x12)
        {
            if (old_shift & 0x400)
                key = KEY_CONTROL_R;

            if (old_shift & 0x100)
                key = KEY_CONTROL_L;

            if (old_shift & 0x800)
                key = KEY_ALT_R;

            if (old_shift & 0x200)
                key = KEY_ALT_L;
        }
        else
        {
            if (old_shift & 4)
                key = KEY_CONTROL_R;

            if (old_shift & 8)
                key = KEY_ALT_R;
        }

        key_pressed = FALSE;
        old_shift = shift_status;

        SP->key_code = TRUE;
        return key;
    }

    regs.h.ah = keyboard_function;
    PDCINT(0x16, regs);
    key = regs.h.al;
    scan = regs.h.ah;

    if (SP->save_key_modifiers)
    {
        if (shift_status & 3)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_SHIFT;

        if (shift_status & 4)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_CONTROL;

        if (shift_status & 8)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_ALT;

        if (shift_status & 0x20)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_NUMLOCK;
    }

    if (scan == 0x1c && key == 0x0a)    /* ^Enter */
        key = CTL_ENTER;
    else if (scan == 0xe0 && key == 0x0d)   /* PadEnter */
        key = PADENTER;
    else if (scan == 0xe0 && key == 0x0a)   /* ^PadEnter */
        key = CTL_PADENTER;
    else if (scan == 0x37 && key == 0x2a)   /* Star */
        key = PADSTAR;
    else if (scan == 0x4a && key == 0x2d)   /* Minus */
        key = PADMINUS;
    else if (scan == 0x4e && key == 0x2b)   /* Plus */
        key = PADPLUS;
    else if (scan == 0xe0 && key == 0x2f)   /* Slash */
        key = PADSLASH;
    else if (key == 0x00 || (key == 0xe0 && scan > 53 && scan != 86))
        key = (scan > 0xa7) ? -1 : key_table[scan];

    if (shift_status & 3)
    {
        switch (key)
        {
        case KEY_HOME:  /* Shift Home */
            key = KEY_SHOME;
            break;
        case KEY_UP:    /* Shift Up */
            key = KEY_SUP;
            break;
        case KEY_PPAGE: /* Shift PgUp */
            key = KEY_SPREVIOUS;
            break;
        case KEY_LEFT:  /* Shift Left */
            key = KEY_SLEFT;
            break;
        case KEY_RIGHT: /* Shift Right */
            key = KEY_SRIGHT;
            break;
        case KEY_END:   /* Shift End */
            key = KEY_SEND;
            break;
        case KEY_DOWN:  /* Shift Down */
            key = KEY_SDOWN;
            break;
        case KEY_NPAGE: /* Shift PgDn */
            key = KEY_SNEXT;
            break;
        case KEY_IC:    /* Shift Ins */
            key = KEY_SIC;
            break;
        case KEY_DC:    /* Shift Del */
            key = KEY_SDC;
        }
    }

    key_pressed = TRUE;
    SP->key_code = ((unsigned)key >= 256);

    return key;
}

/* discard any pending keyboard or mouse input -- this is the core
   routine for flushinp() */

void PDC_flushinp(void)
{
    PDC_LOG(("PDC_flushinp() - called\n"));

    /* Force the BIOS keyboard buffer head and tail pointers to be
       the same...  Real nasty trick... */

    setdosmemword(0x41a, getdosmemword(0x41c));
}

int PDC_mouse_set(void)
{
    PDCREGS regs;
    unsigned long mbe = SP->_trap_mbe;

    if (mbe && !mouse_avail)
    {
        regs.W.ax = 0;
        PDCINT(0x33, regs);

        mouse_avail = !!(regs.W.ax);
    }

    if (mbe)
    {
        if (mouse_avail && !mouse_vis)
        {
            memset(&old_ms, 0, sizeof(old_ms));

            regs.W.ax = 1;
            PDCINT(0x33, regs);

            mouse_vis = TRUE;
        }
    }
    else
    {
        if (mouse_avail && mouse_vis)
        {
            regs.W.ax = 2;
            PDCINT(0x33, regs);

            mouse_vis = FALSE;
        }
    }

    return (mouse_avail || !mbe) ? OK : ERR;
}

int PDC_modifiers_set(void)
{
    key_pressed = FALSE;

    return OK;
}
