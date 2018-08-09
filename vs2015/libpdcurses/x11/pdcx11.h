/* Public Domain Curses */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <curspriv.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <signal.h>
#include <ctype.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>   /* AIX needs this for FD_ZERO etc macros */
#endif
#include <sys/ipc.h>
#include <sys/shm.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>

#ifdef USE_XAW3D
# include <Xaw3d/Box.h>
# include <Xaw3d/Scrollbar.h>
#elif defined(USE_NEXTAW)
# include <neXtaw/Box.h>
# include <neXtaw/Scrollbar.h>
#else
# include <Xaw/Box.h>
# include <Xaw/Scrollbar.h>
#endif
#include "x11/ScrollBox.h"

#include "Xmu/StdSel.h"
#include "Xmu/Atoms.h"

#include <keysym.h>
#include <Xatom.h>

#define XCURSCR_Y_SIZE      (XCursesLINES * XCursesCOLS * sizeof(chtype))
#define XCURSCR_FLAG_SIZE   (XCursesLINES * sizeof(int))
#define XCURSCR_START_SIZE  (XCursesLINES * sizeof(int))
#define XCURSCR_LENGTH_SIZE (XCursesLINES * sizeof(int))
#define XCURSCR_ATRTAB_SIZE (PDC_COLOR_PAIRS * 2 * sizeof(short))
#define XCURSCR_SIZE        (XCURSCR_FLAG_SIZE + XCURSCR_START_SIZE + \
        XCURSCR_LENGTH_SIZE + XCURSCR_Y_SIZE + XCURSCR_ATRTAB_SIZE + \
        sizeof(XColor))

#define XCURSCR_Y_OFF(y)    ((y) * XCursesCOLS * sizeof(chtype))
#define XCURSCR_FLAG_OFF    (XCURSCR_Y_OFF(0) + XCURSCR_Y_SIZE)
#define XCURSCR_START_OFF   (XCURSCR_FLAG_OFF + XCURSCR_FLAG_SIZE)
#define XCURSCR_LENGTH_OFF  (XCURSCR_START_OFF + XCURSCR_START_SIZE)
#define XCURSCR_ATRTAB_OFF  (XCURSCR_LENGTH_OFF + XCURSCR_LENGTH_SIZE)
#define XCURSCR_XCOLOR_OFF  (XCURSCR_ATRTAB_OFF + XCURSCR_ATRTAB_SIZE)

typedef struct
{
    int lines;
    int cols;
    Pixel cursorColor;
    Pixel colorBlack;
    Pixel colorRed;
    Pixel colorGreen;
    Pixel colorYellow;
    Pixel colorBlue;
    Pixel colorMagenta;
    Pixel colorCyan;
    Pixel colorWhite;
    Pixel colorBoldBlack;
    Pixel colorBoldRed;
    Pixel colorBoldGreen;
    Pixel colorBoldYellow;
    Pixel colorBoldBlue;
    Pixel colorBoldMagenta;
    Pixel colorBoldCyan;
    Pixel colorBoldWhite;
    Pixel pointerForeColor;
    Pixel pointerBackColor;
    XFontStruct *normalFont;
    XFontStruct *italicFont;
    XFontStruct *boldFont;
    char *bitmap;
#ifdef HAVE_XPM_H
    char *pixmap;
#endif
    char *composeKey;
    Cursor pointer;
    int shmmin;
    int borderWidth;
    int borderColor;
    int clickPeriod;
    int doubleClickPeriod;
    int scrollbarWidth;
    int cursorBlinkRate;
    char *textCursor;
    int textBlinkRate;
} XCursesAppData;

extern XCursesAppData xc_app_data;

#define XCURSESSHMMIN xc_app_data.shmmin

#define XCLOGMSG (XCursesProcess ? "     X" : "CURSES")

void XC_get_line_lock(int);
void XC_release_line_lock(int);

int PDC_display_cursor(int, int, int, int, int);

void XCursesExitCursesProcess(int, char *);
int XCursesInstruct(int);
int XCursesInstructAndWait(int);
int XCursesInitscr(int, char **);

int XC_write_socket(int, const void *, int);
int XC_read_socket(int, void *, int);
int XC_write_display_socket_int(int);

int XCursesSetupX(int argc, char *argv[]);
void XCursesSigwinchHandler(int signo);

#ifdef _HPUX_SOURCE
# define FD_SET_CAST int *
#else
# define FD_SET_CAST fd_set *
#endif

extern fd_set xc_readfds;

extern unsigned char *Xcurscr;
extern int XCursesProcess;
extern int shmidSP;
extern int shmid_Xcurscr;
extern int shmkeySP;
extern int shmkey_Xcurscr;
extern int xc_otherpid;
extern int XCursesLINES;
extern int XCursesCOLS;
extern int xc_display_sock;
extern int xc_key_sock;
extern int xc_display_sockets[2];
extern int xc_key_sockets[2];
extern int xc_exit_sock;

typedef void (*signal_handler)();

signal_handler XCursesSetSignal(int, signal_handler);

#ifdef PDCDEBUG
void XC_say(const char *msg);
# define XC_LOG(x) XC_say x
#else
# define XC_LOG(x)
#endif

#ifdef MOUSE_DEBUG
# define MOUSE_LOG(x) printf x
#else
# define MOUSE_LOG(x)
#endif

enum
{
    CURSES_CLEAR_SELECTION, CURSES_DISPLAY_CURSOR, CURSES_SET_SELECTION,
    CURSES_GET_SELECTION, CURSES_TITLE, CURSES_REFRESH_SCROLLBAR,
    CURSES_RESIZE, CURSES_BELL, CURSES_CONTINUE, CURSES_CURSOR,
    CURSES_CHILD, CURSES_REFRESH, CURSES_GET_COLOR, CURSES_SET_COLOR,
    CURSES_BLINK_ON, CURSES_BLINK_OFF, CURSES_EXIT
};

extern short *xc_atrtab;
