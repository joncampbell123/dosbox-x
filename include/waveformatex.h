
#ifndef __ISP_UTILS_V4_WIN_WAVEFORMATEX_H
#define __ISP_UTILS_V4_WIN_WAVEFORMATEX_H

#include <stdint.h>

#include "informational.h"
#include "guid.h"	/* <- need windows_GUID definition below */

#ifdef __cplusplus
extern "C" {
#endif

/* [doc] windows_WAVEFORMATOLD
 *
 * Packed portable representation of the Microsoft Windows WAVEFORMAT
 * structure. In the Microsoft SDK this would be "WAVEFORMAT". The
 * wBitsPerSample field is missing. I don't know why Microsoft would
 * define other than perhaps being the earlier form of WAVEFORMAT that
 * was in use when they made the Windows 3.0 Multimedia Extensions.
 *
 * Note that if you actually try to make a WAV file with this variant
 * in the 'fmt' chunk almost nobody will read it properly, except
 * FFMPEG, who will always assume 8-bit signed PCM. Such files will
 * also cause the various multimedia components in Windows 98 to
 * crash!
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint16_t _Little_Endian_	wFormatTag;		/* (2)  +0x00 +0 */
	uint16_t _Little_Endian_	nChannels;		/* (2)  +0x02 +2 */
	uint32_t _Little_Endian_	nSamplesPerSec;		/* (4)  +0x04 +4 */
	uint32_t _Little_Endian_	nAvgBytesPerSec;	/* (4)  +0x08 +8 */
	uint16_t _Little_Endian_	nBlockAlign;		/* (2)  +0x0C +12 */
} __attribute__((packed)) windows_WAVEFORMATOLD;		/* (14) =0x0E =14 */
#define windows_WAVEFORMATOLD_size (14)

/* [doc] windows_WAVEFORMAT
 *
 * Packed portable representation of the Microsoft Windows WAVEFORMAT
 * structure. Unlike the Microsoft SDK version this struct also includes
 * the wBitsPerSample structure member. If you write plain PCM WAV files
 * this is the format you would write into the 'fmt' chunk. Essentially
 * WAVEFORMATEX without the cbSize.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint16_t _Little_Endian_	wFormatTag;		/* (2)  +0x00 +0 */
	uint16_t _Little_Endian_	nChannels;		/* (2)  +0x02 +2 */
	uint32_t _Little_Endian_	nSamplesPerSec;		/* (4)  +0x04 +4 */
	uint32_t _Little_Endian_	nAvgBytesPerSec;	/* (4)  +0x08 +8 */
	uint16_t _Little_Endian_	nBlockAlign;		/* (2)  +0x0C +12 */
	uint16_t _Little_Endian_	wBitsPerSample;		/* (2)  +0x0E +14 */
} __attribute__((packed)) windows_WAVEFORMAT;			/* (16) +0x10 +16 */
#define windows_WAVEFORMAT_size (16)

/* [doc] windows_WAVEFORMATEX
 *
 * Packed portable representation of the Microsoft Windows WAVEFORMATEX
 * structure. This is the most common format because it can accomodate
 * almost every non-PCM WAVE codec. WAVEFORMATEXTENSIBLE builds on this
 * for additional codecs that don't have a 16-bit wFormatTag registered.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint16_t _Little_Endian_	wFormatTag;		/* (2)  +0x00 +0 */
	uint16_t _Little_Endian_	nChannels;		/* (2)  +0x02 +2 */
	uint32_t _Little_Endian_	nSamplesPerSec;		/* (4)  +0x04 +4 */
	uint32_t _Little_Endian_	nAvgBytesPerSec;	/* (4)  +0x08 +8 */
	uint16_t _Little_Endian_	nBlockAlign;		/* (2)  +0x0C +12 */
	uint16_t _Little_Endian_	wBitsPerSample;		/* (2)  +0x0E +14 */
	uint16_t _Little_Endian_	cbSize;			/* (2)  +0x10 +16 */
} __attribute__((packed)) windows_WAVEFORMATEX;			/* (18) =0x12 =18 */
#define windows_WAVEFORMATEX_size (18)

static const windows_WAVEFORMATEX WINDOWS_WAVEFORMATEX_INIT = {
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

/* [doc] windows_ADPCMWAVEFORMAT
 *
 * Packed portable representation of the Microsoft Windows ADPCMWAVEFORMAT
 * structure for WAVE_FORMAT_MS_ADPCM. Microsoft's original definition
 * implies that wNumCoef could be anything and define the last member as
 * aCoef[]. However, every MS ADPCM WAV I've ever found or made has
 * wNumCoef == 7, so it's defined that way here.
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	windows_WAVEFORMATEX		wfx;			/* (18) +0x00 +0 */
	uint16_t _Little_Endian_	wSamplesPerBlock;	/* (2)  +0x12 +18 */
	uint16_t _Little_Endian_	wNumCoef;		/* (2)  +0x14 +20 */
	uint16_t _Little_Endian_	aCoef[7*2];		/* (28) +0x16 +22 */ /* NTS: This array is wNumCoef*2 large, for MS-ADPCM wNumCoef == 7 */
} __attribute__((packed)) windows_ADPCMWAVEFORMAT;		/* (50) =0x42 =50 */
#define windows_ADPCMWAVEFORMAT_size (50)

/* [doc] windows_IMAADPCMWAVEFORMAT
 *
 * Microsoft's implementation of IMA-ADPCM 4-bit compression
 */
typedef struct ima_adpcmwaveformat_tag {			/* (sizeof) (offset hex) (offset dec) */
	windows_WAVEFORMATEX		wfx;			/* (18) +0x00 +0 */
	uint16_t _Little_Endian_	wSamplesPerBlock;	/* (2)  +0x12 +18 */
} __attribute__((packed)) windows_IMAADPCMWAVEFORMAT;		/* (20) =0x14 +20 */
#define windows_IMAADPCMWAVEFORMAT_size (20)

/* [doc] windows_TRUESPEECHWAVEFORMAT
 *
 */
typedef struct truespeechwaveformat_tag {			/* (sizeof) (offset hex) (offset dec) */
	windows_WAVEFORMATEX		wfx;			/* (18) +0x00 +0 */
	uint16_t _Little_Endian_	wRevision;		/* (2)  +0x12 +18 */
	uint16_t _Little_Endian_	nSamplesPerBlock;	/* (2)  +0x14 +20 */
	uint8_t				abReserved[28];		/* (28) +0x16 +22 */
} __attribute__((packed)) windows_TRUESPEECHWAVEFORMAT;		/* (50) =0x42 =50 */
#define windows_TRUESPEECHWAVEFORMAT_size (50)

/* [doc] windows_GSM610WAVEFORMAT
 *
 */
typedef struct gsm610waveformat_tag {				/* (sizeof) (offset hex) (offset dec) */
	windows_WAVEFORMATEX		wfx;			/* (18) +0x00 +0 */
	uint16_t _Little_Endian_	wSamplesPerBlock;	/* (2)  +0x12 +18 */
} __attribute__((packed)) windows_GSM610WAVEFORMAT;		/* (20) =0x14 =20 */
#define windows_GSM610WAVEFORMAT_size (20)

/* [doc] windows_WAVEFORMATEXTENSIBLE
 *
 * Packed portable representation of the Microsoft Windows WAVEFORMATEXTENSIBLE
 * structure. This is the latest evolution of the WAVEFORMATEX structure
 * that allows Windows to represent a wider variety of formats, extend codecs
 * beyond the original wFormatTag registry, and support up to 32 channel audio
 * with defined channel mapping. Microsoft also expects you to use this for
 * 24-bit and 32-bit PCM even though you *can* define WAVEFORMAT structures
 * for 24/32-bit as well (most will play it except Windows Media Player).
 */
typedef struct {							/* (sizeof) (offset hex) (offset dec) */
	windows_WAVEFORMATEX			Format;			/* (18) +0x00 +0 */
	union { /* Ooookay Microsoft how do I derive meaning from THIS now? */
		uint16_t _Little_Endian_	wValidBitsPerSample;	/* <- if it's PCM */
		uint16_t _Little_Endian_	wSamplesPerBlock;	/* <- if it's not PCM, and compressed */
		uint16_t _Little_Endian_	wReserved;		/* <- if ??? */
	} Samples;							/* (2)  +0x12 +18 */
	uint32_t _Little_Endian_		dwChannelMask;		/* (4)  +0x14 +20 */
	windows_GUID				SubFormat;		/* (16) +0x18 +24 */
} __attribute__((packed)) windows_WAVEFORMATEXTENSIBLE;			/* (40) =0x28 =40 */
#define windows_WAVEFORMATEXTENSIBLE_size (40)

#ifdef __cplusplus
}
#endif

#endif /* __ISP_UTILS_V4_WIN_WAVEFORMATEX_H */

