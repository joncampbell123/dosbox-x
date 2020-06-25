/*
 *  Copyright (C) 2018-2020 Jon Campbell
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __VIDEOMGR_UTIL_AVI_H
#define __VIDEOMGR_UTIL_AVI_H

#include <stdint.h>

#include "informational.h"
#include "wave_mmreg.h"
#include "waveformatex.h"
#include "bitmapinfoheader.h"
#include "riff.h"

#if defined(_MSC_VER)
# pragma pack(push,1)
#endif

/* typedef for an AVI FOURCC */
typedef uint32_t avi_fourcc_t;

/* take chars and form them into an AVI FOURCC (little endian byte order) */
#define avi_fourcc_const(a,b,c,d)	( (((uint32_t)(a)) << 0U) | (((uint32_t)(b)) << 8U) | (((uint32_t)(c)) << 16U) | (((uint32_t)(d)) << 24U) )

/* common AVI FOURCCs */
#define avi_riff_AVI			avi_fourcc_const('A','V','I',' ')
#define avi_riff_movi			avi_fourcc_const('m','o','v','i')
#define avi_riff_hdrl			avi_fourcc_const('h','d','r','l')
#define avi_riff_idx1			avi_fourcc_const('i','d','x','1')
#define avi_riff_indx			avi_fourcc_const('i','n','d','x')
#define avi_riff_avih			avi_fourcc_const('a','v','i','h')
#define avi_riff_strl			avi_fourcc_const('s','t','r','l')
#define avi_riff_strh			avi_fourcc_const('s','t','r','h')
#define avi_riff_strf			avi_fourcc_const('s','t','r','f')
#define avi_riff_vprp			avi_fourcc_const('v','p','r','p')
#define avi_riff_dmlh			avi_fourcc_const('d','m','l','h')
#define avi_riff_odml			avi_fourcc_const('o','d','m','l')

#define avi_fccType_audio		avi_fourcc_const('a','u','d','s')
#define avi_fccType_video		avi_fourcc_const('v','i','d','s')
#define avi_fccType_iavs		avi_fourcc_const('i','a','v','s')

/* AVI file struct: AVIMAINHEADER */
/* AVI placement in file:
 * 
 * RIFF:AVI
 *   LIST:hdrl
 *     avih            <------- *HERE* usually first chunk in hdrl list
 *   LIST:strl
 *     strh
 *     strf
 * ...
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	/* NTS: The first two fields defined in Microsoft's SDK headers are not repeated here, because
	 *      they are effectively the AVI RIFF chunk header, which most of our code does not include
	 *      as part of the read (why did you do that, Microsoft?) */
	/* FOURCC	fcc */
	/* uint32_t	cb */
	uint32_t _Little_Endian_	dwMicroSecPerFrame;	/* (4)  +0x00 +0 */
	uint32_t _Little_Endian_	dwMaxBytesPerSec;	/* (4)  +0x04 +4 */
	uint32_t _Little_Endian_	dwPaddingGranularity;	/* (4)  +0x08 +8 */
	uint32_t _Little_Endian_	dwFlags;		/* (4)  +0x0C +12 */
	uint32_t _Little_Endian_	dwTotalFrames;		/* (4)  +0x10 +16 */
	uint32_t _Little_Endian_	dwInitialFrames;	/* (4)  +0x14 +20 */
	uint32_t _Little_Endian_	dwStreams;		/* (4)  +0x18 +24 */
	uint32_t _Little_Endian_	dwSuggestedBufferSize;	/* (4)  +0x1C +28 */
	uint32_t _Little_Endian_	dwWidth;		/* (4)  +0x20 +32 */
	uint32_t _Little_Endian_	dwHeight;		/* (4)  +0x24 +36 */
	uint32_t _Little_Endian_	dwReserved[4];		/* (16) +0x28 +40 */
} GCC_ATTRIBUTE(packed) riff_avih_AVIMAINHEADER;		/* (56) =0x38 =56 */

#define riff_avih_AVIMAINHEADER_flags_HASINDEX				0x00000010UL
#define riff_avih_AVIMAINHEADER_flags_MUSTUSEINDEX			0x00000020UL
#define riff_avih_AVIMAINHEADER_flags_ISINTERLEAVED			0x00000100UL
#define riff_avih_AVIMAINHEADER_flags_TRUSTCKTYPE			0x00000800UL
#define riff_avih_AVIMAINHEADER_flags_WASCAPTUREFILE			0x00010000UL
#define riff_avih_AVIMAINHEADER_flags_COPYRIGHTED			0x00020000UL

/* AVI file struct: AVISTREAMHEADER */
/* AVI placement in file:
 * 
 * RIFF:AVI
 *   LIST:hdrl
 *     avih
 *   LIST:strl         <------- one LIST per AVI stream
 *     strh            <------- *HERE* usually first chunk in strl list
 *     strf
 * ...
 */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	/* NTS: The first two fields defined in Microsoft's SDK headers are not repeated here, because
	 *      they are effectively the AVI RIFF chunk header, which most of our code does not include
	 *      as part of the read (why did you do that, Microsoft?) */
	/* FOURCC	fcc */
	/* uint32_t	cb */
	avi_fourcc_t _Little_Endian_	fccType;		/* (4)  +0x00 +0 */
	avi_fourcc_t _Little_Endian_	fccHandler;		/* (4)  +0x04 +4 */
	uint32_t _Little_Endian_	dwFlags;		/* (4)  +0x08 +8 */
	uint16_t _Little_Endian_	wPriority;		/* (2)  +0x0C +12 */
	uint16_t _Little_Endian_	wLanguage;		/* (2)  +0x0E +14 */
	uint32_t _Little_Endian_	dwInitialFrames;	/* (4)  +0x10 +16 */
	uint32_t _Little_Endian_	dwScale;		/* (4)  +0x14 +20 */
	uint32_t _Little_Endian_	dwRate;			/* (4)  +0x18 +24 */
	uint32_t _Little_Endian_	dwStart;		/* (4)  +0x1C +28 */
	uint32_t _Little_Endian_	dwLength;		/* (4)  +0x20 +32 */
	uint32_t _Little_Endian_	dwSuggestedBufferSize;	/* (4)  +0x24 +36 */
	uint32_t _Little_Endian_	dwQuality;		/* (4)  +0x28 +40 */
	uint32_t _Little_Endian_	dwSampleSize;		/* (4)  +0x2C +44 */
	struct {
		int16_t _Little_Endian_	left;			/* (2)  +0x30 +48 */
		int16_t _Little_Endian_	top;			/* (2)  +0x32 +50 */
		int16_t _Little_Endian_	right;			/* (2)  +0x34 +52 */
		int16_t _Little_Endian_	bottom;			/* (2)  +0x36 +54 */
	} GCC_ATTRIBUTE(packed) rcFrame;
} GCC_ATTRIBUTE(packed) riff_strh_AVISTREAMHEADER;		/* (56) +0x38 +56 */

static const riff_strh_AVISTREAMHEADER riff_strh_AVISTREAMHEADER_INIT = {
	0,
	0,
	0,
	0,
	0,
	0,//uint32_t        dwInitialFrames;
	0,//uint32_t        dwScale;
	0,//uint32_t        dwRate;
	0,//uint32_t        dwStart;
	0,//uint32_t        dwLength;
	0,//uint32_t        dwSuggestedBufferSize;
	0,//uint32_t        dwQuality;
	0,//uint32_t        dwSampleSize;
	{ 0,0,0,0 }
};

#define riff_strh_AVISTREAMHEADER_flags_DISABLED			0x00000001UL
#define riff_strh_AVISTREAMHEADER_flags_VIDEO_PALCHANGES		0x00010000UL

/* AVIPALCHANGE */
typedef struct {
	uint8_t		bFirstEntry;
	uint8_t		bNumEntries;
	uint16_t	wFlags;
	/* PALETTEENTRY[] */
} GCC_ATTRIBUTE(packed) riff_AVIPALCHANGE_header;

/* AVI palette entry */
typedef struct {
	uint8_t		peRed,peGreen,peBlue,peFlags;
} GCC_ATTRIBUTE(packed) riff_AVIPALCHANGE_PALETTEENTRY;

#define riff_AVIPALCHANGE_PALETTEENTRY_flags_PC_RESERVED		0x01U
#define riff_AVIPALCHANGE_PALETTEENTRY_flags_PC_EXPLICIT		0x02U
#define riff_AVIPALCHANGE_PALETTEENTRY_flags_PC_NOCOLLAPSE		0x04U

/* AVIOLDINDEX (one element of the structure) */
typedef struct {
	uint32_t        dwChunkId;
	uint32_t        dwFlags;
	uint32_t        dwOffset;
	uint32_t        dwSize;
} GCC_ATTRIBUTE(packed) riff_idx1_AVIOLDINDEX;

/* AVIOLDINDEX chunk IDs. NOTE that this chunk ID makes the last two bytes of dwChunkId (the upper 16 bits) */
/* NOTICE due to little Endian byte order the string is typed in reverse here */
#define riff_idx1_AVIOLDINDEX_chunkid_type_mask				0xFFFF0000UL
#define riff_idx1_AVIOLDINDEX_chunkid_stream_index_mask			0x0000FFFFUL
#define riff_idx1_AVIOLDINDEX_chunkid_uncompressed_videoframe		avi_fourcc_const(0,0,'d','b')
#define riff_idx1_AVIOLDINDEX_chunkid_compressed_videoframe		avi_fourcc_const(0,0,'d','c')
#define riff_idx1_AVIOLDINDEX_chunkid_palette_change			avi_fourcc_const(0,0,'p','c')
#define riff_idx1_AVIOLDINDEX_chunkid_audio_data			avi_fourcc_const(0,0,'w','b')

#define riff_idx1_AVIOLDINDEX_flags_LIST				0x00000001UL
#define riff_idx1_AVIOLDINDEX_flags_KEYFRAME				0x00000010UL
#define riff_idx1_AVIOLDINDEX_flags_FIRSTPART				0x00000020UL
#define riff_idx1_AVIOLDINDEX_flags_LASTPART				0x00000040UL
#define riff_idx1_AVIOLDINDEX_flags_NO_TIME				0x00000100UL

/* AVIMETAINDEX (a meta-structure for all the variations in indx and nnix chunks) */
typedef struct {
/*	FOURCC          fcc;
	UINT            cb; */
	uint16_t        wLongsPerEntry;
	uint8_t         bIndexSubType;
	uint8_t         bIndexType;
	uint32_t        nEntriesInUse;
	uint32_t        dwChunkId;
	uint32_t        dwReserved[3];
/*	uint32_t           adwIndex[]; */
} GCC_ATTRIBUTE(packed) riff_indx_AVIMETAINDEX;

#define riff_indx_type_AVI_INDEX_OF_INDEXES				0x00
#define riff_indx_type_AVI_INDEX_OF_CHUNKS				0x01
#define riff_indx_type_AVI_INDEX_OF_TIMED_CHUNKS			0x02
#define riff_indx_type_AVI_INDEX_OF_SUB_2FIELD				0x03
#define riff_indx_type_AVI_INDEX_IS_DATA				0x80

#define riff_indx_subtype_AVI_INDEX_SUB_DEFAULT				0x00
#define riff_indx_subtype_AVI_INDEX_SUB_2FIELD				0x01

/* AVISUPERINDEX */
typedef struct {
/*	FOURCC          fcc;
	UINT            cb; */
	uint16_t        wLongsPerEntry;
	uint8_t         bIndexSubType;
	uint8_t         bIndexType;
	uint32_t        nEntriesInUse;
	uint32_t        dwChunkId;
	uint32_t        dwReserved[3];
/*	AVISUPERINDEXentry[] */
} GCC_ATTRIBUTE(packed) riff_indx_AVISUPERINDEX;

typedef struct {
	uint64_t        qwOffset;
	uint32_t        dwSize;
	uint32_t        dwDuration;
} GCC_ATTRIBUTE(packed) riff_indx_AVISUPERINDEX_entry;

/* AVISTDINDEX */
typedef struct {
/*	FOURCC          fcc;
	UINT            cb; */
	uint16_t        wLongsPerEntry;
	uint8_t         bIndexSubType;
	uint8_t         bIndexType;
	uint32_t        nEntriesInUse;
	uint32_t        dwChunkId;
	uint64_t	qwBaseOffset;
	uint32_t	dwReserved_3;
/*	AVISTDINDEXentry[] */
} GCC_ATTRIBUTE(packed) riff_indx_AVISTDINDEX;

typedef struct {
	uint32_t        dwOffset;		/* relative to qwBaseOffset */
	uint32_t        dwSize;			/* bit 31 is set if delta frame */
} GCC_ATTRIBUTE(packed) riff_indx_AVISTDINDEX_entry;

typedef struct {
	uint32_t        CompressedBMHeight;
	uint32_t        CompressedBMWidth;
	uint32_t        ValidBMHeight;
	uint32_t        ValidBMWidth;
	uint32_t        ValidBMXOffset;
	uint32_t        ValidBMYOffset;
	uint32_t        VideoXOffsetInT;
	uint32_t        VideoYValidStartLine;
} GCC_ATTRIBUTE(packed) riff_vprp_VIDEO_FIELD_DESC;

/* vprp chunk */
typedef struct {
	uint32_t        VideoFormatToken;
	uint32_t        VideoStandard;
	uint32_t        dwVerticalRefreshRate;
	uint32_t        dwHTotalInT;
	uint32_t        dwVTotalInLines;
	uint32_t        dwFrameAspectRatio;
	uint32_t        dwFrameWidthInPixels;
	uint32_t        dwFrameHeightInLines;
	uint32_t        nbFieldPerFrame;
/*	riff_vprp_VIDEO_FIELD_DESC FieldInfo[nbFieldPerFrame]; */
} GCC_ATTRIBUTE(packed) riff_vprp_VideoPropHeader;

/* LIST:odml dmlh chunk */
typedef struct {
	uint32_t	dwTotalFrames;
} GCC_ATTRIBUTE(packed) riff_odml_dmlh_ODMLExtendedAVIHeader;

/* AVI stream format contents if stream type is 'iavs' (Interleaved audio/video stream) */
typedef struct windows_DVINFO {
	uint32_t	dwDVAAuxSrc;
	uint32_t	dwDVAAuxCtl;
	uint32_t	dwDVAAuxSrc1;
	uint32_t	dwDVAAuxCtl1;
	uint32_t	dwDVVAuxSrc;
	uint32_t	dwDVVAuxCtl;
	uint32_t	dwDVReserved[2];
} GCC_ATTRIBUTE(packed) windows_DVINFO; /* =32 bytes */

static const windows_DVINFO WINDOWS_DVINFO_INIT = {
	0,
	0,
	0,
	0,
	0,
	0,
	{0,0}
};

#if defined(_MSC_VER)
# pragma pack(pop)
#endif

#endif

