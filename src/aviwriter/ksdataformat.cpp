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

#include "ksdataformat.h"

/* FIXME! I just realized that the windows_GUID struct definition
 *        is written and packed so as to be used to typecast the
 *        GUID data from Windows (little endian), yet these constant
 *        are written as native endian byte order! We need to add
 *        and use macros to allow writing little endian byte order
 *        constants no matter what the host endian byte order */

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_ANALOG = /* 6dba3190-67bd-11cf-a0f7-0020afd156e4 */
	{0x6dba3190,0x67bd,0x11cf,{0xa0,0xf7},{0x00,0x20,0xaf,0xd1,0x56,0xe4}};

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_PCM = /* 00000001-0000-0010-8000-00aa00389b71 */
	{0x00000001,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};
const windows_GUID windows_KSDATAFORMAT_SUBTYPE_ADPCM = /* 00000002-0000-0010-8000-00aa00389b71 */
	{0x00000002,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};
const windows_GUID windows_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = /* 00000003-0000-0010-8000-00aa00389b71 */
	{0x00000003,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_ALAW = /* 00000006-0000-0010-8000-00aa00389b71 */
	{0x00000006,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};
const windows_GUID windows_KSDATAFORMAT_SUBTYPE_MULAW = /* 00000007-0000-0010-8000-00aa00389b71 */
	{0x00000007,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_DRM = /* 00000009-0000-0010-8000-00aa00389b71 */
	{0x00000009,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_MPEG = /* 00000050-0000-0010-8000-00aa00389b71 */
	{0x00000050,0x0000,0x0010,{0x80,0x00},{0x00,0xaa,0x00,0x38,0x9b,0x71}};

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_RIFF = /* 4995DAEE-9EE6-11D0-A40E-00A0C9223196 */
	{0x4995DAEE,0x9EE6,0x11D0,{0xA4,0x0E},{0x00,0xA0,0xC9,0x22,0x31,0x96}};

const windows_GUID windows_KSDATAFORMAT_SUBTYPE_RIFFWAVE = /* e436eb8b-524f-11ce-9f53-0020af0ba770 */
	{0xe436eb8b,0x524f,0x11ce,{0x9f,0x53},{0x00,0x20,0xaf,0x0b,0xa7,0x70}};

