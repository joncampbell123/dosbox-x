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
#define _SHELL32_

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "MbcsBuffer.h"


// ----------------------------------------------------------------------------
// API

// DragQueryFileW
// ExtractIconExW
// ExtractIconW
// FindExecutableW

EXTERN_C {

OCOW_DEF(LPITEMIDLIST, SHBrowseForFolderW,
    (LPBROWSEINFOW lpbi)
    )
{
    char mbcsDisplayName[MAX_PATH];

    BROWSEINFOA biA;
    ::ZeroMemory(&biA, sizeof(biA));

    biA.hwndOwner       = lpbi->hwndOwner;
    biA.pidlRoot        = lpbi->pidlRoot;
    biA.ulFlags         = lpbi->ulFlags;
    biA.lpfn            = lpbi->lpfn;
    biA.lParam          = lpbi->lParam;
    biA.iImage          = lpbi->iImage;
    biA.pszDisplayName  = mbcsDisplayName;

    CMbcsBuffer mbcsTitle;
    if (!mbcsTitle.FromUnicode(lpbi->lpszTitle))
        return NULL;
    biA.lpszTitle = mbcsTitle;

    LPITEMIDLIST result = ::SHBrowseForFolderA(&biA);
    if (!result)
        return NULL;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsDisplayName, -1, lpbi->pszDisplayName, MAX_PATH);
    return result;
}

// SHChangeNotify
// SHFileOperationW
// SHGetFileInfoW
// SHGetNewLinkInfoW

OCOW_DEF(BOOL, SHGetPathFromIDListW,
    (LPCITEMIDLIST pidl, 
    LPWSTR pszPath
    ))
{
    char mbcsPath[MAX_PATH];
    BOOL success = ::SHGetPathFromIDListA(pidl, mbcsPath);
    if (!success)
        return FALSE;

    int nResult = ::MultiByteToWideChar(CP_ACP, 0, mbcsPath, -1, pszPath, MAX_PATH);
    if (nResult < 1)
        return FALSE;

    return TRUE;
}

OCOW_DEF(INT, ShellAboutW,
    (HWND hWnd, 
    LPCWSTR szApp, 
    LPCWSTR szOtherStuff, 
    HICON hIcon
    ))
{
    CMbcsBuffer mbcsApp;
    if (!mbcsApp.FromUnicode(szApp))
        return 0;

    CMbcsBuffer mbcsOtherStuff;
    if (!mbcsOtherStuff.FromUnicode(szOtherStuff))
        return 0;

    return ::ShellAboutA(hWnd, mbcsApp, mbcsOtherStuff, hIcon);
}

// ShellExecuteExW

OCOW_DEF(HINSTANCE, ShellExecuteW,
    (HWND hwnd, 
    LPCWSTR lpOperation, 
    LPCWSTR lpFile, 
    LPCWSTR lpParameters, 
    LPCWSTR lpDirectory, 
    INT nShowCmd
    ))
{
    CMbcsBuffer mbcsOperation;
    if (!mbcsOperation.FromUnicode(lpOperation))
        return 0;

    CMbcsBuffer mbcsFile;
    if (!mbcsFile.FromUnicode(lpFile))
        return 0;

    CMbcsBuffer mbcsParameters;
    if (!mbcsParameters.FromUnicode(lpParameters))
        return 0;

    CMbcsBuffer mbcsDirectory;
    if (!mbcsDirectory.FromUnicode(lpDirectory))
        return 0;

    return ::ShellExecuteA(hwnd, mbcsOperation, mbcsFile, mbcsParameters, mbcsDirectory, nShowCmd);
}

// Shell_NotifyIconW
}//EXTERN_C