/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2021 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#ifndef MT32EMU_MEMORY_REGION_H
#define MT32EMU_MEMORY_REGION_H

#include <cstddef>

#include "globals.h"
#include "Types.h"
#include "Structures.h"

namespace MT32Emu {

enum MemoryRegionType {
	MR_PatchTemp, MR_RhythmTemp, MR_TimbreTemp, MR_Patches, MR_Timbres, MR_System, MR_Display, MR_Reset
};

class Synth;

class MemoryRegion {
private:
	Synth *synth;
	uint8_t *realMemory;
	uint8_t *maxTable;
public:
	MemoryRegionType type;
	uint32_t startAddr, entrySize, entries;

	MemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable, MemoryRegionType useType, uint32_t useStartAddr, uint32_t useEntrySize, uint32_t useEntries) {
		synth = useSynth;
		realMemory = useRealMemory;
		maxTable = useMaxTable;
		type = useType;
		startAddr = useStartAddr;
		entrySize = useEntrySize;
		entries = useEntries;
	}
	int lastTouched(uint32_t addr, uint32_t len) const {
		return (offset(addr) + len - 1) / entrySize;
	}
	int firstTouchedOffset(uint32_t addr) const {
		return offset(addr) % entrySize;
	}
	int firstTouched(uint32_t addr) const {
		return offset(addr) / entrySize;
	}
	uint32_t regionEnd() const {
		return startAddr + entrySize * entries;
	}
	bool contains(uint32_t addr) const {
		return addr >= startAddr && addr < regionEnd();
	}
	int offset(uint32_t addr) const {
		return addr - startAddr;
	}
	uint32_t getClampedLen(uint32_t addr, uint32_t len) const {
		if (addr + len > regionEnd())
			return regionEnd() - addr;
		return len;
	}
	uint32_t next(uint32_t addr, uint32_t len) const {
		if (addr + len > regionEnd()) {
			return regionEnd() - addr;
		}
		return 0;
	}
	uint8_t getMaxValue(int off) const {
		if (maxTable == NULL)
			return 0xFF;
		return maxTable[off % entrySize];
	}
	uint8_t *getRealMemory() const {
		return realMemory;
	}
	bool isReadable() const {
		return getRealMemory() != NULL;
	}
	void read(unsigned int entry, unsigned int off, uint8_t *dst, unsigned int len) const;
	void write(unsigned int entry, unsigned int off, const uint8_t *src, unsigned int len, bool init = false) const;
}; // class MemoryRegion

class PatchTempMemoryRegion : public MemoryRegion {
public:
	PatchTempMemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable) : MemoryRegion(useSynth, useRealMemory, useMaxTable, MR_PatchTemp, MT32EMU_MEMADDR(0x030000), sizeof(MemParams::PatchTemp), 9) {}
};
class RhythmTempMemoryRegion : public MemoryRegion {
public:
	RhythmTempMemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable) : MemoryRegion(useSynth, useRealMemory, useMaxTable, MR_RhythmTemp, MT32EMU_MEMADDR(0x030110), sizeof(MemParams::RhythmTemp), 85) {}
};
class TimbreTempMemoryRegion : public MemoryRegion {
public:
	TimbreTempMemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable) : MemoryRegion(useSynth, useRealMemory, useMaxTable, MR_TimbreTemp, MT32EMU_MEMADDR(0x040000), sizeof(TimbreParam), 8) {}
};
class PatchesMemoryRegion : public MemoryRegion {
public:
	PatchesMemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable) : MemoryRegion(useSynth, useRealMemory, useMaxTable, MR_Patches, MT32EMU_MEMADDR(0x050000), sizeof(PatchParam), 128) {}
};
class TimbresMemoryRegion : public MemoryRegion {
public:
	TimbresMemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable) : MemoryRegion(useSynth, useRealMemory, useMaxTable, MR_Timbres, MT32EMU_MEMADDR(0x080000), sizeof(MemParams::PaddedTimbre), 64 + 64 + 64 + 64) {}
};
class SystemMemoryRegion : public MemoryRegion {
public:
	SystemMemoryRegion(Synth *useSynth, uint8_t *useRealMemory, uint8_t *useMaxTable) : MemoryRegion(useSynth, useRealMemory, useMaxTable, MR_System, MT32EMU_MEMADDR(0x100000), sizeof(MemParams::System), 1) {}
};
class DisplayMemoryRegion : public MemoryRegion {
public:
	DisplayMemoryRegion(Synth *useSynth) : MemoryRegion(useSynth, NULL, NULL, MR_Display, MT32EMU_MEMADDR(0x200000), SYSEX_BUFFER_SIZE - 1, 1) {}
};
class ResetMemoryRegion : public MemoryRegion {
public:
	ResetMemoryRegion(Synth *useSynth) : MemoryRegion(useSynth, NULL, NULL, MR_Reset, MT32EMU_MEMADDR(0x7F0000), 0x3FFF, 1) {}
};

} // namespace MT32Emu

#endif // #ifndef MT32EMU_MEMORY_REGION_H
