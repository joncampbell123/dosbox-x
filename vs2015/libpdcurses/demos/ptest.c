#include <curses.h>
#include <panel.h>
#include <stdlib.h>

PANEL *p1, *p2, *p3, *p4, *p5;
WINDOW *w4, *w5;

long nap_msec = 1;

char *mod[] =
{
    "test ", "TEST ", "(**) ", "*()* ", "<--> ", "LAST "
};

void pflush(void)
{
    update_panels();
    doupdate();
}

void sizecheck(void)
{
    if (COLS < 70 || LINES < 23)
    {
        printw("A window at least 70x23 is required for this demo");
        refresh();
        napms(5000);
        endwin();
        exit(-1);
    }
}

void backfill(void)
{
    int y, x;

    erase();

    for (y = 0; y < LINES - 1; y++)
        for (x = 0; x < COLS; x++)
            printw("%d", (y + x) % 10);
}

void wait_a_while(long msec)
{
    int c;

    if (msec != 1)
        timeout(msec);

    c = getch();

#ifdef KEY_RESIZE
    if (c == KEY_RESIZE)
    {
# ifdef PDCURSES
        resize_term(0, 0);
# endif
        sizecheck();
        backfill();
    }
    else
#endif
    if (c == 'q')
    {
        endwin();
        exit(1);
    }
}

void saywhat(const char *text)
{
    mvprintw(LINES - 1, 0, "%-20.20s", text);
}

/* mkpanel - alloc a win and panel and associate them */

PANEL *mkpanel(int rows, int cols, int tly, int tlx)
{
    WINDOW *win = newwin(rows, cols, tly, tlx);
    PANEL *pan = (PANEL *)0;

    if (win)
    {
        pan = new_panel(win);

        if (!pan)
            delwin(win);
    }

    return pan;
}

void rmpanel(PANEL *pan)
{
    WINDOW *win = pan->win;

    del_panel(pan);
    delwin(win);
}

void fill_panel(PANEL *pan)
{
    WINDOW *win = pan->win;
    char num = *((char *)pan->user + 1);
    int y, x, maxy, maxx;

    box(win, 0, 0);
    mvwprintw(win, 1, 1, "-pan%c-", num);
    getmaxyx(win, maxy, maxx);

    for (y = 2; y < maxy - 1; y++)
        for (x = 1; x < maxx - 1; x++)
            mvwaddch(win, y, x, num);
}

int main(int argc, char **argv)
{
    int itmp, y;

    if (argc > 1 && atol(argv[1]))
        nap_msec = atol(argv[1]);

#ifdef XCURSES
    Xinitscr(argc, argv);
#else
    initscr();
#endif

    keypad(stdscr, TRUE);
    sizecheck();
    backfill();

    for (y = 0; y < 5; y++)
    {
        p1 = mkpanel(10, 10, 0, 0);
        set_panel_userptr(p1, "p1");

        p2 = mkpanel(14, 14, 5, 5);
        set_panel_userptr(p2, "p2");

        p3 = mkpanel(6, 8, 12, 12);
        set_panel_userptr(p3, "p3");

        p4 = mkpanel(10, 10, 10, 30);
        w4 = panel_window(p4);
        set_panel_userptr(p4, "p4");

        p5 = mkpanel(10, 10, 13, 37);
        w5 = panel_window(p5);
        set_panel_userptr(p5, "p5");

        fill_panel(p1);
        fill_panel(p2);
        fill_panel(p3);
        fill_panel(p4);
        fill_panel(p5);
        hide_panel(p4);
        hide_panel(p5);
        pflush();
        wait_a_while(nap_msec);

        saywhat("h3 s1 s2 s4 s5;");
        move_panel(p1, 0, 0);
        hide_panel(p3);
        show_panel(p1);
        show_panel(p2);
        show_panel(p4);
        show_panel(p5);
        pflush();
        wait_a_while(nap_msec);

        saywhat("s1;");
        show_panel(p1);
        pflush();
        wait_a_while(nap_msec);

        saywhat("s2;");
        show_panel(p2);
        pflush();
        wait_a_while(nap_msec);

        saywhat("m2;");
        move_panel(p2, 10, 10);
        pflush();
        wait_a_while(nap_msec);

        saywhat("s3;");
        show_panel(p3);
        pflush();
        wait_a_while(nap_msec);

        saywhat("m3;");
        move_panel(p3, 5, 5);
        pflush();
        wait_a_while(nap_msec);

        saywhat("b3;");
        bottom_panel(p3);
        pflush();
        wait_a_while(nap_msec);

        saywhat("s4;");
        show_panel(p4);
        pflush();
        wait_a_while(nap_msec);

        saywhat("s5;");
        show_panel(p5);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t3;");
        top_panel(p3);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t1;");
        top_panel(p1);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t2;");
        top_panel(p2);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t3;");
        top_panel(p3);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t4;");
        top_panel(p4);
        pflush();
        wait_a_while(nap_msec);

        for (itmp = 0; itmp < 6; itmp++)
        {
            saywhat("m4;");
            mvwaddstr(w4, 3, 1, mod[itmp]);
            move_panel(p4, 4, itmp * 10);
            mvwaddstr(w5, 4, 1, mod[itmp]);
            pflush();
            wait_a_while(nap_msec);

            saywhat("m5;");
            mvwaddstr(w4, 4, 1, mod[itmp]);
            move_panel(p5, 7, itmp * 10 + 6);
            mvwaddstr(w5, 3, 1, mod[itmp]);
            pflush();
            wait_a_while(nap_msec);
        }

        saywhat("m4;");
        move_panel(p4, 4, itmp * 10);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t5;");
        top_panel(p5);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t2;");
        top_panel(p2);
        pflush();
        wait_a_while(nap_msec);

        saywhat("t1;");
        top_panel(p1);
        pflush();
        wait_a_while(nap_msec);

        saywhat("d2;");
        rmpanel(p2);
        pflush();
        wait_a_while(nap_msec);

        saywhat("h3;");
        hide_panel(p3);
        pflush();
        wait_a_while(nap_msec);

        saywhat("d1;");
        rmpanel(p1);
        pflush();
        wait_a_while(nap_msec);

        saywhat("d4; ");
        rmpanel(p4);
        pflush();
        wait_a_while(nap_msec);

        saywhat("d5; ");
        rmpanel(p5);
        pflush();
        wait_a_while(nap_msec);

        if (nap_msec == 1)
            break;

        nap_msec = 100L;
    }

    endwin();

    return 0;
}   /* end of main */
