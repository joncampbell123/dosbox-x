/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is for Open Layer for Unicode (opencow).
 *
 * The Initial Developer of the Original Code is Brodie Thiesfield.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// define these symbols so that we don't get dllimport linkage 
// from the system headers
#define _COMDLG32_

#include <windows.h>
#include <commdlg.h>

#include "MbcsBuffer.h"

// ----------------------------------------------------------------------------
// API

// ChooseColorW
// ChooseFontW
// FindTextW
// GetFileTitleW

static BOOL 
OpenSaveFileNameW(
    LPOPENFILENAMEW lpofn, 
    BOOL            aIsOpen
    )
{
    OPENFILENAMEA ofnA;
    ::ZeroMemory(&ofnA, sizeof(ofnA));

    ofnA.lStructSize = OPENFILENAME_SIZE_VERSION_400A;//sizeof(OPENFILENAME_NT4A);
    ofnA.hwndOwner = lpofn->hwndOwner;
    ofnA.hInstance = lpofn->hInstance;

    CMbcsBuffer mbcsFilter;
    if (lpofn->lpstrFilter) {
        int filterLen = 0;
        int len = wcslen(lpofn->lpstrFilter);
        while (len > 0) {
            filterLen += len + 1;
            len = wcslen(lpofn->lpstrFilter + filterLen);
        }
        ++filterLen;
        if (!mbcsFilter.FromUnicode(lpofn->lpstrFilter, filterLen)) 
            return FALSE;
        ofnA.lpstrFilter = mbcsFilter;
    }

    CMbcsBuffer mbcsCustomFilter;
    if (lpofn->lpstrCustomFilter) {
        int customFilterLen = 0;
        customFilterLen = wcslen(lpofn->lpstrCustomFilter) + 1;
        customFilterLen += wcslen(lpofn->lpstrCustomFilter + customFilterLen) + 1;

        // double the buffer space requested because MBCS can consume double that of Unicode (in chars)
        if (!mbcsCustomFilter.FromUnicode(lpofn->lpstrCustomFilter, customFilterLen, lpofn->nMaxCustFilter * 2)) 
            return FALSE;
        ofnA.lpstrCustomFilter = mbcsCustomFilter;
        ofnA.nMaxCustFilter = mbcsCustomFilter.BufferSize();
    }

    ofnA.nFilterIndex = lpofn->nFilterIndex;

    CMbcsBuffer mbcsFile;
    if (!mbcsFile.FromUnicode(lpofn->lpstrFile)) 
        return FALSE;
    ofnA.lpstrFile = mbcsFile;
    ofnA.nMaxFile = mbcsFile.BufferSize();

    CMbcsBuffer mbcsFileTitle;
    if (!mbcsFileTitle.FromUnicode(lpofn->lpstrFileTitle, -1, lpofn->nMaxFileTitle * 2)) 
        return FALSE;
    ofnA.lpstrFileTitle = mbcsFileTitle;
    ofnA.nMaxFileTitle = mbcsFileTitle.BufferSize();

    CMbcsBuffer mbcsInitialDir;
    if (!mbcsInitialDir.FromUnicode(lpofn->lpstrInitialDir)) 
        return FALSE;
    ofnA.lpstrInitialDir = mbcsInitialDir;

    CMbcsBuffer mbcsTitle;
    if (!mbcsTitle.FromUnicode(lpofn->lpstrTitle)) 
        return FALSE;
    ofnA.lpstrTitle = mbcsTitle;
    
    ofnA.Flags = lpofn->Flags;
    // nFileOffset
    // nFileExtension
    
    CMbcsBuffer mbcsDefExt;
    if (!mbcsDefExt.FromUnicode(lpofn->lpstrDefExt)) 
        return FALSE;
    ofnA.lpstrDefExt = mbcsDefExt;

    ofnA.lCustData = lpofn->lCustData;
    ofnA.lpfnHook = lpofn->lpfnHook;

    CMbcsBuffer mbcsTemplateName;
    if (!mbcsTemplateName.FromUnicode(lpofn->lpTemplateName)) 
        return FALSE;
    ofnA.lpTemplateName = mbcsTemplateName;

    BOOL success;
    if (aIsOpen)
        success = ::GetOpenFileNameA(&ofnA);
    else
        success = ::GetSaveFileNameA(&ofnA);
    if (!success)
        return success;

    if (ofnA.lpstrFile) {
        int fileLen = ::lstrlenA(ofnA.lpstrFile);
        if (fileLen > 0 && (ofnA.Flags & OFN_ALLOWMULTISELECT))
        {
            // lpstrFile contains the directory and file name strings 
            // which are NULL separated, with an extra NULL character after the last file name. 
            int totalLen = 0;
            while (fileLen > 0) {
                totalLen += fileLen + 1;
                fileLen = ::lstrlenA(ofnA.lpstrFile + totalLen);
            }
            fileLen = totalLen;
        }
        ++fileLen; // add null character

        // ensure our buffer is big enough
        int lenRequired = ::MultiByteToWideChar(CP_ACP, 0, ofnA.lpstrFile, fileLen, 0, 0);
        if (lpofn->nMaxFile < (DWORD) lenRequired)
            return FALSE; // doesn't have enough allocated space

        // return these files
        ::MultiByteToWideChar(CP_ACP, 0, ofnA.lpstrFile, fileLen, lpofn->lpstrFile, lpofn->nMaxFile);
    }

    lpofn->nFilterIndex = ofnA.nFilterIndex;

    //TODO: these should be set correctly
    lpofn->nFileOffset = 0;
    lpofn->nFileExtension = 0;

    return TRUE;
}


EXTERN_C {

OCOW_DEF(BOOL, GetOpenFileNameW, (LPOPENFILENAMEW lpofn))
{
    return OpenSaveFileNameW(lpofn, TRUE);
}

OCOW_DEF(BOOL, GetSaveFileNameW, (LPOPENFILENAMEW lpofn))
{
    return OpenSaveFileNameW(lpofn, FALSE);
}

}//EXTERN_C 

// PageSetupDlgW
// PrintDlgW
// ReplaceTextW
