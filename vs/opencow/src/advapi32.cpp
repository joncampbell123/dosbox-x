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
#define _ADVAPI32_

#include <windows.h>
#include <winreg.h>
#include <lmcons.h>

#include "MbcsBuffer.h"


// ----------------------------------------------------------------------------
// API
EXTERN_C {
// CryptAcquireContextW
// CryptEnumProvidersW
// CryptEnumProviderTypesW
// CryptGetDefaultProviderW
// CryptSetProviderExW
// CryptSetProviderW
// CryptSignHashW
// CryptVerifySignatureW
// GetCurrentHwProfileW
OCOW_DEF(BOOL, GetUserNameW, 
    (OUT LPWSTR lpBuffer, IN OUT LPDWORD nSize))
{
    CMbcsBuffer mbcsBuffer;
    DWORD dwBufSize = (DWORD) mbcsBuffer.BufferSize();
    DWORD dwResult = ::GetUserNameA(mbcsBuffer, &dwBufSize);
    if (!dwResult)
        return 0;

    // ensure the supplied buffer is big enough
    int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, dwBufSize, 0, 0);
    if (*nSize < (DWORD) nRequiredSize) {
        *nSize = (DWORD) nRequiredSize;
        ::SetLastError(ERROR_MORE_DATA);
        return 0;
    }

    ::MultiByteToWideChar(CP_ACP, 0, mbcsBuffer, dwBufSize, lpBuffer, *nSize);
    *nSize = (DWORD) nRequiredSize;
    return 1;
}

// IsTextUnicode
OCOW_DEF(LONG, RegConnectRegistryW,
    (IN LPCWSTR lpMachineName,
    IN HKEY hKey,
    OUT PHKEY phkResult))
{
    CMbcsBuffer mbcsMachineName;
    if (!mbcsMachineName.FromUnicode(lpMachineName))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegConnectRegistryA(mbcsMachineName, hKey, phkResult);
}


OCOW_DEF(LONG, RegCreateKeyExW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey,
    IN DWORD Reserved,
    IN LPWSTR lpClass,
    IN DWORD dwOptions,
    IN REGSAM samDesired,
    IN LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    OUT PHKEY phkResult,
    OUT LPDWORD lpdwDisposition))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsClass;
    if (!mbcsClass.FromUnicode(lpClass))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegCreateKeyExA(hKey, mbcsSubKey, Reserved, mbcsClass, dwOptions, 
        samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}

OCOW_DEF(LONG, RegCreateKeyW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey,
    OUT PHKEY phkResult
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegCreateKeyA(hKey, mbcsSubKey, phkResult);
}

OCOW_DEF(LONG, RegDeleteKeyW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegDeleteKeyA(hKey, mbcsSubKey);
}

OCOW_DEF(LONG, RegDeleteValueW,
    (IN HKEY hKey,
    IN LPCWSTR lpValueName
    ))
{
    CMbcsBuffer mbcsValueName;
    if (!mbcsValueName.FromUnicode(lpValueName))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegDeleteValueA(hKey, mbcsValueName);
}

OCOW_DEF(LONG, RegEnumKeyExW,
    (IN HKEY hKey,
    IN DWORD dwIndex,
    OUT LPWSTR lpName,
    IN OUT LPDWORD lpcbName,
    IN LPDWORD lpReserved,
    IN OUT LPWSTR lpClass,
    IN OUT LPDWORD lpcbClass,
    OUT PFILETIME lpftLastWriteTime
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.SetCapacity(*lpcbName))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsClass;
    if (lpClass && lpcbClass) {
        if (!mbcsClass.SetCapacity(*lpcbClass))
            return ERROR_NOT_ENOUGH_MEMORY;
    }
    else {
        mbcsClass.SetNull();
    }

    DWORD dwNameSize = mbcsName.BufferSize();
    DWORD dwClassSize = mbcsClass.BufferSize();
    LONG lResult = ::RegEnumKeyExA(hKey, dwIndex, mbcsName, &dwNameSize,
        lpReserved, mbcsClass, mbcsClass ? &dwClassSize : NULL, lpftLastWriteTime);
    if (lResult != ERROR_SUCCESS && lResult != ERROR_MORE_DATA)
        return lResult;

    int nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsName, dwNameSize, lpName, *lpcbName);
    *lpcbName = nChars - 1; // don't include null terminator

    if (mbcsClass) {
        nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsClass, dwClassSize, lpClass, *lpcbClass);
        *lpcbClass = nChars - 1; // don't include null terminator
    }

    return lResult;
}

OCOW_DEF(LONG, RegEnumKeyW,
    (IN HKEY hKey,
    IN DWORD dwIndex,
    OUT LPWSTR lpName,
    IN DWORD cbName
    ))
{
    CMbcsBuffer mbcsName;
    if (!mbcsName.SetCapacity(cbName))
        return ERROR_NOT_ENOUGH_MEMORY;

    LONG lResult = ::RegEnumKeyA(hKey, dwIndex, mbcsName, mbcsName.BufferSize());
    if (lResult != ERROR_SUCCESS && lResult != ERROR_MORE_DATA)
        return lResult;

    ::MultiByteToWideChar(CP_ACP, 0, mbcsName, -1, lpName, cbName);
    return lResult;
}

OCOW_DEF(LONG, RegEnumValueW,
    (IN HKEY hKey,
    IN DWORD dwIndex,
    OUT LPWSTR lpValueName,
    IN OUT LPDWORD lpcbValueName,
    IN LPDWORD lpReserved,
    OUT LPDWORD lpType,
    OUT LPBYTE lpData,
    IN OUT LPDWORD lpcbData
    ))
{
    CMbcsBuffer mbcsValueName;
    if (!mbcsValueName.SetCapacity(*lpcbValueName))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsData;
    if (lpData) {
        if (!mbcsData.SetCapacity(*lpcbData))
            return ERROR_NOT_ENOUGH_MEMORY;
    }
    else {
        mbcsData.SetNull();
    }

    DWORD dwType;
    DWORD dwValueNameLen = mbcsValueName.BufferSize();
    DWORD dwDataLen = mbcsData.BufferSize();
    LONG lResult = ::RegEnumValueA(hKey, dwIndex, mbcsValueName, &dwValueNameLen,
        lpReserved, &dwType, (LPBYTE) mbcsData.get(), &dwDataLen);
    if (lResult != ERROR_SUCCESS)
        return lResult;

    int nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsValueName, dwValueNameLen, lpValueName, *lpcbValueName);
    *lpcbValueName = nChars - 1; // don't include null terminator

    if (lpType)
        *lpType = dwType;

    if (mbcsData) {
        int len, nConvertLen = -1;
        switch (dwType) {
        case REG_EXPAND_SZ:
        case REG_SZ:
            nConvertLen = ::lstrlenA(mbcsData) + 1;
            break;
        case REG_MULTI_SZ:
            nConvertLen = 0;
            len = ::lstrlenA(mbcsData);
            while (len > 0) {
                nConvertLen += len + 1;
                len = ::lstrlenA(mbcsData.get() + nConvertLen);
            }
            ++nConvertLen;
        }

        // if we have a value > 0 for convert len then we have a string to convert
        if (nConvertLen > 0) {
            nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsData, nConvertLen, 
                (wchar_t*) lpData, (*lpcbData)/sizeof(wchar_t));
            *lpcbData = nChars; // include NULL 
        }
        else {
            ::CopyMemory(lpData, mbcsData.get(), dwDataLen);
            *lpcbData = dwDataLen;
        }
    }
    else if (lpcbData) {
        *lpcbData = dwDataLen;
    }

    return lResult;
}

OCOW_DEF(LONG, RegLoadKeyW,
    (IN HKEY    hKey,
    IN LPCWSTR  lpSubKey,
    IN LPCWSTR  lpFile
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsFile;
    if (!mbcsFile.FromUnicode(lpFile))
        return ERROR_NOT_ENOUGH_MEMORY;

    // RegLoadKeyA doesn't support long file names, we do the conversion
    // ourselves so that we have a consistent interface
    DWORD dwResult = ::GetShortPathNameA(mbcsFile, mbcsFile, mbcsFile.BufferSize());
    if (dwResult < 1 || dwResult > (DWORD) mbcsFile.BufferSize())
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegLoadKeyA(hKey, mbcsSubKey, mbcsFile);
}

OCOW_DEF(LONG, RegOpenKeyExW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey,
    IN DWORD ulOptions,
    IN REGSAM samDesired,
    OUT PHKEY phkResult
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegOpenKeyExA(hKey, mbcsSubKey, ulOptions, samDesired, phkResult);
}

OCOW_DEF(LONG, RegOpenKeyW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey,
    OUT PHKEY phkResult
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegOpenKeyA(hKey, mbcsSubKey, phkResult);
}

OCOW_DEF(LONG, RegQueryInfoKeyW,
    (IN HKEY hKey,
    OUT LPWSTR lpClass,
    IN OUT LPDWORD lpcbClass,
    IN LPDWORD lpReserved,
    OUT LPDWORD lpcSubKeys,
    OUT LPDWORD lpcbMaxSubKeyLen,
    OUT LPDWORD lpcbMaxClassLen,
    OUT LPDWORD lpcValues,
    OUT LPDWORD lpcbMaxValueNameLen,
    OUT LPDWORD lpcbMaxValueLen,
    OUT LPDWORD lpcbSecurityDescriptor,
    OUT PFILETIME lpftLastWriteTime
    ))
{
    CMbcsBuffer mbcsClass;
    if (lpClass && lpcbClass) {
        if (!mbcsClass.SetCapacity(*lpcbClass))
            return ERROR_NOT_ENOUGH_MEMORY;
    }
    else {
        mbcsClass.SetNull();
    }

    DWORD dwClassSize = mbcsClass.BufferSize();
    LONG lResult = ::RegQueryInfoKeyA(hKey, mbcsClass, mbcsClass ? &dwClassSize : NULL,
        lpReserved, lpcSubKeys, lpcbMaxSubKeyLen, lpcbMaxClassLen, lpcValues, 
        lpcbMaxValueNameLen, lpcbMaxValueLen, lpcbSecurityDescriptor,
        lpftLastWriteTime);
    if (lResult != ERROR_SUCCESS) {
        if (lResult == ERROR_MORE_DATA && mbcsClass)
            *lpcbMaxClassLen = dwClassSize;
        return lResult;
    }

    if (mbcsClass) {
        int nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsClass, dwClassSize, lpClass, *lpcbClass);
        *lpcbClass = nChars - 1; // don't include null terminator
    }

    return lResult;
}

// RegQueryMultipleValuesW

OCOW_DEF(LONG, RegQueryValueExW,
    (IN HKEY hKey,
    IN LPCWSTR lpValueName,
    IN LPDWORD lpReserved,
    OUT LPDWORD lpType,
    IN OUT LPBYTE lpData,
    IN OUT LPDWORD lpcbData
    ))
{
    CMbcsBuffer mbcsValueName;
    if (!mbcsValueName.FromUnicode(lpValueName))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsData;
    if (lpData) {
        if (!mbcsData.SetCapacity(*lpcbData))
            return ERROR_NOT_ENOUGH_MEMORY;
    }
    else {
        mbcsData.SetNull();
    }

    DWORD dwType;
    DWORD dwDataLen = mbcsData.BufferSize();
    LONG lResult = ::RegQueryValueExA(hKey, mbcsValueName, lpReserved, 
        &dwType, (LPBYTE) mbcsData.get(), &dwDataLen);
    if (lResult != ERROR_SUCCESS)
        return lResult;

    if (lpType)
        *lpType = dwType;

    if (mbcsData) {
        int len, nConvertLen = -1;
        switch (dwType) {
        case REG_EXPAND_SZ:
        case REG_SZ:
            nConvertLen = ::lstrlenA(mbcsData) + 1;
            break;
        case REG_MULTI_SZ:
            nConvertLen = 0;
            len = ::lstrlenA(mbcsData);
            while (len > 0) {
                nConvertLen += len + 1;
                len = ::lstrlenA(mbcsData.get() + nConvertLen);
            }
            ++nConvertLen;
        }

        // if we have a value > 0 for convert len then we have a string to convert
        if (nConvertLen > 0) {
            int nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsData, nConvertLen, 
                (wchar_t*) lpData, (*lpcbData)/sizeof(wchar_t));
            *lpcbData = nChars; // include NULL 
        }
        else {
            ::CopyMemory(lpData, mbcsData.get(), dwDataLen);
            *lpcbData = dwDataLen;
        }
    }
    else if (lpcbData) {
        *lpcbData = dwDataLen;
    }

    return lResult;
}

OCOW_DEF(LONG, RegQueryValueW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey,
    OUT LPWSTR lpValue,
    IN OUT PLONG   lpcbValue
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsValue;
    if (lpValue) {
        if (!mbcsValue.SetCapacity(*lpcbValue))
            return ERROR_NOT_ENOUGH_MEMORY;
    }
    else {
        mbcsValue.SetNull();
    }

    LONG lValueSize = mbcsValue.BufferSize();
    LONG lResult = ::RegQueryValueA(hKey, mbcsSubKey, mbcsValue, &lValueSize);
    if (lResult != ERROR_SUCCESS)
        return lResult;

    if (!mbcsValue) {
        *lpcbValue = lValueSize;
        return lResult;
    }

    int nChars = ::MultiByteToWideChar(CP_ACP, 0, mbcsValue, lValueSize, lpValue, *lpcbValue);
    *lpcbValue = nChars; // include null terminator

    return lResult;
}

OCOW_DEF(LONG, RegReplaceKeyW,
    (IN HKEY     hKey,
    IN LPCWSTR  lpSubKey,
    IN LPCWSTR  lpNewFile,
    IN LPCWSTR  lpOldFile
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsNewFile;
    if (!mbcsNewFile.FromUnicode(lpNewFile))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsOldFile;
    if (!mbcsOldFile.FromUnicode(lpOldFile))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegReplaceKeyA(hKey, mbcsSubKey, mbcsNewFile, mbcsOldFile);
}

OCOW_DEF(LONG, RegSaveKeyW,
    (IN HKEY hKey,
    IN LPCWSTR lpFile,
    IN LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ))
{
    CMbcsBuffer mbcsFile;
    if (!mbcsFile.FromUnicode(lpFile))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegSaveKeyA(hKey, mbcsFile, lpSecurityAttributes);
}

OCOW_DEF(LONG, RegSetValueExW,
    (IN HKEY hKey,
    IN LPCWSTR lpValueName,
    IN DWORD Reserved,
    IN DWORD dwType,
    IN CONST BYTE* lpData,
    IN DWORD cbData
    ))
{
    CMbcsBuffer mbcsValueName;
    if (!mbcsValueName.FromUnicode(lpValueName))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsData;
    CONST BYTE* pData = lpData;
    if (dwType == REG_EXPAND_SZ || dwType == REG_SZ || dwType == REG_MULTI_SZ) {
        if (!mbcsData.FromUnicode((LPCWSTR)lpData, cbData/sizeof(wchar_t)))
            return ERROR_NOT_ENOUGH_MEMORY;
        cbData = mbcsData.Length() + 1;
        pData = (CONST BYTE*) mbcsData.get();
    }

    return ::RegSetValueExA(hKey, mbcsValueName, Reserved, dwType, pData, cbData);
}

OCOW_DEF(LONG, RegSetValueW,
    (IN HKEY hKey,
    IN LPCWSTR lpSubKey,
    IN DWORD dwType,
    IN LPCWSTR lpData,
    IN DWORD cbData
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    CMbcsBuffer mbcsData;
    if (!mbcsData.FromUnicode(lpData))
        return ERROR_NOT_ENOUGH_MEMORY;
    cbData = ::lstrlenA(mbcsData);

    return ::RegSetValueA(hKey, mbcsSubKey, dwType, mbcsData, cbData);
}

OCOW_DEF(LONG, RegUnLoadKeyW,
    (IN HKEY    hKey,
    IN LPCWSTR lpSubKey
    ))
{
    CMbcsBuffer mbcsSubKey;
    if (!mbcsSubKey.FromUnicode(lpSubKey))
        return ERROR_NOT_ENOUGH_MEMORY;

    return ::RegUnLoadKeyA(hKey, mbcsSubKey);
}
}//EXTERN_C 