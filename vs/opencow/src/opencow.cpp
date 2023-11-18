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
#include <stdlib.h>

#include "SystemVersion.h"

#ifdef _DEBUG
# define BUILD_TYPE     "Debug"
# define DEBUG_DEFAULT  2
#else
# define BUILD_TYPE     "Release"
# define DEBUG_DEFAULT  0
#endif

// ----------------------------------------------------------------------------
// Globals

HINSTANCE   g_hInstanceDLL          = NULL;
int         g_nPlatform             = MZ_NOT_INITIALIZED;
int         g_nPlatformServicePack  = 0;
HMODULE     g_hOleacc               = NULL;
HMODULE     g_hSensapi              = NULL;
HMODULE     g_hImm32                = NULL;
int         g_nDebug                = DEBUG_DEFAULT;

// ----------------------------------------------------------------------------
// Local Functions

static BOOL InitializeSystemVersion();

// ----------------------------------------------------------------------------
// DLL 
#if !defined(STATIC_OPENCOW)
BOOL APIENTRY 
DllMain(
    HINSTANCE   hInstanceDLL, 
    DWORD       dwReason, 
    LPVOID      /*lpReserved*/ 
    )
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstanceDLL = hInstanceDLL;
        ::DisableThreadLibraryCalls(hInstanceDLL);

        if (!InitializeSystemVersion())
            return FALSE;

        // determine the debug level that we are running at. 
        // Level 0 = no notification messages (default for release build)
        // Level 1 = load and unload
        // Level 2 = 1 + GetProcAddress of opencow implemented functions
        char szValue[16];
        DWORD dwLen = ::GetEnvironmentVariableA("OPENCOW_DEBUG", 
            szValue, sizeof(szValue));
        if (dwLen == 1) {
            if (*szValue >= '0' && *szValue <= '2')
                g_nDebug = *szValue - '0';
        }

        // if we are running at any debug level then we output a message
        // when the library is loaded
        if (g_nDebug >= 1) {
            int nResult = ::MessageBoxA(NULL, 
                "Opencow " BUILD_TYPE " DLL has been loaded. "
                "Press cancel to disable all notifications.", "opencow", 
                MB_OKCANCEL | MB_SETFOREGROUND);
            if (nResult == IDCANCEL)
                g_nDebug = 0;
        }

        return TRUE;
    }

    if (dwReason == DLL_PROCESS_DETACH)
    {
        if (g_hOleacc) {
            FreeLibrary(g_hOleacc);
            g_hOleacc = NULL;
        }

        if (g_hSensapi) {
            FreeLibrary(g_hSensapi);
            g_hSensapi = NULL;
        }

        if (g_hImm32) {
            FreeLibrary(g_hImm32);
            g_hImm32 = NULL;
        }

        // if we are running at any debug level then we output a message
        // when the library is unloaded
        if (g_nDebug >= 1) {
            ::MessageBoxA(NULL, 
                "Opencow " BUILD_TYPE " DLL has been unloaded.", "opencow", 
                MB_OK | MB_SETFOREGROUND);
        }

        return TRUE;
    }

    return TRUE;
}
#endif
// ----------------------------------------------------------------------------
// API

static int 
GetServicePack(
    const char * aVersion)
{
    static const char szServicePack[] = "Service Pack ";
    static int ServicePackNumberOffset = sizeof(szServicePack) - 1;
    if (strncmp(aVersion, szServicePack, ServicePackNumberOffset))
        return 0;
    return aVersion[ServicePackNumberOffset] - '0';
}

static BOOL 
InitializeSystemVersion()
{
    OSVERSIONINFOA osvi;
    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (!GetVersionExA(&osvi))
        return FALSE;

    switch (osvi.dwMajorVersion)
    {
    case 3:
        g_nPlatform = MZ_PLATFORM_NT35;
        g_nPlatformServicePack = GetServicePack(osvi.szCSDVersion);
        break;

    case 4:
        switch (osvi.dwMinorVersion)
        {
        case 0:
            if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
            {
                g_nPlatform = MZ_PLATFORM_NT4;
                g_nPlatformServicePack = GetServicePack(osvi.szCSDVersion);
            }
            else
            {
                g_nPlatform = MZ_PLATFORM_95;
                if (osvi.szCSDVersion[0] == 'C')
                    g_nPlatformServicePack = 2; // OSR2
            }
            break;

        case 10:
            g_nPlatform = MZ_PLATFORM_98;
            if (osvi.szCSDVersion[0] == 'A')
                g_nPlatformServicePack = 2; // 98SE
            break;

        case 90:
            g_nPlatform = MZ_PLATFORM_ME;
        }
        break;

    case 5:
        switch (osvi.dwMinorVersion)
        {
        case 0:
            g_nPlatform = MZ_PLATFORM_2K;
            break;

        case 1:
            g_nPlatform = MZ_PLATFORM_XP;
            break;

        case 2:
            g_nPlatform = MZ_PLATFORM_2K3;
            break;
        }
        g_nPlatformServicePack = GetServicePack(osvi.szCSDVersion);
    }

    return TRUE; 
}

