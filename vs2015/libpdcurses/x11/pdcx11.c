/* Public Domain Curses */

#include "pdcx11.h"

#include <errno.h>
#include <stdlib.h>

/*** Functions that are called by both processes ***/

unsigned char *Xcurscr;

int XCursesProcess = 1;
int shmidSP;
int shmid_Xcurscr;
int shmkeySP;
int shmkey_Xcurscr;
int xc_otherpid;
int XCursesLINES = 24;
int XCursesCOLS = 80;
int xc_display_sock;
int xc_key_sock;
int xc_display_sockets[2];
int xc_key_sockets[2];
int xc_exit_sock;

fd_set xc_readfds;

static void _dummy_function(void)
{
}

void XC_get_line_lock(int row)
{
    /* loop until we can write to the line -- Patch by:
       Georg Fuchs, georg.fuchs@rz.uni-regensburg.de */

    while (*(Xcurscr + XCURSCR_FLAG_OFF + row))
        _dummy_function();

    *(Xcurscr + XCURSCR_FLAG_OFF + row) = 1;
}

void XC_release_line_lock(int row)
{
    *(Xcurscr + XCURSCR_FLAG_OFF + row) = 0;
}

int XC_write_socket(int sock_num, const void *buf, int len)
{
    int start = 0, rc;

    PDC_LOG(("%s:XC_write_socket called: sock_num %d len %d\n",
             XCLOGMSG, sock_num, len));

#ifdef MOUSE_DEBUG
    if (sock_num == xc_key_sock)
        printf("%s:XC_write_socket(key) len: %d\n", XCLOGMSG, len);
#endif
    while (1)
    {
        rc = write(sock_num, buf + start, len);

        if (rc < 0 || rc == len)
            return rc;

        len -= rc;
        start = rc;
    }
}

int XC_read_socket(int sock_num, void *buf, int len)
{
    int start = 0, length = len, rc;

    PDC_LOG(("%s:XC_read_socket called: sock_num %d len %d\n",
             XCLOGMSG, sock_num, len));

    while (1)
    {
        rc = read(sock_num, buf + start, length);

#ifdef MOUSE_DEBUG
        if (sock_num == xc_key_sock)
            printf("%s:XC_read_socket(key) rc %d errno %d "
                   "resized: %d\n", XCLOGMSG, rc, errno, SP->resized);
#endif
        if (rc < 0 && sock_num == xc_key_sock && errno == EINTR
            && SP->resized != FALSE)
        {
            MOUSE_LOG(("%s:continuing\n", XCLOGMSG));

            rc = 0;

            if (SP->resized > 1)
                SP->resized = TRUE;
            else
                SP->resized = FALSE;

            memcpy(buf, &rc, sizeof(int));

            return 0;
        }

        if (rc <= 0 || rc == length)
            return rc;

        length -= rc;
        start = rc;
    }
}

int XC_write_display_socket_int(int x)
{
    return XC_write_socket(xc_display_sock, &x, sizeof(int));
}

#ifdef PDCDEBUG
void XC_say(const char *msg)
{
    PDC_LOG(("%s:%s", XCLOGMSG, msg));
}
#endif

/*** Functions that are called by the "curses" process ***/

int XCursesInstruct(int flag)
{
    PDC_LOG(("%s:XCursesInstruct() - called flag %d\n", XCLOGMSG, flag));

    /* Send a request to X */

    if (XC_write_display_socket_int(flag) < 0)
        XCursesExitCursesProcess(4, "exiting from XCursesInstruct");

    return OK;
}

int XCursesInstructAndWait(int flag)
{
    int result;

    XC_LOG(("XCursesInstructAndWait() - called\n"));

    /* tell X we want to do something */

    XCursesInstruct(flag);

    /* wait for X to say the refresh has occurred*/

    if (XC_read_socket(xc_display_sock, &result, sizeof(int)) < 0)
        XCursesExitCursesProcess(5, "exiting from XCursesInstructAndWait");

    if (result != CURSES_CONTINUE)
        XCursesExitCursesProcess(6, "exiting from XCursesInstructAndWait"
                                    " - synchronization error");

    return OK;
}

static int _setup_curses(void)
{
    int wait_value;

    XC_LOG(("_setup_curses called\n"));

    close(xc_display_sockets[1]);
    close(xc_key_sockets[1]);

    xc_display_sock = xc_display_sockets[0];
    xc_key_sock = xc_key_sockets[0];

    FD_ZERO(&xc_readfds);

    XC_read_socket(xc_display_sock, &wait_value, sizeof(int));

    if (wait_value != CURSES_CHILD)
        return ERR;

    /* Set LINES and COLS now so that the size of the shared memory
       segment can be allocated */

    if ((shmidSP = shmget(shmkeySP, sizeof(SCREEN) + XCURSESSHMMIN, 0700)) < 0)
    {
        perror("Cannot allocate shared memory for SCREEN");
        kill(xc_otherpid, SIGKILL);
        return ERR;
    }

    SP = (SCREEN*)shmat(shmidSP, 0, 0);

    XCursesLINES = SP->lines;
    LINES = XCursesLINES - SP->linesrippedoff - SP->slklines;
    XCursesCOLS = COLS = SP->cols;

    if ((shmid_Xcurscr = shmget(shmkey_Xcurscr,
                                SP->XcurscrSize + XCURSESSHMMIN, 0700)) < 0)
    {
        perror("Cannot allocate shared memory for curscr");
        kill(xc_otherpid, SIGKILL);
        return ERR;
    }

    PDC_LOG(("%s:shmid_Xcurscr %d shmkey_Xcurscr %d LINES %d COLS %d\n",
             XCLOGMSG, shmid_Xcurscr, shmkey_Xcurscr, LINES, COLS));

    Xcurscr = (unsigned char *)shmat(shmid_Xcurscr, 0, 0);
    xc_atrtab = (short *)(Xcurscr + XCURSCR_ATRTAB_OFF);

    XC_LOG(("cursesprocess exiting from Xinitscr\n"));

    /* Always trap SIGWINCH if the C library supports SIGWINCH */

    XCursesSetSignal(SIGWINCH, XCursesSigwinchHandler);

    atexit(XCursesExit);

    return OK;
}

int XCursesInitscr(int argc, char *argv[])
{
    int pid, rc;

    XC_LOG(("XCursesInitscr() - called\n"));

    shmkeySP = getpid();

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xc_display_sockets) < 0)
    {
        fprintf(stderr, "ERROR: cannot create display socketpair\n");
        return ERR;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xc_key_sockets) < 0)
    {
        fprintf(stderr, "ERROR: cannot create key socketpair\n");
        return ERR;
    }

    pid = fork();

    switch(pid)
    {
    case -1:
        fprintf(stderr, "ERROR: cannot fork()\n");
        return ERR;
        break;

    case 0: /* child */
        shmkey_Xcurscr = getpid();
#ifdef XISPARENT
        XCursesProcess = 0;
        rc = _setup_curses();
#else
        XCursesProcess = 1;
        xc_otherpid = getppid();
        rc = XCursesSetupX(argc, argv);
#endif
        break;

    default:  /* parent */
        shmkey_Xcurscr = pid;
#ifdef XISPARENT
        XCursesProcess = 1;
        xc_otherpid = pid;
        rc = XCursesSetupX(argc, argv);
#else
        XCursesProcess = 0;
        rc = _setup_curses();
#endif
    }

    return rc;
}

static void _cleanup_curses_process(int rc)
{
    PDC_LOG(("%s:_cleanup_curses_process() - called: %d\n", XCLOGMSG, rc));

    shutdown(xc_display_sock, 2);
    close(xc_display_sock);

    shutdown(xc_key_sock, 2);
    close(xc_key_sock);

    shmdt((char *)SP);
    shmdt((char *)Xcurscr);

    if (rc)
        _exit(rc);
}

void XCursesExitCursesProcess(int rc, char *msg)
{
    PDC_LOG(("%s:XCursesExitCursesProcess() - called: %d %s\n",
             XCLOGMSG, rc, msg));

    endwin();
    _cleanup_curses_process(rc);
}

void XCursesExit(void)
{
    static bool called = FALSE;

    XC_LOG(("XCursesExit() - called\n"));

    if (FALSE == called)
    {
        XCursesInstruct(CURSES_EXIT);
        _cleanup_curses_process(0);

        called = TRUE;
    }
}
