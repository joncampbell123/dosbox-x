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
	static void ChangeDisk(int drive, DOS_Drive* disk);
	static void InitializeDrive(int drive);
	static int UnmountDrive(int drive);
	static int GetDisksSize(int drive);
	static char * GetDrivePosition(int drive);
//	static void CycleDrive(bool pressed);
//	static void CycleDisk(bool pressed);
	static void CycleDisks(int drive, bool notify, unsigned int position=0);
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
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	virtual FILE *GetSystemFilePtr(char const * const name, char const * const type); 
	virtual bool GetSystemFilename(char* sysName, char const * const dosName); 
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes) override;
	bool FileUnlink(const char * name) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	bool TestDir(const char * dir) override;
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false) override;
	bool FindNext(DOS_DTA & dta) override;
	bool SetFileAttr(const char * name,uint16_t attr) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	bool GetFileAttrEx(char* name, struct stat *status) override;
	std::string GetHostName(const char * name);
	unsigned long GetCompressedSize(char* name) override;
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name) override;
	virtual unsigned long GetSerial();
#endif
	bool Rename(const char * oldname,const char * newname) override;
	bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) override;
    bool AllocationInfo64(uint32_t* _bytes_sector, uint32_t* _sectors_cluster, uint64_t* _total_clusters, uint64_t* _free_clusters) override;
	bool FileExists(const char* name) override;
	bool FileStat(const char* name, FileStat_Block * const stat_block) override;
	uint8_t GetMediaByte(void) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	Bits UnMount(void) override;
	char const * GetLabel() override {return dirCache.GetLabel();};
	void SetLabel(const char *label, bool iscdrom, bool updatable) override { dirCache.SetLabel(label,iscdrom,updatable); };
	void *opendir(const char *name) override;
	void closedir(void *handle) override;
	bool read_directory_first(void *handle, char* entry_name, char* entry_sname, bool& is_directory) override;
	bool read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory) override;
	virtual void remove_special_file_from_disk(const char* dosname, const char* operation);
	virtual std::string create_filename_of_special_operation(const char* dosname, const char* operation, bool expand);
	virtual bool add_special_file_to_disk(const char* dosname, const char* operation, uint16_t value, bool isdir);
	void EmptyCache(void) override { dirCache.EmptyCache(); };
	void MediaChange() override {};
	const char* getBasedir() const {return basedir;};
	struct {
		uint16_t bytes_sector;
		uint8_t sectors_cluster;
		uint16_t total_clusters;
		uint16_t free_clusters;
		uint8_t mediaid;
		unsigned long initfree;
	} allocation;
	int remote = -1;

private:
	const std::string special_prefix_local;

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
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes) override;
	bool FileUnlink(const char * name) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	bool TestDir(const char * dir) override;
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false) override;
	bool FindNext(DOS_DTA & dta) override;
	bool Rename(const char * oldname,const char * newname) override;
	bool SetFileAttr(const char * name,uint16_t attr) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	bool GetFileAttrEx(char* name, struct stat *status) override;
	unsigned long GetCompressedSize(char* name) override;
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name) override;
	unsigned long GetSerial() override;
#endif
	bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) override;
	bool FileExists(const char* name) override;
	bool FileStat(const char* name, FileStat_Block * const stat_block) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	void *opendir(const char *dir) override;
	void closedir(void *handle) override;
	bool read_directory_first(void *handle, char* entry_name, char* entry_sname, bool& is_directory) override;
	bool read_directory_next(void *handle, char* entry_name, char* entry_sname, bool& is_directory) override;
	const char *GetInfo(void) override;
	virtual const char *getOverlaydir(void);
	virtual bool setOverlaydir(const char * name);
	Bits UnMount() override;
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
	~fatDrive();
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes) override;
	bool FileUnlink(const char * name) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	bool TestDir(const char * dir) override;
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false) override;
	bool FindNext(DOS_DTA & dta) override;
	bool SetFileAttr(const char * name,uint16_t attr) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	bool GetFileAttrEx(char* name, struct stat *status) override;
	unsigned long GetCompressedSize(char* name) override;
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name) override;
#endif
	virtual unsigned long GetSerial();
	bool Rename(const char * oldname,const char * newname) override;
	bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) override;
	bool AllocationInfo32(uint32_t * _bytes_sector,uint32_t * _sectors_cluster,uint32_t * _total_clusters,uint32_t * _free_clusters) override;
	bool FileExists(const char* name) override;
	bool FileStat(const char* name, FileStat_Block * const stat_block) override;
	uint8_t GetMediaByte(void) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	Bits UnMount(void) override;
public:
	struct clusterChainMemory {
		uint32_t	current_cluster_no = 0;
		uint32_t	current_cluster_index = 0;

		void clear(void);
	};
public:
	uint8_t readSector(uint32_t sectnum, void * data);
	uint8_t writeSector(uint32_t sectnum, void * data);
	uint32_t getAbsoluteSectFromBytePos(uint32_t startClustNum, uint32_t bytePos,clusterChainMemory *ccm=NULL);
	uint32_t getSectorCount(void);
	uint32_t getSectorSize(void);
	uint32_t getClusterSize(void);
	uint32_t getAbsoluteSectFromChain(uint32_t startClustNum, uint32_t logicalSector,clusterChainMemory *ccm=NULL);
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
	uint32_t partSectOff;
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
	bool iseofFAT(const uint32_t cv) const;
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
	uint32_t partSectSize = 0;
	uint32_t firstDataSector = 0;
	uint32_t firstRootDirSect = 0;
	uint32_t physToLogAdj = 0; // Some PC-98 HDI images have larger logical than physical bytes/sector and the partition is not a multiple of it, so this is needed
	uint32_t searchFreeCluster = 0;
	int partition_index = -1;

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
	uint32_t GetSectorCount(void) override;
	uint32_t GetSectorSize(void) override;
	uint8_t Read_AbsoluteSector_INT25(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector_INT25(uint32_t sectnum, void * data) override;
	void UpdateDPB(unsigned char dos_drive) override;

	char const * GetLabel() override {return labelCache.GetLabel();};
	void SetLabel(const char *label, bool iscdrom, bool updatable) override;
	virtual void UpdateBootVolumeLabel(const char *label);
	virtual uint32_t GetPartitionOffset(void);
	virtual uint32_t GetFirstClusterOffset(void);
	virtual uint32_t GetHighestClusterNumber(void);

	void checkDiskChange(void);

	unsigned char bios_disk = 0;
	bool unformatted = false;
};

PhysPt DOS_Get_DPB(unsigned int dos_drive);

class cdromDrive : public localDrive
{
public:
	cdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options);
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes) override;
	bool FileUnlink(const char * name) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	bool Rename(const char * oldname,const char * newname) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	bool GetFileAttrEx(char* name, struct stat *status) override;
	unsigned long GetCompressedSize(char* name) override;
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name) override;
	unsigned long GetSerial() override;
#endif
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false) override;
	void SetDir(const char* path) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	Bits UnMount(void) override;
private:
	uint8_t subUnit = 0;	char driveLetter = '\0';
};

class physfscdromDrive : public physfsDrive
{
public:
	physfscdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options);
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes) override;
	bool FileUnlink(const char * name) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	bool Rename(const char * oldname,const char * newname) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst=false) override;
	void SetDir(const char* path) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	Bits UnMount(void) override;
	const char *GetInfo(void) override;
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

////////////////////////////////////

/* UDF checksum function */
uint16_t UDF_crc_itu_t(uint16_t crc, const uint8_t *buffer, size_t len);

extern const uint16_t UDF_crc_itu_t_table[256];

static inline uint16_t UDF_crc_itu_t_byte(uint16_t crc, const uint8_t data)
{
        return (crc << 8) ^ UDF_crc_itu_t_table[((crc >> 8) ^ data) & 0xff];
}

////////////////////////////////////

using UDF_blob_base = std::vector<uint8_t>;

class UDF_blob : public UDF_blob_base {
        public:
                UDF_blob() : UDF_blob_base() { }
                ~UDF_blob() { }
	public:
                UDF_blob(const std::string &x) : UDF_blob_base() {
                        resize(x.size());
                        uint8_t *p = &UDF_blob_base::operator[](0);
                        for (size_t i=0;i < x.size();i++) p[i] = x[i]; /* Not the NUL at the end, though */
                }
                std::string string_value(void) const {
                        return std::string((char*)(&UDF_blob_base::operator[](0)),UDF_blob_base::size());
                }
                UDF_blob& operator=(const std::vector<uint8_t> &s) {
                        *((UDF_blob_base*)this) = s;
                        return *this;
                }
};

////////////////////////////////////

struct UDFTagId { /* ECMA-167 7.2.1 */
	uint16_t				TagIdentifier = 0;					/*   @0 +   2 uint16_t */
	uint16_t				DescriptorVersion = 0;					/*   @2 +   2 uint16_t */
	uint8_t					TagChecksum = 0;					/*   @4 +   1 uint8_t */
	uint8_t					Reserved = 0;						/*   @5 +   1 uint8_t */
	uint16_t				TagSerialNumber = 0;					/*   @6 +   2 uint16_t */
	uint16_t				DescriptorCRC = 0;					/*   @8 +   2 uint16_t */
	uint16_t				DescriptorCRCLength = 0;				/*  @10 +   2 uint16_t */
	uint32_t				TagLocation = 0;					/*  @12 +   4 uint32_t */

	bool					get(const unsigned int sz,const unsigned char *b);
	void					parse(const unsigned int sz,const unsigned char *b);
	bool					tagChecksumOK(const unsigned int sz,const unsigned char *b) const;
	bool					dataChecksumOK(const unsigned int sz,const unsigned char *b) const;
	bool					checksumOK(const unsigned int sz,const unsigned char *b);
						UDFTagId(const unsigned int sz,const unsigned char *b);
						UDFTagId();
};													/*  =16 */

////////////////////////////////////

struct UDFextent_ad { /* ECMA-167 3/7.1 */
	uint32_t				ExtentLength = 0;					/*   @0 +   4 uint32_t */
	uint32_t				ExtentLocation = 0;					/*   @4 +   4 uint32_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFextent_ad(const unsigned int sz,const unsigned char *b);
						UDFextent_ad();
};													/*   =8 */

////////////////////////////////////

struct UDFAnchorVolumeDescriptorPointer {
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=2 */
	UDFextent_ad				MainVolumeDescriptorSequenceExtent;			/*  @16 +   8 extent_ad */
	UDFextent_ad				ReserveVolumeDescriptorSequenceExtent;			/*  @24 +   8 extent_ad */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFAnchorVolumeDescriptorPointer(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFAnchorVolumeDescriptorPointer();
};													/*  =32 */

////////////////////////////////////

struct UDFdstring : public UDF_blob {
	void					get(const unsigned int sz,const unsigned char *b);
						UDFdstring(const unsigned int sz,const unsigned char *b);
						UDFdstring();
};

////////////////////////////////////

/* NTS: The structure is the same, what the location/position is relative to is different. It's relative to the partition */
struct UDFshort_ad { /* ECMA-167 4/14.14.1 */
	uint32_t				ExtentLength = 0;					/*   @0 +   4 uint32_t */
	uint32_t				ExtentPosition = 0;					/*   @4 +   4 uint32_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFshort_ad(const unsigned int sz,const unsigned char *b);
						UDFshort_ad();
};													/*   =8 */

////////////////////////////////////

struct UDFlb_addr { /* ECMA-167 4/7.1 */
	uint32_t				LogicalBlockNumber = 0;					/*   @0 +   4 uint32_t */
	uint16_t				PartitionReferenceNumber = 0;				/*   @4 +   2 uint16_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFlb_addr(const unsigned int sz,const unsigned char *b);
						UDFlb_addr();
};													/*   =6 */

////////////////////////////////////

struct UDFlong_ad { /* ECMA-167 4/14.14.2 */
	uint32_t				ExtentLength = 0;					/*   @0 +   4 uint32_t */
	UDFlb_addr				ExtentLocation;						/*   @4 +   6 lb_addr */
	uint8_t					ImplementationUse[6];					/*  @10 +   6 uint8_t */

	/* NTS: In the UDF 1.02 standard, ImplementationUse is:
	 *
	 * uint16_t flags
	 * uint8_t  impUse[4];
	 *
	 * This is used to define if an extent is erased. We don't care right now */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFlong_ad(const unsigned int sz,const unsigned char *b);
						UDFlong_ad();
};													/*  =16 */

////////////////////////////////////

struct UDFcharspec { /* ECMA-167 7.2.1 */
	uint8_t					CharacterSetType;					/*   @0 +   1 uint8_t */
	uint8_t					CharacterSetInformation[63];				/*   @1 +  63 uint8_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFcharspec(const unsigned int sz,const unsigned char *b);
						UDFcharspec();
};													/*  =64 */

////////////////////////////////////

struct UDFregid { /* ECMA-167 7.4 */
	uint8_t					Flags = 0;						/*   @0 +   0 uint8_t */
	uint8_t					Identifier[23];						/*   @1 +  23 uint8_t */
	uint8_t					IdentifierSuffix[8];					/*  @24 +   8 uint8_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFregid(const unsigned int sz,const unsigned char *b);
						UDFregid();
};													/*  =32 */

////////////////////////////////////

struct UDFtimestamp { /* ECMA-167 7.3 */
	uint16_t				TypeAndTimeZone = 0;					/*   @0 +   2 uint16_t */
	int16_t					Year = 0;						/*   @2 +   2 int16_t */
	uint8_t					Month = 0;						/*   @4 +   1 uint8_t */
	uint8_t					Day = 0;						/*   @5 +   1 uint8_t */
	uint8_t					Hour = 0;						/*   @6 +   1 uint8_t */
	uint8_t					Minute = 0;						/*   @7 +   1 uint8_t */
	uint8_t					Second = 0;						/*   @8 +   1 uint8_t */
	uint8_t					Centiseconds = 0;					/*   @9 +   1 uint8_t */
	uint8_t					HundredsOfMicroseconds = 0;				/*  @10 +   1 uint8_t */
	uint8_t					Microseconds = 0;					/*  @11 +   1 uint8_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFtimestamp(const unsigned int sz,const unsigned char *b);
						UDFtimestamp();
};													/*  =12 */

////////////////////////////////////

struct UDFPrimaryVolumeDescriptor {
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=1 */
	uint32_t				VolumeDescriptorSequenceNumber = 0;			/*  @16 +   4 uint32_t */
	uint32_t				PrimaryVolumeDescriptorNumber = 0;			/*  @20 +   4 uint32_t */
	UDFdstring				VolumeIdentifier;					/*  @24 +  32 dstring */
	uint16_t				VolumeSequenceNumber = 0;				/*  @56 +   2 uint16_t */
	uint16_t				MaximumVolumeSequenceNumber = 0;			/*  @58 +   2 uint16_t */
	uint16_t				InterchangeLevel = 0;					/*  @60 +   2 uint16_t */
	uint16_t				MaximumInterchangeLevel = 0;				/*  @62 +   2 uint16_t */
	uint32_t				CharacterSetList = 0;					/*  @64 +   4 uint32_t */
	uint32_t				MaximumCharacterSetList = 0;				/*  @68 +   4 uint32_t */
	UDFdstring				VolumeSetIdentifier;					/*  @72 + 128 dstring */
	UDFcharspec				DescriptorCharacterSet;					/* @200 +  64 charspec */
	UDFcharspec				ExplanatoryCharacterSet;				/* @264 +  64 charspec */
	UDFextent_ad				VolumeAbstract;						/* @328 +   8 extent_ad */
	UDFextent_ad				VolumeCopyrightNotice;					/* @336 +   8 extent_ad */
	UDFregid				ApplicationIdentifier;					/* @344 +  32 regid */
	UDFtimestamp				RecordingDateAndTime;					/* @376 +  12 timestamp */
	UDFregid				ImplementationIdentifier;				/* @388 +  32 regid */
	uint8_t					ImplementationUse[64];					/* @420 +  64 bytes */
	uint32_t				PredecessorVolumeDescriptorSequenceLocation = 0;	/* @484 +   4 uint32_t */
	uint16_t				Flags = 0;						/* @488 +   2 uint16_t */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFPrimaryVolumeDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFPrimaryVolumeDescriptor();
};													/* =490 */

////////////////////////////////////

struct UDFPartitionDescriptor {
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=5 */
	uint32_t				VolumeDescriptorSequenceNumber = 0;			/*  @16 +   4 uint32_t */
	uint16_t				PartitionFlags = 0;					/*  @20 +   2 uint16_t */
	uint16_t				PartitionNumber = 0;					/*  @22 +   2 uint16_t */
	UDFregid				PartitionContents;					/*  @24 +  32 regid */
	uint8_t					PartitionContentsUse[128];				/*  @56 + 128 uint8_t */
	uint32_t				AccessType = 0;						/* @184 +   4 uint32_t */
	uint32_t				PartitionStartingLocation = 0;				/* @188 +   4 uint32_t */
	uint32_t				PartitionLength = 0;					/* @192 +   4 uint32_t */
	UDFregid				ImplementationIdentifier;				/* @196 +  32 regid */
	uint8_t					ImplementationUse[128];					/* @228 + 128 uint8_t */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFPartitionDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFPartitionDescriptor();
};													/* =356 */

////////////////////////////////////

struct UDFLogicalVolumeDescriptor {
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=6 */
	uint32_t				VolumeDescriptorSequenceNumber = 0;			/*  @16 +   4 uint32_t */
	UDFcharspec				DescriptorCharacterSet;					/*  @20 +  64 charspec */
	UDFdstring				LogicalVolumeIdentifier;				/*  @84 + 128 dstring */
	uint32_t				LogicalBlockSize = 0;					/* @212 +   4 uint32_t */
	UDFregid				DomainIdentifier;					/* @216 +  32 regid */
	uint8_t					LogicalVolumeContentsUse[16];				/* @248 +  16 uint8_t */
	uint32_t				MapTableLength = 0;					/* @264 +   4 uint32_t */
	uint32_t				NumberOfPartitionMaps = 0;				/* @268 +   4 uint32_t */
	UDFregid				ImplementationIdentifier;				/* @272 +  32 regid */
	uint8_t					ImplementationUse[128];					/* @304 + 128 uint8_t */
	UDFextent_ad				IntegritySequenceExtent;				/* @432 +   8 extent_ad */
	std::vector<uint8_t>			PartitionMaps;						/* @440 + MapTableLength */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFLogicalVolumeDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFLogicalVolumeDescriptor();
};													/* =440 + MapTableLength */

////////////////////////////////////

struct UDFFileSetDescriptor { /* ECMA-167 4/14.1 */
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=256 */
	UDFtimestamp				RecordingDateAndType;					/*  @16 +  12 timestamp */
	uint16_t				InterchangeLevel = 0;					/*  @28 +   2 uint16_t */
	uint16_t				MaximumInterchangeLevel = 0;				/*  @30 +   2 uint16_t */
	uint32_t				CharacterSetList = 0;					/*  @32 +   4 uint32_t */
	uint32_t				MaximumCharacterSetList = 0;				/*  @36 +   4 uint32_t */
	uint32_t				FileSetNumber = 0;					/*  @40 +   4 uint32_t */
	uint32_t				FileSetDescriptorNumber = 0;				/*  @44 +   4 uint32_t */
	UDFcharspec				LogicalVolumeIdentifierCharacterSet;			/*  @48 +  64 charspec */
	UDFdstring				LogicalVolumeIdentifier;				/* @112 + 128 dstring */
	UDFcharspec				FileSetCharacterSet;					/* @240 +  64 charspec */
	UDFdstring				FileSetIdentifier;					/* @304 +  32 dstring */
	UDFdstring				CopyrightFileIdentifier;				/* @336 +  32 dstring */
	UDFdstring				AbstractFileIdentifier;					/* @368 +  32 dstring */
	UDFlong_ad				RootDirectoryICB;					/* @400 +  16 long_ad */
	UDFregid				DomainIdentifier;					/* @416 +  32 regid */
	UDFlong_ad				NextExtent;						/* @448 +  16 long_ad */
	UDFlong_ad				SystemStreamDirectoryICB;				/* @464 +  16 long_ad */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFFileSetDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFFileSetDescriptor();
};													/* =480 */

////////////////////////////////////

struct UDFext_ad { /* ECMA-167 4/14.14.3 */
	uint32_t				ExtentLength = 0;					/*   @0 +   4 uint32_t */
	uint32_t				RecordedLength = 0;					/*   @4 +   4 uint32_t */
	uint32_t				InformationLength = 0;					/*   @8 +   4 uint32_t */
	UDFlb_addr				ExtentLocation;						/*  @12 +   6 lb_addr */
	uint8_t					ImplementationUse[2];					/*  @18 +   2 uint8_t */

	void					get(const unsigned int sz,const unsigned char *b);
						UDFext_ad(const unsigned int sz,const unsigned char *b);
						UDFext_ad();
};													/*  =20 */

////////////////////////////////////

struct UDFicbtag { /* ECMA-167 4/14.6 */
	uint32_t				PriorRecordedNumberOfDirectEntries = 0;			/*   @0 +   4 uint32_t */
	uint16_t				StrategyType = 0;					/*   @4 +   2 uint16_t */
	uint8_t					StrategyParameter[2];					/*   @6 +   2 uint8_t */
	uint16_t				MaximumNumberOfEntries = 0;				/*   @8 +   2 uint16_t */
	uint8_t					Reserved = 0;						/*  @10 +   1 uint8_t */
	uint8_t					FileType = 0;						/*  @11 +   1 uint8_t */
	UDFlb_addr				ParentICBLocation;					/*  @12 +   6 lb_addr */
	uint16_t				Flags = 0;						/*  @18 +   2 uint16 */

	uint8_t					AllocationDescriptorType(void) const;
	void					get(const unsigned int sz,const unsigned char *b);
						UDFicbtag(const unsigned int sz,const unsigned char *b);
						UDFicbtag();
};													/*  =20 */

////////////////////////////////////

struct UDFFileEntry { /* ECMA-167 4/14.9 */
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=261 */
	UDFicbtag				ICBTag;							/*  @16 +  20 icbtag */
	uint32_t				Uid = 0;						/*  @36 +   4 uint32_t */
	uint32_t				Gid = 0;						/*  @40 +   4 uint32_t */
	uint32_t				Permissions = 0;					/*  @44 +   4 uint32_t */
	uint16_t				FileLinkCount = 0;					/*  @48 +   2 uint16_t */
	uint8_t					RecordFormat = 0;					/*  @50 +   1 uint8_t */
	uint8_t					RecordDisplayAttributes = 0;				/*  @51 +   1 uint8_t */
	uint32_t				RecordLength = 0;					/*  @52 +   4 uint32_t */
	uint64_t				InformationLength = 0;					/*  @56 +   8 uint64_t */
	uint64_t				LogicalBlocksRecorded = 0;				/*  @64 +   8 uint64_t */
	UDFtimestamp				AccessDateAndTime;					/*  @72 +  12 timestamp */
	UDFtimestamp				ModificationDateAndTime;				/*  @84 +  12 timestamp */
	UDFtimestamp				AttributeDateAndTime;					/*  @96 +  12 timestamp */
	uint32_t				Checkpoint = 0;						/* @108 +   4 uint32_t */
	UDFlong_ad				ExtendedAttributeICB;					/* @112 +  16 long_ad */
	UDFregid				ImplementationIdentifier;				/* @128 +  32 regid */
	uint64_t				UniqueId = 0;						/* @160 +   8 uint64_t */
	uint32_t				LengthOfExtendedAttributes = 0;				/* @168 +   4 uint32_t */
	uint32_t				LengthOfAllocationDescriptors = 0;			/* @172 +   4 uint32_t */

	// TODO: Extended Attributes @176

	/* NTS: ECMA-167 describes this section as clear as mud, until you finally read the icbtag Flags and realize that
	 *      the low 3 bits define it as an array of 0=short_ad 1=long_ad 2=ext_ad 3=the file contents take place of
	 *      the allocation descriptors. */
	std::vector<UDFshort_ad>		AllocationDescriptors_short_ad;				/* @172+L_EA (ICBTag.Flags&7) == 0 */
	std::vector<UDFlong_ad>			AllocationDescriptors_long_ad;				/* @172+L_EA (ICBTag.Flags&7) == 1 */
	std::vector<UDFext_ad>			AllocationDescriptors_ext_ad;				/* @172+L_EA (ICBTag.Flags&7) == 2 */
	std::vector<uint8_t>			AllocationDescriptors_file;				/* @172+L_EA (ICBTag.Flags&7) == 3 */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFFileEntry(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFFileEntry();
};

////////////////////////////////////

struct UDFFileIdentifierDescriptor { /* ECMA-167 4/14.4 */
	UDFTagId				DescriptorTag;						/*   @0 +  16 tag ID=257 */
	uint16_t				FileVersionNumber = 0;					/*  @16 +   2 uint16_t */
	uint8_t					FileCharacteristics = 0;				/*  @18 +   1 uint8_t */
	uint8_t					LengthOfFileIdentifier = 0;				/*  @19 +   1 uint8_t */
	UDFlong_ad				ICB;							/*  @20 +  16 long_ad */
	uint16_t				LengthOfImplementationUse = 0;				/*  @36 +   2 uint16_t */
	std::vector<uint8_t>			ImplementationUse;					/*  @38 + L_IU bytes */
	UDF_blob				FileIdentifier;						/*  @38+L_IU + L_FI bytes */

	void					get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFFileIdentifierDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b);
						UDFFileIdentifierDescriptor();
};													/*  =38+L_IU+L_FI */

////////////////////////////////////

struct UDFextent {
	struct UDFextent_ad ex;

	UDFextent();
	UDFextent(const struct UDFextent_ad &s);
};

struct UDFextents {
	std::vector<struct UDFextent> xl;

	// extents stored within extent area
	bool is_indata = false;
	std::vector<uint8_t> indata;

	// current position
	uint32_t		relofs = 0;	// offset within extent
	uint64_t		extofs = 0;	// base offset of extent
	size_t			extent = 0;	// which extent
	uint64_t		filesz = 0;	// file size

	std::vector<uint8_t>	sector_buffer;
	uint32_t		sector_buffer_n = 0xFFFFFFFFu;

				UDFextents();
				UDFextents(const struct UDFextent_ad &s);
};

////////////////////////////////////

class isoDrive : public DOS_Drive {
public:
	isoDrive(char driveLetter, const char* fileName, uint8_t mediaid, int &error, std::vector<std::string>& options);
	~isoDrive();
	bool FileOpen(DOS_File **file, const char *name, uint32_t flags) override;
	bool FileCreate(DOS_File **file, const char *name, uint16_t attributes) override;
	bool FileUnlink(const char *name) override;
	bool RemoveDir(const char *dir) override;
	bool MakeDir(const char *dir) override;
	bool TestDir(const char *dir) override;
	bool FindFirst(const char *dir, DOS_DTA &dta, bool fcb_findfirst) override;
	bool FindNext(DOS_DTA &dta) override;
	bool SetFileAttr(const char *name,uint16_t attr) override;
	bool GetFileAttr(const char *name, uint16_t *attr) override;
	bool GetFileAttrEx(char* name, struct stat *status) override;
	unsigned long GetCompressedSize(char* name) override;
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name) override;
#endif
	bool Rename(const char * oldname,const char * newname) override;
	bool AllocationInfo(uint16_t *bytes_sector, uint8_t *sectors_cluster, uint16_t *total_clusters, uint16_t *free_clusters) override;
	bool FileExists(const char *name) override;
	bool FileStat(const char *name, FileStat_Block *const stat_block) override;
	uint8_t GetMediaByte(void) override;
	void EmptyCache(void) override;
	void MediaChange(void) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	Bits UnMount(void) override;
	bool loadImage();
	bool loadImageUDF();
	bool loadImageUDFAnchorVolumePointer(UDFAnchorVolumeDescriptorPointer &avdp,uint8_t *pvd/*COOKED_SECTOR_SIZE*/,uint32_t sector) const;
	bool readSector(uint8_t *buffer, uint32_t sector) const;
	void setFileName(const char* fileName);
	char const* GetLabel(void) override {return discLabel;};
	void Activate(void) override;
private:
    int  readDirEntry(isoDirEntry* de, const uint8_t* data, unsigned int direntindex) const;
	bool lookup(isoDirEntry *de, const char *path);
	bool lookup(UDFFileIdentifierDescriptor &fid, UDFFileEntry &fe, const char *path);
	int  UpdateMscdex(char driveLetter, const char* path, uint8_t& subUnit);
	int  GetDirIterator(const isoDirEntry* de);
	int  GetDirIterator(const UDFFileEntry &fe);
	bool GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de);
	void FreeDirIterator(const int dirIterator);
	bool ReadCachedSector(uint8_t** buffer, const uint32_t sector);

	int nextFreeDirIterator;
	
	struct SectorHashEntry {
		bool valid;
		uint32_t sector;
		uint8_t data[ISO_FRAMESIZE];
	} sectorHashEntries[ISO_MAX_HASH_TABLE_SIZE];

    bool iso = false;
    bool dataCD = false;
    bool is_udf = false;
    bool is_joliet = false;
    bool empty_drive = false;
    bool is_rock_ridge = false; // NTS: Rock Ridge and System Use Sharing Protocol was detected in the root directory
    bool enable_joliet = false; // NTS: "Joliet" is just ISO 9660 with filenames encoded as UTF-16 Unicode. One of the few times Microsoft extended something yet kept it simple --J.C.
    bool enable_rock_ridge = false; // NTS: Windows 95/98 are unlikely to support Rock Ridge, therefore this is off by default. If they do support RR, let me know --J.C.
    bool enable_udf = false; // NTS: Windows 98 is said to have added UDF support
	isoDirEntry rootEntry;
    uint8_t mediaid = 0;
	char fileName[CROSS_LEN];
	uint8_t rr_susp_skip = 0;
    uint8_t subUnit = 0;
    char driveLetter = '\0';
	char discLabel[32];
public:
	UDFextent_ad convertToUDFextent_ad(const UDFshort_ad &s,const uint32_t partition_ref_id=0xFFFFFFFFu) const;
	UDFextent_ad convertToUDFextent_ad(const UDFextent_ad &s) const;
	UDFextent_ad convertToUDFextent_ad(const UDFlong_ad &s) const;
	UDFextent_ad convertToUDFextent_ad(const UDFext_ad &s) const;
public:
	bool convertToUDFextent_ad(UDFextent_ad &d,const UDFshort_ad &s,const uint32_t partition_ref_id=0xFFFFFFFFu) const;
	bool convertToUDFextent_ad(UDFextent_ad &d,const UDFextent_ad &s) const;
	bool convertToUDFextent_ad(UDFextent_ad &d,const UDFlong_ad &s) const;
	bool convertToUDFextent_ad(UDFextent_ad &d,const UDFext_ad &s) const;
private:
	UDFLogicalVolumeDescriptor					lvold;
	UDFPrimaryVolumeDescriptor					pvold;
	UDFFileSetDescriptor						fsetd;
	UDFPartitionDescriptor						partd;
public:
	void UDFextent_rewind(struct UDFextents &ex) const;
	void UDFFileEntryToExtents(UDFextents &ex,UDFFileEntry &fe) const;
	uint64_t UDFextent_seek(struct UDFextents &ex,uint64_t ofs) const;
	unsigned int UDFextent_read(struct UDFextents &ex,unsigned char *buf,size_t count) const;
	uint64_t UDFtotalsize(struct UDFextents &ex) const;
private:
	struct DirIterator {
		bool valid;
		bool root;
		uint32_t currentSector;
		uint32_t endSector;
		uint32_t index;
		uint32_t pos;
		UDFextents udfdirext;
    } dirIterators[MAX_OPENDIRS] = {};
private:
	bool GetNextDirEntry(const int dirIteratorHandle, UDFFileIdentifierDescriptor &fid, UDFFileEntry &fe, UDFextents &dirext, char fname[LFN_NAMELENGTH],unsigned int dirIteratorIndex);
};

struct VFILE_Block;

class Virtual_Drive: public DOS_Drive {
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	bool FileCreate(DOS_File * * file,const char * name,uint16_t attributes) override;
	bool FileUnlink(const char * name) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	bool TestDir(const char * dir) override;
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) override;
	bool FindNext(DOS_DTA & dta) override;
	bool SetFileAttr(const char * name,uint16_t attr) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	bool GetFileAttrEx(char* name, struct stat *status) override;
	unsigned long GetCompressedSize(char* name) override;
#if defined (WIN32)
	HANDLE CreateOpenFile(char const* const name) override;
#endif
	bool Rename(const char * oldname,const char * newname) override;
	bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) override;
	bool FileExists(const char* name) override;
	bool FileStat(const char* name, FileStat_Block* const stat_block) override;
	void MediaChange() override {}
	uint8_t GetMediaByte(void) override;
	void EmptyCache(void) override;
	bool isRemote(void) override;
	bool isRemovable(void) override;
	Bits UnMount(void) override;
	char const* GetLabel(void) override;
private:
	VFILE_Block* search_file = nullptr;
};

class Overlay_Drive: public localDrive {
public:
	Overlay_Drive(const char * startdir,const char* overlay, uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid,uint8_t &error, std::vector<std::string> &options);

	bool FileOpen(DOS_File * * file,const char * name,uint32_t flags) override;
	bool FileCreate(DOS_File * * file,const char * name,uint16_t /*attributes*/) override;
	bool FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) override;
	bool FindNext(DOS_DTA & dta) override;
	bool FileUnlink(const char * name) override;
	bool SetFileAttr(const char * name,uint16_t attr) override;
	bool GetFileAttr(const char * name,uint16_t * attr) override;
	std::string GetHostName(const char * name);
	bool FileExists(const char* name) override;
	bool Rename(const char * oldname,const char * newname) override;
	bool FileStat(const char* name, FileStat_Block * const stat_block) override;
	void EmptyCache(void) override;

	FILE* create_file_in_overlay(const char* dos_filename, char const* mode);
	Bits UnMount(void) override;
	bool TestDir(const char * dir) override;
	bool RemoveDir(const char * dir) override;
	bool MakeDir(const char * dir) override;
	const char* getOverlaydir() const {return overlaydir;};
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


	void remove_special_file_from_disk(const char* dosname, const char* operation) override;
	bool add_special_file_to_disk(const char* dosname, const char* operation, uint16_t value = 0, bool isdir = false) override;
	std::string create_filename_of_special_operation(const char* dosname, const char* operation, bool expand = false) override;
	void convert_overlay_to_DOSname_in_base(char* dirname );
	//For caching the update_cache routine.
	std::vector<std::string> DOSnames_cache; //Also set is probably better.
	std::vector<std::string> DOSdirs_cache; //Can not blindly change its type. it is important that subdirs come after the parent directory.
	const std::string special_prefix;
};

int get_expanded_files(const std::string &path, std::vector<std::string> &paths, bool readonly);

/* No LFN filefind in progress (SFN call). This index is out of range and meant to indicate no LFN call in progress. */
#define LFN_FILEFIND_NONE           258
/* FAT image handle */
#define LFN_FILEFIND_IMG            256
/* Internal handle */
#define LFN_FILEFIND_INTERNAL       255
/* Highest valid handle */
#define LFN_FILEFIND_MAX            255

#if defined(WIN32)
// Windows: Use UTF-16 (wide char)
// TODO: Offer an option to NOT use wide char on Windows if directed by config.h
//       for people who compile this code for Windows 95 or earlier where some
//       widechar functions are missing.
typedef wchar_t host_cnv_char_t;
# define host_cnv_use_wchar
# define _HT(x) L##x
# if defined(HX_DOS) || defined(_WIN32_WINDOWS)
#  define ht_stat_t struct _stat
#  define ht_stat(x,y) _wstat(x,y)
# elif defined(__MINGW32__)
#  define ht_stat_t struct __stat64
#  define ht_stat(x,y) _wstat64(x,y)
# else
#  define ht_stat_t struct _stat64 /* WTF Microsoft?? Why aren't _stat and _wstat() consistent on stat struct type? */
#  define ht_stat(x,y) _wstat64(x,y)
# endif
# define ht_access(x,y) _waccess(x,y)
# define ht_strdup(x) _wcsdup(x)
# define ht_unlink(x) _wunlink(x)
#else
// Linux: Use UTF-8
typedef char host_cnv_char_t;
# define _HT(x) x
# define ht_stat_t struct stat
# define ht_stat(x,y) stat(x,y)
# define ht_access(x,y) access(x,y)
# define ht_strdup(x) strdup(x)
# define ht_unlink(x) unlink(x)
#endif

#if defined (WIN32) || defined (OS2)				/* Win 32 & OS/2*/
#define CROSS_DOSFILENAME(blah)
#else
//Convert back to DOS PATH
#define	CROSS_DOSFILENAME(blah) strreplace(blah,'/','\\')
#endif

#endif
