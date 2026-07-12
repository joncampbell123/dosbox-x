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
#include <vector>

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "logging.h"
#include "support.h"
#include "control.h"
#include "cross.h"
#include "regs.h"

extern bool gbk, isDBCSCP(), isKanji1_gbk(uint8_t chr), shiftjis_lead_byte(int c);

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
	bool intprog;
	VFILE_Block * next;
};

#define MAX_VFILES 500
unsigned int vfpos=1, lfn_id[256];
bool internal_program = false, skipintprog = false;
char sfn[DOS_NAMELENGTH_ASCII],vfnames[MAX_VFILES][CROSS_LEN],vfsnames[MAX_VFILES][DOS_NAMELENGTH_ASCII];
static VFILE_Block * first_file, * lfn_search[256], * parent_dir = NULL;

extern int lfn_filefind_handle;
extern bool filename_not_8x3(const char *n), filename_not_strict_8x3(const char *n);
extern void Add_VFiles(bool usecp), PROGRAMS_Shutdown(void);
extern char * DBCS_upcase(char * str);
std::string hidefiles="";

/* Generate 8.3 names from LFNs, with tilde usage (from ~1 to ~999999). */
void GenerateSFN(char *lfn, unsigned int k, unsigned int &i, unsigned int &t) {
    char *n=lfn;
    if (t>strlen(n)||k==1||k==10||k==100||k==1000||k==10000||k==100000) {
        i=0;
        *sfn=0;
        while (*n == '.'||*n == ' ') n++;
        while (strlen(n)&&(*(n+strlen(n)-1)=='.'||*(n+strlen(n)-1)==' ')) *(n+strlen(n)-1)=0;
        bool lead = false;
        unsigned int m = k<10?6u:(k<100?5u:(k<1000?4u:(k<10000?3u:(k<100000?2u:1u))));
        while (*n != 0 && *n != '.' && i < m) {
            if (*n == ' ') {
                n++;
                lead = false;
                continue;
            }
            if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*n & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*n & 0xFF)))) {
                if (i==m-1) break;
                sfn[i++]=*(n++);
                lead = true;
            } else if (*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||((*n=='['||*n==']'||*n=='|'||*n=='\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*n=='?'||*n=='*') {
                sfn[i++]='_';
                n++;
                lead = false;
            } else {
                sfn[i++]=lead?*n:toupper(*n);
                n++;
                lead = false;
            }
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
    } else if (k<10000) {
        sfn[i++]='0'+(k/1000);
        sfn[i++]='0'+((k%1000)/100);
        sfn[i++]='0'+((k%100)/10);
        sfn[i++]='0'+(k%10);
    } else if (k<100000) {
        sfn[i++]='0'+(k/10000);
        sfn[i++]='0'+((k%10000)/1000);
        sfn[i++]='0'+((k%1000)/100);
        sfn[i++]='0'+((k%100)/10);
        sfn[i++]='0'+(k%10);
    } else {
        sfn[i++]='0'+(k/100000);
        sfn[i++]='0'+((k%100000)/10000);
        sfn[i++]='0'+((k%10000)/1000);
        sfn[i++]='0'+((k%1000)/100);
        sfn[i++]='0'+((k%100)/10);
        sfn[i++]='0'+(k%10);
    }
    if (t>strlen(n)||k==1||k==10||k==100||k==1000||k==10000||k==100000) {
        char *p=strrchr(n, '.');
        if (p!=NULL) {
            sfn[i++]='.';
            n=p+1;
            while (*n == '.') n++;
            int j=0;
            bool lead = false;
            while (*n != 0 && j++<3) {
                if (*n == ' ') {
                    n++;
                    lead = false;
                    continue;
                }
                if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*n & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*n & 0xFF)))) {
                    if (j==3) break;
                    sfn[i++]=*(n++);
                    lead = true;
                } else if (*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||((*n=='['||*n==']'||*n=='|'||*n=='\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*n=='?'||*n=='*') {
                    sfn[i++]='_';
                    n++;
                    lead = false;
                } else {
                    sfn[i++]=lead?*n:toupper(*n);
                    n++;
                    lead = false;
                }
            }
        }
        sfn[i++]=0;
    }
}

char* VFILE_Generate_SFN(const char *name, unsigned int onpos) {
	if (!filename_not_8x3(name)) {
		strcpy(sfn, name);
		DBCS_upcase(sfn);
		return sfn;
	}
	char lfn[LFN_NAMELENGTH+1];
	if (name==NULL||!*name) return NULL;
	if (strlen(name)>LFN_NAMELENGTH) {
		strncpy(lfn, name, LFN_NAMELENGTH);
		lfn[LFN_NAMELENGTH]=0;
	} else
		strcpy(lfn, name);
	if (!strlen(lfn)) return NULL;
	unsigned int k=1, i, t=1000000;
	const VFILE_Block* cur_file;
	while (k<1000000) {
        GenerateSFN(lfn, k, i, t);
        cur_file = first_file;
        bool found=false;
        while (cur_file) {
            if (onpos==cur_file->onpos&&(strcasecmp(sfn,cur_file->name)==0||(uselfn&&strcasecmp(sfn,cur_file->lname)==0))) {found=true;break;}
            cur_file=cur_file->next;
        }
        if (!found) return sfn;
		k++;
	}
	return nullptr;
}

void VFILE_Shutdown(void) {
	LOG(LOG_DOSMISC,LOG_DEBUG)("Shutting down VFILE system");

    if (parent_dir != NULL) {
        delete parent_dir;
        parent_dir = NULL;
    }
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
// Helper function for case-insensitive string comparison using std::string
bool case_insensitive_equal(const std::string& str1, const std::string& str2) {
    if(str1.size() != str2.size()) return false;
    return std::equal(str1.begin(), str1.end(), str2.begin(), [](unsigned char a, unsigned char b) {
        return std::tolower(a) == std::tolower(b);
        });
}

void VFILE_Register(const char* name, uint8_t* data, uint32_t size, const char* dir) {
    if(vfpos >= MAX_VFILES) return;

    std::string name_str(name);
    std::string dir_str(dir);

    std::istringstream in(hidefiles);
    bool hidden = false;

    bool isdir = (dir_str == "/" || name_str == "." || name_str == "..") || (data == nullptr && size == 0);

    unsigned int onpos = 0;
    char fullname[CROSS_LEN], fullsname[CROSS_LEN];

    static std::string last_normalized_dir = "";
    static unsigned int last_resolved_onpos = 0;

    std::string current_normalized_dir = dir_str;
    if(current_normalized_dir.front() == '/' || current_normalized_dir.front() == '\\') current_normalized_dir.erase(0, 1);
    if(!current_normalized_dir.empty() && (current_normalized_dir.back() == '/' || current_normalized_dir.back() == '\\')) current_normalized_dir.pop_back();

    // Improvement: To avoid catching its own name during the search, the global arrays (vfnames/vfsnames) 
    // are not modified at all until the parent directory lookup is completely resolved.
    if(dir_str.size() > 1) {
        if(!current_normalized_dir.empty() && current_normalized_dir == last_normalized_dir) {
            onpos = last_resolved_onpos;
        }
        else {
            std::string search_path = current_normalized_dir;

            if(!search_path.empty()) {
                std::vector<std::string> components;
                std::stringstream ss(search_path);
                std::string item;
                while(std::getline(ss, item, '/')) {
                    std::stringstream ss2(item);
                    std::string subitem;
                    while(std::getline(ss2, subitem, '\\')) {
                        if(!subitem.empty()) components.push_back(subitem);
                    }
                }

                unsigned int current_lookup_onpos = 0;

                for(const auto& comp : components) {
                    unsigned int next_onpos = 0;

                    // Safe because the search target is strictly limited to entries that have already been fully registered (less than the current vfpos).
                    for(unsigned int i = 1; i < vfpos; i++) {
                        if(strcasecmp(vfsnames[i], comp.c_str()) == 0 || strcasecmp(vfnames[i], comp.c_str()) == 0) {

                            const VFILE_Block* scan = first_file;
                            bool context_matched = false;
                            while(scan) {
                                if((scan->name == vfsnames[i] || scan->lname == vfnames[i]) &&
                                    scan->onpos == current_lookup_onpos) {
                                    context_matched = true;
                                    break;
                                }
                                scan = scan->next;
                            }

                            if(context_matched) {
                                next_onpos = i;
                                break;
                            }
                        }
                    }

                    if(next_onpos != 0) {
                        current_lookup_onpos = next_onpos;
                    }
                    else {
                        current_lookup_onpos = 0;
                        break;
                    }
                }

                onpos = current_lookup_onpos;

                if(onpos != 0) {
                    last_normalized_dir = current_normalized_dir;
                    last_resolved_onpos = onpos;
                }
            }
        }
    }

    // Deduplication check
    const VFILE_Block* cur_file = first_file;
    while(cur_file) {
        if(onpos == cur_file->onpos && case_insensitive_equal(name_str, cur_file->name)) return;
        cur_file = cur_file->next;
    }

    // Standard SFN generation
    std::string sname = filename_not_strict_8x3(name) ? VFILE_Generate_SFN(name, onpos) : name;

    if(in) {
        for(std::string file; in >> file; ) {
            if(dir_str.size() > 2 && dir_str.front() == '/' && dir_str.back() == '/') {
                strncpy(fullname, dir + 1, dir_str.size() - 2);
                *(fullname + dir_str.size() - 2) = 0;
                strcat(fullname, "\\");
                strcpy(fullsname, "\\");
                strcat(fullsname, sname.c_str());
                strcat(fullname, name);
            }
            else {
                strcpy(fullname, name);
                strcpy(fullsname, sname.c_str());
            }
            if(case_insensitive_equal(fullname, file) || case_insensitive_equal("\"" + std::string(fullname) + "\"", file) ||
                case_insensitive_equal(fullsname, file) || case_insensitive_equal("\"" + std::string(fullsname) + "\"", file))
                return;
            if(case_insensitive_equal("/" + std::string(fullname), file) || case_insensitive_equal("/\"" + std::string(fullname) + "\"", file) ||
                case_insensitive_equal("/" + std::string(fullsname), file) || case_insensitive_equal("/\"" + std::string(fullsname) + "\"", file)) {
                hidden = true;
                break;
            }
        }
    }

    std::string trimmed_name(name);
    std::string trimmed_sname = sname;
    trim(trimmed_name);
    trim(trimmed_sname);
    if(trimmed_name.empty() || trimmed_sname.empty()) return;

    // Safely register its own information to the global arrays only after the parent has been successfully resolved.
    unsigned int assigned_index = vfpos;
    strcpy(vfnames[assigned_index], name);
    strcpy(vfsnames[assigned_index], sname.c_str());

    VFILE_Block* new_file = new VFILE_Block;
    new_file->name = vfsnames[assigned_index];
    new_file->lname = vfnames[assigned_index];
    vfpos++;

    new_file->intprog = internal_program;
    new_file->data = data;
    new_file->size = size;
    new_file->date = fztime || fzdate ? fzdate : DOS_PackDate(2002, 10, 1);
    new_file->time = fztime || fzdate ? fztime : DOS_PackTime(12, 34, 56);
    new_file->onpos = onpos;
    new_file->isdir = isdir;
    new_file->hidden = hidden;

    new_file->next = first_file;
    first_file = new_file;

    //LOG_MSG("[DEBUG Register] SUCCESS: Created entry '%s' at array slot index: %u (Parent onpos context: %u, isdir: %d)",
    //    name, assigned_index, new_file->onpos, new_file->isdir);
}

void VFILE_Remove(const char *name,const char *dir = "") {
    unsigned int onpos=0;
    if (*dir) {
        for (unsigned int i=1; i<vfpos; i++)
            if (!strcasecmp(vfsnames[i], dir)||!strcasecmp(vfnames[i], dir)) {
                onpos=i;
                break;
            }
        if (onpos==0) return;
    }
	VFILE_Block * chan=first_file;
	VFILE_Block * * where=&first_file;
	while (chan) {
		if (onpos==chan->onpos && (strcmp(name,chan->name) == 0 || strcmp(name,chan->lname) == 0)) {
			*where = chan->next;
			if(chan == first_file) first_file = chan->next;
			delete chan;
			return;
		}
		where=&chan->next;
		chan=chan->next;
	}
}

uint32_t VFILE_GetCPIData(const char *filename, std::vector<uint8_t> &cpibuf) {
    if (!*filename) return 0;
    unsigned int onpos=0;
    for (unsigned int i=1; i<vfpos; i++)
        if (!strcasecmp(vfsnames[i], "CPI")||!strcasecmp(vfnames[i], "CPI")) {
            onpos=i;
            break;
        }
    if (onpos==0) return 0;
	VFILE_Block * chan=first_file;
	while (chan) {
		if (onpos==chan->onpos && (strcmp(filename,chan->name) == 0 || strcmp(filename,chan->lname) == 0)) {
            if (chan->size>65535) return 0;
            for (size_t bct=0; bct<chan->size; bct++) cpibuf.push_back(chan->data[bct]);
            return chan->size;
		}
		chan=chan->next;
	}
    return 0;
}

class Virtual_File : public DOS_File {
public:
	Virtual_File(uint8_t * in_data,uint32_t in_size);
	bool Read(uint8_t * data,uint16_t * size) override;
	bool Write(const uint8_t * data,uint16_t * size) override;
	bool Seek(uint32_t * new_pos,uint32_t type) override;
	bool Close() override;
	uint16_t GetInformation(void) override;
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
	return DeviceInfoFlags::NotWritten;  // read-only drive
}


Virtual_Drive::Virtual_Drive() {
	strcpy(info,"Internal Virtual Drive");
	for (int i=0; i<256; i++) {lfn_id[i] = 0;lfn_search[i] = nullptr;}
    const Section_prop * section=static_cast<Section_prop *>(control->GetSection("dos"));
    hidefiles = section->Get_string("drive z hide files");
    if (parent_dir == NULL) parent_dir = new VFILE_Block;
}

bool Virtual_Drive::FileOpen(DOS_File** file, const char* name, uint32_t flags) {
    if(*name == 0) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }

    std::string path_str(name);
    if(path_str.size() >= 2 && path_str[1] == ':') {
        path_str.erase(0, 2);
    }

    unsigned int current_lookup_onpos = 0;
    if(!path_str.empty() && (path_str.front() == '\\' || path_str.front() == '/')) {
        current_lookup_onpos = 0;
        while(!path_str.empty() && (path_str.front() == '\\' || path_str.front() == '/')) {
            path_str.erase(0, 1);
        }
    }

    std::string target_file_name = path_str;
    std::string dir_part = "";
    size_t last_slash = path_str.find_last_of("\\/");
    if(last_slash != std::string::npos) {
        dir_part = path_str.substr(0, last_slash);
        target_file_name = path_str.substr(last_slash + 1);
    }

    if(!dir_part.empty()) {
        std::vector<std::string> components;
        std::stringstream ss(dir_part);
        std::string item;
        while(std::getline(ss, item, '\\')) {
            std::stringstream ss2(item);
            std::string subitem;
            while(std::getline(ss2, subitem, '/')) {
                if(!subitem.empty()) components.push_back(subitem);
            }
        }

        for(const auto& comp : components) {
            unsigned int next_onpos = 0;
            const VFILE_Block* scan = first_file;

            while(scan) {
                // Fix: Find the correct structural block whose parent ID (onpos) and name match exactly.
                if(scan->onpos == current_lookup_onpos &&
                    (!strcasecmp(scan->name, comp.c_str()) || !strcasecmp(scan->lname, comp.c_str()))) {

                    // Fix: Instead of a simple pointer comparison loop, accurately reverse-lookup the index 
                    // in the global arrays (vfnames/vfsnames) that the scan object points to.
                    for(unsigned int i = 1; i < vfpos; i++) {
                        if((vfsnames[i] == scan->name || vfnames[i] == scan->lname) &&
                            (vfsnames[i] != nullptr)) {

                            // To be absolutely safe, verify that the parent of the VFILE_Block corresponding to 
                            // this array index [i] matches the current layer being searched to prevent any false positives.
                            const VFILE_Block* verify = first_file;
                            while(verify) {
                                if((verify->name == vfsnames[i] || verify->lname == vfnames[i]) &&
                                    verify->onpos == current_lookup_onpos) {
                                    next_onpos = i;
                                    break;
                                }
                                verify = verify->next;
                            }
                            if(next_onpos != 0) break;
                        }
                    }
                    if(next_onpos != 0) break;
                }
                scan = scan->next;
            }

            if(next_onpos != 0) {
                current_lookup_onpos = next_onpos;
            }
            else {
                current_lookup_onpos = 0;
                break;
            }
        }
    }

    const VFILE_Block* cur_file = first_file;
    while(cur_file) {
        if(cur_file->onpos == current_lookup_onpos) {
            bool match = (strcasecmp(target_file_name.c_str(), cur_file->name) == 0) ||
                (strcasecmp(target_file_name.c_str(), cur_file->lname) == 0);

            if(match) {
                Virtual_File* vfile = new Virtual_File(cur_file->data, cur_file->size);
                vfile->SetName(cur_file->name);
                vfile->flags = flags;
                *file = vfile;
                return true;
            }
        }
        cur_file = cur_file->next;
    }

    DOS_SetError(DOSERR_FILE_NOT_FOUND);
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
            (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0))) {
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

bool Virtual_Drive::TestDir(const char* fulldir) {
#define ENABLE_LOGGING 0
#if ENABLE_LOGGING
    LOG_MSG("--- [DEBUG TestDir Start] ---");
    LOG_MSG("Target fulldir: '%s'", fulldir ? fulldir : "NULL");
#endif
    if(!fulldir || fulldir[0] == '\0' || (fulldir[0] == '\\' && fulldir[1] == '\0')) {
#if ENABLE_LOGGING
        LOG_MSG("[DEBUG] Root directory matched directly. Return true.");
#endif

        return true;
    }

    std::vector<std::string> path_components;
    std::stringstream ss(fulldir);
    std::string component;
    while(std::getline(ss, component, '\\')) {
        if(!component.empty()) {
            path_components.push_back(component);
        }
    }

#if ENABLE_LOGGING
    LOG_MSG("[DEBUG] Path components count: %d", (int)path_components.size());
#endif

    unsigned int current_onpos = 0;

    for(size_t depth = 0; depth < path_components.size(); ++depth) {
        const auto& dir_name = path_components[depth];
        bool match_found = false;
        unsigned int next_onpos = 0;
        const VFILE_Block* scan = first_file;
#if ENABLE_LOGGING
        LOG_MSG("[DEBUG] Layer %d: Searching for '%s' under parent onpos: %u", (int)depth, dir_name.c_str(), current_onpos);
#endif
        while(scan) {
            // Check context parent mapping and match string contents first
            if(scan->onpos == current_onpos &&
                (case_insensitive_equal(scan->name, dir_name) ||
                    case_insensitive_equal(scan->lname, dir_name))) {
#if ENABLE_LOGGING
                LOG_MSG("    => Target Structure Block Found! Resolving index safely...");
#endif
                //  POINTER REFERENCE FIX: Find the unique slot index by matching 
                // raw memory addresses instead of string value iterations.
                for(unsigned int i = 1; i < vfpos; i++) {
                    if(vfsnames[i] == scan->name || vfnames[i] == scan->lname) {
                        next_onpos = i;
                        break;
                    }
                }
                if(next_onpos != 0) break;
            }
            scan = scan->next;
        }

        if(next_onpos != 0) {
#if ENABLE_LOGGING
            LOG_MSG("      => Target index found. current_onpos updated: %u -> %u", current_onpos, next_onpos);
#endif
            current_onpos = next_onpos;
            match_found = true;
        }

        if(!match_found) {
#if ENABLE_LOGGING
            LOG_MSG("[DEBUG TestDir End] FAILED at layer %d ('%s'). Return false.", (int)depth, dir_name.c_str());
#endif
            return false;
        }
    }
#if ENABLE_LOGGING
    LOG_MSG("[DEBUG TestDir End] SUCCESS! Path verified. Return true.");
#endif
    return true;
#undef ENABLE_LOGGING
}
bool Virtual_Drive::FileStat(const char* name, FileStat_Block * const stat_block){
    const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0))) {
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

bool Virtual_Drive::FileExists(const char* name) {
    if(*name == 0) return false;

    std::string path_str(name);
    if(path_str.size() >= 2 && path_str[1] == ':') {
        path_str.erase(0, 2);
    }

    unsigned int current_lookup_onpos = 0;
    if(!path_str.empty() && (path_str.front() == '\\' || path_str.front() == '/')) {
        current_lookup_onpos = 0;
        while(!path_str.empty() && (path_str.front() == '\\' || path_str.front() == '/')) {
            path_str.erase(0, 1);
        }
    }

    std::string target_file_name = path_str;
    std::string dir_part = "";
    size_t last_slash = path_str.find_last_of("\\/");
    if(last_slash != std::string::npos) {
        dir_part = path_str.substr(0, last_slash);
        target_file_name = path_str.substr(last_slash + 1);
    }

    // 1. Correctly resolve the directory hierarchy to identify the final onpos, which will be the parent of the target file.
    if(!dir_part.empty()) {
        std::vector<std::string> components;
        std::stringstream ss(dir_part);
        std::string item;
        while(std::getline(ss, item, '\\')) {
            std::stringstream ss2(item);
            std::string subitem;
            while(std::getline(ss2, subitem, '/')) {
                if(!subitem.empty()) components.push_back(subitem);
            }
        }

        for(const auto& comp : components) {
            unsigned int next_onpos = 0;
            const VFILE_Block* scan = first_file;

            while(scan) {
                if(scan->onpos == current_lookup_onpos &&
                    (!strcasecmp(scan->name, comp.c_str()) || !strcasecmp(scan->lname, comp.c_str()))) {

                    for(unsigned int i = 1; i < vfpos; i++) {
                        if((vfsnames[i] == scan->name || vfnames[i] == scan->lname) &&
                            (vfsnames[i] != nullptr)) {

                            const VFILE_Block* verify = first_file;
                            while(verify) {
                                if((verify->name == vfsnames[i] || verify->lname == vfnames[i]) &&
                                    verify->onpos == current_lookup_onpos) {
                                    next_onpos = i;
                                    break;
                                }
                                verify = verify->next;
                            }
                            if(next_onpos != 0) break;
                        }
                    }
                    if(next_onpos != 0) break;
                }
                scan = scan->next;
            }

            if(next_onpos != 0) {
                current_lookup_onpos = next_onpos;
            }
            else {
                current_lookup_onpos = 0;
                return false; // Return false if the intermediate directory is not found, as the file cannot exist.
            }
        }
    }

    // 2. Check if the target file exists under the identified parent (current_lookup_onpos).
    const VFILE_Block* cur_file = first_file;
    while(cur_file) {
        if(cur_file->onpos == current_lookup_onpos) {
            bool match = (strcasecmp(target_file_name.c_str(), cur_file->name) == 0) ||
                (strcasecmp(target_file_name.c_str(), cur_file->lname) == 0);
            if(match) {
                return !cur_file->isdir; // Return true if it is a file, not a directory.
            }
        }
        cur_file = cur_file->next;
    }

    return false;
}

bool Virtual_Drive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
    unsigned int onpos=0;
    if (*_dir) {
        if (FileExists(_dir)) {
            DOS_SetError(DOSERR_FILE_NOT_FOUND);
            return false;
        }

        // Fix: Instead of a simple name loop, correctly parse the passed path (dir) level by level to identify the onpos.
        std::string search_path(_dir);
        if(search_path.front() == '/' || search_path.front() == '\\') search_path.erase(0, 1);
        if(!search_path.empty() && (search_path.back() == '/' || search_path.back() == '\\')) search_path.pop_back();

        if(!search_path.empty()) {
            std::vector<std::string> components;
            std::stringstream ss(search_path);
            std::string item;
            while(std::getline(ss, item, '/')) {
                std::stringstream ss2(item);
                std::string subitem;
                while(std::getline(ss2, subitem, '\\')) {
                    if(!subitem.empty()) components.push_back(subitem);
                }
            }

            unsigned int current_lookup_onpos = 0;
            for(const auto& comp : components) {
                unsigned int next_onpos = 0;
                // Accurately search for an entry with the same name whose parent is the layer currently being searched (current_lookup_onpos).
                for(unsigned int i = 1; i < vfpos; i++) {
                    if(strcasecmp(vfsnames[i], comp.c_str()) == 0 || strcasecmp(vfnames[i], comp.c_str()) == 0) {
                        const VFILE_Block* scan = first_file;
                        bool context_matched = false;
                        while(scan) {
                            if((scan->name == vfsnames[i] || scan->lname == vfnames[i]) &&
                                scan->onpos == current_lookup_onpos) {
                                context_matched = true;
                                break;
                            }
                            scan = scan->next;
                        }
                        if(context_matched) {
                            next_onpos = i;
                            break;
                        }
                    }
                }

                if(next_onpos != 0) {
                    current_lookup_onpos = next_onpos;
                }
                else {
                    current_lookup_onpos = 0;
                    break;
                }
            }
            onpos = current_lookup_onpos;
        }
        if (!onpos) {
            DOS_SetError(DOSERR_PATH_NOT_FOUND);
            return false;
        }
    }
	uint8_t attr;char pattern[CROSS_LEN];
	dta.GetSearchParams(attr,pattern,false);
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) {
		dta.SetDirID(onpos);
		search_file=(attr & DOS_ATTR_DIRECTORY) && onpos>0?parent_dir:first_file;
	} else {
		lfn_id[lfn_filefind_handle]=onpos;
		lfn_search[lfn_filefind_handle]=(attr & DOS_ATTR_DIRECTORY) && onpos>0?parent_dir:first_file;
	}
	if (attr == DOS_ATTR_VOLUME) {
		dta.SetResult(GetLabel(),GetLabel(),0,0,0,0,DOS_ATTR_VOLUME);
		return true;
	} else if ((attr & DOS_ATTR_DIRECTORY) && onpos>0) {
		if (WildFileCmp(".",pattern)) {
			dta.SetResult(".",".",0,0,DOS_PackDate(2002,10,1),DOS_PackTime(12,34,56),DOS_ATTR_DIRECTORY);
			return true;
		}
	} else if ((attr & DOS_ATTR_VOLUME) && !fcb_findfirst) {
		if (WildFileCmp(GetLabel(),pattern)) {
			dta.SetResult(GetLabel(),GetLabel(),0,0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	}
	return FindNext(dta);
}

bool Virtual_Drive::FindNext(DOS_DTA & dta) {
	uint8_t attr;char pattern[CROSS_LEN];
	dta.GetSearchParams(attr,pattern,false);
    unsigned int pos=lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():lfn_id[lfn_filefind_handle];

    if ((lfn_filefind_handle>=LFN_FILEFIND_MAX&&search_file==parent_dir) || (lfn_filefind_handle<LFN_FILEFIND_MAX&&lfn_search[lfn_filefind_handle]==parent_dir)) {
        bool cmp=WildFileCmp("..",pattern);
        if (cmp) dta.SetResult("..","..",0,0,DOS_PackDate(2002,10,1),DOS_PackTime(12,34,56),DOS_ATTR_DIRECTORY);
        if (lfn_filefind_handle>=LFN_FILEFIND_MAX)
            search_file=first_file;
        else
            lfn_search[lfn_filefind_handle] = first_file;
        if(cmp) return true;
    }

    if(lfn_filefind_handle >= LFN_FILEFIND_MAX) {
        while(search_file) {
            // Fix: Now that the data structure is clean, we only need to enumerate entries where the parent ID (onpos) matches the current position (pos).
            // All previous ad-hoc guards that forcibly excluded directories with the same name can be safely removed.
            bool is_current_dir = (pos == search_file->onpos);

            if(!(skipintprog && search_file->intprog) &&
                is_current_dir &&
                ((attr & DOS_ATTR_DIRECTORY) || !search_file->isdir) &&
                (WildFileCmp(search_file->name, pattern) || LWildFileCmp(search_file->lname, pattern)))
            {
                dta.SetResult(search_file->name, search_file->lname, search_file->size, 0, search_file->date, search_file->time,
                    search_file->isdir ? (search_file->hidden ? DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN : DOS_ATTR_DIRECTORY)
                    : (search_file->hidden ? DOS_ATTR_ARCHIVE | DOS_ATTR_HIDDEN : DOS_ATTR_ARCHIVE));
                search_file = search_file->next;
                return true;
            }
            search_file = search_file->next;
        }
    }
    else {
        while(lfn_search[lfn_filefind_handle]) {
            bool is_current_dir = (pos == lfn_search[lfn_filefind_handle]->onpos);

            if(!(skipintprog && lfn_search[lfn_filefind_handle]->intprog) &&
                is_current_dir &&
                ((attr & DOS_ATTR_DIRECTORY) || !lfn_search[lfn_filefind_handle]->isdir) &&
                (WildFileCmp(lfn_search[lfn_filefind_handle]->name, pattern) || LWildFileCmp(lfn_search[lfn_filefind_handle]->lname, pattern)))
            {
                dta.SetResult(lfn_search[lfn_filefind_handle]->name, lfn_search[lfn_filefind_handle]->lname, lfn_search[lfn_filefind_handle]->size, 0, lfn_search[lfn_filefind_handle]->date, lfn_search[lfn_filefind_handle]->time,
                    lfn_search[lfn_filefind_handle]->isdir ? (lfn_search[lfn_filefind_handle]->hidden ? DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN : DOS_ATTR_DIRECTORY)
                    : (lfn_search[lfn_filefind_handle]->hidden ? DOS_ATTR_ARCHIVE | DOS_ATTR_HIDDEN : DOS_ATTR_ARCHIVE));
                lfn_search[lfn_filefind_handle] = lfn_search[lfn_filefind_handle]->next;
                return true;
            }
            lfn_search[lfn_filefind_handle] = lfn_search[lfn_filefind_handle]->next;
        }
    }
    if(lfn_filefind_handle < LFN_FILEFIND_MAX) {
        lfn_id[lfn_filefind_handle] = 0;
        lfn_search[lfn_filefind_handle] = nullptr;
    }
    DOS_SetError(DOSERR_NO_MORE_FILES);
    return false;
}

bool Virtual_Drive::SetFileAttr(const char * name,uint16_t attr) {
    (void)attr;//UNUSED
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return true;
	}
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		unsigned int onpos=cur_file->onpos;
		if (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||(uselfn&&
            (strcasecmp(name,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(name,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0))) {
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

    std::string path_str(name);
    if(path_str.size() >= 2 && path_str[1] == ':') {
        path_str.erase(0, 2);
    }

    unsigned int current_lookup_onpos = 0;
    if(!path_str.empty() && (path_str.front() == '\\' || path_str.front() == '/')) {
        current_lookup_onpos = 0;
        while(!path_str.empty() && (path_str.front() == '\\' || path_str.front() == '/')) {
            path_str.erase(0, 1);
        }
    }

    std::string target_name = path_str;
    std::string dir_part = "";
    size_t last_slash = path_str.find_last_of("\\/");
    if(last_slash != std::string::npos) {
        dir_part = path_str.substr(0, last_slash);
        target_name = path_str.substr(last_slash + 1);
    }

    if(!dir_part.empty()) {
        std::vector<std::string> components;
        std::stringstream ss(dir_part);
        std::string item;
        while(std::getline(ss, item, '\\')) {
            std::stringstream ss2(item);
            std::string subitem;
            while(std::getline(ss2, subitem, '/')) {
                if(!subitem.empty()) components.push_back(subitem);
            }
        }

        for(const auto& comp : components) {
            unsigned int next_onpos = 0;
            const VFILE_Block* scan = first_file;

            while(scan) {
                if(scan->onpos == current_lookup_onpos &&
                    (!strcasecmp(scan->name, comp.c_str()) || !strcasecmp(scan->lname, comp.c_str()))) {

                    // Fix: Just like in FileOpen, strictly identify the correct index belonging to the proper hierarchy layer.
                    for(unsigned int i = 1; i < vfpos; i++) {
                        if((vfsnames[i] == scan->name || vfnames[i] == scan->lname) &&
                            (vfsnames[i] != nullptr)) {

                            const VFILE_Block* verify = first_file;
                            while(verify) {
                                if((verify->name == vfsnames[i] || verify->lname == vfnames[i]) &&
                                    verify->onpos == current_lookup_onpos) {
                                    next_onpos = i;
                                    break;
                                }
                                verify = verify->next;
                            }
                            if(next_onpos != 0) break;
                        }
                    }
                    if(next_onpos != 0) break;
                }
                scan = scan->next;
            }

            if(next_onpos != 0) {
                current_lookup_onpos = next_onpos;
            }
            else {
                current_lookup_onpos = 0;
                break;
            }
        }
    }

    const VFILE_Block* cur_file = first_file;
    while(cur_file) {
        if(cur_file->onpos == current_lookup_onpos) {
            bool match = (strcasecmp(target_name.c_str(), cur_file->name) == 0) ||
                (strcasecmp(target_name.c_str(), cur_file->lname) == 0);

            if(match) {
                // Cleanup: Now that the data structure is perfectly clean, all ad-hoc fallback checks are completely unnecessary.
                *attr = cur_file->isdir ?
                    (cur_file->hidden ? DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN : DOS_ATTR_DIRECTORY) :
                    (cur_file->hidden ? DOS_ATTR_ARCHIVE | DOS_ATTR_HIDDEN : DOS_ATTR_ARCHIVE);
                return true;
            }
        }
        cur_file = cur_file->next;
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
            (strcasecmp(oldname,(std::string(onpos?vfsnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0||
            strcasecmp(oldname,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->name).c_str())==0||
            strcasecmp(oldname,(std::string(onpos?vfnames[onpos]+std::string(1, '\\'):"")+cur_file->lname).c_str())==0))) {
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

void Virtual_Drive::EmptyCache(void) {
	while (first_file != NULL) {
		VFILE_Block *n = first_file->next;
		delete first_file;
		first_file = n;
	}
    vfpos=1;
    PROGRAMS_Shutdown();
    Add_VFiles(true);
}
