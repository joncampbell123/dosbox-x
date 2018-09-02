/* Public Domain Curses */

#include "pdcx11.h"

#include <stdlib.h>

/*man-start**************************************************************

clipboard
---------

### Synopsis

    int PDC_getclipboard(char **contents, long *length);
    int PDC_setclipboard(const char *contents, long length);
    int PDC_freeclipboard(char *contents);
    int PDC_clearclipboard(void);

### Description

   PDC_getclipboard() gets the textual contents of the system's
   clipboard. This function returns the contents of the clipboard
   in the contents argument. It is the responsibilitiy of the
   caller to free the memory returned, via PDC_freeclipboard().
   The length of the clipboard contents is returned in the length
   argument.

   PDC_setclipboard copies the supplied text into the system's
   clipboard, emptying the clipboard prior to the copy.

   PDC_clearclipboard() clears the internal clipboard.

### Return Values

   indicator of success/failure of call.
   PDC_CLIP_SUCCESS        the call was successful
   PDC_CLIP_MEMORY_ERROR   unable to allocate sufficient memory for
                           the clipboard contents
   PDC_CLIP_EMPTY          the clipboard contains no text
   PDC_CLIP_ACCESS_ERROR   no clipboard support

### Portability
                             X/Open    BSD    SYS V
    PDC_getclipboard            -       -       -
    PDC_setclipboard            -       -       -
    PDC_freeclipboard           -       -       -
    PDC_clearclipboard          -       -       -

**man-end****************************************************************/

int PDC_getclipboard(char **contents, long *length)
{
#ifdef PDC_WIDE
    wchar_t *wcontents;
#endif
    int result = 0;
    int len;

    PDC_LOG(("PDC_getclipboard() - called\n"));

    XCursesInstructAndWait(CURSES_GET_SELECTION);

    if (XC_read_socket(xc_display_sock, &result, sizeof(int)) < 0)
        XCursesExitCursesProcess(5, "exiting from PDC_getclipboard");

    if (result == PDC_CLIP_SUCCESS)
    {
        if (XC_read_socket(xc_display_sock, &len, sizeof(int)) < 0)
            XCursesExitCursesProcess(5, "exiting from PDC_getclipboard");
#ifdef PDC_WIDE
        wcontents = malloc((len + 1) * sizeof(wchar_t));
        *contents = malloc(len * 3 + 1);

        if (!wcontents || !*contents)
#else
            *contents = malloc(len + 1);

        if (!*contents)
#endif
        XCursesExitCursesProcess(6, "exiting from PDC_getclipboard - "
                                    "synchronization error");

        if (len)
        {
            if (XC_read_socket(xc_display_sock,
#ifdef PDC_WIDE
                wcontents, len * sizeof(wchar_t)) < 0)
#else
                *contents, len) < 0)
#endif
                XCursesExitCursesProcess(5, "exiting from PDC_getclipboard");
        }

#ifdef PDC_WIDE
        wcontents[len] = 0;
        len = PDC_wcstombs(*contents, wcontents, len * 3);
        free(wcontents);
#endif
        (*contents)[len] = '\0';
        *length = len;
    }

    return result;
}

int PDC_setclipboard(const char *contents, long length)
{
#ifdef PDC_WIDE
    wchar_t *wcontents;
#endif
    int rc;

    PDC_LOG(("PDC_setclipboard() - called\n"));

#ifdef PDC_WIDE
    wcontents = malloc((length + 1) * sizeof(wchar_t));
    if (!wcontents)
        return PDC_CLIP_MEMORY_ERROR;

    length = PDC_mbstowcs(wcontents, contents, length);
#endif
    XCursesInstruct(CURSES_SET_SELECTION);

    /* Write, then wait for X to do its stuff; expect return code. */

    if (XC_write_socket(xc_display_sock, &length, sizeof(long)) >= 0)
    {
        if (XC_write_socket(xc_display_sock,
#ifdef PDC_WIDE
            wcontents, length * sizeof(wchar_t)) >= 0)
        {
            free(wcontents);
#else
            contents, length) >= 0)
        {
#endif
            if (XC_read_socket(xc_display_sock, &rc, sizeof(int)) >= 0)
                return rc;
        }
    }

    XCursesExitCursesProcess(5, "exiting from PDC_setclipboard");

    return PDC_CLIP_ACCESS_ERROR;   /* not reached */
}

int PDC_freeclipboard(char *contents)
{
    PDC_LOG(("PDC_freeclipboard() - called\n"));

    free(contents);
    return PDC_CLIP_SUCCESS;
}

int PDC_clearclipboard(void)
{
    int rc;
    long len = 0;

    PDC_LOG(("PDC_clearclipboard() - called\n"));

    XCursesInstruct(CURSES_CLEAR_SELECTION);

    /* Write, then wait for X to do its stuff; expect return code. */

    if (XC_write_socket(xc_display_sock, &len, sizeof(long)) >= 0)
        if (XC_read_socket(xc_display_sock, &rc, sizeof(int)) >= 0)
            return rc;

    XCursesExitCursesProcess(5, "exiting from PDC_clearclipboard");

    return PDC_CLIP_ACCESS_ERROR;   /* not reached */
}
