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
#include "regs.h"

struct VFILE_Block {
	const char * name;
	const char * lname;
	uint8_t * data;
	uint32_t size;
	uint16_t date;
	uint16_t time;
	unsigned int onpos;
	bool isdir;
	bool hidden;
	VFILE_Block * next;
};

#define MAX_VFILES 500
unsigned int vfpos=1, lfn_id[256];
char vfnames[MAX_VFILES][CROSS_LEN],vfsnames[MAX_VFILES][DOS_NAMELENGTH_ASCII],ondirs[MAX_VFILES][CROSS_LEN];
static VFILE_Block * first_file, * lfn_search[256];

extern int lfn_filefind_handle;
extern bool filename_not_8x3(const char *n), filename_not_strict_8x3(const char *n);
extern char sfn[DOS_NAMELENGTH_ASCII];
std::string hidefiles="";
/* Generate 8.3 names from LFNs, with tilde usage (from ~1 to ~9999). */
char* Generate_SFN(const char *name) {
	if (!filename_not_8x3(name)) {
		strcpy(sfn, name);
		upcase(sfn);
		return sfn;
	}
	char lfn[LFN_NAMELENGTH+1], *n;
	if (name==NULL||!*name) return NULL;
	if (strlen(name)>LFN_NAMELENGTH) {
		strncpy(lfn, name, LFN_NAMELENGTH);
		lfn[LFN_NAMELENGTH]=0;
	} else
		strcpy(lfn, name);
	if (!strlen(lfn)) return NULL;
	unsigned int k=1, i, t=10000;
    const VFILE_Block* cur_file;
	while (k<10000) {
		n=lfn;
		if (t>strlen(n)||k==1||k==10||k==100||k==1000) {
			i=0;
			*sfn=0;
			while (*n == '.'||*n == ' ') n++;
			while (strlen(n)&&(*(n+strlen(n)-1)=='.'||*(n+strlen(n)-1)==' ')) *(n+strlen(n)-1)=0;
			while (*n != 0 && *n != '.' && i<(k<10?6u:(k<100?5u:(k<1000?4:3u)))) {
				if (*n == ' ') {
					n++;
					continue;
				}
				if (*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||*n=='['||*n==']'||*n=='|'||*n=='?'||*n=='*') {
					sfn[i++]='_';
					n++;
				} else
					sfn[i++]=toupper(*(n++));
			}
			sfn[i++]='~';
			t=i;
		} else
			i=t;
		if (k<10)
			sfn[i++]='0'+k;
		else if (k<100) {
			sfn[i++]='0'+(k/10);
			sfn[i++]='0'+(k%10);
		} else if (k<1000) {
			sfn[i++]='0'+(k/100);
			sfn[i++]='0'+((k%100)/10);
			sfn[i++]='0'+(k%10);
		} else {
			sfn[i++]='0'+(k/1000);
			sfn[i++]='0'+((k%1000)/100);
			sfn[i++]='0'+((k%100)/10);
			sfn[i++]='0'+(k%10);
		}
		if (t>strlen(n)||k==1||k==10||k==100||k==1000) {
			char *p=strrchr(n, '.');
			if (p!=NULL) {
				sfn[i++]='.';
				n=p+1;
				while (*n == '.') n++;
				int j=0;
				while (*n != 0 && j++<3) {
					if (*n == ' ') {
						n++;
						continue;
					}
					if (*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||*n=='['||*n==']'||*n=='|'||*n=='?'||*n=='*') {
						sfn[i++]='_';
						n++;
					} else
						sfn[i++]=toupper(*(n++));
				}
			}
			sfn[i++]=0;
		}
        cur_file = first_file;
        bool found=false;
        while (cur_file) {
            if (strcasecmp(sfn,cur_file->name)==0||(uselfn&&strcasecmp(sfn,cur_file->lname)==0)) {found=true;break;}
            cur_file=cur_file->next;
        }
        if (!found) return sfn;
		k++;
	}
	return 0;
}


void VFILE_Shutdown(void) {
	LOG(LOG_DOSMISC,LOG_DEBUG)("Shutting down VFILE system");

	while (first_file != NULL) {
		VFILE_Block *n = first_file->next;
		delete first_file;
		first_file = n;
	}
    vfpos=1;
}

void VFILE_RegisterBuiltinFileBlob(const struct BuiltinFileBlob &b, const char *dir) {
	VFILE_Register(b.recommended_file_name, (uint8_t*)b.data, (uint32_t)b.length, dir);
}

uint16_t fztime=0, fzdate=0;
void VFILE_Register(const char * name,uint8_t * data,uint32_t size,const char *dir) {
    if (vfpos>=MAX_VFILES) return;
    std::istringstream in(hidefiles);
    bool hidden=false;
    bool isdir=!strcmp(dir,"/")||!strcmp(name,".")||!strcmp(name,"..");
    unsigned int onpos=0;
    char fullname[CROSS_LEN], fullsname[CROSS_LEN];
    if (strlen(dir)>2&&dir[0]=='/'&&dir[strlen(dir)-1]=='/') {
        for (unsigned int i=1; i<vfpos; i++)
            if (!strcasecmp((std::string(vfsnames[i])+"/").c_str(), dir+1)||!strcasecmp((std::string(vfnames[i])+"/").c_str(), dir+1)) {
                onpos=i;
                break;
            }
        if (onpos==0) return;
    }
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (onpos==cur_file->onpos&&(strcasecmp(name,cur_file->name)==0||(uselfn&&strcasecmp(name,cur_file->name)==0))) return;
		cur_file=cur_file->next;
	}
    std::string sname=filename_not_strict_8x3(name)?Generate_SFN(name):name;
    if (in)	for (std::string file; in >> file; ) {
        if (strlen(dir)>2&&dir[0]=='/'&&dir[strlen(dir)-1]=='/') {
            strncpy(fullname, dir+1, strlen(dir)-2);
            *(fullname+strlen(dir)-2)=0;
            strcat(fullname, "\\");
            strcpy(fullsname, "\\");
            strcat(fullsname, sname.c_str());
            strcat(fullname, name);
        } else {
            strcpy(fullname, name);
            strcpy(fullsname, sname.c_str());
        }
        if (!strcasecmp(fullname,file.c_str())||!strcasecmp(("\""+std::string(fullname)+"\"").c_str(),file.c_str())
        || !strcasecmp(fullsname,file.c_str())||!strcasecmp(("\""+std::string(fullsname)+"\"").c_str(),file.c_str()))
            return;
        if (!strcasecmp(("/"+std::string(fullname)).c_str(),file.c_str())||!strcasecmp(("/\""+std::string(fullname)+"\"").c_str(),file.c_str())
        || !strcasecmp(("/"+std::string(fullsname)).c_str(),file.c_str())||!strcasecmp(("/\""+std::string(fullsname)+"\"").c_str(),file.c_str())) {
            hidden=true;
            break;
        }
    }
    strcpy(vfnames[vfpos],name);
    strcpy(vfsnames[vfpos],sname.c_str());
    if (!strlen(trim(vfnames[vfpos]))||!strlen(trim(vfsnames[vfpos]))) return;
	VFILE_Block * new_file=new VFILE_Block;
	new_file->name=vfsnames[vfpos];
	new_file->lname=vfnames[vfpos];
    vfpos++;
	new_file->data=data;
	new_file->size=size;
	new_file->date=fztime||fzdate?fzdate:DOS_PackDate(2002,10,1);
	new_file->time=fztime||fzdate?fztime:DOS_PackTime(12,34,56);
	new_file->onpos=onpos;
	new_file->isdir=isdir;
	new_file->hidden=hidden;
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
	Virtual_File(uint8_t * in_data,uint32_t in_size);
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * new_pos,uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
private:
	uint32_t file_size;
    uint32_t file_pos = 0;
	uint8_t * file_data;
};


Virtual_File::Virtual_File(uint8_t* in_data, uint32_t in_size) : file_size(in_size), file_data(in_data) {
	date=DOS_PackDate(2002,10,1);
	time=DOS_PackTime(12,34,56);
	open=true;
}

bool Virtual_File::Read(uint8_t * data,uint16_t * size) {
	uint32_t left=file_size-file_pos;
	if (left<=*size) { 
		memcpy(data,&file_data[file_pos],left);
		*size=(uint16_t)left;
	} else {
		memcpy(data,&file_data[file_pos],*size);
	}
	file_pos+=*size;
	return true;
}

bool Virtual_File::Write(const uint8_t * data,uint16_t * size){
    (void)data;//UNUSED
    (void)size;//UNUSED
	/* Not really writable */
	return false;
}

bool Virtual_File::Seek(uint32_t * new_pos,uint32_t type){
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


uint16_t Virtual_File::GetInformation(void) {
	return 0x40;	// read-only drive
}


Virtual_Drive::Virtual_Drive() {
	strcpy(info,"Internal Virtual Drive");
	for (int i=0; i<256; i++) {lfn_id[i] = 0;lfn_search[i] = 0;}
    const Section_prop * section=static_cast<Section_prop *>(control->GetSection("dos"));
    hidefiles = section->Get_string("drive z hide files");
}


bool Virtual_Drive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
    /* Scan through the internal list of files */
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0)) {
			/* We have a match */
			*file=new Virtual_File(cur_file->data,cur_file->size);
			(*file)->flags=flags;
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileCreate(DOS_File * * file,const char * name,uint16_t attributes) {
    (void)file;//UNUSED
    (void)name;//UNUSED
    (void)attributes;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::FileUnlink(const char * name) {
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0)) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::RemoveDir(const char * dir) {
    (void)dir;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::MakeDir(const char * dir) {
    (void)dir;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::TestDir(const char * dir) {
	if (!dir[0]) return true;		//root directory
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (cur_file->isdir&&(!strcasecmp(cur_file->name, dir)||!strcasecmp(cur_file->lname, dir))) return true;
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileStat(const char* name, FileStat_Block * const stat_block){
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0)) {
			stat_block->attr=cur_file->isdir?(cur_file->hidden?DOS_ATTR_DIRECTORY|DOS_ATTR_HIDDEN:DOS_ATTR_DIRECTORY):(cur_file->hidden?DOS_ATTR_ARCHIVE|DOS_ATTR_HIDDEN:DOS_ATTR_ARCHIVE);
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
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0))
            return true;
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
    unsigned int onpos=0;
    if (*_dir) for (unsigned int i=1; i<vfpos; i++) {
        if (!strcasecmp(vfsnames[i], _dir)||!strcasecmp(vfnames[i], _dir)) {
            onpos=i;
            break;
        }
    }
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) {
		dta.SetDirID(onpos);
		search_file=first_file;
	} else {
		lfn_id[lfn_filefind_handle]=onpos;
		lfn_search[lfn_filefind_handle]=first_file;
	}
	uint8_t attr;char pattern[CROSS_LEN];
    dta.GetSearchParams(attr,pattern,uselfn);
	if (attr == DOS_ATTR_VOLUME) {
		dta.SetResult(GetLabel(),GetLabel(),0,0,0,DOS_ATTR_VOLUME);
		return true;
	} else if ((attr & DOS_ATTR_VOLUME) && !fcb_findfirst) {
		if (WildFileCmp(GetLabel(),pattern)) {
			dta.SetResult(GetLabel(),GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	} else if ((attr & DOS_ATTR_DIRECTORY) && onpos>0) {
		if (WildFileCmp("..",pattern)) {
			dta.SetResult("..","..",0,DOS_PackDate(2002,10,1),DOS_PackTime(12,34,56),DOS_ATTR_DIRECTORY);
			return true;
		}
	}
	return FindNext(dta);
}

bool Virtual_Drive::FindNext(DOS_DTA & dta) {
	uint8_t attr;char pattern[CROSS_LEN];
	dta.GetSearchParams(attr,pattern,uselfn);
    unsigned int pos=lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():lfn_id[lfn_filefind_handle];

	if (lfn_filefind_handle>=LFN_FILEFIND_MAX)
		while (search_file) {
			if (pos==search_file->onpos&&(WildFileCmp(search_file->name,pattern)||LWildFileCmp(search_file->lname,pattern))) {
				dta.SetResult(search_file->name,search_file->lname,search_file->size,search_file->date,search_file->time,search_file->isdir?(search_file->hidden?DOS_ATTR_DIRECTORY|DOS_ATTR_HIDDEN:DOS_ATTR_DIRECTORY):(search_file->hidden?DOS_ATTR_ARCHIVE|DOS_ATTR_HIDDEN:DOS_ATTR_ARCHIVE));
				search_file=search_file->next;
				return true;
			}
			search_file=search_file->next;
		}
	else
		while (lfn_search[lfn_filefind_handle]) {
			if (pos==lfn_search[lfn_filefind_handle]->onpos&&(WildFileCmp(lfn_search[lfn_filefind_handle]->name,pattern)||LWildFileCmp(lfn_search[lfn_filefind_handle]->lname,pattern))) {
				dta.SetResult(lfn_search[lfn_filefind_handle]->name,lfn_search[lfn_filefind_handle]->lname,lfn_search[lfn_filefind_handle]->size,lfn_search[lfn_filefind_handle]->date,lfn_search[lfn_filefind_handle]->time,lfn_search[lfn_filefind_handle]->isdir?(lfn_search[lfn_filefind_handle]->hidden?DOS_ATTR_DIRECTORY|DOS_ATTR_HIDDEN:DOS_ATTR_DIRECTORY):(lfn_search[lfn_filefind_handle]->hidden?DOS_ATTR_ARCHIVE|DOS_ATTR_HIDDEN:DOS_ATTR_ARCHIVE));
				lfn_search[lfn_filefind_handle]=lfn_search[lfn_filefind_handle]->next;
				return true;
			}
			lfn_search[lfn_filefind_handle]=lfn_search[lfn_filefind_handle]->next;
		}
	if (lfn_filefind_handle<LFN_FILEFIND_MAX) {
		lfn_id[lfn_filefind_handle]=0;
		lfn_search[lfn_filefind_handle]=0;
	}
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool Virtual_Drive::SetFileAttr(const char * name,uint16_t attr) {
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return true;
	}
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0)) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		cur_file=cur_file->next;
	}
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
}

bool Virtual_Drive::GetFileAttr(const char * name,uint16_t * attr) {
	if (*name == 0) {
		*attr=DOS_ATTR_DIRECTORY;
		return true;
	}
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0)) {
			*attr = cur_file->isdir?(cur_file->hidden?DOS_ATTR_DIRECTORY|DOS_ATTR_HIDDEN:DOS_ATTR_DIRECTORY):(cur_file->hidden?DOS_ATTR_ARCHIVE|DOS_ATTR_HIDDEN:DOS_ATTR_ARCHIVE);	//Maybe readonly ?
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
	if (*oldname == 0 || *newname == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(oldname,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            strcasecmp(oldname,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(oldname,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(oldname,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0)) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) {
	*_bytes_sector=512;
	*_sectors_cluster=32;
	*_total_clusters=32765;	// total size is always 500 mb
	*_free_clusters=0;		// nothing free here
	return true;
}

uint8_t Virtual_Drive::GetMediaByte(void) {
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
	char psp_name[9];
	DOS_MCB psp_mcb(dos.psp()-1);
	psp_mcb.GetFileName(psp_name);
	if (!strcmp(psp_name, "SCANDISK") || !strcmp(psp_name, "CHKDSK")) {
		/* Check for SCANDISK.EXE (or CHKDSK.EXE) and return true (Wengier) */
		return true;
	}
	/* Automatically detect if called by SCANDISK.EXE even if it is renamed (tested with the program from MS-DOS 6.20 to Windows ME) */
    if (dos.version.major >= 5 && reg_sp >=0x4000 && mem_readw(SegPhys(ss)+reg_sp)/0x100 == 0x1 && mem_readw(SegPhys(ss)+reg_sp+2)/0x100 >= 0xB && mem_readw(SegPhys(ss)+reg_sp+2)/0x100 <= 0x12)
		return true;

	return false;
}

bool Virtual_Drive::isRemovable(void) {
	return false;
}

Bits Virtual_Drive::UnMount(void) {
	return 1;
}

char const* Virtual_Drive::GetLabel(void) {
	return "DOSBOX-X";
}
