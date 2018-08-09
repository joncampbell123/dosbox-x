/* Here's a simple example of combining SDL and PDCurses functionality.
   The top portion of the window is devoted to SDL, with a four-line
   (assuming the default 8x16 font) stdscr at the bottom.
*/

#include <SDL.h>
#include <curses.h>
#include <stdlib.h>
#include <time.h>

/* You could #include pdcsdl.h, or just add the relevant declarations
   here: */

PDCEX SDL_Surface *pdc_screen;
PDCEX int pdc_yoffset;

int main(int argc, char **argv)
{
    char inp[60];
    int i, j, seed;

    seed = time((time_t *)0);
    srand(seed);

    /* Initialize SDL */

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        exit(1);

    atexit(SDL_Quit);

    pdc_screen = SDL_SetVideoMode(640, 480, 0, SDL_SWSURFACE|SDL_ANYFORMAT);

    /* Initialize PDCurses */

    pdc_yoffset = 416;  /* 480 - 4 * 16 */

    initscr();
    start_color();
    scrollok(stdscr, TRUE);

    PDC_set_title("PDCurses for SDL");

    /* Do some SDL stuff */

    for (i = 640, j = 416; j; i -= 2, j -= 2)
    {
        SDL_Rect dest;

        dest.x = (640 - i) / 2;
        dest.y = (416 - j) / 2;
        dest.w = i;
        dest.h = j;

        SDL_FillRect(pdc_screen, &dest,
                     SDL_MapRGB(pdc_screen->format, rand() % 256,
                                rand() % 256, rand() % 256));
    }

    SDL_UpdateRect(pdc_screen, 0, 0, 640, 416);

    /* Do some curses stuff */

    init_pair(1, COLOR_WHITE + 8, COLOR_BLUE);
    bkgd(COLOR_PAIR(1));

    addstr("This is a demo of ");
    attron(A_UNDERLINE);
    addstr("PDCurses for SDL");
    attroff(A_UNDERLINE);
    addstr(".\nYour comments here: ");
    getnstr(inp, 59);
    addstr("Press any key to exit.");

    getch();
    endwin();

    return 0;
}
