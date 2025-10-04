/********************************* tui.c ************************************/
/*
 * 'textual user interface'
 *
 * Author : P.J. Kunst <kunst@prl.philips.nl>
 * Date   : 25-02-93
 */

#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tui.h"

void statusmsg(char *);
int waitforkey(void);
void rmerror(void);

#if defined(__unix) && !defined(__DJGPP__)
#include <unistd.h>
#endif

#ifdef A_COLOR
# define TITLECOLOR       1       /* color pair indices */
# define MAINMENUCOLOR    (2 | A_BOLD)
# define MAINMENUREVCOLOR (3 | A_BOLD | A_REVERSE)
# define SUBMENUCOLOR     (4 | A_BOLD)
# define SUBMENUREVCOLOR  (5 | A_BOLD | A_REVERSE)
# define BODYCOLOR        6
# define STATUSCOLOR      (7 | A_BOLD)
# define INPUTBOXCOLOR    8
# define EDITBOXCOLOR     (9 | A_BOLD | A_REVERSE)
#else
# define TITLECOLOR       0       /* color pair indices */
# define MAINMENUCOLOR    (A_BOLD)
# define MAINMENUREVCOLOR (A_BOLD | A_REVERSE)
# define SUBMENUCOLOR     (A_BOLD)
# define SUBMENUREVCOLOR  (A_BOLD | A_REVERSE)
# define BODYCOLOR        0
# define STATUSCOLOR      (A_BOLD)
# define INPUTBOXCOLOR    0
# define EDITBOXCOLOR     (A_BOLD | A_REVERSE)
#endif


#define th 1     /* title window height */
#define mh 1     /* main menu height */
#define sh 2     /* status window height */
#define bh (LINES - th - mh - sh)   /* body window height */
#define bw COLS  /* body window width */


/******************************* STATIC ************************************/

static WINDOW *wtitl, *wmain, *wbody, *wstat; /* title, menu, body, status win*/
static int nexty, nextx;
static int key = ERR, ch = ERR;
static bool quit = FALSE;
static bool incurses = FALSE;

#ifndef PDCURSES
static char wordchar(void)
{
    return 0x17;    /* ^W */
}
#endif

static char *padstr(char *s, int length)
{
    static char buf[MAXSTRLEN];
    char fmt[10];

    sprintf(fmt, (int)strlen(s) > length ? "%%.%ds" : "%%-%ds", length);
    sprintf(buf, fmt, s);

    return buf;
}

static char *prepad(char *s, int length)
{
    int i;
    char *p = s;

    if (length > 0)
    {
        memmove((void *)(s + length), (const void *)s, strlen(s) + 1);

        for (i = 0; i < length; i++)
            *p++ = ' ';
    }

    return s;
}

static void rmline(WINDOW *win, int nr)   /* keeps box lines intact */
{
    mvwaddstr(win, nr, 1, padstr(" ", bw - 2));
    wrefresh(win);
}

static void initcolor(void)
{
#ifdef A_COLOR
    if (has_colors())
        start_color();

    /* foreground, background */

    init_pair(TITLECOLOR       & ~A_ATTR, COLOR_BLACK, COLOR_CYAN);
    init_pair(MAINMENUCOLOR    & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);
    init_pair(MAINMENUREVCOLOR & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
    init_pair(SUBMENUCOLOR     & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);
    init_pair(SUBMENUREVCOLOR  & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
    init_pair(BODYCOLOR        & ~A_ATTR, COLOR_WHITE, COLOR_BLUE);
    init_pair(STATUSCOLOR      & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);
    init_pair(INPUTBOXCOLOR    & ~A_ATTR, COLOR_BLACK, COLOR_CYAN);
    init_pair(EDITBOXCOLOR     & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
#endif
}

static void setcolor(WINDOW *win, chtype color)
{
    chtype attr = color & A_ATTR;  /* extract Bold, Reverse, Blink bits */

#ifdef A_COLOR
    attr &= ~A_REVERSE;  /* ignore reverse, use colors instead! */
    wattrset(win, COLOR_PAIR(color & A_CHARTEXT) | attr);
#else
    attr &= ~A_BOLD;     /* ignore bold, gives messy display on HP-UX */
    wattrset(win, attr);
#endif
}

static void colorbox(WINDOW *win, chtype color, int hasbox)
{
    int maxy;
#ifndef PDCURSES
    int maxx;
#endif
    chtype attr = color & A_ATTR;  /* extract Bold, Reverse, Blink bits */

    setcolor(win, color);

#ifdef A_COLOR
    if (has_colors())
        wbkgd(win, COLOR_PAIR(color & A_CHARTEXT) | (attr & ~A_REVERSE));
    else
#endif
        wbkgd(win, attr);

    werase(win);

#ifdef PDCURSES
    maxy = getmaxy(win);
#else
    getmaxyx(win, maxy, maxx);
#endif
    if (hasbox && (maxy > 2))
        box(win, 0, 0);

    touchwin(win);
    wrefresh(win);
}

static void idle(void)
{
    char buf[MAXSTRLEN];
    time_t t;
    struct tm *tp;

    if (time (&t) == -1)
        return;  /* time not available */

    tp = localtime(&t);
    sprintf(buf, " %.2d-%.2d-%.4d  %.2d:%.2d:%.2d",
            tp->tm_mday, tp->tm_mon + 1, tp->tm_year + 1900,
            tp->tm_hour, tp->tm_min, tp->tm_sec);

    mvwaddstr(wtitl, 0, bw - strlen(buf) - 2, buf);
    wrefresh(wtitl);
}

static void menudim(menu *mp, int *lines, int *columns)
{
    int n, l, mmax = 0;

    for (n = 0; mp->func; n++, mp++)
        if ((l = strlen(mp->name)) > mmax) mmax = l;

    *lines = n;
    *columns = mmax + 2;
}

static void setmenupos(int y, int x)
{
    nexty = y;
    nextx = x;
}

static void getmenupos(int *y, int *x)
{
    *y = nexty;
    *x = nextx;
}

static int hotkey(const char *s)
{
    int c0 = *s;    /* if no upper case found, return first char */

    for (; *s; s++)
        if (isupper((unsigned char)*s))
            break;

    return *s ? *s : c0;
}

static void repaintmenu(WINDOW *wmenu, menu *mp)
{
    int i;
    menu *p = mp;

    for (i = 0; p->func; i++, p++)
        mvwaddstr(wmenu, i + 1, 2, p->name);

    touchwin(wmenu);
    wrefresh(wmenu);
}

static void repaintmainmenu(int width, menu *mp)
{
    int i;
    menu *p = mp;

    for (i = 0; p->func; i++, p++)
        mvwaddstr(wmain, 0, i * width, prepad(padstr(p->name, width - 1), 1));

    touchwin(wmain);
    wrefresh(wmain);
}

static void mainhelp(void)
{
#ifdef ALT_X
    statusmsg("Use arrow keys and Enter to select (Alt-X to quit)");
#else
    statusmsg("Use arrow keys and Enter to select");
#endif
}

static void mainmenu(menu *mp)
{
    int nitems, barlen, old = -1, cur = 0, c, cur0;

    menudim(mp, &nitems, &barlen);
    repaintmainmenu(barlen, mp);

    while (!quit)
    {
        if (cur != old)
        {
            if (old != -1)
            {
                mvwaddstr(wmain, 0, old * barlen,
                          prepad(padstr(mp[old].name, barlen - 1), 1));

                statusmsg(mp[cur].desc);
            }
            else
                mainhelp();

            setcolor(wmain, MAINMENUREVCOLOR);

            mvwaddstr(wmain, 0, cur * barlen,
                      prepad(padstr(mp[cur].name, barlen - 1), 1));

            setcolor(wmain, MAINMENUCOLOR);
            old = cur;
            wrefresh(wmain);
        }

        switch (c = (key != ERR ? key : waitforkey()))
        {
        case KEY_DOWN:
        case '\n':              /* menu item selected */
            touchwin(wbody);
            wrefresh(wbody);
            rmerror();
            setmenupos(th + mh, cur * barlen);
            curs_set(1);
            (mp[cur].func)();   /* perform function */
            curs_set(0);

            switch (key)
            {
            case KEY_LEFT:
                cur = (cur + nitems - 1) % nitems;
                key = '\n';
                break;

            case KEY_RIGHT:
                cur = (cur + 1) % nitems;
                key = '\n';
                break;

            default:
                key = ERR;
            }

            repaintmainmenu(barlen, mp);
            old = -1;
            break;

        case KEY_LEFT:
            cur = (cur + nitems - 1) % nitems;
            break;

        case KEY_RIGHT:
            cur = (cur + 1) % nitems;
            break;

        case KEY_ESC:
            mainhelp();
            break;

        default:
            cur0 = cur;

            do
            {
                cur = (cur + 1) % nitems;

            } while ((cur != cur0) && (hotkey(mp[cur].name) != toupper(c)));

            if (hotkey(mp[cur].name) == toupper(c))
                key = '\n';
        }

    }

    rmerror();
    touchwin(wbody);
    wrefresh(wbody);
}

static void cleanup(void)   /* cleanup curses settings */
{
    if (incurses)
    {
        delwin(wtitl);
        delwin(wmain);
        delwin(wbody);
        delwin(wstat);
        curs_set(1);
        endwin();
        incurses = FALSE;
    }
}


/******************************* EXTERNAL **********************************/

void clsbody(void)
{
    werase(wbody);
    wmove(wbody, 0, 0);
}

int bodylen(void)
{
#ifdef PDCURSES
    return getmaxy(wbody);
#else
    int maxy, maxx;

    getmaxyx(wbody, maxy, maxx);
    return maxy;
#endif
}

WINDOW *bodywin(void)
{
    return wbody;
}

void rmerror(void)
{
    rmline(wstat, 0);
}

void rmstatus(void)
{
    rmline(wstat, 1);
}

void titlemsg(char *msg)
{
    mvwaddstr(wtitl, 0, 2, padstr(msg, bw - 3));
    wrefresh(wtitl);
}

void bodymsg(char *msg)
{
    waddstr(wbody, msg);
    wrefresh(wbody);
}

void errormsg(char *msg)
{
    beep();
    mvwaddstr(wstat, 0, 2, padstr(msg, bw - 3));
    wrefresh(wstat);
}

void statusmsg(char *msg)
{
    mvwaddstr(wstat, 1, 2, padstr(msg, bw - 3));
    wrefresh(wstat);
}

bool keypressed(void)
{
    ch = wgetch(wbody);

    return ch != ERR;
}

int getkey(void)
{
    int c = ch;

    ch = ERR;
#ifdef ALT_X
    quit = (c == ALT_X);    /* PC only ! */
#endif
    return c;
}

int waitforkey(void)
{
    do idle(); while (!keypressed());
    return getkey();
}

void DoExit(void)   /* terminate program */
{
    quit = TRUE;
}

void domenu(menu *mp)
{
    int y, x, nitems, barlen, mheight, mw, old = -1, cur = 0, cur0;
    bool stop = FALSE;
    WINDOW *wmenu;

    curs_set(0);
    getmenupos(&y, &x);
    menudim(mp, &nitems, &barlen);
    mheight = nitems + 2;
    mw = barlen + 2;
    wmenu = newwin(mheight, mw, y, x);
    colorbox(wmenu, SUBMENUCOLOR, 1);
    repaintmenu(wmenu, mp);

    key = ERR;

    while (!stop && !quit)
    {
        if (cur != old)
        {
            if (old != -1)
                mvwaddstr(wmenu, old + 1, 1,
                          prepad(padstr(mp[old].name, barlen - 1), 1));

            setcolor(wmenu, SUBMENUREVCOLOR);
            mvwaddstr(wmenu, cur + 1, 1,
                      prepad(padstr(mp[cur].name, barlen - 1), 1));

            setcolor(wmenu, SUBMENUCOLOR);
            statusmsg(mp[cur].desc);

            old = cur;
            wrefresh(wmenu);
        }

        switch (key = ((key != ERR) ? key : waitforkey()))
        {
        case '\n':          /* menu item selected */
            touchwin(wbody);
            wrefresh(wbody);
            setmenupos(y + 1, x + 1);
            rmerror();

            key = ERR;
            curs_set(1);
            (mp[cur].func)();   /* perform function */
            curs_set(0);

            repaintmenu(wmenu, mp);

            old = -1;
            break;

        case KEY_UP:
            cur = (cur + nitems - 1) % nitems;
            key = ERR;
            break;

        case KEY_DOWN:
            cur = (cur + 1) % nitems;
            key = ERR;
            break;

        case KEY_ESC:
        case KEY_LEFT:
        case KEY_RIGHT:
            if (key == KEY_ESC)
                key = ERR;  /* return to prev submenu */

            stop = TRUE;
            break;

        default:
            cur0 = cur;

            do
            {
                cur = (cur + 1) % nitems;

            } while ((cur != cur0) &&
                     (hotkey(mp[cur].name) != toupper((int)key)));

            key = (hotkey(mp[cur].name) == toupper((int)key)) ? '\n' : ERR;
        }

    }

    rmerror();
    delwin(wmenu);
    touchwin(wbody);
    wrefresh(wbody);
}

void startmenu(menu *mp, char *mtitle)
{
    initscr();
    incurses = TRUE;
    initcolor();

    wtitl = subwin(stdscr, th, bw, 0, 0);
    wmain = subwin(stdscr, mh, bw, th, 0);
    wbody = subwin(stdscr, bh, bw, th + mh, 0);
    wstat = subwin(stdscr, sh, bw, th + mh + bh, 0);

    colorbox(wtitl, TITLECOLOR, 0);
    colorbox(wmain, MAINMENUCOLOR, 0);
    colorbox(wbody, BODYCOLOR, 0);
    colorbox(wstat, STATUSCOLOR, 0);

    if (mtitle)
        titlemsg(mtitle);

    cbreak();              /* direct input (no newline required)... */
    noecho();              /* ... without echoing */
    curs_set(0);           /* hide cursor (if possible) */
    nodelay(wbody, TRUE);  /* don't wait for input... */
    halfdelay(10);         /* ...well, no more than a second, anyway */
    keypad(wbody, TRUE);   /* enable cursor keys */
    scrollok(wbody, TRUE); /* enable scrolling in main window */

    leaveok(stdscr, TRUE);
    leaveok(wtitl, TRUE);
    leaveok(wmain, TRUE);
    leaveok(wstat, TRUE);

    mainmenu(mp);

    cleanup();
}

static void repainteditbox(WINDOW *win, int x, char *buf)
{
#ifndef PDCURSES
    int maxy;
#endif
    int maxx;

#ifdef PDCURSES
    maxx = getmaxx(win);
#else
    getmaxyx(win, maxy, maxx);
#endif
    werase(win);
    mvwprintw(win, 0, 0, "%s", padstr(buf, maxx));
    wmove(win, 0, x);
    wrefresh(win);
}

/*

  weditstr()     - edit string

  Description:
    The initial value of 'str' with a maximum length of 'field' - 1,
    which is supplied by the calling routine, is editted. The user's
    erase (^H), kill (^U) and delete word (^W) chars are interpreted.
    The PC insert or Tab keys toggle between insert and edit mode.
    Escape aborts the edit session, leaving 'str' unchanged.
    Enter, Up or Down Arrow are used to accept the changes to 'str'.
    NOTE: editstr(), mveditstr(), and mvweditstr() are macros.

  Return Value:
    Returns the input terminating character on success (Escape,
    Enter, Up or Down Arrow) and ERR on error.

  Errors:
    It is an error to call this function with a NULL window pointer.
    The length of the initial 'str' must not exceed 'field' - 1.

*/

int weditstr(WINDOW *win, char *buf, int field)
{
    char org[MAXSTRLEN], *tp, *bp = buf;
    bool defdisp = TRUE, stop = FALSE, insert = FALSE;
    int cury, curx, begy, begx;
    chtype oldattr;
    WINDOW *wedit;
    int c = 0;

    if ((field >= MAXSTRLEN) || (buf == NULL) ||
        ((int)strlen(buf) > field - 1))
        return ERR;

    strcpy(org, buf);   /* save original */

    wrefresh(win);
    getyx(win, cury, curx);
    getbegyx(win, begy, begx);

    wedit = subwin(win, 1, field, begy + cury, begx + curx);
    oldattr = getattrs(wedit);
    colorbox(wedit, EDITBOXCOLOR, 0);

    keypad(wedit, TRUE);
    curs_set(1);

    while (!stop)
    {
        idle();
        repainteditbox(wedit, bp - buf, buf);

        switch (c = wgetch(wedit))
        {
        case ERR:
            break;

        case KEY_ESC:
            strcpy(buf, org);   /* restore original */
            stop = TRUE;
            break;

        case '\n':
        case KEY_UP:
        case KEY_DOWN:
            stop = TRUE;
            break;

        case KEY_LEFT:
            if (bp > buf)
                bp--;
            break;

        case KEY_RIGHT:
            defdisp = FALSE;
            if (bp - buf < (int)strlen(buf))
                bp++;
            break;

        case '\t':            /* TAB -- because insert
                                  is broken on HPUX */
        case KEY_IC:          /* enter insert mode */
        case KEY_EIC:         /* exit insert mode */
            defdisp = FALSE;
            insert = !insert;

            curs_set(insert ? 2 : 1);
            break;

        default:
            if (c == erasechar())       /* backspace, ^H */
            {
                if (bp > buf)
                {
                    memmove((void *)(bp - 1), (const void *)bp, strlen(bp) + 1);
                    bp--;
                }
            }
            else if (c == killchar())   /* ^U */
            {
                bp = buf;
                *bp = '\0';
            }
            else if (c == wordchar())   /* ^W */
            {
                tp = bp;

                while ((bp > buf) && (*(bp - 1) == ' '))
                    bp--;
                while ((bp > buf) && (*(bp - 1) != ' '))
                    bp--;

                memmove((void *)bp, (const void *)tp, strlen(tp) + 1);
            }
            else if (isprint(c))
            {
                if (defdisp)
                {
                    bp = buf;
                    *bp = '\0';
                    defdisp = FALSE;
                }

                if (insert)
                {
                    if ((int)strlen(buf) < field - 1)
                    {
                        memmove((void *)(bp + 1), (const void *)bp,
                                strlen(bp) + 1);

                        *bp++ = c;
                    }
                }
                else if (bp - buf < field - 1)
                {
                    /* append new string terminator */

                    if (!*bp)
                        bp[1] = '\0';

                    *bp++ = c;
                }
            }
        }
    }

    curs_set(0);

    wattrset(wedit, oldattr);
    repainteditbox(wedit, bp - buf, buf);
    delwin(wedit);

    return c;
}

WINDOW *winputbox(WINDOW *win, int nlines, int ncols)
{
    WINDOW *winp;
    int cury, curx, begy, begx;

    getyx(win, cury, curx);
    getbegyx(win, begy, begx);

    winp = newwin(nlines, ncols, begy + cury, begx + curx);
    colorbox(winp, INPUTBOXCOLOR, 1);

    return winp;
}

int getstrings(char *desc[], char *buf[], int field)
{
    WINDOW *winput;
    int oldy, oldx, maxy, maxx, nlines, ncols, i, n, l, mmax = 0;
    int c = 0;
    bool stop = FALSE;

    for (n = 0; desc[n]; n++)
        if ((l = strlen(desc[n])) > mmax)
            mmax = l;

    nlines = n + 2; ncols = mmax + field + 4;
    getyx(wbody, oldy, oldx);
    getmaxyx(wbody, maxy, maxx);

    winput = mvwinputbox(wbody, (maxy - nlines) / 2, (maxx - ncols) / 2,
        nlines, ncols);

    for (i = 0; i < n; i++)
        mvwprintw(winput, i + 1, 2, "%s", desc[i]);

    i = 0;

    while (!stop)
    {
        switch (c = mvweditstr(winput, i+1, mmax+3, buf[i], field))
        {
        case KEY_ESC:
            stop = TRUE;
            break;

        case KEY_UP:
            i = (i + n - 1) % n;
            break;

        case '\n':
        case '\t':
        case KEY_DOWN:
            if (++i == n)
                stop = TRUE;    /* all passed? */
        }
    }

    delwin(winput);
    touchwin(wbody);
    wmove(wbody, oldy, oldx);
    wrefresh(wbody);

    return c;
}
