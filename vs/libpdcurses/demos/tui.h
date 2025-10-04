/*
 * 'textual user interface'
 *
 * Author : P.J. Kunst <kunst@prl.philips.nl>
 * Date   : 25-02-93
 */

#ifndef _TUI_H_
#define _TUI_H_

#include <curses.h>

#ifdef A_COLOR
#define A_ATTR  (A_ATTRIBUTES ^ A_COLOR)  /* A_BLINK, A_REVERSE, A_BOLD */
#else
#define A_ATTR  (A_ATTRIBUTES)            /* standard UNIX attributes */
#endif

#define MAXSTRLEN  256
#define KEY_ESC    0x1b     /* Escape */

typedef void (*FUNC)(void);

typedef struct
{
    char *name; /* item label */
    FUNC  func; /* (pointer to) function */
    char *desc; /* function description */
} menu;

/* ANSI C function prototypes: */

void    clsbody(void);
int     bodylen(void);
WINDOW *bodywin(void);

void    rmerror(void);
void    rmstatus(void);

void    titlemsg(char *msg);
void    bodymsg(char *msg);
void    errormsg(char *msg);
void    statusmsg(char *msg);

bool    keypressed(void);
int     getkey(void);
int     waitforkey(void);

void    DoExit(void);
void    startmenu(menu *mp, char *title);
void    domenu(menu *mp);

int     weditstr(WINDOW *win, char *buf, int field);
WINDOW *winputbox(WINDOW *win, int nlines, int ncols);
int     getstrings(char *desc[], char *buf[], int field);

#define editstr(s,f)           (weditstr(stdscr,s,f))
#define mveditstr(y,x,s,f)     (move(y,x)==ERR?ERR:editstr(s,f))
#define mvweditstr(w,y,x,s,f)  (wmove(w,y,x)==ERR?ERR:weditstr(w,s,f))

#define inputbox(l,c)          (winputbox(stdscr,l,c))
#define mvinputbox(y,x,l,c)    (move(y,x)==ERR?w:inputbox(l,c))
#define mvwinputbox(w,y,x,l,c) (wmove(w,y,x)==ERR?w:winputbox(w,l,c))

#endif
