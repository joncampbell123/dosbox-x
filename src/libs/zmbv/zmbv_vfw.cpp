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
#include "resource.h"

#include <crtdbg.h>
#include <string.h>

TCHAR szDescription[] = TEXT("Zipped Motion Block Video v0.1");
TCHAR szName[]        = TEXT(CODEC_4CC);

#define VERSION         0x00000001      // 0.1

/********************************************************************
********************************************************************/

CodecInst *encode_table_owner, *decode_table_owner;

/********************************************************************
********************************************************************/

void Msg(const char fmt[], ...) {
  DWORD written;
  char buf[2000];
  va_list val;
  
  va_start(val, fmt);
  wvsprintf(buf, fmt, val);

  const COORD _80x50 = {80,50};
  static BOOL startup = (AllocConsole(), SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), _80x50));
  WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buf, lstrlen(buf), &written, 0);
}


/********************************************************************
********************************************************************/

CodecInst::CodecInst() {
	codec = 0;
}

CodecInst* Open(ICOPEN* icinfo) {
  if (icinfo && icinfo->fccType != ICTYPE_VIDEO)
      return NULL;

  CodecInst* pinst = new CodecInst();

  if (icinfo) icinfo->dwError = pinst ? ICERR_OK : ICERR_MEMORY;

  return pinst;
}

DWORD Close(CodecInst* pinst) {
//    delete pinst;       // this caused problems when deleting at app close time
    return 1;
}

/********************************************************************
********************************************************************/


/********************************************************************
********************************************************************/

BOOL CodecInst::QueryAbout() { return TRUE; }

static BOOL CALLBACK AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_COMMAND) {
    switch (LOWORD(wParam)) {
    case IDOK:
      EndDialog(hwndDlg, 0);
      break;
    case IDC_HOMEPAGE:
      ShellExecute(NULL, NULL, "http://www.dosbox.com", NULL, NULL, SW_SHOW);
      break;
    case IDC_EMAIL:
      ShellExecute(NULL, NULL, "mailto:dosbox.crew@gmail.com", NULL, NULL, SW_SHOW);
      break;
    }
  }
  return FALSE;
}
DWORD CodecInst::About(HWND hwnd) {
  DialogBox(hmoduleCodec, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDialogProc);
  return ICERR_OK;
}

static BOOL CALLBACK ConfigureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  if (uMsg == WM_INITDIALOG) {

  } else if (uMsg == WM_COMMAND) {

    switch (LOWORD(wParam)) {

    case IDOK:

    case IDCANCEL:
      EndDialog(hwndDlg, 0);
      break;

    default:
      return AboutDialogProc(hwndDlg, uMsg, wParam, lParam);    // handle email and home-page buttons
    }
  }
  return FALSE;
}

BOOL CodecInst::QueryConfigure() { return TRUE; }

DWORD CodecInst::Configure(HWND hwnd) {
  DialogBox(hmoduleCodec, MAKEINTRESOURCE(IDD_CONFIGURE), hwnd, ConfigureDialogProc);
  return ICERR_OK;
}


/********************************************************************
********************************************************************/


// we have no state information which needs to be stored

DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize) { return 0; }

DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) { return 0; }


DWORD CodecInst::GetInfo(ICINFO* icinfo, DWORD dwSize) {
  if (icinfo == NULL)
    return sizeof(ICINFO);

  if (dwSize < sizeof(ICINFO))
    return 0;

  icinfo->dwSize            = sizeof(ICINFO);
  icinfo->fccType           = ICTYPE_VIDEO;
  memcpy(&icinfo->fccHandler,CODEC_4CC, 4);
  icinfo->dwFlags           = VIDCF_FASTTEMPORALC | VIDCF_FASTTEMPORALD | VIDCF_TEMPORAL;

  icinfo->dwVersion         = VERSION;
  icinfo->dwVersionICM      = ICVERSION;
  MultiByteToWideChar(CP_ACP, 0, szDescription, -1, icinfo->szDescription, sizeof(icinfo->szDescription)/sizeof(WCHAR));
  MultiByteToWideChar(CP_ACP, 0, szName, -1, icinfo->szName, sizeof(icinfo->szName)/sizeof(WCHAR));

  return sizeof(ICINFO);
}

/********************************************************************
****************************************************************/

static int GetInputBitDepth(const BITMAPINFOHEADER *lpbiIn) {
	if (lpbiIn->biCompression == BI_RGB) {
		if (lpbiIn->biPlanes != 1)
			return -1;

		switch(lpbiIn->biBitCount) {
			case 8:
				return 8;
			case 16:
				return 15;		// Standard Windows 16-bit RGB is 1555.
			case 32:
				return 32;
		}

	} else if (lpbiIn->biCompression == BI_BITFIELDS) {
		// BI_BITFIELDS RGB masks lie right after the BITMAPINFOHEADER structure,
		// at (ptr+40). This is true even for a BITMAPV4HEADER or BITMAPV5HEADER.
		const DWORD *masks = (const DWORD *)(lpbiIn + 1);

		if (lpbiIn->biBitCount == 16) {
			// Test for 16 (555)
			if (masks[0] == 0x7C00 && masks[1] == 0x03E0 && masks[2] == 0x001F)
				return 15;

			// Test for 16 (565)
			if (masks[0] == 0xF800 && masks[1] == 0x07E0 && masks[2] == 0x001F)
				return 16;
		} else if (lpbiIn->biBitCount == 32) {
			if (masks[0] == 0xFF0000 && masks[1] == 0x00FF00 && masks[2] == 0x0000FF)
				return 32;
		}
	}

	return -1;
}

static bool CanCompress(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut, bool requireOutput) {
	if (lpbiIn) {
		if (GetInputBitDepth(lpbiIn) < 0)
			return false;
	} else return false;
	if (lpbiOut) {
		if (memcmp(&lpbiOut->biCompression,CODEC_4CC, 4))
			return false;
	} else return !requireOutput;
	return true;
}

/********************************************************************
****************************************************************/

DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (CanCompress(lpbiIn,lpbiOut,false)) return ICERR_OK;
	return ICERR_BADFORMAT;
}

DWORD CodecInst::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (!lpbiOut)
		return sizeof(BITMAPINFOHEADER);
	lpbiOut->biSize			= sizeof(BITMAPINFOHEADER);
	lpbiOut->biWidth		= lpbiIn->biWidth;
	lpbiOut->biHeight		= lpbiIn->biHeight;
	lpbiOut->biPlanes		= 1;
	lpbiOut->biCompression	= *(const DWORD *)CODEC_4CC;
	lpbiOut->biBitCount		= lpbiIn->biBitCount;
	lpbiOut->biSizeImage	= lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount/8 + 1024;
	lpbiOut->biXPelsPerMeter = lpbiIn->biXPelsPerMeter;
	lpbiOut->biYPelsPerMeter = lpbiIn->biYPelsPerMeter;
	lpbiOut->biClrUsed		= 0;
	lpbiOut->biClrImportant	= 0;
	return ICERR_OK;
}

DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	CompressEnd();  // free resources if necessary
	if (!CanCompress(lpbiIn, lpbiOut, true))
		return ICERR_BADFORMAT;
	codec = new VideoCodec();
	if (!codec)
		return ICERR_MEMORY;
	if (!codec->SetupCompress( lpbiIn->biWidth, lpbiIn->biHeight))
		return ICERR_MEMORY;
	return ICERR_OK;
}

DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (!CanCompress(lpbiIn, lpbiOut, true))
		return ICERR_BADFORMAT;
	return lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount/8 + 1024;
}

DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD dwSize) {
	int i, pitch;
	zmbv_format_t format;
	LPBITMAPINFOHEADER lpbiIn=icinfo->lpbiInput;
	LPBITMAPINFOHEADER lpbiOut=icinfo->lpbiOutput;
	if (!CanCompress(lpbiIn, lpbiOut, true))
		return ICERR_BADFORMAT;
	if (!icinfo->lpInput || !icinfo->lpOutput)
		return ICERR_ABORT;
	switch (GetInputBitDepth(lpbiIn)) {
	case 8:
		format = ZMBV_FORMAT_8BPP;
		pitch = lpbiIn->biWidth;
		break;
	case 15:
		format = ZMBV_FORMAT_15BPP;
		pitch = lpbiIn->biWidth * 2;
		break;
	case 16:
		format = ZMBV_FORMAT_16BPP;
		pitch = lpbiIn->biWidth * 2;
		break;
	case 32:
		format = ZMBV_FORMAT_32BPP;
		pitch = lpbiIn->biWidth * 4;
		break;
	}

	// DIB scanlines for RGB formats are always aligned to DWORD.
	pitch = (pitch + 3) & ~3;

	// force a key frame if requested by the client
	int flags = 0;
	if (icinfo->dwFlags & ICCOMPRESS_KEYFRAME)
		flags |= 1;

	codec->PrepareCompressFrame( flags, format, 0, icinfo->lpOutput, 99999999);
	char *readPt = (char *)icinfo->lpInput + pitch*(lpbiIn->biHeight - 1);
	for(i = 0;i<lpbiIn->biHeight;i++) {
		codec->CompressLines(1, (void **)&readPt );
		readPt -= pitch;
	}
	lpbiOut->biSizeImage = codec->FinishCompressFrame();

	if (flags & 1)
		*icinfo->lpdwFlags = AVIIF_KEYFRAME;
	else
		*icinfo->lpdwFlags = 0;

	return ICERR_OK;
}


DWORD CodecInst::CompressEnd() {
	if (codec) 
		delete codec;
	codec = 0;
	return ICERR_OK;
}

/********************************************************************
********************************************************************/

static bool CanDecompress(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (memcmp(&lpbiIn->biCompression,CODEC_4CC,4))
		return false;
	if (lpbiOut) {
		if (lpbiOut->biCompression!=0) return false;
		if (lpbiOut->biBitCount != 24) return false;
		if (lpbiIn->biWidth!=lpbiOut->biWidth || lpbiIn->biHeight!=lpbiOut->biHeight)
            return false;
	}
	return true;
}

/********************************************************************
********************************************************************/


DWORD CodecInst::DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	return CanDecompress(lpbiIn, lpbiOut) ? ICERR_OK : ICERR_BADFORMAT;
}

DWORD CodecInst::DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (memcmp(&lpbiIn->biCompression,CODEC_4CC,4))
		 return ICERR_BADFORMAT;
	if (!lpbiOut) return sizeof(BITMAPINFOHEADER);
	*lpbiOut = *lpbiIn;
	lpbiOut->biPlanes		= 1;
	lpbiOut->biSize			= sizeof(BITMAPINFOHEADER);
	lpbiOut->biBitCount		= 24;
	lpbiOut->biSizeImage	= ((lpbiOut->biWidth*3 + 3) & ~3) * lpbiOut->biHeight;
	lpbiOut->biCompression	= BI_RGB;
	lpbiOut->biClrUsed		= 0;
	lpbiOut->biClrImportant	= 0;
	return ICERR_OK;
}

DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	DecompressEnd();  // free resources if necessary
	if (!CanDecompress(lpbiIn, lpbiOut))
		return ICERR_BADFORMAT;
	codec=new VideoCodec();
	if (!codec)
		return ICERR_MEMORY;
	if (!codec->SetupDecompress( lpbiIn->biWidth, lpbiIn->biHeight))
		return ICERR_MEMORY;
	return ICERR_OK;
}

DWORD CodecInst::Decompress(ICDECOMPRESS* icinfo, DWORD dwSize) {
	if (!codec || !icinfo)
		return ICERR_ABORT;
	if (codec->DecompressFrame( icinfo->lpInput, icinfo->lpbiInput->biSizeImage)) {
		codec->Output_UpsideDown_24(icinfo->lpOutput);
	} else return ICERR_DONTDRAW;
	return ICERR_OK;
}


// palette-mapped output only
DWORD CodecInst::DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	return ICERR_BADFORMAT;
}

DWORD CodecInst::DecompressEnd() {
	if (codec) 
		delete codec;
	codec = 0;
	return ICERR_OK;
}
