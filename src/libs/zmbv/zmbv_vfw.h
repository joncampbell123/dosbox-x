//
// Huffyuv v2.1.1, by Ben Rudiak-Gould.
// http://www.math.berkeley.edu/~benrg/huffyuv.html
//
// This file is copyright 2000 Ben Rudiak-Gould, and distributed under
// the terms of the GNU General Public License, v2 or later.  See
// http://www.gnu.org/copyleft/gpl.html.
//
// I edit these files in 10-point Verdana, a proportionally-spaced font.
// You may notice formatting oddities if you use a monospaced font.
//


#include <windows.h>
#include <vfw.h>
#include <zlib.h>
#include "zmbv.h"
#pragma hdrstop

extern HMODULE hmoduleCodec;


// huffyuv.cpp

class CodecInst {
private:
	VideoCodec * codec;
public:
  CodecInst();
  ~CodecInst();

  BOOL QueryAbout();
  DWORD About(HWND hwnd);

  BOOL QueryConfigure();
  DWORD Configure(HWND hwnd);

  DWORD GetState(LPVOID pv, DWORD dwSize);
  DWORD SetState(LPVOID pv, DWORD dwSize);

  DWORD GetInfo(ICINFO* icinfo, DWORD dwSize);

  DWORD CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD Compress(ICCOMPRESS* icinfo, DWORD dwSize);
  DWORD CompressEnd();

  DWORD DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  
  DWORD Decompress(ICDECOMPRESS* icinfo, DWORD dwSize);
  DWORD DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressEnd();
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);

