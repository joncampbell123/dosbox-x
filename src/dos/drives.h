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


#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <vector>
#include <sys/types.h>
#include "dos_system.h"
#include "shell.h" /* for DOS_Shell */

bool DOS_CommonFAT32FAT16DiskSpaceConv(
		Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free,
		const Bit32u bytes32,const Bit32u sectors32,const Bit32u clusters32,const Bit32u free32);

bool WildFileCmp(const char * file, const char * wild);
bool LWildFileCmp(const char * file, const char * wild);
void Set_Label(char const * const input, char * const output, bool cdrom);

class DriveManager {
public:
	static void AppendDisk(int drive, DOS_Drive* disk);
	static void InitializeDrive(int drive);
	static int UnmountDrive(int drive);
//	static void CycleDrive(bool pressed);
//	static void CycleDisk(bool pressed);
	static void CycleDisks(int drive, bool notify);
	static void CycleAllDisks(void);
	static void CycleAllCDs(void);
	static void Init(Section* s);
	
	static void SaveState( std::ostream& stream );
	static void LoadState( std::istream& stream );
	
private:
	static struct DriveInfo {
		std::vector<DOS_Drive*> disks;
		Bit32u currentDisk = 0;
	} driveInfos[DOS_DRIVES];
	
	static int currentDrive;
};

class localDrive : public DOS_Drive {
public:
	localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, std::vector<std::string> &options);
	virtual bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	virtual FILE *GetSystemFilePtr(char const * const name, char const * const type); 
	virtual bool GetSystemFilename(char* sysName, char const * const dosName); 
	virtual bool FileCreate(DOS_File * * file,const char * name,Bit16u attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool TestDir(const char * dir);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool SetFileAttr(const char * name,Bit16u attr);
	virtual bool GetFileAttr(const char * name,Bit16u * attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
	virtual unsigned long GetSerial();
#endif
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	virtual char const * GetLabel(){return dirCache.GetLabel();};
	virtual void SetLabel(const char *label, bool iscdrom, bool updatable) { dirCache.SetLabel(label,iscdrom,updatable); };
	virtual void *opendir(const char *name);
	virtual void closedir(void *handle);
	virtual bool read_directory_first(void *handle, char* entry_name, char* entry_sname, bool& is_directory);
    virtual bool read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory);

	virtual void EmptyCache(void) { dirCache.EmptyCache(); };
	virtual void MediaChange() {};
	const char* getBasedir() {return basedir;};

	int remote = -1;

protected:
	DOS_Drive_Cache dirCache;
	char basedir[CROSS_LEN];
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
    } srchInfo[MAX_OPENDIRS] = {};

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
};

#if 0 // nothing uses this
class physfsDrive : public localDrive {
private:
	bool isdir(const char *dir);

public:
	physfsDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid);
	virtual bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,Bit16u attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool TestDir(const char * dir);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(const char * name,Bit16u * attr);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual void *opendir(const char *dir);
	virtual void closedir(void *handle);
	virtual bool read_directory_first(void *handle, char* entry_name, bool& is_directory);
	virtual bool read_directory_next(void *handle, char* entry_name, bool& is_directory);
	virtual const char *GetInfo(void);
	virtual ~physfsDrive(void);
};
#endif

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct FAT_BPB_MSDOS20 {
    Bit16u      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    Bit8u       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    Bit16u      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    Bit8u       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    Bit16u      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    Bit16u      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    Bit8u       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    Bit16u      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x018 size 0x00D total */

struct FAT_BPB_MSDOS30 {
    Bit16u      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    Bit8u       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    Bit16u      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    Bit8u       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    Bit16u      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    Bit16u      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    Bit8u       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    Bit16u      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    Bit16u      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    Bit16u      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    Bit32u      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x020 size 0x015 total */
                                                    /* ==== ADDITIONAL NOTES (Wikipedia) */
                                                    /* offset 0x01C size 0x002 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.0) */
                                                    /* offset 0x01E size 0x002 Total sectors including hidden (?) if BPB_TotSec16 != 0 (MS-DOS 3.20) */

struct FAT_BPB_MSDOS331 {
    Bit16u      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    Bit8u       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    Bit16u      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    Bit8u       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    Bit16u      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    Bit16u      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    Bit8u       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    Bit16u      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    Bit16u      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    Bit16u      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    Bit32u      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
    Bit32u      BPB_TotSec32;                       /* offset 0x020 size 0x004 Total sectors of volume if count >= 0x10000 or FAT32, or 0 if not. totalsecdword */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x024 size 0x019 total */

struct FAT_BPB_MSDOS40 { /* FAT12/FAT16 only */
    Bit16u      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    Bit8u       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    Bit16u      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    Bit8u       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    Bit16u      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    Bit16u      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    Bit8u       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    Bit16u      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    Bit16u      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    Bit16u      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    Bit32u      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
    Bit32u      BPB_TotSec32;                       /* offset 0x020 size 0x004 Total sectors of volume if count >= 0x10000 or FAT32, or 0 if not. totalsecdword */
    Bit8u       BPB_DrvNum;                         /* offset 0x024 size 0x001 Physical (INT 13h) drive number */
    Bit8u       BPB_Reserved1;                      /* offset 0x025 size 0x001 Reserved? */
    Bit8u       BPB_BootSig;                        /* offset 0x026 size 0x001 Extended boot signature. 0x29 or 0x28 to indicate following members exist. */
    Bit32u      BPB_VolID;                          /* offset 0x027 size 0x004 Volume ID, if BPB_BootSig is 0x28 or 0x29. */
    Bit8u       BPB_VolLab[11];                     /* offset 0x02B size 0x00B Volume label, if BPB_BootSig is 0x28 or 0x29. */
    Bit8u       BPB_FilSysType[8];                  /* offset 0x036 size 0x008 File system type, for display purposes if BPB_BootSig is 0x29. */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x03E size 0x033 total */

struct FAT_BPB_MSDOS710_FAT32 { /* FAT32 only */
    Bit16u      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    Bit8u       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    Bit16u      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    Bit8u       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    Bit16u      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    Bit16u      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    Bit8u       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    Bit16u      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    Bit16u      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    Bit16u      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    Bit32u      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
    Bit32u      BPB_TotSec32;                       /* offset 0x020 size 0x004 Total sectors of volume if count >= 0x10000 or FAT32, or 0 if not. totalsecdword */
    Bit32u      BPB_FATSz32;                        /* offset 0x024 size 0x004 Sectors per fat (FAT32). */
    Bit16u      BPB_ExtFlags;                       /* offset 0x028 size 0x002 Bitfield: [7:7] 1=one fat active 0=mirrored  [3:0]=active FAT if mirroring disabled */
    Bit16u      BPB_FSVer;                          /* offset 0x02A size 0x002 Version number. Only 0.0 is defined now. Do not mount if newer version beyond what we support */
    Bit32u      BPB_RootClus;                       /* offset 0x02C size 0x004 Starting cluster number of the root directory (FAT32) */
    Bit16u      BPB_FSInfo;                         /* offset 0x030 size 0x002 Sector number in volume of FAT32 FSInfo structure in reserved area */
    Bit16u      BPB_BkBootSec;                      /* offset 0x032 size 0x002 Sector number in volume of FAT32 backup boot sector */
    Bit8u       BPB_Reserved[12];                   /* offset 0x034 size 0x00C Reserved for future expansion */
    Bit8u       BS_DrvNum;                          /* offset 0x040 size 0x001 BPB_DrvNum but moved for FAT32 */
    Bit8u       BS_Reserved1;                       /* offset 0x041 size 0x001 BPB_Reserved1 but moved for FAT32 */
    Bit8u       BS_BootSig;                         /* offset 0x042 size 0x001 Extended boot signature. 0x29 or 0x28 to indicate following members exist. */
    Bit32u      BS_VolID;                           /* offset 0x043 size 0x004 Volume ID, if BPB_BootSig is 0x28 or 0x29. */
    Bit8u       BS_VolLab[11];                      /* offset 0x047 size 0x00B Volume label, if BPB_BootSig is 0x28 or 0x29. */
    Bit8u       BS_FilSysType[8];                   /* offset 0x052 size 0x008 File system type, for display purposes if BPB_BootSig is 0x29. */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x05A size 0x04F total */

typedef struct FAT_BPB_MSDOS40 FAT_BPB_MSDOS;       /* what we use internally */
typedef struct FAT_BPB_MSDOS710_FAT32 FAT32_BPB_MSDOS;       /* what we use internally */

struct FAT_BootSector {
    /* --------- Common fields: Amalgam of Wikipedia documentation with names from Microsoft's FAT32 whitepaper */
    Bit8u       BS_jmpBoot[3];                      /* offset 0x000 size 0x003 Jump instruction to boot code. Formerly nearjmp[3] */
    Bit8u       BS_OEMName[8];                      /* offset 0x003 size 0x008 OEM string. Formerly oemname[8] */
    /* --------- BIOS Parameter Block (converted in place from existing DOSBox-X code) */
    union bpb_union_t {
        struct FAT_BPB_MSDOS20              v20;    /* offset 0x00B size 0x00D MS-DOS 2.0 BPB */
        struct FAT_BPB_MSDOS30              v30;    /* offset 0x00B size 0x015 MS-DOS 3.0 BPB */
        struct FAT_BPB_MSDOS331             v331;   /* offset 0x00B size 0x019 MS-DOS 3.31 BPB */
        struct FAT_BPB_MSDOS40              v40;    /* offset 0x00B size 0x039 MS-DOS 4.0 BPB (FAT12/FAT16) */
        struct FAT_BPB_MSDOS710_FAT32       v710_32;/* offset 0x00B size 0x04F MS-DOS 7.10 BPB (FAT32) */

        FAT_BPB_MSDOS                       v;      /* offset 0x00B ... */
        FAT32_BPB_MSDOS                     v32;    /* offset 0x00B ... */

        inline bool is_fat32(void) const {
            return (v.BPB_RootEntCnt == 0 && v.BPB_TotSec16 == 0 && v.BPB_FATSz16 == 0); /* all fields are "must be set to 0" for FAT32 */
        }
    } bpb;
    /* --------- The rest of the sector ---------- */
    Bit8u  bootcode[512 - 2/*magic*/ - sizeof(bpb_union_t) - 8/*OEM*/ - 3/*JMP*/];
    Bit8u  magic1; /* 0x55 */
    Bit8u  magic2; /* 0xaa */
#ifndef SECTOR_SIZE_MAX
# pragma warning SECTOR_SIZE_MAX not defined
#endif
#if SECTOR_SIZE_MAX > 512
    Bit8u  extra[SECTOR_SIZE_MAX - 512];
#endif
} GCC_ATTRIBUTE(packed);
static_assert(offsetof(FAT_BootSector,bpb.v20) == 0x00B,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v30) == 0x00B,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v331) == 0x00B,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v40) == 0x00B,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v) == 0x00B,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v20.BPB_TotSec16) == 0x013,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v30.BPB_TotSec16) == 0x013,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v331.BPB_TotSec16) == 0x013,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v40.BPB_TotSec16) == 0x013,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v.BPB_TotSec16) == 0x013,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v331.BPB_TotSec32) == 0x020,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v30.BPB_HiddSec) == 0x01C,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v40.BPB_TotSec32) == 0x020,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v40.BPB_VolLab) == 0x02B,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v710_32.BS_FilSysType) == 0x052,"Oops");
static_assert(sizeof(FAT_BootSector) == SECTOR_SIZE_MAX,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v331.BPB_TotSec32) == 0x020,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v40.BPB_TotSec32) == 0x020,"Oops");
static_assert(offsetof(FAT_BootSector,bpb.v710_32.BPB_TotSec32) == 0x020,"Oops");
static_assert(sizeof(FAT_BootSector::bpb.v20) == 0x00D,"Oops");
static_assert(sizeof(FAT_BootSector::bpb.v30) == 0x015,"Oops");
static_assert(sizeof(FAT_BootSector::bpb.v331) == 0x019,"Oops");
static_assert(sizeof(FAT_BootSector::bpb.v40) == 0x033,"Oops");
static_assert(sizeof(FAT_BootSector::bpb.v710_32) == 0x04F,"Oops");

struct direntry {
	Bit8u entryname[11];
	Bit8u attrib;
	Bit8u NTRes;
	Bit8u milliSecondStamp;
	Bit16u crtTime;             // <- NTS: This field did not appear until MS-DOS 7.0 (Windows 95)
	Bit16u crtDate;             // <- NTS: This field did not appear until MS-DOS 7.0 (Windows 95)
	Bit16u accessDate;          // <- NTS: This field did not appear until MS-DOS 7.0 (Windows 95)
	Bit16u hiFirstClust;        // <- NTS: FAT32 only!
	Bit16u modTime;
	Bit16u modDate;
	Bit16u loFirstClust;
	Bit32u entrysize;

	inline Bit32u Cluster32(void) const {
		return ((Bit32u)hiFirstClust << (Bit32u)16) + loFirstClust;
	}
	inline void SetCluster32(const Bit32u v) {
		loFirstClust = (Bit16u)v;
		hiFirstClust = (Bit16u)(v >> (Bit32u)16);
	}
} GCC_ATTRIBUTE(packed);

struct direntry_lfn {
    Bit8u LDIR_Ord;                 /* 0x00 Long filename ordinal (1 to 63). bit 6 (0x40) is set if the last entry, which normally comes first in the directory */
    Bit16u LDIR_Name1[5];           /* 0x01 first 5 chars */
    Bit8u attrib;                   /* 0x0B */
    Bit8u LDIR_Type;                /* 0x0C zero to indicate a LFN */
    Bit8u LDIR_Chksum;              /* 0x0D checksum */
    Bit16u LDIR_Name2[6];           /* 0x0E next 6 chars */
    Bit16u LDIR_FstClusLO;          /* 0x1A zero (loFirstClust) */
    Bit16u LDIR_Name3[2];           /* 0x1C next 2 chars */
} GCC_ATTRIBUTE(packed);
static_assert(sizeof(direntry_lfn) == 0x20,"Oops");
static_assert(offsetof(direntry_lfn,LDIR_Name3) == 0x1C,"Oops");

#define MAX_DIRENTS_PER_SECTOR (SECTOR_SIZE_MAX / sizeof(direntry))

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
#ifndef SECTOR_SIZE_MAX
# pragma warning SECTOR_SIZE_MAX not defined
#endif
#if SECTOR_SIZE_MAX > 512
    Bit8u  extra[SECTOR_SIZE_MAX - 512];
#endif
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif
//Forward
class imageDisk;
class fatDrive : public DOS_Drive {
public:
	fatDrive(const char * sysFilename, Bit32u bytesector, Bit32u cylsector, Bit32u headscyl, Bit32u cylinders, std::vector<std::string> &options);
	fatDrive(imageDisk *sourceLoadedDisk, std::vector<std::string> &options);
    void fatDriveInit(const char *sysFilename, Bit32u bytesector, Bit32u cylsector, Bit32u headscyl, Bit32u cylinders, Bit64u filesize, const std::vector<std::string> &options);
    virtual ~fatDrive();
	virtual bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,Bit16u attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool TestDir(const char * dir);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool SetFileAttr(const char * name,Bit16u attr);
	virtual bool GetFileAttr(const char * name,Bit16u * attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
#endif
	virtual unsigned long GetSerial();
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool AllocationInfo32(Bit32u * _bytes_sector,Bit32u * _sectors_cluster,Bit32u * _total_clusters,Bit32u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
public:
	Bit8u readSector(Bit32u sectnum, void * data);
	Bit8u writeSector(Bit32u sectnum, void * data);
	Bit32u getAbsoluteSectFromBytePos(Bit32u startClustNum, Bit32u bytePos);
	Bit32u getSectorSize(void);
	Bit32u getClusterSize(void);
	Bit32u getAbsoluteSectFromChain(Bit32u startClustNum, Bit32u logicalSector);
	bool allocateCluster(Bit32u useCluster, Bit32u prevCluster);
	Bit32u appendCluster(Bit32u startCluster);
	void deleteClustChain(Bit32u startCluster, Bit32u bytePos);
	Bit32u getFirstFreeClust(void);
	bool directoryBrowse(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum, Bit32s start=0);
	bool directoryChange(Bit32u dirClustNumber, const direntry *useEntry, Bit32s entNum);
	const FAT_BootSector::bpb_union_t &GetBPB(void);
	void SetBPB(const FAT_BootSector::bpb_union_t &bpb);
	imageDisk *loadedDisk = NULL;
	float req_ver = 0.0;
	bool created_successfully = true;
private:
	char* Generate_SFN(const char *path, const char *name);
	Bit32u getClusterValue(Bit32u clustNum);
	void setClusterValue(Bit32u clustNum, Bit32u clustValue);
	Bit32u getClustFirstSect(Bit32u clustNum);
	bool FindNextInternal(Bit32u dirClustNumber, DOS_DTA & dta, direntry *foundEntry);
	bool getDirClustNum(const char * dir, Bit32u * clustNum, bool parDir);
	bool getFileDirEntry(char const * const filename, direntry * useEntry, Bit32u * dirClust, Bit32u * subEntry,bool dirOk=false);
	bool addDirectoryEntry(Bit32u dirClustNumber, const direntry& useEntry,const char *lfn=NULL);
	void zeroOutCluster(Bit32u clustNumber);
	bool getEntryName(const char *fullname, char *entname);
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
    } srchInfo[MAX_OPENDIRS] = {};

	/* directory entry range of LFN entries after FindNextInternal(), needed by
	 * filesystem code such as RemoveDir() which needs to delete the dirent AND
	 * the LFNs. Range is dirPos_start inclusive to dirPos_end exclusive.
	 * If start == end then there are no LFNs.
	 *
	 * Removal of entries:
	 * for (x=start;x < end;x++) ... */
	struct lfnRange_t {
		Bit16u      dirPos_start;
		Bit16u      dirPos_end;

		void clear(void) {
			dirPos_start = dirPos_end = 0;
		}
		bool empty(void) const {
			return dirPos_start == dirPos_end;
		}
	} lfnRange = {0,0};

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
    } allocation = {};
	
	FAT_BootSector::bpb_union_t BPB = {}; // BPB in effect (translated from on-disk BPB as needed)
	bool absolute = false;
	Bit8u fattype = 0;
	Bit32u CountOfClusters = 0;
	Bit32u partSectOff = 0;
	Bit32u partSectSize = 0;
	Bit32u firstDataSector = 0;
	Bit32u firstRootDirSect = 0;

	Bit32u cwdDirCluster = 0;

    Bit8u fatSectBuffer[SECTOR_SIZE_MAX * 2] = {};
	Bit32u curFatSect = 0;

	DOS_Drive_Cache labelCache;
public:
    /* the driver code must use THESE functions to read the disk, not directly from the disk drive,
     * in order to support a drive with a smaller sector size than the FAT filesystem's "sector".
     *
     * It is very common for instance to have PC-98 HDI images formatted with 256 bytes/sector at
     * the disk level and a FAT filesystem marked as having 1024 bytes/sector. */
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit32u getSectSize(void);
	Bit32u sector_size = 0;

    // INT 25h/INT 26h
    virtual Bit32u GetSectorCount(void);
    virtual Bit32u GetSectorSize(void);
	virtual Bit8u Read_AbsoluteSector_INT25(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector_INT25(Bit32u sectnum, void * data);
    virtual void UpdateDPB(unsigned char dos_drive);

	virtual char const * GetLabel(){return labelCache.GetLabel();};
	virtual void SetLabel(const char *label, bool iscdrom, bool updatable);
	virtual void UpdateBootVolumeLabel(const char *label);
	virtual Bit32u GetPartitionOffset(void);
	virtual Bit32u GetFirstClusterOffset(void);
	virtual Bit32u GetHighestClusterNumber(void);
};

PhysPt DOS_Get_DPB(unsigned int dos_drive);

class cdromDrive : public localDrive
{
public:
	cdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error, std::vector<std::string> &options);
	virtual bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,Bit16u attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool GetFileAttr(const char * name,Bit16u * attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
	virtual unsigned long GetSerial();
#endif
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual void SetDir(const char* path);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);virtual Bits UnMount(void);
private:
	Bit8u subUnit;	char driveLetter;
};

#if 0 // nothing uses this
class physfscdromDrive : public physfsDrive
{
public:
	physfscdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error);
	virtual bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,Bit16u attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool GetFileAttr(const char * name,Bit16u * attr);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual void SetDir(const char* path);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	virtual const char *GetInfo(void);
private:
	Bit8u subUnit;
	char driveLetter;
};
#endif

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

#define ISO_FRAMESIZE		2048u
#define ISO_ASSOCIATED		4u
#define ISO_DIRECTORY		2u
#define ISO_HIDDEN		1u
#define ISO_MAX_FILENAME_LENGTH 37u
#define ISO_MAXPATHNAME		256u
#define ISO_FIRST_VD		16u
#define IS_ASSOC(fileFlags)	(fileFlags & ISO_ASSOCIATED)
#define IS_DIR(fileFlags)	(fileFlags & ISO_DIRECTORY)
#define IS_HIDDEN(fileFlags)	(fileFlags & ISO_HIDDEN)
#define ISO_MAX_HASH_TABLE_SIZE 	100u

class isoDrive : public DOS_Drive {
public:
	isoDrive(char driveLetter, const char* fileName, Bit8u mediaid, int &error);
	~isoDrive();
	virtual bool FileOpen(DOS_File **file, const char *name, Bit32u flags);
	virtual bool FileCreate(DOS_File **file, const char *name, Bit16u attributes);
	virtual bool FileUnlink(const char *name);
	virtual bool RemoveDir(const char *dir);
	virtual bool MakeDir(const char *dir);
	virtual bool TestDir(const char *dir);
	virtual bool FindFirst(const char *dir, DOS_DTA &dta, bool fcb_findfirst);
	virtual bool FindNext(DOS_DTA &dta);
	virtual bool SetFileAttr(const char *name,Bit16u attr);
	virtual bool GetFileAttr(const char *name, Bit16u *attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
#endif
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(Bit16u *bytes_sector, Bit8u *sectors_cluster, Bit16u *total_clusters, Bit16u *free_clusters);
	virtual bool FileExists(const char *name);
   	virtual bool FileStat(const char *name, FileStat_Block *const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual void EmptyCache(void){}
	virtual void MediaChange();
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	bool readSector(Bit8u *buffer, Bit32u sector);
	virtual char const* GetLabel(void) {return discLabel;};
	virtual void Activate(void);
private:
    int  readDirEntry(isoDirEntry* de, const Bit8u* data);
	bool loadImage();
	bool lookupSingle(isoDirEntry *de, const char *name, Bit32u sectorStart, Bit32u length);
	bool lookup(isoDirEntry *de, const char *path);
	int  UpdateMscdex(char driveLetter, const char* path, Bit8u& subUnit);
	int  GetDirIterator(const isoDirEntry* de);
	bool GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de);
	void FreeDirIterator(const int dirIterator);
	bool ReadCachedSector(Bit8u** buffer, const Bit32u sector);
    void GetLongName(const char* ident, char* lfindName);
	
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

    bool iso = false;
    bool dataCD = false;
	isoDirEntry rootEntry;
    Bit8u mediaid = 0;
	char fileName[CROSS_LEN];
    Bit8u subUnit = 0;
    char driveLetter = '\0';
	char discLabel[32];
};

struct VFILE_Block;

class Virtual_Drive: public DOS_Drive {
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	bool FileCreate(DOS_File * * file,const char * name,Bit16u attributes);
	bool FileUnlink(const char * name);
	bool RemoveDir(const char * dir);
	bool MakeDir(const char * dir);
	bool TestDir(const char * dir);
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	bool FindNext(DOS_DTA & dta);
	bool SetFileAttr(const char * name,Bit16u attr);
	bool GetFileAttr(const char * name,Bit16u * attr);
	bool GetFileAttrEx(char* name, struct stat *status);
	unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name);
#endif
	bool Rename(const char * oldname,const char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	bool FileExists(const char* name);
	bool FileStat(const char* name, FileStat_Block* const stat_block);
	virtual void MediaChange() {}
	Bit8u GetMediaByte(void);
	void EmptyCache(void){}
	bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	virtual char const* GetLabel(void);
private:
    VFILE_Block* search_file = 0;
};

class Overlay_Drive: public localDrive {
public:
	Overlay_Drive(const char * startdir,const char* overlay, Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid,Bit8u &error, std::vector<std::string> &options);

	virtual bool FileOpen(DOS_File * * file,const char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,Bit16u /*attributes*/);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool FileUnlink(const char * name);
	virtual bool SetFileAttr(const char * name,Bit16u attr);
	virtual bool GetFileAttr(const char * name,Bit16u * attr);
	virtual bool FileExists(const char* name);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual void EmptyCache(void);

	FILE* create_file_in_overlay(const char* dos_filename, char const* mode);
	virtual Bits UnMount(void);
	virtual bool TestDir(const char * dir);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	bool ovlnocachedir = false;
	bool ovlreadonly = false;
private:
	char overlaydir[CROSS_LEN];
	bool optimize_cache_v1;
	bool Sync_leading_dirs(const char* dos_filename);
	void add_DOSname_to_cache(const char* name);
	void remove_DOSname_from_cache(const char* name);
	void add_DOSdir_to_cache(const char* name, const char* sname);
	void remove_DOSdir_from_cache(const char* name);
	void update_cache(bool read_directory_contents = false);
	
	std::vector<std::string> deleted_files_in_base; //Set is probably better, or some other solution (involving the disk).
	std::vector<std::string> deleted_paths_in_base; //Currently only used to hide the overlay folder.
	std::string overlap_folder;
	void add_deleted_file(const char* name, bool create_on_disk);
	void remove_deleted_file(const char* name, bool create_on_disk);
	bool is_deleted_file(const char* name);
	void add_deleted_path(const char* name, bool create_on_disk);
	void remove_deleted_path(const char* name, bool create_on_disk);
	bool is_deleted_path(const char* name);
	bool check_if_leading_is_deleted(const char* name);

	bool is_dir_only_in_overlay(const char* name); //cached


	void remove_special_file_from_disk(const char* dosname, const char* operation);
	void add_special_file_to_disk(const char* dosname, const char* operation);
	std::string create_filename_of_special_operation(const char* dosname, const char* operation);
	void convert_overlay_to_DOSname_in_base(char* dirname );
	//For caching the update_cache routine.
	std::vector<std::string> DOSnames_cache; //Also set is probably better.
	std::vector<std::string> DOSdirs_cache; //Can not blindly change its type. it is important that subdirs come after the parent directory.
	const std::string special_prefix;
};

/* No LFN filefind in progress (SFN call). This index is out of range and meant to indicate no LFN call in progress. */
#define LFN_FILEFIND_NONE           258
/* FAT image handle */
#define LFN_FILEFIND_IMG            256
/* Internal handle */
#define LFN_FILEFIND_INTERNAL       255
/* Highest valid handle */
#define LFN_FILEFIND_MAX            255

#endif
