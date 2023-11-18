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
#include <sensapi.h>

#include "MbcsBuffer.h"

// ----------------------------------------------------------------------------
// Globals

extern HMODULE g_hOleacc;

// ----------------------------------------------------------------------------
// Macros

#define LOAD_FUNCTION(func, err)                                    \
    static fp##func p##func = 0;                                    \
    if (!p##func) {                                                 \
        if (!g_hOleacc) {                                           \
            g_hOleacc = ::LoadLibraryA("oleacc.dll");               \
            if (!g_hOleacc) {                                       \
                SetLastError(ERROR_CALL_NOT_IMPLEMENTED);           \
                return err;                                         \
            }                                                       \
        }                                                           \
        p##func = (fp##func) ::GetProcAddress(g_hOleacc, #func);    \
        if (!p##func) {                                             \
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);               \
            return err;                                             \
        }                                                           \
    }


// ----------------------------------------------------------------------------
// API

typedef HRESULT (STDAPICALLTYPE *fpCreateStdAccessibleProxyA)(
    HWND hwnd, 
    LPCSTR pClassName, 
    LONG idObject, 
    REFIID riid, 
    void** ppvObject);

EXTERN_C {
OCOW_DEF(HRESULT, CreateStdAccessibleProxyW,
    (HWND hwnd, 
    LPCWSTR pClassName, 
    LONG idObject, 
    REFIID riid, 
    void** ppvObject
    ))
{
    LOAD_FUNCTION(CreateStdAccessibleProxyA, E_NOTIMPL)

    CMbcsBuffer mbcsClassName;
    if (!mbcsClassName.FromUnicode(pClassName))
        return FALSE;

    return pCreateStdAccessibleProxyA(hwnd, mbcsClassName, idObject, riid, ppvObject);
}

typedef UINT (STDAPICALLTYPE *fpGetRoleTextA)(
    DWORD lRole, 
    LPSTR lpszRole, 
    UINT cchRoleMax);

OCOW_DEF(UINT, GetRoleTextW,
    (DWORD lRole, 
    LPWSTR lpszRole, 
    UINT cchRoleMax
    ))
{
    LOAD_FUNCTION(GetRoleTextA, 0)

    CMbcsBuffer mbcsRole;
    UINT uiLen = pGetRoleTextA(lRole, NULL, 0);
    if (!uiLen)
        return 0;
    if (!mbcsRole.SetCapacity((int) uiLen + 1))
        return 0;

    uiLen = pGetRoleTextA(lRole, mbcsRole, mbcsRole.BufferSize());
    int nRequiredLen = ::MultiByteToWideChar(CP_ACP, 0, mbcsRole, (int) uiLen + 1, 0, 0);
    if (!lpszRole)
        return (UINT) (nRequiredLen - 1);
    if (cchRoleMax < (UINT) nRequiredLen) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    ::MultiByteToWideChar(CP_ACP, 0, mbcsRole, (int) uiLen + 1, lpszRole, cchRoleMax);
    return (UINT) (nRequiredLen - 1);
}

typedef UINT (STDAPICALLTYPE *fpGetStateTextA)(
    DWORD lStateBit, 
    LPSTR lpszState, 
    UINT cchState);

OCOW_DEF(UINT, GetStateTextW,
    (DWORD lStateBit, 
    LPWSTR lpszState, 
    UINT cchState
    ))
{
    LOAD_FUNCTION(GetStateTextA, 0)

    CMbcsBuffer mbcsState;
    UINT uiLen = pGetStateTextA(lStateBit, NULL, 0);
    if (!uiLen)
        return 0;
    if (!mbcsState.SetCapacity((int) uiLen + 1))
        return 0;

    uiLen = pGetStateTextA(lStateBit, mbcsState, mbcsState.BufferSize());
    int nRequiredLen = ::MultiByteToWideChar(CP_ACP, 0, mbcsState, (int) uiLen + 1, 0, 0);
    if (!lpszState)
        return (UINT) (nRequiredLen - 1);
    if (cchState < (UINT) nRequiredLen) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    ::MultiByteToWideChar(CP_ACP, 0, mbcsState, (int) uiLen + 1, lpszState, cchState);
    return (UINT) (nRequiredLen - 1);
}
}//EXTERN_C