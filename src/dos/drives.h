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


#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <vector>
#include <sys/types.h>
#include "dos_system.h"
#include "shell.h" /* for DOS_Shell */

bool DOS_CommonFAT32FAT16DiskSpaceConv(
		uint16_t * bytes,uint8_t * sectors,uint16_t * clusters,uint16_t * free,
		const uint32_t bytes32,const uint32_t sectors32,const uint32_t clusters32,const uint32_t free32);

bool WildFileCmp(const char * file, const char * wild);
bool LWildFileCmp(const char * file, const char * wild);
void Set_Label(char const * const input, char * const output, bool cdrom);

class DriveManager {
public:
	static void AppendDisk(int drive, DOS_Drive* disk);
	static void InitializeDrive(int drive);
	static int UnmountDrive(int drive);
	static char * GetDrivePosition(int drive);
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
		uint32_t currentDisk = 0;
	} driveInfos[DOS_DRIVES];
	
	static int currentDrive;
};

class localDrive : public DOS_Drive {
public:
	localDrive(const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, std::vector<std::string> &options);
	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	virtual FILE *GetSystemFilePtr(char const * const name, char const * const type); 
	virtual bool GetSystemFilename(char* sysName, char const * const dosName); 
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool TestDir(const char * dir);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool SetFileAttr(const char * name,uint16_t attr);
	virtual bool GetFileAttr(const char * name,uint16_t * attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
	virtual unsigned long GetSerial();
#endif
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual uint8_t GetMediaByte(void);
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
	struct {
		uint16_t bytes_sector;
		uint8_t sectors_cluster;
		uint16_t total_clusters;
		uint16_t free_clusters;
		uint8_t mediaid;
		unsigned long initfree;
	} allocation;
	int remote = -1;

protected:
	DOS_Drive_Cache dirCache;
	char basedir[CROSS_LEN];
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
    } srchInfo[MAX_OPENDIRS] = {};
};

class physfsDrive : public localDrive {
private:
	bool isdir(const char *dir);
	char driveLetter;

public:
	physfsDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options);
	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool TestDir(const char * dir);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool SetFileAttr(const char * name,uint16_t attr);
	virtual bool GetFileAttr(const char * name,uint16_t * attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
	virtual unsigned long GetSerial();
#endif
	virtual bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual void *opendir(const char *dir);
	virtual void closedir(void *handle);
	virtual bool read_directory_first(void *handle, char* entry_name, char* entry_sname, bool& is_directory);
	virtual bool read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory);
	virtual const char *GetInfo(void);
	virtual const char *getOverlaydir(void);
	virtual bool setOverlaydir(const char * name);
	Bits UnMount();
	virtual ~physfsDrive(void);

protected:
	std::string mountarc;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct FAT_BPB_MSDOS20 {
    uint16_t      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    uint8_t       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    uint16_t      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    uint8_t       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    uint16_t      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    uint16_t      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    uint8_t       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    uint16_t      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x018 size 0x00D total */

struct FAT_BPB_MSDOS30 {
    uint16_t      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    uint8_t       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    uint16_t      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    uint8_t       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    uint16_t      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    uint16_t      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    uint8_t       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    uint16_t      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    uint16_t      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    uint16_t      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    uint32_t      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x020 size 0x015 total */
                                                    /* ==== ADDITIONAL NOTES (Wikipedia) */
                                                    /* offset 0x01C size 0x002 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.0) */
                                                    /* offset 0x01E size 0x002 Total sectors including hidden (?) if BPB_TotSec16 != 0 (MS-DOS 3.20) */

struct FAT_BPB_MSDOS331 {
    uint16_t      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    uint8_t       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    uint16_t      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    uint8_t       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    uint16_t      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    uint16_t      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    uint8_t       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    uint16_t      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    uint16_t      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    uint16_t      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    uint32_t      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
    uint32_t      BPB_TotSec32;                       /* offset 0x020 size 0x004 Total sectors of volume if count >= 0x10000 or FAT32, or 0 if not. totalsecdword */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x024 size 0x019 total */

struct FAT_BPB_MSDOS40 { /* FAT12/FAT16 only */
    uint16_t      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    uint8_t       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    uint16_t      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    uint8_t       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    uint16_t      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    uint16_t      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    uint8_t       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    uint16_t      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    uint16_t      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    uint16_t      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    uint32_t      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
    uint32_t      BPB_TotSec32;                       /* offset 0x020 size 0x004 Total sectors of volume if count >= 0x10000 or FAT32, or 0 if not. totalsecdword */
    uint8_t       BPB_DrvNum;                         /* offset 0x024 size 0x001 Physical (INT 13h) drive number */
    uint8_t       BPB_Reserved1;                      /* offset 0x025 size 0x001 Reserved? */
    uint8_t       BPB_BootSig;                        /* offset 0x026 size 0x001 Extended boot signature. 0x29 or 0x28 to indicate following members exist. */
    uint32_t      BPB_VolID;                          /* offset 0x027 size 0x004 Volume ID, if BPB_BootSig is 0x28 or 0x29. */
    uint8_t       BPB_VolLab[11];                     /* offset 0x02B size 0x00B Volume label, if BPB_BootSig is 0x28 or 0x29. */
    uint8_t       BPB_FilSysType[8];                  /* offset 0x036 size 0x008 File system type, for display purposes if BPB_BootSig is 0x29. */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x03E size 0x033 total */

struct FAT_BPB_MSDOS710_FAT32 { /* FAT32 only */
    uint16_t      BPB_BytsPerSec;                     /* offset 0x00B size 0x002 Bytes per sector. Formerly bytespersector */
    uint8_t       BPB_SecPerClus;                     /* offset 0x00D size 0x001 Sectors per cluster, must be a power of 2. Formerly sectorspercluster */
    uint16_t      BPB_RsvdSecCnt;                     /* offset 0x00E size 0x002 Number of reserved sectors starting from partition, to FAT table. reservedsectors */
    uint8_t       BPB_NumFATs;                        /* offset 0x010 size 0x001 Number of FAT tables. fatcopies */
    uint16_t      BPB_RootEntCnt;                     /* offset 0x011 size 0x002 Number of 32-byte root directories (FAT12/FAT16), or 0 (FAT32). rootdirentries */
    uint16_t      BPB_TotSec16;                       /* offset 0x013 size 0x002 Total sectors of volume if count < 0x10000, or 0 if not. 0 if FAT32. totalsectorcount */
    uint8_t       BPB_Media;                          /* offset 0x015 size 0x001 Media type byte. mediadescriptor */
    uint16_t      BPB_FATSz16;                        /* offset 0x016 size 0x002 Sectors per fat (FAT12/FAT16), or 0 (FAT32). sectorsperfat */
    uint16_t      BPB_SecPerTrk;                      /* offset 0x018 size 0x002 Sectors per track. sectorspertrack */
    uint16_t      BPB_NumHeads;                       /* offset 0x01A size 0x002 Number of heads. headcount */
    uint32_t      BPB_HiddSec;                        /* offset 0x01C size 0x004 Number of hidden sectors (i.e. starting sector of partition). hiddensectorcount (MS-DOS 3.31) */
    uint32_t      BPB_TotSec32;                       /* offset 0x020 size 0x004 Total sectors of volume if count >= 0x10000 or FAT32, or 0 if not. totalsecdword */
    uint32_t      BPB_FATSz32;                        /* offset 0x024 size 0x004 Sectors per fat (FAT32). */
    uint16_t      BPB_ExtFlags;                       /* offset 0x028 size 0x002 Bitfield: [7:7] 1=one fat active 0=mirrored  [3:0]=active FAT if mirroring disabled */
    uint16_t      BPB_FSVer;                          /* offset 0x02A size 0x002 Version number. Only 0.0 is defined now. Do not mount if newer version beyond what we support */
    uint32_t      BPB_RootClus;                       /* offset 0x02C size 0x004 Starting cluster number of the root directory (FAT32) */
    uint16_t      BPB_FSInfo;                         /* offset 0x030 size 0x002 Sector number in volume of FAT32 FSInfo structure in reserved area */
    uint16_t      BPB_BkBootSec;                      /* offset 0x032 size 0x002 Sector number in volume of FAT32 backup boot sector */
    uint8_t       BPB_Reserved[12];                   /* offset 0x034 size 0x00C Reserved for future expansion */
    uint8_t       BS_DrvNum;                          /* offset 0x040 size 0x001 BPB_DrvNum but moved for FAT32 */
    uint8_t       BS_Reserved1;                       /* offset 0x041 size 0x001 BPB_Reserved1 but moved for FAT32 */
    uint8_t       BS_BootSig;                         /* offset 0x042 size 0x001 Extended boot signature. 0x29 or 0x28 to indicate following members exist. */
    uint32_t      BS_VolID;                           /* offset 0x043 size 0x004 Volume ID, if BPB_BootSig is 0x28 or 0x29. */
    uint8_t       BS_VolLab[11];                      /* offset 0x047 size 0x00B Volume label, if BPB_BootSig is 0x28 or 0x29. */
    uint8_t       BS_FilSysType[8];                   /* offset 0x052 size 0x008 File system type, for display purposes if BPB_BootSig is 0x29. */
} GCC_ATTRIBUTE(packed);                            /*    ==> 0x05A size 0x04F total */

typedef struct FAT_BPB_MSDOS40 FAT_BPB_MSDOS;       /* what we use internally */
typedef struct FAT_BPB_MSDOS710_FAT32 FAT32_BPB_MSDOS;       /* what we use internally */

struct FAT_BootSector {
    /* --------- Common fields: Amalgam of Wikipedia documentation with names from Microsoft's FAT32 whitepaper */
    uint8_t       BS_jmpBoot[3];                      /* offset 0x000 size 0x003 Jump instruction to boot code. Formerly nearjmp[3] */
    uint8_t       BS_OEMName[8];                      /* offset 0x003 size 0x008 OEM string. Formerly oemname[8] */
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
    uint8_t  bootcode[512 - 2/*magic*/ - sizeof(bpb_union_t) - 8/*OEM*/ - 3/*JMP*/];
    uint8_t  magic1; /* 0x55 */
    uint8_t  magic2; /* 0xaa */
#ifndef SECTOR_SIZE_MAX
# pragma warning SECTOR_SIZE_MAX not defined
#endif
#if SECTOR_SIZE_MAX > 512
    uint8_t  extra[SECTOR_SIZE_MAX - 512];
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
	uint8_t entryname[11];
	uint8_t attrib;
	uint8_t NTRes;
	uint8_t milliSecondStamp;
	uint16_t crtTime;             // <- NTS: This field did not appear until MS-DOS 7.0 (Windows 95)
	uint16_t crtDate;             // <- NTS: This field did not appear until MS-DOS 7.0 (Windows 95)
	uint16_t accessDate;          // <- NTS: This field did not appear until MS-DOS 7.0 (Windows 95)
	uint16_t hiFirstClust;        // <- NTS: FAT32 only!
	uint16_t modTime;
	uint16_t modDate;
	uint16_t loFirstClust;
	uint32_t entrysize;

	inline uint32_t Cluster32(void) const {
		return ((uint32_t)hiFirstClust << (uint32_t)16) + loFirstClust;
	}
	inline void SetCluster32(const uint32_t v) {
		loFirstClust = (uint16_t)v;
		hiFirstClust = (uint16_t)(v >> (uint32_t)16);
	}
} GCC_ATTRIBUTE(packed);

struct direntry_lfn {
    uint8_t LDIR_Ord;                 /* 0x00 Long filename ordinal (1 to 63). bit 6 (0x40) is set if the last entry, which normally comes first in the directory */
    uint16_t LDIR_Name1[5];           /* 0x01 first 5 chars */
    uint8_t attrib;                   /* 0x0B */
    uint8_t LDIR_Type;                /* 0x0C zero to indicate a LFN */
    uint8_t LDIR_Chksum;              /* 0x0D checksum */
    uint16_t LDIR_Name2[6];           /* 0x0E next 6 chars */
    uint16_t LDIR_FstClusLO;          /* 0x1A zero (loFirstClust) */
    uint16_t LDIR_Name3[2];           /* 0x1C next 2 chars */
} GCC_ATTRIBUTE(packed);
static_assert(sizeof(direntry_lfn) == 0x20,"Oops");
static_assert(offsetof(direntry_lfn,LDIR_Name3) == 0x1C,"Oops");

#define MAX_DIRENTS_PER_SECTOR (SECTOR_SIZE_MAX / sizeof(direntry))

struct partTable {
	uint8_t booter[446];
	struct {
		uint8_t bootflag;
		uint8_t beginchs[3];
		uint8_t parttype;
		uint8_t endchs[3];
		uint32_t absSectStart;
		uint32_t partSize;
	} pentry[4];
	uint8_t  magic1; /* 0x55 */
	uint8_t  magic2; /* 0xaa */
#ifndef SECTOR_SIZE_MAX
# pragma warning SECTOR_SIZE_MAX not defined
#endif
#if SECTOR_SIZE_MAX > 512
    uint8_t  extra[SECTOR_SIZE_MAX - 512];
#endif
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif
//Forward
class imageDisk;
class fatDrive : public DOS_Drive {
public:
	fatDrive(const char * sysFilename, uint32_t bytesector, uint32_t cylsector, uint32_t headscyl, uint32_t cylinders, std::vector<std::string> &options);
	fatDrive(imageDisk *sourceLoadedDisk, std::vector<std::string> &options);
    void fatDriveInit(const char *sysFilename, uint32_t bytesector, uint32_t cylsector, uint32_t headscyl, uint32_t cylinders, uint64_t filesize, const std::vector<std::string> &options);
    virtual ~fatDrive();
	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool TestDir(const char * dir);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool SetFileAttr(const char * name,uint16_t attr);
	virtual bool GetFileAttr(const char * name,uint16_t * attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
#endif
	virtual unsigned long GetSerial();
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters);
	virtual bool AllocationInfo32(uint32_t * _bytes_sector,uint32_t * _sectors_cluster,uint32_t * _total_clusters,uint32_t * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual uint8_t GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
public:
	uint8_t readSector(uint32_t sectnum, void * data);
	uint8_t writeSector(uint32_t sectnum, void * data);
	uint32_t getAbsoluteSectFromBytePos(uint32_t startClustNum, uint32_t bytePos);
	uint32_t getSectorSize(void);
	uint32_t getClusterSize(void);
	uint32_t getAbsoluteSectFromChain(uint32_t startClustNum, uint32_t logicalSector);
	bool allocateCluster(uint32_t useCluster, uint32_t prevCluster);
	uint32_t appendCluster(uint32_t startCluster);
	void deleteClustChain(uint32_t startCluster, uint32_t bytePos);
	uint32_t getFirstFreeClust(void);
	bool directoryBrowse(uint32_t dirClustNumber, direntry *useEntry, int32_t entNum, int32_t start=0);
	bool directoryChange(uint32_t dirClustNumber, const direntry *useEntry, int32_t entNum);
	const FAT_BootSector::bpb_union_t &GetBPB(void);
	void SetBPB(const FAT_BootSector::bpb_union_t &bpb);
	imageDisk *loadedDisk = NULL;
	uint8_t req_ver_major = 0,req_ver_minor = 0;
	bool created_successfully = true;
    struct {
        uint32_t bytesector;
        uint32_t cylsector;
        uint32_t headscyl;
        uint32_t cylinders;
        int mounttype;
    } opts = {0, 0, 0, 0, -1};
    struct {
        unsigned char CDROM_drive;
        unsigned long cdrom_sector_offset;
        unsigned char floppy_emu_type;
    } el = {0, 0, 0};

private:
	char* Generate_SFN(const char *path, const char *name);
	uint32_t getClusterValue(uint32_t clustNum);
	void setClusterValue(uint32_t clustNum, uint32_t clustValue);
	uint32_t getClustFirstSect(uint32_t clustNum);
	bool FindNextInternal(uint32_t dirClustNumber, DOS_DTA & dta, direntry *foundEntry);
	bool getDirClustNum(const char * dir, uint32_t * clustNum, bool parDir);
	bool getFileDirEntry(char const * const filename, direntry * useEntry, uint32_t * dirClust, uint32_t * subEntry,bool dirOk=false);
	bool addDirectoryEntry(uint32_t dirClustNumber, const direntry& useEntry,const char *lfn=NULL);
	void zeroOutCluster(uint32_t clustNumber);
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
		uint16_t      dirPos_start;
		uint16_t      dirPos_end;

		void clear(void) {
			dirPos_start = dirPos_end = 0;
		}
		bool empty(void) const {
			return dirPos_start == dirPos_end;
		}
	} lfnRange = {0,0};

	struct {
		uint16_t bytes_sector;
		uint8_t sectors_cluster;
		uint16_t total_clusters;
		uint16_t free_clusters;
		uint8_t mediaid;
    } allocation = {};
	
	FAT_BootSector::bpb_union_t BPB = {}; // BPB in effect (translated from on-disk BPB as needed)
	bool absolute = false;
	uint8_t fattype = 0;
	uint32_t CountOfClusters = 0;
	uint32_t partSectOff = 0;
	uint32_t partSectSize = 0;
	uint32_t firstDataSector = 0;
	uint32_t firstRootDirSect = 0;

	uint32_t cwdDirCluster = 0;

    uint8_t fatSectBuffer[SECTOR_SIZE_MAX * 2] = {};
	uint32_t curFatSect = 0;

	DOS_Drive_Cache labelCache;
public:
    /* the driver code must use THESE functions to read the disk, not directly from the disk drive,
     * in order to support a drive with a smaller sector size than the FAT filesystem's "sector".
     *
     * It is very common for instance to have PC-98 HDI images formatted with 256 bytes/sector at
     * the disk level and a FAT filesystem marked as having 1024 bytes/sector. */
	virtual uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data);
	virtual uint8_t Write_AbsoluteSector(uint32_t sectnum, void * data);
	virtual uint32_t getSectSize(void);
	uint32_t sector_size = 0;

    // INT 25h/INT 26h
    virtual uint32_t GetSectorCount(void);
    virtual uint32_t GetSectorSize(void);
	virtual uint8_t Read_AbsoluteSector_INT25(uint32_t sectnum, void * data);
	virtual uint8_t Write_AbsoluteSector_INT25(uint32_t sectnum, void * data);
    virtual void UpdateDPB(unsigned char dos_drive);

	virtual char const * GetLabel(){return labelCache.GetLabel();};
	virtual void SetLabel(const char *label, bool iscdrom, bool updatable);
	virtual void UpdateBootVolumeLabel(const char *label);
	virtual uint32_t GetPartitionOffset(void);
	virtual uint32_t GetFirstClusterOffset(void);
	virtual uint32_t GetHighestClusterNumber(void);
};

PhysPt DOS_Get_DPB(unsigned int dos_drive);

class cdromDrive : public localDrive
{
public:
	cdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options);
	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool GetFileAttr(const char * name,uint16_t * attr);
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
	uint8_t subUnit;	char driveLetter;
};

class physfscdromDrive : public physfsDrive
{
public:
	physfscdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options);
	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes);
	virtual bool FileUnlink(const char * name);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool GetFileAttr(const char * name,uint16_t * attr);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual void SetDir(const char* path);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	virtual const char *GetInfo(void);
private:
	uint8_t subUnit;
	char driveLetter;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct isoPVD {
	uint8_t type;
	uint8_t standardIdent[5];
	uint8_t version;
	uint8_t unused1;
	uint8_t systemIdent[32];
	uint8_t volumeIdent[32];
	uint8_t unused2[8];
	uint32_t volumeSpaceSizeL;
	uint32_t volumeSpaceSizeM;
	uint8_t unused3[32];
	uint16_t volumeSetSizeL;
	uint16_t volumeSetSizeM;
	uint16_t volumeSeqNumberL;
	uint16_t volumeSeqNumberM;
	uint16_t logicBlockSizeL;
	uint16_t logicBlockSizeM;
	uint32_t pathTableSizeL;
	uint32_t pathTableSizeM;
	uint32_t locationPathTableL;
	uint32_t locationOptPathTableL;
	uint32_t locationPathTableM;
	uint32_t locationOptPathTableM;
	uint8_t rootEntry[34];
	uint32_t unused4[1858];
} GCC_ATTRIBUTE(packed);

struct isoDirEntry {
	uint8_t length;
	uint8_t extAttrLength;
	uint32_t extentLocationL;
	uint32_t extentLocationM;
	uint32_t dataLengthL;
	uint32_t dataLengthM;
	uint8_t dateYear;
	uint8_t dateMonth;
	uint8_t dateDay;
	uint8_t timeHour;
	uint8_t timeMin;
	uint8_t timeSec;
	uint8_t timeZone;
	uint8_t fileFlags;
	uint8_t fileUnitSize;
	uint8_t interleaveGapSize;
	uint16_t VolumeSeqNumberL;
	uint16_t VolumeSeqNumberM;
	uint8_t fileIdentLength;
	uint8_t ident[222];
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
	isoDrive(char driveLetter, const char* fileName, uint8_t mediaid, int &error);
	~isoDrive();
	virtual bool FileOpen(DOS_File **file, const char *name, uint32_t flags);
	virtual bool FileCreate(DOS_File **file, const char *name, uint16_t attributes);
	virtual bool FileUnlink(const char *name);
	virtual bool RemoveDir(const char *dir);
	virtual bool MakeDir(const char *dir);
	virtual bool TestDir(const char *dir);
	virtual bool FindFirst(const char *dir, DOS_DTA &dta, bool fcb_findfirst);
	virtual bool FindNext(DOS_DTA &dta);
	virtual bool SetFileAttr(const char *name,uint16_t attr);
	virtual bool GetFileAttr(const char *name, uint16_t *attr);
	virtual bool GetFileAttrEx(char* name, struct stat *status);
	virtual unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	virtual HANDLE CreateOpenFile(char const* const name);
#endif
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool AllocationInfo(uint16_t *bytes_sector, uint8_t *sectors_cluster, uint16_t *total_clusters, uint16_t *free_clusters);
	virtual bool FileExists(const char *name);
   	virtual bool FileStat(const char *name, FileStat_Block *const stat_block);
	virtual uint8_t GetMediaByte(void);
	virtual void EmptyCache(void){}
	virtual void MediaChange();
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	bool readSector(uint8_t *buffer, uint32_t sector);
	virtual char const* GetLabel(void) {return discLabel;};
	virtual void Activate(void);
private:
    int  readDirEntry(isoDirEntry* de, const uint8_t* data);
	bool loadImage();
	bool lookupSingle(isoDirEntry *de, const char *name, uint32_t sectorStart, uint32_t length);
	bool lookup(isoDirEntry *de, const char *path);
	int  UpdateMscdex(char driveLetter, const char* path, uint8_t& subUnit);
	int  GetDirIterator(const isoDirEntry* de);
	bool GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de);
	void FreeDirIterator(const int dirIterator);
	bool ReadCachedSector(uint8_t** buffer, const uint32_t sector);
    void GetLongName(const char* ident, char* lfindName);
	
	struct DirIterator {
		bool valid;
		bool root;
		uint32_t currentSector;
		uint32_t endSector;
		uint32_t pos;
	} dirIterators[MAX_OPENDIRS];
	
	int nextFreeDirIterator;
	
	struct SectorHashEntry {
		bool valid;
		uint32_t sector;
		uint8_t data[ISO_FRAMESIZE];
	} sectorHashEntries[ISO_MAX_HASH_TABLE_SIZE];

    bool iso = false;
    bool dataCD = false;
	isoDirEntry rootEntry;
    uint8_t mediaid = 0;
	char fileName[CROSS_LEN];
    uint8_t subUnit = 0;
    char driveLetter = '\0';
	char discLabel[32];
};

struct VFILE_Block;

class Virtual_Drive: public DOS_Drive {
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes);
	bool FileUnlink(const char * name);
	bool RemoveDir(const char * dir);
	bool MakeDir(const char * dir);
	bool TestDir(const char * dir);
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	bool FindNext(DOS_DTA & dta);
	bool SetFileAttr(const char * name,uint16_t attr);
	bool GetFileAttr(const char * name,uint16_t * attr);
	bool GetFileAttrEx(char* name, struct stat *status);
	unsigned long GetCompressedSize(char* name);
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name);
#endif
	bool Rename(const char * oldname,const char * newname);
	bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters);
	bool FileExists(const char* name);
	bool FileStat(const char* name, FileStat_Block* const stat_block);
	virtual void MediaChange() {}
	uint8_t GetMediaByte(void);
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
	Overlay_Drive(const char * startdir,const char* overlay, uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid,uint8_t &error, std::vector<std::string> &options);

	virtual bool FileOpen(DOS_File * * file,const char * name,uint32_t flags);
	virtual bool FileCreate(DOS_File * * file,const char * name,uint16_t /*attributes*/);
	virtual bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool FileUnlink(const char * name);
	virtual bool SetFileAttr(const char * name,uint16_t attr);
	virtual bool GetFileAttr(const char * name,uint16_t * attr);
	virtual bool FileExists(const char* name);
	virtual bool Rename(const char * oldname,const char * newname);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual void EmptyCache(void);

	FILE* create_file_in_overlay(const char* dos_filename, char const* mode);
	virtual Bits UnMount(void);
	virtual bool TestDir(const char * dir);
	virtual bool RemoveDir(const char * dir);
	virtual bool MakeDir(const char * dir);
	const char* getOverlaydir() {return overlaydir;};
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
