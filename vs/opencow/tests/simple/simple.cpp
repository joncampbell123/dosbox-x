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

#include <windows.h>
#include <tchar.h>

#define UNUSED_ARG(x)	((void)(x))

#ifdef _DEBUG
# define OPENCOW_DLL    "opencowD.dll"
#else
# define OPENCOW_DLL    "opencow.dll"
#endif

extern "C" HMODULE __stdcall 
LoadOpencowLibrary(void)
{
    HMODULE hModule = ::LoadLibraryA(".\\" OPENCOW_DLL);
    if (hModule)
        ::MessageBoxA(NULL, "Loaded .\\" OPENCOW_DLL, "simple", MB_OK);
    else
        ::MessageBoxA(NULL, "Failed to load .\\" OPENCOW_DLL, "simple", MB_OK);
    return hModule;
}
extern "C" HMODULE (__stdcall *_PfnLoadUnicows)(void) = &LoadOpencowLibrary;

int _tmain(int argc, _TCHAR* argv[])
{
	UNUSED_ARG(argc);
	UNUSED_ARG(argv);

    TCHAR szCurrentDir[MAX_PATH];
    szCurrentDir[0] = 0;
    GetCurrentDirectory(MAX_PATH, szCurrentDir);

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(_T("*.*"), &findData);
    if (hFind) {
        BOOL bContinue = TRUE;
        while (bContinue) {
            if (MessageBox(NULL, findData.cFileName, szCurrentDir, MB_OKCANCEL) == IDCANCEL)
                bContinue = FALSE;
            else
                bContinue = FindNextFile(hFind, &findData);
        }
        FindClose(hFind);
    }
    else {
        MessageBox(NULL, _T("No files"), _T("simple"), MB_OK);
    }

	return 0;
}

