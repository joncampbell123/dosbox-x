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
#define _USER32_

#include <windows.h>
#if defined(__MINGW64_VERSION_MAJOR)
#include <crtdbg.h>
#else
#define _ASSERTE(...) 
#endif

#include "MbcsBuffer.h"


// ----------------------------------------------------------------------------
// API
EXTERN_C {
// AppendMenuW
// BroadcastSystemMessageW
// CallMsgFilterW
// CallWindowProcA

OCOW_DEF(BOOL, AppendMenuW, (HMENU hMenu,UINT uFlags,UINT_PTR uIDNewItem,LPCWSTR lpNewItem))
{
    CMbcsBuffer buff;
    if(!buff.FromUnicode(lpNewItem))
        return FALSE;
    return ::AppendMenuA(hMenu, uFlags, uIDNewItem, buff);
}

OCOW_DEF(LRESULT, CallWindowProcW,
    (IN WNDPROC lpPrevWndFunc,
    IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
    ))
{
    //TODO: is this sufficient?
    return ::CallWindowProcA(lpPrevWndFunc, hWnd, Msg, wParam, lParam);
}

// ChangeDisplaySettingsExW
// ChangeDisplaySettingsW
// ChangeMenuW

OCOW_DEF(DWORD, CharLowerBuffW,
    (IN OUT LPWSTR lpsz,
    IN DWORD cchLength
    ))
{
    wchar_t c;
    for (DWORD dwCount = 0; dwCount < cchLength; ++dwCount) {
        c = lpsz[dwCount];
        if (c >= 'A' && c <= 'Z')
            lpsz[dwCount] = (unsigned short) ((c - 'A') + 'a');
    }
    return cchLength;
}

OCOW_DEF(LPWSTR, CharLowerW,
    (IN OUT LPWSTR lpsz)
    )
{
    if (HIWORD(lpsz) == 0) {
        wchar_t c = LOWORD(lpsz);
        if (c >= 'A' && c <= 'Z')
            c = (unsigned short) ((c - 'A') + 'a');
        return (LPWSTR) c;
    }
    else {
        wchar_t c;
        for (wchar_t *pTemp = lpsz; *pTemp; ++pTemp) {
            c = *pTemp;
            if (c >= 'A' && c <= 'Z')
                *pTemp = (unsigned short) ((c - 'A') + 'a');
        }
        return lpsz;
    }
}

OCOW_DEF(LPWSTR, CharNextW,
    (IN LPCWSTR lpsz)
    )
{
    if (*lpsz)
        ++lpsz;
    return const_cast<LPWSTR>(lpsz);
}

OCOW_DEF(LPWSTR, CharPrevW,
    (IN LPCWSTR lpszStart,
    IN LPCWSTR lpszCurrent
    ))
{
    if (lpszStart < lpszCurrent)
        --lpszCurrent;
    return const_cast<LPWSTR>(lpszCurrent);
}

// CharToOemBuffW
// CharToOemW

OCOW_DEF(DWORD, CharUpperBuffW,
    (IN OUT LPWSTR lpsz,
    IN DWORD cchLength
    ))
{
    wchar_t c;
    for (DWORD dwCount = 0; dwCount < cchLength; ++dwCount) {
        c = lpsz[dwCount];
        if (c >= 'a' && c <= 'z')
            lpsz[dwCount] = (unsigned short) ((c - 'a') + 'Z');
    }
    return cchLength;
}

OCOW_DEF(LPWSTR, CharUpperW,
    (IN OUT LPWSTR lpsz)
    )
{
    if (HIWORD(lpsz) == 0) {
        wchar_t c = LOWORD(lpsz);
        if (c >= 'a' && c <= 'z')
            c = (unsigned short) ((c - 'a') + 'A');
        return (LPWSTR) c;
    }
    else {
        wchar_t c;
        for (wchar_t *pTemp = lpsz; *pTemp; ++pTemp) {
            c = *pTemp;
            if (c >= 'a' && c <= 'z')
                *pTemp = (unsigned short) ((c - 'a') + 'A');
        }
        return lpsz;
    }
}

// CopyAcceleratorTableW
// CreateAcceleratorTableW
// CreateDialogIndirectParamW
// CreateDialogParamW
// CreateMDIWindowW
 
OCOW_DEF(HWND, CreateWindowExW,
    (IN DWORD dwExStyle,
    IN LPCWSTR lpClassName,
    IN LPCWSTR lpWindowName,
    IN DWORD dwStyle,
    IN int X,
    IN int Y,
    IN int nWidth,
    IN int nHeight,
    IN HWND hWndParent,
    IN HMENU hMenu,
    IN HINSTANCE hInstance,
    IN LPVOID lpParam
    ))
{
    // class name is only a string if the HIWORD is not 0
    CMbcsBuffer mbcsClassName;
    LPCSTR pmbcsClassName = (LPCSTR) lpClassName;
    if (HIWORD(lpClassName) != 0) {
        if (!mbcsClassName.FromUnicode(lpClassName))
            return NULL;
        pmbcsClassName = mbcsClassName;
    }

    CMbcsBuffer mbcsWindowName;
    if (!mbcsWindowName.FromUnicode(lpWindowName))
        return NULL;

    return ::CreateWindowExA(dwExStyle, pmbcsClassName, mbcsWindowName,
        dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

// DdeConnect
// DdeConnectList
// DdeCreateStringHandleW
// DdeInitializeW
// DdeQueryConvInfo
// DdeQueryStringW
// DefDlgProcW
// DefFrameProcW
// DefMDIChildProcW

OCOW_DEF(LRESULT, DefWindowProcW,
    (IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
    ))
{
    //TODO: is this sufficient?
    return ::DefWindowProcA(hWnd, Msg, wParam, lParam);
}

// DialogBoxIndirectParamW
// DialogBoxParamW

OCOW_DEF(LRESULT, DispatchMessageW,
    (IN CONST MSG *lpMsg
    ))
{
    //TODO: is this sufficient?
    return ::DispatchMessageA(lpMsg);
}

// DlgDirListComboBoxW
// DlgDirListW
// DlgDirSelectComboBoxExW
// DlgDirSelectExW
// DrawStateW
// DrawTextExW
// DrawTextW
// EnableWindow
// EnumClipboardFormats
// EnumDisplayDevicesW
// EnumDisplaySettingsExW
// EnumDisplaySettingsW
// EnumPropsA
// EnumPropsExA
// EnumPropsExW
// EnumPropsW
// FindWindowExW
// FindWindowW
// GetAltTabInfoW
// GetClassInfoExW
// GetClassInfoW
// GetClassLongW

OCOW_DEF(int, GetClassNameW,
    (IN HWND hWnd,
    OUT LPWSTR lpClassName,
    IN int nMaxCount
    ))
{
    CMbcsBuffer mbcsClassName;
    if (!mbcsClassName.SetCapacity(nMaxCount*2))
        return 0;

    int nLen = ::GetClassNameA(hWnd, mbcsClassName, mbcsClassName.BufferSize());
    if (nLen < 1)
        return nLen;

    // truncate the returned string into the output buffer
    ++nLen;
    int nResult;
    do {
        nResult = ::MultiByteToWideChar(CP_ACP, 0, mbcsClassName, nLen--, lpClassName, nMaxCount);
    }
    while (nLen > 0 && nResult == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    return nResult;
}

// GetClipboardData

OCOW_DEF(int, GetClipboardFormatNameW,
    (IN UINT format,
    OUT LPWSTR lpszFormatName,
    IN int cchMaxCount
    ))
{
    CMbcsBuffer mbcsFormatName;
    if (!mbcsFormatName.SetCapacity(cchMaxCount))
        return 0;

    int nLen = ::GetClipboardFormatNameA(format, mbcsFormatName, mbcsFormatName.BufferSize());
    if (nLen < 1)
        return nLen;

    // truncate the returned string into the output buffer
    ++nLen;
    int nResult;
    do {
        nResult = ::MultiByteToWideChar(CP_ACP, 0, mbcsFormatName, nLen--, lpszFormatName, cchMaxCount);
    }
    while (nLen > 0 && nResult == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    return nResult;
}

// GetDlgItemTextW
// GetKeyNameTextW
// GetKeyboardLayoutNameW
// GetMenuItemInfoW
// GetMenuStringW

OCOW_DEF(BOOL, GetMessageW,
    (OUT LPMSG lpMsg,
    IN HWND hWnd,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax
    ))
{
    //TODO: is this sufficient?
    return ::GetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}

// GetMonitorInfoW
// GetPropA
// GetPropW
// GetTabbedTextExtentW
// GetWindowLongA
// GetWindowLongW
// GetWindowModuleFileNameW
// GetWindowTextLengthW
// GetWindowTextW
// GrayStringW
// InsertMenuItemW
// InsertMenuW
// IsCharAlphaNumericW
// IsCharAlphaW
// IsCharLowerW
// IsCharUpperW
// IsClipboardFormatAvailable
// IsDialogMessageW
// IsWindowUnicode
// LoadAcceleratorsW
// LoadBitmapW
// LoadCursorFromFileW
// LoadCursorW
// LoadIconW
// LoadImageW
// LoadKeyboardLayoutW
// LoadMenuIndirectW
// LoadMenuW
// LoadStringW
// MapVirtualKeyExW
// MapVirtualKeyW

OCOW_DEF(BOOL, InsertMenuItemW, (HMENU hMenu,UINT item,BOOL fByPosition,LPCMENUITEMINFOW lpmi))
{
    if(!lpmi) return FALSE;
    
    CMbcsBuffer buff;

    MENUITEMINFOA mbsmi = 
    {
        lpmi->cbSize,
        lpmi->fMask,
        lpmi->fType,
        lpmi->fState,
        lpmi->wID,
        lpmi->hSubMenu,
        lpmi->hbmpChecked,
        lpmi->hbmpUnchecked,
        lpmi->dwItemData,
        (LPSTR)lpmi->dwTypeData,
        lpmi->cch,
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
        lpmi->hbmpItem,
#endif
    };
    if((lpmi->fMask&MIIM_STRING))
    {
        if(!buff.FromUnicode(lpmi->dwTypeData))
            return FALSE;
        mbsmi.dwTypeData = (LPSTR)buff;
    }
    return ::InsertMenuItemA(hMenu,item,fByPosition,&mbsmi);
}

OCOW_DEF(BOOL, InsertMenuW, (HMENU hMenu,UINT uPosition,UINT uFlags,UINT_PTR uIDNewItem,LPCWSTR lpNewItem))
{
    CMbcsBuffer buff;
    if(!buff.FromUnicode(lpNewItem))
        return FALSE;
    return ::InsertMenuA(hMenu,uPosition,uFlags,uIDNewItem,buff);
}

OCOW_DEF(int, MessageBoxExW,
    (IN HWND hWnd,
    IN LPCWSTR lpText,
    IN LPCWSTR lpCaption,
    IN UINT uType,
    IN WORD wLanguageId
    ))
{
    CMbcsBuffer mbcsText;
    if (!mbcsText.FromUnicode(lpText))
        return 0;

    CMbcsBuffer mbcsCaption;
    if (!mbcsCaption.FromUnicode(lpCaption))
        return 0;

    return ::MessageBoxExA(hWnd, mbcsText, mbcsCaption, uType, wLanguageId);
}

// MessageBoxIndirectW

OCOW_DEF(int, MessageBoxW,
    (IN HWND hWnd,
    IN LPCWSTR lpText,
    IN LPCWSTR lpCaption,
    IN UINT uType
    ))
{
    CMbcsBuffer mbcsText;
    if (!mbcsText.FromUnicode(lpText))
        return 0;

    CMbcsBuffer mbcsCaption;
    if (!mbcsCaption.FromUnicode(lpCaption))
        return 0;

    return ::MessageBoxA(hWnd, mbcsText, mbcsCaption, uType);
}

// ModifyMenuW
// OemToCharBuffW
// OemToCharW

OCOW_DEF(BOOL, PeekMessageW,
    (OUT LPMSG lpMsg,
    IN HWND hWnd,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax,
    IN UINT wRemoveMsg
    ))
{
    //TODO: is this sufficient?
    return ::PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

OCOW_DEF(BOOL, PostMessageW,
  (IN HWND   hWnd,
  IN UINT   Msg,
  IN WPARAM wParam,
  IN LPARAM lParam
))
{
    return ::PostMessageA(hWnd, Msg, wParam, lParam);
}

OCOW_DEF(BOOL, PostThreadMessageW,
  (IN DWORD idThread,
  IN UINT   Msg,
  IN WPARAM wParam,
  IN LPARAM lParam
))
{
    return ::PostThreadMessageA(idThread, Msg, wParam, lParam);
}

// RegisterClassExW

OCOW_DEF(ATOM, RegisterClassW,
    (IN CONST WNDCLASSW *lpWndClass)
    )
{
    WNDCLASSA wndClassA;
    ::ZeroMemory(&wndClassA, sizeof(wndClassA));

    wndClassA.style         = lpWndClass->style;
    wndClassA.lpfnWndProc   = lpWndClass->lpfnWndProc;
    wndClassA.cbClsExtra    = lpWndClass->cbClsExtra;
    wndClassA.cbWndExtra    = lpWndClass->cbWndExtra;
    wndClassA.hInstance     = lpWndClass->hInstance;
    wndClassA.hIcon         = lpWndClass->hIcon;
    wndClassA.hCursor       = lpWndClass->hCursor;
    wndClassA.hbrBackground = lpWndClass->hbrBackground;

    CMbcsBuffer mbcsMenuName;
    if (lpWndClass->lpszMenuName) {
        if (!mbcsMenuName.FromUnicode(lpWndClass->lpszMenuName))
            return 0;
        wndClassA.lpszMenuName = mbcsMenuName;
    }

    // class name is only a string if the HIWORD is not 0
    CMbcsBuffer mbcsClassName;
    wndClassA.lpszClassName = (LPCSTR) lpWndClass->lpszClassName;
    if (HIWORD(lpWndClass->lpszClassName) != 0) {
        if (!mbcsClassName.FromUnicode(lpWndClass->lpszClassName))
            return NULL;
        wndClassA.lpszClassName = mbcsClassName;
    }

    return ::RegisterClassA(&wndClassA);
}

OCOW_DEF(UINT, RegisterClipboardFormatW,
    (IN LPCWSTR lpszFormat)
    )
{
    CMbcsBuffer mbcsFormat;
    if (!mbcsFormat.FromUnicode(lpszFormat))
        return 0;

    return ::RegisterClipboardFormatA(mbcsFormat);
}

// RegisterDeviceNotificationW
// RegisterWindowMessageW
// RemovePropA
// RemovePropW
// SendDlgItemMessageW
// SendMessageCallbackW

OCOW_DEF(LRESULT, SendMessageTimeoutW,
    (IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam,
    IN UINT fuFlags,
    IN UINT uTimeout,
    OUT PDWORD_PTR lpdwResult
    ))
{
    // only the following message types have been verified, before using
    // other message types ensure that they are implemented correctly
    _ASSERTE(
        Msg == WM_SETTEXT || 
        Msg == WM_SETICON || 
        Msg == WM_SETFONT ||
        Msg == WM_SETTINGCHANGE );

    if (Msg == WM_SETTEXT) {
        CMbcsBuffer mbcsString;
        if (!mbcsString.FromUnicode((LPWSTR)lParam))
            return ::SendMessageTimeoutA(hWnd, WM_SETTEXT, 0, 0, 
                fuFlags, uTimeout, lpdwResult); // error value depends on hWnd

        return ::SendMessageTimeoutA(hWnd, WM_SETTEXT, wParam, (LPARAM) mbcsString.get(), 
            fuFlags, uTimeout, lpdwResult);
    }

    if (Msg == WM_SETTINGCHANGE) {
        CMbcsBuffer mbcsString;
        if (!mbcsString.FromUnicode((LPWSTR)lParam))
            mbcsString.SetNull();   // don't set the string
        return ::SendMessageTimeoutA(hWnd, WM_SETTINGCHANGE, wParam, (LPARAM) mbcsString.get(), 
            fuFlags, uTimeout, lpdwResult);
    }

    return ::SendMessageTimeoutA(hWnd, Msg, wParam, lParam, 
        fuFlags, uTimeout, lpdwResult);
}

OCOW_DEF(LRESULT, SendMessageW,
    (IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
    ))
{
    // only the following message types have been verified, before using
    // other message types ensure that they are implemented correctly
    _ASSERTE(
        Msg == WM_SETTEXT || 
        Msg == WM_SETICON || 
        Msg == WM_SETFONT ||
        Msg == WM_SETTINGCHANGE );

    if (Msg == WM_SETTEXT) {
        CMbcsBuffer mbcsString;
        if (!mbcsString.FromUnicode((LPWSTR)lParam))
            return ::SendMessageA(hWnd, WM_SETTEXT, 0, 0); // error value depends on hWnd

        return ::SendMessageA(hWnd, WM_SETTEXT, wParam, (LPARAM) mbcsString.get());
    }

    if (Msg == WM_SETTINGCHANGE) {
        CMbcsBuffer mbcsString;
        if (!mbcsString.FromUnicode((LPWSTR)lParam))
            mbcsString.SetNull();   // don't set the string
        return ::SendMessageA(hWnd, WM_SETTINGCHANGE, wParam, (LPARAM) mbcsString.get());
    }

    return ::SendMessageA(hWnd, Msg, wParam, lParam);
}

// SendNotifyMessageW
// SetClassLongW
// SetDlgItemTextW
// SetMenuItemInfoW
// SetPropA
// SetPropW
// SetWindowLongA
// SetWindowLongW
// SetWindowTextW
// SetWindowsHookExW
// SetWindowsHookW
// SystemParametersInfoW
// TabbedTextOutW
// TranslateAcceleratorW

OCOW_DEF(BOOL, UnregisterClassW,
    (IN LPCWSTR lpClassName,
    IN HINSTANCE hInstance
    ))
{
    // class name is only a string if the HIWORD is not 0
    CMbcsBuffer mbcsClassName;
    LPCSTR pmbcsClassName = (LPCSTR) lpClassName;
    if (HIWORD(lpClassName) != 0) {
        if (!mbcsClassName.FromUnicode(lpClassName))
            return NULL;
        pmbcsClassName = mbcsClassName;
    }

    return ::UnregisterClassA(pmbcsClassName, hInstance);
}

// VkKeyScanExW
// VkKeyScanW
// WinHelpW
// wsprintfW
// wvsprintfW
}//EXTERN_C 