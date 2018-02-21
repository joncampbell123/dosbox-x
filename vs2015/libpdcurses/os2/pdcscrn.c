/* Public Domain Curses */

#include "pdcos2.h"

/* COLOR_PAIR to attribute encoding table. */

static struct {short f, b;} atrtab[PDC_COLOR_PAIRS];

static short realtocurs[16] =
{
    COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, COLOR_RED,
    COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE, COLOR_BLACK + 8,
    COLOR_BLUE + 8, COLOR_GREEN + 8, COLOR_CYAN + 8, COLOR_RED + 8,
    COLOR_MAGENTA + 8, COLOR_YELLOW + 8, COLOR_WHITE + 8
};

static PCH saved_screen = NULL;
static USHORT saved_lines = 0;
static USHORT saved_cols = 0;
static VIOMODEINFO scrnmode;    /* default screen mode  */
static VIOMODEINFO saved_scrnmode[3];
static int saved_font[3];

short pdc_curstoreal[16];
int pdc_font;  /* default font size */

static int _get_font(void)
{
    VIOMODEINFO modeInfo = {0};

    modeInfo.cb = sizeof(modeInfo);

    VioGetMode(&modeInfo, 0);
    return (modeInfo.vres / modeInfo.row);
}

static void _set_font(int size)
{
    VIOMODEINFO modeInfo = {0};

    if (pdc_font != size)
    {
        modeInfo.cb = sizeof(modeInfo);

        /* set most parameters of modeInfo */

        VioGetMode(&modeInfo, 0);
        modeInfo.cb = 8;    /* ignore horiz an vert resolution */
        modeInfo.row = modeInfo.vres / size;
        VioSetMode(&modeInfo, 0);
    }

    curs_set(SP->visibility);

    pdc_font = _get_font();
}

/* close the physical screen -- may restore the screen to its state
   before PDC_scr_open(); miscellaneous cleanup */

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));

    if (saved_screen && getenv("PDC_RESTORE_SCREEN"))
    {
        VioWrtCellStr(saved_screen, saved_lines * saved_cols * 2,
            0, 0, (HVIO)NULL);

        free(saved_screen);
        saved_screen = NULL;
    }

    reset_shell_mode();

    if (SP->visibility != 1)
        curs_set(1);

    /* Position cursor to the bottom left of the screen. */

    PDC_gotoyx(PDC_get_rows() - 2, 0);
}

void PDC_scr_free(void)
{
    if (SP)
        free(SP);
}

/* open the physical screen -- allocate SP, miscellaneous intialization,
   and may save the existing screen for later restoration */

int PDC_scr_open(int argc, char **argv)
{
    USHORT totchars;
    int i;

    PDC_LOG(("PDC_scr_open() - called\n"));

    SP = calloc(1, sizeof(SCREEN));

    if (!SP)
        return ERR;

    for (i = 0; i < 16; i++)
        pdc_curstoreal[realtocurs[i]] = i;

    SP->orig_attr = FALSE;

    VioGetMode(&scrnmode, 0);
    PDC_get_keyboard_info();
    pdc_font = _get_font();

    SP->lines = PDC_get_rows();
    SP->cols = PDC_get_columns();

    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->audible = TRUE;

    SP->termattrs = (SP->mono ? 0 : A_COLOR) | A_REVERSE | A_BLINK;

    /* This code for preserving the current screen */

    if (getenv("PDC_RESTORE_SCREEN"))
    {
        saved_lines = SP->lines;
        saved_cols = SP->cols;

        saved_screen = malloc(2 * saved_lines * saved_cols);

        if (!saved_screen)
        {
            SP->_preserve = FALSE;
            return OK;
        }

        totchars = saved_lines * saved_cols * 2;
        VioReadCellStr((PCH)saved_screen, &totchars, 0, 0, (HVIO)NULL);
    }

    SP->_preserve = (getenv("PDC_PRESERVE_SCREEN") != NULL);

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
    VIOMODEINFO modeInfo = {0};
    USHORT result;

    PDC_LOG(("PDC_resize_screen() - called. Lines: %d Cols: %d\n",
              nlines, ncols));

    modeInfo.cb = sizeof(modeInfo);

    /* set most parameters of modeInfo */

    VioGetMode(&modeInfo, 0);
    modeInfo.fbType = 1;
    modeInfo.row = nlines;
    modeInfo.col = ncols;
    result = VioSetMode(&modeInfo, 0);

    LINES = PDC_get_rows();
    COLS = PDC_get_columns();

    return (result == 0) ? OK : ERR;
}

void PDC_reset_prog_mode(void)
{
    PDC_LOG(("PDC_reset_prog_mode() - called.\n"));

    PDC_set_keyboard_binary(TRUE);
}

void PDC_reset_shell_mode(void)
{
    PDC_LOG(("PDC_reset_shell_mode() - called.\n"));

    PDC_set_keyboard_default();
}

static bool _screen_mode_equals(VIOMODEINFO *oldmode)
{
    VIOMODEINFO current = {0};

    VioGetMode(&current, 0);

    return ((current.cb == oldmode->cb) &&
            (current.fbType == oldmode->fbType) &&
            (current.color == oldmode->color) &&
            (current.col == oldmode->col) &&
            (current.row == oldmode->row) &&
            (current.hres == oldmode->vres) &&
            (current.vres == oldmode->vres));
}

void PDC_restore_screen_mode(int i)
{
    if (i >= 0 && i <= 2)
    {
        pdc_font = _get_font();
        _set_font(saved_font[i]);

        if (!_screen_mode_equals(&saved_scrnmode[i]))
            if (VioSetMode(&saved_scrnmode[i], 0) != 0)
            {
                pdc_font = _get_font();
                scrnmode = saved_scrnmode[i];
                LINES = PDC_get_rows();
                COLS = PDC_get_columns();
            }
    }
}

void PDC_save_screen_mode(int i)
{
    if (i >= 0 && i <= 2)
    {
        saved_font[i] = pdc_font;
        saved_scrnmode[i] = scrnmode;
    }
}

void PDC_init_pair(short pair, short fg, short bg)
{
    atrtab[pair].f = fg;
    atrtab[pair].b = bg;
}

int PDC_pair_content(short pair, short *fg, short *bg)
{
    *fg = atrtab[pair].f;
    *bg = atrtab[pair].b;

    return OK;
}

bool PDC_can_change_color(void)
{
    return FALSE;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    return ERR;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    return ERR;
}
