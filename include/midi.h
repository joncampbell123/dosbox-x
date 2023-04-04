/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#ifndef DOSBOX_MIDI_H
#define DOSBOX_MIDI_H

#include "programs.h"

class MidiHandler {
public:
	MidiHandler();
	virtual bool Open(const char * /*conf*/) { return true; };
	virtual void Close(void) {};
	virtual void PlayMsg(uint8_t * /*msg*/) {};
	virtual void PlaySysex(uint8_t * /*sysex*/,Bitu /*len*/) {};
	virtual const char * GetName(void) { return "none"; };
	virtual void ListAll(Program * /*base*/) {};
	virtual ~MidiHandler() { };
	MidiHandler * next;
};


#define SYSEX_SIZE 8192
struct DB_Midi {
	Bitu status = 0;
	Bitu cmd_len = 0;
	Bitu cmd_pos = 0;
    uint8_t cmd_buf[8] = {};
    uint8_t rt_buf[8] = {};
	struct midi_state_sysex_t {
        uint8_t buf[SYSEX_SIZE] = {};
		Bitu used = 0;
		Bitu delay = 0;
		uint32_t start = 0;

		midi_state_sysex_t() {}
	} sysex;
	bool available = false;
	MidiHandler * handler = NULL;

	DB_Midi() {}
};

extern DB_Midi midi;

#endif
