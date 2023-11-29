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
#define _KERNEL32_

#include <windows.h>
#include <mbstring.h>

#include "MbcsBuffer.h"

// ----------------------------------------------------------------------------
// Definitions

#ifndef INVALID_FILE_ATTRIBUTES
# define INVALID_FILE_ATTRIBUTES    ((DWORD)-1)
#endif
#ifndef IS_INTRESOURCE
# define IS_INTRESOURCE(p)          (((unsigned long,(p) >> 16) == 0UL)
#endif

// ----------------------------------------------------------------------------
// Globals

extern HINSTANCE g_hInstanceDLL;
extern int       g_nDebug;

static void
CopyFindDataAtoW(
    LPWIN32_FIND_DATAA lpFindDataA,
    LPWIN32_FIND_DATAW lpFindDataW
    )
{
    lpFindDataW->dwFileAttributes = lpFindDataA->dwFileAttributes;
    lpFindDataW->ftCreationTime   = lpFindDataA->ftCreationTime;
    lpFindDataW->ftLastAccessTime = lpFindDataA->ftLastAccessTime;
    lpFindDataW->ftLastWriteTime  = lpFindDataA->ftLastWriteTime;
    lpFindDataW->nFileSizeHigh    = lpFindDataA->nFileSizeHigh;
    lpFindDataW->nFileSizeLow     = lpFindDataA->nFileSizeLow;
    lpFindDataW->dwReserved0      = lpFindDataA->dwReserved0;
    lpFindDataW->dwReserved1      = lpFindDataA->dwReserved1;
    
    ::MultiByteToWideChar(CP_ACP, 0, lpFindDataA->cFileName, -1, 
        lpFindDataW->cFileName, MAX_PATH);

    ::MultiByteToWideChar(CP_ACP, 0, lpFindDataA->cAlternateFileName, -1, 
        lpFindDataW->cAlternateFileName, 
        ARRAY_SIZE(lpFindDataW->cAlternateFileName));
}

typedef struct _MsluExport {
    const char * dllName;
    const char * exportName;
} MsluExport;
static int __cdecl 
OCOW_Compare(
    const void * first, 
    const void * second
    )
{
    const MsluExport * term1 = reinterpret_cast<const MsluExport*>(first);
    const MsluExport * term2 = reinterpret_cast<const MsluExport*>(second);
    int nResult = ::strcmp(term1->dllName, term2->dllName);
    if (nResult == 0)
        nResult = ::strcmp(term1->exportName, term2->exportName);
    return nResult;
}


// ----------------------------------------------------------------------------
// API
EXTERN_C {

OCOW_DEF(ATOM, AddAtomW,
    (IN LPCWSTR lpString
    ))
{
    CMbcsBuffer mbcsString;
    if (!mbcsString.FromUnicode(lpString))
        return 0;

    return ::AddAtomA(mbcsString);
}

// BeginUpdateResourceA
// BeginUpdateResourceW
// BuildCommDCBAndTimeoutsW
// BuildCommDCBW
// CallNamedPipeW
// CommConfigDialogW
// CompareStringW
// CopyFileExW

OCOW_DEF(BOOL, CopyFileW,(
    IN LPCWSTR lpExistingFileName,
    IN LPCWSTR lpNewFileName,
    IN BOOL bFailIfExists
    ))
{
    CMbcsBuffer mbcsExistingFileName;
    if (!mbcsExistingFileName.FromUnicode(lpExistingFileName))
        return FALSE;

    CMbcsBuffer mbcsNewFileName;
    if (!mbcsNewFileName.FromUnicode(lpNewFileName))
        return FALSE;

    return ::CopyFileA(mbcsExistingFileName, mbcsNewFileName, bFailIfExists);
}

OCOW_DEF(BOOL, CreateDirectoryExW,(
    IN LPCWSTR lpTemplateDirectory,
    IN LPCWSTR lpNewDirectory,
    IN LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ))
{
    CMbcsBuffer mbcsTemplateDirectory;
    if (!mbcsTemplateDirectory.FromUnicode(lpTemplateDirectory))
        return FALSE;

    CMbcsBuffer mbcsNewDirectory;
    if (!mbcsNewDirectory.FromUnicode(lpNewDirectory))
        return FALSE;

    return ::CreateDirectoryExA(mbcsTemplateDirectory, mbcsNewDirectory, lpSecurityAttributes);
}

OCOW_DEF(BOOL, CreateDirectoryW,(
    IN LPCWSTR lpPathName,
    IN LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ))
{
    CMbcsBuffer mbcsPathName;
    if (!mbcsPathName.FromUnicode(lpPathName))
        return FALSE;

    return ::CreateDirectoryA(mbcsPathName, lpSecurityAttributes);
}

OCOW_DEF(HANDLE, CreateEventW,(
    IN LPSECURITY_ATTRIBUTES lpEventAttributes,
    IN BOOL bManualReset,
    IN BOOL bInitialState,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::CreateEventA(lpEventAttributes, bManualReset, bInitialState, mbcsName);
}

OCOW_DEF(HANDLE, CreateFileMappingW,(
    IN HANDLE hFile,
    IN LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    IN DWORD flProtect,
    IN DWORD dwMaximumSizeHigh,
    IN DWORD dwMaximumSizeLow,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::CreateFileMappingA(hFile, lpFileMappingAttributes, flProtect, 
        dwMaximumSizeHigh, dwMaximumSizeLow, mbcsName);
}

OCOW_DEF(HANDLE, CreateFileW,(
    IN LPCWSTR lpFileName,
    IN DWORD dwDesiredAccess,
    IN DWORD dwShareMode,
    IN LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    IN DWORD dwCreationDisposition,
    IN DWORD dwFlagsAndAttributes,
    IN HANDLE hTemplateFile
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return INVALID_HANDLE_VALUE;

    return ::CreateFileA(mbcsFileName, dwDesiredAccess, dwShareMode, 
        lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
        hTemplateFile);
}

OCOW_DEF(HANDLE, CreateMailslotW,(
    IN LPCWSTR lpName,
    IN DWORD nMaxMessageSize,
    IN DWORD lReadTimeout,
    IN LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return INVALID_HANDLE_VALUE;

    return ::CreateMailslotA(mbcsName, nMaxMessageSize, lReadTimeout, lpSecurityAttributes);
}

OCOW_DEF(HANDLE, CreateMutexW,(
    IN LPSECURITY_ATTRIBUTES lpMutexAttributes,
    IN BOOL bInitialOwner,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::CreateMutexA(lpMutexAttributes, bInitialOwner, mbcsName);
}

// CreateNamedPipeW

OCOW_DEF(BOOL, CreateProcessW,(
    IN LPCWSTR lpApplicationName,
    IN LPWSTR lpCommandLine,
    IN LPSECURITY_ATTRIBUTES lpProcessAttributes,
    IN LPSECURITY_ATTRIBUTES lpThreadAttributes,
    IN BOOL bInheritHandles,
    IN DWORD dwCreationFlags,
    IN LPVOID lpEnvironment,
    IN LPCWSTR lpCurrentDirectory,
    IN LPSTARTUPINFOW lpStartupInfo,
    OUT LPPROCESS_INFORMATION lpProcessInformation
    ))
{
    CMbcsBuffer mbcsApplicationName;
    if (!mbcsApplicationName.FromUnicode(lpApplicationName))
        return FALSE;

    CMbcsBuffer mbcsCommandLine;
    if (!mbcsCommandLine.FromUnicode(lpCommandLine))
        return FALSE;

    CMbcsBuffer mbcsCurrentDirectory;
    if (!mbcsCurrentDirectory.FromUnicode(lpCurrentDirectory))
        return FALSE;

    STARTUPINFOA startupInfoA;
    ::ZeroMemory(&startupInfoA, sizeof(startupInfoA));
    startupInfoA.cb              = sizeof(startupInfoA);
    startupInfoA.dwX             = lpStartupInfo->dwX;
    startupInfoA.dwY             = lpStartupInfo->dwY;
    startupInfoA.dwXSize         = lpStartupInfo->dwXSize;
    startupInfoA.dwYSize         = lpStartupInfo->dwYSize;
    startupInfoA.dwXCountChars   = lpStartupInfo->dwXCountChars;
    startupInfoA.dwYCountChars   = lpStartupInfo->dwYCountChars;
    startupInfoA.dwFillAttribute = lpStartupInfo->dwFillAttribute;
    startupInfoA.dwFlags         = lpStartupInfo->dwFlags;
    startupInfoA.wShowWindow     = lpStartupInfo->wShowWindow;
    startupInfoA.hStdInput       = lpStartupInfo->hStdInput;
    startupInfoA.hStdOutput      = lpStartupInfo->hStdOutput;
    startupInfoA.hStdError       = lpStartupInfo->hStdError;

    CMbcsBuffer mbcsDesktop;
    if (!mbcsDesktop.FromUnicode(lpStartupInfo->lpDesktop))
        return FALSE;
    startupInfoA.lpDesktop = mbcsDesktop;

    CMbcsBuffer mbcsTitle;
    if (!mbcsTitle.FromUnicode(lpStartupInfo->lpTitle))
        return FALSE;
    startupInfoA.lpTitle = mbcsTitle;

    return ::CreateProcessA(mbcsApplicationName, mbcsCommandLine, 
        lpProcessAttributes, lpThreadAttributes, bInheritHandles, 
        dwCreationFlags, lpEnvironment, mbcsCurrentDirectory,
        &startupInfoA, lpProcessInformation);
}

OCOW_DEF(HANDLE, CreateSemaphoreW,(
    IN LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
    IN LONG lInitialCount,
    IN LONG lMaximumCount,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::CreateSemaphoreA(lpSemaphoreAttributes, lInitialCount, lMaximumCount, mbcsName);
}

typedef HANDLE (WINAPI *fpCreateWaitableTimerA)(
    IN LPSECURITY_ATTRIBUTES lpTimerAttributes,
    IN BOOL bManualReset,
    IN LPCSTR lpTimerName
    );

static fpCreateWaitableTimerA pCreateWaitableTimerA; //move out to emit cxa_guard_* for libstdc++ dependency
OCOW_DEF(HANDLE, CreateWaitableTimerW,(
    IN LPSECURITY_ATTRIBUTES lpTimerAttributes,
    IN BOOL bManualReset,
    IN LPCWSTR lpTimerName
    ))
{
    if(!pCreateWaitableTimerA)
        pCreateWaitableTimerA = (fpCreateWaitableTimerA) ::GetProcAddress(
            ::GetModuleHandleA("kernel32.dll"), "CreateWaitableTimerA");
    if (!pCreateWaitableTimerA) {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return NULL;
    }

    CMbcsBuffer mbcsTimerName;
    if (!mbcsTimerName.FromUnicode(lpTimerName))
        return NULL;

    return pCreateWaitableTimerA(lpTimerAttributes, bManualReset, mbcsTimerName);
}

OCOW_DEF(BOOL, DeleteFileW,(
    IN LPCWSTR lpFileName
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return FALSE;

    return ::DeleteFileA(mbcsFileName);
}

// EndUpdateResourceA
// EndUpdateResourceW
// EnumCalendarInfoExW
// EnumCalendarInfoW
// EnumDateFormatsExW
// EnumDateFormatsW
// EnumSystemCodePagesW
// EnumSystemLocalesW
// EnumTimeFormatsW


OCOW_DEF(DWORD, ExpandEnvironmentStringsW,(
    IN LPCWSTR lpSrc,
    OUT LPWSTR lpDst,
    IN DWORD nSize
    ))
{
    CMbcsBuffer mbcsSrc;
    if (!mbcsSrc.FromUnicode(lpSrc))
        return FALSE;

    DWORD dwResult = 0;
    CMbcsBuffer mbcsDst;
    do {
        if (dwResult > (DWORD) mbcsDst.BufferSize()) {
            if (!mbcsDst.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::ExpandEnvironmentStringsA(mbcsSrc, mbcsDst, (DWORD) mbcsDst.BufferSize());
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsDst.BufferSize());
    int nDstLen = ::lstrlenA(mbcsDst);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsDst, nDstLen + 1, 0, 0);
    if (nSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsDst, nDstLen + 1, lpDst, nSize);
    return nRequiredSize; // include the NULL 
}


OCOW_DEF(VOID, FatalAppExitW,(
    IN UINT uAction,
    IN LPCWSTR lpMessageText
    ))
{
    CMbcsBuffer mbcsMessageText;
    mbcsMessageText.FromUnicode(lpMessageText);
    ::FatalAppExit(uAction, mbcsMessageText);
}

// FillConsoleOutputCharacterW


OCOW_DEF(ATOM, FindAtomW,(
    IN LPCWSTR lpString
    ))
{
    CMbcsBuffer mbcsString;
    if (!mbcsString.FromUnicode(lpString))
        return 0;

    return ::FindAtomA(mbcsString);
}


OCOW_DEF(HANDLE, FindFirstChangeNotificationW,(
    IN LPCWSTR lpPathName,
    IN BOOL bWatchSubtree,
    IN DWORD dwNotifyFilter
    ))
{
    CMbcsBuffer mbcsPathName;
    if (!mbcsPathName.FromUnicode(lpPathName))
        return INVALID_HANDLE_VALUE;

    return ::FindFirstChangeNotificationA(mbcsPathName, bWatchSubtree, dwNotifyFilter);
}


OCOW_DEF(HANDLE, FindFirstFileW,(
    IN LPCWSTR lpFileName,
    OUT LPWIN32_FIND_DATAW lpFindFileData
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return INVALID_HANDLE_VALUE;
        
    WIN32_FIND_DATAA findDataA;
    HANDLE hFile = ::FindFirstFileA(mbcsFileName, &findDataA);
    if (hFile == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;
    CopyFindDataAtoW(&findDataA, lpFindFileData);
    return hFile;
}


OCOW_DEF(BOOL, FindNextFileW,(
    IN HANDLE hFindFile,
    OUT LPWIN32_FIND_DATAW lpFindFileData
    ))
{
    WIN32_FIND_DATAA findDataA;
    BOOL bSuccess = ::FindNextFileA(hFindFile, &findDataA);
    if (bSuccess)
        CopyFindDataAtoW(&findDataA, lpFindFileData);
    return bSuccess;
}


OCOW_DEF(HRSRC, FindResourceExW,(
    IN HMODULE hModule,
    IN LPCWSTR lpType,
    IN LPCWSTR lpName,
    IN WORD    wLanguage
    ))
{
    LPCSTR lpTypeA = (LPCSTR) lpType;
    CMbcsBuffer mbcsType;
    if (!IS_INTRESOURCE(lpType)) {
        if (!mbcsType.FromUnicode(lpType))
            return NULL;
        lpTypeA = mbcsType;
    }

    LPCSTR lpNameA = (LPCSTR) lpName;
    CMbcsBuffer mbcsName;
    if (!IS_INTRESOURCE(lpName)) {
        if (!mbcsName.FromUnicode(lpName))
            return NULL;
        lpNameA = mbcsName;
    }

    return ::FindResourceExA(hModule, lpTypeA, lpNameA, wLanguage);
}


OCOW_DEF(HRSRC, FindResourceW,(
    IN HMODULE hModule,
    IN LPCWSTR lpName,
    IN LPCWSTR lpType
    ))
{
    LPCSTR lpTypeA = (LPCSTR) lpType;
    CMbcsBuffer mbcsType;
    if (!IS_INTRESOURCE(lpType)) {
        if (!mbcsType.FromUnicode(lpType))
            return NULL;
        lpTypeA = mbcsType;
    }

    LPCSTR lpNameA = (LPCSTR) lpName;
    CMbcsBuffer mbcsName;
    if (!IS_INTRESOURCE(lpName)) {
        if (!mbcsName.FromUnicode(lpName))
            return NULL;
        lpNameA = mbcsName;
    }

    return ::FindResourceA(hModule, lpTypeA, lpNameA);
}

// FormatMessageW


OCOW_DEF(BOOL, FreeEnvironmentStringsW,(
    IN LPWSTR lpStrings
    ))
{
    if (lpStrings)
        ::free(lpStrings);
    return TRUE;
}


OCOW_DEF(UINT, GetAtomNameW,(
    IN ATOM nAtom,
    OUT LPWSTR lpBuffer,
    IN int nSize
    ))
{
    CMbcsBuffer mbcsBuffer;
    UINT uiLen = ::GetAtomNameA(nAtom, mbcsBuffer, mbcsBuffer.BufferSize());
    if (!uiLen)
        return 0;

    int nLen = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, uiLen + 1, lpBuffer, nSize);
    if (!nLen) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    return (UINT) (nLen - 1);
}

//TODO: MSLU adds support for CP_UTF7 and CP_UTF8
BOOL WINAPI
OCOW_GetCPInfo(
    IN UINT       CodePage,
    OUT LPCPINFO  lpCPInfo
    )
{
    return ::GetCPInfo(CodePage, lpCPInfo);
}

//TODO: MSLU adds support for CP_UTF7 and CP_UTF8

OCOW_DEF(BOOL, GetCPInfoExW,(
    IN UINT          CodePage,
    IN DWORD         dwFlags,
    OUT LPCPINFOEXW  lpCPInfoEx
    ))
{
    CPINFOEXA cpInfoA;
    BOOL bSuccess = ::GetCPInfoExA(CodePage, dwFlags, &cpInfoA);
    if (!bSuccess)
        return FALSE;

    lpCPInfoEx->CodePage = cpInfoA.CodePage;
    ::CopyMemory(lpCPInfoEx->DefaultChar, cpInfoA.DefaultChar, MAX_DEFAULTCHAR);
    ::CopyMemory(lpCPInfoEx->LeadByte, cpInfoA.LeadByte, MAX_LEADBYTES);
    lpCPInfoEx->MaxCharSize = cpInfoA.MaxCharSize;
    lpCPInfoEx->UnicodeDefaultChar = cpInfoA.UnicodeDefaultChar;
    ::MultiByteToWideChar(CP_ACP, 0, cpInfoA.CodePageName, -1, lpCPInfoEx->CodePageName, MAX_PATH);

    return TRUE;
}

// GetCalendarInfoW


OCOW_DEF(BOOL, GetComputerNameW,(
    OUT LPWSTR lpBuffer,
    IN OUT LPDWORD lpnSize
    ))
{
    CMbcsBuffer mbcsBuffer;
    DWORD dwLen = mbcsBuffer.BufferSize();
    if (!::GetComputerNameA(mbcsBuffer, &dwLen))
        return FALSE;

    int nRequiredLen = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, (int) dwLen + 1, 0, 0);
    if (*lpnSize < (DWORD) nRequiredLen) {
        *lpnSize = (DWORD) (nRequiredLen - 1);
        SetLastError(ERROR_MORE_DATA);
        return FALSE;
    }

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, (int) dwLen + 1, lpBuffer, *lpnSize);
    *lpnSize = (DWORD) (nRequiredLen - 1);
    return TRUE;
}

// GetConsoleTitleW
// GetCurrencyFormatW


OCOW_DEF(DWORD, GetCurrentDirectoryW,(
    IN DWORD uSize,
    OUT LPWSTR lpBuffer
    ))
{
    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::GetCurrentDirectoryA((DWORD) mbcsBuffer.BufferSize(), mbcsBuffer);
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (uSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, uSize);
    return nRequiredSize - 1; // do not include the NULL 
}

// GetDateFormatW
// GetDefaultCommConfigW

typedef BOOL (WINAPI *fpGetDiskFreeSpaceExA)(
    IN LPCSTR lpDirectoryName,
    OUT PULARGE_INTEGER lpFreeBytesAvailableToCaller,
    OUT PULARGE_INTEGER lpTotalNumberOfBytes,
    OUT PULARGE_INTEGER lpTotalNumberOfFreeBytes
    );


static fpGetDiskFreeSpaceExA pGetDiskFreeSpaceExA; //move out to emit cxa_guard_* for libstdc++ dependency
OCOW_DEF(BOOL, GetDiskFreeSpaceExW,(
    IN LPCWSTR lpDirectoryName,
    OUT PULARGE_INTEGER lpFreeBytesAvailableToCaller,
    OUT PULARGE_INTEGER lpTotalNumberOfBytes,
    OUT PULARGE_INTEGER lpTotalNumberOfFreeBytes
    ))
{
    // dynamically loaded 
    if (!pGetDiskFreeSpaceExA) {
        pGetDiskFreeSpaceExA = (fpGetDiskFreeSpaceExA)
            ::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "GetDiskFreeSpaceExA");
        if (!pGetDiskFreeSpaceExA) {
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            return FALSE;
        }
    }

    CMbcsBuffer mbcsDirectoryName;
    if (!mbcsDirectoryName.FromUnicode(lpDirectoryName))
        return FALSE;

    return pGetDiskFreeSpaceExA(mbcsDirectoryName, lpFreeBytesAvailableToCaller,
        lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}


OCOW_DEF(BOOL, GetDiskFreeSpaceW,(
    IN LPCWSTR lpRootPathName,
    OUT LPDWORD lpSectorsPerCluster,
    OUT LPDWORD lpBytesPerSector,
    OUT LPDWORD lpNumberOfFreeClusters,
    OUT LPDWORD lpTotalNumberOfClusters
    ))
{
    CMbcsBuffer mbcsRootPathName;
    if (!mbcsRootPathName.FromUnicode(lpRootPathName))
        return FALSE;

    return ::GetDiskFreeSpaceA(mbcsRootPathName, lpSectorsPerCluster,
        lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}


OCOW_DEF(UINT, GetDriveTypeW,(
    IN LPCWSTR lpRootPathName
    ))
{
    CMbcsBuffer mbcsRootPathName;
    if (!mbcsRootPathName.FromUnicode(lpRootPathName))
        return DRIVE_UNKNOWN;

    return ::GetDriveTypeA(mbcsRootPathName);
}


OCOW_DEF(LPWSTR, GetEnvironmentStringsW,(
    VOID
    ))
{
    LPSTR pStringsA = ::GetEnvironmentStringsA();
    if (!pStringsA)
        return 0;

    int nTotalLen = 0;
    int nLen = ::lstrlenA(pStringsA);
    while (nLen > 0) {
        nTotalLen += nLen + 1;
        nLen = ::lstrlenA(pStringsA + nTotalLen);
    }
    ++nTotalLen;

    int nRequiredLen = ::MultiByteToWideChar(CP_ACP, 0, pStringsA, nTotalLen, 0, 0);
    if (nRequiredLen < 1) {
        ::FreeEnvironmentStringsA(pStringsA);
        return 0;
    }

    LPWSTR pStringsW = (LPWSTR) ::malloc(nRequiredLen * sizeof(wchar_t));
    if (!pStringsW) {
        ::FreeEnvironmentStringsA(pStringsA);
        return 0;
    }

    ::MultiByteToWideChar(CP_ACP, 0, pStringsA, nTotalLen, pStringsW, nRequiredLen);
    ::FreeEnvironmentStringsA(pStringsA);

    return pStringsW;
}


OCOW_DEF(DWORD, GetEnvironmentVariableW,(
    IN LPCWSTR lpName,
    OUT LPWSTR lpBuffer,
    IN DWORD nSize
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return 0;

    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::GetEnvironmentVariableA(mbcsName, mbcsBuffer, (DWORD) mbcsBuffer.BufferSize());
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (nSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, nSize);
    return nRequiredSize - 1; // don't include NULL 
}

OCOW_DEF(BOOL, GetFileAttributesExW,(
    IN LPCWSTR lpFileName,
    IN GET_FILEEX_INFO_LEVELS  fInfoLevelId,
    OUT LPVOID                 lpFileInformation
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return INVALID_FILE_ATTRIBUTES;

    return ::GetFileAttributesExA(mbcsFileName, fInfoLevelId, lpFileInformation);
}

OCOW_DEF(DWORD, GetFileAttributesW,(
    IN LPCWSTR lpFileName
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return INVALID_FILE_ATTRIBUTES;

    return ::GetFileAttributesA(mbcsFileName);
}


OCOW_DEF(DWORD, GetFullPathNameW,(
    IN LPCWSTR lpFileName,
    IN DWORD uSize,
    OUT LPWSTR lpBuffer,
    OUT LPWSTR *lpFilePart
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return 0;

    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        char * pszFilePart;
        dwResult = ::GetFullPathNameA(mbcsFileName, (DWORD) mbcsBuffer.BufferSize(), mbcsBuffer, &pszFilePart);
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (uSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, uSize);

    // set the file part pointer to the beginning of the file, we know that
    // there must be a slash in the path somewhere, so this loop won't run
    // off the beginning of the string
    if (lpFilePart) {
        wchar_t * pFilePart = lpBuffer + nRequiredSize - 1;
        while (*pFilePart != L'\\')
            --pFilePart;
        *lpFilePart = pFilePart;
    }

    return nRequiredSize - 1; // do not include the NULL 
}

// GetLocaleInfoW


OCOW_DEF(DWORD, GetLogicalDriveStringsW,(
    IN DWORD nBufferLength,
    OUT LPWSTR lpBuffer
    ))
{
    CMbcsBuffer mbcsDriveStrings;
    if (!mbcsDriveStrings.SetCapacity(nBufferLength))
        return 0;

    int len = ::GetLogicalDriveStringsA(mbcsDriveStrings.BufferSize(), mbcsDriveStrings);
    if (len > 0 && len < mbcsDriveStrings.BufferSize()) {
        // include terminating null character by using len + 1
        int nResult = ::MultiByteToWideChar(CP_ACP, 0, mbcsDriveStrings, 
            len + 1, lpBuffer, nBufferLength);
        if (nResult == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                len = ::MultiByteToWideChar(CP_ACP, 0, mbcsDriveStrings, len + 1, 0, 0);
            else
                len = 0;
        }
        else {
            len = nResult - 1; // don't include the NULL
        }
    }

    return len;
}


OCOW_DEF(DWORD, GetLongPathNameW,(
    IN LPCWSTR  lpszShortPath,
    OUT LPWSTR  lpszLongPath,
    IN DWORD    cchBuffer
    ))
{
    CMbcsBuffer mbcsShortPath;
    if (!mbcsShortPath.FromUnicode(lpszShortPath))
        return 0;

    DWORD dwResult = 0;
    CMbcsBuffer mbcsLongPath;
    do {
        if (dwResult > (DWORD) mbcsLongPath.BufferSize()) {
            if (!mbcsLongPath.SetCapacity((int) dwResult))
                return 0;
        }
#if (defined(_WIN32_WINDOWS) && _WIN32_WINDOWS >= 0x0410) || (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500) //Win98+/2k+
        dwResult = ::GetLongPathNameA(mbcsShortPath, mbcsLongPath, (DWORD) mbcsLongPath.BufferSize());
#else
        dwResult = mbcsShortPath.BufferSize() + 1;
        if(dwResult <= mbcsLongPath.BufferSize())
            memcpy(mbcsLongPath, mbcsShortPath, dwResult);
#endif
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsLongPath.BufferSize());
    int nLongPathLen = ::lstrlenA(mbcsLongPath);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsLongPath, nLongPathLen + 1, 0, 0);
    if (cchBuffer < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsLongPath, nLongPathLen + 1, lpszLongPath, cchBuffer);
    return nRequiredSize - 1; // do not include the NULL 
}


OCOW_DEF(DWORD, GetModuleFileNameW,(
    IN HMODULE hModule,
    OUT LPWSTR lpFilename,
    IN DWORD nSize
    ))
{
    CMbcsBuffer mbcsFilename;
    if (!mbcsFilename.SetCapacity(nSize))
        return 0;

    //TODO: does the return value include the NULL char or not??
    if (!::GetModuleFileNameA(hModule, mbcsFilename, mbcsFilename.BufferSize()))
        return 0;

    return (DWORD) ::MultiByteToWideChar(CP_ACP, 0, mbcsFilename, -1, lpFilename, nSize);
}


OCOW_DEF(HMODULE, GetModuleHandleW,(
    IN LPCWSTR lpModuleName
    ))
{
    CMbcsBuffer mbcsModuleName;
    if (!mbcsModuleName.FromUnicode(lpModuleName))
        return NULL;

    return ::GetModuleHandleA(mbcsModuleName);
}

// GetNamedPipeHandleStateW
// GetNumberFormatW
// GetPrivateProfileIntW
// GetPrivateProfileSectionNamesW
// GetPrivateProfileSectionW
// GetPrivateProfileStringW
// GetPrivateProfileStructW
#if !defined(STATIC_OPENCOW)
FARPROC WINAPI
OCOW_GetProcAddress(
    IN HMODULE hModule,
    IN LPCSTR lpProcName
    )
{
    // we need to catch any request for a known wrapped MSLU function 
    // and redirect it to a GetProcAddress to this DLL. This list is generated
    // from the libunicows file symbols.txt using manual regexp replacement to
    // create the proper format and sorting so that bsearch works correctly.
    // The contents of the OCOW.def section for msvcrt.dll has been added manually.

    static const MsluExport rgCheckedExports[] = {
        { "advapi32.dll", "CryptAcquireContextW" },
        { "advapi32.dll", "CryptEnumProvidersW" },
        { "advapi32.dll", "CryptEnumProviderTypesW" },
        { "advapi32.dll", "CryptGetDefaultProviderW" },
        { "advapi32.dll", "CryptSetProviderExW" },
        { "advapi32.dll", "CryptSetProviderW" },
        { "advapi32.dll", "CryptSignHashW" },
        { "advapi32.dll", "CryptVerifySignatureW" },
        { "advapi32.dll", "GetCurrentHwProfileW" },
        { "advapi32.dll", "GetUserNameW" },
        { "advapi32.dll", "IsTextUnicode" },
        { "advapi32.dll", "RegConnectRegistryW" },
        { "advapi32.dll", "RegCreateKeyExW" },
        { "advapi32.dll", "RegCreateKeyW" },
        { "advapi32.dll", "RegDeleteKeyW" },
        { "advapi32.dll", "RegDeleteValueW" },
        { "advapi32.dll", "RegEnumKeyExW" },
        { "advapi32.dll", "RegEnumKeyW" },
        { "advapi32.dll", "RegEnumValueW" },
        { "advapi32.dll", "RegLoadKeyW" },
        { "advapi32.dll", "RegOpenKeyExW" },
        { "advapi32.dll", "RegOpenKeyW" },
        { "advapi32.dll", "RegQueryInfoKeyW" },
        { "advapi32.dll", "RegQueryMultipleValuesW" },
        { "advapi32.dll", "RegQueryValueExW" },
        { "advapi32.dll", "RegQueryValueW" },
        { "advapi32.dll", "RegReplaceKeyW" },
        { "advapi32.dll", "RegSaveKeyW" },
        { "advapi32.dll", "RegSetValueExW" },
        { "advapi32.dll", "RegSetValueW" },
        { "advapi32.dll", "RegUnLoadKeyW" },
        { "avicap32.dll", "capCreateCaptureWindowW" },
        { "avicap32.dll", "capGetDriverDescriptionW" },
        { "comdlg32.dll", "ChooseColorW" },
        { "comdlg32.dll", "ChooseFontW" },
        { "comdlg32.dll", "FindTextW" },
        { "comdlg32.dll", "GetFileTitleW" },
        { "comdlg32.dll", "GetOpenFileNameW" },
        { "comdlg32.dll", "GetSaveFileNameW" },
        { "comdlg32.dll", "PageSetupDlgW" },
        { "comdlg32.dll", "PrintDlgW" },
        { "comdlg32.dll", "ReplaceTextW" },
        { "gdi32.dll", "AddFontResourceW" },
        { "gdi32.dll", "CopyEnhMetaFileW" },
        { "gdi32.dll", "CopyMetaFileW" },
        { "gdi32.dll", "CreateColorSpaceW" },
        { "gdi32.dll", "CreateDCW" },
        { "gdi32.dll", "CreateEnhMetaFileW" },
        { "gdi32.dll", "CreateFontIndirectW" },
        { "gdi32.dll", "CreateFontW" },
        { "gdi32.dll", "CreateICW" },
        { "gdi32.dll", "CreateMetaFileW" },
        { "gdi32.dll", "CreateScalableFontResourceW" },
        { "gdi32.dll", "EnumFontFamiliesExW" },
        { "gdi32.dll", "EnumFontFamiliesW" },
        { "gdi32.dll", "EnumFontsW" },
        { "gdi32.dll", "EnumICMProfilesW" },
        { "gdi32.dll", "ExtTextOutW" },
        { "gdi32.dll", "GetCharABCWidthsFloatW" },
        { "gdi32.dll", "GetCharABCWidthsW" },
        { "gdi32.dll", "GetCharacterPlacementW" },
        { "gdi32.dll", "GetCharWidth32W" },
        { "gdi32.dll", "GetCharWidthFloatW" },
        { "gdi32.dll", "GetCharWidthW" },
        { "gdi32.dll", "GetEnhMetaFileDescriptionW" },
        { "gdi32.dll", "GetEnhMetaFileW" },
        { "gdi32.dll", "GetGlyphOutlineW" },
        { "gdi32.dll", "GetICMProfileW" },
        { "gdi32.dll", "GetKerningPairsW" },
        { "gdi32.dll", "GetLogColorSpaceW" },
        { "gdi32.dll", "GetMetaFileW" },
        { "gdi32.dll", "GetObjectW" },
        { "gdi32.dll", "GetOutlineTextMetricsW" },
        { "gdi32.dll", "GetTextExtentExPointW" },
        { "gdi32.dll", "GetTextExtentPoint32W" },
        { "gdi32.dll", "GetTextExtentPointW" },
        { "gdi32.dll", "GetTextFaceW" },
        { "gdi32.dll", "GetTextMetricsW" },
        { "gdi32.dll", "PolyTextOutW" },
        { "gdi32.dll", "RemoveFontResourceW" },
        { "gdi32.dll", "ResetDCW" },
        { "gdi32.dll", "SetICMProfileW" },
        { "gdi32.dll", "StartDocW" },
        { "gdi32.dll", "TextOutW" },
        { "gdi32.dll", "UpdateICMRegKeyW" },
        { "kernel32.dll", "AddAtomW" },
        { "kernel32.dll", "BeginUpdateResourceA" },
        { "kernel32.dll", "BeginUpdateResourceW" },
        { "kernel32.dll", "BuildCommDCBAndTimeoutsW" },
        { "kernel32.dll", "BuildCommDCBW" },
        { "kernel32.dll", "CallNamedPipeW" },
        { "kernel32.dll", "CommConfigDialogW" },
        { "kernel32.dll", "CompareStringW" },
        { "kernel32.dll", "CopyFileExW" },
        { "kernel32.dll", "CopyFileW" },
        { "kernel32.dll", "CreateDirectoryExW" },
        { "kernel32.dll", "CreateDirectoryW" },
        { "kernel32.dll", "CreateEventW" },
        { "kernel32.dll", "CreateFileMappingW" },
        { "kernel32.dll", "CreateFileW" },
        { "kernel32.dll", "CreateMailslotW" },
        { "kernel32.dll", "CreateMutexW" },
        { "kernel32.dll", "CreateNamedPipeW" },
        { "kernel32.dll", "CreateProcessW" },
        { "kernel32.dll", "CreateSemaphoreW" },
        { "kernel32.dll", "CreateWaitableTimerW" },
        { "kernel32.dll", "DeleteFileW" },
        { "kernel32.dll", "EndUpdateResourceA" },
        { "kernel32.dll", "EndUpdateResourceW" },
        { "kernel32.dll", "EnumCalendarInfoExW" },
        { "kernel32.dll", "EnumCalendarInfoW" },
        { "kernel32.dll", "EnumDateFormatsExW" },
        { "kernel32.dll", "EnumDateFormatsW" },
        { "kernel32.dll", "EnumSystemCodePagesW" },
        { "kernel32.dll", "EnumSystemLocalesW" },
        { "kernel32.dll", "EnumTimeFormatsW" },
        { "kernel32.dll", "ExpandEnvironmentStringsW" },
        { "kernel32.dll", "FatalAppExitW" },
        { "kernel32.dll", "FillConsoleOutputCharacterW" },
        { "kernel32.dll", "FindAtomW" },
        { "kernel32.dll", "FindFirstChangeNotificationW" },
        { "kernel32.dll", "FindFirstFileW" },
        { "kernel32.dll", "FindNextFileW" },
        { "kernel32.dll", "FindResourceExW" },
        { "kernel32.dll", "FindResourceW" },
        { "kernel32.dll", "FormatMessageW" },
        { "kernel32.dll", "FreeEnvironmentStringsW" },
        { "kernel32.dll", "GetAtomNameW" },
        { "kernel32.dll", "GetCalendarInfoW" },
        { "kernel32.dll", "GetComputerNameW" },
        { "kernel32.dll", "GetConsoleTitleW" },
        { "kernel32.dll", "GetCPInfo" },
        { "kernel32.dll", "GetCPInfoExW" },
        { "kernel32.dll", "GetCurrencyFormatW" },
        { "kernel32.dll", "GetCurrentDirectoryW" },
        { "kernel32.dll", "GetDateFormatW" },
        { "kernel32.dll", "GetDefaultCommConfigW" },
        { "kernel32.dll", "GetDiskFreeSpaceExW" },
        { "kernel32.dll", "GetDiskFreeSpaceW" },
        { "kernel32.dll", "GetDriveTypeW" },
        { "kernel32.dll", "GetEnvironmentStringsW" },
        { "kernel32.dll", "GetEnvironmentVariableW" },
        { "kernel32.dll", "GetFileAttributesExW" },
        { "kernel32.dll", "GetFileAttributesW" },
        { "kernel32.dll", "GetFullPathNameW" },
        { "kernel32.dll", "GetLocaleInfoW" },
        { "kernel32.dll", "GetLogicalDriveStringsW" },
        { "kernel32.dll", "GetLongPathNameW" },
        { "kernel32.dll", "GetModuleFileNameW" },
        { "kernel32.dll", "GetModuleHandleW" },
        { "kernel32.dll", "GetNamedPipeHandleStateW" },
        { "kernel32.dll", "GetNumberFormatW" },
        { "kernel32.dll", "GetPrivateProfileIntW" },
        { "kernel32.dll", "GetPrivateProfileSectionNamesW" },
        { "kernel32.dll", "GetPrivateProfileSectionW" },
        { "kernel32.dll", "GetPrivateProfileStringW" },
        { "kernel32.dll", "GetPrivateProfileStructW" },
        { "kernel32.dll", "GetProcAddress" },
        { "kernel32.dll", "GetProfileIntW" },
        { "kernel32.dll", "GetProfileSectionW" },
        { "kernel32.dll", "GetProfileStringW" },
        { "kernel32.dll", "GetShortPathNameW" },
        { "kernel32.dll", "GetStartupInfoW" },
        { "kernel32.dll", "GetStringTypeExW" },
        { "kernel32.dll", "GetStringTypeW" },
        { "kernel32.dll", "GetSystemDirectoryW" },
        { "kernel32.dll", "GetSystemWindowsDirectoryW" },
        { "kernel32.dll", "GetTempFileNameW" },
        { "kernel32.dll", "GetTempPathW" },
        { "kernel32.dll", "GetTimeFormatW" },
        { "kernel32.dll", "GetVersionExW" },
        { "kernel32.dll", "GetVolumeInformationW" },
        { "kernel32.dll", "GetWindowsDirectoryW" },
        { "kernel32.dll", "GlobalAddAtomW" },
        { "kernel32.dll", "GlobalFindAtomW" },
        { "kernel32.dll", "GlobalGetAtomNameW" },
        { "kernel32.dll", "IsBadStringPtrW" },
        { "kernel32.dll", "IsValidCodePage" },
        { "kernel32.dll", "LCMapStringW" },
        { "kernel32.dll", "LoadLibraryExW" },
        { "kernel32.dll", "LoadLibraryW" },
        { "kernel32.dll", "lstrcatW" },
        { "kernel32.dll", "lstrcmpiW" },
        { "kernel32.dll", "lstrcmpW" },
        { "kernel32.dll", "lstrcpynW" },
        { "kernel32.dll", "lstrcpyW" },
        { "kernel32.dll", "lstrlenW" },
        { "kernel32.dll", "MoveFileW" },
        { "kernel32.dll", "MultiByteToWideChar" },
        { "kernel32.dll", "OpenEventW" },
        { "kernel32.dll", "OpenFileMappingW" },
        { "kernel32.dll", "OpenMutexW" },
        { "kernel32.dll", "OpenSemaphoreW" },
        { "kernel32.dll", "OpenWaitableTimerW" },
        { "kernel32.dll", "OutputDebugStringW" },
        { "kernel32.dll", "PeekConsoleInputW" },
        { "kernel32.dll", "QueryDosDeviceW" },
        { "kernel32.dll", "ReadConsoleInputW" },
        { "kernel32.dll", "ReadConsoleOutputCharacterW" },
        { "kernel32.dll", "ReadConsoleOutputW" },
        { "kernel32.dll", "ReadConsoleW" },
        { "kernel32.dll", "RemoveDirectoryW" },
        { "kernel32.dll", "ScrollConsoleScreenBufferW" },
        { "kernel32.dll", "SearchPathW" },
        { "kernel32.dll", "SetCalendarInfoW" },
        { "kernel32.dll", "SetComputerNameW" },
        { "kernel32.dll", "SetConsoleTitleW" },
        { "kernel32.dll", "SetCurrentDirectoryW" },
        { "kernel32.dll", "SetDefaultCommConfigW" },
        { "kernel32.dll", "SetEnvironmentVariableW" },
        { "kernel32.dll", "SetFileAttributesW" },
        { "kernel32.dll", "SetLocaleInfoW" },
        { "kernel32.dll", "SetVolumeLabelW" },
        { "kernel32.dll", "UpdateResourceA" },
        { "kernel32.dll", "UpdateResourceW" },
        { "kernel32.dll", "WaitNamedPipeW" },
        { "kernel32.dll", "WideCharToMultiByte" },
        { "kernel32.dll", "WriteConsoleInputW" },
        { "kernel32.dll", "WriteConsoleOutputCharacterW" },
        { "kernel32.dll", "WriteConsoleOutputW" },
        { "kernel32.dll", "WriteConsoleW" },
        { "kernel32.dll", "WritePrivateProfileSectionW" },
        { "kernel32.dll", "WritePrivateProfileStringW" },
        { "kernel32.dll", "WritePrivateProfileStructW" },
        { "kernel32.dll", "WriteProfileSectionW" },
        { "kernel32.dll", "WriteProfileStringW" },
        { "mpr.dll", "MultinetGetConnectionPerformanceW" },
        { "mpr.dll", "WNetAddConnection2W" },
        { "mpr.dll", "WNetAddConnection3W" },
        { "mpr.dll", "WNetAddConnectionW" },
        { "mpr.dll", "WNetCancelConnection2W" },
        { "mpr.dll", "WNetCancelConnectionW" },
        { "mpr.dll", "WNetConnectionDialog1W" },
        { "mpr.dll", "WNetDisconnectDialog1W" },
        { "mpr.dll", "WNetEnumResourceW" },
        { "mpr.dll", "WNetGetConnectionW" },
        { "mpr.dll", "WNetGetLastErrorW" },
        { "mpr.dll", "WNetGetNetworkInformationW" },
        { "mpr.dll", "WNetGetProviderNameW" },
        { "mpr.dll", "WNetGetResourceInformationW" },
        { "mpr.dll", "WNetGetResourceParentW" },
        { "mpr.dll", "WNetGetUniversalNameW" },
        { "mpr.dll", "WNetGetUserW" },
        { "mpr.dll", "WNetOpenEnumW" },
        { "mpr.dll", "WNetUseConnectionW" },
        { "msvcrt.dll", "_waccess" },
        { "msvcrt.dll", "_wchdir" },
        { "msvcrt.dll", "_wgetcwd" },
        { "msvcrt.dll", "_wgetdcwd" },
        { "msvcrt.dll", "_wmkdir" },
        { "msvcrt.dll", "_wopen" },
        { "msvcrt.dll", "_wremove" },
        { "msvcrt.dll", "_wrename" },
        { "msvcrt.dll", "_wstat" },
        { "msvcrt.dll", "_wstati64" },
        { "msvfw32.dll", "GetOpenFileNamePreviewW" },
        { "msvfw32.dll", "GetSaveFileNamePreviewW" },
        { "msvfw32.dll", "MCIWndCreateW" },
        { "oleacc.dll", "CreateStdAccessibleProxyW" },
        { "oleacc.dll", "GetRoleTextW" },
        { "oleacc.dll", "GetStateTextW" },
        { "oledlg.dll", "OleUIAddVerbMenuW" },
        { "oledlg.dll", "OleUIBusyW" },
        { "oledlg.dll", "OleUIChangeIconW" },
        { "oledlg.dll", "OleUIChangeSourceW" },
        { "oledlg.dll", "OleUIConvertW" },
        { "oledlg.dll", "OleUIEditLinksW" },
        { "oledlg.dll", "OleUIInsertObjectW" },
        { "oledlg.dll", "OleUIObjectPropertiesW" },
        { "oledlg.dll", "OleUIPasteSpecialW" },
        { "oledlg.dll", "OleUIPromptUserW" },
        { "oledlg.dll", "OleUIUpdateLinksW" },
        { "rasapi32.dll", "RasConnectionNotificationW" },
        { "rasapi32.dll", "RasCreatePhonebookEntryW" },
        { "rasapi32.dll", "RasDeleteEntryW" },
        { "rasapi32.dll", "RasDeleteSubEntryW" },
        { "rasapi32.dll", "RasDialW" },
        { "rasapi32.dll", "RasEditPhonebookEntryW" },
        { "rasapi32.dll", "RasEnumConnectionsW" },
        { "rasapi32.dll", "RasEnumDevicesW" },
        { "rasapi32.dll", "RasEnumEntriesW" },
        { "rasapi32.dll", "RasGetConnectStatusW" },
        { "rasapi32.dll", "RasGetEntryDialParamsW" },
        { "rasapi32.dll", "RasGetEntryPropertiesW" },
        { "rasapi32.dll", "RasGetErrorStringW" },
        { "rasapi32.dll", "RasGetProjectionInfoW" },
        { "rasapi32.dll", "RasHangUpW" },
        { "rasapi32.dll", "RasRenameEntryW" },
        { "rasapi32.dll", "RasSetEntryDialParamsW" },
        { "rasapi32.dll", "RasSetEntryPropertiesW" },
        { "rasapi32.dll", "RasSetSubEntryPropertiesW" },
        { "rasapi32.dll", "RasValidateEntryNameW" },
        { "secur32.dll", "AcquireCredentialsHandleW" },
        { "secur32.dll", "EnumerateSecurityPackagesW" },
        { "secur32.dll", "FreeContextBuffer" },
        { "secur32.dll", "InitializeSecurityContextW" },
        { "secur32.dll", "InitSecurityInterfaceW" },
        { "secur32.dll", "QueryContextAttributesW" },
        { "secur32.dll", "QueryCredentialsAttributesW" },
        { "secur32.dll", "QuerySecurityPackageInfoW" },
        { "sensapi.dll", "IsDestinationReachableW" },
        { "shell32.dll", "DragQueryFileW" },
        { "shell32.dll", "ExtractIconExW" },
        { "shell32.dll", "ExtractIconW" },
        { "shell32.dll", "FindExecutableW" },
        { "shell32.dll", "SHBrowseForFolderW" },
        { "shell32.dll", "SHChangeNotify" },
        { "shell32.dll", "Shell_NotifyIconW" },
        { "shell32.dll", "ShellAboutW" },
        { "shell32.dll", "ShellExecuteExW" },
        { "shell32.dll", "ShellExecuteW" },
        { "shell32.dll", "SHFileOperationW" },
        { "shell32.dll", "SHGetFileInfoW" },
        { "shell32.dll", "SHGetNewLinkInfoW" },
        { "shell32.dll", "SHGetPathFromIDListW" },
        { "user32.dll", "AppendMenuW" },
        { "user32.dll", "BroadcastSystemMessageW" },
        { "user32.dll", "CallMsgFilterW" },
        { "user32.dll", "CallWindowProcA" },
        { "user32.dll", "CallWindowProcW" },
        { "user32.dll", "ChangeDisplaySettingsExW" },
        { "user32.dll", "ChangeDisplaySettingsW" },
        { "user32.dll", "ChangeMenuW" },
        { "user32.dll", "CharLowerBuffW" },
        { "user32.dll", "CharLowerW" },
        { "user32.dll", "CharNextW" },
        { "user32.dll", "CharPrevW" },
        { "user32.dll", "CharToOemBuffW" },
        { "user32.dll", "CharToOemW" },
        { "user32.dll", "CharUpperBuffW" },
        { "user32.dll", "CharUpperW" },
        { "user32.dll", "CopyAcceleratorTableW" },
        { "user32.dll", "CreateAcceleratorTableW" },
        { "user32.dll", "CreateDialogIndirectParamW" },
        { "user32.dll", "CreateDialogParamW" },
        { "user32.dll", "CreateMDIWindowW" },
        { "user32.dll", "CreateWindowExW" },
        { "user32.dll", "DdeConnect" },
        { "user32.dll", "DdeConnectList" },
        { "user32.dll", "DdeCreateStringHandleW" },
        { "user32.dll", "DdeInitializeW" },
        { "user32.dll", "DdeQueryConvInfo" },
        { "user32.dll", "DdeQueryStringW" },
        { "user32.dll", "DefDlgProcW" },
        { "user32.dll", "DefFrameProcW" },
        { "user32.dll", "DefMDIChildProcW" },
        { "user32.dll", "DefWindowProcW" },
        { "user32.dll", "DialogBoxIndirectParamW" },
        { "user32.dll", "DialogBoxParamW" },
        { "user32.dll", "DispatchMessageW" },
        { "user32.dll", "DlgDirListComboBoxW" },
        { "user32.dll", "DlgDirListW" },
        { "user32.dll", "DlgDirSelectComboBoxExW" },
        { "user32.dll", "DlgDirSelectExW" },
        { "user32.dll", "DrawStateW" },
        { "user32.dll", "DrawTextExW" },
        { "user32.dll", "DrawTextW" },
        { "user32.dll", "EnableWindow" },
        { "user32.dll", "EnumClipboardFormats" },
        { "user32.dll", "EnumDisplayDevicesW" },
        { "user32.dll", "EnumDisplaySettingsExW" },
        { "user32.dll", "EnumDisplaySettingsW" },
        { "user32.dll", "EnumPropsA" },
        { "user32.dll", "EnumPropsExA" },
        { "user32.dll", "EnumPropsExW" },
        { "user32.dll", "EnumPropsW" },
        { "user32.dll", "FindWindowExW" },
        { "user32.dll", "FindWindowW" },
        { "user32.dll", "GetAltTabInfoW" },
        { "user32.dll", "GetClassInfoExW" },
        { "user32.dll", "GetClassInfoW" },
        { "user32.dll", "GetClassLongW" },
        { "user32.dll", "GetClassNameW" },
        { "user32.dll", "GetClipboardData" },
        { "user32.dll", "GetClipboardFormatNameW" },
        { "user32.dll", "GetDlgItemTextW" },
        { "user32.dll", "GetKeyboardLayoutNameW" },
        { "user32.dll", "GetKeyNameTextW" },
        { "user32.dll", "GetMenuItemInfoW" },
        { "user32.dll", "GetMenuStringW" },
        { "user32.dll", "GetMessageW" },
        { "user32.dll", "GetMonitorInfoW" },
        { "user32.dll", "GetPropA" },
        { "user32.dll", "GetPropW" },
        { "user32.dll", "GetTabbedTextExtentW" },
        { "user32.dll", "GetWindowLongA" },
        { "user32.dll", "GetWindowLongW" },
        { "user32.dll", "GetWindowModuleFileNameW" },
        { "user32.dll", "GetWindowTextLengthW" },
        { "user32.dll", "GetWindowTextW" },
        { "user32.dll", "GrayStringW" },
        { "user32.dll", "InsertMenuItemW" },
        { "user32.dll", "InsertMenuW" },
        { "user32.dll", "IsCharAlphaNumericW" },
        { "user32.dll", "IsCharAlphaW" },
        { "user32.dll", "IsCharLowerW" },
        { "user32.dll", "IsCharUpperW" },
        { "user32.dll", "IsClipboardFormatAvailable" },
        { "user32.dll", "IsDialogMessageW" },
        { "user32.dll", "IsWindowUnicode" },
        { "user32.dll", "LoadAcceleratorsW" },
        { "user32.dll", "LoadBitmapW" },
        { "user32.dll", "LoadCursorFromFileW" },
        { "user32.dll", "LoadCursorW" },
        { "user32.dll", "LoadIconW" },
        { "user32.dll", "LoadImageW" },
        { "user32.dll", "LoadKeyboardLayoutW" },
        { "user32.dll", "LoadMenuIndirectW" },
        { "user32.dll", "LoadMenuW" },
        { "user32.dll", "LoadStringW" },
        { "user32.dll", "MapVirtualKeyExW" },
        { "user32.dll", "MapVirtualKeyW" },
        { "user32.dll", "MessageBoxExW" },
        { "user32.dll", "MessageBoxIndirectW" },
        { "user32.dll", "MessageBoxW" },
        { "user32.dll", "ModifyMenuW" },
        { "user32.dll", "OemToCharBuffW" },
        { "user32.dll", "OemToCharW" },
        { "user32.dll", "PeekMessageW" },
        { "user32.dll", "PostMessageW" },
        { "user32.dll", "PostThreadMessageW" },
        { "user32.dll", "RegisterClassExW" },
        { "user32.dll", "RegisterClassW" },
        { "user32.dll", "RegisterClipboardFormatW" },
        { "user32.dll", "RegisterDeviceNotificationW" },
        { "user32.dll", "RegisterWindowMessageW" },
        { "user32.dll", "RemovePropA" },
        { "user32.dll", "RemovePropW" },
        { "user32.dll", "SendDlgItemMessageW" },
        { "user32.dll", "SendMessageCallbackW" },
        { "user32.dll", "SendMessageTimeoutW" },
        { "user32.dll", "SendMessageW" },
        { "user32.dll", "SendNotifyMessageW" },
        { "user32.dll", "SetClassLongW" },
        { "user32.dll", "SetDlgItemTextW" },
        { "user32.dll", "SetMenuItemInfoW" },
        { "user32.dll", "SetPropA" },
        { "user32.dll", "SetPropW" },
        { "user32.dll", "SetWindowLongA" },
        { "user32.dll", "SetWindowLongW" },
        { "user32.dll", "SetWindowsHookExW" },
        { "user32.dll", "SetWindowsHookW" },
        { "user32.dll", "SetWindowTextW" },
        { "user32.dll", "SystemParametersInfoW" },
        { "user32.dll", "TabbedTextOutW" },
        { "user32.dll", "TranslateAcceleratorW" },
        { "user32.dll", "UnregisterClassW" },
        { "user32.dll", "VkKeyScanExW" },
        { "user32.dll", "VkKeyScanW" },
        { "user32.dll", "WinHelpW" },
        { "user32.dll", "wsprintfW" },
        { "user32.dll", "wvsprintfW" },
        { "version.dll", "GetFileVersionInfoSizeW" },
        { "version.dll", "GetFileVersionInfoW" },
        { "version.dll", "VerFindFileW" },
        { "version.dll", "VerInstallFileW" },
        { "version.dll", "VerLanguageNameW" },
        { "version.dll", "VerQueryValueW" },
        { "winmm.dll", "auxGetDevCapsW" },
        { "winmm.dll", "joyGetDevCapsW" },
        { "winmm.dll", "mciGetDeviceIDW" },
        { "winmm.dll", "mciGetErrorStringW" },
        { "winmm.dll", "mciSendCommandW" },
        { "winmm.dll", "mciSendStringW" },
        { "winmm.dll", "midiInGetDevCapsW" },
        { "winmm.dll", "midiInGetErrorTextW" },
        { "winmm.dll", "midiOutGetDevCapsW" },
        { "winmm.dll", "midiOutGetErrorTextW" },
        { "winmm.dll", "mixerGetControlDetailsW" },
        { "winmm.dll", "mixerGetDevCapsW" },
        { "winmm.dll", "mixerGetLineControlsW" },
        { "winmm.dll", "mixerGetLineInfoW" },
        { "winmm.dll", "mmioInstallIOProcW" },
        { "winmm.dll", "mmioOpenW" },
        { "winmm.dll", "mmioRenameW" },
        { "winmm.dll", "mmioStringToFOURCCW" },
        { "winmm.dll", "PlaySoundW" },
        { "winmm.dll", "sndPlaySoundW" },
        { "winmm.dll", "waveInGetDevCapsW" },
        { "winmm.dll", "waveInGetErrorTextW" },
        { "winmm.dll", "waveOutGetDevCapsW" },
        { "winmm.dll", "waveOutGetErrorTextW" },
        { "winspool.drv", "AddJobW" },
        { "winspool.drv", "AddMonitorW" },
        { "winspool.drv", "AddPortW" },
        { "winspool.drv", "AddPrinterDriverW" },
        { "winspool.drv", "AddPrinterW" },
        { "winspool.drv", "AddPrintProcessorW" },
        { "winspool.drv", "AddPrintProvidorW" },
        { "winspool.drv", "AdvancedDocumentPropertiesW" },
        { "winspool.drv", "ConfigurePortW" },
        { "winspool.drv", "DeleteMonitorW" },
        { "winspool.drv", "DeletePortW" },
        { "winspool.drv", "DeletePrinterDriverW" },
        { "winspool.drv", "DeletePrintProcessorW" },
        { "winspool.drv", "DeletePrintProvidorW" },
        { "winspool.drv", "DeviceCapabilitiesW" },
        { "winspool.drv", "DocumentPropertiesW" },
        { "winspool.drv", "EnumMonitorsW" },
        { "winspool.drv", "EnumPortsW" },
        { "winspool.drv", "EnumPrinterDriversW" },
        { "winspool.drv", "EnumPrintersW" },
        { "winspool.drv", "EnumPrintProcessorDatatypesW" },
        { "winspool.drv", "EnumPrintProcessorsW" },
        { "winspool.drv", "GetJobW" },
        { "winspool.drv", "GetPrinterDataW" },
        { "winspool.drv", "GetPrinterDriverDirectoryW" },
        { "winspool.drv", "GetPrinterDriverW" },
        { "winspool.drv", "GetPrinterW" },
        { "winspool.drv", "GetPrintProcessorDirectoryW" },
        { "winspool.drv", "OpenPrinterW" },
        { "winspool.drv", "ResetPrinterW" },
        { "winspool.drv", "SetJobW" },
        { "winspool.drv", "SetPrinterDataW" },
        { "winspool.drv", "SetPrinterW" },
        { "winspool.drv", "StartDocPrinterW" }
    };

    // if they are calling it on our DLL directly then let kernel32 handle it,
    // because we know it will return the correct value as necessary
    if (hModule == (HMODULE) g_hInstanceDLL)
        return ::GetProcAddress(hModule, lpProcName);

    // get the system path once
    static char szSystemPath[MAX_PATH+1] = { 0 };
    if (!*szSystemPath) {
        if (!::GetSystemDirectoryA(szSystemPath, sizeof(szSystemPath)))
            return NULL;
    }

    // determine the path for the DLL that they have requested the 
    // function for. 
    char szModulePath[MAX_PATH+1];
    if (!::GetModuleFileNameA(hModule, szModulePath, sizeof(szModulePath)))
        return NULL;
    char * pszModuleName = (char *) ::_mbsrchr((unsigned char *)szModulePath, '\\');
    if (!pszModuleName) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return NULL;
    }
    *pszModuleName++ = 0;

    // we only check to see if we load our own implementation of this function
    // if the requested DLL is in the system path.
    if (0 == ::strcmp(szModulePath, szSystemPath))
    {
        MsluExport searchTerm = { pszModuleName, lpProcName };
        MsluExport * pEntry = (MsluExport *) ::bsearch(
            &searchTerm, rgCheckedExports, 
            sizeof(rgCheckedExports)/sizeof(rgCheckedExports[0]),
            sizeof(rgCheckedExports[0]), 
            OCOW_Compare);
        if (pEntry) {
            FARPROC pProc = ::GetProcAddress(g_hInstanceDLL, lpProcName);
            if (g_nDebug >= 2) {
                char szMessage[256];
                if (pProc)
                    lstrcpyA(szMessage, "Successfully loaded opencow function '");
                else
                    lstrcpyA(szMessage, "Failed to load opencow function '");
                lstrcatA(szMessage, lpProcName);
                lstrcatA(szMessage, "' from '");
                lstrcatA(szMessage, pszModuleName);
                lstrcatA(szMessage, "'. Press cancel to disable getprocaddress notifications.");

                int nResult = ::MessageBoxA(NULL, szMessage, "opencow", 
                    MB_OKCANCEL | MB_SETFOREGROUND);
                if (nResult == IDCANCEL)
                    g_nDebug = 1;
            }
            return pProc;
        }
    }

    // if we haven't found the procedure we pass it on to the kernel
    return ::GetProcAddress(hModule, lpProcName);
}
#endif
// GetProfileIntW
// GetProfileSectionW
// GetProfileStringW


OCOW_DEF(DWORD, GetShortPathNameW,(
    IN LPCWSTR  lpszLongPath,
    OUT LPWSTR  lpszShortPath,
    IN DWORD    cchBuffer
    ))
{
    CMbcsBuffer mbcsLongPath;
    if (!mbcsLongPath.FromUnicode(lpszLongPath))
        return 0;

    DWORD dwResult = 0;
    CMbcsBuffer mbcsShortPath;
    do {
        if (dwResult > (DWORD) mbcsShortPath.BufferSize()) {
            if (!mbcsShortPath.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::GetShortPathNameA(mbcsLongPath, mbcsShortPath, (DWORD) mbcsShortPath.BufferSize());
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsShortPath.BufferSize());
    int nShortPathLen = ::lstrlenA(mbcsShortPath);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsShortPath, nShortPathLen + 1, 0, 0);
    if (cchBuffer < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsShortPath, nShortPathLen + 1, lpszShortPath, cchBuffer);
    return nRequiredSize - 1; // do not include the NULL 
}

#if 0

OCOW_DEF(VOID, GetStartupInfoW,(
    OUT LPSTARTUPINFOW lpStartupInfo
    ))
{
    // we use static buffers here because they must be valid for the caller
    // to use after we have returned. Also, we know that they will not change
    // from call to call because this is information which was generated at
    // application startup time and is never changed from then.
    static bool bIsInitialized = false;
    static wchar_t wszDesktop[MAX_PATH] = { 0 };
    static wchar_t wszTitle[MAX_PATH] = { 0 };
    static STARTUPINFOA startupInfoA = { 0 };
    if (!bIsInitialized) {
        ::GetStartupInfoA(&startupInfoA);
        ::MultiByteToWideChar(CP_ACP, 0, startupInfoA.lpDesktop, -1, wszDesktop, MAX_PATH);
        ::MultiByteToWideChar(CP_ACP, 0, startupInfoA.lpTitle, -1, wszTitle, MAX_PATH);
        bIsInitialized = true;
    }

    ::ZeroMemory(lpStartupInfo, sizeof(STARTUPINFOW));
    lpStartupInfo->cb               = sizeof(STARTUPINFOW);
    lpStartupInfo->dwX              = startupInfoA.dwX;
    lpStartupInfo->dwY              = startupInfoA.dwY;
    lpStartupInfo->dwXSize          = startupInfoA.dwXSize;
    lpStartupInfo->dwYSize          = startupInfoA.dwYSize;
    lpStartupInfo->dwXCountChars    = startupInfoA.dwXCountChars;
    lpStartupInfo->dwYCountChars    = startupInfoA.dwYCountChars;
    lpStartupInfo->dwFillAttribute  = startupInfoA.dwFillAttribute;
    lpStartupInfo->dwFlags          = startupInfoA.dwFlags;
    lpStartupInfo->wShowWindow      = startupInfoA.wShowWindow;
    lpStartupInfo->hStdInput        = startupInfoA.hStdInput;
    lpStartupInfo->hStdOutput       = startupInfoA.hStdOutput;
    lpStartupInfo->hStdError        = startupInfoA.hStdError;

    if (startupInfoA.lpDesktop)
        lpStartupInfo->lpDesktop = wszDesktop;
    if (startupInfoA.lpTitle)
        lpStartupInfo->lpTitle = wszTitle;
}
#endif
// GetStringTypeExW


OCOW_DEF(BOOL, GetStringTypeW,(
    IN DWORD    dwInfoType,
    IN LPCWSTR  lpSrcStr,
    IN int      cchSrc,
    OUT LPWORD  lpCharType
    ))
{
    CMbcsBuffer mbcsString;
    if (!mbcsString.FromUnicode(lpSrcStr, cchSrc))
        return FALSE;

    return ::GetStringTypeA(LOCALE_SYSTEM_DEFAULT, dwInfoType, 
        mbcsString, mbcsString.Length(), lpCharType);
}


OCOW_DEF(UINT, GetSystemDirectoryW,(
    OUT LPWSTR lpBuffer,
    IN UINT uSize
    ))
{
    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::GetSystemDirectoryA(mbcsBuffer, (DWORD) mbcsBuffer.BufferSize());
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (uSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, uSize);
    return nRequiredSize - 1; // do not include the NULL 
}


OCOW_DEF(UINT, GetSystemWindowsDirectoryW,(
    OUT LPWSTR lpBuffer,
    IN UINT uSize
    ))
{
    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        // The GetSystemWindowsDirectoryA function doesn't exist on Windows 95/98/ME.
        // As the result is the shared windows directory, this is the same as the 
        // standard GetWindowsDirectoryA call on these platforms.
        dwResult = ::GetWindowsDirectoryA(mbcsBuffer, (DWORD) mbcsBuffer.BufferSize());
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (uSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, uSize);
    return nRequiredSize - 1; // do not include the NULL 
}


OCOW_DEF(UINT, GetTempFileNameW,(
    IN LPCWSTR lpPathName,
    IN LPCWSTR lpPrefixString,
    IN UINT uUnique,
    OUT LPWSTR lpTempFileName
    ))
{
    CMbcsBuffer mbcsPathName;
    if (!mbcsPathName.FromUnicode(lpPathName))
        return 0;

    CMbcsBuffer mbcsPrefixString;
    if (!mbcsPrefixString.FromUnicode(lpPrefixString))
        return 0;

    char szTempFileName[MAX_PATH];
    UINT uiResult = ::GetTempFileNameA(mbcsPathName, mbcsPrefixString, uUnique, szTempFileName);
    if (uiResult == 0)
        return 0;

    ::MultiByteToWideChar(CP_ACP, 0, szTempFileName, -1, lpTempFileName, MAX_PATH);
    return uiResult;
}


OCOW_DEF(DWORD, GetTempPathW,(
    IN DWORD uSize,
    OUT LPWSTR lpBuffer
    ))
{
    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::GetTempPathA((DWORD) mbcsBuffer.BufferSize(), mbcsBuffer);
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (uSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, uSize);
    return nRequiredSize - 1; // do not include the NULL 
}

// GetTimeFormatW
// GetVersionExW
// GetVolumeInformationW

OCOW_DEF(UINT, GetWindowsDirectoryW,(
    OUT LPWSTR lpBuffer,
    IN UINT uSize
    ))
{
    DWORD dwResult = 0;
    CMbcsBuffer mbcsBuffer;
    do {
        if (dwResult > (DWORD) mbcsBuffer.BufferSize()) {
            if (!mbcsBuffer.SetCapacity((int) dwResult))
                return 0;
        }

        dwResult = ::GetWindowsDirectoryA(mbcsBuffer, (DWORD) mbcsBuffer.BufferSize());
        if (!dwResult)
            return 0;
    }
    while (dwResult > (DWORD) mbcsBuffer.BufferSize());
    int nBufferLen = ::lstrlenA(mbcsBuffer);

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, 0, 0);
    if (uSize < (DWORD) nRequiredSize)
        return (DWORD) nRequiredSize;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, nBufferLen + 1, lpBuffer, uSize);
    return nRequiredSize - 1; // do not include the NULL 
}


OCOW_DEF(ATOM, GlobalAddAtomW,(
    IN LPCWSTR lpString
    ))
{
    CMbcsBuffer mbcsString;
    if (!mbcsString.FromUnicode(lpString))
        return 0;

    return ::GlobalAddAtomA(mbcsString);
}


OCOW_DEF(ATOM, GlobalFindAtomW,(
    IN LPCWSTR lpString
    ))
{
    CMbcsBuffer mbcsString;
    if (!mbcsString.FromUnicode(lpString))
        return 0;

    return ::GlobalFindAtomA(mbcsString);
}

OCOW_DEF(UINT, GlobalGetAtomNameW,(
    IN ATOM nAtom,
    OUT LPWSTR lpBuffer,
    IN int nSize
    ))
{
    CMbcsBuffer mbcsBuffer;
    UINT uiLen = ::GlobalGetAtomNameA(nAtom, mbcsBuffer, mbcsBuffer.BufferSize());
    if (!uiLen)
        return 0;

    int nLen = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, uiLen + 1, lpBuffer, nSize);
    if (!nLen) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    return (UINT) (nLen - 1);
}


OCOW_DEF(BOOL, IsBadStringPtrW,(
    IN LPCWSTR lpsz,
    IN UINT_PTR ucchMax
    ))
{
    if (ucchMax == 0)
        return 0;

    //Note: slow because we call the IsBadReadPtr function
    // once for every character before we examine the character,
    // but I can't see another way to do this.
    do
    {
        if (::IsBadReadPtr(lpsz, sizeof(wchar_t)))
            return 1;
    }
    while (*lpsz++ && --ucchMax > 0);

    return 0;
}

//TODO: MSLU adds support for CP_UTF7 and CP_UTF8 
BOOL WINAPI
OCOW_IsValidCodePage(
    IN UINT  CodePage
    )
{
    return ::IsValidCodePage(CodePage);
}

//TODO: MSLU adds support for CP_UTF7 and CP_UTF8 

OCOW_DEF(int, LCMapStringW,(
    IN LCID     Locale,
    IN DWORD    dwMapFlags,
    IN LPCWSTR  lpSrcStr,
    IN int      cchSrc,
    OUT LPWSTR  lpDestStr,
    IN int      cchDest
    ))
{
    CMbcsBuffer mbcsString;
    if (!mbcsString.FromUnicode(lpSrcStr, cchSrc))
        return 0;

    // get the buffer size required
    int nRequired = ::LCMapStringA(Locale, dwMapFlags, 
        mbcsString, mbcsString.Length(), 0, 0);
    if (nRequired == 0)
        return 0;
    if ((dwMapFlags & LCMAP_SORTKEY) && (cchDest == 0))
        return nRequired;

    CMbcsBuffer mbcsDest;
    if (!mbcsDest.SetCapacity(nRequired))
        return 0;

    nRequired = ::LCMapStringA(Locale, dwMapFlags, 
        mbcsString, mbcsString.Length(), mbcsDest, mbcsDest.BufferSize());
    if (nRequired == 0)
        return 0;

    if (dwMapFlags & LCMAP_SORTKEY) {
        if (cchDest < nRequired) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return 0;
        }
        ::CopyMemory(lpDestStr, mbcsDest.get(), nRequired);
        return nRequired;
    }

    return ::MultiByteToWideChar(CP_ACP, 0, mbcsDest, nRequired, lpDestStr, cchDest);
}


OCOW_DEF(HMODULE, LoadLibraryExW,(
    IN LPCWSTR lpLibFileName,
    IN HANDLE hFile,
    IN DWORD dwFlags
    ))
{
    CMbcsBuffer mbcsLibFileName;
    if (!mbcsLibFileName.FromUnicode(lpLibFileName))
        return NULL;

    return ::LoadLibraryExA(mbcsLibFileName, hFile, dwFlags);
}


OCOW_DEF(HMODULE, LoadLibraryW,(
    IN LPCWSTR lpLibFileName
    ))
{
    CMbcsBuffer mbcsLibFileName;
    if (!mbcsLibFileName.FromUnicode(lpLibFileName))
        return NULL;

    return ::LoadLibraryA(mbcsLibFileName);
}


OCOW_DEF(BOOL, MoveFileW,(
    IN LPCWSTR lpExistingFileName,
    IN LPCWSTR lpNewFileName
    ))
{
    CMbcsBuffer mbcsExistingFileName;
    if (!mbcsExistingFileName.FromUnicode(lpExistingFileName))
        return FALSE;

    CMbcsBuffer mbcsNewFileName;
    if (!mbcsNewFileName.FromUnicode(lpNewFileName))
        return FALSE;

    return ::MoveFileA(mbcsExistingFileName, mbcsNewFileName);
}

//TODO: MSLU adds support for CP_UTF7 and CP_UTF8 to Windows 95
int WINAPI
OCOW_MultiByteToWideChar(
    IN UINT     CodePage,
    IN DWORD    dwFlags,
    IN LPCSTR   lpMultiByteStr,
    IN int      cbMultiByte,
    OUT LPWSTR  lpWideCharStr,
    IN int      cchWideChar
    )
{
    return ::MultiByteToWideChar(CodePage, dwFlags, 
        lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
}


OCOW_DEF(HANDLE, OpenEventW,(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::OpenEventA(dwDesiredAccess, bInheritHandle, mbcsName);
}


OCOW_DEF(HANDLE, OpenFileMappingW,(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::OpenFileMappingA(dwDesiredAccess, bInheritHandle, mbcsName);
}


OCOW_DEF(HANDLE, OpenMutexW,(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::OpenMutexA(dwDesiredAccess, bInheritHandle, mbcsName);
}


OCOW_DEF(HANDLE, OpenSemaphoreW,(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN LPCWSTR lpName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return NULL;

    return ::OpenSemaphoreA(dwDesiredAccess, bInheritHandle, mbcsName);
}

typedef HANDLE (WINAPI *fpOpenWaitableTimerA)(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN LPCSTR lpTimerName
    );


static fpOpenWaitableTimerA pOpenWaitableTimerA; //move out to emit cxa_guard_* for libstdc++ dependency
OCOW_DEF(HANDLE, OpenWaitableTimerW,(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN LPCWSTR lpTimerName
    ))
{
    if(!pOpenWaitableTimerA)
        pOpenWaitableTimerA = (fpOpenWaitableTimerA) ::GetProcAddress(
            ::GetModuleHandleA("kernel32.dll"), "OpenWaitableTimerA");
    if (!pOpenWaitableTimerA) {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return NULL;
    }

    CMbcsBuffer mbcsTimerName;
    if (!mbcsTimerName.FromUnicode(lpTimerName))
        return NULL;

    return pOpenWaitableTimerA(dwDesiredAccess, bInheritHandle, mbcsTimerName);
}


OCOW_DEF(VOID, OutputDebugStringW,(
    IN LPCWSTR lpOutputString
    ))
{
    CMbcsBuffer mbcsOutputString;
    if (!mbcsOutputString.FromUnicode(lpOutputString))
        return;
    
    ::OutputDebugStringA(mbcsOutputString);
}

// PeekConsoleInputW
// QueryDosDeviceW
// ReadConsoleInputW
// ReadConsoleOutputCharacterW
// ReadConsoleOutputW
// ReadConsoleW


OCOW_DEF(BOOL, RemoveDirectoryW,(
    IN LPCWSTR lpPathName
    ))
{
    CMbcsBuffer mbcsPathName;
    if (!mbcsPathName.FromUnicode(lpPathName))
        return FALSE;

    return ::RemoveDirectoryA(mbcsPathName);
}

// ScrollConsoleScreenBufferW
// SearchPathW
// SetCalendarInfoW


OCOW_DEF(BOOL, SetComputerNameW,(
    IN LPCWSTR lpComputerName
    ))
{
    // ensure that the computer name is valid as per NT guidelines,
    // we don't coerce the characters here
    const wchar_t * pTemp = lpComputerName;
    for (wchar_t c = *pTemp; c; c = *(++pTemp)) {
        if ((c >= L'A' && c <= L'Z') ||
            (c >= L'a' && c <= L'z') ||
            (c >= L'0' && c <= L'9')) {
            continue;
        }
        switch (c) {
            case L'!': case L'@': case L'#': case L'$': case L'%': case L'^':
            case L'&': case L')': case L'(': case L'.': case L'-': case L'_':
            case L'{': case L'}': case L'~': case L'\'': 
                break;
            default:
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
        }
    }

    CMbcsBuffer mbcsComputerName;
    if (!mbcsComputerName.FromUnicode(lpComputerName))
        return FALSE;

    return ::SetComputerNameA(mbcsComputerName);
}


OCOW_DEF(BOOL, SetConsoleTitleW,(
    IN LPCWSTR lpConsoleTitle
    ))
{
    CMbcsBuffer mbcsConsoleTitle;
    if (!mbcsConsoleTitle.FromUnicode(lpConsoleTitle))
        return FALSE;

    return ::SetConsoleTitleA(mbcsConsoleTitle);
}


OCOW_DEF(BOOL, SetCurrentDirectoryW,(
    IN LPCWSTR lpPathName
    ))
{
    CMbcsBuffer mbcsPathName;
    if (!mbcsPathName.FromUnicode(lpPathName))
        return FALSE;

    return ::SetCurrentDirectoryA(mbcsPathName);
}

// SetDefaultCommConfigW


OCOW_DEF(BOOL, SetEnvironmentVariableW,(
    IN LPCWSTR lpName,
    IN LPCWSTR lpValue
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.FromUnicode(lpName))
        return FALSE;

    CMbcsBuffer mbcsValue;
    if (!mbcsValue.FromUnicode(lpValue))
        return FALSE;

    return ::SetEnvironmentVariableA(mbcsName, mbcsValue);
}


OCOW_DEF(BOOL, SetFileAttributesW,(
    IN LPCWSTR lpFileName,
    IN DWORD dwFileAttributes
    ))
{
    CMbcsBuffer mbcsFileName;
    if (!mbcsFileName.FromUnicode(lpFileName))
        return FALSE;

    return ::SetFileAttributesA(mbcsFileName, dwFileAttributes);
}

// SetLocaleInfoW


OCOW_DEF(BOOL, SetVolumeLabelW,(
    IN LPCWSTR lpRootPathName,
    IN LPCWSTR lpVolumeName
    ))
{
    CMbcsBuffer mbcsRootPathName;
    if (!mbcsRootPathName.FromUnicode(lpRootPathName))
        return FALSE;

    CMbcsBuffer mbcsVolumeName;
    if (!mbcsVolumeName.FromUnicode(lpVolumeName))
        return FALSE;

    return ::SetVolumeLabelA(mbcsRootPathName, mbcsVolumeName);
}

// UpdateResourceA
// UpdateResourceW
// WaitNamedPipeW

//TODO: MSLU adds support for CP_UTF7 and CP_UTF8 to Windows 95
int WINAPI
OCOW_WideCharToMultiByte(
    IN UINT     CodePage,
    IN DWORD    dwFlags,
    IN LPCWSTR  lpWideCharStr,
    IN int      cchWideChar,
    OUT LPSTR   lpMultiByteStr,
    IN int      cbMultiByte,
    IN LPCSTR   lpDefaultChar,
    OUT LPBOOL  lpUsedDefaultChar)
{
    return ::WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar,
        lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar);
}

// WriteConsoleInputW
// WriteConsoleOutputCharacterW
// WriteConsoleOutputW
// WriteConsoleW
// WritePrivateProfileSectionW
// WritePrivateProfileStringW
// WritePrivateProfileStructW
// WriteProfileSectionW
// WriteProfileStringW


OCOW_DEF(LPWSTR, lstrcatW,(
    IN OUT LPWSTR lpString1,
    IN LPCWSTR lpString2
    ))
{
    if (!lpString1 || !lpString2)
        return NULL;
    if (!*lpString2)
        return lpString1;

    LPWSTR lpTemp = lpString1;
    while (*lpTemp) 
        ++lpTemp;
    while (*lpString2)
        *lpTemp++ = *lpString2++;
    *lpTemp = 0;

    return lpString1;
}

// lstrcmpW
// lstrcmpiW
OCOW_DEF(LPWSTR, lstrcpyW,(
    OUT LPWSTR lpString1,
    IN LPCWSTR lpString2
    ))
{
    if (!lpString1 || !lpString2)
        return NULL;

    LPWSTR lpTemp = lpString1;
    while (*lpString2)
        *lpTemp++ = *lpString2++;
    *lpTemp = 0;

    return lpString1;
}


OCOW_DEF(LPWSTR, lstrcpynW,(
    OUT LPWSTR lpString1,
    IN LPCWSTR lpString2,
    IN int iMaxLength
    ))
{
    if (!lpString1 || !lpString2)
        return NULL;
    if (iMaxLength < 1)
        return lpString1;

    LPWSTR lpTemp = lpString1;
    while (*lpString2 && iMaxLength-- > 1)
        *lpTemp++ = *lpString2++;
    *lpTemp = 0;

    return lpString1;
}

int WINAPI
OCOW_lstrlenW(
    IN LPCWSTR lpString
    )
{
    int len = 0;
    while (*lpString++)
        ++len;
    return len;
}
}//EXTERN_C 