/* Public Domain Curses */

#include "pdcx11.h"

/* COLOR_PAIR to attribute encoding table. */

short *xc_atrtab = (short *)NULL;

/* close the physical screen */

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
}

void PDC_scr_free(void)
{
    XCursesExit();

    xc_atrtab = (short *)NULL;
}

/* open the physical screen -- allocate SP, miscellaneous intialization */

int PDC_scr_open(int argc, char **argv)
{
    extern bool sb_started;

    PDC_LOG(("PDC_scr_open() - called\n"));

    if ((XCursesInitscr(argc, argv) == ERR) || !SP)
        return ERR;

    SP->cursrow = SP->curscol = 0;
    SP->orig_attr = FALSE;
    SP->sb_on = sb_started;
    SP->sb_total_y = 0;
    SP->sb_viewport_y = 0;
    SP->sb_cur_y = 0;
    SP->sb_total_x = 0;
    SP->sb_viewport_x = 0;
    SP->sb_cur_x = 0;

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
    PDC_LOG(("PDC_resize_screen() - called. Lines: %d Cols: %d\n",
             nlines, ncols));

    if (nlines || ncols || !SP->resized)
        return ERR;

    shmdt((char *)Xcurscr);
    XCursesInstructAndWait(CURSES_RESIZE);

    if ((shmid_Xcurscr = shmget(shmkey_Xcurscr,
        SP->XcurscrSize + XCURSESSHMMIN, 0700)) < 0)
    {
        perror("Cannot allocate shared memory for curscr");
        kill(xc_otherpid, SIGKILL);
        return ERR;
    }

    XCursesLINES = SP->lines;
    XCursesCOLS = SP->cols;

    PDC_LOG(("%s:shmid_Xcurscr %d shmkey_Xcurscr %d SP->lines %d "
             "SP->cols %d\n", XCLOGMSG, shmid_Xcurscr,
             shmkey_Xcurscr, SP->lines, SP->cols));

    Xcurscr = (unsigned char*)shmat(shmid_Xcurscr, 0, 0);
    xc_atrtab = (short *)(Xcurscr + XCURSCR_ATRTAB_OFF);

    SP->resized = FALSE;

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
}

void PDC_save_screen_mode(int i)
{
}

void PDC_init_pair(short pair, short fg, short bg)
{
    xc_atrtab[pair * 2] = fg;
    xc_atrtab[pair * 2 + 1] = bg;
}

int PDC_pair_content(short pair, short *fg, short *bg)
{
    *fg = xc_atrtab[pair * 2];
    *bg = xc_atrtab[pair * 2 + 1];

    return OK;
}

bool PDC_can_change_color(void)
{
    return TRUE;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    XColor *tmp = (XColor *)(Xcurscr + XCURSCR_XCOLOR_OFF);

    tmp->pixel = color;

    XCursesInstructAndWait(CURSES_GET_COLOR);

    *red = ((double)(tmp->red) * 1000 / 65535) + 0.5;
    *green = ((double)(tmp->green) * 1000 / 65535) + 0.5;
    *blue = ((double)(tmp->blue) * 1000 / 65535) + 0.5;

    return OK;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    XColor *tmp = (XColor *)(Xcurscr + XCURSCR_XCOLOR_OFF);

    tmp->pixel = color;

    tmp->red = ((double)red * 65535 / 1000) + 0.5;
    tmp->green = ((double)green * 65535 / 1000) + 0.5;
    tmp->blue = ((double)blue * 65535 / 1000) + 0.5;

    XCursesInstructAndWait(CURSES_SET_COLOR);

    return OK;
}
