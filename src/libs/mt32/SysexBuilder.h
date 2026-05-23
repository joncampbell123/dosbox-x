/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2026 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#ifndef MT32EMU_SYSEX_BUILDER_H
#define MT32EMU_SYSEX_BUILDER_H

#include "globals.h"
#include "Types.h"

namespace MT32Emu {

/**
 * Facilitates creation of a bank of MT-32 compatible SysEx messages.
 */
class SysexBuilder {
	Bit8u *writePtr;
	const Bit8u * const endPtr;

public:
	// Initialises the builder to fill SysEx messages into sysexBank of the given size.
	SysexBuilder(Bit8u *sysexBank, Bit32u size) : writePtr(sysexBank), endPtr(sysexBank + size) {}

	// Wraps the provided data array to be written to sysexAddress into a SysEx message with the header and trailer and appends it
	// to the SysEx bank if there is enough free space within.
	bool appendSysex(Bit32u sysexAddress, const Bit8u *data, Bit32u dataLength);
};

} // namespace MT32Emu

#endif // #ifndef MT32EMU_SYSEX_BUILDER_H
