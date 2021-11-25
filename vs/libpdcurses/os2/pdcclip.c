/* Public Domain Curses */

#include "pdcos2.h"

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
    HMQ hmq;
    HAB hab;
    PTIB ptib;
    PPIB ppib;
    ULONG ulRet;
    long len;
    int rc;

    PDC_LOG(("PDC_getclipboard() - called\n"));

    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype = 3;
    hab = WinInitialize(0);
    hmq = WinCreateMsgQueue(hab, 0);

    if (!WinOpenClipbrd(hab))
    {
        WinDestroyMsgQueue(hmq);
        WinTerminate(hab);
        return PDC_CLIP_ACCESS_ERROR;
    }

    rc = PDC_CLIP_EMPTY;

    ulRet = WinQueryClipbrdData(hab, CF_TEXT);

    if (ulRet)
    {
        len = strlen((char *)ulRet);
        *contents = malloc(len + 1);

        if (!*contents)
            rc = PDC_CLIP_MEMORY_ERROR;
        else
        {
            strcpy((char *)*contents, (char *)ulRet);
            *length = len;
            rc = PDC_CLIP_SUCCESS;
        }
    }

    WinCloseClipbrd(hab);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    return rc;
}

int PDC_setclipboard(const char *contents, long length)
{
    HAB hab;
    PTIB ptib;
    PPIB ppib;
    ULONG ulRC;
    PSZ szTextOut = NULL;
    int rc;

    PDC_LOG(("PDC_setclipboard() - called\n"));

    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype = 3;
    hab = WinInitialize(0);

    if (!WinOpenClipbrd(hab))
    {
        WinTerminate(hab);
        return PDC_CLIP_ACCESS_ERROR;
    }

    rc = PDC_CLIP_MEMORY_ERROR;

    ulRC = DosAllocSharedMem((PVOID)&szTextOut, NULL, length + 1,
                             PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE);

    if (ulRC == 0)
    {
        strcpy(szTextOut, contents);
        WinEmptyClipbrd(hab);

        if (WinSetClipbrdData(hab, (ULONG)szTextOut, CF_TEXT, CFI_POINTER))
            rc = PDC_CLIP_SUCCESS;
        else
        {
            DosFreeMem(szTextOut);
            rc = PDC_CLIP_ACCESS_ERROR;
        }
    }

    WinCloseClipbrd(hab);
    WinTerminate(hab);

    return rc;
}

int PDC_freeclipboard(char *contents)
{
    PDC_LOG(("PDC_freeclipboard() - called\n"));

    if (contents)
        free(contents);

    return PDC_CLIP_SUCCESS;
}

int PDC_clearclipboard(void)
{
    HAB hab;
    PTIB ptib;
    PPIB ppib;

    PDC_LOG(("PDC_clearclipboard() - called\n"));

    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype = 3;
    hab = WinInitialize(0);

    WinEmptyClipbrd(hab);

    WinCloseClipbrd(hab);
    WinTerminate(hab);

    return PDC_CLIP_SUCCESS;
}
