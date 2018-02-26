/* Public Domain Curses */

#include "pdcsdl.h"

#include <stdlib.h>
#ifndef PDC_WIDE
# include "deffont.h"
#endif
#include "deficon.h"

#ifdef PDC_WIDE
# ifndef PDC_FONT_PATH
#  ifdef _WIN32
#   define PDC_FONT_PATH "C:/Windows/Fonts/consola.ttf"
#  elif defined(__APPLE__)
#   define PDC_FONT_PATH "/Library/Fonts/Courier New.ttf"
#  else
#   define PDC_FONT_PATH "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
#  endif
# endif
TTF_Font *pdc_ttffont = NULL;
int pdc_font_size = 18;
#endif

SDL_Window *pdc_window = NULL;
SDL_Surface *pdc_screen = NULL, *pdc_font = NULL, *pdc_icon = NULL,
            *pdc_back = NULL, *pdc_tileback = NULL;
int pdc_sheight = 0, pdc_swidth = 0, pdc_yoffset = 0, pdc_xoffset = 0;

SDL_Color pdc_color[256];
Uint32 pdc_mapped[256];
int pdc_fheight, pdc_fwidth, pdc_flastc;
bool pdc_own_window;

/* COLOR_PAIR to attribute encoding table. */

static struct {short f, b;} atrtab[PDC_COLOR_PAIRS];

static void _clean(void)
{
#ifdef PDC_WIDE
    if (pdc_ttffont)
    {
        TTF_CloseFont(pdc_ttffont);
        TTF_Quit();
    }
#endif
    SDL_FreeSurface(pdc_tileback);
    SDL_FreeSurface(pdc_back);
    SDL_FreeSurface(pdc_icon);
    SDL_FreeSurface(pdc_font);
    SDL_DestroyWindow(pdc_window);
    SDL_Quit();
}

void PDC_retile(void)
{
    if (pdc_tileback)
        SDL_FreeSurface(pdc_tileback);

    pdc_tileback = SDL_ConvertSurface(pdc_screen, pdc_screen->format, 0);
    if (pdc_tileback == NULL)
        return;

    if (pdc_back)
    {
        SDL_Rect dest;

        dest.y = 0;

        while (dest.y < pdc_tileback->h)
        {
            dest.x = 0;

            while (dest.x < pdc_tileback->w)
            {
                SDL_BlitSurface(pdc_back, 0, pdc_tileback, &dest);
                dest.x += pdc_back->w;
            }

            dest.y += pdc_back->h;
        }

        SDL_BlitSurface(pdc_tileback, 0, pdc_screen, 0);
    }
}

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
}

void PDC_scr_free(void)
{
    if (SP)
        free(SP);
}

static void _initialize_colors(void)
{
    int i, r, g, b;

    for (i = 0; i < 8; i++)
    {
        pdc_color[i].r = (i & COLOR_RED) ? 0xc0 : 0;
        pdc_color[i].g = (i & COLOR_GREEN) ? 0xc0 : 0;
        pdc_color[i].b = (i & COLOR_BLUE) ? 0xc0 : 0;

        pdc_color[i + 8].r = (i & COLOR_RED) ? 0xff : 0x40;
        pdc_color[i + 8].g = (i & COLOR_GREEN) ? 0xff : 0x40;
        pdc_color[i + 8].b = (i & COLOR_BLUE) ? 0xff : 0x40;
    }

    /* 256-color xterm extended palette: 216 colors in a 6x6x6 color
       cube, plus 24 shades of gray */

    for (i = 16, r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++, i++)
            {
                pdc_color[i].r = (r ? r * 40 + 55 : 0);
                pdc_color[i].g = (g ? g * 40 + 55 : 0);
                pdc_color[i].b = (b ? b * 40 + 55 : 0);
            }

    for (i = 232; i < 256; i++)
        pdc_color[i].r = pdc_color[i].g = pdc_color[i].b = (i - 232) * 10 + 8;

    for (i = 0; i < 256; i++)
        pdc_mapped[i] = SDL_MapRGB(pdc_screen->format, pdc_color[i].r,
                                   pdc_color[i].g, pdc_color[i].b);
}

/* open the physical screen -- allocate SP, miscellaneous intialization */

int PDC_scr_open(int argc, char **argv)
{
    PDC_LOG(("PDC_scr_open() - called\n"));

    SP = calloc(1, sizeof(SCREEN));

    if (!SP)
        return ERR;

    pdc_own_window = !pdc_window;

    if (pdc_own_window)
    {
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS) < 0)
        {
            fprintf(stderr, "Could not start SDL: %s\n", SDL_GetError());
            return ERR;
        }

        atexit(_clean);
    }

#ifdef PDC_WIDE
    if (!pdc_ttffont)
    {
        const char *ptsz, *fname;

        if (TTF_Init() == -1)
        {
            fprintf(stderr, "Could not start SDL_TTF: %s\n", SDL_GetError());
            return ERR;
        }

        ptsz = getenv("PDC_FONT_SIZE");
        if (ptsz != NULL)
            pdc_font_size = atoi(ptsz);
        if (pdc_font_size <= 0)
            pdc_font_size = 18;

        fname = getenv("PDC_FONT");
        pdc_ttffont = TTF_OpenFont(fname ? fname : PDC_FONT_PATH,
                                   pdc_font_size);
    }

    if (!pdc_ttffont)
    {
        fprintf(stderr, "Could not load font\n");
        return ERR;
    }

    TTF_SetFontKerning(pdc_ttffont, 0);
    TTF_SetFontHinting(pdc_ttffont, TTF_HINTING_MONO);

    SP->mono = FALSE;
#else
    if (!pdc_font)
    {
        const char *fname = getenv("PDC_FONT");
        pdc_font = SDL_LoadBMP(fname ? fname : "pdcfont.bmp");
    }

    if (!pdc_font)
        pdc_font = SDL_LoadBMP_RW(SDL_RWFromMem(deffont, sizeof(deffont)), 0);

    if (!pdc_font)
    {
        fprintf(stderr, "Could not load font\n");
        return ERR;
    }

    SP->mono = !pdc_font->format->palette;
#endif

    if (!SP->mono && !pdc_back)
    {
        const char *bname = getenv("PDC_BACKGROUND");
        pdc_back = SDL_LoadBMP(bname ? bname : "pdcback.bmp");
    }

    if (!SP->mono && (pdc_back || !pdc_own_window))
    {
        SP->orig_attr = TRUE;
        SP->orig_fore = COLOR_WHITE;
        SP->orig_back = -1;
    }
    else
        SP->orig_attr = FALSE;

#ifdef PDC_WIDE
    TTF_SizeText(pdc_ttffont, "W", &pdc_fwidth, &pdc_fheight);
#else
    pdc_fheight = pdc_font->h / 8;
    pdc_fwidth = pdc_font->w / 32;

    if (!SP->mono)
        pdc_flastc = pdc_font->format->palette->ncolors - 1;
#endif

    if (pdc_own_window && !pdc_icon)
    {
        const char *iname = getenv("PDC_ICON");
        pdc_icon = SDL_LoadBMP(iname ? iname : "pdcicon.bmp");

        if (!pdc_icon)
            pdc_icon = SDL_LoadBMP_RW(SDL_RWFromMem(deficon,
                                                    sizeof(deficon)), 0);
    }

    if (pdc_own_window)
    {
        const char *env = getenv("PDC_LINES");
        pdc_sheight = (env ? atoi(env) : 25) * pdc_fheight;

        env = getenv("PDC_COLS");
        pdc_swidth = (env ? atoi(env) : 80) * pdc_fwidth;

        pdc_window = SDL_CreateWindow((argc ? argv[0] : "PDCurses"),
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pdc_swidth,
            pdc_sheight, SDL_WINDOW_RESIZABLE);
        if (pdc_window == NULL)
        {
            fprintf(stderr, "Could not open SDL window: %s\n", SDL_GetError());
            return ERR;
        }
        SDL_SetWindowIcon(pdc_window, pdc_icon);

        /* Events must be pumped before calling SDL_GetWindowSurface, or
           initial modifiers (e.g. numlock) will be ignored and out-of-sync. */
        SDL_PumpEvents();

        pdc_screen = SDL_GetWindowSurface(pdc_window);
        if (pdc_screen == NULL)
        {
            fprintf(stderr, "Could not open SDL window surface: %s\n",
                    SDL_GetError());
            return ERR;
        }
    }
    else
    {
        if (!pdc_screen)
            pdc_screen = SDL_GetWindowSurface(pdc_window);

        if (!pdc_sheight)
            pdc_sheight = pdc_screen->h - pdc_yoffset;

        if (!pdc_swidth)
            pdc_swidth = pdc_screen->w - pdc_xoffset;
    }

    if (!pdc_screen)
    {
        fprintf(stderr, "Couldn't create a surface: %s\n", SDL_GetError());
        return ERR;
    }

    if (SP->orig_attr)
        PDC_retile();

    _initialize_colors();

    SDL_StartTextInput();

    PDC_mouse_set();

    if (pdc_own_window)
        PDC_set_title(argc ? argv[0] : "PDCurses");

    SP->lines = PDC_get_rows();
    SP->cols = PDC_get_columns();

    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->audible = FALSE;

    SP->termattrs = A_COLOR | A_UNDERLINE | A_LEFT | A_RIGHT | A_REVERSE;
#ifdef PDC_WIDE
    SP->termattrs |= A_ITALIC;
#endif

    PDC_reset_prog_mode();

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_Rect max;
    int top, left, bottom, right;
#endif

    if (!pdc_own_window)
        return ERR;

#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_GetDisplayUsableBounds(0, &max);
    SDL_GetWindowBordersSize(pdc_window, &top, &left, &bottom, &right);
    max.h -= top + bottom;
    max.w -= left + right;
#endif

    if (nlines && ncols)
    {
#if SDL_VERSION_ATLEAST(2, 0, 5)
        while (nlines * pdc_fheight > max.h)
            nlines--;
        while (ncols * pdc_fwidth > max.w)
            ncols--;
#endif
        pdc_sheight = nlines * pdc_fheight;
        pdc_swidth = ncols * pdc_fwidth;
    }

    SDL_SetWindowSize(pdc_window, pdc_swidth, pdc_sheight);
    pdc_screen = SDL_GetWindowSurface(pdc_window);

    if (pdc_tileback)
        PDC_retile();

    SP->resized = FALSE;
    SP->cursrow = SP->curscol = 0;

    return OK;
}

void PDC_reset_prog_mode(void)
{
    PDC_LOG(("PDC_reset_prog_mode() - called.\n"));

    PDC_flushinp();
}

void PDC_reset_shell_mode(void)
{
    PDC_LOG(("PDC_reset_shell_mode() - called.\n"));

    PDC_flushinp();
}

void PDC_restore_screen_mode(int i)
{
}

void PDC_save_screen_mode(int i)
{
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
    return TRUE;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    *red = DIVROUND(pdc_color[color].r * 1000, 255);
    *green = DIVROUND(pdc_color[color].g * 1000, 255);
    *blue = DIVROUND(pdc_color[color].b * 1000, 255);

    return OK;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    pdc_color[color].r = DIVROUND(red * 255, 1000);
    pdc_color[color].g = DIVROUND(green * 255, 1000);
    pdc_color[color].b = DIVROUND(blue * 255, 1000);

    pdc_mapped[color] = SDL_MapRGB(pdc_screen->format, pdc_color[color].r,
                                   pdc_color[color].g, pdc_color[color].b);

    wrefresh(curscr);

    return OK;
}
