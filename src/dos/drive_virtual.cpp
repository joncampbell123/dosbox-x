/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "control.h"
#include "cross.h"

struct VFILE_Block {
	const char * name;
	const char * lname;
	Bit8u * data;
	Bit32u size;
	Bit16u date;
	Bit16u time;
	VFILE_Block * next;
};


static VFILE_Block * first_file;	

void VFILE_Shutdown(void) {
	LOG(LOG_MISC,LOG_DEBUG)("Shutting down VFILE system");

	while (first_file != NULL) {
		VFILE_Block *n = first_file->next;
		delete first_file;
		first_file = n;
	}
}

void VFILE_RegisterBuiltinFileBlob(const struct BuiltinFileBlob &b) {
	VFILE_Register(b.recommended_file_name, (Bit8u*)b.data, (Bit32u)b.length);
}

void VFILE_Register(const char * name,Bit8u * data,Bit32u size) {
	VFILE_Block * new_file=new VFILE_Block;
	new_file->name=name;
	new_file->lname=name;
	new_file->data=data;
	new_file->size=size;
	new_file->date=DOS_PackDate(2002,10,1);
	new_file->time=DOS_PackTime(12,34,56);
	new_file->next=first_file;
	first_file=new_file;
}

void VFILE_Remove(const char *name) {
	VFILE_Block * chan=first_file;
	VFILE_Block * * where=&first_file;
	while (chan) {
		if (strcmp(name,chan->name) == 0) {
			*where = chan->next;
			if(chan == first_file) first_file = chan->next;
			delete chan;
			return;
		}
		where=&chan->next;
		chan=chan->next;
	}
}

class Virtual_File : public DOS_File {
public:
	Virtual_File(Bit8u * in_data,Bit32u in_size);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(const Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * new_pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
private:
	Bit32u file_size;
    Bit32u file_pos = 0;
	Bit8u * file_data;
};


Virtual_File::Virtual_File(Bit8u* in_data, Bit32u in_size) : file_size(in_size), file_data(in_data) {
	date=DOS_PackDate(2002,10,1);
	time=DOS_PackTime(12,34,56);
	open=true;
}

bool Virtual_File::Read(Bit8u * data,Bit16u * size) {
	Bit32u left=file_size-file_pos;
	if (left<=*size) { 
		memcpy(data,&file_data[file_pos],left);
		*size=(Bit16u)left;
	} else {
		memcpy(data,&file_data[file_pos],*size);
	}
	file_pos+=*size;
	return true;
}

bool Virtual_File::Write(const Bit8u * data,Bit16u * size){
    (void)data;//UNUSED
    (void)size;//UNUSED
	/* Not really writable */
	return false;
}

bool Virtual_File::Seek(Bit32u * new_pos,Bit32u type){
	switch (type) {
	case DOS_SEEK_SET:
		if (*new_pos<=file_size) file_pos=*new_pos;
		else return false;
		break;
	case DOS_SEEK_CUR:
		if ((*new_pos+file_pos)<=file_size) file_pos=*new_pos+file_pos;
		else return false;
		break;
	case DOS_SEEK_END:
		if (*new_pos<=file_size) file_pos=file_size-*new_pos;
		else return false;
		break;
	}
	*new_pos=file_pos;
	return true;
}

bool Virtual_File::Close(){
	return true;
}


Bit16u Virtual_File::GetInformation(void) {
	return 0x40;	// read-only drive
}


Virtual_Drive::Virtual_Drive() {
	strcpy(info,"Internal Virtual Drive");
}


bool Virtual_Drive::FileOpen(DOS_File * * file,const char * name,Bit32u flags) {
/* Scan through the internal list of files */
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (strcasecmp(name,cur_file->name)==0) {
		/* We have a match */
			*file=new Virtual_File(cur_file->data,cur_file->size);
			(*file)->flags=flags;
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileCreate(DOS_File * * file,const char * name,Bit16u attributes) {
    (void)file;//UNUSED
    (void)name;//UNUSED
    (void)attributes;//UNUSED
	return false;
}

bool Virtual_Drive::FileUnlink(const char * name) {
    (void)name;//UNUSED
	return false;
}

bool Virtual_Drive::RemoveDir(const char * dir) {
    (void)dir;//UNUSED
	return false;
}

bool Virtual_Drive::MakeDir(const char * dir) {
    (void)dir;//UNUSED
	return false;
}

bool Virtual_Drive::TestDir(const char * dir) {
	if (!dir[0]) return true;		//only valid dir is the empty dir
	return false;
}

bool Virtual_Drive::FileStat(const char* name, FileStat_Block * const stat_block){
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (strcasecmp(name,cur_file->name)==0) {
			stat_block->attr=DOS_ATTR_ARCHIVE;
			stat_block->size=cur_file->size;
			stat_block->date=DOS_PackDate(2002,10,1);
			stat_block->time=DOS_PackTime(12,34,56);
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileExists(const char* name){
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (strcasecmp(name,cur_file->name)==0) return true;
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
    (void)_dir;//UNUSED
	search_file=first_file;
	Bit8u attr;char pattern[CROSS_LEN];
    dta.GetSearchParams(attr,pattern,false);
	if (attr == DOS_ATTR_VOLUME) {
		dta.SetResult(GetLabel(),GetLabel(),0,0,0,DOS_ATTR_VOLUME);
		return true;
	} else if ((attr & DOS_ATTR_VOLUME) && !fcb_findfirst) {
		if (WildFileCmp(GetLabel(),pattern)) {
			dta.SetResult(GetLabel(),GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	}
	return FindNext(dta);
}

bool Virtual_Drive::FindNext(DOS_DTA & dta) {
	Bit8u attr;char pattern[CROSS_LEN];
    dta.GetSearchParams(attr,pattern,false);
	while (search_file) {
		if (WildFileCmp(search_file->name,pattern)) {
			dta.SetResult(search_file->name,search_file->lname,search_file->size,search_file->date,search_file->time,DOS_ATTR_ARCHIVE);
			search_file=search_file->next;
			return true;
		}
		search_file=search_file->next;
	}
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool Virtual_Drive::SetFileAttr(const char * name,Bit16u attr) {
    (void)name;
    (void)attr;
	return false;
}

bool Virtual_Drive::GetFileAttr(const char * name,Bit16u * attr) {
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (strcasecmp(name,cur_file->name)==0) { 
			*attr = DOS_ATTR_ARCHIVE;	//Maybe readonly ?
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::GetFileAttrEx(char* name, struct stat *status) {
    (void)name;
    (void)status;
	return false;
}

unsigned long Virtual_Drive::GetCompressedSize(char* name) {
    (void)name;
	return 0;
}

#if defined (WIN32)
HANDLE Virtual_Drive::CreateOpenFile(const char* name) {
    (void)name;
	DOS_SetError(1);
	return INVALID_HANDLE_VALUE;
}
#endif

bool Virtual_Drive::Rename(const char * oldname,const char * newname) {
    (void)oldname;//UNUSED
    (void)newname;//UNUSED
	return false;
}

bool Virtual_Drive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	*_bytes_sector=512;
	*_sectors_cluster=32;
	*_total_clusters=32765;	// total size is always 500 mb
	*_free_clusters=0;		// nothing free here
	return true;
}

Bit8u Virtual_Drive::GetMediaByte(void) {
	return 0xF8;
}

bool Virtual_Drive::isRemote(void) {
    const Section_prop * section=static_cast<Section_prop *>(control->GetSection("dos"));
    const char * opt = section->Get_string("drive z is remote");

    if (!strcmp(opt,"1") || !strcmp(opt,"true")) {
        return true;
    }
    else if (!strcmp(opt,"0") || !strcmp(opt,"false")) {
        return false;
    }

    /* TODO: Automatically detect if called by SCANDISK.EXE and return true */

	return false;
}

bool Virtual_Drive::isRemovable(void) {
	return false;
}

Bits Virtual_Drive::UnMount(void) {
	return 1;
}

char const* Virtual_Drive::GetLabel(void) {
	return "DOSBOX";
}
