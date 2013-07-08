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

/* $Id: drives.h,v 1.41 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <vector>
#include <sys/types.h>
#include "dos_system.h"
#include "shell.h" /* for DOS_Shell */
#include "bios_disk.h"  /* for fatDrive */

bool WildFileCmp(const char * file, const char * wild);
void Set_Label(char const * const input, char * const output, bool cdrom);

class DriveManager {
public:
	static void AppendDisk(int drive, DOS_Drive* disk);
	static void InitializeDrive(int drive);
	static int UnmountDrive(int drive);
//	static void CycleDrive(bool pressed);
//	static void CycleDisk(bool pressed);
	static void CycleAllDisks(void);
	static void Init(Section* sec);
	
private:
	static struct DriveInfo {
		std::vector<DOS_Drive*> disks;
		Bit32u currentDisk;
	} driveInfos[DOS_DRIVES];
	
	static int currentDrive;
};

class localDrive : public DOS_Drive {
public:
	localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual FILE *GetSystemFilePtr(char const * const name, char const * const type);
	virtual bool GetSystemFilename(char* sysName, char const * const dosName);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool TestDir(char * dir);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
private:
	char basedir[CROSS_LEN];
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
	} srchInfo[MAX_OPENDIRS];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct bootstrap {
	Bit8u  nearjmp[3];
	Bit8u  oemname[8];
	Bit16u bytespersector;
	Bit8u  sectorspercluster;
	Bit16u reservedsectors;
	Bit8u  fatcopies;
	Bit16u rootdirentries;
	Bit16u totalsectorcount;
	Bit8u  mediadescriptor;
	Bit16u sectorsperfat;
	Bit16u sectorspertrack;
	Bit16u headcount;
	/* 32-bit FAT extensions */
	Bit32u hiddensectorcount;
	Bit32u totalsecdword;
	Bit8u  bootcode[474];
	Bit8u  magic1; /* 0x55 */
	Bit8u  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

struct direntry {
	Bit8u entryname[11];
	Bit8u attrib;
	Bit8u NTRes;
	Bit8u milliSecondStamp;
	Bit16u crtTime;
	Bit16u crtDate;
	Bit16u accessDate;
	Bit16u hiFirstClust;
	Bit16u modTime;
	Bit16u modDate;
	Bit16u loFirstClust;
	Bit32u entrysize;
} GCC_ATTRIBUTE(packed);

struct partTable {
	Bit8u booter[446];
	struct {
		Bit8u bootflag;
		Bit8u beginchs[3];
		Bit8u parttype;
		Bit8u endchs[3];
		Bit32u absSectStart;
		Bit32u partSize;
	} pentry[4];
	Bit8u  magic1; /* 0x55 */
	Bit8u  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

class fatDrive : public DOS_Drive {
public:
	fatDrive(const char * sysFilename, Bit32u bytesector, Bit32u cylsector, Bit32u headscyl, Bit32u cylinders, Bit32u startSector);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool TestDir(char * dir);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
public:
	Bit32u getAbsoluteSectFromBytePos(Bit32u startClustNum, Bit32u bytePos);
	Bit32u getSectorSize(void);
	Bit32u getAbsoluteSectFromChain(Bit32u startClustNum, Bit32u logicalSector);
	bool allocateCluster(Bit32u useCluster, Bit32u prevCluster);
	Bit32u appendCluster(Bit32u startCluster);
	void deleteClustChain(Bit32u startCluster);
	Bit32u getFirstFreeClust(void);
	bool directoryBrowse(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum);
	bool directoryChange(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum);
	imageDisk *loadedDisk;
	bool created_successfully;
private:
	Bit32u getClusterValue(Bit32u clustNum);
	void setClusterValue(Bit32u clustNum, Bit32u clustValue);
	Bit32u getClustFirstSect(Bit32u clustNum);
	bool FindNextInternal(Bit32u dirClustNumber, DOS_DTA & dta, direntry *foundEntry);
	bool getDirClustNum(char * dir, Bit32u * clustNum, bool parDir);
	bool getFileDirEntry(char const * const filename, direntry * useEntry, Bit32u * dirClust, Bit32u * subEntry);
	bool addDirectoryEntry(Bit32u dirClustNumber, direntry useEntry);
	void zeroOutCluster(Bit32u clustNumber);
	bool getEntryName(char *fullname, char *entname);
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
	} srchInfo[MAX_OPENDIRS];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
	
	bootstrap bootbuffer;
	Bit8u fattype;
	Bit32u CountOfClusters;
	Bit32u partSectOff;
	Bit32u firstDataSector;
	Bit32u firstRootDirSect;

	Bit32u cwdDirCluster;
	Bit32u dirPosition; /* Position in directory search */
};


class cdromDrive : public localDrive
{
public:
	cdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual void SetDir(const char* path);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
private:
	Bit8u subUnit;
	char driveLetter;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct isoPVD {
	Bit8u type;
	Bit8u standardIdent[5];
	Bit8u version;
	Bit8u unused1;
	Bit8u systemIdent[32];
	Bit8u volumeIdent[32];
	Bit8u unused2[8];
	Bit32u volumeSpaceSizeL;
	Bit32u volumeSpaceSizeM;
	Bit8u unused3[32];
	Bit16u volumeSetSizeL;
	Bit16u volumeSetSizeM;
	Bit16u volumeSeqNumberL;
	Bit16u volumeSeqNumberM;
	Bit16u logicBlockSizeL;
	Bit16u logicBlockSizeM;
	Bit32u pathTableSizeL;
	Bit32u pathTableSizeM;
	Bit32u locationPathTableL;
	Bit32u locationOptPathTableL;
	Bit32u locationPathTableM;
	Bit32u locationOptPathTableM;
	Bit8u rootEntry[34];
	Bit32u unused4[1858];
} GCC_ATTRIBUTE(packed);

struct isoDirEntry {
	Bit8u length;
	Bit8u extAttrLength;
	Bit32u extentLocationL;
	Bit32u extentLocationM;
	Bit32u dataLengthL;
	Bit32u dataLengthM;
	Bit8u dateYear;
	Bit8u dateMonth;
	Bit8u dateDay;
	Bit8u timeHour;
	Bit8u timeMin;
	Bit8u timeSec;
	Bit8u timeZone;
	Bit8u fileFlags;
	Bit8u fileUnitSize;
	Bit8u interleaveGapSize;
	Bit16u VolumeSeqNumberL;
	Bit16u VolumeSeqNumberM;
	Bit8u fileIdentLength;
	Bit8u ident[222];
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

#if defined (WORDS_BIGENDIAN)
#define EXTENT_LOCATION(de)	((de).extentLocationM)
#define DATA_LENGTH(de)		((de).dataLengthM)
#else
#define EXTENT_LOCATION(de)	((de).extentLocationL)
#define DATA_LENGTH(de)		((de).dataLengthL)
#endif

#define ISO_FRAMESIZE		2048
#define ISO_DIRECTORY		2
#define ISO_HIDDEN		1
#define ISO_MAX_FILENAME_LENGTH 37
#define ISO_MAXPATHNAME		256
#define ISO_FIRST_VD		16
#define IS_DIR(fileFlags)	(fileFlags & ISO_DIRECTORY)
#define IS_HIDDEN(fileFlags)	(fileFlags & ISO_HIDDEN)
#define ISO_MAX_HASH_TABLE_SIZE 	100

class isoDrive : public DOS_Drive {
public:
	isoDrive(char driveLetter, const char* device_name, Bit8u mediaid, int &error);
	~isoDrive();
	virtual bool FileOpen(DOS_File **file, char *name, Bit32u flags);
	virtual bool FileCreate(DOS_File **file, char *name, Bit16u attributes);
	virtual bool FileUnlink(char *name);
	virtual bool RemoveDir(char *dir);
	virtual bool MakeDir(char *dir);
	virtual bool TestDir(char *dir);
	virtual bool FindFirst(char *_dir, DOS_DTA &dta, bool fcb_findfirst);
	virtual bool FindNext(DOS_DTA &dta);
	virtual bool GetFileAttr(char *name, Bit16u *attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u *bytes_sector, Bit8u *sectors_cluster, Bit16u *total_clusters, Bit16u *free_clusters);
	virtual bool FileExists(const char *name);
   	virtual bool FileStat(const char *name, FileStat_Block *const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual void EmptyCache(void){}
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	bool readSector(Bit8u *buffer, Bit32u sector);
	virtual char const* GetLabel(void) {return discLabel;};
	virtual void Activate(void);
private:
	int  readDirEntry(isoDirEntry *de, Bit8u *data);
	bool loadImage();
	bool lookupSingle(isoDirEntry *de, const char *name, Bit32u sectorStart, Bit32u length);
	bool lookup(isoDirEntry *de, const char *path);
	int  UpdateMscdex(char driveLetter, const char* physicalPath, Bit8u& subUnit);
	int  GetDirIterator(const isoDirEntry* de);
	bool GetNextDirEntry(const int dirIterator, isoDirEntry* de);
	void FreeDirIterator(const int dirIterator);
	bool ReadCachedSector(Bit8u** buffer, const Bit32u sector);
	
	struct DirIterator {
		bool valid;
		bool root;
		Bit32u currentSector;
		Bit32u endSector;
		Bit32u pos;
	} dirIterators[MAX_OPENDIRS];
	
	int nextFreeDirIterator;
	
	struct SectorHashEntry {
		bool valid;
		Bit32u sector;
		Bit8u data[ISO_FRAMESIZE];
	} sectorHashEntries[ISO_MAX_HASH_TABLE_SIZE];

	bool dataCD;
	isoDirEntry rootEntry;
	Bit8u mediaid;
	char fileName[CROSS_LEN];
	Bit8u subUnit;
	char driveLetter;
	char discLabel[32];
};

struct VFILE_Block;

class Virtual_Drive: public DOS_Drive {
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	bool FileUnlink(char * name);
	bool RemoveDir(char * dir);
	bool MakeDir(char * dir);
	bool TestDir(char * dir);
	bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	bool FindNext(DOS_DTA & dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	bool FileExists(const char* name);
	bool FileStat(const char* name, FileStat_Block* const stat_block);
	Bit8u GetMediaByte(void);
	void EmptyCache(void){}
	bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
private:
	VFILE_Block * search_file;
};



#endif
