/*
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

#include <CoreMIDI/MIDIServices.h>
#include <sstream>
#include <string>

class MidiHandler_coremidi : public MidiHandler {
private:
	MIDIPortRef m_port;
	MIDIClientRef m_client;
	MIDIEndpointRef m_endpoint;
	MIDIPacket* m_pCurPacket;
public:
	MidiHandler_coremidi()  {m_pCurPacket = 0;}
	const char * GetName(void) { return "coremidi"; }
	bool Open(const char * conf) {	
		// Get the MIDIEndPoint
		m_endpoint = 0;
//		OSStatus result;
		Bitu numDests = MIDIGetNumberOfDestinations();
		Bitu destId = numDests;
		if(conf && *conf) {
			std::string strconf(conf);
			std::istringstream configmidi(strconf);
			configmidi >> destId;
			if (configmidi.fail() && numDests) {
				lowcase(strconf);
				for(Bitu i = 0; i<numDests; i++) {
					MIDIEndpointRef dummy = MIDIGetDestination(i);
					if (!dummy) continue;
					CFStringRef midiname = 0;
					if (MIDIObjectGetStringProperty(dummy,kMIDIPropertyDisplayName,&midiname) == noErr) {
						const char* s = CFStringGetCStringPtr(midiname,kCFStringEncodingMacRoman);
						if (s) {
							std::string devname(s);
							lowcase(devname);
							if (devname.find(strconf) != std::string::npos) { 
								destId = i;
								break;
							}
						}
					}
				}
			}
		}
		if (destId >= numDests) destId = 0;

		if (destId < numDests)
		{
			m_endpoint = MIDIGetDestination(destId);
		}

		// Create a MIDI client and port
		MIDIClientCreate(CFSTR("MyClient"), 0, 0, &m_client);

		if (!m_client)
		{
			LOG_MSG("MIDI:coremidi: No client created.");
			return false;
		}

		MIDIOutputPortCreate(m_client, CFSTR("MyOutPort"), &m_port);

		if (!m_port)
		{
			LOG_MSG("MIDI:coremidi: No port created.");
			return false;
		}

		
		return true;
	}
	
	void Close(void) {
		// Dispose the port
		MIDIPortDispose(m_port);

		// Dispose the client
		MIDIClientDispose(m_client);

		// Dispose the endpoint
		// Not, as it is for Endpoints created by us
//		MIDIEndpointDispose(m_endpoint);
	}
	
	void PlayMsg(uint8_t * msg) {
		// Acquire a MIDIPacketList
		Byte packetBuf[128];
		MIDIPacketList *packetList = (MIDIPacketList *)packetBuf;
		m_pCurPacket = MIDIPacketListInit(packetList);
		
		// Determine the length of msg
		Bitu len=MIDI_evt_len[*msg];
		
		// Add msg to the MIDIPacketList
		MIDIPacketListAdd(packetList, (ByteCount)sizeof(packetBuf), m_pCurPacket, (MIDITimeStamp)0, len, msg);
		
		// Send the MIDIPacketList
		MIDISend(m_port,m_endpoint,packetList);
	}
	
	void PlaySysex(uint8_t * sysex, Bitu len) {
		// Acquire a MIDIPacketList
		Byte packetBuf[SYSEX_SIZE*4];
//		Bitu pos=0;
		MIDIPacketList *packetList = (MIDIPacketList *)packetBuf;
		m_pCurPacket = MIDIPacketListInit(packetList);
		
		// Add msg to the MIDIPacketList
		MIDIPacketListAdd(packetList, (ByteCount)sizeof(packetBuf), m_pCurPacket, (MIDITimeStamp)0, len, sysex);
		
		// Send the MIDIPacketList
		MIDISend(m_port,m_endpoint,packetList);
	}
	
	void ListAll(Program* base) {
		Bitu numDests = MIDIGetNumberOfDestinations();
		for(Bitu i = 0; i < numDests; i++){
			MIDIEndpointRef dest = MIDIGetDestination(i);
			if (!dest) continue;
			CFStringRef midiname = 0;
			if(MIDIObjectGetStringProperty(dest, kMIDIPropertyDisplayName, &midiname) == noErr) {
				const char * s = CFStringGetCStringPtr(midiname, kCFStringEncodingMacRoman);
				if (s) base->WriteOut("  %02d - %s\n",i,s);
			}
			//This is for EndPoints created by us.
			//MIDIEndpointDispose(dest);
		}

	}
};

MidiHandler_coremidi Midi_coremidi;

