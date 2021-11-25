/* Public Domain Curses */

#include "pdcx11.h"

#ifdef HAVE_DECKEYSYM_H
# include <DECkeysym.h>
#endif

#ifdef HAVE_SUNKEYSYM_H
# include <Sunkeysym.h>
#endif

#ifdef HAVE_XPM_H
# include <xpm.h>
#endif

#if defined PDC_XIM
# include <Xlocale.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifndef XPOINTER_TYPEDEFED
typedef char * XPointer;
#endif

#ifndef MAX_PATH
# define MAX_PATH 256
#endif

XCursesAppData xc_app_data;

#if NeedWidePrototypes
# define PDC_SCROLLBAR_TYPE double
#else
# define PDC_SCROLLBAR_TYPE float
#endif

#define MAX_COLORS   256            /* maximum of "normal" colors */
#define COLOR_CURSOR MAX_COLORS     /* color of cursor */
#define COLOR_BORDER MAX_COLORS + 1 /* color of border */

#define XCURSESDISPLAY (XtDisplay(drawing))
#define XCURSESWIN     (XtWindow(drawing))

/* Default icons for XCurses applications.  */

#include "big_icon.xbm"
#include "little_icon.xbm"

static void _selection_off(void);
static void _display_cursor(int, int, int, int);
static void _redraw_cursor(void);
static void _exit_process(int, int, char *);
static void _send_key_to_curses(unsigned long, MOUSE_STATUS *, bool);

static void XCursesButton(Widget, XEvent *, String *, Cardinal *);
static void XCursesHandleString(Widget, XEvent *, String *, Cardinal *);
static void XCursesKeyPress(Widget, XEvent *, String *, Cardinal *);
static void XCursesPasteSelection(Widget, XButtonEvent *);

static struct
{
    KeySym keycode;
    bool numkeypad;
    unsigned short normal;
    unsigned short shifted;
    unsigned short control;
    unsigned short alt;
} key_table[] =
{
/* keycode      keypad  normal       shifted       control      alt*/
 {XK_Left,      FALSE,  KEY_LEFT,    KEY_SLEFT,    CTL_LEFT,    ALT_LEFT},
 {XK_Right,     FALSE,  KEY_RIGHT,   KEY_SRIGHT,   CTL_RIGHT,   ALT_RIGHT},
 {XK_Up,        FALSE,  KEY_UP,      KEY_SUP,      CTL_UP,      ALT_UP},
 {XK_Down,      FALSE,  KEY_DOWN,    KEY_SDOWN,    CTL_DOWN,    ALT_DOWN},
 {XK_Home,      FALSE,  KEY_HOME,    KEY_SHOME,    CTL_HOME,    ALT_HOME},
/* Sun Type 4 keyboard */
 {XK_R7,        FALSE,  KEY_HOME,    KEY_SHOME,    CTL_HOME,    ALT_HOME},
 {XK_End,       FALSE,  KEY_END,     KEY_SEND,     CTL_END,     ALT_END},
/* Sun Type 4 keyboard */
 {XK_R13,       FALSE,  KEY_END,     KEY_SEND,     CTL_END,     ALT_END},
 {XK_Prior,     FALSE,  KEY_PPAGE,   KEY_SPREVIOUS,CTL_PGUP,    ALT_PGUP},
/* Sun Type 4 keyboard */
 {XK_R9,        FALSE,  KEY_PPAGE,   KEY_SPREVIOUS,CTL_PGUP,    ALT_PGUP},
 {XK_Next,      FALSE,  KEY_NPAGE,   KEY_SNEXT,    CTL_PGDN,    ALT_PGDN},
/* Sun Type 4 keyboard */
 {XK_R15,       FALSE,  KEY_NPAGE,   KEY_SNEXT,    CTL_PGDN,    ALT_PGDN},
 {XK_Insert,    FALSE,  KEY_IC,      KEY_SIC,      CTL_INS,     ALT_INS},
 {XK_Delete,    FALSE,  KEY_DC,      KEY_SDC,      CTL_DEL,     ALT_DEL},
 {XK_F1,        FALSE,  KEY_F(1),    KEY_F(13),    KEY_F(25),   KEY_F(37)},
 {XK_F2,        FALSE,  KEY_F(2),    KEY_F(14),    KEY_F(26),   KEY_F(38)},
 {XK_F3,        FALSE,  KEY_F(3),    KEY_F(15),    KEY_F(27),   KEY_F(39)},
 {XK_F4,        FALSE,  KEY_F(4),    KEY_F(16),    KEY_F(28),   KEY_F(40)},
 {XK_F5,        FALSE,  KEY_F(5),    KEY_F(17),    KEY_F(29),   KEY_F(41)},
 {XK_F6,        FALSE,  KEY_F(6),    KEY_F(18),    KEY_F(30),   KEY_F(42)},
 {XK_F7,        FALSE,  KEY_F(7),    KEY_F(19),    KEY_F(31),   KEY_F(43)},
 {XK_F8,        FALSE,  KEY_F(8),    KEY_F(20),    KEY_F(32),   KEY_F(44)},
 {XK_F9,        FALSE,  KEY_F(9),    KEY_F(21),    KEY_F(33),   KEY_F(45)},
 {XK_F10,       FALSE,  KEY_F(10),   KEY_F(22),    KEY_F(34),   KEY_F(46)},
 {XK_F11,       FALSE,  KEY_F(11),   KEY_F(23),    KEY_F(35),   KEY_F(47)},
 {XK_F12,       FALSE,  KEY_F(12),   KEY_F(24),    KEY_F(36),   KEY_F(48)},
 {XK_F13,       FALSE,  KEY_F(13),   KEY_F(25),    KEY_F(37),   KEY_F(49)},
 {XK_F14,       FALSE,  KEY_F(14),   KEY_F(26),    KEY_F(38),   KEY_F(50)},
 {XK_F15,       FALSE,  KEY_F(15),   KEY_F(27),    KEY_F(39),   KEY_F(51)},
 {XK_F16,       FALSE,  KEY_F(16),   KEY_F(28),    KEY_F(40),   KEY_F(52)},
 {XK_F17,       FALSE,  KEY_F(17),   KEY_F(29),    KEY_F(41),   KEY_F(53)},
 {XK_F18,       FALSE,  KEY_F(18),   KEY_F(30),    KEY_F(42),   KEY_F(54)},
 {XK_F19,       FALSE,  KEY_F(19),   KEY_F(31),    KEY_F(43),   KEY_F(55)},
 {XK_F20,       FALSE,  KEY_F(20),   KEY_F(32),    KEY_F(44),   KEY_F(56)},
 {XK_BackSpace, FALSE,  0x08,        0x08,         CTL_BKSP,    ALT_BKSP},
 {XK_Tab,       FALSE,  0x09,        KEY_BTAB,     CTL_TAB,     ALT_TAB},
#if defined(XK_ISO_Left_Tab)
 {XK_ISO_Left_Tab, FALSE, 0x09,      KEY_BTAB,     CTL_TAB,     ALT_TAB},
#endif
 {XK_Select,    FALSE,  KEY_SELECT,  KEY_SELECT,   KEY_SELECT,  KEY_SELECT},
 {XK_Print,     FALSE,  KEY_PRINT,   KEY_SPRINT,   KEY_PRINT,   KEY_PRINT},
 {XK_Find,      FALSE,  KEY_FIND,    KEY_SFIND,    KEY_FIND,    KEY_FIND},
 {XK_Pause,     FALSE,  KEY_SUSPEND, KEY_SSUSPEND, KEY_SUSPEND, KEY_SUSPEND},
 {XK_Clear,     FALSE,  KEY_CLEAR,   KEY_CLEAR,    KEY_CLEAR,   KEY_CLEAR},
 {XK_Cancel,    FALSE,  KEY_CANCEL,  KEY_SCANCEL,  KEY_CANCEL,  KEY_CANCEL},
 {XK_Break,     FALSE,  KEY_BREAK,   KEY_BREAK,    KEY_BREAK,   KEY_BREAK},
 {XK_Help,      FALSE,  KEY_HELP,    KEY_SHELP,    KEY_LHELP,   KEY_HELP},
 {XK_L4,        FALSE,  KEY_UNDO,    KEY_SUNDO,    KEY_UNDO,    KEY_UNDO},
 {XK_L6,        FALSE,  KEY_COPY,    KEY_SCOPY,    KEY_COPY,    KEY_COPY},
 {XK_L9,        FALSE,  KEY_FIND,    KEY_SFIND,    KEY_FIND,    KEY_FIND},
 {XK_Menu,      FALSE,  KEY_OPTIONS, KEY_SOPTIONS, KEY_OPTIONS, KEY_OPTIONS},
 {XK_Super_R,   FALSE,  KEY_COMMAND, KEY_SCOMMAND, KEY_COMMAND, KEY_COMMAND},
 {XK_Super_L,   FALSE,  KEY_COMMAND, KEY_SCOMMAND, KEY_COMMAND, KEY_COMMAND},
#ifdef HAVE_SUNKEYSYM_H
 {SunXK_F36,    FALSE,  KEY_F(41),   KEY_F(43),    KEY_F(45),   KEY_F(47)},
 {SunXK_F37,    FALSE,  KEY_F(42),   KEY_F(44),    KEY_F(46),   KEY_F(48)},
#endif
#ifdef HAVE_DECKEYSYM_H
 {DXK_Remove,   FALSE,  KEY_DC,      KEY_SDC,      CTL_DEL,     ALT_DEL},
#endif
 {XK_Escape,    FALSE,  0x1B,        0x1B,         0x1B,        ALT_ESC},
 {XK_KP_Enter,  TRUE,   PADENTER,    PADENTER,     CTL_PADENTER,ALT_PADENTER},
 {XK_KP_Add,    TRUE,   PADPLUS,     '+',          CTL_PADPLUS, ALT_PADPLUS},
 {XK_KP_Subtract,TRUE,  PADMINUS,    '-',          CTL_PADMINUS,ALT_PADMINUS},
 {XK_KP_Multiply,TRUE,  PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
/* Sun Type 4 keyboard */
 {XK_R6,        TRUE,   PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
 {XK_KP_Divide, TRUE,   PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
/* Sun Type 4 keyboard */
 {XK_R5,        TRUE,   PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
 {XK_KP_Decimal,TRUE,   PADSTOP,     '.',          CTL_PADSTOP, ALT_PADSTOP},
 {XK_KP_0,      TRUE,   PAD0,        '0',          CTL_PAD0,    ALT_PAD0},
 {XK_KP_1,      TRUE,   KEY_C1,      '1',          CTL_PAD1,    ALT_PAD1},
 {XK_KP_2,      TRUE,   KEY_C2,      '2',          CTL_PAD2,    ALT_PAD2},
 {XK_KP_3,      TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
 {XK_KP_4,      TRUE,   KEY_B1,      '4',          CTL_PAD4,    ALT_PAD4},
 {XK_KP_5,      TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
/* Sun Type 4 keyboard */
 {XK_R11,       TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
 {XK_KP_6,      TRUE,   KEY_B3,      '6',          CTL_PAD6,    ALT_PAD6},
 {XK_KP_7,      TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
 {XK_KP_8,      TRUE,   KEY_A2,      '8',          CTL_PAD8,    ALT_PAD8},
 {XK_KP_9,      TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
/* the following added to support Sun Type 5 keyboards */
 {XK_F21,       FALSE,  KEY_SUSPEND, KEY_SSUSPEND, KEY_SUSPEND, KEY_SUSPEND},
 {XK_F22,       FALSE,  KEY_PRINT,   KEY_SPRINT,   KEY_PRINT,   KEY_PRINT},
 {XK_F24,       TRUE,   PADMINUS,    '-',          CTL_PADMINUS,ALT_PADMINUS},
/* Sun Type 4 keyboard */
 {XK_F25,       TRUE,   PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
/* Sun Type 4 keyboard */
 {XK_F26,       TRUE,   PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
 {XK_F27,       TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
 {XK_F29,       TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
 {XK_F31,       TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
 {XK_F35,       TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
#ifdef XK_KP_Delete
 {XK_KP_Delete, TRUE,   PADSTOP,     '.',          CTL_PADSTOP, ALT_PADSTOP},
#endif
#ifdef XK_KP_Insert
 {XK_KP_Insert, TRUE,   PAD0,        '0',          CTL_PAD0,    ALT_PAD0},
#endif
#ifdef XK_KP_End
 {XK_KP_End,    TRUE,   KEY_C1,      '1',          CTL_PAD1,    ALT_PAD1},
#endif
#ifdef XK_KP_Down
 {XK_KP_Down,   TRUE,   KEY_C2,      '2',          CTL_PAD2,    ALT_PAD2},
#endif
#ifdef XK_KP_Next
 {XK_KP_Next,   TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
#endif
#ifdef XK_KP_Left
 {XK_KP_Left,   TRUE,   KEY_B1,      '4',          CTL_PAD4,    ALT_PAD4},
#endif
#ifdef XK_KP_Begin
 {XK_KP_Begin,  TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
#endif
#ifdef XK_KP_Right
 {XK_KP_Right,  TRUE,   KEY_B3,      '6',          CTL_PAD6,    ALT_PAD6},
#endif
#ifdef XK_KP_Home
 {XK_KP_Home,   TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
#endif
#ifdef XK_KP_Up
 {XK_KP_Up,     TRUE,   KEY_A2,      '8',          CTL_PAD8,    ALT_PAD8},
#endif
#ifdef XK_KP_Prior
 {XK_KP_Prior,  TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
#endif
 {0,            0,      0,           0,            0,           0}
};

#ifndef PDC_XIM
# include "compose.h"
#endif

#define BITMAPDEPTH 1

unsigned long pdc_key_modifiers = 0L;

static GC normal_gc, rect_cursor_gc, italic_gc, bold_gc, border_gc;
static int font_height, font_width, font_ascent, font_descent,
           window_width, window_height;
static int resize_window_width = 0, resize_window_height = 0;
static char *bitmap_file = NULL;
#ifdef HAVE_XPM_H
static char *pixmap_file = NULL;
#endif
static KeySym keysym = 0;

static int state_mask[8] =
{
    ShiftMask,
    LockMask,
    ControlMask,
    Mod1Mask,
    Mod2Mask,
    Mod3Mask,
    Mod4Mask,
    Mod5Mask
};

static Atom wm_atom[2];
static String class_name = "XCurses";
static XtAppContext app_context;
static Widget topLevel, drawing, scrollBox, scrollVert, scrollHoriz;
static int received_map_notify = 0;
static bool mouse_selection = FALSE;
static chtype *tmpsel = NULL;
static unsigned long tmpsel_length = 0;
static int selection_start_x = 0, selection_start_y = 0,
           selection_end_x = 0, selection_end_y = 0;
static Pixmap icon_bitmap;
#ifdef HAVE_XPM_H
static Pixmap icon_pixmap;
static Pixmap icon_pixmap_mask;
#endif
static bool visible_cursor = FALSE;
static bool window_entered = TRUE;
static char *program_name;
static bool blinked_off;

/* Macros just for app_resources */

#ifdef PDC_WIDE
# define DEFNFONT "-misc-fixed-medium-r-normal--20-200-75-75-c-100-iso10646-1"
# define DEFIFONT "-misc-fixed-medium-o-normal--20-200-75-75-c-100-iso10646-1"
# define DEFBFONT "-misc-fixed-bold-r-normal--20-200-75-75-c-100-iso10646-1"
#else
# define DEFNFONT "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1"
# define DEFIFONT "-misc-fixed-medium-o-normal--13-120-75-75-c-70-iso8859-1"
# define DEFBFONT "-misc-fixed-bold-r-normal--13-120-75-75-c-70-iso8859-1"
#endif

#define APPDATAOFF(n) XtOffsetOf(XCursesAppData, n)

#define RINT(name1, name2, value) { \
                #name1, #name2, XtRInt, \
                sizeof(int), APPDATAOFF(name1), XtRImmediate, \
                (XtPointer)value \
        }

#define RPIXEL(name1, name2, value) { \
                #name1, #name2, XtRPixel, \
                sizeof(Pixel), APPDATAOFF(name1), XtRString, \
                (XtPointer)#value \
        }

#define RCOLOR(name, value) RPIXEL(color##name, Color##name, value)


#define RSTRINGP(name1, name2, param) { \
                #name1, #name2, XtRString, \
                MAX_PATH, APPDATAOFF(name1), XtRString, (XtPointer)param \
        }

#define RSTRING(name1, name2) RSTRINGP(name1, name2, "")

#define RFONT(name1, name2, value) { \
                #name1, #name2, XtRFontStruct, \
                sizeof(XFontStruct), APPDATAOFF(name1), XtRString, \
                (XtPointer)value \
        }

#define RCURSOR(name1, name2, value) { \
                #name1, #name2, XtRCursor, \
                sizeof(Cursor), APPDATAOFF(name1), XtRString, \
                (XtPointer)#value \
        }

static XtResource app_resources[] =
{
    RINT(lines, Lines, 24),
    RINT(cols, Cols, 80),

    RPIXEL(cursorColor, CursorColor, Red),

    RCOLOR(Black, Black),
    RCOLOR(Red, red3),
    RCOLOR(Green, green3),
    RCOLOR(Yellow, yellow3),
    RCOLOR(Blue, blue3),
    RCOLOR(Magenta, magenta3),
    RCOLOR(Cyan, cyan3),
    RCOLOR(White, Grey),

    RCOLOR(BoldBlack, grey40),
    RCOLOR(BoldRed, red1),
    RCOLOR(BoldGreen, green1),
    RCOLOR(BoldYellow, yellow1),
    RCOLOR(BoldBlue, blue1),
    RCOLOR(BoldMagenta, magenta1),
    RCOLOR(BoldCyan, cyan1),
    RCOLOR(BoldWhite, White),

    RFONT(normalFont, NormalFont, DEFNFONT),
    RFONT(italicFont, ItalicFont, DEFIFONT),
    RFONT(boldFont, BoldFont, DEFBFONT),

    RSTRING(bitmap, Bitmap),
#ifdef HAVE_XPM_H
    RSTRING(pixmap, Pixmap),
#endif
    RSTRINGP(composeKey, ComposeKey, "Multi_key"),

    RCURSOR(pointer, Pointer, xterm),

    RPIXEL(pointerForeColor, PointerForeColor, Black),
    RPIXEL(pointerBackColor, PointerBackColor, White),

    RINT(shmmin, Shmmin, 0),
    RINT(borderWidth, BorderWidth, 0),

    RPIXEL(borderColor, BorderColor, Black),

    RINT(doubleClickPeriod, DoubleClickPeriod, (PDC_CLICK_PERIOD * 2)),
    RINT(clickPeriod, ClickPeriod, PDC_CLICK_PERIOD),
    RINT(scrollbarWidth, ScrollbarWidth, 15),
    RINT(cursorBlinkRate, CursorBlinkRate, 0),

    RSTRING(textCursor, TextCursor),
    RINT(textBlinkRate, TextBlinkRate, 500)
};

#undef RCURSOR
#undef RFONT
#undef RSTRING
#undef RCOLOR
#undef RPIXEL
#undef RINT
#undef APPDATAOFF
#undef DEFBFONT
#undef DEFIFONT
#undef DEFNFONT

/* Macros for options */

#define COPT(name) {"-" #name, "*" #name, XrmoptionSepArg, NULL}
#define CCOLOR(name) COPT(color##name)

static XrmOptionDescRec options[] =
{
    COPT(lines), COPT(cols), COPT(normalFont), COPT(italicFont),
    COPT(boldFont), COPT(bitmap),
#ifdef HAVE_XPM_H
    COPT(pixmap),
#endif
    COPT(pointer), COPT(shmmin), COPT(composeKey), COPT(clickPeriod),
    COPT(doubleClickPeriod), COPT(scrollbarWidth),
    COPT(pointerForeColor), COPT(pointerBackColor),
    COPT(cursorBlinkRate), COPT(cursorColor), COPT(textCursor),
    COPT(textBlinkRate),

    CCOLOR(Black), CCOLOR(Red), CCOLOR(Green), CCOLOR(Yellow),
    CCOLOR(Blue), CCOLOR(Magenta), CCOLOR(Cyan), CCOLOR(White),

    CCOLOR(BoldBlack), CCOLOR(BoldRed), CCOLOR(BoldGreen),
    CCOLOR(BoldYellow), CCOLOR(BoldBlue), CCOLOR(BoldMagenta),
    CCOLOR(BoldCyan), CCOLOR(BoldWhite)
};

#undef CCOLOR
#undef COPT

static XtActionsRec action_table[] =
{
    {"XCursesButton",         (XtActionProc)XCursesButton},
    {"XCursesKeyPress",       (XtActionProc)XCursesKeyPress},
    {"XCursesPasteSelection", (XtActionProc)XCursesPasteSelection},
    {"string",                (XtActionProc)XCursesHandleString}
};

static bool after_first_curses_request = FALSE;
static Pixel colors[MAX_COLORS + 2];
static bool vertical_cursor = FALSE;

#ifdef PDC_XIM
static XIM Xim = NULL;
static XIC Xic = NULL;
#endif

static const char *default_translations =
{
    "<Key>: XCursesKeyPress() \n" \
    "<KeyUp>: XCursesKeyPress() \n" \
    "<BtnDown>: XCursesButton() \n" \
    "<BtnUp>: XCursesButton() \n" \
    "<BtnMotion>: XCursesButton()"
};

static int _to_utf8(char *outcode, chtype code)
{
#ifdef PDC_WIDE
    if (code & A_ALTCHARSET && !(code & 0xff80))
        code = acs_map[code & 0x7f];
#endif
    code &= A_CHARTEXT;

    if (code < 0x80)
    {
        outcode[0] = code;
        return 1;
    }
    else
        if (code < 0x800)
        {
            outcode[0] = ((code & 0x07c0) >> 6) | 0xc0;
            outcode[1] = (code & 0x003f) | 0x80;
            return 2;
        }
        else
        {
            outcode[0] = ((code & 0xf000) >> 12) | 0xe0;
            outcode[1] = ((code & 0x0fc0) >> 6) | 0x80;
            outcode[2] = (code & 0x003f) | 0x80;
            return 3;
        }
}

static int _from_utf8(wchar_t *pwc, const char *s, size_t n)
{
    wchar_t key;
    int i = -1;
    const unsigned char *string;

    if (!s || (n < 1))
        return -1;

    if (!*s)
        return 0;

    string = (const unsigned char *)s;

    key = string[0];

    /* Simplistic UTF-8 decoder -- only does the BMP, minimal validation */

    if (key & 0x80)
    {
        if ((key & 0xe0) == 0xc0)
        {
            if (1 < n)
            {
                key = ((key & 0x1f) << 6) | (string[1] & 0x3f);
                i = 2;
            }
        }
        else if ((key & 0xe0) == 0xe0)
        {
            if (2 < n)
            {
                key = ((key & 0x0f) << 12) |
                      ((string[1] & 0x3f) << 6) | (string[2] & 0x3f);
                i = 3;
            }
        }
    }
    else
        i = 1;

    if (i)
        *pwc = key;

    return i;
}

#ifndef X_HAVE_UTF8_STRING
static Atom XA_UTF8_STRING(Display *dpy)
{
    static AtomPtr p = NULL;

    if (!p)
        p = XmuMakeAtom("UTF8_STRING");

    return XmuInternAtom(dpy, p);
}
#endif

signal_handler XCursesSetSignal(int signo, signal_handler action)
{
#if defined(SA_INTERRUPT) || defined(SA_RESTART)
    struct sigaction sigact, osigact;

    sigact.sa_handler = action;

    sigact.sa_flags =
# ifdef SA_INTERRUPT
#  ifdef SA_RESTART
        SA_INTERRUPT | SA_RESTART;
#  else
        SA_INTERRUPT;
#  endif
# else  /* must be SA_RESTART */
        SA_RESTART;
# endif
    sigemptyset(&sigact.sa_mask);

    if (sigaction(signo, &sigact, &osigact))
        return SIG_ERR;

    return osigact.sa_handler;

#else   /* not SA_INTERRUPT or SA_RESTART, use plain signal */
    return signal(signo, action);
#endif
}

void XCursesSigwinchHandler(int signo)
{
    PDC_LOG(("%s:XCursesSigwinchHandler() - called: SIGNO: %d\n",
             XCLOGMSG, signo));

    /* Patch by: Georg Fuchs, georg.fuchs@rz.uni-regensburg.de
       02-Feb-1999 */

    SP->resized += 1;

    /* Always trap SIGWINCH if the C library supports SIGWINCH */

#ifdef SIGWINCH
    XCursesSetSignal(SIGWINCH, XCursesSigwinchHandler);
#endif
}

/* Convert character positions x and y to pixel positions, stored in
   xpos and ypos */

static void _make_xy(int x, int y, int *xpos, int *ypos)
{
    *xpos = (x * font_width) + xc_app_data.borderWidth;
    *ypos = xc_app_data.normalFont->ascent + (y * font_height) +
            xc_app_data.borderWidth;
}

/* Output a block of characters with common attributes */

static int _new_packet(chtype attr, bool rev, int len, int col, int row,
#ifdef PDC_WIDE
                       XChar2b *text)
#else
                       char *text)
#endif
{
    XRectangle bounds;
    GC gc;
    int xpos, ypos;
    short fore, back;
    attr_t sysattrs;

    PDC_pair_content(PAIR_NUMBER(attr), &fore, &back);

    /* Specify the color table offsets */

    sysattrs = SP->termattrs;

    if ((attr & A_BOLD) && !(sysattrs & A_BOLD))
        fore |= 8;
    if ((attr & A_BLINK) && !(sysattrs & A_BLINK))
        back |= 8;

    /* Reverse flag = highlighted selection XOR A_REVERSE set */

    rev ^= !!(attr & A_REVERSE);

    /* Determine which GC to use - normal, italic or bold */

    if ((attr & A_ITALIC) && (sysattrs & A_ITALIC))
        gc = italic_gc;
    else if ((attr & A_BOLD) && (sysattrs & A_BOLD))
        gc = bold_gc;
    else
        gc = normal_gc;

    _make_xy(col, row, &xpos, &ypos);

    bounds.x = xpos;
    bounds.y = ypos - font_ascent;
    bounds.width = font_width * len;
    bounds.height = font_height;

    XSetClipRectangles(XCURSESDISPLAY, gc, 0, 0, &bounds, 1, Unsorted);

    if (blinked_off && (sysattrs & A_BLINK) && (attr & A_BLINK))
    {
        XSetForeground(XCURSESDISPLAY, gc, colors[rev ? fore : back]);
        XFillRectangle(XCURSESDISPLAY, XCURSESWIN, gc, xpos, bounds.y,
                       bounds.width, font_height);
    }
    else
    {
        /* Draw it */

        XSetForeground(XCURSESDISPLAY, gc, colors[rev ? back : fore]);
        XSetBackground(XCURSESDISPLAY, gc, colors[rev ? fore : back]);

#ifdef PDC_WIDE
        XDrawImageString16(
#else
        XDrawImageString(
#endif
            XCURSESDISPLAY, XCURSESWIN, gc, xpos, ypos, text, len);

        /* Underline, etc. */

        if (attr & (A_LEFT | A_RIGHT | A_UNDERLINE))
        {
            int k;

            if (SP->line_color != -1)
                XSetForeground(XCURSESDISPLAY, gc, colors[SP->line_color]);

            if (attr & A_UNDERLINE)
                XDrawLine(XCURSESDISPLAY, XCURSESWIN, gc,
                          xpos, ypos + 1, xpos + font_width * len, ypos + 1);

            if (attr & A_LEFT)
                for (k = 0; k < len; k++)
                {
                    int x = xpos + font_width * k;
                    XDrawLine(XCURSESDISPLAY, XCURSESWIN, gc,
                              x, ypos - font_ascent, x, ypos + font_descent);
                }

            if (attr & A_RIGHT)
                for (k = 0; k < len; k++)
                {
                    int x = xpos + font_width * (k + 1) - 1;
                    XDrawLine(XCURSESDISPLAY, XCURSESWIN, gc,
                              x, ypos - font_ascent, x, ypos + font_descent);
                }
        }
    }

    PDC_LOG(("%s:_new_packet() - row: %d col: %d "
             "num_cols: %d fore: %d back: %d text:<%s>\n",
             XCLOGMSG, row, col, len, fore, back, text));

    return OK;
}

/* The core display routine -- update one line of text */

static int _display_text(const chtype *ch, int row, int col,
                         int num_cols, bool highlight)
{
#ifdef PDC_WIDE
    XChar2b text[513];
#else
    char text[513];
#endif
    chtype old_attr, attr;
    int i, j;

    PDC_LOG(("%s:_display_text() - called: row: %d col: %d "
             "num_cols: %d\n", XCLOGMSG, row, col, num_cols));

    if (!num_cols)
        return OK;

    old_attr = *ch & A_ATTRIBUTES;

    for (i = 0, j = 0; j < num_cols; j++)
    {
        chtype curr = ch[j];

        attr = curr & A_ATTRIBUTES;

#ifdef CHTYPE_LONG
        if (attr & A_ALTCHARSET && !(curr & 0xff80))
        {
            attr ^= A_ALTCHARSET;
            curr = acs_map[curr & 0x7f];
        }
#endif

#ifndef PDC_WIDE
        /* Special handling for ACS_BLOCK */

        if (!(curr & A_CHARTEXT))
        {
            curr |= ' ';
            attr ^= A_REVERSE;
        }
#endif
        if (attr != old_attr)
        {
            if (_new_packet(old_attr, highlight, i, col, row, text) == ERR)
                return ERR;

            old_attr = attr;
            col += i;
            i = 0;
        }

#ifdef PDC_WIDE
        text[i].byte1 = (curr & 0xff00) >> 8;
        text[i++].byte2 = curr & 0x00ff;
#else
        text[i++] = curr & 0xff;
#endif
    }

    return _new_packet(old_attr, highlight, i, col, row, text);
}

static void _get_gc(GC *gc, XFontStruct *font_info, int fore, int back)
{
    XGCValues values;

    /* Create default Graphics Context */

    *gc = XCreateGC(XCURSESDISPLAY, XCURSESWIN, 0L, &values);

    /* specify font */

    XSetFont(XCURSESDISPLAY, *gc, font_info->fid);

    XSetForeground(XCURSESDISPLAY, *gc, colors[fore]);
    XSetBackground(XCURSESDISPLAY, *gc, colors[back]);
}

static void _initialize_colors(void)
{
    int i, r, g, b;

    colors[COLOR_BLACK]   = xc_app_data.colorBlack;
    colors[COLOR_RED]     = xc_app_data.colorRed;
    colors[COLOR_GREEN]   = xc_app_data.colorGreen;
    colors[COLOR_YELLOW]  = xc_app_data.colorYellow;
    colors[COLOR_BLUE]    = xc_app_data.colorBlue;
    colors[COLOR_MAGENTA] = xc_app_data.colorMagenta;
    colors[COLOR_CYAN]    = xc_app_data.colorCyan;
    colors[COLOR_WHITE]   = xc_app_data.colorWhite;

    colors[COLOR_BLACK + 8]   = xc_app_data.colorBoldBlack;
    colors[COLOR_RED + 8]     = xc_app_data.colorBoldRed;
    colors[COLOR_GREEN + 8]   = xc_app_data.colorBoldGreen;
    colors[COLOR_YELLOW + 8]  = xc_app_data.colorBoldYellow;
    colors[COLOR_BLUE + 8]    = xc_app_data.colorBoldBlue;
    colors[COLOR_MAGENTA + 8] = xc_app_data.colorBoldMagenta;
    colors[COLOR_CYAN + 8]    = xc_app_data.colorBoldCyan;
    colors[COLOR_WHITE + 8]   = xc_app_data.colorBoldWhite;

#define RGB(R, G, B) ( ((unsigned long)(R) << 16) | \
                       ((unsigned long)(G) << 8) | \
                       ((unsigned long)(B)) )

    /* 256-color xterm extended palette: 216 colors in a 6x6x6 color
       cube, plus 24 shades of gray */

    for (i = 16, r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++)
                colors[i++] = RGB(r ? r * 40 + 55 : 0,
                                  g ? g * 40 + 55 : 0,
                                  b ? b * 40 + 55 : 0);
    for (i = 0; i < 24; i++)
        colors[i + 232] = RGB(i * 10 + 8, i * 10 + 8, i * 10 + 8);

#undef RGB

    colors[COLOR_CURSOR] = xc_app_data.cursorColor;
    colors[COLOR_BORDER] = xc_app_data.borderColor;
}

static void _refresh_scrollbar(void)
{
    XC_LOG(("_refresh_scrollbar() - called\n"));

    if (SP->sb_on)
    {
        PDC_SCROLLBAR_TYPE total_y = SP->sb_total_y;
        PDC_SCROLLBAR_TYPE total_x = SP->sb_total_x;

        if (total_y)
            XawScrollbarSetThumb(scrollVert,
                (PDC_SCROLLBAR_TYPE)(SP->sb_cur_y) / total_y,
                (PDC_SCROLLBAR_TYPE)(SP->sb_viewport_y) / total_y);

        if (total_x)
            XawScrollbarSetThumb(scrollHoriz,
                (PDC_SCROLLBAR_TYPE)(SP->sb_cur_x) / total_x,
                (PDC_SCROLLBAR_TYPE)(SP->sb_viewport_x) / total_x);
    }
}

static void _set_cursor_color(chtype *ch, short *fore, short *back)
{
    int attr;
    short f, b;

    attr = PAIR_NUMBER(*ch);

    if (attr)
    {
        PDC_pair_content(attr, &f, &b);
        *fore = 7 - (f % 8);
        *back = 7 - (b % 8);
    }
    else
    {
        if (*ch & A_REVERSE)
        {
            *back = COLOR_BLACK;
            *fore = COLOR_WHITE;
        }
        else
        {
            *back = COLOR_WHITE;
            *fore = COLOR_BLACK;
        }
    }
}

static void _get_icon(void)
{
    XIconSize *icon_size;
    int size_count = 0;
    Status rc;
    unsigned char *bitmap_bits = NULL;
    unsigned icon_bitmap_width = 0, icon_bitmap_height = 0,
             file_bitmap_width = 0, file_bitmap_height = 0;

    XC_LOG(("_get_icon() - called\n"));

    icon_size = XAllocIconSize();

    rc = XGetIconSizes(XtDisplay(topLevel),
                       RootWindowOfScreen(XtScreen(topLevel)),
                       &icon_size, &size_count);

    /* if the WM can advise on icon sizes... */

    if (rc && size_count)
    {
        int i, max_height = 0, max_width = 0;

        PDC_LOG(("%s:size_count: %d rc: %d\n", XCLOGMSG, size_count, rc));

        for (i = 0; i < size_count; i++)
        {
            if (icon_size[i].max_width > max_width)
                max_width = icon_size[i].max_width;
            if (icon_size[i].max_height > max_height)
                max_height = icon_size[i].max_height;

            PDC_LOG(("%s:min: %d %d\n", XCLOGMSG,
                     icon_size[i].min_width, icon_size[i].min_height));

            PDC_LOG(("%s:max: %d %d\n", XCLOGMSG,
                     icon_size[i].max_width, icon_size[i].max_height));

            PDC_LOG(("%s:inc: %d %d\n", XCLOGMSG,
                     icon_size[i].width_inc, icon_size[i].height_inc));
        }

        if (max_width >= big_icon_width && max_height >= big_icon_height)
        {
            icon_bitmap_width = big_icon_width;
            icon_bitmap_height = big_icon_height;
            bitmap_bits = (unsigned char *)big_icon_bits;
        }
        else
        {
            icon_bitmap_width = little_icon_width;
            icon_bitmap_height = little_icon_height;
            bitmap_bits = (unsigned char *)little_icon_bits;
        }

    }
    else  /* use small icon */
    {
        icon_bitmap_width = little_icon_width;
        icon_bitmap_height = little_icon_height;
        bitmap_bits = (unsigned char *)little_icon_bits;
    }

    XFree(icon_size);

#ifdef HAVE_XPM_H
    if (xc_app_data.pixmap && xc_app_data.pixmap[0]) /* supplied pixmap */
    {
        XpmReadFileToPixmap(XtDisplay(topLevel),
                            RootWindowOfScreen(XtScreen(topLevel)),
                            (char *)xc_app_data.pixmap,
                            &icon_pixmap, &icon_pixmap_mask, NULL);
        return;
    }
#endif

    if (xc_app_data.bitmap && xc_app_data.bitmap[0]) /* supplied bitmap */
    {
        int x_hot = 0, y_hot = 0;

        rc = XReadBitmapFile(XtDisplay(topLevel),
                             RootWindowOfScreen(XtScreen(topLevel)),
                             (char *)xc_app_data.bitmap,
                             &file_bitmap_width, &file_bitmap_height,
                             &icon_bitmap, &x_hot, &y_hot);

        switch(rc)
        {
        case BitmapOpenFailed:
            fprintf(stderr, "bitmap file %s: not found\n",
                    xc_app_data.bitmap);
            break;
        case BitmapFileInvalid:
            fprintf(stderr, "bitmap file %s: contents invalid\n",
                    xc_app_data.bitmap);
            break;
        default:
            return;
        }
    }

    icon_bitmap = XCreateBitmapFromData(XtDisplay(topLevel),
        RootWindowOfScreen(XtScreen(topLevel)),
        (char *)bitmap_bits, icon_bitmap_width, icon_bitmap_height);
}

static void _draw_border(void)
{
    /* Draw the border if required */

    if (xc_app_data.borderWidth)
        XDrawRectangle(XCURSESDISPLAY, XCURSESWIN, border_gc,
                       xc_app_data.borderWidth / 2,
                       xc_app_data.borderWidth / 2,
                       window_width - xc_app_data.borderWidth,
                       window_height - xc_app_data.borderWidth);
}

/* Redraw the entire screen */

static void _display_screen(void)
{
    int row;

    XC_LOG(("_display_screen() - called\n"));

    for (row = 0; row < XCursesLINES; row++)
    {
        XC_get_line_lock(row);

        _display_text((const chtype *)(Xcurscr + XCURSCR_Y_OFF(row)),
                      row, 0, COLS, FALSE);

        XC_release_line_lock(row);
    }

    _redraw_cursor();
    _draw_border();
}

/* Draw changed portions of the screen */

static void _refresh_screen(void)
{
    int row, start_col, num_cols;

    XC_LOG(("_refresh_screen() - called\n"));

    for (row = 0; row < XCursesLINES; row++)
    {
        num_cols = (int)*(Xcurscr + XCURSCR_LENGTH_OFF + row);

        if (num_cols)
        {
            XC_get_line_lock(row);

            start_col = (int)*(Xcurscr + XCURSCR_START_OFF + row);

            _display_text((const chtype *)(Xcurscr + XCURSCR_Y_OFF(row) +
                          (start_col * sizeof(chtype))), row, start_col,
                          num_cols, FALSE);

            *(Xcurscr + XCURSCR_LENGTH_OFF + row) = 0;

            XC_release_line_lock(row);
        }
    }

    _selection_off();
}

static void _handle_expose(Widget w, XtPointer client_data, XEvent *event,
                           Boolean *unused)
{
    XC_LOG(("_handle_expose() - called\n"));

    /* ignore all Exposes except last */

    if (event->xexpose.count)
        return;

    if (after_first_curses_request && received_map_notify)
        _display_screen();
}

static void _handle_nonmaskable(Widget w, XtPointer client_data, XEvent *event,
                                Boolean *unused)
{
    XClientMessageEvent *client_event = (XClientMessageEvent *)event;

    PDC_LOG(("%s:_handle_nonmaskable called: xc_otherpid %d event %d\n",
             XCLOGMSG, xc_otherpid, event->type));

    if (event->type == ClientMessage)
    {
        XC_LOG(("ClientMessage received\n"));

        /* This code used to include handling of WM_SAVE_YOURSELF, but
           it resulted in continual failure of THE on my Toshiba laptop.
           Removed on 3-3-2001. Now only exits on WM_DELETE_WINDOW. */

        if ((Atom)client_event->data.s[0] == wm_atom[0])
            _exit_process(0, SIGKILL, "");
    }
}

static void XCursesKeyPress(Widget w, XEvent *event, String *params,
                            Cardinal *nparams)
{
    enum { STATE_NORMAL, STATE_COMPOSE, STATE_CHAR };

#ifdef PDC_XIM
    Status status;
    wchar_t buffer[120];
#else
    unsigned char buffer[120];
    XComposeStatus compose;
    static int compose_state = STATE_NORMAL;
    static int compose_index = 0;
    int char_idx = 0;
#endif
    unsigned long key = 0;
    int buflen = 40;
    int i, count;
    unsigned long modifier = 0;
    bool key_code = FALSE;

    XC_LOG(("XCursesKeyPress() - called\n"));

    /* Handle modifier keys first; ignore other KeyReleases */

    if (event->type == KeyRelease)
    {
        /* The keysym value was set by a previous call to this function
           with a KeyPress event (or reset by the mouse event handler) */

        if (SP->return_key_modifiers &&
#ifndef PDC_XIM
            keysym != compose_key &&
#endif
            IsModifierKey(keysym))
        {
            switch (keysym) {
            case XK_Shift_L:
                key = KEY_SHIFT_L;
                break;
            case XK_Shift_R:
                key = KEY_SHIFT_R;
                break;
            case XK_Control_L:
                key = KEY_CONTROL_L;
                break;
            case XK_Control_R:
                key = KEY_CONTROL_R;
                break;
            case XK_Alt_L:
                key = KEY_ALT_L;
                break;
            case XK_Alt_R:
                key = KEY_ALT_R;
            }

            if (key)
                _send_key_to_curses(key, NULL, TRUE);
        }

        return;
    }

    buffer[0] = '\0';

#ifdef PDC_XIM
    count = XwcLookupString(Xic, &(event->xkey), buffer, buflen,
                            &keysym, &status);
#else
    count = XLookupString(&(event->xkey), (char *)buffer, buflen,
                          &keysym, &compose);
#endif

    /* translate keysym into curses key code */

    PDC_LOG(("%s:Key mask: %x\n", XCLOGMSG, event->xkey.state));

#ifdef PDCDEBUG
    for (i = 0; i < 4; i++)
        PDC_debug("%s:Keysym %x %d\n", XCLOGMSG,
                  XKeycodeToKeysym(XCURSESDISPLAY, event->xkey.keycode, i), i);
#endif

#ifndef PDC_XIM

    /* Check if the key just pressed is the user-specified compose
       key; if it is, set the compose state and exit. */

    if (keysym == compose_key)
    {
        chtype *ch;
        int xpos, ypos, save_visibility = SP->visibility;
        short fore = 0, back = 0;

        /* Change the shape of the cursor to an outline rectangle to
           indicate we are in "compose" status */

        SP->visibility = 0;

        _redraw_cursor();

        SP->visibility = save_visibility;
        _make_xy(SP->curscol, SP->cursrow, &xpos, &ypos);

        ch = (chtype *)(Xcurscr + XCURSCR_Y_OFF(SP->cursrow) +
             (SP->curscol * sizeof(chtype)));

        _set_cursor_color(ch, &fore, &back);

        XSetForeground(XCURSESDISPLAY, rect_cursor_gc, colors[back]);

        XDrawRectangle(XCURSESDISPLAY, XCURSESWIN, rect_cursor_gc,
                       xpos + 1, ypos - font_height +
                       xc_app_data.normalFont->descent + 1,
                       font_width - 2, font_height - 2);

        compose_state = STATE_COMPOSE;
        return;
    }

    switch (compose_state)
    {
    case STATE_COMPOSE:
        if (IsModifierKey(keysym))
            return;

        if (event->xkey.state & compose_mask)
        {
            compose_state = STATE_NORMAL;
            _redraw_cursor();
            break;
        }

        if (buffer[0] && count == 1)
            key = buffer[0];

        compose_index = -1;

        for (i = 0; i < (int)strlen(compose_chars); i++)
            if (compose_chars[i] == key)
            {
                compose_index = i;
                break;
            }

        if (compose_index == -1)
        {
            compose_state = STATE_NORMAL;
            compose_index = 0;
            _redraw_cursor();
            break;
        }

        compose_state = STATE_CHAR;
        return;

    case STATE_CHAR:
        if (IsModifierKey(keysym))
            return;

        if (event->xkey.state & compose_mask)
        {
            compose_state = STATE_NORMAL;
            _redraw_cursor();
            break;
        }

        if (buffer[0] && count == 1)
            key = buffer[0];

        char_idx = -1;

        for (i = 0; i < MAX_COMPOSE_CHARS; i++)
            if (compose_lookups[compose_index][i] == key)
            {
                char_idx = i;
                break;
            }

        if (char_idx == -1)
        {
            compose_state = STATE_NORMAL;
            compose_index = 0;
            _redraw_cursor();
            break;
        }

        _send_key_to_curses(compose_keys[compose_index][char_idx],
            NULL, FALSE);

        compose_state = STATE_NORMAL;
        compose_index = 0;

        _redraw_cursor();

        return;
    }

#endif /* PDC_XIM */

    /* To get here we are procesing "normal" keys */

    PDC_LOG(("%s:Keysym %x %d\n", XCLOGMSG,
             XKeycodeToKeysym(XCURSESDISPLAY, event->xkey.keycode, key), key));

    if (SP->save_key_modifiers)
    {
        /* 0x10: usually, numlock modifier */

        if (event->xkey.state & Mod2Mask)
            modifier |= PDC_KEY_MODIFIER_NUMLOCK;

        /* 0x01: shift modifier */

        if (event->xkey.state & ShiftMask)
            modifier |= PDC_KEY_MODIFIER_SHIFT;

        /* 0x04: control modifier */

        if (event->xkey.state & ControlMask)
            modifier |= PDC_KEY_MODIFIER_CONTROL;

        /* 0x08: usually, alt modifier */

        if (event->xkey.state & Mod1Mask)
            modifier |= PDC_KEY_MODIFIER_ALT;
    }

    for (i = 0; key_table[i].keycode; i++)
    {
        if (key_table[i].keycode == keysym)
        {
            PDC_LOG(("%s:State %x\n", XCLOGMSG, event->xkey.state));

            /* ControlMask: 0x04: control modifier
               Mod1Mask: 0x08: usually, alt modifier
               Mod2Mask: 0x10: usually, numlock modifier
               ShiftMask: 0x01: shift modifier */

            if ((event->xkey.state & ShiftMask) ||
                (key_table[i].numkeypad &&
                (event->xkey.state & Mod2Mask)))
            {
                key = key_table[i].shifted;
            }
            else if (event->xkey.state & ControlMask)
            {
                key = key_table[i].control;
            }
            else if (event->xkey.state & Mod1Mask)
            {
                key = key_table[i].alt;
            }

            /* To get here, we ignore all other modifiers */

            else
                key = key_table[i].normal;

            key_code = (key > 0x100);
            break;
        }
    }

    if (!key && buffer[0] && count == 1)
        key = buffer[0];

    PDC_LOG(("%s:Key: %s pressed - %x Mod: %x\n", XCLOGMSG,
             XKeysymToString(keysym), key, event->xkey.state));

    /* Handle ALT letters and numbers */

    if (event->xkey.state & Mod1Mask)
    {
        if (key >= 'A' && key <= 'Z')
        {
            key += ALT_A - 'A';
            key_code = TRUE;
        }

        if (key >= 'a' && key <= 'z')
        {
            key += ALT_A - 'a';
            key_code = TRUE;
        }

        if (key >= '0' && key <= '9')
        {
            key += ALT_0 - '0';
            key_code = TRUE;
        }
    }

    /* After all that, send the key back to the application if is
       NOT zero. */

    if (key)
    {
        key |= (modifier << 24);

        _send_key_to_curses(key, NULL, key_code);
    }
}

static void XCursesHandleString(Widget w, XEvent *event, String *params,
                                Cardinal *nparams)
{
    unsigned char *ptr;

    if (*nparams != 1)
        return;

    ptr = (unsigned char *)*params;

    if (ptr[0] == '0' && ptr[1] == 'x' && ptr[2] != '\0')
    {
        unsigned char c;
        unsigned long total = 0;

        for (ptr += 2; (c = tolower(*ptr)); ptr++)
        {
            total <<= 4;

            if (c >= '0' && c <= '9')
                total += c - '0';
            else
                if (c >= 'a' && c <= 'f')
                    total += c - ('a' - 10);
                else
                    break;
        }

        if (c == '\0')
            _send_key_to_curses(total, NULL, FALSE);
    }
    else
        for (; *ptr; ptr++)
            _send_key_to_curses((unsigned long)*ptr, NULL, FALSE);
}

static void _paste_string(Widget w, XtPointer data, Atom *selection, Atom *type,
                          XtPointer value, unsigned long *length, int *format)
{
    unsigned long i, key;
    unsigned char *string = value;

    XC_LOG(("_paste_string() - called\n"));

    if (!*type || !*length || !string)
        return;

    for (i = 0; string[i] && (i < (*length)); i++)
    {
        key = string[i];

        if (key == 10)      /* new line - convert to ^M */
            key = 13;

        _send_key_to_curses(key, NULL, FALSE);
    }

    XtFree(value);
}

static void _paste_utf8(Widget w, XtPointer event, Atom *selection, Atom *type,
                        XtPointer value, unsigned long *length, int *format)
{
    wchar_t key;
    size_t i = 0, len;
    char *string = value;

    XC_LOG(("_paste_utf8() - called\n"));

    if (!*type || !*length)
    {
        XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, _paste_string,
                            event, ((XButtonEvent *)event)->time);
        return;
    }

    len = *length;

    if (!string)
        return;

    while (string[i] && (i < len))
    {
        int retval = _from_utf8(&key, string + i, len - i);

        if (retval < 1)
            return;

        if (key == 10)      /* new line - convert to ^M */
            key = 13;

        _send_key_to_curses(key, NULL, FALSE);

        i += retval;
    }

    XtFree(value);
}

static void XCursesPasteSelection(Widget w, XButtonEvent *button_event)
{
    XC_LOG(("XCursesPasteSelection() - called\n"));

    XtGetSelectionValue(w, XA_PRIMARY, XA_UTF8_STRING(XtDisplay(w)),
                        _paste_utf8, (XtPointer)button_event,
                        button_event->time);
}

static Boolean _convert_proc(Widget w, Atom *selection, Atom *target,
                             Atom *type_return, XtPointer *value_return,
                             unsigned long *length_return, int *format_return)
{
    XC_LOG(("_convert_proc() - called\n"));

    if (*target == XA_TARGETS(XtDisplay(topLevel)))
    {
        XSelectionRequestEvent *req = XtGetSelectionRequest(w,
            *selection, (XtRequestId)NULL);

        Atom *targetP;
        XPointer std_targets;
        unsigned long std_length;

        XmuConvertStandardSelection(topLevel, req->time, selection,
                                    target, type_return, &std_targets,
                                    &std_length, format_return);

        *length_return = std_length + 2;
        *value_return = XtMalloc(sizeof(Atom) * (*length_return));

        targetP = *(Atom**)value_return;
        *targetP++ = XA_STRING;
        *targetP++ = XA_UTF8_STRING(XtDisplay(topLevel));

        memmove((void *)targetP, (const void *)std_targets,
                sizeof(Atom) * std_length);

        XtFree((char *)std_targets);
        *type_return = XA_ATOM;
        *format_return = sizeof(Atom) * 8;

        return True;
    }
    else if (*target == XA_UTF8_STRING(XtDisplay(topLevel)) ||
             *target == XA_STRING)
    {
        bool utf8 = !(*target == XA_STRING);
        char *data = XtMalloc(tmpsel_length * 3 + 1);
        chtype *tmp = tmpsel;
        int ret_length = 0;

        if (utf8)
        {
            while (*tmp)
                ret_length += _to_utf8(data + ret_length, *tmp++);
        }
        else
            while (*tmp)
                data[ret_length++] = *tmp++ & 0xff;

        data[ret_length++] = '\0';

        *value_return = data;
        *length_return = ret_length;
        *format_return = 8;
        *type_return = *target;

        return True;
    }
    else
        return XmuConvertStandardSelection(topLevel, CurrentTime,
            selection, target, type_return, (XPointer*)value_return,
            length_return, format_return);
}

static void _lose_ownership(Widget w, Atom *type)
{
    XC_LOG(("_lose_ownership() - called\n"));

    if (tmpsel)
        free(tmpsel);

    tmpsel = NULL;
    tmpsel_length = 0;
    _selection_off();
}

static void _show_selection(int start_x, int start_y, int end_x, int end_y,
                            bool highlight)
{
    int i, num_cols, start_col, row;

    PDC_LOG(("%s:_show_selection() - called StartX: %d StartY: %d "
             "EndX: %d EndY: %d Highlight: %d\n", XCLOGMSG,
             start_x, start_y, end_x, end_y, highlight));

    for (i = 0; i < end_y - start_y + 1; i++)
    {
        if (start_y == end_y)       /* only one line */
        {
            start_col = start_x;
            num_cols = end_x - start_x + 1;
            row = start_y;
        }
        else if (!i)            /* first line */
        {
            start_col = start_x;
            num_cols = COLS - start_x;
            row = start_y;
        }
        else if (start_y + i == end_y)  /* last line */
        {
            start_col = 0;
            num_cols = end_x + 1;
            row = end_y;
        }
        else                /* full line */
        {
            start_col = 0;
            num_cols = COLS;
            row = start_y + i;
        }

        XC_get_line_lock(row);

        _display_text((const chtype *)(Xcurscr + XCURSCR_Y_OFF(row) +
                      (start_col * sizeof(chtype))), row, start_col,
                      num_cols, highlight);

        XC_release_line_lock(row);
    }
}

static void _selection_off(void)
{
    XC_LOG(("_selection_off() - called\n"));

    if (mouse_selection)
    {
        _display_screen();

        selection_start_x = selection_start_y = selection_end_x =
            selection_end_y = 0;

        mouse_selection = FALSE;
    }
}

static void _selection_on(int x, int y)
{
    XC_LOG(("_selection_on() - called\n"));

    selection_start_x = selection_end_x = x;
    selection_start_y = selection_end_y = y;
}

static void _selection_extend(int x, int y)
{
    int temp, current_start, current_end, current_start_x,
        current_end_x, current_start_y, current_end_y, new_start,
        new_end, new_start_x, new_end_x, new_start_y, new_end_y;

    XC_LOG(("_selection_extend() - called\n"));

    mouse_selection = TRUE;

    /* convert x/y coordinates into start/stop */

    current_start = (selection_start_y * COLS) + selection_start_x;
    current_end = (selection_end_y * COLS) + selection_end_x;

    if (current_start > current_end)
    {
        current_start_x = selection_end_x;
        current_start_y = selection_end_y;
        current_end_x = selection_start_x;
        current_end_y = selection_start_y;
        temp = current_start;
        current_start = current_end;
        current_end = temp;
    }
    else
    {
        current_end_x = selection_end_x;
        current_end_y = selection_end_y;
        current_start_x = selection_start_x;
        current_start_y = selection_start_y;
    }

    /* Now we have the current selection as a linear expression.
       Convert the new position to a linear expression. */

    selection_end_x = x;
    selection_end_y = y;

    /* convert x/y coordinates into start/stop */

    new_start = (selection_start_y * COLS) + selection_start_x;
    new_end = (selection_end_y * COLS) + selection_end_x;

    if (new_start > new_end)
    {
        new_start_x = selection_end_x;
        new_start_y = selection_end_y;
        new_end_x = selection_start_x;
        new_end_y = selection_start_y;
        temp = new_start;
        new_start = new_end;
        new_end = temp;
    }
    else
    {
        new_end_x = selection_end_x;
        new_end_y = selection_end_y;
        new_start_x = selection_start_x;
        new_start_y = selection_start_y;
    }

    if (new_end > current_end)
        _show_selection(current_end_x, current_end_y, new_end_x,
                        new_end_y, TRUE);
    else if (new_end < current_end)
        _show_selection(new_end_x, new_end_y, current_end_x,
                        current_end_y, FALSE);
    else if (new_start < current_start)
        _show_selection(new_start_x, new_start_y, current_start_x,
                        current_start_y, TRUE);
    else if (new_start > current_start)
        _show_selection(current_start_x, current_start_y,
                        new_start_x, new_start_y, FALSE);
    else
        _show_selection(current_start_x, current_start_y,
                        new_start_x, new_start_y, TRUE);
}

static void _selection_set(void)
{
    int i, j, start, end, start_x, end_x, start_y, end_y, num_cols,
        start_col, row, num_chars, ch, last_nonblank, length, newlen;
    chtype *ptr = NULL;

    XC_LOG(("_selection_set() - called\n"));

    /* convert x/y coordinates into start/stop */

    start = (selection_start_y * COLS) + selection_start_x;
    end = (selection_end_y * COLS) + selection_end_x;

    if (start == end)
    {
        if (tmpsel)
            free(tmpsel);

        tmpsel = NULL;
        tmpsel_length = 0;

        return;
    }

    if (start > end)
    {
        start_x = selection_end_x;
        start_y = selection_end_y;
        end_x = selection_start_x;
        end_y = selection_start_y;
        length = start - end + 1;
    }
    else
    {
        end_x = selection_end_x;
        end_y = selection_end_y;
        start_x = selection_start_x;
        start_y = selection_start_y;
        length = end - start + 1;
    }

    newlen = length + end_y - start_y + 2;

    if (length > (int)tmpsel_length)
    {
        if (!tmpsel_length)
            tmpsel = malloc(newlen * sizeof(chtype));
        else
            tmpsel = realloc(tmpsel, newlen * sizeof(chtype));
    }

    if (!tmpsel)
    {
        tmpsel_length = 0;
        return;
    }

    tmpsel_length = length;
    num_chars = 0;

    for (i = 0; i < end_y - start_y + 1; i++)
    {

        if (start_y == end_y)       /* only one line */
        {
            start_col = start_x;
            num_cols = end_x - start_x + 1;
            row = start_y;
        }
        else if (!i)            /* first line */
        {
            start_col = start_x;
            num_cols = COLS - start_x;
            row = start_y;
        }
        else if (start_y + i == end_y)  /* last line */
        {
            start_col = 0;
            num_cols = end_x + 1;
            row = end_y;
        }
        else                /* full line */
        {
            start_col = 0;
            num_cols = COLS;
            row = start_y + i;
        }

        XC_get_line_lock(row);

        ptr = (chtype *)(Xcurscr + XCURSCR_Y_OFF(row) +
              start_col * sizeof(chtype));

        if (i < end_y - start_y)
        {
            last_nonblank = 0;

            for (j = 0; j < num_cols; j++)
            {
                ch = (int)(ptr[j] & A_CHARTEXT);
                if (ch != (int)' ')
                    last_nonblank = j;
            }
        }
        else
            last_nonblank = num_cols - 1;

        for (j = 0; j <= last_nonblank; j++)
            tmpsel[num_chars++] = ptr[j];

        XC_release_line_lock(row);

        if (i < end_y - start_y)
            tmpsel[num_chars++] = '\n';
    }

    tmpsel[num_chars] = '\0';
    tmpsel_length = num_chars;
}

static void _display_cursor(int old_row, int old_x, int new_row, int new_x)
{
    int xpos, ypos, i;
    chtype *ch;
    short fore = 0, back = 0;

    PDC_LOG(("%s:_display_cursor() - draw char at row: %d col %d\n",
             XCLOGMSG, old_row, old_x));

    /* if the cursor position is outside the boundary of the screen,
       ignore the request */

    if (old_row >= XCursesLINES || old_x >= COLS ||
        new_row >= XCursesLINES || new_x >= COLS)
        return;

    /* display the character at the current cursor position */

    PDC_LOG(("%s:_display_cursor() - draw char at row: %d col %d\n",
             XCLOGMSG, old_row, old_x));

    _display_text((const chtype *)(Xcurscr + (XCURSCR_Y_OFF(old_row) +
                  (old_x * sizeof(chtype)))), old_row, old_x, 1, FALSE);

    /* display the cursor at the new cursor position */

    if (!SP->visibility)
        return;     /* cursor not displayed, no more to do */

    _make_xy(new_x, new_row, &xpos, &ypos);

    ch = (chtype *)(Xcurscr + XCURSCR_Y_OFF(new_row) + new_x * sizeof(chtype));

    _set_cursor_color(ch, &fore, &back);

    if (vertical_cursor)
    {
        XSetForeground(XCURSESDISPLAY, rect_cursor_gc, colors[back]);

        for (i = 1; i <= SP->visibility; i++)
            XDrawLine(XCURSESDISPLAY, XCURSESWIN, rect_cursor_gc,
                      xpos + i, ypos - xc_app_data.normalFont->ascent,
                      xpos + i, ypos - xc_app_data.normalFont->ascent +
                      font_height - 1);
    }
    else
    {
        /* For block cursors, paint the block with invert. */

        int yp, yh;

        if (SP->visibility == 2)
        {
            yp = ypos - font_height + font_descent;
            yh = font_height;
        }
        else
        {
            yp = ypos - font_height / 4 + font_descent;
            yh = font_height / 4;
        }

        XSetFunction(XCURSESDISPLAY, rect_cursor_gc, GXinvert);
        XFillRectangle(XCURSESDISPLAY, XCURSESWIN, rect_cursor_gc,
            xpos, yp, font_width, yh);
    }

    PDC_LOG(("%s:_display_cursor() - draw cursor at row %d col %d\n",
             XCLOGMSG, new_row, new_x));
}

static void _redraw_cursor(void)
{
    _display_cursor(SP->cursrow, SP->curscol, SP->cursrow, SP->curscol);
}

static void _handle_enter_leave(Widget w, XtPointer client_data,
                                XEvent *event, Boolean *unused)
{
    XC_LOG(("_handle_enter_leave called\n"));

    switch(event->type)
    {
    case EnterNotify:
        XC_LOG(("EnterNotify received\n"));

        window_entered = TRUE;
        break;

    case LeaveNotify:
        XC_LOG(("LeaveNotify received\n"));

        window_entered = FALSE;

        /* Display the cursor so it stays on while the window is
           not current */

        _redraw_cursor();
        break;

    default:
        PDC_LOG(("%s:_handle_enter_leave - unknown event %d\n",
                 XCLOGMSG, event->type));
    }
}

static void _send_key_to_curses(unsigned long key, MOUSE_STATUS *ms,
                                bool key_code)
{
    PDC_LOG(("%s:_send_key_to_curses() - called: sending %d\n",
             XCLOGMSG, key));

    SP->key_code = key_code;

    if (XC_write_socket(xc_key_sock, &key, sizeof(unsigned long)) < 0)
        _exit_process(1, SIGKILL, "exiting from _send_key_to_curses");

    if (ms)
    {
        MOUSE_LOG(("%s:writing mouse stuff\n", XCLOGMSG));

        if (XC_write_socket(xc_key_sock, ms, sizeof(MOUSE_STATUS)) < 0)
            _exit_process(1, SIGKILL, "exiting from _send_key_to_curses");
    }
}

static void _blink_cursor(XtPointer unused, XtIntervalId *id)
{
    XC_LOG(("_blink_cursor() - called:\n"));

    if (window_entered)
    {
        if (visible_cursor)
        {
            /* Cursor currently ON, turn it off */

            int save_visibility = SP->visibility;
            SP->visibility = 0;
            _redraw_cursor();
            SP->visibility = save_visibility;
            visible_cursor = FALSE;
        }
        else
        {
            /* Cursor currently OFF, turn it on */

            _redraw_cursor();
            visible_cursor = TRUE;
        }
    }

    XtAppAddTimeOut(app_context, xc_app_data.cursorBlinkRate,
                    _blink_cursor, NULL);
}

static void _blink_text(XtPointer unused, XtIntervalId *id)
{
    int row;
    int j, k;
    chtype *ch;

    XC_LOG(("_blink_text() - called:\n"));

    blinked_off = !blinked_off;

    /* Redraw changed lines on the screen to match the blink state */

    for (row = 0; row < XCursesLINES; row++)
    {
        ch = (chtype *)(Xcurscr + XCURSCR_Y_OFF(row));

        for (j = 0; j < COLS; j++)
            if (ch[j] & A_BLINK)
            {
                k = j;
                while (ch[k] & A_BLINK && k < COLS)
                    k++;

                XC_get_line_lock(row);
                _display_text(ch + j, row, j, k - j, FALSE);
                XC_release_line_lock(row);

                j = k;
            }
    }

    _redraw_cursor();
    _draw_border();

    if ((SP->termattrs & A_BLINK) || !blinked_off)
        XtAppAddTimeOut(app_context, xc_app_data.textBlinkRate,
                        _blink_text, NULL);
}

static void XCursesButton(Widget w, XEvent *event, String *params,
                          Cardinal *nparams)
{
    int button_no;
    static int last_button_no = 0;
    static Time last_button_press_time = 0;
    MOUSE_STATUS save_mouse_status;
    bool send_key = TRUE;
    static bool remove_release;
    static bool handle_real_release;

    XC_LOG(("XCursesButton() - called\n"));

    keysym = 0; /* suppress any modifier key return */

    save_mouse_status = Mouse_status;
    button_no = event->xbutton.button;

    /* It appears that under X11R6 (at least on Linux), that an
       event_type of ButtonMotion does not include the mouse button in
       the event. The following code is designed to cater for this
       situation. */

    if (!button_no)
        button_no = last_button_no;

    last_button_no = button_no;

    Mouse_status.changes = 0;

    switch(event->type)
    {
    case ButtonPress:
        /* Handle button 4 and 5, which are normally mapped to the wheel
           mouse scroll up and down, and button 6 and 7, which are
           normally mapped to the wheel mouse scroll left and right */

        if (button_no >= 4 && button_no <= 7)
        {
            /* Send the KEY_MOUSE to curses program */

            memset(&Mouse_status, 0, sizeof(Mouse_status));

            switch(button_no)
            {
               case 4:
                  Mouse_status.changes = PDC_MOUSE_WHEEL_UP;
                  break;
               case 5:
                  Mouse_status.changes = PDC_MOUSE_WHEEL_DOWN;
                  break;
               case 6:
                  Mouse_status.changes = PDC_MOUSE_WHEEL_LEFT;
                  break;
               case 7:
                  Mouse_status.changes = PDC_MOUSE_WHEEL_RIGHT;
            }

            MOUSE_X_POS = MOUSE_Y_POS = -1;
            _send_key_to_curses(KEY_MOUSE, &Mouse_status, TRUE);
            remove_release = TRUE;

            return;
        }

        if (button_no == 2 &&
            (!SP->_trap_mbe || (event->xbutton.state & ShiftMask)))
        {
            XCursesPasteSelection(drawing, (XButtonEvent *)event);
            remove_release = TRUE;

            return;
        }

        remove_release = False;
        handle_real_release = False;

        MOUSE_LOG(("\nButtonPress\n"));

        if ((event->xbutton.time - last_button_press_time) <
            xc_app_data.doubleClickPeriod)
        {
            MOUSE_X_POS = save_mouse_status.x;
            MOUSE_Y_POS = save_mouse_status.y;
            BUTTON_STATUS(button_no) = BUTTON_DOUBLE_CLICKED;

            _selection_off();
            remove_release = True;
        }
        else
        {
            napms(SP->mouse_wait);
            event->type = ButtonRelease;
            XSendEvent(event->xbutton.display, event->xbutton.window,
                       True, 0, event);
            last_button_press_time = event->xbutton.time;

            return;
        }

        last_button_press_time = event->xbutton.time;
        break;

    case MotionNotify:
        MOUSE_LOG(("\nMotionNotify: y: %d x: %d Width: %d "
                   "Height: %d\n", event->xbutton.y, event->xbutton.x,
                   font_width, font_height));

        MOUSE_X_POS = (event->xbutton.x - xc_app_data.borderWidth) /
                      font_width;
        MOUSE_Y_POS = (event->xbutton.y - xc_app_data.borderWidth) /
                      font_height;

        if (button_no == 1 &&
            (!SP->_trap_mbe || (event->xbutton.state & ShiftMask)))
        {
            _selection_extend(MOUSE_X_POS, MOUSE_Y_POS);
            send_key = FALSE;
        }
        else
            _selection_off();

        /* Throw away mouse movements if they are in the same character
           position as the last mouse event, or if we are currently in
           the middle of a double click event. */

        if ((MOUSE_X_POS == save_mouse_status.x &&
             MOUSE_Y_POS == save_mouse_status.y) ||
             save_mouse_status.button[button_no - 1] == BUTTON_DOUBLE_CLICKED)
        {
            send_key = FALSE;
            break;
        }

        Mouse_status.changes |= PDC_MOUSE_MOVED;
        break;

    case ButtonRelease:
        if (remove_release)
        {
            MOUSE_LOG(("Release at: %ld - removed\n", event->xbutton.time));
            return;
        }
        else
        {
            MOUSE_X_POS = (event->xbutton.x - xc_app_data.borderWidth) /
                          font_width;
            MOUSE_Y_POS = (event->xbutton.y - xc_app_data.borderWidth) /
                          font_height;

            if (!handle_real_release)
            {
                if ((event->xbutton.time - last_button_press_time) <
                    SP->mouse_wait &&
                    (event->xbutton.time != last_button_press_time))
                {
                    /* The "real" release was shorter than usleep() time;
                       therefore generate a click event */

                    MOUSE_LOG(("Release at: %ld - click\n",
                               event->xbutton.time));

                    BUTTON_STATUS(button_no) = BUTTON_CLICKED;

                    if (button_no == 1 && mouse_selection &&
                        (!SP->_trap_mbe || (event->xbutton.state & ShiftMask)))
                    {
                        send_key = FALSE;

                        if (XtOwnSelection(topLevel, XA_PRIMARY,
                                           event->xbutton.time, _convert_proc,
                                           _lose_ownership, NULL) == False)
                            _selection_off();
                    }
                    else
                        _selection_off();

                    /* Ensure the "pseudo" release event is ignored */

                    remove_release = True;
                    handle_real_release = False;
                    break;
                }
                else
                {
                    /* Button release longer than usleep() time;
                       therefore generate a press and wait for the real
                       release to occur later. */

                    MOUSE_LOG(("Generated Release at: %ld - "
                               "press & release\n", event->xbutton.time));

                    BUTTON_STATUS(button_no) = BUTTON_PRESSED;

                    if (button_no == 1 &&
                        (!SP->_trap_mbe || (event->xbutton.state & ShiftMask)))
                    {
                        _selection_off();
                        _selection_on(MOUSE_X_POS, MOUSE_Y_POS);
                    }

                    handle_real_release = True;
                    break;
                }
            }
            else
            {
                MOUSE_LOG(("Release at: %ld - released\n",
                           event->xbutton.time));
            }
        }

        MOUSE_LOG(("\nButtonRelease\n"));

        BUTTON_STATUS(button_no) = BUTTON_RELEASED;

        if (button_no == 1 && mouse_selection &&
            (!SP->_trap_mbe || (event->xbutton.state & ShiftMask)))
        {
            send_key = FALSE;

            if (XtOwnSelection(topLevel, XA_PRIMARY,
                               event->xbutton.time, _convert_proc,
                               _lose_ownership, NULL) == False)
                _selection_off();

            _selection_set();
        }
        else
            _selection_off();

        break;
    }

    /* Set up the mouse status fields in preparation for sending */

    Mouse_status.changes |= 1 << (button_no - 1);

    if (Mouse_status.changes & PDC_MOUSE_MOVED &&
        BUTTON_STATUS(button_no) == BUTTON_PRESSED)
        BUTTON_STATUS(button_no) = BUTTON_MOVED;

    if (event->xbutton.state & ShiftMask)
        BUTTON_STATUS(button_no) |= BUTTON_SHIFT;
    if (event->xbutton.state & ControlMask)
        BUTTON_STATUS(button_no) |= BUTTON_CONTROL;
    if (event->xbutton.state & Mod1Mask)
        BUTTON_STATUS(button_no) |= BUTTON_ALT;

    /* If we are ignoring the event, or the mouse position is outside
       the bounds of the screen (because of the border), return here */

    MOUSE_LOG(("Button: %d x: %d y: %d Button status: %x "
        "Mouse status: %x\n", button_no, MOUSE_X_POS, MOUSE_Y_POS,
        BUTTON_STATUS(button_no), Mouse_status.changes));

    MOUSE_LOG(("Send: %d Button1: %x Button2: %x Button3: %x %d %d\n",
        send_key, BUTTON_STATUS(1), BUTTON_STATUS(2),
        BUTTON_STATUS(3), XCursesLINES, XCursesCOLS));

    if (!send_key || MOUSE_X_POS < 0 || MOUSE_X_POS >= XCursesCOLS ||
        MOUSE_Y_POS < 0 || MOUSE_Y_POS >= XCursesLINES)
        return;

    /* Send the KEY_MOUSE to curses program */

    _send_key_to_curses(KEY_MOUSE, &Mouse_status, TRUE);
}

static void _scroll_up_down(Widget w, XtPointer client_data,
                            XtPointer call_data)
{
    int pixels = (long) call_data;
    int total_y = SP->sb_total_y * font_height;
    int viewport_y = SP->sb_viewport_y * font_height;
    int cur_y = SP->sb_cur_y * font_height;

    /* When pixels is negative, right button pressed, move data down,
       thumb moves up.  Otherwise, left button pressed, pixels positive,
       move data up, thumb down. */

    cur_y += pixels;

    /* limit panning to size of overall */

    if (cur_y < 0)
        cur_y = 0;
    else
        if (cur_y > (total_y - viewport_y))
            cur_y = total_y - viewport_y;

    SP->sb_cur_y = cur_y / font_height;

    XawScrollbarSetThumb(w, (double)((double)cur_y / (double)total_y),
                         (double)((double)viewport_y / (double)total_y));

    /* Send a key: if pixels negative, send KEY_SCROLL_DOWN */

    _send_key_to_curses(KEY_SF, NULL, TRUE);
}

static void _scroll_left_right(Widget w, XtPointer client_data,
                               XtPointer call_data)
{
    int pixels = (long) call_data;
    int total_x = SP->sb_total_x * font_width;
    int viewport_x = SP->sb_viewport_x * font_width;
    int cur_x = SP->sb_cur_x * font_width;

    cur_x += pixels;

    /* limit panning to size of overall */

    if (cur_x < 0)
        cur_x = 0;
    else
        if (cur_x > (total_x - viewport_x))
            cur_x = total_x - viewport_x;

    SP->sb_cur_x = cur_x / font_width;

    XawScrollbarSetThumb(w, (double)((double)cur_x / (double)total_x),
                         (double)((double)viewport_x / (double)total_x));

    _send_key_to_curses(KEY_SR, NULL, TRUE);
}

static void _thumb_up_down(Widget w, XtPointer client_data,
                           XtPointer call_data)
{
    double percent = *(double *) call_data;
    double total_y = (double)SP->sb_total_y;
    double viewport_y = (double)SP->sb_viewport_y;
    int cur_y = SP->sb_cur_y;

    /* If the size of the viewport is > overall area simply return,
       as no scrolling is permitted. */

    if (SP->sb_viewport_y >= SP->sb_total_y)
        return;

    if ((SP->sb_cur_y = (int)((double)total_y * percent)) >=
        (total_y - viewport_y))
        SP->sb_cur_y = total_y - viewport_y;

    XawScrollbarSetThumb(w, (double)(cur_y / total_y),
                         (double)(viewport_y / total_y));

    _send_key_to_curses(KEY_SF, NULL, TRUE);
}

static void _thumb_left_right(Widget w, XtPointer client_data,
                  XtPointer call_data)
{
    double percent = *(double *) call_data;
    double total_x = (double)SP->sb_total_x;
    double viewport_x = (double)SP->sb_viewport_x;
    int cur_x = SP->sb_cur_x;

    if (SP->sb_viewport_x >= SP->sb_total_x)
        return;

    if ((SP->sb_cur_x = (int)((float)total_x * percent)) >=
        (total_x - viewport_x))
        SP->sb_cur_x = total_x - viewport_x;

    XawScrollbarSetThumb(w, (double)(cur_x / total_x),
                         (double)(viewport_x / total_x));

    _send_key_to_curses(KEY_SR, NULL, TRUE);
}

static void _exit_process(int rc, int sig, char *msg)
{
    if (rc || sig)
        fprintf(stderr, "%s:_exit_process() - called: rc:%d sig:%d <%s>\n",
                XCLOGMSG, rc, sig, msg);

    shmdt((char *)SP);
    shmdt((char *)Xcurscr);
    shmctl(shmidSP, IPC_RMID, 0);
    shmctl(shmid_Xcurscr, IPC_RMID, 0);

    if (bitmap_file)
    {
        XFreePixmap(XCURSESDISPLAY, icon_bitmap);
        free(bitmap_file);
    }

#ifdef HAVE_XPM_H
    if (pixmap_file)
    {
        XFreePixmap(XCURSESDISPLAY, icon_pixmap);
        XFreePixmap(XCURSESDISPLAY, icon_pixmap_mask);
        free(pixmap_file);
    }
#endif
    XFreeGC(XCURSESDISPLAY, normal_gc);
    XFreeGC(XCURSESDISPLAY, italic_gc);
    XFreeGC(XCURSESDISPLAY, bold_gc);
    XFreeGC(XCURSESDISPLAY, rect_cursor_gc);
    XFreeGC(XCURSESDISPLAY, border_gc);
#ifdef PDC_XIM
    XDestroyIC(Xic);
#endif

    shutdown(xc_display_sock, 2);
    close(xc_display_sock);

    shutdown(xc_exit_sock, 2);
    close(xc_exit_sock);

    shutdown(xc_key_sock, 2);
    close(xc_key_sock);

    if (sig)
        kill(xc_otherpid, sig); /* to kill parent process */

    _exit(rc);
}

static void _resize(void)
{
    short save_atrtab[PDC_COLOR_PAIRS * 2];

    after_first_curses_request = FALSE;

    SP->lines = XCursesLINES = ((resize_window_height -
        (2 * xc_app_data.borderWidth)) / font_height);

    LINES = XCursesLINES - SP->linesrippedoff - SP->slklines;

    SP->cols = COLS = XCursesCOLS = ((resize_window_width -
        (2 * xc_app_data.borderWidth)) / font_width);

    window_width = resize_window_width;
    window_height = resize_window_height;
    visible_cursor = TRUE;

    _draw_border();

    /* Detach and drop the current shared memory segment and create and
       attach to a new segment */

    memcpy(save_atrtab, xc_atrtab, sizeof(save_atrtab));

    SP->XcurscrSize = XCURSCR_SIZE;
    shmdt((char *)Xcurscr);
    shmctl(shmid_Xcurscr, IPC_RMID, 0);

    if ((shmid_Xcurscr = shmget(shmkey_Xcurscr,
        SP->XcurscrSize + XCURSESSHMMIN, 0700 | IPC_CREAT)) < 0)
    {
        perror("Cannot allocate shared memory for curscr");

        _exit_process(4, SIGKILL, "exiting from _process_curses_requests");
    }

    Xcurscr = (unsigned char*)shmat(shmid_Xcurscr, 0, 0);
    memset(Xcurscr, 0, SP->XcurscrSize);
    xc_atrtab = (short *)(Xcurscr + XCURSCR_ATRTAB_OFF);
    memcpy(xc_atrtab, save_atrtab, sizeof(save_atrtab));
}

/* For PDC_set_title() */

static void _set_title(void)
{
    char title[1024];   /* big enough for window title */
    int pos;

    if ((XC_read_socket(xc_display_sock, &pos, sizeof(int)) < 0) ||
        (XC_read_socket(xc_display_sock, title, pos) < 0))
    {
        _exit_process(5, SIGKILL, "exiting from _set_title");
    }

    XtVaSetValues(topLevel, XtNtitle, title, NULL);
}

/* For color_content() */

static void _get_color(void)
{
    XColor *tmp = (XColor *)(Xcurscr + XCURSCR_XCOLOR_OFF);
    int index = tmp->pixel;
    Colormap cmap = DefaultColormap(XCURSESDISPLAY,
                                    DefaultScreen(XCURSESDISPLAY));

    if (index < 0 || index >= MAX_COLORS)
        _exit_process(4, SIGKILL, "exiting from _get_color");

    tmp->pixel = colors[index];
    XQueryColor(XCURSESDISPLAY, cmap, tmp);
}

/* For init_color() */

static void _set_color(void)
{
    XColor *tmp = (XColor *)(Xcurscr + XCURSCR_XCOLOR_OFF);
    int index = tmp->pixel;
    Colormap cmap = DefaultColormap(XCURSESDISPLAY,
                                    DefaultScreen(XCURSESDISPLAY));

    if (index < 0 || index >= MAX_COLORS)
        _exit_process(4, SIGKILL, "exiting from _set_color");

    if (XAllocColor(XCURSESDISPLAY, cmap, tmp))
    {
        XFreeColors(XCURSESDISPLAY, cmap, colors + index, 1, 0);
        colors[index] = tmp->pixel;

        _display_screen();
    }
}

/* For PDC_getclipboard() */

static void _get_selection(Widget w, XtPointer data, Atom *selection,
                           Atom *type, XtPointer value,
                           unsigned long *length, int *format)
{
    unsigned char *src = value;
    int pos, len = *length;

    XC_LOG(("_get_selection() - called\n"));

    if (!value && !len)
    {
        if (XC_write_display_socket_int(PDC_CLIP_EMPTY) < 0)
            _exit_process(4, SIGKILL, "exiting from _get_selection");
    }
    else
    {
        /* Here all is OK, send PDC_CLIP_SUCCESS, then length, then
           contents */

        if (XC_write_display_socket_int(PDC_CLIP_SUCCESS) < 0)
            _exit_process(4, SIGKILL, "exiting from _get_selection");

        if (XC_write_display_socket_int(len) < 0)
            _exit_process(4, SIGKILL, "exiting from _get_selection");

        for (pos = 0; pos < len; pos++)
        {
#ifdef PDC_WIDE
            wchar_t c;
#else
            unsigned char c;
#endif
            c = *src++;

            if (XC_write_socket(xc_display_sock, &c, sizeof(c)) < 0)
                _exit_process(4, SIGKILL, "exiting from _get_selection");
        }
    }
}

#ifdef PDC_WIDE
static void _get_selection_utf8(Widget w, XtPointer data, Atom *selection,
                                Atom *type, XtPointer value,
                                unsigned long *length, int *format)
{
    int len = *length;

    XC_LOG(("_get_selection_utf8() - called\n"));

    if (!*type || !*length)
    {
        XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, _get_selection,
                            (XtPointer)NULL, 0);
        return;
    }

    if (!value && !len)
    {
        if (XC_write_display_socket_int(PDC_CLIP_EMPTY) >= 0)
            return;
    }
    else
    {
        wchar_t *wcontents = malloc((len + 1) * sizeof(wchar_t));
        char *src = value;
        int i = 0;

        while (*src && i < (*length))
        {
            int retval = _from_utf8(wcontents + i, src, len);

            src += retval;
            len -= retval;
            i++;
        }

        wcontents[i] = 0;
        len = i;

        /* Here all is OK, send PDC_CLIP_SUCCESS, then length, then
           contents */

        if (XC_write_display_socket_int(PDC_CLIP_SUCCESS) >= 0)
            if (XC_write_display_socket_int(len) >= 0)
                if (XC_write_socket(xc_display_sock,
                    wcontents, len * sizeof(wchar_t)) >= 0)
                {
                    free(wcontents);
                    return;
                }
    }

    _exit_process(4, SIGKILL, "exiting from _get_selection_utf8");
}
#endif

/* For PDC_setclipboard() */

static void _set_selection(void)
{
    long length, pos;
    int status;

    if (XC_read_socket(xc_display_sock, &length, sizeof(long)) < 0)
        _exit_process(5, SIGKILL, "exiting from _set_selection");

    if (length > (long)tmpsel_length)
    {
        if (!tmpsel_length)
            tmpsel = malloc((length + 1) * sizeof(chtype));
        else
            tmpsel = realloc(tmpsel, (length + 1) * sizeof(chtype));
    }

    if (!tmpsel)
        if (XC_write_display_socket_int(PDC_CLIP_MEMORY_ERROR) < 0)
            _exit_process(4, SIGKILL, "exiting from _set_selection");

    for (pos = 0; pos < length; pos++)
    {
#ifdef PDC_WIDE
        wchar_t c;
#else
        unsigned char c;
#endif
        if (XC_read_socket(xc_display_sock, &c, sizeof(c)) < 0)
            _exit_process(5, SIGKILL, "exiting from _set_selection");

        tmpsel[pos] = c;
    }

    tmpsel_length = length;
    tmpsel[length] = 0;

    if (XtOwnSelection(topLevel, XA_PRIMARY, CurrentTime,
                       _convert_proc, _lose_ownership, NULL) == False)
    {
        status = PDC_CLIP_ACCESS_ERROR;
        free(tmpsel);
        tmpsel = NULL;
        tmpsel_length = 0;
    }
    else
        status = PDC_CLIP_SUCCESS;

    _selection_off();

    if (XC_write_display_socket_int(status) < 0)
        _exit_process(4, SIGKILL, "exiting from _set_selection");
}

/* The curses process is waiting; tell it to continue */

static void _resume_curses(void)
{
    if (XC_write_display_socket_int(CURSES_CONTINUE) < 0)
        _exit_process(4, SIGKILL, "exiting from _process_curses_requests");
}

/* The curses process sent us a message */

static void _process_curses_requests(XtPointer client_data, int *fid,
                                     XtInputId *id)
{
    struct timeval socket_timeout = {0};
    int s;
    int old_row, new_row;
    int old_x, new_x;
    int pos, num_cols;

    char buf[12];       /* big enough for 2 integers */

    XC_LOG(("_process_curses_requests() - called\n"));

    if (!received_map_notify)
        return;

    FD_ZERO(&xc_readfds);
    FD_SET(xc_display_sock, &xc_readfds);

    if ((s = select(FD_SETSIZE, (FD_SET_CAST)&xc_readfds, NULL,
                    NULL, &socket_timeout)) < 0)
        _exit_process(2, SIGKILL, "exiting from _process_curses_requests"
                                  " - select failed");

    if (!s)     /* no requests pending - should never happen! */
        return;

    if (FD_ISSET(xc_display_sock, &xc_readfds))
    {
        /* read first integer to determine total message has been
           received */

        XC_LOG(("_process_curses_requests() - before XC_read_socket()\n"));

        if (XC_read_socket(xc_display_sock, &num_cols, sizeof(int)) < 0)
            _exit_process(3, SIGKILL, "exiting from _process_curses_requests"
                                      " - first read");

        XC_LOG(("_process_curses_requests() - after XC_read_socket()\n"));

        after_first_curses_request = TRUE;

        switch(num_cols)
        {
        case CURSES_EXIT:   /* request from curses to stop */
            XC_LOG(("CURSES_EXIT received from child\n"));
            _exit_process(0, 0, "XCursesProcess requested to exit by child");
            break;

        case CURSES_BELL:
            XC_LOG(("CURSES_BELL received from child\n"));
            XBell(XCURSESDISPLAY, 50);
            break;

        /* request from curses to confirm completion of display */

        case CURSES_REFRESH:
            XC_LOG(("CURSES_REFRESH received from child\n"));
            _refresh_screen();
            _resume_curses();
            break;

        case CURSES_REFRESH_SCROLLBAR:
            _refresh_scrollbar();
            break;

        case CURSES_BLINK_ON:
            if (!(SP->termattrs & A_BLINK))
            {
                SP->termattrs |= A_BLINK;
                blinked_off = FALSE;
                XtAppAddTimeOut(app_context, xc_app_data.textBlinkRate,
                                _blink_text, NULL);
            }
            break;

        case CURSES_BLINK_OFF:
            SP->termattrs &= ~A_BLINK;
            break;

        case CURSES_CURSOR:
            XC_LOG(("CURSES_CURSOR received from child\n"));

            if (XC_read_socket(xc_display_sock, buf, sizeof(int) * 2) < 0)
                _exit_process(5, SIGKILL, "exiting from CURSES_CURSOR "
                                          "_process_curses_requests");

            memcpy(&pos, buf, sizeof(int));
            old_row = pos & 0xFF;
            old_x = pos >> 8;

            memcpy(&pos, buf + sizeof(int), sizeof(int));
            new_row = pos & 0xFF;
            new_x = pos >> 8;

            visible_cursor = TRUE;
            _display_cursor(old_row, old_x, new_row, new_x);
            break;

        case CURSES_DISPLAY_CURSOR:
            XC_LOG(("CURSES_DISPLAY_CURSOR received from child. Vis now: "));
            XC_LOG((visible_cursor ? "1\n" : "0\n"));

            /* If the window is not active, ignore this command. The
               cursor will stay solid. */

            if (window_entered)
            {
                if (visible_cursor)
                {
                    /* Cursor currently ON, turn it off */

                    int save_visibility = SP->visibility;
                    SP->visibility = 0;
                    _redraw_cursor();
                    SP->visibility = save_visibility;
                    visible_cursor = FALSE;
                }
                else
                {
                    /* Cursor currently OFF, turn it on */

                    _redraw_cursor();
                    visible_cursor = TRUE;
                }
            }

            break;

        case CURSES_TITLE:
            XC_LOG(("CURSES_TITLE received from child\n"));
            _set_title();
            break;

        case CURSES_RESIZE:
            XC_LOG(("CURSES_RESIZE received from child\n"));
            _resize();
            _resume_curses();
            break;

        case CURSES_GET_SELECTION:
            XC_LOG(("CURSES_GET_SELECTION received from child\n"));

            _resume_curses();

            XtGetSelectionValue(topLevel, XA_PRIMARY,
#ifdef PDC_WIDE
                                XA_UTF8_STRING(XtDisplay(topLevel)),
                                _get_selection_utf8,
#else
                                XA_STRING, _get_selection,
#endif
                                (XtPointer)NULL, 0);

            break;

        case CURSES_SET_SELECTION:
            XC_LOG(("CURSES_SET_SELECTION received from child\n"));
            _set_selection();
            break;

        case CURSES_CLEAR_SELECTION:
            XC_LOG(("CURSES_CLEAR_SELECTION received from child\n"));
            _resume_curses();
            _selection_off();
            break;

        case CURSES_GET_COLOR:
            XC_LOG(("CURSES_GET_COLOR recieved from child\n"));
            _get_color();
            _resume_curses();
            break;

        case CURSES_SET_COLOR:
            XC_LOG(("CURSES_SET_COLOR recieved from child\n"));
            _set_color();
            _resume_curses();
            break;

        default:
            PDC_LOG(("%s:Unknown request %d\n", XCLOGMSG, num_cols));
        }
    }
}

static void _handle_structure_notify(Widget w, XtPointer client_data,
                                     XEvent *event, Boolean *unused)
{
    XC_LOG(("_handle_structure_notify() - called\n"));

    switch(event->type)
    {
    case ConfigureNotify:
        XC_LOG(("ConfigureNotify received\n"));

        /* Window has been resized, change width and height to send to
           place_text and place_graphics in next Expose. Also will need
           to kill (SIGWINCH) curses process if screen size changes. */

        resize_window_width = event->xconfigure.width;
        resize_window_height = event->xconfigure.height;

        after_first_curses_request = FALSE;

#ifdef SIGWINCH
        SP->resized = 1;

        kill(xc_otherpid, SIGWINCH);
#endif
        _send_key_to_curses(KEY_RESIZE, NULL, TRUE);
        break;

    case MapNotify:
        XC_LOG(("MapNotify received\n"));

        received_map_notify = 1;

        _draw_border();
        break;

    default:
        PDC_LOG(("%s:_handle_structure_notify - unknown event %d\n",
                 XCLOGMSG, event->type));
    }
}

static void _handle_signals(int signo)
{
    int flag = CURSES_EXIT;

    PDC_LOG(("%s:_handle_signals() - called: %d\n", XCLOGMSG, signo));

    /* Patch by: Georg Fuchs */

    XCursesSetSignal(signo, _handle_signals);

#ifdef SIGTSTP
    if (signo == SIGTSTP)
    {
        pause();
        return;
    }
#endif
#ifdef SIGCONT
    if (signo == SIGCONT)
        return;
#endif
#ifdef SIGCLD
    if (signo == SIGCLD)
        return;
#endif
#ifdef SIGTTIN
    if (signo == SIGTTIN)
        return;
#endif
#ifdef SIGWINCH
    if (signo == SIGWINCH)
        return;
#endif

    /* End of patch by: Georg Fuchs */

    XCursesSetSignal(signo, SIG_IGN);

    /* Send a CURSES_EXIT to myself */

    if (XC_write_socket(xc_exit_sock, &flag, sizeof(int)) < 0)
        _exit_process(7, signo, "exiting from _handle_signals");
}

#ifdef PDC_XIM
static void _dummy_handler(Widget w, XtPointer client_data,
                           XEvent *event, Boolean *unused)
{
}
#endif

int XCursesSetupX(int argc, char *argv[])
{
    char *myargv[] = {"PDCurses", NULL};
    extern bool sb_started;

    bool italic_font_valid, bold_font_valid;
    XColor pointerforecolor, pointerbackcolor;
    XrmValue rmfrom, rmto;
    int i = 0;
    int minwidth, minheight;

    XC_LOG(("XCursesSetupX called\n"));

    if (!argv)
    {
        argv = myargv;
        argc = 1;
    }

    program_name = argv[0];

    /* Keep open the 'write' end of the socket so the XCurses process
       can send a CURSES_EXIT to itself from within the signal handler */

    xc_exit_sock = xc_display_sockets[0];
    xc_display_sock = xc_display_sockets[1];

    close(xc_key_sockets[0]);
    xc_key_sock = xc_key_sockets[1];

    /* Trap all signals when XCurses is the child process, but only if
       they haven't already been ignored by the application. */

    for (i = 0; i < PDC_MAX_SIGNALS; i++)
        if (XCursesSetSignal(i, _handle_signals) == SIG_IGN)
            XCursesSetSignal(i, SIG_IGN);

    /* Start defining X Toolkit things */

#if XtSpecificationRelease > 4
    XtSetLanguageProc(NULL, (XtLanguageProc)NULL, NULL);
#endif

    /* Exit if no DISPLAY variable set */

    if (!getenv("DISPLAY"))
    {
        fprintf(stderr, "Error: no DISPLAY variable set\n");
        kill(xc_otherpid, SIGKILL);
        return ERR;
    }

    /* Initialise the top level widget */

    topLevel = XtVaAppInitialize(&app_context, class_name, options,
                                 XtNumber(options), &argc, argv, NULL, NULL);

    XtVaGetApplicationResources(topLevel, &xc_app_data, app_resources,
                                XtNumber(app_resources), NULL);

    /* Check application resource values here */

    font_width = xc_app_data.normalFont->max_bounds.rbearing -
                 xc_app_data.normalFont->min_bounds.lbearing;

    font_ascent = xc_app_data.normalFont->max_bounds.ascent;
    font_descent = xc_app_data.normalFont->max_bounds.descent;
    font_height = font_ascent + font_descent;

    /* Check that the italic font and normal fonts are the same size */

    italic_font_valid = font_width ==
        xc_app_data.italicFont->max_bounds.rbearing -
        xc_app_data.italicFont->min_bounds.lbearing;

    bold_font_valid = font_width ==
        xc_app_data.boldFont->max_bounds.rbearing -
        xc_app_data.boldFont->min_bounds.lbearing;

    /* Calculate size of display window */

    XCursesCOLS = xc_app_data.cols;
    XCursesLINES = xc_app_data.lines;

    window_width = font_width * XCursesCOLS +
                   2 * xc_app_data.borderWidth;

    window_height = font_height * XCursesLINES +
                    2 * xc_app_data.borderWidth;

    minwidth = font_width * 2 + xc_app_data.borderWidth * 2;
    minheight = font_height * 2 + xc_app_data.borderWidth * 2;

    /* Set up the icon for the application; the default is an internal
       one for PDCurses. Then set various application level resources. */

    _get_icon();

#ifdef HAVE_XPM_H
    if (xc_app_data.pixmap && xc_app_data.pixmap[0])
        XtVaSetValues(topLevel, XtNminWidth, minwidth, XtNminHeight,
                      minheight, XtNbaseWidth, xc_app_data.borderWidth * 2,
                      XtNbaseHeight, xc_app_data.borderWidth * 2,
                      XtNiconPixmap, icon_pixmap,
                      XtNiconMask, icon_pixmap_mask, NULL);
    else
#endif
        XtVaSetValues(topLevel, XtNminWidth, minwidth, XtNminHeight,
                      minheight, XtNbaseWidth, xc_app_data.borderWidth * 2,
                      XtNbaseHeight, xc_app_data.borderWidth * 2,
                      XtNiconPixmap, icon_bitmap, NULL);

    /* Create a BOX widget in which to draw */

    if (xc_app_data.scrollbarWidth && sb_started)
    {
        scrollBox = XtVaCreateManagedWidget(program_name,
            scrollBoxWidgetClass, topLevel, XtNwidth,
            window_width + xc_app_data.scrollbarWidth,
            XtNheight, window_height + xc_app_data.scrollbarWidth,
            XtNwidthInc, font_width, XtNheightInc, font_height, NULL);

        drawing = XtVaCreateManagedWidget(program_name,
            boxWidgetClass, scrollBox, XtNwidth,
            window_width, XtNheight, window_height, XtNwidthInc,
            font_width, XtNheightInc, font_height, NULL);

        scrollVert = XtVaCreateManagedWidget("scrollVert",
            scrollbarWidgetClass, scrollBox, XtNorientation,
            XtorientVertical, XtNheight, window_height, XtNwidth,
            xc_app_data.scrollbarWidth, NULL);

        XtAddCallback(scrollVert, XtNscrollProc, _scroll_up_down, drawing);
        XtAddCallback(scrollVert, XtNjumpProc, _thumb_up_down, drawing);

        scrollHoriz = XtVaCreateManagedWidget("scrollHoriz",
            scrollbarWidgetClass, scrollBox, XtNorientation,
            XtorientHorizontal, XtNwidth, window_width, XtNheight,
            xc_app_data.scrollbarWidth, NULL);

        XtAddCallback(scrollHoriz, XtNscrollProc, _scroll_left_right, drawing);
        XtAddCallback(scrollHoriz, XtNjumpProc, _thumb_left_right, drawing);
    }
    else
    {
        drawing = XtVaCreateManagedWidget(program_name, boxWidgetClass,
            topLevel, XtNwidth, window_width, XtNheight, window_height,
            XtNwidthInc, font_width, XtNheightInc, font_height, NULL);

        XtVaSetValues(topLevel, XtNwidthInc, font_width, XtNheightInc,
                      font_height, NULL);
    }

    /* Process any default translations */

    XtAugmentTranslations(drawing,
                          XtParseTranslationTable(default_translations));
    XtAppAddActions(app_context, action_table, XtNumber(action_table));

    /* Process the supplied colors */

    _initialize_colors();

    /* Determine text cursor alignment from resources */

    if (!strcmp(xc_app_data.textCursor, "vertical"))
        vertical_cursor = TRUE;

    /* Now have LINES and COLS. Set these in the shared SP so the curses
       program can find them. */

    LINES = XCursesLINES;
    COLS = XCursesCOLS;

    if ((shmidSP = shmget(shmkeySP, sizeof(SCREEN) + XCURSESSHMMIN,
        0700 | IPC_CREAT)) < 0)
    {
        perror("Cannot allocate shared memory for SCREEN");
        kill(xc_otherpid, SIGKILL);
        return ERR;
    }

    SP = (SCREEN*)shmat(shmidSP, 0, 0);
    memset(SP, 0, sizeof(SCREEN));
    SP->XcurscrSize = XCURSCR_SIZE;
    SP->lines = XCursesLINES;
    SP->cols = XCursesCOLS;

    SP->mouse_wait = xc_app_data.clickPeriod;
    SP->audible = TRUE;

    SP->termattrs = A_COLOR | A_ITALIC | A_UNDERLINE | A_LEFT | A_RIGHT |
                    A_REVERSE;

    PDC_LOG(("%s:SHM size for curscr %d\n", XCLOGMSG, SP->XcurscrSize));

    if ((shmid_Xcurscr = shmget(shmkey_Xcurscr, SP->XcurscrSize +
        XCURSESSHMMIN, 0700 | IPC_CREAT)) < 0)
    {
        perror("Cannot allocate shared memory for curscr");
        kill(xc_otherpid, SIGKILL);
        shmdt((char *)SP);
        shmctl(shmidSP, IPC_RMID, 0);
        return ERR;
    }

    Xcurscr = (unsigned char *)shmat(shmid_Xcurscr, 0, 0);
    memset(Xcurscr, 0, SP->XcurscrSize);
    xc_atrtab = (short *)(Xcurscr + XCURSCR_ATRTAB_OFF);

    PDC_LOG(("%s:shmid_Xcurscr %d shmkey_Xcurscr %d LINES %d COLS %d\n",
             XCLOGMSG, shmid_Xcurscr, shmkey_Xcurscr, LINES, COLS));

    /* Add Event handlers to the drawing widget */

    XtAddEventHandler(drawing, ExposureMask, False, _handle_expose, NULL);
    XtAddEventHandler(drawing, StructureNotifyMask, False,
                      _handle_structure_notify, NULL);
    XtAddEventHandler(drawing, EnterWindowMask | LeaveWindowMask, False,
                      _handle_enter_leave, NULL);
    XtAddEventHandler(topLevel, 0, True, _handle_nonmaskable, NULL);

    /* Add input handler from xc_display_sock (requests from curses
       program) */

    XtAppAddInput(app_context, xc_display_sock, (XtPointer)XtInputReadMask,
                  _process_curses_requests, NULL);

    /* If there is a cursorBlink resource, start the Timeout event */

    if (xc_app_data.cursorBlinkRate)
        XtAppAddTimeOut(app_context, xc_app_data.cursorBlinkRate,
                        _blink_cursor, NULL);

    /* Leave telling the curses process that it can start to here so
       that when the curses process makes a request, the Xcurses
       process can service the request. */

    XC_write_display_socket_int(CURSES_CHILD);

    XtRealizeWidget(topLevel);

    /* Handle trapping of the WM_DELETE_WINDOW property */

    wm_atom[0] = XInternAtom(XtDisplay(topLevel), "WM_DELETE_WINDOW", False);

    XSetWMProtocols(XtDisplay(topLevel), XtWindow(topLevel), wm_atom, 1);

    /* Create the Graphics Context for drawing. This MUST be done AFTER
       the associated widget has been realized. */

    XC_LOG(("before _get_gc\n"));

    _get_gc(&normal_gc, xc_app_data.normalFont, COLOR_WHITE, COLOR_BLACK);

    _get_gc(&italic_gc, italic_font_valid ? xc_app_data.italicFont :
            xc_app_data.normalFont, COLOR_WHITE, COLOR_BLACK);

    _get_gc(&bold_gc, bold_font_valid ? xc_app_data.boldFont :
            xc_app_data.normalFont, COLOR_WHITE, COLOR_BLACK);

    _get_gc(&rect_cursor_gc, xc_app_data.normalFont,
            COLOR_CURSOR, COLOR_BLACK);

    _get_gc(&border_gc, xc_app_data.normalFont, COLOR_BORDER, COLOR_BLACK);

    XSetLineAttributes(XCURSESDISPLAY, rect_cursor_gc, 2,
                       LineSolid, CapButt, JoinMiter);

    XSetLineAttributes(XCURSESDISPLAY, border_gc, xc_app_data.borderWidth,
                       LineSolid, CapButt, JoinMiter);

    /* Set the cursor for the application */

    XDefineCursor(XCURSESDISPLAY, XCURSESWIN, xc_app_data.pointer);
    rmfrom.size = sizeof(Pixel);
    rmto.size = sizeof(XColor);

    rmto.addr = (XPointer)&pointerforecolor;
    rmfrom.addr = (XPointer)&(xc_app_data.pointerForeColor);
    XtConvertAndStore(drawing, XtRPixel, &rmfrom, XtRColor, &rmto);

    rmfrom.size = sizeof(Pixel);
    rmto.size = sizeof(XColor);

    rmfrom.addr = (XPointer)&(xc_app_data.pointerBackColor);
    rmto.addr = (XPointer)&pointerbackcolor;
    XtConvertAndStore(drawing, XtRPixel, &rmfrom, XtRColor, &rmto);

    XRecolorCursor(XCURSESDISPLAY, xc_app_data.pointer,
                   &pointerforecolor, &pointerbackcolor);

#ifndef PDC_XIM

    /* Convert the supplied compose key to a Keysym */

    compose_key = XStringToKeysym(xc_app_data.composeKey);

    if (compose_key && IsModifierKey(compose_key))
    {
        int i, j;
        KeyCode *kcp;
        XModifierKeymap *map;
        KeyCode compose_keycode = XKeysymToKeycode(XCURSESDISPLAY, compose_key);

        map = XGetModifierMapping(XCURSESDISPLAY);
        kcp = map->modifiermap;

        for (i = 0; i < 8; i++)
        {
            for (j = 0; j < map->max_keypermod; j++, kcp++)
            {
                if (!*kcp)
                    continue;

                if (compose_keycode == *kcp)
                {
                    compose_mask = state_mask[i];
                    break;
                }
            }

            if (compose_mask)
                break;
        }

        XFreeModifiermap(map);
    }

#else
    Xim = XOpenIM(XCURSESDISPLAY, NULL, NULL, NULL);

    if (Xim)
    {
        Xic = XCreateIC(Xim, XNInputStyle,
                        XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, XCURSESWIN, NULL);
    }

    if (Xic)
    {
        long im_event_mask;

        XGetICValues(Xic, XNFilterEvents, &im_event_mask, NULL);
        if (im_event_mask)
            XtAddEventHandler(drawing, im_event_mask, False,
                              _dummy_handler, NULL);

        XSetICFocus(Xic);
    }
    else
    {
        perror("ERROR: Cannot create input context");
        kill(xc_otherpid, SIGKILL);
        shmdt((char *)SP);
        shmdt((char *)Xcurscr);
        shmctl(shmidSP, IPC_RMID, 0);
        shmctl(shmid_Xcurscr, IPC_RMID, 0);
        return ERR;
    }

#endif

    /* Wait for events */
    {
        XEvent event;

        for (;;) /* forever */
        {
             XtAppNextEvent(app_context, &event);
             XtDispatchEvent(&event);
        }
    }

    return OK;          /* won't get here */
}
