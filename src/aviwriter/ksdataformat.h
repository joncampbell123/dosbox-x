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

#ifndef _ISP_UTILS_V4_WIN_KSDATAFORMAT_H_H
#define _ISP_UTILS_V4_WIN_KSDATAFORMAT_H_H

#include "guid.h"

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_ANALOG;		/* 6dba3190-67bd-11cf-a0f7-0020afd156e4 */

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_PCM;		/* 00000001-0000-0010-8000-00aa00389b71 */
extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_ADPCM;		/* 00000002-0000-0010-8000-00aa00389b71 */
extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;	/* 00000003-0000-0010-8000-00aa00389b71 */

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_ALAW;		/* 00000006-0000-0010-8000-00aa00389b71 */
extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_MULAW;		/* 00000007-0000-0010-8000-00aa00389b71 */

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_DRM;		/* 00000009-0000-0010-8000-00aa00389b71 */

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_MPEG;		/* 00000050-0000-0010-8000-00aa00389b71 */

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_RIFF;		/* 4995DAEE-9EE6-11D0-A40E-00A0C9223196 */

extern const windows_GUID windows_KSDATAFORMAT_SUBTYPE_RIFFWAVE;	/* e436eb8b-524f-11ce-9f53-0020af0ba770 */

#endif /* _ISP_UTILS_V4_WIN_KSDATAFORMAT_H_H */

