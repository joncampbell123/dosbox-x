/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
//
// Zipped Motion Block Video
//
// Based on Huffyuv by Ben Rudiak-Gould.
// which was based on MSYUV sample code, which is:
// Copyright (c) 1993 Microsoft Corporation.
// All Rights Reserved.
//

#include "zmbv_vfw.h"

/***************************************************************************
 * DriverProc  -  The entry point for an installable driver.
 *
 * PARAMETERS
 * dwDriverId:  For most messages, <dwDriverId> is the DWORD
 *     value that the driver returns in response to a <DRV_OPEN> message.
 *     Each time that the driver is opened, through the <DrvOpen> API,
 *     the driver receives a <DRV_OPEN> message and can return an
 *     arbitrary, non-zero value. The installable driver interface
 *     saves this value and returns a unique driver handle to the
 *     application. Whenever the application sends a message to the
 *     driver using the driver handle, the interface routes the message
 *     to this entry point and passes the corresponding <dwDriverId>.
 *     This mechanism allows the driver to use the same or different
 *     identifiers for multiple opens but ensures that driver handles
 *     are unique at the application interface layer.
 *
 *     The following messages are not related to a particular open
 *     instance of the driver. For these messages, the dwDriverId
 *     will always be zero.
 *
 *         DRV_LOAD, DRV_FREE, DRV_ENABLE, DRV_DISABLE, DRV_OPEN
 *
 * hDriver: This is the handle returned to the application by the
 *    driver interface.
 *
 * uiMessage: The requested action to be performed. Message
 *     values below <DRV_RESERVED> are used for globally defined messages.
 *     Message values from <DRV_RESERVED> to <DRV_USER> are used for
 *     defined driver protocols. Messages above <DRV_USER> are used
 *     for driver specific messages.
 *
 * lParam1: Data for this message.  Defined separately for
 *     each message
 *
 * lParam2: Data for this message.  Defined separately for
 *     each message
 *
 * RETURNS
 *   Defined separately for each message.
 *
 ***************************************************************************/
LRESULT PASCAL DriverProc(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) {
  CodecInst* pi = (CodecInst*)dwDriverID;

  switch (uiMessage) {
    case DRV_LOAD:
      return (LRESULT)1L;

    case DRV_FREE:
      return (LRESULT)1L;

    case DRV_OPEN:
      // GAAH! This used to return a pointer to 0xFFFF0000 when lParam==0!
      return (LRESULT)(DWORD)(UINT) Open((ICOPEN*) lParam2);

    case DRV_CLOSE:
      if (pi) Close(pi);
      return (LRESULT)1L;

    /*********************************************************************

      state messages

    *********************************************************************/

    // cwk
    case DRV_QUERYCONFIGURE:    // configuration from drivers applet
      return (LRESULT)1L;

    case DRV_CONFIGURE:
      pi->Configure((HWND)lParam1);
      return DRV_OK;

    case ICM_CONFIGURE:
      //
      //  return ICERR_OK if you will do a configure box, error otherwise
      //
      if (lParam1 == -1)
        return pi->QueryConfigure() ? ICERR_OK : ICERR_UNSUPPORTED;
      else
        return pi->Configure((HWND)lParam1);

    case ICM_ABOUT:
      //
      //  return ICERR_OK if you will do a about box, error otherwise
      //
      if (lParam1 == -1)
        return pi->QueryAbout() ? ICERR_OK : ICERR_UNSUPPORTED;
      else
        return pi->About((HWND)lParam1);

    case ICM_GETSTATE:
      return pi->GetState((LPVOID)lParam1, (DWORD)lParam2);

    case ICM_SETSTATE:
      return pi->SetState((LPVOID)lParam1, (DWORD)lParam2);

    case ICM_GETINFO:
      return pi->GetInfo((ICINFO*)lParam1, (DWORD)lParam2);

    case ICM_GETDEFAULTQUALITY:
      if (lParam1) {
        *((LPDWORD)lParam1) = 10000;
        return ICERR_OK;
      }
      break;

    /*********************************************************************

      compression messages

    *********************************************************************/

    case ICM_COMPRESS_QUERY:
      return pi->CompressQuery((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_COMPRESS_BEGIN:
      return pi->CompressBegin((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_COMPRESS_GET_FORMAT:
      return pi->CompressGetFormat((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_COMPRESS_GET_SIZE:
      return pi->CompressGetSize((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_COMPRESS:
      return pi->Compress((ICCOMPRESS*)lParam1, (DWORD)lParam2);

    case ICM_COMPRESS_END:
      return pi->CompressEnd();

    /*********************************************************************

      decompress messages

    *********************************************************************/

    case ICM_DECOMPRESS_QUERY:
      return pi->DecompressQuery((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_DECOMPRESS_BEGIN:
      return pi->DecompressBegin((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_DECOMPRESS_GET_FORMAT:
      return pi->DecompressGetFormat((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_DECOMPRESS_GET_PALETTE:
      return pi->DecompressGetPalette((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);

    case ICM_DECOMPRESS:
      return pi->Decompress((ICDECOMPRESS*)lParam1, (DWORD)lParam2);

    case ICM_DECOMPRESS_END:
      return pi->DecompressEnd();

    /*********************************************************************

      standard driver messages

    *********************************************************************/

    case DRV_DISABLE:
    case DRV_ENABLE:
      return (LRESULT)1L;

    case DRV_INSTALL:
    case DRV_REMOVE:
      return (LRESULT)DRV_OK;
  }

  if (uiMessage < DRV_USER)
    return DefDriverProc(dwDriverID, hDriver, uiMessage, lParam1, lParam2);
  else
    return ICERR_UNSUPPORTED;
}


HMODULE hmoduleCodec=0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID) {
  hmoduleCodec = (HMODULE) hinstDLL;
  return TRUE;
}
