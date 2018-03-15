/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2017 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#ifndef MT32EMU_MIDI_EVENT_QUEUE_H
#define MT32EMU_MIDI_EVENT_QUEUE_H

#include "globals.h"
#include "Types.h"

namespace MT32Emu {

/**
 * Used to safely store timestamped MIDI events in a local queue.
 */
struct MidiEvent {
	Bit32u shortMessageData;
	const Bit8u *sysexData;
	Bit32u sysexLength;
	Bit32u timestamp;

	~MidiEvent();
	void setShortMessage(Bit32u shortMessageData, Bit32u timestamp);
	void setSysex(const Bit8u *sysexData, Bit32u sysexLength, Bit32u timestamp);
};

/**
 * Simple queue implementation using a ring buffer to store incoming MIDI event before the synth actually processes it.
 * It is intended to:
 * - get rid of prerenderer while retaining graceful partial abortion
 * - add fair emulation of the MIDI interface delays
 * - extend the synth interface with the default implementation of a typical rendering loop.
 * THREAD SAFETY:
 * It is safe to use either in a single thread environment or when there are only two threads - one performs only reading
 * and one performs only writing. More complicated usage requires external synchronisation.
 */
class MidiEventQueue {
private:
	MidiEvent * const ringBuffer;
	const Bit32u ringBufferMask;
	volatile Bit32u startPosition;
	volatile Bit32u endPosition;

public:
	MidiEventQueue(Bit32u ringBufferSize = DEFAULT_MIDI_EVENT_QUEUE_SIZE); // Must be a power of 2
	~MidiEventQueue();
	void reset();
	bool pushShortMessage(Bit32u shortMessageData, Bit32u timestamp);
	bool pushSysex(const Bit8u *sysexData, Bit32u sysexLength, Bit32u timestamp);
	const MidiEvent *peekMidiEvent();
	void dropMidiEvent();
	bool isFull() const;
	bool inline isEmpty() const;
};

} // namespace MT32Emu

#endif // #ifndef MT32EMU_MIDI_EVENT_QUEUE_H
