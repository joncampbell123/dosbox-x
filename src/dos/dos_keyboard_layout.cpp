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


#include "dosbox.h"
#include "bios.h"
#include "bios_disk.h"
#include "setup.h"
#include "support.h"
#include "../ints/int10.h"
#include "regs.h"
#include "callback.h"
#include "mapper.h"
#include "drives.h"
#include "dos_inc.h"
#include "control.h"
#include "render.h"
#include "dos_codepages.h"
#include "dos_keyboard_layout_data.h"

#if defined (WIN32)
#include <windows.h>
#endif

void DOSBox_SetSysMenu(void);
#if defined(USE_TTF)
int setTTFCodePage(void);
bool TTF_using(void);
#endif
static FILE* OpenDosboxFile(const char* name) {
	uint8_t drive;
	char fullname[DOS_PATHLENGTH];

	localDrive* ldp=0;
	// try to build dos name
	if (DOS_MakeName(name,fullname,&drive)) {
		try {
			// try to open file on mounted drive first
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if (ldp) {
				FILE *tmpfile=ldp->GetSystemFilePtr(fullname, "rb");
				if (tmpfile != NULL) return tmpfile;
			}
		}
		catch(...) {}
	}
	FILE *tmpfile=fopen(name, "rb");
	return tmpfile;
}


class keyboard_layout {
public:
	keyboard_layout() {
		this->reset();
		language_codes=NULL;
		use_foreign_layout=false;
		sprintf(current_keyboard_file_name, "none");
	};

	~keyboard_layout();

	// read in a codepage from a .cpi-file
	Bitu read_codepage_file(const char* codepage_file_name, int32_t codepage_id);
	uint16_t extract_codepage(const char* keyboard_file_name);
	// read in a keyboard layout from a .kl-file
	Bitu read_keyboard_file(const char* keyboard_file_name, int32_t req_cp);

	// call layout_key to apply the current language layout
	bool layout_key(Bitu key, uint8_t flags1, uint8_t flags2, uint8_t flags3);

	Bitu switch_keyboard_layout(const char* new_layout, keyboard_layout* &created_layout, int32_t& tried_cp);
	void switch_foreign_layout();
	const char* get_layout_name();
	const char* main_language_code();


private:
	static const uint8_t layout_pages=12;
	uint16_t current_layout[(MAX_SCAN_CODE+1)*layout_pages];
	struct {
		uint16_t required_flags,forbidden_flags;
		uint16_t required_userflags,forbidden_userflags;
	} current_layout_planes[layout_pages-4];
    uint8_t additional_planes = 0;
    uint8_t used_lock_modifiers;

	// diacritics table
    uint8_t diacritics[2048] = {};
	uint16_t diacritics_entries;
	uint16_t diacritics_character;
	uint16_t user_keys;

	char current_keyboard_file_name[256];
	bool use_foreign_layout;

	// language code storage used when switching layouts
	char** language_codes;
	Bitu language_code_count;

	void reset();
	void read_keyboard_file(int32_t specific_layout);
	Bitu read_keyboard_file(const char* keyboard_file_name, int32_t specific_layout, int32_t requested_codepage);
	bool map_key(Bitu key, uint16_t layouted_key, bool is_command, bool is_keypair);
};


keyboard_layout::~keyboard_layout() {
	if (language_codes) {
		for (Bitu i=0; i<language_code_count; i++)
			delete[] language_codes[i];
		delete[] language_codes;
		language_codes=NULL;
	}
}

void keyboard_layout::reset() {
	for (uint32_t i=0; i<(MAX_SCAN_CODE+1)*layout_pages; i++) current_layout[i]=0;
	for (uint32_t i=0; i<layout_pages-4; i++) {
		current_layout_planes[i].required_flags=0;
		current_layout_planes[i].forbidden_flags=0xffff;
		current_layout_planes[i].required_userflags=0;
		current_layout_planes[i].forbidden_userflags=0xffff;
	}
	used_lock_modifiers=0x0f;
	diacritics_entries=0;		// no diacritics loaded
	diacritics_character=0;
	user_keys=0;				// all userkeys off
	language_code_count=0;
}

Bitu keyboard_layout::read_keyboard_file(const char* keyboard_file_name, int32_t req_cp) {
	return this->read_keyboard_file(keyboard_file_name, -1, req_cp);
}

// switch to a different layout
void keyboard_layout::read_keyboard_file(int32_t specific_layout) {
	if (strcmp(current_keyboard_file_name,"none"))
		this->read_keyboard_file(current_keyboard_file_name, specific_layout, dos.loaded_codepage);
}

static uint32_t read_kcl_file(const char* kcl_file_name, const char* layout_id, bool first_id_only) {
	FILE* tempfile = OpenDosboxFile(kcl_file_name);
	if (tempfile==0) return 0;

	static uint8_t rbuf[8192];

	// check ID-bytes of file
	uint32_t dr=(uint32_t)fread(rbuf, sizeof(uint8_t), 7, tempfile);
	if ((dr<7) || (rbuf[0]!=0x4b) || (rbuf[1]!=0x43) || (rbuf[2]!=0x46)) {
		fclose(tempfile);
		return 0;
	}

	fseek(tempfile, 7+rbuf[6], SEEK_SET);

	for (;;) {
		uint32_t cur_pos=(uint32_t)(ftell(tempfile));
		dr=(uint32_t)fread(rbuf, sizeof(uint8_t), 5, tempfile);
		if (dr<5) break;
		uint16_t len=host_readw(&rbuf[0]);

		uint8_t data_len=rbuf[2];

		char lng_codes[258];
		fseek(tempfile, -2, SEEK_CUR);
		// get all language codes for this layout
		for (Bitu i=0; i<data_len;) {
            size_t readResult = fread(rbuf, sizeof(uint8_t), 2, tempfile);
            if (readResult != 2) {
                LOG(LOG_IO, LOG_ERROR) ("Reading error in read_kcl_file\n");
                return 0;
            }
			uint16_t lcnum=host_readw(&rbuf[0]);
			i+=2;
			Bitu lcpos=0;
			for (;i<data_len;) {
                readResult = fread(rbuf, sizeof(uint8_t), 1, tempfile);
                if (readResult != 1) {
                    LOG(LOG_IO, LOG_ERROR) ("Reading error in read_kcl_file\n");
                    return 0;
                }
				i++;
				if (((char)rbuf[0])==',') break;
				lng_codes[lcpos++]=(char)rbuf[0];
			}
			lng_codes[lcpos]=0;
			if (strcasecmp(lng_codes, layout_id)==0) {
				// language ID found in file, return file position
				fclose(tempfile);
				return cur_pos;
			}
			if (first_id_only) break;
			if (lcnum) {
				sprintf(&lng_codes[lcpos],"%d",lcnum);
				if (strcasecmp(lng_codes, layout_id)==0) {
					// language ID found in file, return file position
					return cur_pos;
				}
			}
		}
		fseek(tempfile, long(cur_pos+3+len), SEEK_SET);
	}

	fclose(tempfile);
	return 0;
}

static uint32_t read_kcl_data(const uint8_t* kcl_data, uint32_t kcl_data_size, const char* layout_id, bool first_id_only) {
	// check ID-bytes
	if ((kcl_data[0]!=0x4b) || (kcl_data[1]!=0x43) || (kcl_data[2]!=0x46)) {
		return 0;
	}

	uint32_t dpos=7u+kcl_data[6];

	for (;;) {
		if (dpos+5>kcl_data_size) break;
		uint32_t cur_pos=dpos;
		uint16_t len=host_readw(&kcl_data[dpos]);
		uint8_t data_len=kcl_data[dpos+2];
		dpos+=5;

		char lng_codes[258];
		// get all language codes for this layout
		for (Bitu i=0; i<data_len;) {
			uint16_t lcnum=host_readw(&kcl_data[dpos-2]);
			i+=2;
			Bitu lcpos=0;
			for (;i<data_len;) {
				if (dpos+1>kcl_data_size) break;
				char lc=(char)kcl_data[dpos];
				dpos++;
				i++;
				if (lc==',') break;
				lng_codes[lcpos++]=lc;
			}
			lng_codes[lcpos]=0;
			if (strcasecmp(lng_codes, layout_id)==0) {
				// language ID found, return position
				return cur_pos;
			}
			if (first_id_only) break;
			if (lcnum) {
				sprintf(&lng_codes[lcpos],"%d",lcnum);
				if (strcasecmp(lng_codes, layout_id)==0) {
					// language ID found, return position
					return cur_pos;
				}
			}
			dpos+=2;
		}
		dpos=cur_pos+3+len;
	}
	return 0;
}

Bitu keyboard_layout::read_keyboard_file(const char* keyboard_file_name, int32_t specific_layout, int32_t requested_codepage) {
	this->reset();

	if (specific_layout==-1) strcpy(current_keyboard_file_name, keyboard_file_name);
	if (!strcmp(keyboard_file_name,"none")) return KEYB_NOERROR;

	static uint8_t read_buf[65535];
	uint32_t read_buf_size, read_buf_pos, bytes_read;
	uint32_t start_pos=5;

	char nbuf[512];
	read_buf_size = 0;
	sprintf(nbuf, "%s.kl", keyboard_file_name);
	FILE* tempfile = OpenDosboxFile(nbuf);
	if (tempfile==NULL) {
		// try keyboard layout libraries next
		if ((start_pos=read_kcl_file("keyboard.sys",keyboard_file_name,true))) {
			tempfile = OpenDosboxFile("keyboard.sys");
		} else if ((start_pos=read_kcl_file("keybrd2.sys",keyboard_file_name,true))) {
			tempfile = OpenDosboxFile("keybrd2.sys");
		} else if ((start_pos=read_kcl_file("keybrd3.sys",keyboard_file_name,true))) {
			tempfile = OpenDosboxFile("keybrd3.sys");
		} else if ((start_pos=read_kcl_file("keyboard.sys",keyboard_file_name,false))) {
			tempfile = OpenDosboxFile("keyboard.sys");
		} else if ((start_pos=read_kcl_file("keybrd2.sys",keyboard_file_name,false))) {
			tempfile = OpenDosboxFile("keybrd2.sys");
		} else if ((start_pos=read_kcl_file("keybrd3.sys",keyboard_file_name,false))) {
			tempfile = OpenDosboxFile("keybrd3.sys");
		} else if ((start_pos=read_kcl_data(layout_keyboardsys,33196,keyboard_file_name,true))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<33196; ct++) read_buf[read_buf_size++]=layout_keyboardsys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd2sys,25431,keyboard_file_name,true))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<25431; ct++) read_buf[read_buf_size++]=layout_keybrd2sys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd3sys,27122,keyboard_file_name,true))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<27122; ct++) read_buf[read_buf_size++]=layout_keybrd3sys[ct];
		} else if ((start_pos=read_kcl_data(layout_keyboardsys,33196,keyboard_file_name,false))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<33196; ct++) read_buf[read_buf_size++]=layout_keyboardsys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd2sys,25431,keyboard_file_name,false))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<25431; ct++) read_buf[read_buf_size++]=layout_keybrd2sys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd3sys,27122,keyboard_file_name,false))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<27122; ct++) read_buf[read_buf_size++]=layout_keybrd3sys[ct];
		} else {
			LOG(LOG_BIOS,LOG_ERROR)("Keyboard layout file %s not found",keyboard_file_name);
			return KEYB_FILENOTFOUND;
		}
		if (tempfile) {
			fseek(tempfile, long(start_pos+2), SEEK_SET);
			read_buf_size=(uint32_t)fread(read_buf, sizeof(uint8_t), 65535, tempfile);
			fclose(tempfile);
		}
		start_pos=0;
	} else {
		// check ID-bytes of file
		uint32_t dr=(uint32_t)fread(read_buf, sizeof(uint8_t), 4, tempfile);
		if ((dr<4) || (read_buf[0]!=0x4b) || (read_buf[1]!=0x4c) || (read_buf[2]!=0x46)) {
			LOG(LOG_BIOS,LOG_ERROR)("Invalid keyboard layout file %s",keyboard_file_name);
			return KEYB_INVALIDFILE;
		}
		
		fseek(tempfile, 0, SEEK_SET);
		read_buf_size=(uint32_t)fread(read_buf, sizeof(uint8_t), 65535, tempfile);
		fclose(tempfile);
	}

	uint8_t data_len,submappings;
	data_len=read_buf[start_pos++];

	language_codes=new char*[data_len];
	language_code_count=0;
	// get all language codes for this layout
	for (Bitu i=0; i<data_len;) {
		language_codes[language_code_count]=new char[256];
		i+=2;
		Bitu lcpos=0;
		for (;i<data_len;) {
			char lcode=char(read_buf[start_pos+i]);
			i++;
			if (lcode==',') break;
			language_codes[language_code_count][lcpos++]=lcode;
		}
		language_codes[language_code_count][lcpos]=0;
		language_code_count++;
	}

	start_pos+=data_len;		// start_pos==absolute position of KeybCB block

	submappings=read_buf[start_pos];
	additional_planes=read_buf[start_pos+1];

	// four pages always occupied by normal,shift,flags,commandbits
	if (additional_planes>(layout_pages-4)) additional_planes=(layout_pages-4);

	// seek to plane descriptor
	read_buf_pos=start_pos+0x14u+submappings*8u;
	for (uint16_t cplane=0; cplane<additional_planes; cplane++) {
		uint16_t plane_flags;

		// get required-flags (shift/alt/ctrl-states etc.)
		plane_flags=host_readw(&read_buf[read_buf_pos]);
		read_buf_pos+=2;
		current_layout_planes[cplane].required_flags=plane_flags;
		used_lock_modifiers |= (plane_flags&0x70);
		// get forbidden-flags
		plane_flags=host_readw(&read_buf[read_buf_pos]);
		read_buf_pos+=2;
		current_layout_planes[cplane].forbidden_flags=plane_flags;

		// get required-userflags
		plane_flags=host_readw(&read_buf[read_buf_pos]);
		read_buf_pos+=2;
		current_layout_planes[cplane].required_userflags=plane_flags;
		// get forbidden-userflags
		plane_flags=host_readw(&read_buf[read_buf_pos]);
		read_buf_pos+=2;
		current_layout_planes[cplane].forbidden_userflags=plane_flags;
	}

	bool found_matching_layout=false;
	
	// check all submappings and use them if general submapping or same codepage submapping
	for (uint16_t sub_map=0; (sub_map<submappings) && (!found_matching_layout); sub_map++) {
		uint16_t submap_cp, table_offset;

		if ((sub_map!=0) && (specific_layout!=-1)) sub_map=(uint16_t)(specific_layout&0xffff);

		// read codepage of submapping
		submap_cp=host_readw(&read_buf[start_pos+0x14u+sub_map*8u]);
		if ((submap_cp!=0) && (submap_cp!=requested_codepage) && (specific_layout==-1))
			continue;		// skip nonfitting submappings

		if (submap_cp==requested_codepage) found_matching_layout=true;

		// get table offset
		table_offset=host_readw(&read_buf[start_pos+0x18u+sub_map*8u]);
		diacritics_entries=0;
		if (table_offset!=0) {
			// process table
			uint16_t i,j;
			for (i=0; i<2048;) {
				if (read_buf[start_pos+table_offset+i]==0) break;	// end of table
				diacritics_entries++;
				i+=read_buf[start_pos+table_offset+i+1]*2+2;
			}
			// copy diacritics table
			for (j=0; j<=i; j++) diacritics[j]=read_buf[start_pos+table_offset+j];
		}


		// get table offset
		table_offset=host_readw(&read_buf[start_pos+0x16u+sub_map*8u]);
		if (table_offset==0) continue;	// non-present table

		read_buf_pos=start_pos+table_offset;

		bytes_read=read_buf_size-read_buf_pos;

		// process submapping table
		for (uint32_t i=0; i<bytes_read;) {
			uint8_t scan=read_buf[read_buf_pos++];
			if (scan==0) break;
			uint8_t scan_length=(read_buf[read_buf_pos]&7)+1;		// length of data struct
			read_buf_pos+=2;
			i+=3;
			if (((scan&0x7f)<=MAX_SCAN_CODE) && (scan_length>0)) {
				// add all available mappings
				for (uint16_t addmap=0; addmap<scan_length; addmap++) {
					if (addmap>additional_planes+2) break;
					Bitu charptr=read_buf_pos+addmap*((read_buf[read_buf_pos-2u]&0x80u)?2u:1u);
					uint16_t kchar=read_buf[charptr];

					if (kchar!=0) {		// key remapped
						if (read_buf[read_buf_pos-2]&0x80) kchar|=read_buf[charptr+1]<<8;	// scancode/char pair
						// overwrite mapping
						current_layout[scan*layout_pages+addmap]=kchar;
						// clear command bit
						current_layout[scan*layout_pages+layout_pages-2]&=~(1<<addmap);
						// add command bit
						current_layout[scan*layout_pages+layout_pages-2]|=(read_buf[read_buf_pos-1] & (1<<addmap));
					}
				}

				// calculate max length of entries, taking into account old number of entries
				uint8_t new_flags=current_layout[scan*layout_pages+layout_pages-1]&0x7;
				if ((read_buf[read_buf_pos-2]&0x7) > new_flags) new_flags = read_buf[read_buf_pos-2]&0x7;

				// merge flag bits in as well
				new_flags |= (read_buf[read_buf_pos-2] | current_layout[scan*layout_pages+layout_pages-1]) & 0xf0;

				current_layout[scan*layout_pages+layout_pages-1]=new_flags;
				if (read_buf[read_buf_pos-2]&0x80) scan_length*=2;		// granularity flag (S)
			}
			i+=scan_length;		// advance pointer
			read_buf_pos+=scan_length;
		}
		if (specific_layout==sub_map) break;
	}

	if (found_matching_layout) {
		if (specific_layout==-1) LOG(LOG_BIOS,LOG_NORMAL)("Keyboard layout %s successfully loaded",keyboard_file_name);
		else LOG(LOG_BIOS,LOG_NORMAL)("Keyboard layout %s (%i) successfully loaded",keyboard_file_name,specific_layout);
		this->use_foreign_layout=true;
		return KEYB_NOERROR;
	}

	LOG(LOG_BIOS,LOG_ERROR)("No matching keyboard layout found in %s",keyboard_file_name);

	// reset layout data (might have been changed by general layout)
	this->reset();

	return KEYB_LAYOUTNOTFOUND;
}

bool keyboard_layout::layout_key(Bitu key, uint8_t flags1, uint8_t flags2, uint8_t flags3) {
	if (key>MAX_SCAN_CODE) return false;
	if (!this->use_foreign_layout) return false;

	bool is_special_pair=(current_layout[key*layout_pages+layout_pages-1] & 0x80)==0x80;

	if ((((flags1&used_lock_modifiers)&0x7c)==0) && ((flags3&2)==0)) {
		// check if shift/caps is active:
		// (left_shift OR right_shift) XOR (key_affected_by_caps AND caps_locked)
		if ((((flags1&2)>>1) | (flags1&1)) ^ (((current_layout[key*layout_pages+layout_pages-1] & 0x40) & (flags1 & 0x40))>>6)) {
			// shift plane
			if (current_layout[key*layout_pages+1]!=0) {
				// check if command-bit is set for shift plane
				bool is_command=(current_layout[key*layout_pages+layout_pages-2]&2)!=0;
				if (this->map_key(key, current_layout[key*layout_pages+1],
					is_command, is_special_pair)) return true;
			}
		} else {
			// normal plane
			if (current_layout[key*layout_pages]!=0) {
				// check if command-bit is set for normal plane
				bool is_command=(current_layout[key*layout_pages+layout_pages-2]&1)!=0;
				if (this->map_key(key, current_layout[key*layout_pages],
					is_command, is_special_pair)) return true;
			}
		}
	}

	// calculate current flags
	uint16_t current_flags=(flags1&0x7f) | (((flags2&3) | (flags3&0xc))<<8);
	if (flags1&3) current_flags|=0x4000;	// either shift key active
	if (flags3&2) current_flags|=0x1000;	// e0 prefixed

	// check all planes if flags fit
	for (uint16_t cplane=0; cplane<additional_planes; cplane++) {
		uint16_t req_flags=current_layout_planes[cplane].required_flags;
		uint16_t req_userflags=current_layout_planes[cplane].required_userflags;
		// test flags
		if (((current_flags & req_flags)==req_flags) &&
			((user_keys & req_userflags)==req_userflags) &&
			((current_flags & current_layout_planes[cplane].forbidden_flags)==0) &&
			((user_keys & current_layout_planes[cplane].forbidden_userflags)==0)) {
				// remap key
				if (current_layout[key*layout_pages+2+cplane]!=0) {
					// check if command-bit is set for this plane
					bool is_command=((current_layout[key*layout_pages+layout_pages-2]>>(cplane+2))&1)!=0;
					if (this->map_key(key, current_layout[key*layout_pages+2+cplane],
						is_command, is_special_pair)) return true;
				} else break;	// abort plane checking
			}
	}

	if (diacritics_character>0) {
		// ignore state-changing keys
		switch(key) {
			case 0x1d:			/* Ctrl Pressed */
			case 0x2a:			/* Left Shift Pressed */
			case 0x36:			/* Right Shift Pressed */
			case 0x38:			/* Alt Pressed */
			case 0x3a:			/* Caps Lock */
			case 0x45:			/* Num Lock */
			case 0x46:			/* Scroll Lock */
				break;
			default:
				if (diacritics_character-200>=diacritics_entries) {
					diacritics_character=0;
					return true;
				}
				uint16_t diacritics_start=0;
				// search start of subtable
				for (uint16_t i=0; i<diacritics_character-200; i++)
					diacritics_start+=diacritics[diacritics_start+1]*2+2;

				BIOS_AddKeyToBuffer((uint16_t)(key<<8) | diacritics[diacritics_start]);
				diacritics_character=0;
		}
	}

	return false;
}

bool keyboard_layout::map_key(Bitu key, uint16_t layouted_key, bool is_command, bool is_keypair) {
	if (is_command) {
		uint8_t key_command=(uint8_t)(layouted_key&0xff);
		// check if diacritics-command
		if ((key_command>=200) && (key_command<235)) {
			// diacritics command
			diacritics_character=key_command;
			if (diacritics_character-200>=diacritics_entries) diacritics_character=0;
			return true;
		} else if ((key_command>=120) && (key_command<140)) {
			// switch layout command
			this->read_keyboard_file(key_command-119);
			return true;
		} else if ((key_command>=180) && (key_command<188)) {
			// switch user key off
			user_keys&=~(1<<(key_command-180));
			return true;
		} else if ((key_command>=188) && (key_command<196)) {
			// switch user key on
			user_keys|=(1<<(key_command-188));
			return true;
		} else if (key_command==160) return true;	// nop command
	} else {
		// non-command
		if (diacritics_character>0) {
			if (diacritics_character-200>=diacritics_entries) diacritics_character = 0;
			else {
				uint16_t diacritics_start=0;
				// search start of subtable
				for (uint16_t i=0; i<diacritics_character-200; i++)
					diacritics_start+=diacritics[diacritics_start+1]*2+2;

				uint8_t diacritics_length=diacritics[diacritics_start+1];
				diacritics_start+=2;
				diacritics_character=0;	// reset

				// search scancode
				for (uint16_t i=0; i<diacritics_length; i++) {
					if (diacritics[diacritics_start+i*2]==(layouted_key&0xff)) {
						// add diacritics to keybuf
						if (diacritics[diacritics_start+i*2+1] == 0xe0 || diacritics[diacritics_start+i*2+1] == 0xf0)
							BIOS_AddKeyToBuffer(diacritics[diacritics_start+i*2+1]);
						else
							BIOS_AddKeyToBuffer((uint16_t)(key<<8) | diacritics[diacritics_start+i*2+1]);
						return true;
					}
				}
				// add standard-diacritics to keybuf
				BIOS_AddKeyToBuffer((uint16_t)(key<<8) | diacritics[diacritics_start-2]);
			}
		}

		// add remapped key to keybuf
		if (is_keypair) BIOS_AddKeyToBuffer(layouted_key);
		else BIOS_AddKeyToBuffer((uint16_t)(key<<8) | (layouted_key&0xff));

		return true;
	}
	return false;
}

uint16_t keyboard_layout::extract_codepage(const char* keyboard_file_name) {
	if (!strcmp(keyboard_file_name,"none")) return (IS_PC98_ARCH ? 932 : 437);

	uint32_t read_buf_size;
	static uint8_t read_buf[65535];
	uint32_t start_pos=5;

	char nbuf[512];
	sprintf(nbuf, "%s.kl", keyboard_file_name);
	FILE* tempfile = OpenDosboxFile(nbuf);
	if (tempfile==NULL) {
		// try keyboard layout libraries next
		if ((start_pos=read_kcl_file("keyboard.sys",keyboard_file_name,true))) {
			tempfile = OpenDosboxFile("keyboard.sys");
		} else if ((start_pos=read_kcl_file("keybrd2.sys",keyboard_file_name,true))) {
			tempfile = OpenDosboxFile("keybrd2.sys");
		} else if ((start_pos=read_kcl_file("keybrd3.sys",keyboard_file_name,true))) {
			tempfile = OpenDosboxFile("keybrd3.sys");
		} else if ((start_pos=read_kcl_file("keyboard.sys",keyboard_file_name,false))) {
			tempfile = OpenDosboxFile("keyboard.sys");
		} else if ((start_pos=read_kcl_file("keybrd2.sys",keyboard_file_name,false))) {
			tempfile = OpenDosboxFile("keybrd2.sys");
		} else if ((start_pos=read_kcl_file("keybrd3.sys",keyboard_file_name,false))) {
			tempfile = OpenDosboxFile("keybrd3.sys");
		} else if ((start_pos=read_kcl_data(layout_keyboardsys,33196,keyboard_file_name,true))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<33196; ct++) read_buf[read_buf_size++]=layout_keyboardsys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd2sys,25431,keyboard_file_name,true))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<25431; ct++) read_buf[read_buf_size++]=layout_keybrd2sys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd3sys,27122,keyboard_file_name,true))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<27122; ct++) read_buf[read_buf_size++]=layout_keybrd3sys[ct];
		} else if ((start_pos=read_kcl_data(layout_keyboardsys,33196,keyboard_file_name,false))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<33196; ct++) read_buf[read_buf_size++]=layout_keyboardsys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd2sys,25431,keyboard_file_name,false))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<25431; ct++) read_buf[read_buf_size++]=layout_keybrd2sys[ct];
		} else if ((start_pos=read_kcl_data(layout_keybrd3sys,27122,keyboard_file_name,false))) {
			read_buf_size=0;
			for (Bitu ct=start_pos+2; ct<27122; ct++) read_buf[read_buf_size++]=layout_keybrd3sys[ct];
		} else {
			start_pos=0;
			LOG(LOG_BIOS,LOG_ERROR)("Keyboard layout file %s not found",keyboard_file_name);
			return (IS_PC98_ARCH ? 932 : 437);
		}
		if (tempfile) {
			fseek(tempfile, long(start_pos+2), SEEK_SET);
			read_buf_size=(uint32_t)fread(read_buf, sizeof(uint8_t), 65535, tempfile);
			fclose(tempfile);
		}
		start_pos=0;
	} else {
		// check ID-bytes of file
		uint32_t dr=(uint32_t)fread(read_buf, sizeof(uint8_t), 4, tempfile);
		if ((dr<4) || (read_buf[0]!=0x4b) || (read_buf[1]!=0x4c) || (read_buf[2]!=0x46)) {
			LOG(LOG_BIOS,LOG_ERROR)("Invalid keyboard layout file %s",keyboard_file_name);
			return (IS_PC98_ARCH ? 932 : 437);
		}

		fseek(tempfile, 0, SEEK_SET);
		read_buf_size=(uint32_t)fread(read_buf, sizeof(uint8_t), 65535, tempfile);
		fclose(tempfile);
	}

	uint8_t data_len,submappings;
	data_len=read_buf[start_pos++];

	start_pos+=data_len;		// start_pos==absolute position of KeybCB block

	submappings=read_buf[start_pos];

	// check all submappings and use them if general submapping or same codepage submapping
	for (uint16_t sub_map=0; (sub_map<submappings); sub_map++) {
		uint16_t submap_cp;

		// read codepage of submapping
		submap_cp=host_readw(&read_buf[start_pos+0x14u+sub_map*8u]);

		if (submap_cp!=0) return submap_cp;
	}
	return (IS_PC98_ARCH ? 932 : 437);
}

extern int eurAscii;
extern uint8_t euro_08[8], euro_14[14], euro_16[16];
Bitu keyboard_layout::read_codepage_file(const char* codepage_file_name, int32_t codepage_id) {
	char cp_filename[512];
	strcpy(cp_filename, codepage_file_name);
	if (!strcmp(cp_filename,"none")) return KEYB_NOERROR;

	if (codepage_id==dos.loaded_codepage) return KEYB_NOERROR;

	if (!strcmp(cp_filename,"auto")) {
		// select matching .cpi-file for specified codepage
		switch (codepage_id) {
			case 437:	case 850:	case 852:	case 853:	case 857:	case 858:	
						sprintf(cp_filename, "EGA.CPI"); break;
			case 775:	case 859:	case 1116:	case 1117:
						sprintf(cp_filename, "EGA2.CPI"); break;
			case 771:	case 772:	case 808:	case 855:	case 866:	case 872:
						sprintf(cp_filename, "EGA3.CPI"); break;
			case 848:	case 849:	case 1125:	case 1131:	case 61282:
						sprintf(cp_filename, "EGA4.CPI"); break;
			case 737:	case 851:	case 869:
						sprintf(cp_filename, "EGA5.CPI"); break;
			case 113:	case 899:	case 59829:	case 60853:
						sprintf(cp_filename, "EGA6.CPI"); break;
			case 58152:	case 58210:	case 59234:	case 60258:	case 62306:
						sprintf(cp_filename, "EGA7.CPI"); break;
			case 770:	case 773:	case 774:	case 777:	case 778:
						sprintf(cp_filename, "EGA8.CPI"); break;
			case 860:	case 861:	case 863:	case 865:
						sprintf(cp_filename, "EGA9.CPI"); break;
			case 667:	case 668:	case 790:	case 867:	case 991:	case 57781:
						sprintf(cp_filename, "EGA10.CPI"); break;
			default:
				LOG_MSG("No matching cpi file for codepage %i",codepage_id);
				return KEYB_INVALIDCPFILE;
		}
	}

	uint32_t start_pos;
	uint16_t number_of_codepages;

	char nbuf[512];
	sprintf(nbuf, "%s", cp_filename);
	FILE* tempfile=OpenDosboxFile(nbuf);
	if (tempfile==NULL) {
		size_t strsz=strlen(nbuf);
		if (strsz) {
			char plc=(char)toupper(*reinterpret_cast<unsigned char*>(&nbuf[strsz-1]));
			if (plc=='I') {
				// try CPX-extension as well
				nbuf[strsz-1]='X';
				tempfile=OpenDosboxFile(nbuf);
			} else if (plc=='X') {
				// try CPI-extension as well
				nbuf[strsz-1]='I';
				tempfile=OpenDosboxFile(nbuf);
			}
		}
	}

	static uint8_t cpi_buf[65536];
	uint32_t cpi_buf_size=0,size_of_cpxdata=0;
	bool upxfound=false;
	uint16_t found_at_pos=5;
	if (tempfile==NULL) {
		// check if build-in codepage is available
		switch (codepage_id) {
			case 437:	case 850:	case 852:	case 853:	case 857:	case 858:	
						for (Bitu bct=0; bct<6322; bct++) cpi_buf[bct]=font_ega_cpx[bct];
						cpi_buf_size=6322;
						break;
			case 771:	case 772:	case 808:	case 855:	case 866:	case 872:
						for (Bitu bct=0; bct<5455; bct++) cpi_buf[bct]=font_ega3_cpx[bct];
						cpi_buf_size=5455;
						break;
			case 737:	case 851:	case 869:
						for (Bitu bct=0; bct<5720; bct++) cpi_buf[bct]=font_ega5_cpx[bct];
						cpi_buf_size=5720;
						break;
			default: 
				return KEYB_INVALIDCPFILE;
		}
		upxfound=true;
		found_at_pos=0x29;
		size_of_cpxdata=cpi_buf_size;
	} else {
		uint32_t dr=(uint32_t)fread(cpi_buf, sizeof(uint8_t), 5, tempfile);
		// check if file is valid
		if (dr<5) {
			LOG(LOG_BIOS,LOG_ERROR)("Codepage file %s invalid",cp_filename);
			return KEYB_INVALIDCPFILE;
		}
		// check if non-compressed cpi file
		if ((cpi_buf[0]!=0xff) || (cpi_buf[1]!=0x46) || (cpi_buf[2]!=0x4f) || 
			(cpi_buf[3]!=0x4e) || (cpi_buf[4]!=0x54)) {
			// check if dr-dos custom cpi file
			if ((cpi_buf[0]==0x7f) && (cpi_buf[1]!=0x44) && (cpi_buf[2]!=0x52) && 
				(cpi_buf[3]!=0x46) && (cpi_buf[4]!=0x5f)) {
				LOG(LOG_BIOS,LOG_ERROR)("Codepage file %s has unsupported DR-DOS format",cp_filename);
				return KEYB_INVALIDCPFILE;
			}
			// check if compressed cpi file
			uint8_t next_byte=0;
			for (Bitu i=0; i<100; i++) {
                size_t readResult = fread(&next_byte, sizeof(uint8_t), 1, tempfile);
                if (readResult != 1) {
                    LOG(LOG_IO, LOG_ERROR) ("Reading error in read_codepage_file\n");
                }
                found_at_pos++;
                while (next_byte == 0x55) {
                    readResult = fread(&next_byte, sizeof(uint8_t), 1, tempfile);
                    if (readResult != 1) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in read_codepage_file\n");
                    }
                    found_at_pos++;
                    if (next_byte == 0x50) {
                        readResult = fread(&next_byte, sizeof(uint8_t), 1, tempfile);
                        if (readResult != 1) {
                            LOG(LOG_IO, LOG_ERROR) ("Reading error in read_codepage_file\n");
                        }
                        found_at_pos++;
                        if (next_byte == 0x58) {
                            readResult = fread(&next_byte, sizeof(uint8_t), 1, tempfile);
                            if (readResult != 1) {
                                LOG(LOG_IO, LOG_ERROR) ("Reading error in read_codepage_file\n");
                            }
                            found_at_pos++;
                            if (next_byte == 0x21) {
                                // read version ID
                                readResult = fread(&next_byte, sizeof(uint8_t), 1, tempfile);
                                if (readResult != 1) {
                                    LOG(LOG_IO, LOG_ERROR) ("Reading error in read_codepage_file\n");
                                }
								found_at_pos++;
								upxfound=true;
								break;
							}
						}
					}
				}
				if (upxfound) break;
			}
			if (!upxfound) {
				LOG(LOG_BIOS,LOG_ERROR)("Codepage file %s invalid: %x",cp_filename,cpi_buf[0]);
				return KEYB_INVALIDCPFILE;
			} else {
				if (next_byte<10) E_Exit("UPX-compressed cpi file, but upx-version too old");

				// read in compressed CPX-file
				fseek(tempfile, 0, SEEK_SET);
				size_of_cpxdata=(uint32_t)fread(cpi_buf, sizeof(uint8_t), 65536, tempfile);
			}
		} else {
			// standard uncompressed cpi-file
			fseek(tempfile, 0, SEEK_SET);
			cpi_buf_size=(uint32_t)fread(cpi_buf, sizeof(uint8_t), 65536, tempfile);
		}
	}

	if (upxfound) {
		if (size_of_cpxdata>0xfe00) E_Exit("Size of cpx-compressed data too big");

		found_at_pos+=19;
		// prepare for direct decompression
		cpi_buf[found_at_pos]=0xcb;

		uint16_t seg=0;
		uint16_t size=0x1500;
		if (!DOS_AllocateMemory(&seg,&size)) E_Exit("Not enough free low memory to unpack data");
		MEM_BlockWrite(((unsigned int)seg<<4u)+0x100u,cpi_buf,size_of_cpxdata);

		// setup segments
		uint16_t save_ds=SegValue(ds);
		uint16_t save_es=SegValue(es);
		uint16_t save_ss=SegValue(ss);
		uint32_t save_esp=reg_esp;
		SegSet16(ds,seg);
		SegSet16(es,seg);
		SegSet16(ss,seg+0x1000);
		reg_esp=0xfffe;

		// let UPX unpack the file
		CALLBACK_RunRealFar(seg,0x100);

		SegSet16(ds,save_ds);
		SegSet16(es,save_es);
		SegSet16(ss,save_ss);
		reg_esp=save_esp;

		// get unpacked content
		MEM_BlockRead(((unsigned int)seg<<4u)+0x100u,cpi_buf,65536u);
		cpi_buf_size=65536;

		DOS_FreeMemory(seg);
	}


	start_pos=host_readd(&cpi_buf[0x13]);
	number_of_codepages=host_readw(&cpi_buf[start_pos]);
	start_pos+=4;

	// search if codepage is provided by file
	for (uint16_t test_codepage=0; test_codepage<number_of_codepages; test_codepage++) {
		uint16_t device_type, font_codepage, font_type;

		// device type can be display/printer (only the first is supported)
		device_type=host_readw(&cpi_buf[start_pos+0x04]);
		font_codepage=host_readw(&cpi_buf[start_pos+0x0e]);

		uint32_t font_data_header_pt;
		font_data_header_pt=host_readd(&cpi_buf[start_pos+0x16]);

		font_type=host_readw(&cpi_buf[font_data_header_pt]);

		if ((device_type==0x0001) && (font_type==0x0001) && (font_codepage==codepage_id)) {
			// valid/matching codepage found

			uint16_t number_of_fonts;//,font_data_length;
			number_of_fonts=host_readw(&cpi_buf[font_data_header_pt+0x02]);
//			font_data_length=host_readw(&cpi_buf[font_data_header_pt+0x04]);

			bool font_changed=false;
			uint32_t font_data_start=font_data_header_pt+0x06;

			// load all fonts if possible
			for (uint16_t current_font=0; current_font<number_of_fonts; current_font++) {
				uint8_t font_height=cpi_buf[font_data_start];
				font_data_start+=6;
				if (font_height==0x10) {
					// 16x8 font, IF supported by the video card
                    if (int10.rom.font_16 != 0) {
                        PhysPt font16pt=Real2Phys(int10.rom.font_16);
                        for (uint16_t i=0;i<256*16;i++) {
                            phys_writeb(font16pt+i,eurAscii>32&&i/16==eurAscii?euro_16[i%16]:cpi_buf[font_data_start+i]);
                        }
                        // terminate alternate list to prevent loading
                        phys_writeb(Real2Phys(int10.rom.font_16_alternate),0);
                        font_changed=true;
                    }
				} else if (font_height==0x0e) {
					// 14x8 font, IF supported by the video card
                    if (int10.rom.font_14 != 0) {
                        PhysPt font14pt=Real2Phys(int10.rom.font_14);
                        for (uint16_t i=0;i<256*14;i++) {
                            phys_writeb(font14pt+i,eurAscii>32&&i/14==eurAscii?euro_14[i%14]:cpi_buf[font_data_start+i]);
                        }
                        // terminate alternate list to prevent loading
                        phys_writeb(Real2Phys(int10.rom.font_14_alternate),0);
                        font_changed=true;
                    }
				} else if (font_height==0x08) {
                    // 8x8 fonts. All video cards support it
                    if (int10.rom.font_8_first != 0) {
                        PhysPt font8pt=Real2Phys(int10.rom.font_8_first);
                        for (uint16_t i=0;i<128*8;i++) {
                            phys_writeb(font8pt+i,eurAscii>32&&i/8==eurAscii?euro_08[i%8]:cpi_buf[font_data_start+i]);
                        }
                        font_changed=true;
                    }
                    if (int10.rom.font_8_second != 0) {
                        PhysPt font8pt=Real2Phys(int10.rom.font_8_second);
                        for (uint16_t i=0;i<128*8;i++) {
                            phys_writeb(font8pt+i,eurAscii>127&&(i+128)/8==eurAscii?euro_08[i%8]:cpi_buf[font_data_start+i+128*8]);
                        }
                        font_changed=true;
                    }
                }
				font_data_start+=font_height*256u;
			}

			LOG(LOG_BIOS,LOG_NORMAL)("Codepage %i successfully loaded",codepage_id);

			// set codepage entries
			dos.loaded_codepage=(uint16_t)(codepage_id&0xffff);
#if defined(USE_TTF)
            if (TTF_using()) setTTFCodePage(); else
#endif
            DOSBox_SetSysMenu();

			// update font if necessary (EGA/VGA/SVGA only)
			if (font_changed && (CurMode->type==M_TEXT) && (IS_EGAVGA_ARCH)) {
				INT10_ReloadFont();
			}
			INT10_SetupRomMemoryChecksum();

			return KEYB_NOERROR;
		}

		start_pos=host_readd(&cpi_buf[start_pos]);
		start_pos+=2;
	}

	LOG(LOG_BIOS,LOG_ERROR)("Codepage %i not found",codepage_id);

	return KEYB_INVALIDCPFILE;
}

Bitu keyboard_layout::switch_keyboard_layout(const char* new_layout, keyboard_layout*& created_layout, int32_t& tried_cp) {
	if (strncasecmp(new_layout,"US",2)) {
		// switch to a foreign layout
		char tbuf[256];
		strcpy(tbuf, new_layout);
		size_t newlen=strlen(tbuf);

		bool language_code_found=false;
		// check if language code is present in loaded foreign layout
		for (Bitu i=0; i<language_code_count; i++) {
			if (!strncasecmp(tbuf,language_codes[i],newlen)) {
				language_code_found=true;
				break;
			}
		}

		if (language_code_found) {
			if (!this->use_foreign_layout) {
				// switch to foreign layout
				this->use_foreign_layout=true;
				diacritics_character=0;
				LOG(LOG_BIOS,LOG_NORMAL)("Switched to layout %s",tbuf);
			}
		} else {
			keyboard_layout * temp_layout=new keyboard_layout();
			uint16_t req_codepage=temp_layout->extract_codepage(new_layout);
			tried_cp = req_codepage;
			Bitu kerrcode=temp_layout->read_keyboard_file(new_layout, req_codepage);
			if (kerrcode) {
				delete temp_layout;
				return kerrcode;
			}
			// ...else keyboard layout loaded successfully, change codepage accordingly
			kerrcode=temp_layout->read_codepage_file("auto", req_codepage);
			if (kerrcode) {
				delete temp_layout;
				return kerrcode;
			}
			// Everything went fine, switch to new layout
			created_layout=temp_layout;
		}
	} else if (this->use_foreign_layout) {
		// switch to the US layout
		this->use_foreign_layout=false;
		if (tried_cp < 0) dos.loaded_codepage = 437;
		diacritics_character=0;
		LOG(LOG_BIOS,LOG_NORMAL)("Switched to US layout");
	}
	return KEYB_NOERROR;
}

void keyboard_layout::switch_foreign_layout() {
	this->use_foreign_layout=!this->use_foreign_layout;
	diacritics_character=0;
	if (this->use_foreign_layout) LOG(LOG_BIOS,LOG_NORMAL)("Switched to foreign layout");
	else LOG(LOG_BIOS,LOG_NORMAL)("Switched to US layout");
}

const char* keyboard_layout::get_layout_name() {
	// get layout name (language ID or NULL if default layout)
	if (use_foreign_layout) {
		if (strcmp(current_keyboard_file_name,"none") != 0) {
			return (const char*)&current_keyboard_file_name;
		}
	}
	return NULL;
}

const char* keyboard_layout::main_language_code() {
	if (language_codes) {
		return language_codes[0];
	}
	return NULL;
}


static keyboard_layout* loaded_layout=NULL;

// CTRL-ALT-F2 switches between foreign and US-layout using this function
/* static void switch_keyboard_layout(bool pressed) {
	if (!pressed)
		return;
	if (loaded_layout) loaded_layout->switch_foreign_layout();
} */

// called by int9-handler
bool DOS_LayoutKey(Bitu key, uint8_t flags1, uint8_t flags2, uint8_t flags3) {
	if (loaded_layout) return loaded_layout->layout_key(key, flags1, flags2, flags3);
	else return false;
}

Bitu DOS_LoadKeyboardLayout(const char * layoutname, int32_t codepage, const char * codepagefile) {
	keyboard_layout * temp_layout=new keyboard_layout();
	// try to read the layout for the specified codepage
	Bitu kerrcode=temp_layout->read_keyboard_file(layoutname, codepage);
	if (kerrcode) {
		delete temp_layout;
		return kerrcode;
	}
	// ...else keyboard layout loaded successfully, change codepage accordingly
	kerrcode=temp_layout->read_codepage_file(codepagefile, codepage);
	if (kerrcode) {
		delete temp_layout;
		return kerrcode;
	}
	// Everything went fine, switch to new layout
	loaded_layout=temp_layout;
	return KEYB_NOERROR;
}

Bitu DOS_SwitchKeyboardLayout(const char* new_layout, int32_t& tried_cp) {
	if (loaded_layout) {
		keyboard_layout* changed_layout=NULL;
		Bitu ret_code=loaded_layout->switch_keyboard_layout(new_layout, changed_layout, tried_cp);
		if (changed_layout) {
			// Remove old layout, activate new layout
			delete loaded_layout;
			loaded_layout=changed_layout;
		}
		return ret_code;
	} else return 0xff;
}

// get currently loaded layout name (NULL if no layout is loaded)
const char* DOS_GetLoadedLayout(void) {
	if (loaded_layout) {
		return loaded_layout->get_layout_name();
	}
	return NULL;
}

class DOS_KeyboardLayout: public Module_base {
public:
	DOS_KeyboardLayout(Section* configuration):Module_base(configuration){
        const Section_prop* section = static_cast<Section_prop*>(configuration);
		dos.loaded_codepage=(IS_PC98_ARCH ? 932 : 437);	// US codepage already initialized
#if defined(USE_TTF)
        if (TTF_using()) setTTFCodePage(); else
#endif
        DOSBox_SetSysMenu();
		loaded_layout=new keyboard_layout();

		const char * layoutname=section->Get_string("keyboardlayout");

		Bits wants_dos_codepage = -1;
		if (!strncmp(layoutname,"auto",4)) {
#if defined (WIN32)
			WORD cur_kb_layout = LOWORD(GetKeyboardLayout(0));
			WORD cur_kb_subID  = 0;
			char layoutID_string[KL_NAMELENGTH];
			if (GetKeyboardLayoutName(layoutID_string)) {
				if (strlen(layoutID_string) == 8) {
					int cur_kb_layout_by_name = (int)ConvHexWord((char*)&layoutID_string[4]);
					layoutID_string[4] = 0;
					int subID = (int)ConvHexWord((char*)&layoutID_string[0]);
					if ((cur_kb_layout_by_name>0) && (cur_kb_layout_by_name<65536)) {
						// use layout ID extracted from the layout string
						cur_kb_layout = (WORD)cur_kb_layout_by_name;
					}
					if ((subID>=0) && (subID<100)) {
						// use sublanguage ID extracted from the layout string
						cur_kb_subID  = (WORD)subID;
					}
				}
			}
			// try to match emulated keyboard layout with host-keyboardlayout
			// codepage 437 (standard) is preferred
			switch (cur_kb_layout) {
/*				case 1026: // Bulgaria, CP 915, Alt CP 850
					layoutname = "bg241";
					break; */
				case 1029: // Czech Republic, CP 852, Alt CP 850
					layoutname = "cz243";
					break;
				case 1030: // Denmark, CP 850, Alt CP 865
					layoutname = "dk";
					break;
				case 1031: // Germany, CP 850, Alt CP 437
					layoutname = "gr";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1033: // US, CP 437
					return;
				case 1032: // Greece, CP 869, Alt CP 813
					layoutname = "gk";
					break;
				case 1034: // Spain, CP 850, Alt CP 437
					layoutname = "sp";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1035: // Finland, CP 850, Alt CP 437
					layoutname = "su";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1036: // France, CP 850, Alt CP 437
					layoutname = "fr";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1038: // Hungary, CP 852, Alt CP 850
					if (cur_kb_subID==1) layoutname = "hu";
					else layoutname = "hu208";
					break;
				case 1039: // Iceland, CP 850, Alt CP 861
					layoutname = "is161";
					break;
				case 1040: // Italy, CP 850, Alt CP 437
					layoutname = "it";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1043: // Netherlands, CP 850, Alt CP 437
					layoutname = "nl";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1044: // Norway, CP 850, Alt CP 865
					layoutname = "no";
					break;
				case 1045: // Poland, CP 852, Alt CP 850
					layoutname = "pl";
					break;
				case 1046: // Brazil, CP 850, Alt CP 437
					layoutname = "br";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
/*				case 1048: // Romania, CP  852, Alt CP 850
					layoutname = "ro446";
					break; */
				case 1049: // Russia, CP  866, Alt CP 915
					layoutname = "ru";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1050: // Croatia, CP 852, Alt CP 850
					layoutname = "hr";
					break;
				case 1051: // Slovakia, CP 852, Alt CP 850
					layoutname = "sk";
					break;
/*				case 1052: // Albania, CP 850, Alt CP 437
					layoutname = "sq448";
					break; */
				case 1053: // Sweden, CP 850, Alt CP 437
					layoutname = "sv";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1055: // Turkey, CP 857
					layoutname = "tr";
					break;
				case 1058: // Ukraine, CP 848?
					layoutname = "ur";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1059: // Belarus, CP 849?
					layoutname = "bl";
					break;
				case 1060: // Slovenia, CP 852, Alt CP 850
					layoutname = "si";
					break;
				case 1061: // Estonia, CP 922, Alt CP 850
					layoutname = "et";
					break;
/*				case 1062: // Latvia, CP 921, Alt CP 850
					layoutname = "lv";
					break; */
/*				case 1063: // Lithuania, CP 921, Alt CP 850
					layoutname = "lt221";
					break; */
/*				case 1064: // Tajik, CP 866?
					layoutname = "tj";
					break;
				case 1066: // Vietnam, has no DOS CP
					layoutname = "vi";
					break;
				case 1067: // Armenian, has no DOS CP
					layoutname = "hy";
					break; */
				case 2055: // Swiss-German, CP 850, Alt CP 437
					layoutname = "sg";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 2057: // UK, CP 850, Alt CP 437
					layoutname = "uk"; 
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 2060: // Belgium-French, CP 850, Alt CP 437
					layoutname = "be";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 2064: // Swiss-Italian, CP 850, Alt CP 437
					layoutname = "sf"; // Uses Swiss-French
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 2067: // Belgium-Dutch, CP 850, Alt CP 437
					layoutname = "be";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 2070: // Portugal, CP 850, Alt CP 860
					layoutname = "po";
					break;
				case 3081: // Australia, CP 850, Alt CP 437
					layoutname = "us"; 
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 3184: // Canada-French, CP 850, Alt CP 863
					layoutname = "cf";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 4103: // Luxembourg-German, CP 850, Alt CP 437
					layoutname = "sf"; // Official, but BE and DE are also common
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 4105: // Canada-English, CP 850, Alt CP 437
					layoutname = "ca";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 4108: // Swiss-French, CP 850, Alt CP 437
					layoutname = "sf";
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 4127: // Liechtenstein, CP 850, Alt CP 437
					layoutname = "sg"; // Uses Swiss-German
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 5129: // New-Zealand, CP 850, Alt CP 437
					layoutname = "us"; 
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 5132: // Luxembourg-French, CP 840, Alt CP 437
					layoutname = "sf"; // Official, but BE and DE are also common
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 6153: // Ireland, CP 850, Alt CP 437
					layoutname = "uk"; 
					wants_dos_codepage = (IS_PC98_ARCH ? 932 : 437);
					break;
				case 1041: // Japan, CP 943, Alt CP 942
					layoutname = "jp";
					break;
				default:
					break;
			}
#endif
		}

		bool extract_codepage = true;
		if (wants_dos_codepage>0) {
			if ((loaded_layout->read_codepage_file("auto", (int32_t)wants_dos_codepage)) == KEYB_NOERROR) {
				// preselected codepage was successfully loaded
				extract_codepage = false;
			}
		}
		if (extract_codepage) {
			// try to find a good codepage for the requested layout
			uint16_t req_codepage = loaded_layout->extract_codepage(layoutname);
			loaded_layout->read_codepage_file("auto", req_codepage);
		}

/*		if (strncmp(layoutname,"auto",4) && strncmp(layoutname,"none",4)) {
			LOG_MSG("Loading DOS keyboard layout %s ...",layoutname);
		} */
		if (loaded_layout->read_keyboard_file(layoutname, dos.loaded_codepage)) {
			if (strncmp(layoutname,"auto",4)) {
				LOG_MSG("Error loading keyboard layout %s",layoutname);
			}
		} else {
			const char* lcode = loaded_layout->main_language_code();
			if (lcode) {
				LOG_MSG("DOS keyboard layout loaded with main language code %s for layout %s",lcode,layoutname);
			}
		}
	}

	~DOS_KeyboardLayout(){
		if ((dos.loaded_codepage!=(IS_PC98_ARCH ? 932 : 437)) && (CurMode->type==M_TEXT)) {
			INT10_ReloadRomFonts();
			dos.loaded_codepage=(IS_PC98_ARCH ? 932 : 437);	// US codepage
		}
		if (loaded_layout) {
			delete loaded_layout;
			loaded_layout=NULL;
		}
	}
};

static DOS_KeyboardLayout* test;

void DOS_KeyboardLayout_ShutDown(Section* /*sec*/) {
	if (test != NULL) {
		delete test;
		test = NULL;
	}
}

void DOS_KeyboardLayout_Startup(Section* sec) {
    (void)sec;//UNUSED
	if (test == NULL) {
		LOG(LOG_DOSMISC,LOG_DEBUG)("Reinitializing DOS keyboard layout support");
		test = new DOS_KeyboardLayout(control->GetSection("dos"));
	}
}

void DOS_KeyboardLayout_Init() {
	LOG(LOG_DOSMISC,LOG_DEBUG)("Initializing DOS keyboard layout emulation");

	AddExitFunction(AddExitFunctionFuncPair(DOS_KeyboardLayout_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(DOS_KeyboardLayout_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_BEGIN,AddVMEventFunctionFuncPair(DOS_KeyboardLayout_ShutDown));
}

