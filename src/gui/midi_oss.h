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

#include <fcntl.h>
#define SEQ_MIDIPUTC    5

class MidiHandler_oss: public MidiHandler {
private:
	int  device;
	uint8_t device_num;
	bool isOpen;
public:
	MidiHandler_oss() : MidiHandler(),isOpen(false) {};
	const char * GetName(void) { return "oss";};
	bool Open(const char * conf) {
		char devname[512];
		if (conf && conf[0]) safe_strncpy(devname,conf,512);
		else strcpy(devname,"/dev/sequencer");
		char * devfind=(strrchr(devname,','));
		if (devfind) {
			*devfind++=0;
			device_num=atoi(devfind);
		} else device_num=0;
		if (isOpen) return false;
		device=open(devname, O_WRONLY, 0);
		if (device<0) return false;
		return true;
	};
	void Close(void) {
		if (!isOpen) return;
		if (device>0) close(device);
	};
	void PlayMsg(uint8_t * msg) {
		uint8_t buf[128];Bitu pos=0;
		Bitu len=MIDI_evt_len[*msg];
		for (;len>0;len--) {
			buf[pos++] = SEQ_MIDIPUTC;
			buf[pos++] = *msg;
			buf[pos++] = device_num;
			buf[pos++] = 0;
			msg++;
		}
        ssize_t writeResult = write(device, buf, pos);
        if (writeResult == -1) {
            LOG(LOG_IO, LOG_ERROR) ("Writing error in PlayMsg\n");
        }
	};
	void PlaySysex(uint8_t * sysex,Bitu len) {
		uint8_t buf[SYSEX_SIZE*4];Bitu pos=0;
		for (;len>0;len--) {
			buf[pos++] = SEQ_MIDIPUTC;
			buf[pos++] = *sysex++;
			buf[pos++] = device_num;
			buf[pos++] = 0;
		}
        ssize_t writeResult = write(device, buf, pos);
        if (writeResult == -1) {
            LOG(LOG_IO, LOG_ERROR) ("Writing error in PlaySysex\n");
        }
	}
};

MidiHandler_oss Midi_oss;




