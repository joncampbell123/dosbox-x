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

#ifndef DOSBOX_BIOS_DISK_H
#define DOSBOX_BIOS_DISK_H

#include "dos_inc.h"
#include "logging.h"
#include "../src/dos/cdrom.h"

/* The Section handling Bios Disk Access */
#define BIOS_MAX_DISK 10

#define MAX_SWAPPABLE_DISKS 20
struct diskGeo {
	uint32_t ksize;  /* Size in kilobytes */
	uint16_t secttrack; /* Sectors per track */
	uint16_t headscyl;  /* Heads per cylinder */
	uint16_t cylcount;  /* Cylinders per side */
	uint16_t biosval;   /* Type to return from BIOS */
    uint16_t bytespersect; /* Bytes per sector */
	uint16_t rootentries;  /* Root directory entries */
	uint8_t sectcluster;   /* Sectors per cluster */
	uint8_t mediaid;       /* Media ID */
};
extern diskGeo DiskGeometryList[];

extern const uint8_t freedos_mbr[];

#define BytsPerSec 0x0b /* 2-bytes */
#define SecPerClus 0x0d /* 1-byte  */
#define RsvdSecCnt 0x0e /* 2-bytes */
#define NumFATs    0x10 /* 1-byte  */
#define RootEntCnt 0x11 /* 2-bytes */
#define TotSec16   0x13 /* 2-bytes */
#define Media      0x15 /* 1-byte  */
#define FATSz16    0x16 /* 2-bytes */
#define SecPerTrk  0x18 /* 2-bytes */
#define NumHeads   0x1a /* 2-bytes */
#define HiddSec    0x1c /* 4-bytes */
#define TotSec32   0x20 /* 4-bytes */
#define DrvNum     0x24 /* 1-byte  */
#define BootSig    0x26 /* 1-byte  */
#define VolID      0x27 /* 4-bytes */
#define VolLab     0x2b /* 11-bytes*/
#define FilSysType 0x36 /* 8-bytes */
#define BootCode   0x3e 
#define BootSign   0x1fe /*2-bytes */

#define FATSz32    0x24 /* 4-bytes */
#define DrvNum32   0x40 /* 1-byte  */
#define BootSig32  0x42 /* 1-bytes */
#define VolID32    0x43 /* 4-bytes */
#define VolLab32   0x47 /* 11-bytes*/
#define FilSysType32  0x52 /* 8-bytes */
#define BootCode32 0x5A

class imageDisk {
	public:
		enum IMAGE_TYPE {
			ID_BASE=0,
			ID_EL_TORITO_FLOPPY,
			ID_VFD,
			ID_MEMORY,
			ID_VHD,
			ID_D88,
			ID_NFD,
			ID_EMPTY_DRIVE,
			ID_INT13
		};

		virtual uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0);
		virtual uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0);
		virtual uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data);
		virtual uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data);

		virtual void UpdateFloppyType(void);
		virtual void Set_Reserved_Cylinders(Bitu resCyl);
		virtual uint32_t Get_Reserved_Cylinders();
		virtual void Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize);
		virtual void Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize);
		virtual uint8_t GetBiosType(void);
		virtual uint32_t getSectSize(void);
		imageDisk(class DOS_Drive *useDrive, unsigned int letter, uint32_t freeMB, int timeout);
		imageDisk(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk);
		imageDisk(FILE* diskimg, const char* diskName, uint32_t cylinders, uint32_t heads, uint32_t sectors, uint32_t sector_size, bool hardDrive);
		virtual ~imageDisk();
		void Set_GeometryForHardDisk();
		struct fatFromDOSDrive* ffdd = NULL;
		unsigned int drvnum = DOS_DRIVES;

		IMAGE_TYPE class_id = ID_BASE;
		std::string diskname;
		bool active = false;
		uint32_t sector_size = 512;
		uint32_t heads = 0;
		uint32_t cylinders = 0;
		uint32_t sectors = 0;
		bool hardDrive = false;
		uint64_t diskSizeK = 0;
		FILE* diskimg = NULL;
		bool diskChangeFlag = false;

		/* this is intended only for when the disk can change out from under us while mounted */
		virtual bool detectDiskChange(void) { const bool r = diskChangeFlag; diskChangeFlag = false; return r; }

	protected:
		imageDisk(IMAGE_TYPE class_id);
		uint8_t floppytype = 0;

		uint32_t reserved_cylinders = 0;
		uint64_t image_base = 0;
		uint64_t image_length = 0;

	private:
		volatile int refcount = 0;
		std::vector<bool> partition_in_use; /* used by FAT driver to prevent mounting a partition twice */
		uint64_t current_fpos = 0;

	public:
		int Addref() {
			return ++refcount;
		}
		int Release() {
			int ret = --refcount;
			if (ret < 0) {
				fprintf(stderr,"WARNING: imageDisk Release() changed refcount to %d\n",ret);
				abort();
			}
			if (ret == 0) delete this;
			return ret;
		}
		bool partitionInUse(const size_t i) {
			if (i < partition_in_use.size())
				return partition_in_use[i];

			return false;
		}
		bool partitionMarkUse(const size_t i,bool inUse) {
			if (i < 256) {
				if (partition_in_use.size() != 256) {
					partition_in_use.resize(256); // keep it simple, this is a bitfield, 256 bits = 32 bytes
					for (size_t i=0;i < partition_in_use.size();i++) partition_in_use[i] = false;
				}

				partition_in_use[i] = inUse;
			}

			return false;
		}
};

class imageDiskEmptyDrive : public imageDisk {
public:
	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	imageDiskEmptyDrive();
	virtual ~imageDiskEmptyDrive();
};

class imageDiskD88 : public imageDisk {
public:
	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	imageDiskD88(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk);
	virtual ~imageDiskD88();

    unsigned char fd_type_major;
    unsigned char fd_type_minor;

    enum { /* major */
        DISKTYPE_2D			= 0,
        DISKTYPE_2DD,
        DISKTYPE_2HD
    };

    struct vfdentry {
        uint8_t         track = 0,head = 0,sector = 0;
        uint16_t        sector_size = 0;

        uint32_t        data_offset = 0;
        uint32_t        entry_offset = 0; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() {}

        uint16_t getSectorSize(void) const {
            return sector_size;
        }
    };

    vfdentry *findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

class imageDiskNFD : public imageDisk {
public:
	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	imageDiskNFD(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk, unsigned int revision);
	virtual ~imageDiskNFD();

    struct vfdentry {
        uint8_t         track = 0,head = 0,sector = 0;
        uint16_t        sector_size = 0;

        uint32_t        data_offset = 0;
        uint32_t        entry_offset = 0; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() {}

        uint16_t getSectorSize(void) const {
            return sector_size;
        }
    };

    vfdentry *findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

class imageDiskVFD : public imageDisk {
public:
	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	imageDiskVFD(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk);
	virtual ~imageDiskVFD();

    struct vfdentry {
        uint8_t         track = 0,head = 0,sector = 0,sizebyte = 0;
        uint8_t         fillbyte = 0xFF;

        uint32_t        data_offset = 0;
        uint32_t        entry_offset = 0; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() {}

        bool hasSectorData(void) const {
            return fillbyte == 0xFF && data_offset != 0xFFFFFFFFUL;
        }

        bool hasFill(void) const {
            return fillbyte != 0xFF || data_offset == 0xFFFFFFFFUL;
        }

        uint16_t getSectorSize(void) const {
            return 128 << sizebyte;
        }
    };

    vfdentry *findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

class imageDiskMemory : public imageDisk {
public:
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;
	uint8_t GetBiosType(void) override;
	void Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) override;
	// Partition and format the ramdrive
	virtual uint8_t Format();

	// Create a hard drive image of a specified size; automatically select c/h/s
	imageDiskMemory(uint32_t imgSizeK);
	// Create a hard drive image of a specified geometry
	imageDiskMemory(uint16_t cylinders, uint16_t heads, uint16_t sectors, uint16_t sector_size);
	// Create a floppy image of a specified geometry
    imageDiskMemory(const diskGeo& floppyGeometry);
	// Create a copy-on-write memory image of an existing image
	imageDiskMemory(imageDisk* underlyingImage);
	virtual ~imageDiskMemory();

private:
	void init(diskGeo diskParams, bool isHardDrive, imageDisk* underlyingImage);
	bool CalculateFAT(uint32_t partitionStartingSector, uint32_t partitionLength, bool isHardDrive, uint32_t rootEntries, uint32_t* rootSectors, uint32_t* sectorsPerCluster, bool* isFat16, uint32_t* fatSectors, uint32_t* reservedSectors);

	uint8_t * * ChunkMap;
	uint32_t sectors_per_chunk;
	uint32_t chunk_size;
	uint32_t total_chunks;
    uint32_t total_sectors = 0;
    imageDisk* underlyingImage = NULL;

	diskGeo floppyInfo;
};

class imageDiskVHD : public imageDisk {
public:
	enum ErrorCodes : int
	{
		OPEN_SUCCESS = 0,
		ERROR_OPENING = 1,
		INVALID_DATA = 2,
		UNSUPPORTED_TYPE = 3,
		INVALID_MATCH = 4,
		INVALID_DATE = 5,
		UNSUPPORTED_WRITE = 6,
        UNSUPPORTED_SIZE = 7,
        ERROR_WRITING = 8,
		PARENT_ERROR = 0x10,
		ERROR_OPENING_PARENT = 0x11,
		PARENT_INVALID_DATA = 0x12,
		PARENT_UNSUPPORTED_TYPE = 0x13,
		PARENT_INVALID_MATCH = 0x14,
		PARENT_INVALID_DATE = 0x15
	};
	enum VHDTypes : uint32_t
	{
		VHD_TYPE_NONE = 0,
		VHD_TYPE_FIXED = 2,
		VHD_TYPE_DYNAMIC = 3,
		VHD_TYPE_DIFFERENCING = 4
	};
    struct Geometry {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    };
    struct VHDFooter {
        char cookie[8];
        uint32_t features;
        uint32_t fileFormatVersion;
        uint64_t dataOffset;
        uint32_t timeStamp;
        char creatorApp[4];
        uint32_t creatorVersion;
        uint32_t creatorHostOS;
        uint64_t originalSize;
        uint64_t currentSize;
        Geometry geometry;
        VHDTypes diskType;
        uint32_t checksum;
        char uniqueId[16];
        char savedState;
        char reserved[427];

        void SwapByteOrder();
        uint32_t CalculateChecksum();
        bool IsValid();
        void SetDefaults();
    };
    struct VHDInfo {
        uint32_t allocatedBlocks;
        uint32_t totalBlocks;
        uint32_t blockSize;
        float vhdSizeMB;
        uint32_t vhdType;
        VHDInfo *parentInfo = NULL;
        std::string diskname;
    };
    VHDTypes vhdType = VHD_TYPE_NONE;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;
	static ErrorCodes Open(const char* fileName, const bool readOnly, imageDisk** disk);
	static VHDTypes GetVHDType(const char* fileName);
	VHDTypes GetVHDType(void) const;
    static uint32_t GetInfo(const char* filename, VHDInfo** info);
    uint32_t GetInfo(VHDInfo* info);
    static uint32_t CreateFixed(const char* filename, uint64_t size); //dummy
    static uint32_t ConvertFixed(const char* filename);
    static uint32_t CreateDynamic(const char* filename, uint64_t size);
    static uint32_t CreateDifferencing(const char* filename, const char* basename);
    uint32_t CreateSnapshot();
    void DetectGeometry(Bitu sizes[]);
    static void DetectGeometry(uint8_t* buf, Bitu sizes[], uint64_t currentSize);
    static uint64_t scanMBR(uint8_t* mbr, Bitu sizes[], uint64_t disksize=0);
    bool MergeSnapshot(uint32_t* totalSectorsMerged, uint32_t* totalBlocksUpdated);
    static void SizeToCHS(uint64_t size, uint16_t* c, uint8_t* h, uint8_t* s);
    bool UpdateUUID();
    static void mk_uuid(uint8_t* buf);
    virtual ~imageDiskVHD();

private:
	struct ParentLocatorEntry {
		uint32_t platformCode;
		uint32_t platformDataSpace;
		uint32_t platformDataLength;
		uint32_t reserved;
		uint64_t platformDataOffset;
	};
	struct DynamicHeader {
		char cookie[8];
		uint64_t dataOffset;
		uint64_t tableOffset;
		uint32_t headerVersion;
		uint32_t maxTableEntries;
		uint32_t blockSize;
		uint32_t checksum;
		uint8_t parentUniqueId[16];
		uint32_t parentTimeStamp;
		uint32_t reserved;
		uint16_t parentUnicodeName[256];
		ParentLocatorEntry parentLocatorEntry[8];
		char reserved2[256];

		void SwapByteOrder();
		uint32_t CalculateChecksum();
		bool IsValid();
        void SetDefaults();
    };

	imageDiskVHD() : imageDisk(ID_VHD) { }
    static ErrorCodes TryOpenParent(const char* childFileName, const ParentLocatorEntry& entry, const uint8_t* data, const uint32_t dataLength, imageDisk** disk, const uint8_t* uniqueId);
	static ErrorCodes Open(const char* fileName, const bool readOnly, imageDisk** disk, const uint8_t* matchUniqueId);
	virtual bool loadBlock(const uint32_t blockNumber);
	static bool convert_UTF16_for_fopen(std::string &string, const void* data, const uint32_t dataLength);
    bool is_zeroed_sector(const void* data);
	bool is_block_allocated(uint32_t blockNumber);

    imageDisk* parentDisk = NULL;
    imageDisk* fixedDisk = NULL;
	uint64_t footerPosition = 0;
    VHDFooter footer = {};
    VHDFooter originalFooter = {};
    bool copiedFooter = false;
    DynamicHeader dynamicHeader = {};
	uint32_t sectorsPerBlock = 0;
	uint32_t blockMapSectors = 0;
	uint32_t blockMapSize = 0;
	uint32_t currentBlock = 0xFFFFFFFF;
    bool currentBlockAllocated = false;
	uint32_t currentBlockSectorOffset = 0;
	uint8_t* currentBlockDirtyMap = nullptr;
};

/* C++ class implementing El Torito floppy emulation */
class imageDiskElToritoFloppy : public imageDisk {
public:
    /* Read_Sector and Write_Sector take care of geometry translation for us,
     * then call the absolute versions. So, we override the absolute versions only */
    uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override {
        unsigned char buffer[2048];

        bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom);

        CDROM_Interface *src_drive=NULL;
        if (!GetMSCDEXDrive(CDROM_drive-'A',&src_drive)) return 0x05;

        if (!src_drive->ReadSectorsHost(buffer,false,cdrom_sector_offset+(sectnum>>2)/*512 byte/sector to 2048 byte/sector conversion*/,1))
            return 0x05;

        memcpy(data,buffer+((sectnum&3)*512),512);
        return 0x00;
    }
    uint8_t Write_AbsoluteSector(uint32_t sectnum,const void * data) override {
        (void)sectnum;//UNUSED
        (void)data;//UNUSED
        return 0x05; /* fail, read only */
    }
    imageDiskElToritoFloppy(unsigned char new_CDROM_drive,unsigned long new_cdrom_sector_offset,unsigned char floppy_emu_type) : imageDisk((FILE *)NULL,NULL,0,false), CDROM_drive(new_CDROM_drive), cdrom_sector_offset(new_cdrom_sector_offset), floppy_type(floppy_emu_type) {
        diskimg = NULL;
        sector_size = 512;
        class_id = ID_EL_TORITO_FLOPPY;

        if (floppy_emu_type == 1) { /* 1.2MB */
            heads = 2;
            cylinders = 80;
            sectors = 15;
        }
        else if (floppy_emu_type == 2) { /* 1.44MB */
            heads = 2;
            cylinders = 80;
            sectors = 18;
        }
        else if (floppy_emu_type == 3) { /* 2.88MB */
            heads = 2;
            cylinders = 80;
            sectors = 36; /* FIXME: right? */
        }
        else {
            heads = 2;
            cylinders = 69;
            sectors = 14;
            LOG_MSG("BUG! unsupported floppy_emu_type in El Torito floppy object\n");
        }

        diskSizeK = ((uint64_t)heads * cylinders * sectors * sector_size) / 1024;
        active = true;
    }
    virtual ~imageDiskElToritoFloppy() {
    }

    unsigned char CDROM_drive;
    unsigned long cdrom_sector_offset;
    unsigned char floppy_type;
/*
    int class_id;

    bool hardDrive;
    bool active;
    FILE *diskimg;
    std::string diskname;
    uint8_t floppytype;

    uint32_t sector_size;
    uint32_t heads,cylinders,sectors;
    uint32_t reserved_cylinders;
    uint64_t current_fpos; */
};

/* PC-98 IPL1 partition table entry.
 * Taken from GNU Parted source code.
 * Maximum 16 entries. */
#pragma pack(push,1)
struct _PC98RawPartition {
	uint8_t		mid;		/* 0x00: unused, 0x20: MS-DOS(not bootable), 0xa1-0xaf: MS-DOS(bootable) */
	uint8_t		sid;		/* 0x00: unused, 0x81,0x91,0xa1,0xe1: MS-DOS(active), 0x01,0x11,0x21,0x61: MS-DOS(sleep) */
	uint8_t		dum1;		/* dummy for padding */
	uint8_t		dum2;		/* dummy for padding */
	uint8_t		ipl_sect;	/* IPL sector */
	uint8_t		ipl_head;	/* IPL head */
	uint16_t	ipl_cyl;	/* IPL cylinder */
	uint8_t		sector;		/* starting sector */
	uint8_t		head;		/* starting head */
	uint16_t	cyl;		/* starting cylinder */
	uint8_t		end_sector;	/* end sector */
	uint8_t		end_head;	/* end head */
	uint16_t	end_cyl;	/* end cylinder */
	char		name[16];
};

struct partTable {
	uint8_t booter[446];
	struct partentry_t {
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
};
#pragma pack(pop)

void updateDPT(void);
void incrementFDD(void);

//in order to attach to the virtual IDE controllers, the disk must be mounted
//  in the BIOS first (the imageDiskList array), so the IDE controller can obtain
//  a reference to the drive in the imageDiskList array
#define MAX_HDD_IMAGES 4
#define MAX_DISK_IMAGES 6 //MAX_HDD_IMAGES + 2

extern bool imageDiskChange[MAX_DISK_IMAGES];
extern imageDisk *imageDiskList[MAX_DISK_IMAGES];
extern imageDisk *diskSwap[MAX_SWAPPABLE_DISKS];
extern int32_t swapPosition;
extern uint16_t imgDTASeg; /* Real memory location of temporary DTA pointer for fat image disk access */
extern RealPt imgDTAPtr; /* Real memory location of temporary DTA pointer for fat image disk access */
extern DOS_DTA *imgDTA;

void swapInDisks(int drive);
bool getSwapRequest(void);
imageDisk *GetINT13HardDrive(unsigned char drv);
imageDisk *GetINT13FloppyDrive(unsigned char drv);

bool PartitionLoadMBR(std::vector<partTable::partentry_t> &parts,imageDisk *loadedDisk);
bool PartitionLoadIPL1(std::vector<_PC98RawPartition> &parts,imageDisk *loadedDisk);
std::string PartitionIdentifyType(imageDisk *loadedDisk);

void LogPrintPartitionTable(const std::vector<_PC98RawPartition> &parts);
void LogPrintPartitionTable(const std::vector<partTable::partentry_t> &parts);

extern unsigned char INT13_ElTorito_NoEmuDriveNumber;
extern signed char INT13_ElTorito_IDEInterface;
extern char INT13_ElTorito_NoEmuCDROMDrive;

class imageDiskINT13Drive : public imageDisk {
public:
	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	void UpdateFloppyType(void) override;
	void Set_Reserved_Cylinders(Bitu resCyl) override;
	uint32_t Get_Reserved_Cylinders() override;
	void Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) override;
	void Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize) override;
	uint8_t GetBiosType(void) override;
	uint32_t getSectSize(void) override;
	bool detectDiskChange(void) override;

	imageDiskINT13Drive(imageDisk *sdisk);
	virtual ~imageDiskINT13Drive();

	uint8_t bios_disk = 0;
	bool enable_int13 = false;
	imageDisk* subdisk = NULL;
	bool busy = false;
};

#endif
