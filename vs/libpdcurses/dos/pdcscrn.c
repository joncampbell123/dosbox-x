/* Public Domain Curses */

#include "pdcdos.h"

#include <stdlib.h>

/* COLOR_PAIR to attribute encoding table. */

static struct {short f, b;} atrtab[PDC_COLOR_PAIRS];

int pdc_adapter;         /* screen type */
int pdc_scrnmode;        /* default screen mode */
int pdc_font;            /* default font size */
bool pdc_direct_video;   /* allow direct screen memory writes */
bool pdc_bogus_adapter;  /* TRUE if adapter has insane values */
unsigned pdc_video_seg;  /* video base segment */
unsigned pdc_video_ofs;  /* video base offset */

static short realtocurs[16] =
{
    COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, COLOR_RED,
    COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE, COLOR_BLACK + 8,
    COLOR_BLUE + 8, COLOR_GREEN + 8, COLOR_CYAN + 8, COLOR_RED + 8,
    COLOR_MAGENTA + 8, COLOR_YELLOW + 8, COLOR_WHITE + 8
};

short pdc_curstoreal[16];

static bool sizeable = FALSE;   /* TRUE if adapter is resizeable    */

static unsigned short *saved_screen = NULL;
static int saved_lines = 0;
static int saved_cols = 0;

static int saved_scrnmode[3];
static int saved_font[3];

/* Thanks to Jeff Duntemann, K16RA for providing the impetus
   (through the Dr. Dobbs Journal, March 1989 issue) for getting
   the routines below merged into Bjorn Larsson's PDCurses 1.3...
    -- frotz@dri.com    900730 */

/* _get_font() - Get the current font size */

static int _get_font(void)
{
    int retval;

    retval = getdosmemword(0x485);

    /* Assume the MDS Genius is in 66 line mode. */

    if ((retval == 0) && (pdc_adapter == _MDS_GENIUS))
        retval = _FONT15;

    switch (pdc_adapter)
    {
    case _MDA:
        retval = 10;    /* POINTS is not certain on MDA/Hercules */
        break;

    case _EGACOLOR:
    case _EGAMONO:
        switch (retval)
        {
        case _FONT8:
        case _FONT14:
            break;
        default:
            retval = _FONT14;
        }
        break;

    case _CGA:
        retval = _FONT8;
    }

    return retval;
}

/* _set_font() - Sets the current font size, if the adapter allows such a
   change. It is an error to attempt to change the font size on a
   "bogus" adapter. The reason for this is that we have a known video
   adapter identity problem. e.g. Two adapters report the same identifying
   characteristics. */

static void _set_font(int size)
{
    PDCREGS regs;

    if (pdc_bogus_adapter)
        return;

    switch (pdc_adapter)
    {
    case _CGA:
    case _MDA:
    case _MCGACOLOR:
    case _MCGAMONO:
    case _MDS_GENIUS:
        break;

    case _EGACOLOR:
    case _EGAMONO:
        if (sizeable && (pdc_font != size))
        {
            switch (size)
            {
            case _FONT8:
                regs.W.ax = 0x1112;
                regs.h.bl = 0x00;
                PDCINT(0x10, regs);
                break;
            case _FONT14:
                regs.W.ax = 0x1111;
                regs.h.bl = 0x00;
                PDCINT(0x10, regs);
            }
        }
        break;

    case _VGACOLOR:
    case _VGAMONO:
        if (sizeable && (pdc_font != size))
        {
            switch (size)
            {
            case _FONT8:
                regs.W.ax = 0x1112;
                regs.h.bl = 0x00;
                PDCINT(0x10, regs);
                break;
            case _FONT14:
                regs.W.ax = 0x1111;
                regs.h.bl = 0x00;
                PDCINT(0x10, regs);
                break;
            case _FONT16:
                regs.W.ax = 0x1114;
                regs.h.bl = 0x00;
                PDCINT(0x10, regs);
            }
        }
    }

    curs_set(SP->visibility);

    pdc_font = _get_font();
}

/* _set_80x25() - force a known screen state: 80x25 text mode. Forces the
   appropriate 80x25 alpha mode given the display adapter. */

static void _set_80x25(void)
{
    PDCREGS regs;

    switch (pdc_adapter)
    {
    case _CGA:
    case _EGACOLOR:
    case _EGAMONO:
    case _VGACOLOR:
    case _VGAMONO:
    case _MCGACOLOR:
    case _MCGAMONO:
        regs.h.ah = 0x00;
        regs.h.al = 0x03;
        PDCINT(0x10, regs);
        break;
    case _MDA:
        regs.h.ah = 0x00;
        regs.h.al = 0x07;
        PDCINT(0x10, regs);
    }
}

/* _get_scrn_mode() - Return the current BIOS video mode */

static int _get_scrn_mode(void)
{
    PDCREGS regs;

    regs.h.ah = 0x0f;
    PDCINT(0x10, regs);

    return (int)regs.h.al;
}

/* _set_scrn_mode() - Sets the BIOS Video Mode Number only if it is
   different from the current video mode. */

static void _set_scrn_mode(int new_mode)
{
    PDCREGS regs;

    if (_get_scrn_mode() != new_mode)
    {
        regs.h.ah = 0;
        regs.h.al = (unsigned char) new_mode;
        PDCINT(0x10, regs);
    }

    pdc_font = _get_font();
    pdc_scrnmode = new_mode;
    LINES = PDC_get_rows();
    COLS = PDC_get_columns();
}

/* _sanity_check() - A video adapter identification sanity check. This
   routine will force sane values for various control flags. */

static int _sanity_check(int adapter)
{
    int fontsize = _get_font();
    int rows = PDC_get_rows();

    PDC_LOG(("_sanity_check() - called: Adapter %d\n", adapter));

    switch (adapter)
    {
    case _EGACOLOR:
    case _EGAMONO:
        switch (rows)
        {
        case 25:
        case 43:
            break;
        default:
            pdc_bogus_adapter = TRUE;
        }

        switch (fontsize)
        {
        case _FONT8:
        case _FONT14:
            break;
        default:
            pdc_bogus_adapter = TRUE;
        }
        break;

    case _VGACOLOR:
    case _VGAMONO:
        break;

    case _CGA:
    case _MDA:
    case _MCGACOLOR:
    case _MCGAMONO:
        switch (rows)
        {
        case 25:
            break;
        default:
            pdc_bogus_adapter = TRUE;
        }
        break;

    default:
        pdc_bogus_adapter = TRUE;
    }

    if (pdc_bogus_adapter)
    {
        sizeable = FALSE;
        pdc_direct_video = FALSE;
    }

    return adapter;
}

/* _query_adapter_type() - Determine PC video adapter type. */

static int _query_adapter_type(void)
{
    PDCREGS regs;
    int retval = _NONE;

    /* thanks to paganini@ax.apc.org for the GO32 fix */

#if !defined(__DJGPP__) && !defined(__WATCOMC__)
    struct SREGS segs;
#endif
    short video_base = getdosmemword(0x463);

    PDC_LOG(("_query_adapter_type() - called\n"));

    /* attempt to call VGA Identify Adapter Function */

    regs.W.ax = 0x1a00;
    PDCINT(0x10, regs);

    if ((regs.h.al == 0x1a) && (retval == _NONE))
    {
        /* We know that the PS/2 video BIOS is alive and well. */

        switch (regs.h.al)
        {
        case 0:
            retval = _NONE;
            break;
        case 1:
            retval = _MDA;
            break;
        case 2:
            retval = _CGA;
            break;
        case 4:
            retval = _EGACOLOR;
            sizeable = TRUE;
            break;
        case 5:
            retval = _EGAMONO;
            break;
        case 26:            /* ...alt. VGA BIOS... */
        case 7:
            retval = _VGACOLOR;
            sizeable = TRUE;
            break;
        case 8:
            retval = _VGAMONO;
            break;
        case 10:
        case 13:
            retval = _MCGACOLOR;
            break;
        case 12:
            retval = _MCGAMONO;
            break;
        default:
            retval = _CGA;
        }
    }
    else
    {
        /* No VGA BIOS, check for an EGA BIOS by selecting an
           Alternate Function Service...

           bx == 0x0010 --> return EGA information */

        regs.h.ah = 0x12;
        regs.W.bx = 0x10;
        PDCINT(0x10, regs);

        if ((regs.h.bl != 0x10) && (retval == _NONE))
        {
            /* An EGA BIOS exists */

            regs.h.ah = 0x12;
            regs.h.bl = 0x10;
            PDCINT(0x10, regs);

            if (regs.h.bh == 0)
                retval = _EGACOLOR;
            else
                retval = _EGAMONO;
        }
        else if (retval == _NONE)
        {
            /* Now we know we only have CGA or MDA */

            PDCINT(0x11, regs);

            switch (regs.h.al & 0x30)
            {
            case 0x10:
            case 0x20:
                retval = _CGA;
                break;
            case 0x30:
                retval = _MDA;
                break;
            default:
                retval = _NONE;
            }
        }
    }

    if (video_base == 0x3d4)
    {
        pdc_video_seg = 0xb800;
        switch (retval)
        {
        case _EGAMONO:
            retval = _EGACOLOR;
            break;
        case _VGAMONO:
            retval = _VGACOLOR;
        }
    }

    if (video_base == 0x3b4)
    {
        pdc_video_seg = 0xb000;
        switch (retval)
        {
        case _EGACOLOR:
            retval = _EGAMONO;
            break;
        case _VGACOLOR:
            retval = _VGAMONO;
        }
    }

    if ((retval == _NONE)
#ifndef CGA_DIRECT
    ||  (retval == _CGA)
#endif
    )
        pdc_direct_video = FALSE;

    if ((unsigned)pdc_video_seg == 0xb000)
        SP->mono = TRUE;
    else
        SP->mono = FALSE;

    /* Check for DESQview shadow buffer
       thanks to paganini@ax.apc.org for the GO32 fix */

#ifndef __WATCOMC__
    regs.h.ah = 0xfe;
    regs.h.al = 0;
    regs.x.di = pdc_video_ofs;
# ifdef __DJGPP__
    regs.x.es = pdc_video_seg;
    __dpmi_int(0x10, &regs);
    pdc_video_seg = regs.x.es;
# else
    segs.es   = pdc_video_seg;
    int86x(0x10, &regs, &regs, &segs);
    pdc_video_seg = segs.es;
# endif
    pdc_video_ofs = regs.x.di;
#endif
    if (!pdc_adapter)
        pdc_adapter = retval;

    return _sanity_check(retval);
}

/* close the physical screen -- may restore the screen to its state
   before PDC_scr_open(); miscellaneous cleanup */

void PDC_scr_close(void)
{
#if SMALL || MEDIUM
    struct SREGS segregs;
    int ds;
#endif
    PDC_LOG(("PDC_scr_close() - called\n"));

    if (getenv("PDC_RESTORE_SCREEN") && saved_screen)
    {
#ifdef __DJGPP__
        dosmemput(saved_screen, saved_lines * saved_cols * 2,
            (unsigned long)_FAR_POINTER(pdc_video_seg,
            pdc_video_ofs));
#else
# if (SMALL || MEDIUM)
        segread(&segregs);
        ds = segregs.ds;
        movedata(ds, (int)saved_screen, pdc_video_seg, pdc_video_ofs,
        (saved_lines * saved_cols * 2));
# else
        memcpy((void *)_FAR_POINTER(pdc_video_seg, pdc_video_ofs),
        (void *)saved_screen, (saved_lines * saved_cols * 2));
# endif
#endif
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
#if SMALL || MEDIUM
    struct SREGS segregs;
    int ds;
#endif
    int i;

    PDC_LOG(("PDC_scr_open() - called\n"));

    SP = calloc(1, sizeof(SCREEN));

    if (!SP)
        return ERR;

    for (i = 0; i < 16; i++)
        pdc_curstoreal[realtocurs[i]] = i;

    SP->orig_attr = FALSE;

    pdc_direct_video = TRUE; /* Assume that we can */
    pdc_video_seg = 0xb000;  /* Base screen segment addr */
    pdc_video_ofs = 0x0;     /* Base screen segment ofs */

    pdc_adapter = _query_adapter_type();
    pdc_scrnmode = _get_scrn_mode();
    pdc_font = _get_font();

    SP->lines = PDC_get_rows();
    SP->cols = PDC_get_columns();

    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->audible = TRUE;

    SP->termattrs = (SP->mono ? A_UNDERLINE : A_COLOR) | A_REVERSE | A_BLINK;

    /* If the environment variable PDCURSES_BIOS is set, the DOS int10()
       BIOS calls are used in place of direct video memory access. */

    if (getenv("PDCURSES_BIOS"))
        pdc_direct_video = FALSE;

    /* This code for preserving the current screen. */

    if (getenv("PDC_RESTORE_SCREEN"))
    {
        saved_lines = SP->lines;
        saved_cols = SP->cols;

        saved_screen = malloc(saved_lines * saved_cols * 2);

        if (!saved_screen)
        {
            SP->_preserve = FALSE;
            return OK;
        }
#ifdef __DJGPP__
        dosmemget((unsigned long)_FAR_POINTER(pdc_video_seg, pdc_video_ofs),
                  saved_lines * saved_cols * 2, saved_screen);
#else
# if SMALL || MEDIUM
        segread(&segregs);
        ds = segregs.ds;
        movedata(pdc_video_seg, pdc_video_ofs, ds, (int)saved_screen,
                 (saved_lines * saved_cols * 2));
# else
        memcpy((void *)saved_screen,
               (void *)_FAR_POINTER(pdc_video_seg, pdc_video_ofs),
               (saved_lines * saved_cols * 2));
# endif
#endif
    }

    SP->_preserve = (getenv("PDC_PRESERVE_SCREEN") != NULL);

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
    PDC_LOG(("PDC_resize_screen() - called. Lines: %d Cols: %d\n",
             nlines, ncols));

    /* Trash the stored value of orig_cursor -- it's only good if the
       video mode doesn't change */

    SP->orig_cursor = 0x0607;

    switch (pdc_adapter)
    {
    case _EGACOLOR:
        if (nlines >= 43)
            _set_font(_FONT8);
        else
            _set_80x25();
        break;

    case _VGACOLOR:
        if (nlines > 28)
            _set_font(_FONT8);
        else
            if (nlines > 25)
                _set_font(_FONT14);
            else
                _set_80x25();
    }

    PDC_set_blink(COLORS == 8);

    return OK;
}

void PDC_reset_prog_mode(void)
{
        PDC_LOG(("PDC_reset_prog_mode() - called.\n"));
}

void PDC_reset_shell_mode(void)
{
        PDC_LOG(("PDC_reset_shell_mode() - called.\n"));
}

void PDC_restore_screen_mode(int i)
{
    if (i >= 0 && i <= 2)
    {
        pdc_font = _get_font();
        _set_font(saved_font[i]);

        if (_get_scrn_mode() != saved_scrnmode[i])
            _set_scrn_mode(saved_scrnmode[i]);
    }
}

void PDC_save_screen_mode(int i)
{
    if (i >= 0 && i <= 2)
    {
        saved_font[i] = pdc_font;
        saved_scrnmode[i] = pdc_scrnmode;
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

/* _egapal() - Find the EGA palette value (0-63) for the color (0-15).
   On VGA, this is an index into the DAC. */

static short _egapal(short color)
{
    PDCREGS regs;

    regs.W.ax = 0x1007;
    regs.h.bl = pdc_curstoreal[color];

    PDCINT(0x10, regs);

    return regs.h.bh;
}

bool PDC_can_change_color(void)
{
    return (pdc_adapter == _VGACOLOR);
}

/* These are only valid when pdc_adapter == _VGACOLOR */

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    PDCREGS regs;

    /* Read single DAC register */

    regs.W.ax = 0x1015;
    regs.h.bl = _egapal(color);

    PDCINT(0x10, regs);

    /* Scale and store */

    *red = DIVROUND((unsigned)(regs.h.dh) * 1000, 63);
    *green = DIVROUND((unsigned)(regs.h.ch) * 1000, 63);
    *blue = DIVROUND((unsigned)(regs.h.cl) * 1000, 63);

    return OK;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    PDCREGS regs;

    /* Scale */

    regs.h.dh = DIVROUND((unsigned)red * 63, 1000);
    regs.h.ch = DIVROUND((unsigned)green * 63, 1000);
    regs.h.cl = DIVROUND((unsigned)blue * 63, 1000);

    /* Set single DAC register */

    regs.W.ax = 0x1010;
    regs.W.bx = _egapal(color);

    PDCINT(0x10, regs);

    return OK;
}
