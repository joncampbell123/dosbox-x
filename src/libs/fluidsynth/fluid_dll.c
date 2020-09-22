/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifdef WIN32
#include "utils\fluidsynth_priv.h"
#include "utils\fluid_sys.h"

static HINSTANCE fluid_hinstance = NULL;
static HWND fluid_wnd = NULL;
static int fluid_refCount = 0;

int fluid_win32_create_window(void);
void fluid_win32_destroy_window(void);
HWND fluid_win32_get_window(void);

#ifndef FLUIDSYNTH_NOT_A_DLL
BOOL WINAPI DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  FLUID_LOG(FLUID_DBG, "DllMain");
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    fluid_refCount++;
    if (1 == fluid_refCount) {
      fluid_set_hinstance((void*) hModule);
      fluid_win32_create_window();
    }
    break;
  case DLL_PROCESS_DETACH:
    fluid_refCount--;
    if (fluid_refCount == 0) {
      fluid_win32_destroy_window();
    }
    break;
  }
  return TRUE;
}
#endif

/**
 * Set the handle to the instance of the application on the Windows platform.
 * @param Application instance pointer
 *
 * The handle is needed to open DirectSound.
 */
void fluid_set_hinstance(void* hinstance)
{
  if (fluid_hinstance == NULL) {
    fluid_hinstance = (HINSTANCE) hinstance;
    FLUID_LOG(FLUID_DBG, "DLL instance = %d", (int) fluid_hinstance);
  }
}

/**
 * Get the handle to the instance of the application on the Windows platform.
 * @return Application instance pointer or NULL if not set
 */
void* fluid_get_hinstance(void)
{
  return (void*) fluid_hinstance;
}

static long FAR PASCAL fluid_win32_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
  case WM_CREATE:
    break;
  case WM_DESTROY:
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
    break;
  }
  return(0L);
}

int fluid_win32_create_window(void)
{
  WNDCLASS myClass;
  myClass.hCursor = LoadCursor( NULL, IDC_ARROW );
  myClass.hIcon = NULL;
  myClass.lpszMenuName = (LPSTR) NULL;
  myClass.lpszClassName = (LPSTR) "FluidSynth";
  myClass.hbrBackground = (HBRUSH)(COLOR_WINDOW);
  myClass.hInstance = fluid_hinstance;
  myClass.style = CS_GLOBALCLASS;
  myClass.lpfnWndProc = fluid_win32_wndproc;
  myClass.cbClsExtra = 0;
  myClass.cbWndExtra = 0;
  if (!RegisterClass(&myClass)) {
    return -100;
  }
  fluid_wnd = CreateWindow((LPSTR) "FluidSynth", (LPSTR) "FluidSynth", WS_OVERLAPPEDWINDOW,
              CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, (HWND) NULL, (HMENU) NULL,
              fluid_hinstance, (LPSTR) NULL);
  if (fluid_wnd == NULL) {
    FLUID_LOG(FLUID_ERR, "Can't create window");
    return -101;
  }
  return 0;
}

void fluid_win32_destroy_window(void)
{
  HWND hwnd = fluid_win32_get_window();
  if (hwnd) {
    DestroyWindow(hwnd);
    fluid_wnd = 0;
  }
}

HWND fluid_win32_get_window(void)
{
  return fluid_wnd;
}

#endif	// #ifdef WIN32
