/*
 * Author : P.J. Kunst <kunst@prl.philips.nl>
 * Date   : 25-02-93
 *
 * Purpose: This program demonstrates the use of the 'curses' library
 *          for the creation of (simple) menu-operated programs.
 *          In the PDCurses version, use is made of colors for the
 *          highlighting of subwindows (title bar, status bar etc).
 *
 * Acknowledgement: some ideas were borrowed from Mark Hessling's
 *                  version of the 'testcurs' program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "tui.h"

/* change this if source at other location */

#ifdef XCURSES
# define FNAME  "../demos/tui.c"
#else
# define FNAME  "..\\demos\\tui.c"
#endif

/**************************** strings entry box ***************************/

void address(void)
{
    char *fieldname[6] =
    {
        "Name", "Street", "City", "State", "Country", (char *)0
    };

    char *fieldbuf[5];
    WINDOW *wbody = bodywin();
    int i, field = 50;

    for (i = 0; i < 5; i++)
        fieldbuf[i] = calloc(1, field + 1);

    if (getstrings(fieldname, fieldbuf, field) != KEY_ESC)
    {
        for (i = 0; fieldname[i]; i++)
            wprintw(wbody, "%10s : %s\n",
                fieldname[i], fieldbuf[i]);

        wrefresh(wbody);
    }

    for (i = 0; i < 5; i++)
        free(fieldbuf[i]);
}

/**************************** string entry box ****************************/

char *getfname(char *desc, char *fname, int field)
{
    char *fieldname[2];
    char *fieldbuf[1];

    fieldname[0] = desc;
    fieldname[1] = 0;
    fieldbuf[0] = fname;

    return (getstrings(fieldname, fieldbuf, field) == KEY_ESC) ? NULL : fname;
}

/**************************** a very simple file browser ******************/

void showfile(char *fname)
{
    int i, bh = bodylen();
    FILE *fp;
    char buf[MAXSTRLEN];
    bool ateof = FALSE;

    statusmsg("FileBrowser: Hit key to continue, Q to quit");

    if ((fp = fopen(fname, "r")) != NULL)   /* file available? */
    {
        while (!ateof)
        {
            clsbody();

            for (i = 0; i < bh - 1 && !ateof; i++)
            {
                if (fgets(buf, MAXSTRLEN, fp))
                    bodymsg(buf);
                else
                    ateof = TRUE;
            }

            switch (waitforkey())
            {
            case 'Q':
            case 'q':
            case 0x1b:
                ateof = TRUE;
            }
        }

        fclose(fp);
    }
    else
    {
        sprintf(buf, "ERROR: file '%s' not found", fname);
        errormsg(buf);
    }
}

/***************************** forward declarations ***********************/

void sub0(void), sub1(void), sub2(void), sub3(void);
void func1(void), func2(void);
void subfunc1(void), subfunc2(void);
void subsub(void);

/***************************** menus initialization ***********************/

menu MainMenu[] =
{
    { "Asub", sub0, "Go inside first submenu" },
    { "Bsub", sub1, "Go inside second submenu" },
    { "Csub", sub2, "Go inside third submenu" },
    { "Dsub", sub3, "Go inside fourth submenu" },
    { "", (FUNC)0, "" }   /* always add this as the last item! */
};

menu SubMenu0[] =
{
    { "Exit", DoExit, "Terminate program" },
    { "", (FUNC)0, "" }
};

menu SubMenu1[] =
{
    { "OneBeep", func1, "Sound one beep" },
    { "TwoBeeps", func2, "Sound two beeps" },
    { "", (FUNC)0, "" }
};

menu SubMenu2[] =
{
    { "Browse", subfunc1, "Source file lister" },
    { "Input", subfunc2, "Interactive file lister" },
    { "Address", address, "Get address data" },
    { "", (FUNC)0, "" }
};

menu SubMenu3[] =
{
    { "SubSub", subsub, "Go inside sub-submenu" },
    { "", (FUNC)0, "" }
};

/***************************** main menu functions ************************/

void sub0(void)
{
    domenu(SubMenu0);
}

void sub1(void)
{
    domenu(SubMenu1);
}

void sub2(void)
{
    domenu(SubMenu2);
}

void sub3(void)
{
    domenu(SubMenu3);
}

/***************************** submenu1 functions *************************/

void func1(void)
{
    beep();
    bodymsg("One beep! ");
}

void func2(void)
{
    beep();
    bodymsg("Two beeps! ");
    beep();
}

/***************************** submenu2 functions *************************/

void subfunc1(void)
{
    showfile(FNAME);
}

void subfunc2(void)
{
    char fname[MAXSTRLEN];

    strcpy(fname, FNAME);
    if (getfname ("File to browse:", fname, 50))
        showfile(fname);
}

/***************************** submenu3 functions *************************/

void subsub(void)
{
    domenu(SubMenu2);
}

/***************************** start main menu  ***************************/

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    startmenu(MainMenu, "TUI - 'textual user interface' demonstration program");

    return 0;
}
