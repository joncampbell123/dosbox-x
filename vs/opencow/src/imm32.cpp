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
#if !defined(STATIC_OPENCOW)
#define _KERNEL32_
#endif

// ensure that the IMM header doesn't get included yet by defining the
// guard for it
#define _IMM_
#include <windows.h>

#include "MbcsBuffer.h"
#include "SystemVersion.h"

// we want all functions from imm.h to be defined
#undef WINVER
#define WINVER  0x0500

#undef _IMM_
#include <imm.h>

// ----------------------------------------------------------------------------
// Globals

extern HMODULE g_hImm32;

// ----------------------------------------------------------------------------
// Macros

#define ENSURE_LIBRARY                                  \
    if (!g_hImm32) {                                    \
        g_hImm32 = ::LoadLibraryA("imm32.dll");         \
        if (!g_hImm32) {                                \
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);   \
            return 0;                                   \
        }                                               \
    }

#define CALL_NATIVE_UNICODE_VERSION(func, params, err)          \
    if (g_nPlatform != MZ_PLATFORM_95) {                        \
        static fp##func##W p##func##W =                         \
            (fp##func##W) ::GetProcAddress(g_hImm32, #func "W");\
        if (!p##func##W) {                                      \
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);           \
            return err;                                         \
        }                                                       \
        return p##func##W params;                               \
    }

#define LOAD_ANSI_VERSION(func, err)                            \
    static fp##func##A p##func##A =                             \
        (fp##func##A) ::GetProcAddress(g_hImm32, #func "A");    \
    if (!p##func##A) {                                          \
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);               \
        return err;                                             \
    }                                                           \

#define IMPLEMENT_IMM_FUNCTION(func, params, err)       \
    ENSURE_LIBRARY                                      \
    CALL_NATIVE_UNICODE_VERSION(Imm##func, params, err) \
    LOAD_ANSI_VERSION(Imm##func, err)


// ----------------------------------------------------------------------------
// API
EXTERN_C {
typedef HKL (WINAPI *fpImmInstallIMEA)(IN LPCSTR lpszIMEFileName, IN LPCSTR lpszLayoutText);
typedef HKL (WINAPI *fpImmInstallIMEW)(IN LPCWSTR lpszIMEFileName, IN LPCWSTR lpszLayoutText);

OCOW_DEF(HKL, ImmInstallIMEW,
    (IN LPCWSTR lpszIMEFileName, 
    IN LPCWSTR lpszLayoutText
    ))
{
    IMPLEMENT_IMM_FUNCTION(InstallIME, 
        (lpszIMEFileName, lpszLayoutText), 0)

    // --- Windows 95 only ---

    CMbcsBuffer mbcsIMEFileName;
    if (!mbcsIMEFileName.FromUnicode(lpszIMEFileName))
        return 0;

    CMbcsBuffer mbcsLayoutText;
    if (!mbcsLayoutText.FromUnicode(lpszLayoutText))
        return 0;

    return pImmInstallIMEA(mbcsIMEFileName, mbcsLayoutText);
}

typedef UINT (WINAPI *fpImmGetDescriptionA)(IN HKL, OUT LPSTR, IN UINT uBufLen);
typedef UINT (WINAPI *fpImmGetDescriptionW)(IN HKL, OUT LPWSTR, IN UINT uBufLen);

OCOW_DEF(UINT, ImmGetDescriptionW,
    (IN HKL hKL, 
    OUT LPWSTR lpszDescription, 
    IN UINT uBufLen
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetDescription, 
        (hKL, lpszDescription, uBufLen), 0)

    // --- Windows 95 only ---

    UINT uiLen = pImmGetDescriptionA(hKL, 0, 0);

    CMbcsBuffer mbcsDescription;
    mbcsDescription.SetCapacity((int) uiLen + 1);

    uiLen = pImmGetDescriptionA(hKL, mbcsDescription, (UINT) mbcsDescription.BufferSize());
    if (!uiLen)
        return 0;

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsDescription, (int) uiLen + 1, 0, 0);
    if (uBufLen < 1)
        return (UINT) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsDescription, (int) uiLen + 1, lpszDescription, uBufLen);
    lpszDescription[uBufLen - 1] = L'\0';
    return wcslen(lpszDescription); // do not include the NULL 
}

typedef UINT (WINAPI *fpImmGetIMEFileNameA)(IN HKL, OUT LPSTR, IN UINT uBufLen);
typedef UINT (WINAPI *fpImmGetIMEFileNameW)(IN HKL, OUT LPWSTR, IN UINT uBufLen);

OCOW_DEF(UINT, ImmGetIMEFileNameW,
    (IN HKL hKL, 
    OUT LPWSTR lpszFileName, 
    IN UINT uBufLen
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetIMEFileName, 
        (hKL, lpszFileName, uBufLen), 0)

    // --- Windows 95 only ---

    UINT uiLen = pImmGetIMEFileNameA(hKL, 0, 0);

    CMbcsBuffer mbcsFileName;
    mbcsFileName.SetCapacity((int) uiLen + 1);

    uiLen = pImmGetIMEFileNameA(hKL, mbcsFileName, (UINT) mbcsFileName.BufferSize());
    if (!uiLen)
        return 0;

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsFileName, (int) uiLen + 1, 0, 0);
    if (uBufLen < 1)
        return (UINT) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsFileName, (int) uiLen + 1, lpszFileName, uBufLen);
    lpszFileName[uBufLen - 1] = L'\0';
    return wcslen(lpszFileName); // do not include the NULL 
}

// ----------------------------------------------------------------------------
// NOT YET IMPLEMENTED

typedef LONG (WINAPI *fpImmGetCompositionStringA)(IN HIMC, IN DWORD, OUT LPVOID, IN DWORD);
typedef LONG (WINAPI *fpImmGetCompositionStringW)(IN HIMC, IN DWORD, OUT LPVOID, IN DWORD);

OCOW_DEF(LONG, ImmGetCompositionStringW,
    (IN HIMC hIMC,      
    IN DWORD dwIndex,  
    OUT LPVOID lpBuf,   
    IN DWORD dwBufLen  
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetCompositionString, 
        (hIMC, dwIndex, lpBuf, dwBufLen), IMM_ERROR_GENERAL)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return IMM_ERROR_GENERAL;
}

typedef BOOL (WINAPI *fpImmSetCompositionStringA)(IN HIMC, IN DWORD dwIndex, IN LPVOID lpComp, IN DWORD, IN LPVOID lpRead, IN DWORD);
typedef BOOL (WINAPI *fpImmSetCompositionStringW)(IN HIMC, IN DWORD dwIndex, IN LPVOID lpComp, IN DWORD, IN LPVOID lpRead, IN DWORD);

OCOW_DEF(BOOL, ImmSetCompositionStringW,
    (IN HIMC hIMC,        
    IN DWORD dwIndex,    
#if defined(__MINGW64_VERSION_MAJOR)
    IN LPVOID lpComp,    
#else
    IN PCVOID lpComp,    
#endif
    IN DWORD dwCompLen,  
#if defined(__MINGW64_VERSION_MAJOR)
    IN LPVOID lpRead,    
#else
    IN PCVOID lpRead,    
#endif
    IN DWORD dwReadLen   
    ))
{
    IMPLEMENT_IMM_FUNCTION(SetCompositionString, 
        (hIMC, dwIndex, (LPVOID)lpComp, dwCompLen, (LPVOID)lpRead, dwReadLen), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef DWORD (WINAPI *fpImmGetCandidateListCountA)(IN HIMC, OUT LPDWORD lpdwListCount);
typedef DWORD (WINAPI *fpImmGetCandidateListCountW)(IN HIMC, OUT LPDWORD lpdwListCount);

OCOW_DEF(DWORD, ImmGetCandidateListCountW,
    (IN HIMC hIMC,        
    OUT LPDWORD lpdwListCount
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetCandidateListCount, 
        (hIMC, lpdwListCount), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef DWORD (WINAPI *fpImmGetCandidateListA)(IN HIMC, IN DWORD dwIndex, OUT LPCANDIDATELIST, IN DWORD dwBufLen);
typedef DWORD (WINAPI *fpImmGetCandidateListW)(IN HIMC, IN DWORD dwIndex, OUT LPCANDIDATELIST, IN DWORD dwBufLen);

OCOW_DEF(DWORD, ImmGetCandidateListW,
    (IN HIMC hIMC,        
    IN DWORD dwIndex, 
    OUT LPCANDIDATELIST lpCandList, 
    IN DWORD dwBufLen
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetCandidateList, 
        (hIMC, dwIndex, lpCandList, dwBufLen), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef DWORD (WINAPI *fpImmGetGuideLineA)(IN HIMC, IN DWORD dwIndex, OUT LPSTR, IN DWORD dwBufLen);
typedef DWORD (WINAPI *fpImmGetGuideLineW)(IN HIMC, IN DWORD dwIndex, OUT LPWSTR, IN DWORD dwBufLen);

OCOW_DEF(DWORD, ImmGetGuideLineW,
    (IN HIMC hIMC,        
    IN DWORD dwIndex, 
    OUT LPWSTR lpBuf,  
    IN DWORD dwBufLen
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetGuideLine, 
        (hIMC, dwIndex, lpBuf, dwBufLen), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef BOOL (WINAPI *fpImmGetCompositionFontA)(IN HIMC, OUT LPLOGFONTA);
typedef BOOL (WINAPI *fpImmGetCompositionFontW)(IN HIMC, OUT LPLOGFONTW);

OCOW_DEF(BOOL, ImmGetCompositionFontW,
    (IN HIMC hIMC,        
    OUT LPLOGFONTW lplf  
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetCompositionFont, 
        (hIMC, lplf), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef BOOL (WINAPI *fpImmSetCompositionFontA)(IN HIMC, IN LPLOGFONTA);
typedef BOOL (WINAPI *fpImmSetCompositionFontW)(IN HIMC, IN LPLOGFONTW);

OCOW_DEF(BOOL, ImmSetCompositionFontW,
    (IN HIMC hIMC,        
    IN LPLOGFONTW lplf  
    ))
{
    IMPLEMENT_IMM_FUNCTION(SetCompositionFont, 
        (hIMC, lplf), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef BOOL (WINAPI *fpImmConfigureIMEA)(IN HKL, IN HWND, IN DWORD, IN LPVOID);
typedef BOOL (WINAPI *fpImmConfigureIMEW)(IN HKL, IN HWND, IN DWORD, IN LPVOID);

OCOW_DEF(BOOL, ImmConfigureIMEW,
    (IN HKL hKL,       
    IN HWND hWnd,     
    IN DWORD dwMode,  
    IN LPVOID lpData  
    ))
{
    IMPLEMENT_IMM_FUNCTION(ConfigureIME, 
        (hKL, hWnd, dwMode, lpData), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef LRESULT (WINAPI *fpImmEscapeA)(IN HKL, IN HIMC, IN UINT, IN LPVOID);
typedef LRESULT (WINAPI *fpImmEscapeW)(IN HKL, IN HIMC, IN UINT, IN LPVOID);

OCOW_DEF(LRESULT, ImmEscapeW,
    (IN HKL hKL,       
    IN HIMC hIMC,     
    IN UINT uEscape,  
    IN LPVOID lpData  
    ))
{
    IMPLEMENT_IMM_FUNCTION(Escape, 
        (hKL, hIMC, uEscape, lpData), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef DWORD (WINAPI *fpImmGetConversionListA)(IN HKL, IN HIMC, IN LPCSTR, OUT LPCANDIDATELIST, IN DWORD dwBufLen, IN UINT uFlag);
typedef DWORD (WINAPI *fpImmGetConversionListW)(IN HKL, IN HIMC, IN LPCWSTR, OUT LPCANDIDATELIST, IN DWORD dwBufLen, IN UINT uFlag);

OCOW_DEF(DWORD, ImmGetConversionListW,
    (IN HKL hKL,                 
    IN HIMC hIMC,               
    IN LPCWSTR lpSrc,           
    OUT LPCANDIDATELIST lpDst,   
    IN DWORD dwBufLen,          
    IN UINT uFlag               
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetConversionList, 
        (hKL, hIMC, lpSrc, lpDst, dwBufLen, uFlag), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef BOOL (WINAPI *fpImmIsUIMessageA)(IN HWND, IN UINT, IN WPARAM, IN LPARAM);
typedef BOOL (WINAPI *fpImmIsUIMessageW)(IN HWND, IN UINT, IN WPARAM, IN LPARAM);

OCOW_DEF(BOOL, ImmIsUIMessageW,
    (IN HWND hWndIME,   
    IN UINT msg,       
    IN WPARAM wParam,  
    IN LPARAM lParam   
    ))
{
    IMPLEMENT_IMM_FUNCTION(IsUIMessage, 
        (hWndIME, msg, wParam, lParam), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef BOOL (WINAPI *fpImmRegisterWordA)(IN HKL, IN LPCSTR lpszReading, IN DWORD, IN LPCSTR lpszRegister);
typedef BOOL (WINAPI *fpImmRegisterWordW)(IN HKL, IN LPCWSTR lpszReading, IN DWORD, IN LPCWSTR lpszRegister);

OCOW_DEF(BOOL, ImmRegisterWordW,
    (IN HKL hKL,              
    IN LPCWSTR lpszReading,  
    IN DWORD dwStyle,        
    IN LPCWSTR lpszRegister  
    ))
{
    IMPLEMENT_IMM_FUNCTION(RegisterWord, 
        (hKL, lpszReading, dwStyle, lpszRegister), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef BOOL (WINAPI *fpImmUnregisterWordA)(IN HKL, IN LPCSTR lpszReading, IN DWORD, IN LPCSTR lpszUnregister);
typedef BOOL (WINAPI *fpImmUnregisterWordW)(IN HKL, IN LPCWSTR lpszReading, IN DWORD, IN LPCWSTR lpszUnregister);

OCOW_DEF(BOOL, ImmUnregisterWordW,
    (IN HKL hKL,              
    IN LPCWSTR lpszReading,  
    IN DWORD dwStyle,        
    IN LPCWSTR lpszUnregister
    ))
{
    IMPLEMENT_IMM_FUNCTION(UnregisterWord, 
        (hKL, lpszReading, dwStyle, lpszUnregister), FALSE)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

typedef UINT (WINAPI *fpImmGetRegisterWordStyleA)(IN HKL, IN UINT nItem, OUT LPSTYLEBUFA);
typedef UINT (WINAPI *fpImmGetRegisterWordStyleW)(IN HKL, IN UINT nItem, OUT LPSTYLEBUFW);

OCOW_DEF(UINT, ImmGetRegisterWordStyleW,
    (IN HKL hKL,               
    IN UINT nItem,            
    OUT LPSTYLEBUFW lpStyleBuf  
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetRegisterWordStyle, 
        (hKL, nItem, lpStyleBuf), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef UINT (WINAPI *fpImmEnumRegisterWordA)(IN HKL, IN REGISTERWORDENUMPROCA, IN LPCSTR lpszReading, IN DWORD, IN LPCSTR lpszRegister, IN LPVOID);
typedef UINT (WINAPI *fpImmEnumRegisterWordW)(IN HKL, IN REGISTERWORDENUMPROCW, IN LPCWSTR lpszReading, IN DWORD, IN LPCWSTR lpszRegister, IN LPVOID);

OCOW_DEF(UINT, ImmEnumRegisterWordW,
    (IN HKL hKL,                             
    IN REGISTERWORDENUMPROCW lpfnEnumProc,   
    IN LPCWSTR lpszReading,                 
    IN DWORD dwStyle,                       
    IN LPCWSTR lpszRegister,                
    IN LPVOID lpData                        
    ))
{
    IMPLEMENT_IMM_FUNCTION(EnumRegisterWord, 
        (hKL, lpfnEnumProc, lpszReading, dwStyle, lpszRegister, lpData), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

typedef DWORD (WINAPI *fpImmGetImeMenuItemsA)(IN HIMC, IN DWORD, IN DWORD, OUT LPIMEMENUITEMINFOA, OUT LPIMEMENUITEMINFOA, IN DWORD);
typedef DWORD (WINAPI *fpImmGetImeMenuItemsW)(IN HIMC, IN DWORD, IN DWORD, OUT LPIMEMENUITEMINFOW, OUT LPIMEMENUITEMINFOW, IN DWORD);

OCOW_DEF(DWORD, ImmGetImeMenuItemsW,
    (IN HIMC hIMC,
    IN DWORD dwFlags,
    IN DWORD dwType,
    OUT LPIMEMENUITEMINFOW lpImeParentMenu,
    OUT LPIMEMENUITEMINFOW lpImeMenu,
    IN DWORD dwSize
    ))
{
    IMPLEMENT_IMM_FUNCTION(GetImeMenuItems, 
        (hIMC, dwFlags, dwType, lpImeParentMenu, lpImeMenu, dwSize), 0)

    // --- Windows 95 only ---

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

// NOT YET IMPLEMENTED
// ----------------------------------------------------------------------------
}//EXTERN_C 