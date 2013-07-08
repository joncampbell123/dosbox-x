/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: drive_cache.cpp,v 1.59 2009-04-16 12:28:30 qbix79 Exp $ */

#include "drives.h"
#include "dos_inc.h"
#include "support.h"
#include "cross.h"

// STL stuff
#include <vector>
#include <iterator>
#include <algorithm>

#if defined (WIN32)   /* Win 32 */
#define WIN32_LEAN_AND_MEAN        // Exclude rarely-used stuff from 
#include <windows.h>
#endif

#if defined (OS2)
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#include <os2.h>
#endif

int fileInfoCounter = 0;

bool SortByName(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	return strcmp(a->shortname,b->shortname)<0;
}

bool SortByNameRev(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	return strcmp(a->shortname,b->shortname)>0;
}

bool SortByDirName(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	// Directories first...
	if (a->isDir!=b->isDir) return (a->isDir>b->isDir);	
	return strcmp(a->shortname,b->shortname)<0;
}

bool SortByDirNameRev(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	// Directories first...
	if (a->isDir!=b->isDir) return (a->isDir>b->isDir);	
	return strcmp(a->shortname,b->shortname)>0;
}

DOS_Drive_Cache::DOS_Drive_Cache(void) {
	dirBase			= new CFileInfo;
	save_dir		= 0;
	srchNr			= 0;
	label[0]		= 0;
	nextFreeFindFirst	= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) { dirSearch[i] = 0; free[i] = true; dirFindFirst[i] = 0; };
	SetDirSort(DIRALPHABETICAL);
	updatelabel = true;
}

DOS_Drive_Cache::DOS_Drive_Cache(const char* path) {
	dirBase			= new CFileInfo;
	save_dir		= 0;
	srchNr			= 0;
	label[0]		= 0;
	nextFreeFindFirst	= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) { dirSearch[i] = 0; free[i] = true; dirFindFirst[i] = 0; };
	SetDirSort(DIRALPHABETICAL);
	SetBaseDir(path);
	updatelabel = true;
}

DOS_Drive_Cache::~DOS_Drive_Cache(void) {
	Clear();
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) { delete dirFindFirst[i]; dirFindFirst[i]=0; };
}

void DOS_Drive_Cache::Clear(void) {
	delete dirBase; dirBase = 0;
	nextFreeFindFirst	= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) dirSearch[i] = 0;
}

void DOS_Drive_Cache::EmptyCache(void) {
	// Empty Cache and reinit
	Clear();
	dirBase		= new CFileInfo;
	save_dir	= 0;
	srchNr		= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) free[i] = true; 
	SetBaseDir(basePath);
}

void DOS_Drive_Cache::SetLabel(const char* vname,bool cdrom,bool allowupdate) {
/* allowupdate defaults to true. if mount sets a label then allowupdate is 
 * false and will this function return at once after the first call.
 * The label will be set at the first call. */

	if(!this->updatelabel) return;
	this->updatelabel = allowupdate;
	Set_Label(vname,label,cdrom);
	LOG(LOG_DOSMISC,LOG_NORMAL)("DIRCACHE: Set volume label to %s",label);
}

Bit16u DOS_Drive_Cache::GetFreeID(CFileInfo* dir) {
	for (Bit16u i=0; i<MAX_OPENDIRS; i++) if (free[i] || (dir==dirSearch[i])) return i;
	LOG(LOG_FILES,LOG_NORMAL)("DIRCACHE: Too many open directories!");
	return 0;
}

void DOS_Drive_Cache::SetBaseDir(const char* baseDir) {
	Bit16u id;
	strcpy(basePath,baseDir);
	if (OpenDir(baseDir,id)) {
		char* result = 0;
		ReadDir(id,result);
	};
	// Get Volume Label
#if defined (WIN32) || defined (OS2)
	bool cdrom = false;
	char labellocal[256]={ 0 };
	char drive[4] = "C:\\";
	drive[0] = basePath[0];
#if defined (WIN32)
	if (GetVolumeInformation(drive,labellocal,256,NULL,NULL,NULL,NULL,0)) {
	UINT test = GetDriveType(drive);
	if(test == DRIVE_CDROM) cdrom = true;
#else // OS2
	//TODO determine wether cdrom or not!
	FSINFO fsinfo;
	ULONG drivenumber = drive[0];
	if (drivenumber > 26) { // drive letter was lowercase
		drivenumber = drive[0] - 'a' + 1;
	}
	APIRET rc = DosQueryFSInfo(drivenumber, FSIL_VOLSER, &fsinfo, sizeof(FSINFO));
	if (rc == NO_ERROR) {
#endif
		/* Set label and allow being updated */
		SetLabel(labellocal,cdrom,true);
	}
#endif
}

void DOS_Drive_Cache::ExpandName(char* path) {
	strcpy(path,GetExpandName(path));
}

char* DOS_Drive_Cache::GetExpandName(const char* path) {
	static char work [CROSS_LEN] = { 0 };
	char dir [CROSS_LEN]; 

	work[0] = 0;
	strcpy (dir,path);

	const char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos) dir[pos-path+1] = 0;
	CFileInfo* dirInfo = FindDirInfo(dir, work);
		
	if (pos) {
		// Last Entry = File
		strcpy(dir,pos+1); 
		GetLongName(dirInfo, dir);
		strcat(work,dir);
	}

	if (*work) {
		size_t len = strlen(work);
#if defined (WIN32) 
		if((work[len-1] == CROSS_FILESPLIT ) && (len >= 2) && (work[len-2] != ':')) {
#else
		if((len > 1) && (work[len-1] == CROSS_FILESPLIT )) {
#endif       
			work[len-1] = 0; // Remove trailing slashes except when in root
		}
	}
	return work;
}

void DOS_Drive_Cache::AddEntry(const char* path, bool checkExists) {
	// Get Last part...
	char file	[CROSS_LEN];
	char expand	[CROSS_LEN];

	CFileInfo* dir = FindDirInfo(path,expand);
	const char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos) {
		strcpy(file,pos+1);	
		// Check if file already exists, then don't add new entry...
		if (checkExists) {
			if (GetLongName(dir,file)>=0) return;
		}

		CreateEntry(dir,file,false);

		Bits index = GetLongName(dir,file);
		if (index>=0) {
			Bit32u i;
			// Check if there are any open search dir that are affected by this...
			if (dir) for (i=0; i<MAX_OPENDIRS; i++) {
				if ((dirSearch[i]==dir) && ((Bit32u)index<=dirSearch[i]->nextEntry)) 
					dirSearch[i]->nextEntry++;
			}
		}
		//		LOG_DEBUG("DIR: Added Entry %s",path);
	} else {
//		LOG_DEBUG("DIR: Error: Failed to add %s",path);	
	}
}

void DOS_Drive_Cache::DeleteEntry(const char* path, bool ignoreLastDir) {
	CacheOut(path,ignoreLastDir);
	if (dirSearch[srchNr] && (dirSearch[srchNr]->nextEntry>0)) dirSearch[srchNr]->nextEntry--;

	if (!ignoreLastDir) {
		// Check if there are any open search dir that are affected by this...
		Bit32u i;
		char expand	[CROSS_LEN];
		CFileInfo* dir = FindDirInfo(path,expand);
		if (dir) for (i=0; i<MAX_OPENDIRS; i++) {
			if ((dirSearch[i]==dir) && (dirSearch[i]->nextEntry>0)) 
				dirSearch[i]->nextEntry--;
		}	
	}
}

void DOS_Drive_Cache::CacheOut(const char* path, bool ignoreLastDir) {
	char expand[CROSS_LEN] = { 0 };
	CFileInfo* dir;
	
	if (ignoreLastDir) {
		char tmp[CROSS_LEN] = { 0 };
		Bit32s len=0;
		const char* pos = strrchr(path,CROSS_FILESPLIT);
		if (pos) len = (Bit32s)(pos - path);
		if (len>0) { 
			safe_strncpy(tmp,path,len+1); 
		} else	{
			strcpy(tmp,path);
		}
		dir = FindDirInfo(tmp,expand);
	} else {
		dir = FindDirInfo(path,expand);	
	}

//	LOG_DEBUG("DIR: Caching out %s : dir %s",expand,dir->orgname);
	// delete file objects...
	for(Bit32u i=0; i<dir->fileList.size(); i++) {
		if (dirSearch[srchNr]==dir->fileList[i]) dirSearch[srchNr] = 0;
		delete dir->fileList[i]; dir->fileList[i] = 0;
	}
	// clear lists
	dir->fileList.clear();
	dir->longNameList.clear();
	save_dir = 0;
}

bool DOS_Drive_Cache::IsCachedIn(CFileInfo* curDir) {
	return (curDir->fileList.size()>0);
}


bool DOS_Drive_Cache::GetShortName(const char* fullname, char* shortname) {
	// Get Dir Info
	char expand[CROSS_LEN] = {0};
	CFileInfo* curDir = FindDirInfo(fullname,expand);

	std::vector<CFileInfo*>::size_type filelist_size = curDir->longNameList.size();
	if (GCC_UNLIKELY(filelist_size<=0)) return false;

	Bits low		= 0;
	Bits high		= (Bits)(filelist_size-1);
	Bits mid, res;

	while (low<=high) {
		mid = (low+high)/2;
		res = strcmp(fullname,curDir->longNameList[mid]->orgname);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; 
		else {
			strcpy(shortname,curDir->longNameList[mid]->shortname);
			return true;
		};
	}
	return false;
}

int DOS_Drive_Cache::CompareShortname(const char* compareName, const char* shortName) {
	char const* cpos = strchr(shortName,'~');
	if (cpos) {
/* the following code is replaced as it's not safe when char* is 64 bits */
/*		Bits compareCount1	= (int)cpos - (int)shortName;
		char* endPos		= strchr(cpos,'.');
		Bitu numberSize		= endPos ? int(endPos)-int(cpos) : strlen(cpos);
		
		char* lpos			= strchr(compareName,'.');
		Bits compareCount2	= lpos ? int(lpos)-int(compareName) : strlen(compareName);
		if (compareCount2>8) compareCount2 = 8;

		compareCount2 -= numberSize;
		if (compareCount2>compareCount1) compareCount1 = compareCount2;
*/
		size_t compareCount1 = strcspn(shortName,"~");
		size_t numberSize    = strcspn(cpos,".");
		size_t compareCount2 = strcspn(compareName,".");
		if(compareCount2 > 8) compareCount2 = 8;
		/* We want 
		 * compareCount2 -= numberSize;
		 * if (compareCount2>compareCount1) compareCount1 = compareCount2;
		 * but to prevent negative numbers: 
		 */
		if(compareCount2 > compareCount1 + numberSize)
			compareCount1 = compareCount2 - numberSize;
		return strncmp(compareName,shortName,compareCount1);
	}
	return strcmp(compareName,shortName);
}

Bitu DOS_Drive_Cache::CreateShortNameID(CFileInfo* curDir, const char* name) {
	std::vector<CFileInfo*>::size_type filelist_size = curDir->longNameList.size();
	if (GCC_UNLIKELY(filelist_size<=0)) return 1;	// shortener IDs start with 1

	Bitu foundNr	= 0;	
	Bits low		= 0;
	Bits high		= (Bits)(filelist_size-1);
	Bits mid, res;

	while (low<=high) {
		mid = (low+high)/2;
		res = CompareShortname(name,curDir->longNameList[mid]->shortname);
		
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; 
		else {
			// any more same x chars in next entries ?	
			do {
				foundNr = curDir->longNameList[mid]->shortNr;
				mid++;
			} while((Bitu)mid<curDir->longNameList.size() && (CompareShortname(name,curDir->longNameList[mid]->shortname)==0));
			break;
		};
	}
	return foundNr+1;
}

bool DOS_Drive_Cache::RemoveTrailingDot(char* shortname) {
// remove trailing '.' if no extension is available (Linux compatibility)
	size_t len = strlen(shortname);
	if (len && (shortname[len-1]=='.')) {
		if (len==1) return false;
		if ((len==2) && (shortname[0]=='.')) return false;
		shortname[len-1] = 0;	
		return true;
	}	
	return false;
}

Bits DOS_Drive_Cache::GetLongName(CFileInfo* curDir, char* shortName) {
	std::vector<CFileInfo*>::size_type filelist_size = curDir->fileList.size();
	if (GCC_UNLIKELY(filelist_size<=0)) return -1;

	// Remove dot, if no extension...
	RemoveTrailingDot(shortName);
	// Search long name and return array number of element
	Bits low	= 0;
	Bits high	= (Bits)(filelist_size-1);
	Bits mid,res;
	while (low<=high) {
		mid = (low+high)/2;
		res = strcmp(shortName,curDir->fileList[mid]->shortname);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; else
		{	// Found
			strcpy(shortName,curDir->fileList[mid]->orgname);
			return mid;
		};
	}
	// not available
	return -1;
}

bool DOS_Drive_Cache::RemoveSpaces(char* str) {
// Removes all spaces
	char*	curpos	= str;
	char*	chkpos	= str;
	while (*chkpos!=0) { 
		if (*chkpos==' ') chkpos++; else *curpos++ = *chkpos++; 
	}
	*curpos = 0;
	return (curpos!=chkpos);
}

void DOS_Drive_Cache::CreateShortName(CFileInfo* curDir, CFileInfo* info) {
	Bits	len			= 0;
	bool	createShort = false;

	char tmpNameBuffer[CROSS_LEN];

	char* tmpName = tmpNameBuffer;

	// Remove Spaces
	strcpy(tmpName,info->orgname);
	upcase(tmpName);
	createShort = RemoveSpaces(tmpName);

	// Get Length of filename
	char* pos = strchr(tmpName,'.');
	if (pos) {
		// ignore preceding '.' if extension is longer than "3"
		if (strlen(pos)>4) {
			while (*tmpName=='.') tmpName++;
			createShort = true;
		}
		pos = strchr(tmpName,'.');
		if (pos)	len = (Bits)(pos - tmpName);
		else		len = (Bits)strlen(tmpName);
	} else {
		len = (Bits)strlen(tmpName);
	}

	// Should shortname version be created ?
	createShort = createShort || (len>8);
	if (!createShort) {
		char buffer[CROSS_LEN];
		strcpy(buffer,tmpName);
		createShort = (GetLongName(curDir,buffer)>=0);
	}

	if (createShort) {
		// Create number
		char buffer[8];
		info->shortNr = CreateShortNameID(curDir,tmpName);
		sprintf(buffer,"%d",info->shortNr);
		// Copy first letters
		Bits tocopy = 0;
		size_t buflen = strlen(buffer);
		if (len+buflen+1>8)	tocopy = (Bits)(8 - buflen - 1);
		else				tocopy = len;
		safe_strncpy(info->shortname,tmpName,tocopy+1);
		// Copy number
		strcat(info->shortname,"~");
		strcat(info->shortname,buffer);
		// Add (and cut) Extension, if available
		if (pos) {
			// Step to last extension...
			pos = strrchr(tmpName, '.');
			// add extension
			strncat(info->shortname,pos,4);
			info->shortname[DOS_NAMELENGTH] = 0;
		}

		// keep list sorted for CreateShortNameID to work correctly
		if (curDir->longNameList.size()>0) {
			if (!(strcmp(info->shortname,curDir->longNameList.back()->shortname)<0)) {
				// append at end of list
				curDir->longNameList.push_back(info);
			} else {
				// look for position where to insert this element
				bool found=false;
				std::vector<CFileInfo*>::iterator it;
				for (it=curDir->longNameList.begin(); it!=curDir->longNameList.end(); ++it) {
					if (strcmp(info->shortname,(*it)->shortname)<0) {
						found = true;
						break;
					}
				}
				// Put it in longname list...
				if (found) curDir->longNameList.insert(it,info);
				else curDir->longNameList.push_back(info);
			}
		} else {
			// empty file list, append
			curDir->longNameList.push_back(info);
		}
	} else {
		strcpy(info->shortname,tmpName);
	}
	RemoveTrailingDot(info->shortname);
}

DOS_Drive_Cache::CFileInfo* DOS_Drive_Cache::FindDirInfo(const char* path, char* expandedPath) {
	// statics
	static char	split[2] = { CROSS_FILESPLIT,0 };
	
	char		dir  [CROSS_LEN]; 
	char		work [CROSS_LEN];
	const char*	start = path;
	const char*		pos;
	CFileInfo*	curDir = dirBase;
	Bit16u		id;

	if (save_dir && (strcmp(path,save_path)==0)) {
		strcpy(expandedPath,save_expanded);
		return save_dir;
	};

//	LOG_DEBUG("DIR: Find %s",path);

	// Remove base dir path
	start += strlen(basePath);
	strcpy(expandedPath,basePath);

	// hehe, baseDir should be cached in... 
	if (!IsCachedIn(curDir)) {
		strcpy(work,basePath);
		if (OpenDir(curDir,work,id)) {
			char buffer[CROSS_LEN];
			char* result = 0;
			strcpy(buffer,dirPath);
			ReadDir(id,result);
			strcpy(dirPath,buffer);
			free[id] = true;
		};
	};

	do {
//		bool errorcheck = false;
		pos = strchr(start,CROSS_FILESPLIT);
		if (pos) { safe_strncpy(dir,start,pos-start+1); /*errorcheck = true;*/ }
		else	 { strcpy(dir,start); };
 
		// Path found
		Bits nextDir = GetLongName(curDir,dir);
		strcat(expandedPath,dir);

		// Error check
/*		if ((errorcheck) && (nextDir<0)) {
			LOG_DEBUG("DIR: Error: %s not found.",expandedPath);
		};
*/
		// Follow Directory
		if ((nextDir>=0) && curDir->fileList[nextDir]->isDir) {
			curDir = curDir->fileList[nextDir];
			strcpy (curDir->orgname,dir);
			if (!IsCachedIn(curDir)) {
				if (OpenDir(curDir,expandedPath,id)) {
					char buffer[CROSS_LEN];
					char* result = 0;
					strcpy(buffer,dirPath);
					ReadDir(id,result);
					strcpy(dirPath,buffer);
					free[id] = true;
				};
			}
		};
		if (pos) {
			strcat(expandedPath,split);
			start = pos+1;
		}
	} while (pos);

	// Save last result for faster access next time
	strcpy(save_path,path);
	strcpy(save_expanded,expandedPath);
	save_dir = curDir;

	return curDir;
}

bool DOS_Drive_Cache::OpenDir(const char* path, Bit16u& id) {
	char expand[CROSS_LEN] = {0};
	CFileInfo* dir = FindDirInfo(path,expand);
	if (OpenDir(dir,expand,id)) {
		dirSearch[id]->nextEntry = 0;
		return true;
	}
	return false;
}

bool DOS_Drive_Cache::OpenDir(CFileInfo* dir, const char* expand, Bit16u& id) {
	id = GetFreeID(dir);
	dirSearch[id] = dir;
	char expandcopy [CROSS_LEN];
	strcpy(expandcopy,expand);   
	// Add "/"
	char end[2]={CROSS_FILESPLIT,0};
	if (expandcopy[strlen(expandcopy)-1]!=CROSS_FILESPLIT) strcat(expandcopy,end);
	// open dir
	if (dirSearch[id]) {
		// open dir
		dir_information* dirp = open_directory(expandcopy);
		if (dirp) { 
			// Reset it..
			close_directory(dirp);
			strcpy(dirPath,expandcopy);
			free[id] = false;
			return true;
		}
	};
	return false;
}

void DOS_Drive_Cache::CreateEntry(CFileInfo* dir, const char* name, bool is_directory) {
	CFileInfo* info = new CFileInfo;
	strcpy(info->orgname, name);				
	info->shortNr = 0;
	info->isDir = is_directory;

	// Check for long filenames...
	CreateShortName(dir, info);		

	bool found = false;

	// keep list sorted (so GetLongName works correctly, used by CreateShortName in this routine)
	if (dir->fileList.size()>0) {
		if (!(strcmp(info->shortname,dir->fileList.back()->shortname)<0)) {
			// append at end of list
			dir->fileList.push_back(info);
		} else {
			// look for position where to insert this element
			std::vector<CFileInfo*>::iterator it;
			for (it=dir->fileList.begin(); it!=dir->fileList.end(); ++it) {
				if (strcmp(info->shortname,(*it)->shortname)<0) {
					found = true;
					break;
				}
			}
			// Put file in lists
			if (found) dir->fileList.insert(it,info);
			else dir->fileList.push_back(info);
		}
	} else {
		// empty file list, append
		dir->fileList.push_back(info);
	}
}

void DOS_Drive_Cache::CopyEntry(CFileInfo* dir, CFileInfo* from) {
	CFileInfo* info = new CFileInfo;
	// just copy things into new fileinfo
	strcpy(info->orgname, from->orgname);				
	strcpy(info->shortname, from->shortname);				
	info->shortNr = from->shortNr;
	info->isDir = from->isDir;

	dir->fileList.push_back(info);
}

bool DOS_Drive_Cache::ReadDir(Bit16u id, char* &result) {
	// shouldnt happen...
	if (id>MAX_OPENDIRS) return false;

	if (!IsCachedIn(dirSearch[id])) {
		// Try to open directory
		dir_information* dirp = open_directory(dirPath);
		if (!dirp) {
			free[id] = true;
			return false;
		}
		// Read complete directory
		char dir_name[CROSS_LEN];
		bool is_directory;
		if (read_directory_first(dirp, dir_name, is_directory)) {
			CreateEntry(dirSearch[id], dir_name, is_directory);
			while (read_directory_next(dirp, dir_name, is_directory)) {
				CreateEntry(dirSearch[id], dir_name, is_directory);
			}
		}

		// close dir
		close_directory(dirp);

		// Info
/*		if (!dirp) {
			LOG_DEBUG("DIR: Error Caching in %s",dirPath);			
			return false;
		} else {	
			char buffer[128];
			sprintf(buffer,"DIR: Caching in %s (%d Files)",dirPath,dirSearch[srchNr]->fileList.size());
			LOG_DEBUG(buffer);
		};*/
	};
	if (SetResult(dirSearch[id], result, dirSearch[id]->nextEntry)) return true;
	free[id] = true;
	return false;
}

bool DOS_Drive_Cache::SetResult(CFileInfo* dir, char* &result, Bitu entryNr)
{
	static char res[CROSS_LEN] = { 0 };

	result = res;
	if (entryNr>=dir->fileList.size()) return false;
	CFileInfo* info = dir->fileList[entryNr];
	// copy filename, short version
	strcpy(res,info->shortname);
	// Set to next Entry
	dir->nextEntry = entryNr+1;
	return true;
}

// FindFirst / FindNext
bool DOS_Drive_Cache::FindFirst(char* path, Bit16u& id) {
	Bit16u	dirID;
	// Cache directory in 
	if (!OpenDir(path,dirID)) return false;

	//Find a free slot.
	//If the next one isn't free, move on to the next, if none is free => reset and assume the worst
	Bit16u local_findcounter = 0;
	while ( local_findcounter < MAX_OPENDIRS ) {
		if (dirFindFirst[this->nextFreeFindFirst] == 0) break;
		if (++this->nextFreeFindFirst >= MAX_OPENDIRS) this->nextFreeFindFirst = 0; //Wrap around
		local_findcounter++;
	}

	Bit16u	dirFindFirstID = this->nextFreeFindFirst++;
	if (this->nextFreeFindFirst >= MAX_OPENDIRS) this->nextFreeFindFirst = 0; //Increase and wrap around for the next search.

	if (local_findcounter == MAX_OPENDIRS) { //Here is the reset from above.
		// no free slot found...
		LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst/Next: All slots full. Resetting");
		// Clear the internal list then.
		dirFindFirstID = 0;
		this->nextFreeFindFirst = 1; //the next free one after this search
		for(Bitu n=0; n<MAX_OPENDIRS;n++) {	
	     	// Clear and reuse slot
			delete dirFindFirst[n];
			dirFindFirst[n]=0;
		}
	   
	}		
	dirFindFirst[dirFindFirstID] = new CFileInfo();
	dirFindFirst[dirFindFirstID]-> nextEntry	= 0;

	// Copy entries to use with FindNext
	for (Bitu i=0; i<dirSearch[dirID]->fileList.size(); i++) {
		CopyEntry(dirFindFirst[dirFindFirstID],dirSearch[dirID]->fileList[i]);
	}
	// Now re-sort the fileList accordingly to output
	switch (sortDirType) {
		case ALPHABETICAL		: break;
//		case ALPHABETICAL		: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByName);		break;
		case DIRALPHABETICAL	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByDirName);		break;
		case ALPHABETICALREV	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByNameRev);		break;
		case DIRALPHABETICALREV	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByDirNameRev);	break;
		case NOSORT				: break;
	}

//	LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst : %s (ID:%02X)",path,dirFindFirstID);
	id = dirFindFirstID;
	return true;
}

bool DOS_Drive_Cache::FindNext(Bit16u id, char* &result) {
	// out of range ?
	if ((id>=MAX_OPENDIRS) || !dirFindFirst[id]) {
		LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst/Next failure : ID out of range: %04X",id);
		return false;
	}
	if (!SetResult(dirFindFirst[id], result, dirFindFirst[id]->nextEntry)) {
		// free slot
		delete dirFindFirst[id]; dirFindFirst[id] = 0;
		return false;
	}
	return true;
}
