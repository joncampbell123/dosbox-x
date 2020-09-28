/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2020 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MT32EMU_STRUCTURES_H
#define MT32EMU_STRUCTURES_H

#include "globals.h"
#include "Types.h"

namespace MT32Emu {

// MT32EMU_MEMADDR() converts from sysex-padded, MT32EMU_SYSEXMEMADDR converts to it
// Roland provides documentation using the sysex-padded addresses, so we tend to use that in code and output
#define MT32EMU_MEMADDR(x) ((((x) & 0x7f0000) >> 2) | (((x) & 0x7f00) >> 1) | ((x) & 0x7f))
#define MT32EMU_SYSEXMEMADDR(x) ((((x) & 0x1FC000) << 2) | (((x) & 0x3F80) << 1) | ((x) & 0x7f))

#ifdef _MSC_VER
#define  MT32EMU_ALIGN_PACKED __declspec(align(1))
#else
#define MT32EMU_ALIGN_PACKED __attribute__((packed))
#endif

// The following structures represent the MT-32's memory
// Since sysex allows this memory to be written to in blocks of bytes,
// we keep this packed so that we can copy data into the various
// banks directly
#if defined(_MSC_VER) || defined(__MINGW32__)
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

struct TimbreParam {
	struct CommonParam {
		char name[10];
		uint8_t partialStructure12;  // 1 & 2  0-12 (1-13)
		uint8_t partialStructure34;  // 3 & 4  0-12 (1-13)
		uint8_t partialMute;  // 0-15 (0000-1111)
		uint8_t noSustain; // ENV MODE 0-1 (Normal, No sustain)
	} MT32EMU_ALIGN_PACKED common;

	struct PartialParam {
		struct WGParam {
			uint8_t pitchCoarse;  // 0-96 (C1,C#1-C9)
			uint8_t pitchFine;  // 0-100 (-50 to +50 (cents - confirmed by Mok))
			uint8_t pitchKeyfollow;  // 0-16 (-1, -1/2, -1/4, 0, 1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1, 5/4, 3/2, 2, s1, s2)
			uint8_t pitchBenderEnabled;  // 0-1 (OFF, ON)
			uint8_t waveform; // MT-32: 0-1 (SQU/SAW); LAPC-I: WG WAVEFORM/PCM BANK 0 - 3 (SQU/1, SAW/1, SQU/2, SAW/2)
			uint8_t pcmWave; // 0-127 (1-128)
			uint8_t pulseWidth; // 0-100
			uint8_t pulseWidthVeloSensitivity; // 0-14 (-7 - +7)
		} MT32EMU_ALIGN_PACKED wg;

		struct PitchEnvParam {
			uint8_t depth; // 0-10
			uint8_t veloSensitivity; // 0-100
			uint8_t timeKeyfollow; // 0-4
			uint8_t time[4]; // 0-100
			uint8_t level[5]; // 0-100 (-50 - +50) // [3]: SUSTAIN LEVEL, [4]: END LEVEL
		} MT32EMU_ALIGN_PACKED pitchEnv;

		struct PitchLFOParam {
			uint8_t rate; // 0-100
			uint8_t depth; // 0-100
			uint8_t modSensitivity; // 0-100
		} MT32EMU_ALIGN_PACKED pitchLFO;

		struct TVFParam {
			uint8_t cutoff; // 0-100
			uint8_t resonance; // 0-30
			uint8_t keyfollow; // -1, -1/2, -1/4, 0, 1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1, 5/4, 3/2, 2
			uint8_t biasPoint; // 0-127 (<1A-<7C >1A-7C)
			uint8_t biasLevel; // 0-14 (-7 - +7)
			uint8_t envDepth; // 0-100
			uint8_t envVeloSensitivity; // 0-100
			uint8_t envDepthKeyfollow; // DEPTH KEY FOLL0W 0-4
			uint8_t envTimeKeyfollow; // TIME KEY FOLLOW 0-4
			uint8_t envTime[5]; // 0-100
			uint8_t envLevel[4]; // 0-100 // [3]: SUSTAIN LEVEL
		} MT32EMU_ALIGN_PACKED tvf;

		struct TVAParam {
			uint8_t level; // 0-100
			uint8_t veloSensitivity; // 0-100
			uint8_t biasPoint1; // 0-127 (<1A-<7C >1A-7C)
			uint8_t biasLevel1; // 0-12 (-12 - 0)
			uint8_t biasPoint2; // 0-127 (<1A-<7C >1A-7C)
			uint8_t biasLevel2; // 0-12 (-12 - 0)
			uint8_t envTimeKeyfollow; // TIME KEY FOLLOW 0-4
			uint8_t envTimeVeloSensitivity; // VELOS KEY FOLL0W 0-4
			uint8_t envTime[5]; // 0-100
			uint8_t envLevel[4]; // 0-100 // [3]: SUSTAIN LEVEL
		} MT32EMU_ALIGN_PACKED tva;
	} MT32EMU_ALIGN_PACKED partial[4]; // struct PartialParam
} MT32EMU_ALIGN_PACKED; // struct TimbreParam

struct PatchParam {
	uint8_t timbreGroup; // TIMBRE GROUP  0-3 (group A, group B, Memory, Rhythm)
	uint8_t timbreNum; // TIMBRE NUMBER 0-63
	uint8_t keyShift; // KEY SHIFT 0-48 (-24 - +24 semitones)
	uint8_t fineTune; // FINE TUNE 0-100 (-50 - +50 cents)
	uint8_t benderRange; // BENDER RANGE 0-24
	uint8_t assignMode;  // ASSIGN MODE 0-3 (POLY1, POLY2, POLY3, POLY4)
	uint8_t reverbSwitch;  // REVERB SWITCH 0-1 (OFF,ON)
	uint8_t dummy; // (DUMMY)
} MT32EMU_ALIGN_PACKED;

const unsigned int SYSTEM_MASTER_TUNE_OFF = 0;
const unsigned int SYSTEM_REVERB_MODE_OFF = 1;
const unsigned int SYSTEM_REVERB_TIME_OFF = 2;
const unsigned int SYSTEM_REVERB_LEVEL_OFF = 3;
const unsigned int SYSTEM_RESERVE_SETTINGS_START_OFF = 4;
const unsigned int SYSTEM_RESERVE_SETTINGS_END_OFF = 12;
const unsigned int SYSTEM_CHAN_ASSIGN_START_OFF = 13;
const unsigned int SYSTEM_CHAN_ASSIGN_END_OFF = 21;
const unsigned int SYSTEM_MASTER_VOL_OFF = 22;

struct MemParams {
	// NOTE: The MT-32 documentation only specifies PatchTemp areas for parts 1-8.
	// The LAPC-I documentation specified an additional area for rhythm at the end,
	// where all parameters but fine tune, assign mode and output level are ignored
	struct PatchTemp {
		PatchParam patch;
		uint8_t outputLevel; // OUTPUT LEVEL 0-100
		uint8_t panpot; // PANPOT 0-14 (R-L)
		uint8_t dummyv[6];
	} MT32EMU_ALIGN_PACKED patchTemp[9];

	struct RhythmTemp {
		uint8_t timbre; // TIMBRE  0-94 (M1-M64,R1-30,OFF); LAPC-I: 0-127 (M01-M64,R01-R63)
		uint8_t outputLevel; // OUTPUT LEVEL 0-100
		uint8_t panpot; // PANPOT 0-14 (R-L)
		uint8_t reverbSwitch;  // REVERB SWITCH 0-1 (OFF,ON)
	} MT32EMU_ALIGN_PACKED rhythmTemp[85];

	TimbreParam timbreTemp[8];

	PatchParam patches[128];

	// NOTE: There are only 30 timbres in the "rhythm" bank for MT-32; the additional 34 are for LAPC-I and above
	struct PaddedTimbre {
		TimbreParam timbre;
		uint8_t padding[10];
	} MT32EMU_ALIGN_PACKED timbres[64 + 64 + 64 + 64]; // Group A, Group B, Memory, Rhythm

	struct System {
		uint8_t masterTune; // MASTER TUNE 0-127 432.1-457.6Hz
		uint8_t reverbMode; // REVERB MODE 0-3 (room, hall, plate, tap delay)
		uint8_t reverbTime; // REVERB TIME 0-7 (1-8)
		uint8_t reverbLevel; // REVERB LEVEL 0-7 (1-8)
		uint8_t reserveSettings[9]; // PARTIAL RESERVE (PART 1) 0-32
		uint8_t chanAssign[9]; // MIDI CHANNEL (PART1) 0-16 (1-16,OFF)
		uint8_t masterVol; // MASTER VOLUME 0-100
	} MT32EMU_ALIGN_PACKED system;
}; // struct MemParams

struct SoundGroup {
	uint8_t timbreNumberTableAddrLow;
	uint8_t timbreNumberTableAddrHigh;
	uint8_t displayPosition;
	uint8_t name[9];
	uint8_t timbreCount;
	uint8_t pad;
} MT32EMU_ALIGN_PACKED;

#if defined(_MSC_VER) || defined(__MINGW32__)
#pragma pack(pop)
#else
#pragma pack()
#endif

struct ControlROMFeatureSet {
	unsigned int quirkBasePitchOverflow : 1;
	unsigned int quirkPitchEnvelopeOverflow : 1;
	unsigned int quirkRingModulationNoMix : 1;
	unsigned int quirkTVAZeroEnvLevels : 1;
	unsigned int quirkPanMult : 1;
	unsigned int quirkKeyShift : 1;
	unsigned int quirkTVFBaseCutoffLimit : 1;

	// Features below don't actually depend on control ROM version, which is used to identify hardware model
	unsigned int defaultReverbMT32Compatible : 1;
	unsigned int oldMT32AnalogLPF : 1;
};

struct ControlROMMap {
	const char *shortName;
	const ControlROMFeatureSet &featureSet;
	uint16_t pcmTable; // 4 * pcmCount bytes
	uint16_t pcmCount;
	uint16_t timbreAMap; // 128 bytes
	uint16_t timbreAOffset;
	bool timbreACompressed;
	uint16_t timbreBMap; // 128 bytes
	uint16_t timbreBOffset;
	bool timbreBCompressed;
	uint16_t timbreRMap; // 2 * timbreRCount bytes
	uint16_t timbreRCount;
	uint16_t rhythmSettings; // 4 * rhythmSettingsCount bytes
	uint16_t rhythmSettingsCount;
	uint16_t reserveSettings; // 9 bytes
	uint16_t panSettings; // 8 bytes
	uint16_t programSettings; // 8 bytes
	uint16_t rhythmMaxTable; // 4 bytes
	uint16_t patchMaxTable; // 16 bytes
	uint16_t systemMaxTable; // 23 bytes
	uint16_t timbreMaxTable; // 72 bytes
	uint16_t soundGroupsTable; // 14 bytes each entry
	uint16_t soundGroupsCount;
};

struct ControlROMPCMStruct {
	uint8_t pos;
	uint8_t len;
	uint8_t pitchLSB;
	uint8_t pitchMSB;
};

struct PCMWaveEntry {
	Bit32u addr;
	Bit32u len;
	bool loop;
	ControlROMPCMStruct *controlROMPCMStruct;
};

// This is basically a per-partial, pre-processed combination of timbre and patch/rhythm settings
struct PatchCache {
	bool playPartial;
	bool PCMPartial;
	int pcm;
	uint8_t waveform;

	Bit32u structureMix;
	int structurePosition;
	int structurePair;

	// The following fields are actually common to all partials in the timbre
	bool dirty;
	Bit32u partialCount;
	bool sustain;
	bool reverb;

	TimbreParam::PartialParam srcPartial;

	// The following directly points into live sysex-addressable memory
	const TimbreParam::PartialParam *partialParam;
};

} // namespace MT32Emu

#endif // #ifndef MT32EMU_STRUCTURES_H
