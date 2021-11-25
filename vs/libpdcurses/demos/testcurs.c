/*
 * This is a test program for PDCurses. Originally by
 * John Burnell <johnb@kea.am.dsir.govt.nz>
 *
 *  wrs(5/28/93) -- modified to be consistent (perform identically)
 *                  with either PDCurses or under Unix System V, R4
 */

#ifndef _XOPEN_SOURCE_EXTENDED
# define _XOPEN_SOURCE_EXTENDED 1
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <curses.h>

#ifdef WACS_S1
# define HAVE_WIDE 1
#else
# define HAVE_WIDE 0
#endif

#include <locale.h>

#if HAVE_WIDE
# include <wchar.h>
#endif

#if defined(PDCURSES) && !defined(XCURSES)
# define HAVE_RESIZE 1
#else
# define HAVE_RESIZE 0
#endif

#ifdef A_COLOR
# define HAVE_COLOR 1
#else
# define HAVE_COLOR 0
#endif

/* Set to non-zero if you want to test the PDCurses clipboard */

#define HAVE_CLIPBOARD 0

void inputTest(WINDOW *);
void scrollTest(WINDOW *);
void introTest(WINDOW *);
int initTest(WINDOW **, int, char **);
void outputTest(WINDOW *);
void padTest(WINDOW *);
void acsTest(WINDOW *);
void attrTest(WINDOW *);

#if HAVE_COLOR
void colorTest(WINDOW *);
#endif

#if HAVE_RESIZE
void resizeTest(WINDOW *);
#endif

#if HAVE_CLIPBOARD
void clipboardTest(WINDOW *);
#endif

#if HAVE_WIDE
void wideTest(WINDOW *);
#endif

void display_menu(int, int);

struct commands
{
    const char *text;
    void (*function)(WINDOW *);
};

typedef struct commands COMMAND;

#define MAX_OPTIONS (7 + HAVE_COLOR + HAVE_RESIZE + HAVE_CLIPBOARD + HAVE_WIDE)

COMMAND command[MAX_OPTIONS] =
{
    {"Intro Test", introTest},
    {"Pad Test", padTest},
#if HAVE_RESIZE
    {"Resize Test", resizeTest},
#endif
    {"Scroll Test", scrollTest},
    {"Input Test", inputTest},
    {"Output Test", outputTest},
    {"ACS Test", acsTest},
    {"Attrib Test", attrTest},
#if HAVE_COLOR
    {"Color Test", colorTest},
#endif
#if HAVE_CLIPBOARD
    {"Clipboard Test", clipboardTest},
#endif
#if HAVE_WIDE
    {"Wide Input", wideTest}
#endif
};

int width, height;

int main(int argc, char *argv[])
{
    WINDOW *win;
    int key, old_option = -1, new_option = 0;
    bool quit = FALSE;

    setlocale(LC_ALL, "");

    if (initTest(&win, argc, argv))
        return 1;

#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        wbkgd(win, COLOR_PAIR(1));
    }
    else
#endif
        wbkgd(win, A_REVERSE);

    erase();
    display_menu(old_option, new_option);

    while (1)
    {
        noecho();
        keypad(stdscr, TRUE);
        raw();

        key = getch();

        switch(key)
        {
        case 10:
        case 13:
        case KEY_ENTER:
            old_option = -1;
            erase();
            refresh();
            (*command[new_option].function)(win);
            erase();
            display_menu(old_option, new_option);
            break;

        case KEY_PPAGE:
        case KEY_HOME:
            old_option = new_option;
            new_option = 0;
            display_menu(old_option, new_option);
            break;

        case KEY_NPAGE:
        case KEY_END:
            old_option = new_option;
            new_option = MAX_OPTIONS - 1;
            display_menu(old_option, new_option);
            break;

        case KEY_UP:
            old_option = new_option;
            new_option = (new_option == 0) ?
                new_option : new_option - 1;
            display_menu(old_option, new_option);
            break;

        case KEY_DOWN:
            old_option = new_option;
            new_option = (new_option == MAX_OPTIONS - 1) ?
                new_option : new_option + 1;
            display_menu(old_option, new_option);
            break;
#ifdef KEY_RESIZE
        case KEY_RESIZE:
# ifdef PDCURSES
            resize_term(0, 0);
# endif
            old_option = -1;
            erase();
            display_menu(old_option, new_option);
            break;
#endif
        case 'Q':
        case 'q':
            quit = TRUE;
        }

        if (quit == TRUE)
            break;
    }

    delwin(win);
    endwin();

    return 0;
}

void Continue(WINDOW *win)
{
    mvwaddstr(win, 10, 1, " Press any key to continue");
    wrefresh(win);
    raw();
    wgetch(win);
}

void Continue2(void)
{
    move(LINES - 1, 1);
    clrtoeol();
    mvaddstr(LINES - 2, 1, " Press any key to continue");
    refresh();
    raw();
    getch();
}

int initTest(WINDOW **win, int argc, char *argv[])
{
#ifdef XCURSES
    Xinitscr(argc, argv);
#else
    initscr();
#endif
#ifdef A_COLOR
    if (has_colors())
        start_color();
#endif
    /* Create a drawing window */

    width  = 60;
    height = 13;

    *win = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);

    if (*win == NULL)
    {
        endwin();
        return 1;
    }

    return 0;
}

void introTest(WINDOW *win)
{
    werase(win);
    wmove(win, height / 2 - 5, width / 2);
    wvline(win, ACS_VLINE, 10);
    wmove(win, height / 2, width / 2 - 10);
    whline(win, ACS_HLINE, 20);
    Continue(win);

    beep();
    werase(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);

    cbreak();
    mvwaddstr(win, 1, 1,
        "You should have a rectangle in the middle of the screen");
    mvwaddstr(win, 2, 1, "You should have heard a beep");
    Continue(win);

    flash();
    mvwaddstr(win, 3, 1, "You should have seen a flash");
    Continue(win);
}

void scrollTest(WINDOW *win)
{
    int i, OldY;
#ifndef PDCURSES
    int OldX;
#endif
    werase(win);
    mvwaddstr(win, height - 2, 1, "The window will now scroll slowly");
    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);
    scrollok(win, TRUE);
    napms(500);

    for (i = 1; i <= height; i++)
    {
        napms(150);
        scroll(win);
        wrefresh(win);
    };

#ifdef PDCURSES
    OldY = getmaxy(win);
#else
    getmaxyx(win, OldY, OldX);
#endif
    mvwaddstr(win, 6, 1, "The top of the window will scroll");
    wmove(win, 1, 1);
    wsetscrreg(win, 0, 4);
    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);

    for (i = 1; i <= 5; i++)
    {
        napms(500);
        scroll(win);
        wrefresh(win);
    }

    mvwaddstr(win, 3, 1, "The bottom of the window will scroll");
    wmove(win, 8, 1);
    wsetscrreg(win, 5, --OldY);
    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);

    for (i = 5; i <= OldY; i++)
    {
        napms(300);
        wscrl(win, -1);
        wrefresh(win);
    }

    wsetscrreg(win, 0, OldY);
}

void inputTest(WINDOW *win)
{
    int w, h, bx, by, sw, sh, i, c, num = 0;
    char buffer[80];
    WINDOW *subWin;
    static const char spinner[4] = "/-\\|";
    int spinner_count = 0;

    wclear(win);

    getmaxyx(win, h, w);
    getbegyx(win, by, bx);

    sw = w / 3;
    sh = h / 3;

    if ((subWin = subwin(win, sh, sw, by + h - sh - 2, bx + w - sw - 2))
        == NULL)
        return;

#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(2, COLOR_WHITE, COLOR_RED);
        wbkgd(subWin, COLOR_PAIR(2) | A_BOLD);
    }
    else
#endif
        wbkgd(subWin, A_BOLD);

    box(subWin, ACS_VLINE, ACS_HLINE);
    wrefresh(win);

    nocbreak();

    wclear (win);
    mvwaddstr(win, 1, 1,
        "Press keys (or mouse buttons) to show their names");
    mvwaddstr(win, 2, 1, "Press spacebar to finish");
    wrefresh(win);

    keypad(win, TRUE);
    raw();
    noecho();

    wtimeout(win, 200);

#ifdef PDCURSES
    mouse_set(ALL_MOUSE_EVENTS);
    PDC_save_key_modifiers(TRUE);
    PDC_return_key_modifiers(TRUE);
#endif
    curs_set(0);        /* turn cursor off */

    while (1)
    {
        while (1)
        {
            c = wgetch(win);

            if (c == ERR)
            {
                spinner_count++;
                if (spinner_count == 4)
                    spinner_count = 0;
                mvwaddch(win, 3, 3, spinner[spinner_count]);
                wrefresh(win);
            }
            else
                break;
        }
#ifdef PDCURSES
        wmove(win, 4, 18);
        wclrtoeol(win);
#endif
        mvwaddstr(win, 3, 5, "Key Pressed: ");
        wclrtoeol(win);

        if (c >= KEY_MIN)
            wprintw(win, "%s", keyname(c));
        else if (isprint(c))
            wprintw(win, "%c", c);
        else
            wprintw(win, "%s", unctrl(c));
#ifdef PDCURSES
        if (c == KEY_MOUSE)
        {
            int button = 0;
            request_mouse_pos();

            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;

            if (button && (BUTTON_STATUS(button) &
                BUTTON_MODIFIER_MASK))
            {
                waddstr(win, " Modifier(s):");

                if (BUTTON_STATUS(button) & BUTTON_SHIFT)
                    waddstr(win, " SHIFT");

                if (BUTTON_STATUS(button) & BUTTON_CONTROL)
                    waddstr(win, " CONTROL");

                if (BUTTON_STATUS(button) & BUTTON_ALT)
                    waddstr(win, " ALT");
            }

            wmove(win, 4, 18);
            wclrtoeol(win);
            wprintw(win, "Button %d: ", button);

            if (MOUSE_MOVED)
                waddstr(win, "moved: ");
            else if (MOUSE_WHEEL_UP)
                waddstr(win, "wheel up: ");
            else if (MOUSE_WHEEL_DOWN)
                waddstr(win, "wheel dn: ");
            else if (MOUSE_WHEEL_LEFT)
                waddstr(win, "wheel lt: ");
            else if (MOUSE_WHEEL_RIGHT)
                waddstr(win, "wheel rt: ");
            else if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_PRESSED)
                waddstr(win, "pressed: ");
            else if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_CLICKED)
                waddstr(win, "clicked: ");
            else if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_DOUBLE_CLICKED)
                waddstr(win, "double: ");
            else
                waddstr(win, "released: ");

            wprintw(win, "Position: Y: %d X: %d", MOUSE_Y_POS, MOUSE_X_POS);
        }
        else if (PDC_get_key_modifiers())
        {
            waddstr(win, " Modifier(s):");
            if (PDC_get_key_modifiers() & PDC_KEY_MODIFIER_SHIFT)
                waddstr(win, " SHIFT");

            if (PDC_get_key_modifiers() & PDC_KEY_MODIFIER_CONTROL)
                waddstr(win, " CONTROL");

            if (PDC_get_key_modifiers() & PDC_KEY_MODIFIER_ALT)
                waddstr(win, " ALT");

            if (PDC_get_key_modifiers() & PDC_KEY_MODIFIER_NUMLOCK)
                waddstr(win, " NUMLOCK");
        }
#endif
        wrefresh(win);

        if (c == ' ')
            break;
    }

    wtimeout(win, -1);  /* turn off timeout() */
    curs_set(1);        /* turn cursor back on */

#ifdef PDCURSES
    mouse_set(0L);
    PDC_save_key_modifiers(FALSE);
    PDC_return_key_modifiers(FALSE);
#endif
    wclear(win);
    mvwaddstr(win, 2, 1, "Press some keys for 5 seconds");
    mvwaddstr(win, 1, 1, "Pressing ^C should do nothing");
    wrefresh(win);

    werase(subWin);
    box(subWin, ACS_VLINE, ACS_HLINE);

    for (i = 0; i < 5; i++)
    {
        mvwprintw(subWin, 1, 1, "Time = %d", i);
        wrefresh(subWin);
        napms(1000);
        flushinp();
    }

    delwin(subWin);
    werase(win);
    flash();
    wrefresh(win);
    napms(500);
    flushinp();

    mvwaddstr(win, 2, 1, "Press a key, followed by ENTER");
    wmove(win, 9, 10);
    wrefresh(win);
    echo();

    keypad(win, TRUE);
    raw();
    wgetnstr(win, buffer, 3);
    flushinp();

    wmove(win, 9, 10);
    wdelch(win);
    mvwaddstr(win, 4, 1, "The character should now have been deleted");
    Continue(win);

    refresh();
    wclear(win);
    echo();
    buffer[0] = '\0';
    mvwaddstr(win, 3, 2, "The window should have moved");
    mvwaddstr(win, 4, 2,
              "This text should have appeared without you pressing a key");
    mvwaddstr(win, 6, 2, "Enter a number then a string seperated by space");
    mvwin(win, 2, 1);
    wrefresh(win);
    mvwscanw(win, 7, 6, "%d %s", &num, buffer);
    mvwprintw(win, 8, 6, "String: %s Number: %d", buffer, num);
    Continue(win);

    refresh();
    wclear(win);
    echo();
    mvwaddstr(win, 3, 2, "Enter a 5 character string: ");
    wgetnstr(win, buffer, 5);
    mvwprintw(win, 4, 2, "String: %s", buffer);
    Continue(win);
}

void outputTest(WINDOW *win)
{
    WINDOW *win1;
    char Buffer[80];
    chtype ch;
    int by, bx;

#ifdef PDCURSES
    PDC_set_blink(TRUE);
#endif
    nl();
    wclear(win);
    mvwaddstr(win, 1, 1, "You should now have a screen in the upper "
                         "left corner, and this text should have wrapped");
    waddstr(win,"\nThis text should be down\n");
    waddstr(win,  "and broken into two here ^");
    Continue(win);

    wclear(win);
    wattron(win, A_BOLD);
    mvwaddstr(win, 1, 1, "A new window will appear with this text in it");
    mvwaddstr(win, 8, 1, "Press any key to continue");
    wrefresh(win);
    wgetch(win);

    getbegyx(win, by, bx);

    if (LINES < 24 || COLS < 75)
    {
        mvwaddstr(win, 5, 1, "Some tests have been skipped as they require a");
        mvwaddstr(win, 6, 1, "display of at least 24 LINES by 75 COLUMNS");
        Continue(win);
    }
    else
    {
        win1 = newwin(10, 50, 14, 25);

        if (win1 == NULL)
        {
            endwin();
            return;
        }

#ifdef A_COLOR
        if (has_colors())
        {
            init_pair(3, COLOR_BLUE, COLOR_WHITE);
            wbkgd(win1, COLOR_PAIR(3));
        }
        else
#endif
            wbkgd(win1, A_NORMAL);

        wclear(win1);
        mvwaddstr(win1, 5, 1, "This text should appear; using overlay option");
        copywin(win, win1, 0, 0, 0, 0, 9, 49, TRUE);
        box(win1, ACS_VLINE, ACS_HLINE);
        wmove(win1, 8, 26);
        wrefresh(win1);
        wgetch(win1);

        wclear(win1);

        wattron(win1, A_BLINK);
        mvwaddstr(win1, 4, 1,
                  "This blinking text should appear in only the second window");
        wattroff(win1, A_BLINK);

        mvwin(win1, by, bx);
        overlay(win, win1);
        mvwin(win1, 14, 25);
        wmove(win1, 8, 26);
        wrefresh(win1);
        wgetch(win1);

        delwin(win1);
    }

    clear();
    wclear(win);
    wrefresh(win);
    mvwaddstr(win, 6, 2, "This line shouldn't appear");
    mvwaddstr(win, 4, 2, "Only half of the next line is visible");
    mvwaddstr(win, 5, 2, "Only half of the next line is visible");
    wmove(win, 6, 1);
    wclrtobot(win);
    wmove(win, 5, 20);
    wclrtoeol(win);
    mvwaddstr(win, 8, 2, "This line also shouldn't appear");
    wmove(win, 8, 1);
    winsdelln(win, -1);
    Continue(win);

    wmove(win, 5, 9);
    ch = winch(win);

    wclear(win);
    wmove(win, 6, 2);
    waddstr(win, "The next char should be l:  ");
    winsch(win, ch);
    Continue(win);

    mvwinsstr(win, 6, 2, "A1B2C3D4E5");
    Continue(win);

    wmove(win, 5, 1);
    winsdelln(win, 1);
    mvwaddstr(win, 5, 2, "The lines below should have moved down");
    Continue(win);

    wclear(win);
    wmove(win, 2, 2);
    wprintw(win, "This is a formatted string in a window: %d %s\n",
            42, "is it");
    mvwaddstr(win, 10, 1, "Enter a string: ");
    wrefresh(win);
    echo();
    wscanw(win, "%s", Buffer);

    printw("This is a formatted string in stdscr: %d %s\n", 42, "is it");
    mvaddstr(10, 1, "Enter a string: ");
    scanw("%s", Buffer);

    wclear(win);
    curs_set(2);
    mvwaddstr(win, 1, 1, "The cursor should be in high-visibility mode");
    Continue(win);

    wclear(win);
    curs_set(0);
    mvwaddstr(win, 1, 1, "The cursor should have disappeared");
    Continue(win);

    wclear(win);
    curs_set(1);
    mvwaddstr(win, 1, 1, "The cursor should be normal");
    Continue(win);

#ifdef A_COLOR
    if (has_colors())
    {
        wclear(win);
        mvwaddstr(win, 1, 1, "Colors should change after you press a key");
        Continue(win);

        init_pair(1, COLOR_RED, COLOR_WHITE);
        wrefresh(win);
    }
#endif
    werase(win);
    mvwaddstr(win, 1, 1, "Information About Your Terminal");
    mvwaddstr(win, 3, 1, termname());
    mvwaddstr(win, 4, 1, longname());

    if (termattrs() & A_BLINK)
        mvwaddstr(win, 5, 1, "This terminal claims to support blinking.");
    else
        mvwaddstr(win, 5, 1, "This terminal does NOT support blinking.");

    mvwaddnstr(win, 7, 5, "Have a nice day!ok", 16);
    wrefresh(win);

    mvwinnstr(win, 7, 5, Buffer, 18);
    mvaddstr(LINES - 2, 10, Buffer);
    refresh();
    Continue(win);
#ifdef PDCURSES
    PDC_set_blink(FALSE);
#endif
}

#if HAVE_RESIZE
void resizeTest(WINDOW *dummy)
{
    WINDOW *win1;
    int nwidth = 135, nheight = 52;
    int owidth = COLS, oheight = LINES;

    savetty();

    resize_term(nheight, nwidth);

    clear();
    refresh();

    win1 = newwin(10, 50, 14, 25);

    if (win1 == NULL)
    {
        endwin();
        return;
    }

#ifdef A_COLOR
    if (has_colors())
    {
        init_pair(3, COLOR_BLUE, COLOR_WHITE);
        wattrset(win1, COLOR_PAIR(3));
    }

    wclear(win1);
#endif
    mvwaddstr(win1, 0, 0, "The screen may now be resized");
    mvwprintw(win1, 1, 4, "Given size: %d by %d", nwidth, nheight);
    mvwprintw(win1, 2, 4, "Actual size: %d by %d", COLS, LINES);
    Continue(win1);

    wclear(win1);
    resetty();

    mvwaddstr(win1, 0, 0, "The screen should now be reset");
    mvwprintw(win1, 1, 6, "Old size: %d by %d", owidth, oheight);
    mvwprintw(win1, 2, 6, "Size now: %d by %d", COLS, LINES);
    Continue(win1);

    delwin(win1);

    clear();
    refresh();
}
#endif /* HAVE_RESIZE */

void padTest(WINDOW *dummy)
{
    WINDOW *pad, *spad;

    pad = newpad(50, 100);
    wattron(pad, A_REVERSE);
    mvwaddstr(pad, 5, 2, "This is a new pad");
    wattrset(pad, 0);
    mvwaddstr(pad, 8, 0,
        "The end of this line should be truncated here:except  now");
    mvwaddstr(pad, 11, 1, "This line should not appear.It will now");
    wmove(pad, 10, 1);
    wclrtoeol(pad);
    mvwaddstr(pad, 10, 1, " Press any key to continue");
    prefresh(pad, 0, 0, 0, 0, 10, 45);
    keypad(pad, TRUE);
    raw();
    wgetch(pad);

    spad = subpad(pad, 12, 25, 7, 52);
    mvwaddstr(spad, 2, 2, "This is a new subpad");
    box(spad, 0, 0);
    prefresh(pad, 0, 0, 0, 0, 15, 75);
    keypad(pad, TRUE);
    raw();
    wgetch(pad);

    mvwaddstr(pad, 35, 2, "This is displayed at line 35 in the pad");
    mvwaddstr(pad, 40, 1, " Press any key to continue");
    prefresh(pad, 30, 0, 0, 0, 10, 45);
    keypad(pad, TRUE);
    raw();
    wgetch(pad);

    delwin(pad);
}

#if HAVE_CLIPBOARD
void clipboardTest(WINDOW *win)
{
    static const char *text =
        "This string placed in clipboard by PDCurses test program, testcurs.";
    char *ptr = NULL;
    long i, length = 0;

    mvaddstr(1, 1,
             "This test will display the contents of the system clipboard");

    Continue2();

    scrollok(stdscr, TRUE);
    i = PDC_getclipboard(&ptr, &length);

    switch(i)
    {
    case PDC_CLIP_ACCESS_ERROR:
        mvaddstr(3, 1, "There was an error accessing the clipboard");
        refresh();
        break;

    case PDC_CLIP_MEMORY_ERROR:
        mvaddstr(3, 1,
            "Unable to allocate memory for clipboard contents");
        break;

    case PDC_CLIP_EMPTY:
        mvaddstr(3, 1, "There was no text in the clipboard");
        break;

    default:
        wsetscrreg(stdscr, 0, LINES - 1);
        clear();
        mvaddstr(1, 1, "Clipboard contents...");
        mvprintw(2, 1, "%s\n", ptr);
    }

    Continue2();

    clear();
    mvaddstr(1, 1,
        "This test will place the following string in the system clipboard:");
    mvaddstr(2, 1, text);

    i = PDC_setclipboard(text, strlen(text));

    switch(i)
    {
    case PDC_CLIP_ACCESS_ERROR:
        mvaddstr(3, 1, "There was an error accessing the clipboard");
        break;

    case PDC_CLIP_MEMORY_ERROR:
        mvaddstr(3, 1, "Unable to allocate memory for clipboard contents");
        break;

    default:
        mvaddstr(3, 1, "The string was placed in the clipboard successfully");
    }

    Continue2();
}
#endif /* HAVE_CLIPBOARD */

void acsTest(WINDOW *win)
{
#ifdef ACS_S3
# define ACSNUM 32
#else
# define ACSNUM 25
#endif
    static const char *acs_names[] =
    {
        "ACS_ULCORNER", "ACS_URCORNER", "ACS_LLCORNER", "ACS_LRCORNER",
        "ACS_LTEE", "ACS_RTEE", "ACS_TTEE", "ACS_BTEE", "ACS_HLINE",
        "ACS_VLINE", "ACS_PLUS",

        "ACS_S1", "ACS_S9", "ACS_DIAMOND", "ACS_CKBOARD", "ACS_DEGREE",
        "ACS_PLMINUS", "ACS_BULLET",

        "ACS_LARROW", "ACS_RARROW", "ACS_UARROW", "ACS_DARROW",
        "ACS_BOARD", "ACS_LANTERN", "ACS_BLOCK"
#ifdef ACS_S3
        , "ACS_S3", "ACS_S7", "ACS_LEQUAL", "ACS_GEQUAL",
        "ACS_PI", "ACS_NEQUAL", "ACS_STERLING"
#endif
    };

    chtype acs_values[ACSNUM];

#if HAVE_WIDE
    cchar_t *wacs_values[] =
    {
        WACS_ULCORNER, WACS_URCORNER, WACS_LLCORNER, WACS_LRCORNER,
        WACS_LTEE, WACS_RTEE, WACS_TTEE, WACS_BTEE, WACS_HLINE,
        WACS_VLINE, WACS_PLUS,

        WACS_S1, WACS_S9, WACS_DIAMOND, WACS_CKBOARD, WACS_DEGREE,
        WACS_PLMINUS, WACS_BULLET,

        WACS_LARROW, WACS_RARROW, WACS_UARROW, WACS_DARROW, WACS_BOARD,
        WACS_LANTERN, WACS_BLOCK
# ifdef WACS_S3
        , WACS_S3, WACS_S7, WACS_LEQUAL, WACS_GEQUAL, WACS_PI,
        WACS_NEQUAL, WACS_STERLING
# endif
    };

    static const wchar_t russian[] = {0x0420, 0x0443, 0x0441, 0x0441,
        0x043a, 0x0438, 0x0439, L' ', 0x044f, 0x0437, 0x044b, 0x043a, 0};

    static const wchar_t greek[] = {0x0395, 0x03bb, 0x03bb, 0x03b7,
        0x03bd, 0x03b9, 0x03ba, 0x03ac, 0};

    static const wchar_t georgian[] = {0x10e5, 0x10d0, 0x10e0, 0x10d7,
        0x10e3, 0x10da, 0x10d8, L' ', 0x10d4, 0x10dc, 0x10d0, 0};
#endif

    int i, tmarg = (LINES - 22) / 2;

    attrset(A_BOLD);
    mvaddstr(tmarg, (COLS - 23) / 2, "Alternate Character Set");
    attrset(A_NORMAL);

    tmarg += 3;

#define A(b,c) acs_values[b] = ACS_##c

    A(0,ULCORNER); A(1,URCORNER); A(2,LLCORNER); A(3,LRCORNER);
    A(4,LTEE);     A(5,RTEE);     A(6,TTEE);     A(7,BTEE);
    A(8,HLINE);    A(9,VLINE);    A(10,PLUS);    A(11,S1);
    A(12,S9);      A(13,DIAMOND); A(14,CKBOARD); A(15,DEGREE);

    A(16,PLMINUS); A(17,BULLET);  A(18,LARROW);  A(19,RARROW);
    A(20,UARROW);  A(21,DARROW);  A(22,BOARD);   A(23,LANTERN);
    A(24,BLOCK);
#ifdef ACS_S3
    A(25,S3);      A(26,S7);      A(27,LEQUAL);  A(28,GEQUAL);
    A(29,PI);      A(30,NEQUAL);  A(31,STERLING);
#endif

#undef A

    for (i = 0; i < ACSNUM; i++)
    {
        move((i % 8) * 2 + tmarg, (i / 8) * (COLS / 4) + (COLS / 8 - 7));
        addch(acs_values[i]);
        printw(" %s", acs_names[i]);
    }

    mvaddstr(tmarg + 18, 3, "Press any key to continue");
    getch();

#if HAVE_WIDE
    clear();

    attrset(A_BOLD);
    mvaddstr(tmarg - 3, (COLS - 28) / 2, "Wide Alternate Character Set");
    attrset(A_NORMAL);

    for (i = 0; i < ACSNUM; i++)
    {
        move((i % 8) * 2 + tmarg, (i / 8) * (COLS / 4) + (COLS / 8 - 7));
        add_wch(wacs_values[i]);
        printw(" W%s", acs_names[i]);
    }

    /* Spanish, Russian, Greek, Georgian */

    mvaddwstr(tmarg + 16, COLS / 8 - 5, L"Espa\xf1ol");
    mvaddwstr(tmarg + 16, 3 * (COLS / 8) - 5, russian);
    mvaddwstr(tmarg + 16, 5 * (COLS / 8) - 5, greek);
    mvaddwstr(tmarg + 16, 7 * (COLS / 8) - 5, georgian);

    mvaddstr(tmarg + 18, 3, "Press any key to continue");
    getch();
#endif
}

void attrTest(WINDOW *win)
{
    int tmarg = (LINES - 16) / 2;
    int col1 = (COLS - 36) / 2, col2 = col1 + 20;

    attrset(A_BOLD);
    mvaddstr(tmarg, (COLS - 20) / 2, "Character Attributes");
    attrset(A_NORMAL);

    refresh();

#ifdef PDCURSES
    PDC_set_blink(TRUE);
    PDC_set_bold(TRUE);
#endif

#ifdef A_ITALIC
    attrset(A_ITALIC);
    mvaddstr(tmarg + 3, col1, "A_ITALIC");
    attrset(A_NORMAL);
#endif

    attrset(A_BOLD);
    mvaddstr(tmarg + 5, col1, "A_BOLD");
    attrset(A_NORMAL);

    attrset(A_BLINK);
    mvaddstr(tmarg + 7, col1, "A_BLINK");
    attrset(A_NORMAL);

    attrset(A_REVERSE);
    mvaddstr(tmarg + 9, col1, "A_REVERSE");
    attrset(A_NORMAL);

    attrset(A_STANDOUT);
    mvaddstr(tmarg + 11, col1, "A_STANDOUT");
    attrset(A_NORMAL);

    attrset(A_UNDERLINE);
    mvaddstr(tmarg + 13, col1, "A_UNDERLINE");
    attrset(A_NORMAL);

#ifdef A_ITALIC
    attrset(A_ITALIC|A_UNDERLINE);
    mvaddstr(tmarg + 3, col2, "Underlined Italic");
    attrset(A_NORMAL);
#endif

    attrset(A_BOLD|A_UNDERLINE);
    mvaddstr(tmarg + 5, col2, "Underlined Bold");
    attrset(A_NORMAL);

    attrset(A_BLINK|A_UNDERLINE);
    mvaddstr(tmarg + 7, col2, "Underlined Blink");
    attrset(A_NORMAL);

#ifdef A_LEFT
    attrset(A_LEFT);
    mvaddstr(tmarg + 9, col2, "A_LEFT");
    attrset(A_NORMAL);
#endif

#ifdef A_RIGHT
    attrset(A_RIGHT);
    mvaddstr(tmarg + 11, col2, "A_RIGHT");
    attrset(A_NORMAL);
#endif

    attrset(A_BLINK|A_REVERSE);
    mvaddstr(tmarg + 13, col2, "Reverse Blink");
    attrset(A_NORMAL);

    mvaddstr(tmarg + 16, 3, "Press any key to continue");
    getch();

#ifdef PDCURSES
    PDC_set_bold(FALSE);
    PDC_set_blink(FALSE);
#endif
}

#if HAVE_COLOR
void colorTest(WINDOW *win)
{
    static const short colors[] =
    {
        COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_CYAN, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE
    };

    static const char *colornames[] =
    {
        "COLOR_BLACK", "COLOR_RED", "COLOR_GREEN", "COLOR_BLUE",
        "COLOR_CYAN", "COLOR_MAGENTA", "COLOR_YELLOW", "COLOR_WHITE"
    };

    chtype fill = ACS_BLOCK;
    int maxcol = (COLORS >= 16) ? 16 : 8;
    bool widecol = (maxcol == 16);

    int i, j, tmarg, col1, col2, col3;

    if (!has_colors())
        return;

    tmarg = (LINES - 19) / 2;
    col1 = (COLS - 60) / 2;
    col2 = col1 + 20;
    col3 = col2 + 20;

    attrset(A_BOLD);
    mvaddstr(tmarg, (COLS - 22) / 2, "Color Attribute Macros");
    attrset(A_NORMAL);

    if (widecol)
    {
        mvaddstr(tmarg + 3, col2 + 3, "Colors 0-7");
        mvaddstr(tmarg + 3, col3 + 2, "Colors 8-15");
    }
    else
    {
        mvaddstr(tmarg + 3, col2 + 4, "A_NORMAL");
        mvaddstr(tmarg + 3, col3 + 5, "A_BOLD");
    }

    for (i = 0; i < 8; i++)
    {
        init_pair(i + 4, colors[i], COLOR_BLACK);
        if (widecol)
            init_pair(i + 12, colors[i] + 8, COLOR_BLACK);

        mvaddstr(tmarg + i + 5, col1, colornames[i]);

        for (j = 0; j < 16; j++)
        {
            mvaddch(tmarg + i + 5, col2 + j, fill | COLOR_PAIR(i + 4));
            mvaddch(tmarg + i + 5, col3 + j, fill | (widecol ?
                    COLOR_PAIR(i + 12) : (COLOR_PAIR(i + 4) | A_BOLD) ));
        }
    }

    mvprintw(tmarg + 15, col1, "COLORS = %d", COLORS);
    mvprintw(tmarg + 16, col1, "COLOR_PAIRS = %d", COLOR_PAIRS);

    mvaddstr(tmarg + 19, 3, "Press any key to continue");
    getch();

    if (can_change_color())
    {
        struct
        {
            short red, green, blue;
        } orgcolors[16];

        for (i = 0; i < maxcol; i++)
            color_content(i, &(orgcolors[i].red),
                             &(orgcolors[i].green),
                             &(orgcolors[i].blue));

        attrset(A_BOLD);
        mvaddstr(tmarg, (COLS - 22) / 2, " init_color() Example ");
        attrset(A_NORMAL);

        refresh();

        for (i = 0; i < 8; i++)
        {
            init_color(colors[i], i * 125, 0, i * 125);

            if (widecol)
                init_color(colors[i] + 8, 0, i * 125, 0);
        }

        mvaddstr(tmarg + 19, 3, "Press any key to continue");
        getch();

        for (i = 0; i < maxcol; i++)
            init_color(i, orgcolors[i].red,
                          orgcolors[i].green,
                          orgcolors[i].blue);
    }

    if (COLORS >= 256)
    {
        int x, y, z, lmarg = (COLS - 77) / 2;

        erase();

        attrset(A_BOLD);
        mvaddstr(tmarg, (COLS - 15) / 2, "Extended Colors");
        attrset(A_NORMAL);

        mvaddstr(tmarg + 3, lmarg, "6x6x6 Color Cube (16-231):");

        for (i = 16; i < 256; i++)
            init_pair(i, COLOR_BLACK, i);

        for (i = 16, x = 0; x < 6; x++)
            for (z = 0; z < 6; z++)
                for (y = 0; y < 6; y++)
                {
                    chtype ch = ' ' | COLOR_PAIR(i++);

                    mvaddch(tmarg + 5 + y, z * 13 + x * 2 + lmarg, ch);
                    addch(ch);
                }

        mvaddstr(tmarg + 13, lmarg, "Greyscale (232-255):");

        for (x = 0; x < 24; x++)
        {
            chtype ch = ' ' | COLOR_PAIR(232 + x);

            mvaddch(tmarg + 15, x * 2 + lmarg, ch);
            addch(ch);
        }

        mvaddstr(tmarg + 19, 3, "Press any key to continue");
        getch();
    }
}
#endif

#if HAVE_WIDE
void wideTest(WINDOW *win)
{
    wchar_t tmp[513];
    size_t i;

    attrset(A_BOLD);
    mvaddstr(1, (COLS - 25) / 2, "Wide Character Input Test");
    attrset(A_NORMAL);

    mvaddstr(4, 1, "Enter a string: ");

    echo();

    get_wstr((wint_t *)tmp);
    addstr("\n\n String:\n\n ");
    addwstr(tmp);
    addstr("\n\n\n Hex:\n\n ");

    for (i = 0; i < wcslen(tmp); i++)
    {
        printw("%04x ", tmp[i]);
        addnwstr(tmp + i, 1);
        addstr("  ");
    }

    noecho();

    Continue2();
}
#endif

void display_menu(int old_option, int new_option)
{
    int lmarg = (COLS - 14) / 2,
        tmarg = (LINES - (MAX_OPTIONS + 2)) / 2;

    if (old_option == -1)
    {
        int i;

        attrset(A_BOLD);
        mvaddstr(tmarg - 3, lmarg - 5, "PDCurses Test Program");
        attrset(A_NORMAL);

        for (i = 0; i < MAX_OPTIONS; i++)
            mvaddstr(tmarg + i, lmarg, command[i].text);
    }
    else
        mvaddstr(tmarg + old_option, lmarg, command[old_option].text);

    attrset(A_REVERSE);
    mvaddstr(tmarg + new_option, lmarg, command[new_option].text);
    attrset(A_NORMAL);

    mvaddstr(tmarg + MAX_OPTIONS + 2, lmarg - 23,
             "Use Up and Down Arrows to select - Enter to run - Q to quit");
    refresh();
}
