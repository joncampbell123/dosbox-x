#include <curses.h>
#include <stdlib.h>
#include <time.h>

#define DELAYSIZE 200

void myrefresh(void);
void get_color(void);
void explode(int, int);

short color_table[] =
{
    COLOR_RED, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
    COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE
};

int main(int argc, char **argv)
{
    time_t seed;
    int start, end, row, diff, flag, direction;
    short i;

#ifdef XCURSES
    Xinitscr(argc, argv);
#else
    initscr();
#endif
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    noecho();

    if (has_colors())
        start_color();

    for (i = 0; i < 8; i++)
        init_pair(i, color_table[i], COLOR_BLACK);

    seed = time((time_t *)0);
    srand(seed);
    flag = 0;

    while (getch() == ERR)      /* loop until a key is hit */
    {
        do {
            start = rand() % (COLS - 3);
            end = rand() % (COLS - 3);
            start = (start < 2) ? 2 : start;
            end = (end < 2) ? 2 : end;
            direction = (start > end) ? -1 : 1;
            diff = abs(start - end);

        } while (diff < 2 || diff >= LINES - 2);

        attrset(A_NORMAL);

        for (row = 0; row < diff; row++)
        {
            mvaddstr(LINES - row, row * direction + start,
                (direction < 0) ? "\\" : "/");

            if (flag++)
            {
                myrefresh();
                erase();
                flag = 0;
            }
        }

        if (flag++)
        {
            myrefresh();
            flag = 0;
        }

        explode(LINES - row, diff * direction + start);
        erase();
        myrefresh();
    }

    endwin();

    return 0;
}

void explode(int row, int col)
{
    erase();
    mvaddstr(row, col, "-");
    myrefresh();

    --col;

    get_color();
    mvaddstr(row - 1, col, " - ");
    mvaddstr(row,     col, "-+-");
    mvaddstr(row + 1, col, " - ");
    myrefresh();

    --col;

    get_color();
    mvaddstr(row - 2, col, " --- ");
    mvaddstr(row - 1, col, "-+++-");
    mvaddstr(row,     col, "-+#+-");
    mvaddstr(row + 1, col, "-+++-");
    mvaddstr(row + 2, col, " --- ");
    myrefresh();

    get_color();
    mvaddstr(row - 2, col, " +++ ");
    mvaddstr(row - 1, col, "++#++");
    mvaddstr(row,     col, "+# #+");
    mvaddstr(row + 1, col, "++#++");
    mvaddstr(row + 2, col, " +++ ");
    myrefresh();

    get_color();
    mvaddstr(row - 2, col, "  #  ");
    mvaddstr(row - 1, col, "## ##");
    mvaddstr(row,     col, "#   #");
    mvaddstr(row + 1, col, "## ##");
    mvaddstr(row + 2, col, "  #  ");
    myrefresh();

    get_color();
    mvaddstr(row - 2, col, " # # ");
    mvaddstr(row - 1, col, "#   #");
    mvaddstr(row,     col, "     ");
    mvaddstr(row + 1, col, "#   #");
    mvaddstr(row + 2, col, " # # ");
    myrefresh();
}

void myrefresh(void)
{
    napms(DELAYSIZE);
    move(LINES - 1, COLS - 1);
    refresh();
}

void get_color(void)
{
    chtype bold = (rand() % 2) ? A_BOLD : A_NORMAL;
    attrset(COLOR_PAIR(rand() % 8) | bold);
}
