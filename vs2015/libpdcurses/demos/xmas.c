/******************************************************************************/
/* asciixmas                                                                  */
/* December 1989             Larry Bartz           Indianapolis, IN           */
/*                                                                            */
/*                                                                            */
/* I'm dreaming of an ascii character-based monochrome Christmas,             */
/* Just like the one's I used to know!                                        */
/* Via a full duplex communications channel,                                  */
/* At 9600 bits per second,                                                   */
/* Even though it's kinda slow.                                               */
/*                                                                            */
/* I'm dreaming of an ascii character-based monochrome Christmas,             */
/* With ev'ry C program I write!                                              */
/* May your screen be merry and bright!                                       */
/* And may all your Christmases be amber or green,                            */
/* (for reduced eyestrain and improved visibility)!                           */
/*                                                                            */
/*                                                                            */
/*                                                                            */
/* IMPLEMENTATION                                                             */
/*                                                                            */
/* Feel free to modify the defined string FROMWHO to reflect you, your        */
/* organization, your site, whatever.                                         */
/*                                                                            */
/* This looks a lot better if you can turn off your cursor before execution.  */
/* The cursor is distracting but it doesn't really ruin the show.             */
/*                                                                            */
/* At our site, we invoke this for our users just after login and the         */
/* determination of terminal type.                                            */
/*                                                                            */
/*                                                                            */
/* PORTABILITY                                                                */
/*                                                                            */
/* I wrote this using only the very simplest curses functions so that it      */
/* might be the most portable. I was personally able to test on five          */
/* different cpu/UNIX combinations.                                           */
/*                                                                            */
/*                                                                            */
/* COMPILE                                                                    */
/*                                                                            */
/* usually this:                                                              */
/*                                                                            */
/* cc -O xmas.c -lcurses -o xmas -s                                           */
/*                                                                            */
/******************************************************************************/

#include <curses.h>

void lil(WINDOW *);
void midtop(WINDOW *);
void bigtop(WINDOW *);
void bigface(WINDOW *, chtype);
void legs1(WINDOW *);
void legs2(WINDOW *);
void legs3(WINDOW *);
void legs4(WINDOW *);
void initdeer(void);
void boxit(void);
void seas(void);
void greet(void);
void fromwho(void);
void del_msg(void);
void tree(void);
void balls(void);
void star(void);
void strng1(void);
void strng2(void);
void strng3(void);
void strng4(void);
void strng5(void);
void blinkit(void);
void reindeer(void);

#define FROMWHO "From Larry Bartz, Mark Hessling and William McBrine"

int y_pos, x_pos;

WINDOW *treescrn, *treescrn2, *treescrn3, *treescrn4, *treescrn5,
       *treescrn6, *treescrn7, *treescrn8, *dotdeer0, *stardeer0,
       *lildeer0, *lildeer1, *lildeer2, *lildeer3, *middeer0,
       *middeer1, *middeer2, *middeer3, *bigdeer0, *bigdeer1,
       *bigdeer2, *bigdeer3, *bigdeer4, *lookdeer0, *lookdeer1,
       *lookdeer2, *lookdeer3, *lookdeer4, *w_holiday, *w_del_msg;

int main(int argc, char **argv)
{
    int loopy;

#ifdef XCURSES
    Xinitscr(argc, argv);
#else
    initscr();
#endif
    nodelay(stdscr, TRUE);
    noecho();
    nonl();
    refresh();

#ifdef A_COLOR
    if (has_colors())
        start_color();
#endif
    curs_set(0);

    treescrn = newwin(16, 27, 3, 53);
    treescrn2 = newwin(16, 27, 3, 53);
    treescrn3 = newwin(16, 27, 3, 53);
    treescrn4 = newwin(16, 27, 3, 53);
    treescrn5 = newwin(16, 27, 3, 53);
    treescrn6 = newwin(16, 27, 3, 53);
    treescrn7 = newwin(16, 27, 3, 53);
    treescrn8 = newwin(16, 27, 3, 53);

    w_holiday = newwin(1, 26, 3, 27);

    w_del_msg = newwin(1, 12, 23, 60);

    mvwaddstr(w_holiday, 0, 0, "H A P P Y  H O L I D A Y S");

    initdeer();

    clear();
    werase(treescrn);
    touchwin(treescrn);
    werase(treescrn2);
    touchwin(treescrn2);
    werase(treescrn8);
    touchwin(treescrn8);
    refresh();
    napms(1000);

    boxit();
    del_msg();
    napms(1000);

    seas();
    del_msg();
    napms(1000);

    greet();
    del_msg();
    napms(1000);

    fromwho();
    del_msg();
    napms(1000);

    tree();
    napms(1000);

    balls();
    napms(1000);

    star();
    napms(1000);

    strng1();
    strng2();
    strng3();
    strng4();
    strng5();

    /* set up the windows for our blinking trees */
    /* **************************************** */
    /* treescrn3 */

    overlay(treescrn, treescrn3);

    /* balls */
    mvwaddch(treescrn3, 4, 18, ' ');
    mvwaddch(treescrn3, 7, 6, ' ');
    mvwaddch(treescrn3, 8, 19, ' ');
    mvwaddch(treescrn3, 11, 22, ' ');

    /* star */
    mvwaddch(treescrn3, 0, 12, '*');

    /* strng1 */
    mvwaddch(treescrn3, 3, 11, ' ');

    /* strng2 */
    mvwaddch(treescrn3, 5, 13, ' ');
    mvwaddch(treescrn3, 6, 10, ' ');

    /* strng3 */
    mvwaddch(treescrn3, 7, 16, ' ');
    mvwaddch(treescrn3, 7, 14, ' ');

    /* strng4 */
    mvwaddch(treescrn3, 10, 13, ' ');
    mvwaddch(treescrn3, 10, 10, ' ');
    mvwaddch(treescrn3, 11, 8, ' ');

    /* strng5 */
    mvwaddch(treescrn3, 11, 18, ' ');
    mvwaddch(treescrn3, 12, 13, ' ');

    /* treescrn4 */

    overlay(treescrn, treescrn4);

    /* balls */
    mvwaddch(treescrn4, 3, 9, ' ');
    mvwaddch(treescrn4, 4, 16, ' ');
    mvwaddch(treescrn4, 7, 6, ' ');
    mvwaddch(treescrn4, 8, 19, ' ');
    mvwaddch(treescrn4, 11, 2, ' ');
    mvwaddch(treescrn4, 12, 23, ' ');

    /* star */
    mvwaddch(treescrn4, 0, 12, '*' | A_STANDOUT);

    /* strng1 */
    mvwaddch(treescrn4, 3, 13, ' ');

    /* strng2 */

    /* strng3 */
    mvwaddch(treescrn4, 7, 15, ' ');
    mvwaddch(treescrn4, 8, 11, ' ');

    /* strng4 */
    mvwaddch(treescrn4, 9, 16, ' ');
    mvwaddch(treescrn4, 10, 12, ' ');
    mvwaddch(treescrn4, 11, 8, ' ');

    /* strng5 */
    mvwaddch(treescrn4, 11, 18, ' ');
    mvwaddch(treescrn4, 12, 14, ' ');

    /* treescrn5 */

    overlay(treescrn, treescrn5);

    /* balls */
    mvwaddch(treescrn5, 3, 15, ' ');
    mvwaddch(treescrn5, 10, 20, ' ');
    mvwaddch(treescrn5, 12, 1, ' ');

    /* star */
    mvwaddch(treescrn5, 0, 12, '*');

    /* strng1 */
    mvwaddch(treescrn5, 3, 11, ' ');

    /* strng2 */
    mvwaddch(treescrn5, 5, 12, ' ');

    /* strng3 */
    mvwaddch(treescrn5, 7, 14, ' ');
    mvwaddch(treescrn5, 8, 10, ' ');

    /* strng4 */
    mvwaddch(treescrn5, 9, 15, ' ');
    mvwaddch(treescrn5, 10, 11, ' ');
    mvwaddch(treescrn5, 11, 7, ' ');

    /* strng5 */
    mvwaddch(treescrn5, 11, 17, ' ');
    mvwaddch(treescrn5, 12, 13, ' ');

    /* treescrn6 */

    overlay(treescrn, treescrn6);

    /* balls */
    mvwaddch(treescrn6, 6, 7, ' ');
    mvwaddch(treescrn6, 7, 18, ' ');
    mvwaddch(treescrn6, 10, 4, ' ');
    mvwaddch(treescrn6, 11, 23, ' ');

    /* star */
    mvwaddch(treescrn6, 0, 12, '*' | A_STANDOUT);

    /* strng1 */

    /* strng2 */
    mvwaddch(treescrn6, 5, 11, ' ');

    /* strng3 */
    mvwaddch(treescrn6, 7, 13, ' ');
    mvwaddch(treescrn6, 8, 9, ' ');

    /* strng4 */
    mvwaddch(treescrn6, 9, 14, ' ');
    mvwaddch(treescrn6, 10, 10, ' ');
    mvwaddch(treescrn6, 11, 6, ' ');

    /* strng5 */
    mvwaddch(treescrn6, 11, 16, ' ');
    mvwaddch(treescrn6, 12, 12, ' ');

    /* treescrn7 */

    overlay(treescrn, treescrn7);

    /* balls */
    mvwaddch(treescrn7, 3, 15, ' ');
    mvwaddch(treescrn7, 6, 7, ' ');
    mvwaddch(treescrn7, 7, 18, ' ');
    mvwaddch(treescrn7, 10, 4, ' ');
    mvwaddch(treescrn7, 11, 22, ' ');

    /* star */
    mvwaddch(treescrn7, 0, 12, '*');

    /* strng1 */
    mvwaddch(treescrn7, 3, 12, ' ');

    /* strng2 */
    mvwaddch(treescrn7, 5, 13, ' ');
    mvwaddch(treescrn7, 6, 9, ' ');

    /* strng3 */
    mvwaddch(treescrn7, 7, 15, ' ');
    mvwaddch(treescrn7, 8, 11, ' ');

    /* strng4 */
    mvwaddch(treescrn7, 9, 16, ' ');
    mvwaddch(treescrn7, 10, 12, ' ');
    mvwaddch(treescrn7, 11, 8, ' ');

    /* strng5 */
    mvwaddch(treescrn7, 11, 18, ' ');
    mvwaddch(treescrn7, 12, 14, ' ');

    napms(1000);
    reindeer();

    touchwin(w_holiday);
    wrefresh(w_holiday);
    wrefresh(w_del_msg);

    napms(1000);

    for (loopy = 0; loopy < 50; loopy++)
        blinkit();

    clear();
    refresh();
    endwin();

    return 0;
}

void lil(WINDOW *win)
{
    mvwaddch(win, 0, 0, (chtype) 'V');
    mvwaddch(win, 1, 0, (chtype) '@');
    mvwaddch(win, 1, 3, (chtype) '~');
}

void midtop(WINDOW *win)
{
    mvwaddstr(win, 0, 2, "yy");
    mvwaddstr(win, 1, 2, "0(=)~");
}

void bigtop(WINDOW *win)
{
    mvwaddstr(win, 0, 17, "\\/");
    mvwaddstr(win, 0, 20, "\\/");
    mvwaddch(win, 1, 18, (chtype) '\\');
    mvwaddch(win, 1, 20, (chtype) '/');
    mvwaddstr(win, 2, 19, "|_");
    mvwaddstr(win, 3, 18, "/^0\\");
    mvwaddstr(win, 4, 17, "//\\");
    mvwaddch(win, 4, 22, (chtype) '\\');
    mvwaddstr(win, 5, 7, "^~~~~~~~~//  ~~U");
}

void bigface(WINDOW *win, chtype noseattr)
{
    mvwaddstr(win, 0, 16, "\\/     \\/");
    mvwaddstr(win, 1, 17, "\\Y/ \\Y/");
    mvwaddstr(win, 2, 19, "\\=/");
    mvwaddstr(win, 3, 17, "^\\o o/^");
    mvwaddstr(win, 4, 17, "//( )");
    mvwaddstr(win, 5, 7, "^~~~~~~~~// \\");
    waddch(win, 'O' | noseattr);
    waddstr(win, "/");
}

void legs1(WINDOW *win)
{
    mvwaddstr(win, 6, 7, "( \\_____( /");
    mvwaddstr(win, 7, 8, "( )    /");
    mvwaddstr(win, 8, 9, "\\\\   /");
    mvwaddstr(win, 9, 11, "\\>/>");
}

void legs2(WINDOW *win)
{
    mvwaddstr(win, 6, 7, "(( )____( /");
    mvwaddstr(win, 7, 7, "( /      |");
    mvwaddstr(win, 8, 8, "\\/      |");
    mvwaddstr(win, 9, 9, "|>     |>");
}

void legs3(WINDOW *win)
{
    mvwaddstr(win, 6, 6, "( ()_____( /");
    mvwaddstr(win, 7, 6, "/ /       /");
    mvwaddstr(win, 8, 5, "|/          \\");
    mvwaddstr(win, 9, 5, "/>           \\>");
}

void legs4(WINDOW *win)
{
    mvwaddstr(win, 6, 6, "( )______( /");
    mvwaddstr(win, 7, 5, "(/          \\");
    mvwaddstr(win, 8, 0, "v___=             ----^");
}

void initdeer(void)
{
    chtype noseattr;

#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(31, COLOR_RED, COLOR_BLACK);
        noseattr = COLOR_PAIR(31);
    }
    else
#endif
        noseattr = A_NORMAL;

    /* set up the windows for our various reindeer */

    dotdeer0 = newwin(3, 71, 0, 8);
    stardeer0 = newwin(4, 56, 0, 8);
    lildeer0 = newwin(7, 54, 0, 8);
    middeer0 = newwin(15, 42, 0, 8);
    bigdeer0 = newwin(10, 23, 0, 0);
    lookdeer0 = newwin(10, 25, 0, 0);

    /* lildeer1 */
    lildeer1 = newwin(2, 4, 0, 0);
    lil(lildeer1);
    mvwaddstr(lildeer1, 1, 1, "<>");

    /* lildeer2 */
    lildeer2 = newwin(2, 4, 0, 0);
    lil(lildeer2);
    mvwaddstr(lildeer2, 1, 1, "||");

    /* lildeer3 */
    lildeer3 = newwin(2, 4, 0, 0);
    lil(lildeer3);
    mvwaddstr(lildeer3, 1, 1, "><");

    /* middeer1 */
    middeer1 = newwin(3, 7, 0, 0);
    midtop(middeer1);
    mvwaddstr(middeer1, 2, 3, "\\/");

    /* middeer2 */
    middeer2 = newwin(3, 7, 0, 0);
    midtop(middeer2);
    mvwaddch(middeer2, 2, 3, (chtype) '|');
    mvwaddch(middeer2, 2, 5, (chtype) '|');

    /* middeer3 */
    middeer3 = newwin(3, 7, 0, 0);
    midtop(middeer3);
    mvwaddch(middeer3, 2, 2, (chtype) '/');
    mvwaddch(middeer3, 2, 6, (chtype) '\\');

    /* bigdeer1 */
    bigdeer1 = newwin(10, 23, 0, 0);
    bigtop(bigdeer1);
    legs1(bigdeer1);

    /* bigdeer2 */
    bigdeer2 = newwin(10, 23, 0, 0);
    bigtop(bigdeer2);
    legs2(bigdeer2);

    /* bigdeer3 */
    bigdeer3 = newwin(10, 23, 0, 0);
    bigtop(bigdeer3);
    legs3(bigdeer3);

    /* bigdeer4 */
    bigdeer4 = newwin(10, 23, 0, 0);
    bigtop(bigdeer4);
    legs4(bigdeer4);

    /* lookdeer1 */
    lookdeer1 = newwin(10, 25, 0, 0);
    bigface(lookdeer1, noseattr);
    legs1(lookdeer1);

    /* lookdeer2 */
    lookdeer2 = newwin(10, 25, 0, 0);
    bigface(lookdeer2, noseattr);
    legs2(lookdeer2);

    /* lookdeer3 */
    lookdeer3 = newwin(10, 25, 0, 0);
    bigface(lookdeer3, noseattr);
    legs3(lookdeer3);

    /* lookdeer4 */
    lookdeer4 = newwin(10, 25, 0, 0);
    bigface(lookdeer4, noseattr);
    legs4(lookdeer4);
}

void boxit(void)
{
    int x;

    for (x = 0; x < 20; ++x)
        mvaddch(x, 7, '|');

    for (x = 0; x < 80; ++x)
    {
        if (x > 7)
            mvaddch(19, x, '_');

        mvaddch(22, x, '_');
    }
}

void seas(void)
{
    mvaddch(4, 1, 'S');
    mvaddch(6, 1, 'E');
    mvaddch(8, 1, 'A');
    mvaddch(10, 1, 'S');
    mvaddch(12, 1, 'O');
    mvaddch(14, 1, 'N');
    mvaddch(16, 1, '`');
    mvaddch(18, 1, 'S');
}

void greet(void)
{
    mvaddch(3, 5, 'G');
    mvaddch(5, 5, 'R');
    mvaddch(7, 5, 'E');
    mvaddch(9, 5, 'E');
    mvaddch(11, 5, 'T');
    mvaddch(13, 5, 'I');
    mvaddch(15, 5, 'N');
    mvaddch(17, 5, 'G');
    mvaddch(19, 5, 'S');
}

void fromwho(void)
{
    mvaddstr(21, 13, FROMWHO);
}

void del_msg(void)
{
    refresh();
}

void tree(void)
{
#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(30, COLOR_GREEN, COLOR_BLACK);
        wattrset(treescrn, COLOR_PAIR(30));
    }
#endif
    mvwaddch(treescrn, 1, 11, (chtype) '/');
    mvwaddch(treescrn, 2, 11, (chtype) '/');
    mvwaddch(treescrn, 3, 10, (chtype) '/');
    mvwaddch(treescrn, 4, 9, (chtype) '/');
    mvwaddch(treescrn, 5, 9, (chtype) '/');
    mvwaddch(treescrn, 6, 8, (chtype) '/');
    mvwaddch(treescrn, 7, 7, (chtype) '/');
    mvwaddch(treescrn, 8, 6, (chtype) '/');
    mvwaddch(treescrn, 9, 6, (chtype) '/');
    mvwaddch(treescrn, 10, 5, (chtype) '/');
    mvwaddch(treescrn, 11, 3, (chtype) '/');
    mvwaddch(treescrn, 12, 2, (chtype) '/');

    mvwaddch(treescrn, 1, 13, (chtype) '\\');
    mvwaddch(treescrn, 2, 13, (chtype) '\\');
    mvwaddch(treescrn, 3, 14, (chtype) '\\');
    mvwaddch(treescrn, 4, 15, (chtype) '\\');
    mvwaddch(treescrn, 5, 15, (chtype) '\\');
    mvwaddch(treescrn, 6, 16, (chtype) '\\');
    mvwaddch(treescrn, 7, 17, (chtype) '\\');
    mvwaddch(treescrn, 8, 18, (chtype) '\\');
    mvwaddch(treescrn, 9, 18, (chtype) '\\');
    mvwaddch(treescrn, 10, 19, (chtype) '\\');
    mvwaddch(treescrn, 11, 21, (chtype) '\\');
    mvwaddch(treescrn, 12, 22, (chtype) '\\');

    mvwaddch(treescrn, 4, 10, (chtype) '_');
    mvwaddch(treescrn, 4, 14, (chtype) '_');
    mvwaddch(treescrn, 8, 7, (chtype) '_');
    mvwaddch(treescrn, 8, 17, (chtype) '_');

    mvwaddstr(treescrn, 13, 0,
          "//////////// \\\\\\\\\\\\\\\\\\\\\\\\");

#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(20, COLOR_YELLOW, COLOR_BLACK);
        wattrset(treescrn, COLOR_PAIR(20));
    }
#endif
    mvwaddstr(treescrn, 14, 11, "| |");
    mvwaddstr(treescrn, 15, 11, "|_|");

    wrefresh(treescrn);
    wrefresh(w_del_msg);
}

void balls(void)
{
    chtype ball1, ball2, ball3, ball4, ball5, ball6;

    overlay(treescrn, treescrn2);

#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);
        init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(4, COLOR_CYAN, COLOR_BLACK);
        init_pair(5, COLOR_YELLOW, COLOR_BLACK);
        init_pair(6, COLOR_WHITE, COLOR_BLACK);
        ball1 = COLOR_PAIR(1) | '@';
        ball2 = COLOR_PAIR(2) | '@';
        ball3 = COLOR_PAIR(3) | '@';
        ball4 = COLOR_PAIR(4) | '@';
        ball5 = COLOR_PAIR(5) | '@';
        ball6 = COLOR_PAIR(6) | '@';
    }
    else
#endif
        ball1 = ball2 = ball3 = ball4 = ball5 = ball6 = '@';

    mvwaddch(treescrn2, 3, 9, ball1);
    mvwaddch(treescrn2, 3, 15, ball2);
    mvwaddch(treescrn2, 4, 8, ball3);
    mvwaddch(treescrn2, 4, 16, ball4);
    mvwaddch(treescrn2, 5, 7, ball5);
    mvwaddch(treescrn2, 5, 17, ball6);
    mvwaddch(treescrn2, 7, 6, ball1 | A_BOLD);
    mvwaddch(treescrn2, 7, 18, ball2 | A_BOLD);
    mvwaddch(treescrn2, 8, 5, ball3 | A_BOLD);
    mvwaddch(treescrn2, 8, 19, ball4 | A_BOLD);
    mvwaddch(treescrn2, 10, 4, ball5 | A_BOLD);
    mvwaddch(treescrn2, 10, 20, ball6 | A_BOLD);
    mvwaddch(treescrn2, 11, 2, ball1);
    mvwaddch(treescrn2, 11, 22, ball2);
    mvwaddch(treescrn2, 12, 1, ball3);
    mvwaddch(treescrn2, 12, 23, ball4);

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void star(void)
{
    mvwaddch(treescrn2, 0, 12, (chtype) '*' | A_STANDOUT);

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void strng1(void)
{
#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(10, COLOR_YELLOW, COLOR_BLACK);
        wattrset(treescrn2, COLOR_PAIR(10) | A_BOLD);
    }
#endif
    mvwaddstr(treescrn2, 3, 11, ".:'");

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void strng2(void)
{
#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(11, COLOR_RED, COLOR_BLACK);
        wattrset(treescrn2, COLOR_PAIR(11) | A_BOLD);
    }
#endif
    mvwaddstr(treescrn2, 5, 11, ",.:'");
    mvwaddstr(treescrn2, 6, 9, ":'");

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void strng3(void)
{
#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(12, COLOR_GREEN, COLOR_BLACK);
        wattrset(treescrn2, COLOR_PAIR(12) | A_BOLD);
    }
#endif
    mvwaddstr(treescrn2, 7, 13, ",.:'");
    mvwaddstr(treescrn2, 8, 9, ",.:'");

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void strng4(void)
{
#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(13, COLOR_WHITE, COLOR_BLACK);
        wattrset(treescrn2, COLOR_PAIR(13) | A_BOLD);
    }
#endif
    mvwaddstr(treescrn2, 9, 14, ",.:'");
    mvwaddstr(treescrn2, 10, 10, ",.:'");
    mvwaddstr(treescrn2, 11, 6, ",.:'");
    mvwaddch(treescrn2, 12, 5, (chtype) '\'');

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void strng5(void)
{
#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(14, COLOR_CYAN, COLOR_BLACK);
        wattrset(treescrn2, COLOR_PAIR(14) | A_BOLD);
    }
#endif
    mvwaddstr(treescrn2, 11, 16, ",.:'");
    mvwaddstr(treescrn2, 12, 12, ",.:'");

    /* save a fully lit tree */
    overlay(treescrn2, treescrn);

    wrefresh(treescrn2);
    wrefresh(w_del_msg);
}

void blinkit(void)
{
    static int cycle;

    if (cycle > 4)
        cycle = 0;

    touchwin(treescrn8);

    switch (cycle)
    {
    case 0:
        overlay(treescrn3, treescrn8);
        break;

    case 1:
        overlay(treescrn4, treescrn8);
        break;

    case 2:
        overlay(treescrn5, treescrn8);
        break;

    case 3:
        overlay(treescrn6, treescrn8);
        break;

    case 4:
        overlay(treescrn7, treescrn8);
    }

    wrefresh(treescrn8);
    wrefresh(w_del_msg);

    napms(50);
    touchwin(treescrn8);

    /*ALL ON************************************************** */

    overlay(treescrn, treescrn8);
    wrefresh(treescrn8);
    wrefresh(w_del_msg);

    ++cycle;
}

#define TSHOW(win, pause) touchwin(win); wrefresh(win); \
              wrefresh(w_del_msg); napms(pause)

#define SHOW(win, pause) mvwin(win, y_pos, x_pos); wrefresh(win); \
             wrefresh(w_del_msg); napms(pause)

void reindeer(void)
{
    int looper;

    y_pos = 0;

    for (x_pos = 70; x_pos > 62; x_pos--)
    {
        if (x_pos < 62)
            y_pos = 1;

        for (looper = 0; looper < 4; looper++)
        {
            mvwaddch(dotdeer0, y_pos, x_pos, (chtype) '.');
            wrefresh(dotdeer0);
            wrefresh(w_del_msg);
            werase(dotdeer0);
            wrefresh(dotdeer0);
            wrefresh(w_del_msg);
        }
    }

    y_pos = 2;

    for (; x_pos > 50; x_pos--)
    {
        for (looper = 0; looper < 4; looper++)
        {
            if (x_pos < 56)
            {
                y_pos = 3;

                mvwaddch(stardeer0, y_pos, x_pos, (chtype) '*');
                wrefresh(stardeer0);
                wrefresh(w_del_msg);
                werase(stardeer0);
                wrefresh(stardeer0);
            }
            else
            {
                mvwaddch(dotdeer0, y_pos, x_pos, (chtype) '*');
                wrefresh(dotdeer0);
                wrefresh(w_del_msg);
                werase(dotdeer0);
                wrefresh(dotdeer0);
            }
            wrefresh(w_del_msg);
        }
    }

    x_pos = 58;

    for (y_pos = 2; y_pos < 5; y_pos++)
    {
        TSHOW(lildeer0, 50);

        for (looper = 0; looper < 4; looper++)
        {
            SHOW(lildeer3, 50);
            SHOW(lildeer2, 50);
            SHOW(lildeer1, 50);
            SHOW(lildeer2, 50);
            SHOW(lildeer3, 50);

            TSHOW(lildeer0, 50);

            x_pos -= 2;
        }
    }

    x_pos = 35;

    for (y_pos = 5; y_pos < 10; y_pos++)
    {
        touchwin(middeer0);
        wrefresh(middeer0);
        wrefresh(w_del_msg);

        for (looper = 0; looper < 2; looper++)
        {
            SHOW(middeer3, 50);
            SHOW(middeer2, 50);
            SHOW(middeer1, 50);
            SHOW(middeer2, 50);
            SHOW(middeer3, 50);

            TSHOW(middeer0, 50);

            x_pos -= 3;
        }
    }

    napms(2000);

    y_pos = 1;

    for (x_pos = 8; x_pos < 16; x_pos++)
    {
        SHOW(bigdeer4, 30);
        SHOW(bigdeer3, 30);
        SHOW(bigdeer2, 30);
        SHOW(bigdeer1, 30);
        SHOW(bigdeer2, 30);
        SHOW(bigdeer3, 30);
        SHOW(bigdeer4, 30);
        SHOW(bigdeer0, 30);
    }

    --x_pos;

    for (looper = 0; looper < 6; looper++)
    {
        SHOW(lookdeer4, 40);
        SHOW(lookdeer3, 40);
        SHOW(lookdeer2, 40);
        SHOW(lookdeer1, 40);
        SHOW(lookdeer2, 40);
        SHOW(lookdeer3, 40);
        SHOW(lookdeer4, 40);
    }

    SHOW(lookdeer0, 40);

    for (; y_pos < 10; y_pos++)
    {
        for (looper = 0; looper < 2; looper++)
        {
            SHOW(bigdeer4, 30);
            SHOW(bigdeer3, 30);
            SHOW(bigdeer2, 30);
            SHOW(bigdeer1, 30);
            SHOW(bigdeer2, 30);
            SHOW(bigdeer3, 30);
            SHOW(bigdeer4, 30);
        }

        SHOW(bigdeer0, 30);
    }

    --y_pos;

    mvwin(lookdeer3, y_pos, x_pos);
    wrefresh(lookdeer3);
    wrefresh(w_del_msg);
}
