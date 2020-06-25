/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <sstream>

#define WIN32_MIDI_PORT_PROTECT 1

class MidiHandler_win32: public MidiHandler {
private:
	HMIDIOUT m_out = NULL;
    MIDIHDR m_hdr = {};
	HANDLE m_event = NULL;
	bool isOpen;

#if WIN32_MIDI_PORT_PROTECT
	HINSTANCE hMidiHelper = NULL;
	bool midi_dll = false;
	bool midi_dll_active = false;

	void MidiHelper_Reset()
	{
		midi_dll = false;
		midi_dll_active = false;


		// force unloading of old app - releases VSC lock
		hMidiHelper = LoadLibrary( "midi_helper.dll" );
		if( hMidiHelper ) {
			midi_dll = true;
			while( FreeLibrary( hMidiHelper ) != 0 ) Sleep(1);
		}
	}

	int MidiHelper_Start( DWORD devID )
	{
		HMIDIOUT (*func_ptr)(DWORD);

		hMidiHelper = LoadLibrary( "midi_helper.dll" );
		if( !hMidiHelper ) return -1;

    func_ptr = (HMIDIOUT(*)(DWORD))GetProcAddress( hMidiHelper,"MIDIHelper_OpenMidiOut" );
		if (!func_ptr ) return -1;

		m_out = func_ptr( devID );
		if( m_out == 0 ) return -1;


		midi_dll_active = true;
		return 0;
	}

	void MidiHelper_End()
	{
		void (*func_ptr)(void);


    func_ptr = (void(*)(void))GetProcAddress( hMidiHelper,"MIDIHelper_CloseMidiOut" );
 		if (!func_ptr ) return;

		func_ptr();
	}
#endif

public:
	MidiHandler_win32() : MidiHandler(),isOpen(false) {};
	const char * GetName(void) { return "win32";};

	bool Open(const char * conf) {
		MIDIOUTCAPS mididev;
		if (isOpen) return false;


#if WIN32_MIDI_PORT_PROTECT
		// VSC crash protection
		MidiHelper_Reset();
#endif


		m_event = CreateEvent (NULL, true, true, NULL);
		MMRESULT res = MMSYSERR_NOERROR;
		if(conf && *conf) {
			std::string strconf(conf);
			std::istringstream configmidi(strconf);
			unsigned int total = midiOutGetNumDevs();
			unsigned int nummer = total;
			configmidi >> nummer;
			if (configmidi.fail() && total) {
				lowcase(strconf);
				for(unsigned int i = 0; i< total;i++) {
					midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
					std::string devname(mididev.szPname);
					lowcase(devname);
					if (devname.find(strconf) != std::string::npos) {
						nummer = i;
						break;
					}
				}
			}

			if (nummer < total) {
				midiOutGetDevCaps(nummer, &mididev, sizeof(MIDIOUTCAPS));
				LOG_MSG("MIDI:win32 selected %s",mididev.szPname);


#if WIN32_MIDI_PORT_PROTECT
				if( midi_dll == false || strcmp( mididev.szPname, "Roland VSC" ) != 0 )
					res = midiOutOpen(&m_out, nummer, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
				else {
					// Roland VSC - crash protection
					res = MidiHelper_Start(nummer);
				}
#else
				res = midiOutOpen(&m_out, nummer, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
#endif


				if( res != MMSYSERR_NOERROR ) {
					if( strcmp( mididev.szPname, "Roland VSC" ) == 0 ) MessageBox( 0, "Roland VSC failed", "MIDI", MB_OK | MB_TOPMOST );

					if( nummer != 0 ) {
						LOG_MSG("MIDI:win32 selected %s","default");
						res = midiOutOpen(&m_out, MIDI_MAPPER, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
					}
				}
			}
		} else {
			res = midiOutOpen(&m_out, MIDI_MAPPER, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
		}
		if (res != MMSYSERR_NOERROR) return false;

		Reset();

		isOpen=true;
		return true;
	};

	void Close(void) {
		if (!isOpen) return;
		isOpen=false;


#if WIN32_MIDI_PORT_PROTECT
		if( midi_dll ) MidiHelper_End();
#endif

		// flush buffers, then shutdown
		midiOutReset(m_out);
		midiOutClose(m_out);

		CloseHandle (m_event);
	};

	void PlayMsg(Bit8u * msg) {
		midiOutShortMsg(m_out, *(Bit32u*)msg);
	};

	void PlaySysex(Bit8u * sysex,Bitu len) {
#if WIN32_MIDI_PORT_PROTECT
		if( midi_dll_active == false ) {
#endif
			if (WaitForSingleObject (m_event, 2000) == WAIT_TIMEOUT) {
				LOG(LOG_MISC,LOG_ERROR)("Can't send midi message");
				return;
			}
#if WIN32_MIDI_PORT_PROTECT
		}
#endif

		midiOutUnprepareHeader (m_out, &m_hdr, sizeof (m_hdr));

		m_hdr.lpData = (char *) sysex;
		m_hdr.dwBufferLength = (DWORD)len;
		m_hdr.dwBytesRecorded = (DWORD)len;
		m_hdr.dwUser = 0;

		MMRESULT result = midiOutPrepareHeader (m_out, &m_hdr, sizeof (m_hdr));
		if (result != MMSYSERR_NOERROR) return;
		ResetEvent (m_event);
		result = midiOutLongMsg (m_out,&m_hdr,sizeof(m_hdr));
		if (result != MMSYSERR_NOERROR) {
			SetEvent (m_event);
			return;
		}

#if WIN32_MIDI_PORT_PROTECT
		if( midi_dll_active == true ) {
			while( midiOutUnprepareHeader (m_out, &m_hdr, sizeof (m_hdr)) != 0 )
				Sleep(1);
		}
#endif
	}
	
	void ListAll(Program* base) {
#if defined (WIN32)
		unsigned int total = midiOutGetNumDevs();	
		for(unsigned int i = 0;i < total;i++) {
			MIDIOUTCAPS mididev;
			midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
			base->WriteOut("%2d\t \"%s\"\n",i,mididev.szPname);
		}
#endif
	}

	void Reset()
	{
		Bit8u buf[64];

		// flush buffers
		midiOutReset(m_out);


		// GM1 reset
		buf[0] = 0xf0;
		buf[1] = 0x7e;
		buf[2] = 0x7f;
		buf[3] = 0x09;
		buf[4] = 0x01;
		buf[5] = 0xf7;
		PlaySysex( (Bit8u *) buf, 6 );


		// GS1 reset
		buf[0] = 0xf0;
		buf[1] = 0x41;
		buf[2] = 0x10;
		buf[3] = 0x42;
		buf[4] = 0x12;
		buf[5] = 0x40;
		buf[6] = 0x00;
		buf[7] = 0x7f;
		buf[8] = 0x00;
		buf[9] = 0x41;
		buf[10] = 0xf7;
		PlaySysex( (Bit8u *) buf, 11 );
	}
};

MidiHandler_win32 Midi_win32; 
