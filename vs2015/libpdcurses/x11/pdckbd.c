/* Public Domain Curses */

#include "pdcx11.h"

/*man-start**************************************************************

pdckbd
------

### Synopsis

    unsigned long PDC_get_input_fd(void);

### Description

   PDC_get_input_fd() returns the file descriptor that PDCurses
   reads its input from. It can be used for select().

### Portability
                             X/Open    BSD    SYS V
    PDC_get_input_fd            -       -       -

**man-end****************************************************************/

/* check if a key or mouse event is waiting */

bool PDC_check_key(void)
{
    struct timeval socket_timeout = {0};
    int s;

    /* Is something ready to be read on the socket ? Must be a key. */

    FD_ZERO(&xc_readfds);
    FD_SET(xc_key_sock, &xc_readfds);

    s = select(FD_SETSIZE, (FD_SET_CAST)&xc_readfds, NULL, NULL,
               &socket_timeout);
    if (s < 0)
        XCursesExitCursesProcess(3, "child - exiting from "
                                    "PDC_check_key select failed");

    PDC_LOG(("%s:PDC_check_key() - returning %s\n", XCLOGMSG,
             s ? "TRUE" : "FALSE"));

    return !!s;
}

/* return the next available key or mouse event */

int PDC_get_key(void)
{
    unsigned long newkey = 0;
    int key = 0;

    if (XC_read_socket(xc_key_sock, &newkey, sizeof(unsigned long)) < 0)
        XCursesExitCursesProcess(2, "exiting from PDC_get_key");

    pdc_key_modifiers = (newkey >> 24) & 0xFF;
    key = (int)(newkey & 0x00FFFFFF);

    if (key == KEY_MOUSE && SP->key_code)
    {
        if (XC_read_socket(xc_key_sock, &pdc_mouse_status,
            sizeof(MOUSE_STATUS)) < 0)
            XCursesExitCursesProcess(2, "exiting from PDC_get_key");
    }

    PDC_LOG(("%s:PDC_get_key() - key %d returned\n", XCLOGMSG, key));

    return key;
}

unsigned long PDC_get_input_fd(void)
{
    PDC_LOG(("PDC_get_input_fd() - called\n"));

    return xc_key_sock;
}

void PDC_set_keyboard_binary(bool on)
{
    PDC_LOG(("PDC_set_keyboard_binary() - called\n"));
}

/* discard any pending keyboard or mouse input -- this is the core
   routine for flushinp() */

void PDC_flushinp(void)
{
    PDC_LOG(("PDC_flushinp() - called\n"));

    while (PDC_check_key())
        PDC_get_key();
}

int PDC_mouse_set(void)
{
    return OK;
}

int PDC_modifiers_set(void)
{
    return OK;
}
