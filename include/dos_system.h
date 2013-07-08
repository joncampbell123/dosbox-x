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

/* $Id: dos_system.h,v 1.47 2009-03-04 21:08:22 c2woody Exp $ */

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

#define DOS_NAMELENGTH 12
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define DOS_FCBNAME 15
#define DOS_DIRDEPTH 8
#define DOS_PATHLENGTH 80
#define DOS_TEMPSIZE 1024

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
	Bit32u size;
	Bit16u time;
	Bit16u date;
	Bit16u attr;
};

class DOS_DTA;

class DOS_File {
public:
	DOS_File():flags(0)		{ name=0; refCtr = 0; hdrive=0xff; };
	DOS_File(const DOS_File& orig);
	DOS_File & operator= (const DOS_File & orig);
	virtual	~DOS_File(){if(name) delete [] name;};
	virtual bool	Read(Bit8u * data,Bit16u * size)=0;
	virtual bool	Write(Bit8u * data,Bit16u * size)=0;
	virtual bool	Seek(Bit32u * pos,Bit32u type)=0;
	virtual bool	Close()=0;
	virtual Bit16u	GetInformation(void)=0;
	virtual void	SetName(const char* _name)	{ if (name) delete[] name; name = new char[strlen(_name)+1]; strcpy(name,_name); }
	virtual char*	GetName(void)				{ return name; };
	virtual bool	IsOpen()					{ return open; };
	virtual bool	IsName(const char* _name)	{ if (!name) return false; return strcasecmp(name,_name)==0; };
	virtual void	AddRef()					{ refCtr++; };
	virtual Bits	RemoveRef()					{ return --refCtr; };
	virtual bool	UpdateDateTimeFromHost()	{ return true; }
	void SetDrive(Bit8u drv) { hdrive=drv;}
	Bit8u GetDrive(void) { return hdrive;}
	Bit32u flags;
	Bit16u time;
	Bit16u date;
	Bit16u attr;
	Bits refCtr;
	bool open;
	char* name;
/* Some Device Specific Stuff */
private:
	Bit8u hdrive;
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
	virtual bool	Read(Bit8u * data,Bit16u * size);
	virtual bool	Write(Bit8u * data,Bit16u * size);
	virtual bool	Seek(Bit32u * pos,Bit32u type);
	virtual bool	Close();
	virtual Bit16u	GetInformation(void);
	virtual bool	ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode);
	virtual bool	WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode);
	void SetDeviceNumber(Bitu num) { devnum=num;}
private:
	Bitu devnum;
};

/* The following variable can be lowered to free up some memory.
 * The negative side effect: The stored searches will be turned over faster.
 * Should not have impact on systems with few directory entries. */
#define MAX_OPENDIRS 2048
//Can be high as it's only storage (16 bit variable)

class DOS_Drive_Cache {
public:
	DOS_Drive_Cache					(void);
	DOS_Drive_Cache					(const char* path);
	~DOS_Drive_Cache				(void);

	enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };

	void		SetBaseDir			(const char* path);
	void		SetDirSort			(TDirSort sort) { sortDirType = sort; };
	bool		OpenDir				(const char* path, Bit16u& id);
	bool		ReadDir				(Bit16u id, char* &result);

	void		ExpandName			(char* path);
	char*		GetExpandName		(const char* path);
	bool		GetShortName		(const char* fullname, char* shortname);

	bool		FindFirst			(char* path, Bit16u& id);
	bool		FindNext			(Bit16u id, char* &result);

	void		CacheOut			(const char* path, bool ignoreLastDir = false);
	void		AddEntry			(const char* path, bool checkExist = false);
	void		DeleteEntry			(const char* path, bool ignoreLastDir = false);

	void		EmptyCache			(void);
	void		SetLabel			(const char* name,bool cdrom,bool allowupdate);
	char*		GetLabel			(void) { return label; };

	class CFileInfo {
	public:
		CFileInfo(void) {
			orgname[0] = shortname[0] = 0;
			nextEntry = shortNr = 0;
			isDir = false;
		}
		~CFileInfo(void) {
			for (Bit32u i=0; i<fileList.size(); i++) delete fileList[i];
			fileList.clear();
			longNameList.clear();
		};
		char		orgname		[CROSS_LEN];
		char		shortname	[DOS_NAMELENGTH_ASCII];
		bool		isDir;
		Bitu		nextEntry;
		Bitu		shortNr;
		// contents
		std::vector<CFileInfo*>	fileList;
		std::vector<CFileInfo*>	longNameList;
	};

private:

	bool		RemoveTrailingDot	(char* shortname);
	Bits		GetLongName		(CFileInfo* info, char* shortname);
	void		CreateShortName		(CFileInfo* dir, CFileInfo* info);
	Bitu		CreateShortNameID	(CFileInfo* dir, const char* name);
	int		CompareShortname	(const char* compareName, const char* shortName);
	bool		SetResult		(CFileInfo* dir, char * &result, Bitu entryNr);
	bool		IsCachedIn		(CFileInfo* dir);
	CFileInfo*	FindDirInfo		(const char* path, char* expandedPath);
	bool		RemoveSpaces		(char* str);
	bool		OpenDir			(CFileInfo* dir, const char* path, Bit16u& id);
	void		CreateEntry		(CFileInfo* dir, const char* name, bool query_directory);
	void		CopyEntry		(CFileInfo* dir, CFileInfo* from);
	Bit16u		GetFreeID		(CFileInfo* dir);
	void		Clear			(void);

	CFileInfo*	dirBase;
	char		dirPath				[CROSS_LEN];
	char		basePath			[CROSS_LEN];
	bool		dirFirstTime;
	TDirSort	sortDirType;
	CFileInfo*	save_dir;
	char		save_path			[CROSS_LEN];
	char		save_expanded		[CROSS_LEN];

	Bit16u		srchNr;
	CFileInfo*	dirSearch			[MAX_OPENDIRS];
	char		dirSearchName		[MAX_OPENDIRS];
	bool		free				[MAX_OPENDIRS];
	CFileInfo*	dirFindFirst		[MAX_OPENDIRS];
	Bit16u		nextFreeFindFirst;

	char		label				[CROSS_LEN];
	bool		updatelabel;
};

class DOS_Drive {
public:
	DOS_Drive();
	virtual ~DOS_Drive(){};
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags)=0;
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes)=0;
	virtual bool FileUnlink(char * _name)=0;
	virtual bool RemoveDir(char * _dir)=0;
	virtual bool MakeDir(char * _dir)=0;
	virtual bool TestDir(char * _dir)=0;
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false)=0;
	virtual bool FindNext(DOS_DTA & dta)=0;
	virtual bool GetFileAttr(char * name,Bit16u * attr)=0;
	virtual bool Rename(char * oldname,char * newname)=0;
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters)=0;
	virtual bool FileExists(const char* name)=0;
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block)=0;
	virtual Bit8u GetMediaByte(void)=0;
	virtual void SetDir(const char* path) { strcpy(curdir,path); };
	virtual void EmptyCache(void) { dirCache.EmptyCache(); };
	virtual bool isRemote(void)=0;
	virtual bool isRemovable(void)=0;
	virtual Bits UnMount(void)=0;

	char * GetInfo(void);
	char curdir[DOS_PATHLENGTH];
	char info[256];
	/* Can be overridden for example in iso images */
	virtual char const * GetLabel(){return dirCache.GetLabel();};

	DOS_Drive_Cache dirCache;

	// disk cycling functionality (request resources)
	virtual void Activate(void) {};
};

enum { OPEN_READ=0,OPEN_WRITE=1,OPEN_READWRITE=2, DOS_NOT_INHERIT=128};
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

void VFILE_Register(const char * name,Bit8u * data,Bit32u size);
#endif
