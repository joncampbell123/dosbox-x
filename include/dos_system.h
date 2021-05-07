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


#ifndef DOSBOX_DOS_SYSTEM_H
#define DOSBOX_DOS_SYSTEM_H

#include <vector>
#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_CROSS_H
#include "cross.h"
#endif
#ifndef DOSBOX_SUPPORT_H
#include "support.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif
#include <ctype.h>

#define DOS_NAMELENGTH 12u
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define LFN_NAMELENGTH 255u
#define DOS_FCBNAME 15u
#define DOS_DIRDEPTH 8u
#define DOS_PATHLENGTH 255u
#define DOS_TEMPSIZE 1024u

enum {
    CPM_COMPAT_OFF=0,
    CPM_COMPAT_MSDOS2,
    CPM_COMPAT_MSDOS5,
    CPM_COMPAT_DIRECT
};

enum {
	DOS_ATTR_READ_ONLY=	0x01,
	DOS_ATTR_HIDDEN=	0x02,
	DOS_ATTR_SYSTEM=	0x04,
	DOS_ATTR_VOLUME=	0x08,
	DOS_ATTR_DIRECTORY=	0x10,
	DOS_ATTR_ARCHIVE=	0x20,
	DOS_ATTR_DEVICE=	0x40
};

struct FileStat_Block {
	uint32_t size;
	uint16_t time;
	uint16_t date;
	uint16_t attr;
};

class DOS_DTA;

#ifdef WIN32 /* Shaddup MSVC! */
# define stricmp _stricmp
#endif

class DOS_File {
public:
    DOS_File() :flags(0) { name = 0; attr = 0; date = 0; drive = 0; refCtr = 0; open = false; time = 0; hdrive = 0xff; newtime = false; };
	DOS_File(const DOS_File& orig);
	DOS_File & operator= (const DOS_File & orig);
	virtual	~DOS_File(){if(name) delete [] name;};
	virtual bool	Read(uint8_t * data,uint16_t * size)=0;
	virtual bool	Write(const uint8_t * data,uint16_t * size)=0;
	virtual bool	Seek(uint32_t * pos,uint32_t type)=0;
	virtual bool	Close()=0;
	/* ert, 20100711: Locking extensions */
	virtual bool    LockFile(uint8_t mode, uint32_t pos, uint16_t size) { (void)mode; (void)pos; (void)size; return false; };
	virtual uint16_t	GetInformation(void)=0;
	virtual void	SetName(const char* _name)	{ if (name) delete[] name; name = new char[strlen(_name)+1]; strcpy(name,_name); }
	virtual char*	GetName(void)				{ return name; };
	virtual bool	IsOpen()					{ return open; };
	virtual bool	IsName(const char* _name)	{ if (!name) return false; return strcasecmp(name,_name)==0; };
	virtual void	AddRef()					{ refCtr++; };
	virtual Bits	RemoveRef()					{ return --refCtr; };
	virtual bool	UpdateDateTimeFromHost()	{ return true; }
	virtual uint32_t	GetSeekPos()	{ return 0xffffffff; }
	void SetDrive(uint8_t drv) { hdrive=drv;}
	uint8_t GetDrive(void) { return hdrive;}
	virtual void 	SaveState( std::ostream& stream );
	virtual void 	LoadState( std::istream& stream, bool pop );
    virtual void    Flush(void) { }

	char* name = NULL;
	uint8_t drive = 0;
	uint32_t flags;
	bool open;

	uint16_t attr;
	uint16_t time;
	uint16_t date;
	Bits refCtr;
	bool newtime = false;
	/* Some Device Specific Stuff */
private:
	uint8_t hdrive;
};

class DOS_Device : public DOS_File {
public:
	DOS_Device(const DOS_Device& orig):DOS_File(orig) {
		devnum=orig.devnum;
		open=true;
	}
	DOS_Device & operator= (const DOS_Device & orig) {
		DOS_File::operator=(orig);
		devnum=orig.devnum;
		open=true;
		return *this;
	}
	DOS_Device():DOS_File(),devnum(0){};
	virtual ~DOS_Device() {};
	virtual bool	Read(uint8_t * data,uint16_t * size);
	virtual bool	Write(const uint8_t * data,uint16_t * size);
	virtual bool	Seek(uint32_t * pos,uint32_t type);
	virtual bool	Close();
	virtual uint16_t	GetInformation(void);
	virtual bool	ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode);
	virtual bool	WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode);
	void SetDeviceNumber(Bitu num) { devnum=num;}
private:
	Bitu devnum;
};

class localFile : public DOS_File {
public:
	localFile();
	localFile(const char* _name, FILE * handle);
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	bool LockFile(uint8_t mode, uint32_t pos, uint16_t size);
	uint16_t GetInformation(void);
	bool UpdateDateTimeFromHost(void);
	bool UpdateLocalDateTime(void);
	void FlagReadOnlyMedium(void);
	void Flush(void);
	uint32_t GetSeekPos(void);
	FILE * fhandle;
private:
	bool read_only_medium;
	enum { NONE,READ,WRITE } last_action;
};

/* The following variable can be lowered to free up some memory.
 * The negative side effect: The stored searches will be turned over faster.
 * Should not have impact on systems with few directory entries. */
class DOS_Drive;
#define MAX_OPENDIRS 2048
//Can be high as it's only storage (16 bit variable)

class DOS_Drive_Cache {
public:
	DOS_Drive_Cache					(void);
	DOS_Drive_Cache					(const char* path, DOS_Drive *drive); 
	~DOS_Drive_Cache				(void);

	enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };

	void		SetBaseDir			(const char* baseDir, DOS_Drive *drive);
	void		SetDirSort			(TDirSort sort) { sortDirType = sort; };
	bool		OpenDir				(const char* path, uint16_t& id);
    bool        ReadDir             (uint16_t id, char* &result, char * &lresult);

	void		ExpandName			(char* path);
	char*		GetExpandName		(const char* path);
	bool		GetShortName		(const char* fullname, char* shortname);

	bool		FindFirst			(char* path, uint16_t& id);
    bool        FindNext            (uint16_t id, char* &result, char* &lresult);

	void		CacheOut			(const char* path, bool ignoreLastDir = false);
	void		AddEntry			(const char* path, bool checkExists = false);
	void        AddEntryDirOverlay	(const char* path, char *sfile, bool checkExist = false);
	void		DeleteEntry			(const char* path, bool ignoreLastDir = false);

	void		EmptyCache			(void);
	void		MediaChange			(void);
	void		SetLabel			(const char* vname,bool cdrom,bool allowupdate);
	char*		GetLabel			(void) { return label; };

	class CFileInfo {
	public:
		CFileInfo(void) {
			orgname[0] = shortname[0] = 0;
			isOverlayDir = isDir = false;
			id = MAX_OPENDIRS;
			nextEntry = shortNr = 0;
		}
		~CFileInfo(void) {
			for (uint32_t i=0; i<fileList.size(); i++) delete fileList[i];
			fileList.clear();
			longNameList.clear();
		};
		char		orgname		[CROSS_LEN];
		char		shortname	[DOS_NAMELENGTH_ASCII];
		bool        isOverlayDir;
		bool		isDir;
		uint16_t		id;
		Bitu		nextEntry;
		Bitu		shortNr;
		// contents
		std::vector<CFileInfo*>	fileList;
		std::vector<CFileInfo*>	longNameList;
	};

private:
	void ClearFileInfo(CFileInfo *dir);
	void DeleteFileInfo(CFileInfo *dir);

	bool		RemoveTrailingDot	(char* shortname);
	Bits		GetLongName		(CFileInfo* curDir, char* shortName);
	void		CreateShortName		(CFileInfo* curDir, CFileInfo* info);
	Bitu		CreateShortNameID	(CFileInfo* curDir, const char* name);
	int		CompareShortname	(const char* compareName, const char* shortName);
    bool        SetResult       (CFileInfo* dir, char * &result, char * &lresult, Bitu entryNr);
	bool		IsCachedIn		(CFileInfo* curDir);
	CFileInfo*	FindDirInfo		(const char* path, char* expandedPath);
	bool		RemoveSpaces		(char* str);
	bool		OpenDir			(CFileInfo* dir, const char* expand, uint16_t& id);
    char*       CreateEntry     (CFileInfo* dir, const char* name, const char* sname, bool is_directory);
	void		CopyEntry		(CFileInfo* dir, CFileInfo* from);
	uint16_t		GetFreeID		(CFileInfo* dir);
	void		Clear			(void);

	CFileInfo*	dirBase;
	char		dirPath				[CROSS_LEN] = {};
	DOS_Drive*	drive = NULL;
	char		basePath			[CROSS_LEN] = {};
	bool		dirFirstTime = false;
	TDirSort	sortDirType;
	CFileInfo*	save_dir;
	char		save_path			[CROSS_LEN] = {};
	char		save_expanded		[CROSS_LEN] = {};

	uint16_t		srchNr;
	CFileInfo*	dirSearch			[MAX_OPENDIRS];
	char		dirSearchName		[MAX_OPENDIRS] = {};
	CFileInfo*	dirFindFirst		[MAX_OPENDIRS];
	uint16_t		nextFreeFindFirst;

	char		label				[CROSS_LEN];
	bool		updatelabel;
};

class DOS_Drive {
public:
	DOS_Drive();
	virtual ~DOS_Drive(){};
	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags)=0;
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes)=0;
	virtual bool FileUnlink(const char * _name)=0;
	virtual bool RemoveDir(const char * _dir)=0;
	virtual bool MakeDir(const char * _dir)=0;
	virtual bool TestDir(const char * _dir)=0;
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false)=0;
	virtual bool FindNext(DOS_DTA & dta)=0;
	virtual bool SetFileAttr(const char * name,uint16_t attr)=0;
	virtual bool GetFileAttr(const char * name,uint16_t * attr)=0;
	virtual bool GetFileAttrEx(char* name, struct stat *status)=0;
	virtual unsigned long GetCompressedSize(char* name)=0;
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name)=0;
#endif
	virtual bool Rename(const char * oldname,const char * newname)=0;
	virtual bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters)=0;
	virtual bool AllocationInfo32(uint32_t * _bytes_sector,uint32_t * _sectors_cluster,uint32_t * _total_clusters,uint32_t * _free_clusters) { (void)_bytes_sector; (void)_sectors_cluster; (void)_total_clusters; (void)_free_clusters; return false; }
	virtual bool FileExists(const char* name)=0;
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block)=0;
	virtual uint8_t GetMediaByte(void)=0;
	virtual void SetDir(const char* path) { strcpy(curdir,path); };
//	virtual void EmptyCache(void) { dirCache.EmptyCache(); };
	virtual bool isRemote(void)=0;
	virtual bool isRemovable(void)=0;
	virtual Bits UnMount(void)=0;

	/* these 4 may only be used by DOS_Drive_Cache because they have special calling conventions */
	virtual void *opendir(const char *dir) { (void)dir; return NULL; };
	virtual void closedir(void *handle) { (void)handle; };
    virtual bool read_directory_first(void *handle, char* entry_name, char* entry_sname, bool& is_directory) { (void)handle; (void)entry_name; (void)entry_sname; (void)is_directory; return false; };
    virtual bool read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory) { (void)handle; (void)entry_name; (void)entry_sname; (void)is_directory; return false; };

	virtual const char * GetInfo(void);
	char * GetBaseDir(void);

    bool readonly;
    bool nocachedir;
	char curdir[DOS_PATHLENGTH];
	char info[256];
	/* Can be overridden for example in iso images */
	virtual char const * GetLabel() {return "NOLABEL";}; 
	virtual void SetLabel(const char *label, bool iscdrom, bool updatable) { (void)label; (void)iscdrom; (void)updatable; };
	virtual void EmptyCache() {};
	virtual void MediaChange() {};
	// disk cycling functionality (request resources)
	virtual void Activate(void) {};
	virtual void SaveState( std::ostream& stream );
	virtual void LoadState( std::istream& stream );
	virtual void UpdateDPB(unsigned char dos_drive) { (void)dos_drive; };

    // INT 25h/INT 26h
    virtual uint32_t GetSectorCount(void) { return 0; }
    virtual uint32_t GetSectorSize(void) { return 0; } // LOGICAL sector size (from the FAT driver) not PHYSICAL disk sector size
	virtual uint8_t Read_AbsoluteSector_INT25(uint32_t sectnum, void * data) { (void)sectnum; (void)data; return 0x05; }
	virtual uint8_t Write_AbsoluteSector_INT25(uint32_t sectnum, void * data) { (void)sectnum; (void)data; return 0x05; }
};

enum { OPEN_READ=0, OPEN_WRITE=1, OPEN_READWRITE=2, OPEN_READ_NO_MOD=4, DOS_NOT_INHERIT=128};
enum { DOS_SEEK_SET=0,DOS_SEEK_CUR=1,DOS_SEEK_END=2};


/*
 A multiplex handler should read the registers to check what function is being called
 If the handler returns false dos will stop checking other handlers
*/

typedef bool (MultiplexHandler)(void);
void DOS_AddMultiplexHandler(MultiplexHandler * handler);
void DOS_DelMultiplexHandler(MultiplexHandler * handler);

/* AddDevice stores the pointer to a created device */
void DOS_AddDevice(DOS_Device * adddev);
/* DelDevice destroys the device that is pointed to. */
void DOS_DelDevice(DOS_Device * dev);

void VFILE_Register(const char * name,uint8_t * data,uint32_t size,const char *dir = "");
void VFILE_RegisterBuiltinFileBlob(const struct BuiltinFileBlob &b,const char *dir = "");
#endif
