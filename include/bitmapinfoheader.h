
#ifndef __ISP_UTILS_V4_BITMAPINFOHEADER
#define __ISP_UTILS_V4_BITMAPINFOHEADER

#include <stdint.h>
#include "informational.h"

/* [doc] windows_BITMAPFILEHEADER
 *
 * Packed portable representation of the Microsoft Windows BITMAPFILEHEADER
 * structure.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint16_t _Little_Endian_	bfType;			/* (2) +0x00 +0 */
	uint32_t _Little_Endian_	bfSize;			/* (4) +0x02 +2 */
	uint16_t _Little_Endian_	bfReserved1;		/* (2) +0x06 +6 */
	uint16_t _Little_Endian_	bfReserved2;		/* (2) +0x08 +8 */
	uint32_t _Little_Endian_	bfOffBits;		/* (4) +0x0A +10 */
} GCC_ATTRIBUTE(packed) windows_BITMAPFILEHEADER;		/* (14) =0x0E =14 */

static const windows_BITMAPFILEHEADER WINDOWS_BITMAPFILEHEADER_INIT = {
	0,
	0,
	0,
	0,
	0
};

/* [doc] windows_BITMAPINFOHEADER
 *
 * Packed portable representation of the Microsoft Windows BITMAPINFOHEADER
 * structure.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint32_t _Little_Endian_	biSize;			/* (4) +0x00 +0 */
	int32_t _Little_Endian_		biWidth;		/* (4) +0x04 +4 */
	int32_t _Little_Endian_		biHeight;		/* (4) +0x08 +8 */
	uint16_t _Little_Endian_	biPlanes;		/* (2) +0x0C +12 */
	uint16_t _Little_Endian_	biBitCount;		/* (2) +0x0E +14 */
	uint32_t _Little_Endian_	biCompression;		/* (4) +0x10 +16 */
	uint32_t _Little_Endian_	biSizeImage;		/* (4) +0x14 +20 */
	int32_t _Little_Endian_		biXPelsPerMeter;	/* (4) +0x18 +24 */
	int32_t _Little_Endian_		biYPelsPerMeter;	/* (4) +0x1C +28 */
	uint32_t _Little_Endian_	biClrUsed;		/* (4) +0x20 +32 */
	uint32_t _Little_Endian_	biClrImportant;		/* (4) +0x24 +36 */
} GCC_ATTRIBUTE(packed) windows_BITMAPINFOHEADER;		/* (40) =0x28 =40 */

static const windows_BITMAPINFOHEADER WINDOWS_BITMAPINFOHEADER_INIT = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

/* [doc] windows_CIEXYZ
 *
 * Packed portable representation of the Microsoft Windows CIEXYZ
 * structure.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint32_t _Little_Endian_	ciexyzX;		/* (4) +0x00 +0 */
	uint32_t _Little_Endian_	ciexyzY;		/* (4) +0x04 +4 */
	uint32_t _Little_Endian_	ciexyzZ;		/* (4) +0x08 +8 */
} GCC_ATTRIBUTE(packed) windows_CIEXYZ;			/* (12) =0x0C =12 */

/* [doc] windows_CIEXYZTRIPLE
 *
 * Packed portable representation of the Microsoft Windows CIEXYZTRIPLE
 * structure.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	windows_CIEXYZ			ciexyzRed;		/* (12) +0x00 +0 */
	windows_CIEXYZ			ciexyzGreen;		/* (12) +0x0C +12 */
	windows_CIEXYZ			ciexyzBlue;		/* (12) +0x18 +24 */
} GCC_ATTRIBUTE(packed) windows_CIEXYZTRIPLE;			/* (36) =0x24 =36 */

/* [doc] windows_BITMAPV4HEADER
 *
 * Packed portable representation of the Microsoft Windows BITMAPV4HEADER
 * structure.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint32_t _Little_Endian_	bV4Size;		/* (4) +0x00 +0 */
	int32_t _Little_Endian_		bV4Width;		/* (4) +0x04 +4 */
	int32_t _Little_Endian_		bV4Height;		/* (4) +0x08 +8 */
	uint16_t _Little_Endian_	bV4Planes;		/* (2) +0x0C +12 */
	uint16_t _Little_Endian_	bV4BitCount;		/* (2) +0x0E +14 */
	uint32_t _Little_Endian_	bV4V4Compression;	/* (4) +0x10 +16 */
	uint32_t _Little_Endian_	bV4SizeImage;		/* (4) +0x14 +20 */
	int32_t _Little_Endian_		bV4XPelsPerMeter;	/* (4) +0x18 +24 */
	int32_t _Little_Endian_		bV4YPelsPerMeter;	/* (4) +0x1C +28 */
	uint32_t _Little_Endian_	bV4ClrUsed;		/* (4) +0x20 +32 */
	uint32_t _Little_Endian_	bV4ClrImportant;	/* (4) +0x24 +36 */
	uint32_t _Little_Endian_	bV4RedMask;		/* (4) +0x28 +40 */
	uint32_t _Little_Endian_	bV4GreenMask;		/* (4) +0x2C +44 */
	uint32_t _Little_Endian_	bV4BlueMask;		/* (4) +0x30 +48 */
	uint32_t _Little_Endian_	bV4AlphaMask;		/* (4) +0x34 +52 */
	uint32_t _Little_Endian_	bV4CSType;		/* (4) +0x38 +56 */
	windows_CIEXYZTRIPLE		bV4Endpoints;		/* (36) +0x3C +60 */
	uint32_t _Little_Endian_	bV4GammaRed;		/* (4) +0x60 +96 */
	uint32_t _Little_Endian_	bV4GammaGreen;		/* (4) +0x64 +100 */
	uint32_t _Little_Endian_	bV4GammaBlue;		/* (4) +0x68 +104 */
} GCC_ATTRIBUTE(packed) windows_BITMAPV4HEADER;		/* (84) =0x6C =108 */

/* [doc] windows_BITMAPV5HEADER
 *
 * Packed portable representation of the Microsoft Windows BITMAPV5HEADER
 * structure.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint32_t _Little_Endian_	bV5Size;		/* (4) +0x00 +0 */
	int32_t _Little_Endian_		bV5Width;		/* (4) +0x04 +4 */
	int32_t _Little_Endian_		bV5Height;		/* (4) +0x08 +8 */
	uint16_t _Little_Endian_	bV5Planes;		/* (2) +0x0C +12 */
	uint16_t _Little_Endian_	bV5BitCount;		/* (2) +0x0E +14 */
	uint32_t _Little_Endian_	bV5Compression;		/* (4) +0x10 +16 */
	uint32_t _Little_Endian_	bV5SizeImage;		/* (4) +0x14 +20 */
	int32_t _Little_Endian_		bV5XPelsPerMeter;	/* (4) +0x18 +24 */
	int32_t _Little_Endian_		bV5YPelsPerMeter;	/* (4) +0x1C +28 */
	uint32_t _Little_Endian_	bV5ClrUsed;		/* (4) +0x20 +32 */
	uint32_t _Little_Endian_	bV5ClrImportant;	/* (4) +0x24 +36 */
	uint32_t _Little_Endian_	bV5RedMask;		/* (4) +0x28 +40 */
	uint32_t _Little_Endian_	bV5GreenMask;		/* (4) +0x2C +44 */
	uint32_t _Little_Endian_	bV5BlueMask;		/* (4) +0x30 +48 */
	uint32_t _Little_Endian_	bV5AlphaMask;		/* (4) +0x34 +52 */
	uint32_t _Little_Endian_	bV5CSType;		/* (4) +0x38 +56 */
	windows_CIEXYZTRIPLE		bV5Endpoints;		/* (36) +0x3C +60 */
	uint32_t _Little_Endian_	bV5GammaRed;		/* (4) +0x60 +96 */
	uint32_t _Little_Endian_	bV5GammaGreen;		/* (4) +0x64 +100 */
	uint32_t _Little_Endian_	bV5GammaBlue;		/* (4) +0x68 +104 */
	uint32_t _Little_Endian_	bV5Intent;		/* (4) +0x6C +108 */
	uint32_t _Little_Endian_	bV5ProfileData;		/* (4) +0x70 +112 */
	uint32_t _Little_Endian_	bV5ProfileSize;		/* (4) +0x74 +116 */
	uint32_t _Little_Endian_	bV5Reserved;		/* (4) +0x78 +120 */
} GCC_ATTRIBUTE(packed) windows_BITMAPV5HEADER;		/* (100) =0x7C =124 */

#endif


