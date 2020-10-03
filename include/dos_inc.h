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


#ifndef DOSBOX_DOS_INC_H
#define DOSBOX_DOS_INC_H

#include <stddef.h>
#define CTBUF 127

#ifndef DOSBOX_DOS_SYSTEM_H
#include "dos_system.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif
#include <stddef.h> //for offsetof

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct CommandTail{
  uint8_t count;				/* number of bytes returned */
  char buffer[CTBUF];		/* the buffer itself */
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

extern uint16_t first_umb_seg;
extern uint16_t first_umb_size;

bool MEM_unmap_physmem(Bitu start,Bitu end);
bool MEM_map_RAM_physmem(Bitu start,Bitu end);

struct BuiltinFileBlob {
	const char		*recommended_file_name;
	const unsigned char	*data;
	size_t			length;
};

struct DOS_Date {
	uint16_t year;
	uint8_t month;
	uint8_t day;
};

struct DOS_Version {
	uint8_t major,minor,revision;
};

#ifndef MACOSX
#if defined (__APPLE__)
#define MACOSX 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char         INT8, *PINT8;
typedef signed short        INT16, *PINT16;
typedef signed int          INT32, *PINT32;
//typedef signed __int64      INT64, *PINT64;
typedef unsigned char       UINT8, *PUINT8;
typedef unsigned short      UINT16, *PUINT16;
typedef unsigned int        UINT32, *PUINT32;
//typedef unsigned __int64    UINT64, *PUINT64;
#ifdef __cplusplus
}
#endif

#define SECTOR_SIZE_MAX     2048

#ifdef _MSC_VER
#pragma pack (1)
#endif
union bootSector {
	struct entries {
		uint8_t jump[3];
		uint8_t oem_name[8];
		uint16_t bytesect;
		uint8_t sectclust;
		uint16_t reserve_sect;
	} bootdata;
	uint8_t rawdata[SECTOR_SIZE_MAX];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif


enum { MCB_FREE=0x0000,MCB_DOS=0x0008 };
enum { RETURN_EXIT=0,RETURN_CTRLC=1,RETURN_ABORT=2,RETURN_TSR=3};

extern Bitu DOS_FILES;

#define DOS_DRIVES 26
#define DOS_DEVICES 20


#if 0 /* ORIGINAL DEFINES FOR REFERENCE */
// dos swappable area is 0x320 bytes beyond the sysvars table
// device driver chain is inside sysvars
#define DOS_INFOBLOCK_SEG 0x80	// sysvars (list of lists)
#define DOS_CONDRV_SEG 0xa0
#define DOS_CONSTRING_SEG 0xa8
#define DOS_SDA_SEG 0xb2		// dos swappable area
#define DOS_SDA_OFS 0
#define DOS_CDS_SEG 0x108
#define DOS_MEM_START 0x158	 // regression to r3437 fixes nascar 2 colors
//#define DOS_MEM_START 0x16f		//First Segment that DOS can use 

#define DOS_PRIVATE_SEGMENT 0xc800
#define DOS_PRIVATE_SEGMENT_END 0xd000
#endif

// dos swappable area is 0x320 bytes beyond the sysvars table
// device driver chain is inside sysvars
extern uint16_t DOS_INFOBLOCK_SEG;// 0x80	// sysvars (list of lists)
extern uint16_t DOS_CONDRV_SEG;// 0xa0
extern uint16_t DOS_CONSTRING_SEG;// 0xa8
extern uint16_t DOS_SDA_SEG;// 0xb2		// dos swappable area
extern uint16_t DOS_SDA_SEG_SIZE;
extern uint16_t DOS_SDA_OFS;// 0
extern uint16_t DOS_CDS_SEG;// 0x108
extern uint16_t DOS_MEM_START;// 0x158	 // regression to r3437 fixes nascar 2 colors

extern uint16_t DOS_PRIVATE_SEGMENT;// 0xc800
extern uint16_t DOS_PRIVATE_SEGMENT_END;// 0xd000

/* internal Dos Tables */

extern DOS_File ** Files;
extern DOS_Drive * Drives[DOS_DRIVES];
extern DOS_Device * Devices[DOS_DEVICES];

extern uint8_t dos_copybuf[0x10000];


void DOS_SetError(uint16_t code);

/* File Handling Routines */

enum { STDIN=0,STDOUT=1,STDERR=2,STDAUX=3,STDPRN=4};
enum { HAND_NONE=0,HAND_FILE,HAND_DEVICE};

/* Routines for File Class */
void DOS_SetupFiles (void);
bool DOS_ReadFile(uint16_t entry,uint8_t * data,uint16_t * amount, bool fcb = false);
bool DOS_WriteFile(uint16_t entry,uint8_t * data,uint16_t * amount,bool fcb = false);
bool DOS_SeekFile(uint16_t entry,uint32_t * pos,uint32_t type,bool fcb = false);
/* ert, 20100711: Locking extensions */
bool DOS_LockFile(uint16_t entry,uint8_t mode,uint32_t pos,uint32_t size);
bool DOS_CloseFile(uint16_t entry,bool fcb = false,uint8_t * refcnt = NULL);
bool DOS_FlushFile(uint16_t entry);
bool DOS_DuplicateEntry(uint16_t entry,uint16_t * newentry);
bool DOS_ForceDuplicateEntry(uint16_t entry,uint16_t newentry);
bool DOS_GetFileDate(uint16_t entry, uint16_t* otime, uint16_t* odate);
bool DOS_SetFileDate(uint16_t entry, uint16_t ntime, uint16_t ndate);

/* Routines for Drive Class */
bool DOS_OpenFile(char const * name,uint8_t flags,uint16_t * entry,bool fcb = false);
bool DOS_OpenFileExtended(char const * name, uint16_t flags, uint16_t createAttr, uint16_t action, uint16_t *entry, uint16_t* status);
bool DOS_CreateFile(char const * name,uint16_t attributes,uint16_t * entry, bool fcb = false);
bool DOS_UnlinkFile(char const * const name);
bool DOS_GetSFNPath(char const * const path, char *SFNpath, bool LFN);
bool DOS_FindFirst(char *search,uint16_t attr,bool fcb_findfirst=false);
bool DOS_FindNext(void);
bool DOS_Canonicalize(char const * const name,char * const big);
bool DOS_CreateTempFile(char * const name,uint16_t * entry);
bool DOS_FileExists(char const * const name);

/* Helper Functions */
bool DOS_MakeName(char const * const name,char * const fullname,uint8_t * drive);
/* Drive Handing Routines */
uint8_t DOS_GetDefaultDrive(void);
void DOS_SetDefaultDrive(uint8_t drive);
bool DOS_SetDrive(uint8_t drive);
bool DOS_GetCurrentDir(uint8_t drive,char * const buffer, bool LFN);
bool DOS_ChangeDir(char const * const dir);
bool DOS_MakeDir(char const * const dir);
bool DOS_RemoveDir(char const * const dir);
bool DOS_Rename(char const * const oldname,char const * const newname);
bool DOS_GetFreeDiskSpace(uint8_t drive,uint16_t * bytes,uint8_t * sectors,uint16_t * clusters,uint16_t * free);
bool DOS_GetFreeDiskSpace32(uint8_t drive,uint32_t * bytes,uint32_t * sectors,uint32_t * clusters,uint32_t * free);
bool DOS_GetFileAttr(char const * const name,uint16_t * attr);
bool DOS_SetFileAttr(char const * const name,uint16_t attr);
bool DOS_GetFileAttrEx(char const* const name, struct stat *status, uint8_t hdrive=-1);
unsigned long DOS_GetCompressedFileSize(char const* const name);
#if defined (WIN32)
HANDLE DOS_CreateOpenFile(char const* const name);
#endif

/* IOCTL Stuff */
bool DOS_IOCTL(void);
bool DOS_GetSTDINStatus();
uint8_t DOS_FindDevice(char const * name);
void DOS_SetupDevices(void);

/* Execute and new process creation */
bool DOS_NewPSP(uint16_t segment,uint16_t size);
bool DOS_ChildPSP(uint16_t segment,uint16_t size);
bool DOS_Execute(const char* name, PhysPt block_pt, uint8_t flags);
void DOS_Terminate(uint16_t pspseg,bool tsr,uint8_t exitcode);

/* Memory Handling Routines */
void DOS_SetupMemory(void);
bool DOS_AllocateMemory(uint16_t * segment,uint16_t * blocks);
bool DOS_ResizeMemory(uint16_t segment,uint16_t * blocks);
bool DOS_FreeMemory(uint16_t segment);
void DOS_FreeProcessMemory(uint16_t pspseg);
uint16_t DOS_GetMemory(uint16_t pages,const char *who=NULL);
bool DOS_SetMemAllocStrategy(uint16_t strat);
uint16_t DOS_GetMemAllocStrategy(void);
void DOS_BuildUMBChain(bool umb_active,bool ems_active);
bool DOS_LinkUMBsToMemChain(uint16_t linkstate);

/* FCB stuff */
bool DOS_FCBOpen(uint16_t seg,uint16_t offset);
bool DOS_FCBCreate(uint16_t seg,uint16_t offset);
bool DOS_FCBClose(uint16_t seg,uint16_t offset);
bool DOS_FCBFindFirst(uint16_t seg,uint16_t offset);
bool DOS_FCBFindNext(uint16_t seg,uint16_t offset);
uint8_t DOS_FCBRead(uint16_t seg,uint16_t offset, uint16_t recno);
uint8_t DOS_FCBWrite(uint16_t seg,uint16_t offset,uint16_t recno);
uint8_t DOS_FCBRandomRead(uint16_t seg,uint16_t offset,uint16_t * numRec,bool restore);
uint8_t DOS_FCBRandomWrite(uint16_t seg,uint16_t offset,uint16_t * numRec,bool restore);
bool DOS_FCBGetFileSize(uint16_t seg,uint16_t offset);
bool DOS_FCBDeleteFile(uint16_t seg,uint16_t offset);
bool DOS_FCBRenameFile(uint16_t seg, uint16_t offset);
void DOS_FCBSetRandomRecord(uint16_t seg, uint16_t offset);
uint8_t FCB_Parsename(uint16_t seg,uint16_t offset,uint8_t parser ,char *string, uint8_t *change);
bool DOS_GetAllocationInfo(uint8_t drive,uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters);

/* Extra DOS Interrupts */
void DOS_SetupMisc(void);

/* The DOS Tables */
void DOS_SetupTables(void);

/* Internal DOS Setup Programs */
void DOS_SetupPrograms(void);

/* Initialize Keyboard Layout */
void DOS_KeyboardLayout_Init(Section* sec);

bool DOS_LayoutKey(Bitu key, uint8_t flags1, uint8_t flags2, uint8_t flags3);

enum {
	KEYB_NOERROR=0,
	KEYB_FILENOTFOUND,
	KEYB_INVALIDFILE,
	KEYB_LAYOUTNOTFOUND,
	KEYB_INVALIDCPFILE
};


static INLINE uint16_t long2para(uint32_t size) {
	if (size>0xFFFF0) return 0xffff;
	if (size&0xf) return (uint16_t)((size>>4)+1);
	else return (uint16_t)(size>>4);
}


static INLINE uint16_t DOS_PackTime(uint16_t hour,uint16_t min,uint16_t sec) {
	return (hour&0x1f)<<11 | (min&0x3f) << 5 | ((sec/2)&0x1f);
}

static INLINE uint16_t DOS_PackDate(uint16_t year,uint16_t mon,uint16_t day) {
	return ((year-1980)&0x7f)<<9 | (mon&0x3f) << 5 | (day&0x1f);
}

/* fopen64, ftello64, fseeko64 */
#if defined(__linux__)
 #define fseek_ofs_t long
#elif defined (_MSC_VER)
 #define fopen64 fopen
 #if (_MSC_VER >= 1400)
  #define ftello64 _ftelli64
  #define fseeko64 _fseeki64
  #define fseek_ofs_t __int64
 #else
  #define ftello64 ftell
  #define fseeko64 fseek
  #define fseek_ofs_t long
 #endif
#elif defined (__MINGW64_VERSION_MAJOR)
 #define fopen64 fopen
 #define ftello64 _ftelli64
 #define fseeko64 _fseeki64
 #define fseek_ofs_t __int64
#else
 #define fopen64 fopen
 #define ftello64 ftell
 #define fseeko64 fseek
 #define fseek_ofs_t off_t
#endif

/* Dos Error Codes */
#define DOSERR_NONE 0
#define DOSERR_FUNCTION_NUMBER_INVALID 1
#define DOSERR_FILE_NOT_FOUND 2
#define DOSERR_PATH_NOT_FOUND 3
#define DOSERR_TOO_MANY_OPEN_FILES 4
#define DOSERR_ACCESS_DENIED 5
#define DOSERR_INVALID_HANDLE 6
#define DOSERR_MCB_DESTROYED 7
#define DOSERR_INSUFFICIENT_MEMORY 8
#define DOSERR_MB_ADDRESS_INVALID 9
#define DOSERR_ENVIRONMENT_INVALID 10
#define DOSERR_FORMAT_INVALID 11
#define DOSERR_ACCESS_CODE_INVALID 12
#define DOSERR_DATA_INVALID 13
#define DOSERR_RESERVED 14
#define DOSERR_FIXUP_OVERFLOW 14
#define DOSERR_INVALID_DRIVE 15
#define DOSERR_REMOVE_CURRENT_DIRECTORY 16
#define DOSERR_NOT_SAME_DEVICE 17
#define DOSERR_NO_MORE_FILES 18
#define DOSERR_WRITE_PROTECTED 19
#define DOSERR_FILE_ALREADY_EXISTS 80


/* Remains some classes used to access certain things */
#define sOffset(s,m) ((char*)&(((s*)NULL)->m)-(char*)NULL)
#define sGet(s,m) GetIt(sizeof(((s *)&pt)->m),(PhysPt)sOffset(s,m))
#define sSave(s,m,val) SaveIt(sizeof(((s *)&pt)->m),(PhysPt)sOffset(s,m),val)

class MemStruct {
public:
    inline uint32_t GetIt(const uint32_t size, const PhysPt addr) {
		switch (size) {
		case 1:return mem_readb(pt+addr);
		case 2:return mem_readw(pt+addr);
		case 4:return mem_readd(pt+addr);
		}
		return 0;
	}
	inline void SaveIt(const uint32_t size, const PhysPt addr, const uint32_t val) {
		switch (size) {
		case 1:mem_writeb(pt+addr,(uint8_t)val);break;
		case 2:mem_writew(pt+addr,(uint16_t)val);break;
		case 4:mem_writed(pt+addr,(uint32_t)val);break;
		}
	}
    inline void SetPt(const uint16_t seg) { pt=PhysMake(seg,0);}
    inline void SetPt(const uint16_t seg, const uint16_t off) { pt=PhysMake(seg,off);}
    inline void SetPt(const RealPt addr) { pt=Real2Phys(addr);}
    inline PhysPt GetPtPhys(void) const { return pt; }
    inline void SetPtPhys(const PhysPt _pt) { pt=_pt; }
protected:
	PhysPt pt;
};

class DOS_PSP :public MemStruct {
public:
	DOS_PSP						(uint16_t segment)		{ SetPt(segment);seg=segment;};
	void	MakeNew				(uint16_t mem_size);
	void	CopyFileTable		(DOS_PSP* srcpsp,bool createchildpsp);
	uint16_t	FindFreeFileEntry	(void);
	void	CloseFiles			(void);

	void	SaveVectors			(void);
	void	RestoreVectors		(void);
	void	SetSize				(uint16_t size)			{ sSave(sPSP,next_seg,size);		};
	uint16_t	GetSize				(void)					{ return (uint16_t)sGet(sPSP,next_seg);		};
	void	SetEnvironment		(uint16_t envseg)			{ sSave(sPSP,environment,envseg);	};
	uint16_t	GetEnvironment		(void)					{ return (uint16_t)sGet(sPSP,environment);	};
	uint16_t	GetSegment			(void)					{ return seg;						};
	void	SetFileHandle		(uint16_t index, uint8_t handle);
	uint8_t	GetFileHandle		(uint16_t index);
	void	SetParent			(uint16_t parent)			{ sSave(sPSP,psp_parent,parent);	};
	uint16_t	GetParent			(void)					{ return (uint16_t)sGet(sPSP,psp_parent);		};
	void	SetStack			(RealPt stackpt)		{ sSave(sPSP,stack,stackpt);		};
	RealPt	GetStack			(void)					{ return sGet(sPSP,stack);			};
	void	SetInt22			(RealPt int22pt)		{ sSave(sPSP,int_22,int22pt);		};
	RealPt	GetInt22			(void)					{ return sGet(sPSP,int_22);			};
	void	SetFCB1				(RealPt src);
	void	SetFCB2				(RealPt src);
	void	SetCommandTail		(RealPt src);
	void    StoreCommandTail    (void);
	void    RestoreCommandTail  (void);
	bool	SetNumFiles			(uint16_t fileNum);
	uint16_t	FindEntryByHandle	(uint8_t handle);
			
private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sPSP {
		uint8_t	exit[2];			/* CP/M-like exit poimt */
		uint16_t	next_seg;			/* Segment of first byte beyond memory allocated or program */
		uint8_t	fill_1;				/* single char fill */
		uint8_t	far_call;			/* far call opcode */
		RealPt	cpm_entry;			/* CPM Service Request address*/
		RealPt	int_22;				/* Terminate Address */
		RealPt	int_23;				/* Break Address */
		RealPt	int_24;				/* Critical Error Address */
		uint16_t	psp_parent;			/* Parent PSP Segment */
		uint8_t	files[20];			/* File Table - 0xff is unused */
		uint16_t	environment;		/* Segment of evironment table */
		RealPt	stack;				/* SS:SP Save point for int 0x21 calls */
		uint16_t	max_files;			/* Maximum open files */
		RealPt	file_table;			/* Pointer to File Table PSP:0x18 */
		RealPt	prev_psp;			/* Pointer to previous PSP */
		uint8_t interim_flag;
		uint8_t truename_flag;
		uint16_t nn_flags;
		uint16_t dos_version;
		uint8_t	fill_2[14];			/* Lot's of unused stuff i can't care aboue */
		uint8_t	service[3];			/* INT 0x21 Service call int 0x21;retf; */
		uint8_t	fill_3[9];			/* This has some blocks with FCB info */
		uint8_t	fcb1[16];			/* first FCB */
		uint8_t	fcb2[16];			/* second FCB */
		uint8_t	fill_4[4];			/* unused */
		CommandTail cmdtail;		
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
	uint16_t	seg;
public:
	static	uint16_t rootpsp;
};

class DOS_ParamBlock:public MemStruct {
public:
	DOS_ParamBlock(PhysPt addr) {pt=addr;}
	void Clear(void);
	void LoadData(void);
	void SaveData(void);		/* Save it as an exec block */
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sOverlay {
		uint16_t loadseg;
		uint16_t relocation;
	} GCC_ATTRIBUTE(packed);
	struct sExec {
		uint16_t envseg;
		RealPt cmdtail;
		RealPt fcb1;
		RealPt fcb2;
		RealPt initsssp;
		RealPt initcsip;
	}GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
    sExec exec = {};
    sOverlay overlay = {};
};

class DOS_InfoBlock:public MemStruct {
public:
    DOS_InfoBlock() : seg(0) {};
	void SetLocation(uint16_t  segment);
    void SetFirstDPB(uint32_t _first_dpb);
	void SetFirstMCB(uint16_t _firstmcb);
	void SetBuffers(uint16_t x,uint16_t y);
	void SetCurDirStruct(uint32_t _curdirstruct);
	void SetFCBTable(uint32_t _fcbtable);
	void SetDeviceChainStart(uint32_t _devchain);
	void SetDiskBufferHeadPt(uint32_t _dbheadpt);
	void SetStartOfUMBChain(uint16_t _umbstartseg);
	void SetUMBChainState(uint8_t _umbchaining);
	void SetBlockDevices(uint8_t _count);
	uint16_t	GetStartOfUMBChain(void);
	uint8_t	GetUMBChainState(void);
	RealPt	GetPointer(void);
	uint32_t GetDeviceChain(void);

	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDIB {		
		uint8_t	unknown1[4];
		uint16_t	magicWord;			// -0x22 needs to be 1
		uint8_t	unknown2[8];
		uint16_t	regCXfrom5e;		// -0x18 CX from last int21/ah=5e
		uint16_t	countLRUcache;		// -0x16 LRU counter for FCB caching
		uint16_t	countLRUopens;		// -0x14 LRU counter for FCB openings
		uint8_t	stuff[6];		// -0x12 some stuff, hopefully never used....
		uint16_t	sharingCount;		// -0x0c sharing retry count
		uint16_t	sharingDelay;		// -0x0a sharing retry delay
		RealPt	diskBufPtr;		// -0x08 pointer to disk buffer
		uint16_t	ptrCONinput;		// -0x04 pointer to con input
		uint16_t	firstMCB;		// -0x02 first memory control block
		RealPt	firstDPB;		//  0x00 first drive parameter block
		RealPt	firstFileTable;		//  0x04 first system file table
		RealPt	activeClock;		//  0x08 active clock device header
		RealPt	activeCon;		//  0x0c active console device header
		uint16_t	maxSectorLength;	//  0x10 maximum bytes per sector of any block device;
		RealPt	diskInfoBuffer;		//  0x12 pointer to disk info buffer
		RealPt  curDirStructure;	//  0x16 pointer to current array of directory structure
		RealPt	fcbTable;		//  0x1a pointer to system FCB table
		uint16_t	protFCBs;		//  0x1e protected fcbs
		uint8_t	blockDevices;		//  0x20 installed block devices
		uint8_t	lastdrive;		//  0x21 lastdrive
		uint32_t	nulNextDriver;	//  0x22 NUL driver next pointer
		uint16_t	nulAttributes;	//  0x26 NUL driver aattributes
        uint16_t  nulStrategy;    //  0x28 NUL driver strategy routine
        uint16_t  nulInterrupt;   //  0x2A NUL driver interrupt routine
		uint8_t	nulString[8];	//  0x2c NUL driver name string
		uint8_t	joindedDrives;		//  0x34 joined drives
		uint16_t	specialCodeSeg;		//  0x35 special code segment
		RealPt  setverPtr;		//  0x37 pointer to setver
		uint16_t  a20FixOfs;		//  0x3b a20 fix routine offset
		uint16_t  pspLastIfHMA;		//  0x3d psp of last program (if dos in hma)
		uint16_t	buffers_x;		//  0x3f x in BUFFERS x,y
		uint16_t	buffers_y;		//  0x41 y in BUFFERS x,y
		uint8_t	bootDrive;		//  0x43 boot drive
		uint8_t	useDwordMov;		//  0x44 use dword moves
		uint16_t	extendedSize;		//  0x45 size of extended memory
		uint32_t	diskBufferHeadPt;	//  0x47 pointer to least-recently used buffer header
		uint16_t	dirtyDiskBuffers;	//  0x4b number of dirty disk buffers
		uint32_t	lookaheadBufPt;		//  0x4d pointer to lookahead buffer
		uint16_t	lookaheadBufNumber;		//  0x51 number of lookahead buffers
		uint8_t	bufferLocation;			//  0x53 workspace buffer location
		uint32_t	workspaceBuffer;		//  0x54 pointer to workspace buffer
		uint8_t	unknown3[11];			//  0x58
		uint8_t	chainingUMB;			//  0x63 bit0: UMB chain linked to MCB chain
		uint16_t	minMemForExec;			//  0x64 minimum paragraphs needed for current program
		uint16_t	startOfUMBChain;		//  0x66 segment of first UMB-MCB
		uint16_t	memAllocScanStart;		//  0x68 start paragraph for memory allocation
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
	uint16_t	seg;
};

class DOS_DTA:public MemStruct{
public:
	DOS_DTA(RealPt addr) { SetPt(addr); }

    int GetFindData(int fmt,char * finddata,int *c);
	
	void SetupSearch(uint8_t _sdrive,uint8_t _sattr,char * pattern);
	void SetResult(const char * _name,const char * _lname,uint32_t _size,uint16_t _date,uint16_t _time,uint8_t _attr);
	
	uint8_t GetSearchDrive(void);
	void GetSearchParams(uint8_t & _sattr,char * _spattern,bool lfn);
    void GetResult(char * _name,char * _lname,uint32_t & _size,uint16_t & _date,uint16_t & _time,uint8_t & _attr);

	void	SetDirID(uint16_t entry)			{ sSave(sDTA,dirID,entry); };
	void	SetDirIDCluster(uint32_t entry)	{ sSave(sDTA,dirCluster,entry); };
	uint16_t	GetDirID(void)				{ return (uint16_t)sGet(sDTA,dirID); };
	uint32_t	GetDirIDCluster(void)		{ return (uint32_t)sGet(sDTA,dirCluster); };
    uint8_t   GetAttr(void)               { return (uint8_t)sGet(sDTA,sattr); }
private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDTA {
		uint8_t sdrive;						/* The Drive the search is taking place */
        uint8_t spname[8];                    /* The Search pattern for the filename */              
        uint8_t spext[3];                     /* The Search pattern for the extension */
		uint8_t sattr;						/* The Attributes that need to be found */
		uint16_t dirID;						/* custom: dir-search ID for multiple searches at the same time */
		uint32_t dirCluster;					/* custom (drive_fat only): cluster number for multiple searches at the same time. 32-bit wide on FAT32 aware MS-DOS 7.1 or higher. */
		uint8_t fill[2];
		uint8_t attr;
		uint16_t time;
		uint16_t date;
		uint32_t size;
		char name[DOS_NAMELENGTH_ASCII];
	} GCC_ATTRIBUTE(packed);
	static_assert(offsetof(sDTA,dirID) == 0x0D,"oops");
	static_assert(offsetof(sDTA,dirCluster) == 0x0F,"oops");
	static_assert(offsetof(sDTA,fill) == 0x13,"oops");
	static_assert(offsetof(sDTA,attr) == 0x15,"oops");
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

class DOS_FCB: public MemStruct {
public:
	DOS_FCB(uint16_t seg,uint16_t off,bool allow_extended=true);
	void Create(bool _extended);
    void SetName(uint8_t _drive, const char* _fname, const char* _ext);
	void SetSizeDateTime(uint32_t _size,uint16_t _date,uint16_t _time);
	void GetSizeDateTime(uint32_t & _size,uint16_t & _date,uint16_t & _time);
    void GetVolumeName(char * fillname);
	void GetName(char * fillname);
	void FileOpen(uint8_t _fhandle);
	void FileClose(uint8_t & _fhandle);
	void GetRecord(uint16_t & _cur_block,uint8_t & _cur_rec);
	void SetRecord(uint16_t _cur_block,uint8_t _cur_rec);
	void GetSeqData(uint8_t & _fhandle,uint16_t & _rec_size);
	void SetSeqData(uint8_t _fhandle,uint16_t _rec_size);
	void GetRandom(uint32_t & _random);
	void SetRandom(uint32_t  _random);
	uint8_t GetDrive(void);
	bool Extended(void);
	void GetAttr(uint8_t & attr);
	void SetAttr(uint8_t attr);
	void SetResult(uint32_t size,uint16_t date,uint16_t time,uint8_t attr);
	bool Valid(void);
	void ClearBlockRecsize(void);
private:
	bool extended = false;
	PhysPt real_pt;
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sFCB {
		uint8_t drive;			/* Drive number 0=default, 1=A, etc */
		uint8_t filename[8];		/* Space padded name */
		uint8_t ext[3];			/* Space padded extension */
		uint16_t cur_block;		/* Current Block */
		uint16_t rec_size;		/* Logical record size */
		uint32_t filesize;		/* File Size */
		uint16_t date;
		uint16_t time;
		/* Reserved Block should be 8 bytes */
		uint8_t sft_entries;
		uint8_t share_attributes;
		uint8_t extra_info;
		/* Maybe swap file_handle and sft_entries now that fcbs 
		 * aren't stored in the psp filetable anymore */
		uint8_t file_handle;
		uint8_t reserved[4];
		/* end */
		uint8_t  cur_rec;			/* Current record in current block */
		uint32_t rndm;			/* Current relative record number */
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

class DOS_MCB : public MemStruct{
public:
	DOS_MCB(uint16_t seg) { SetPt(seg); }
	void SetFileName(const char * const _name) { MEM_BlockWrite(pt+offsetof(sMCB,filename),_name,8); }
	void GetFileName(char * const _name) { MEM_BlockRead(pt+offsetof(sMCB,filename),_name,8);_name[8]=0;}
	void SetType(uint8_t _type) { sSave(sMCB,type,_type);}
	void SetSize(uint16_t _size) { sSave(sMCB,size,_size);}
	void SetPSPSeg(uint16_t _pspseg) { sSave(sMCB,psp_segment,_pspseg);}
	uint8_t GetType(void) { return (uint8_t)sGet(sMCB,type);}
	uint16_t GetSize(void) { return (uint16_t)sGet(sMCB,size);}
	uint16_t GetPSPSeg(void) { return (uint16_t)sGet(sMCB,psp_segment);}
private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sMCB {
		uint8_t type;
		uint16_t psp_segment;
		uint16_t size;	
		uint8_t unused[3];
		uint8_t filename[8];
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

class DOS_SDA : public MemStruct {
public:
	DOS_SDA(uint16_t _seg,uint16_t _offs) { SetPt(_seg,_offs); }
	void Init();   
	void SetDrive(uint8_t _drive) { sSave(sSDA,current_drive, _drive); }
	void SetDTA(uint32_t _dta) { sSave(sSDA,current_dta, _dta); }
	void SetPSP(uint16_t _psp) { sSave(sSDA,current_psp, _psp); }
	uint8_t GetDrive(void) { return (uint8_t)sGet(sSDA,current_drive); }
	uint16_t GetPSP(void) { return (uint16_t)sGet(sSDA,current_psp); }
	uint32_t GetDTA(void) { return (uint32_t)sGet(sSDA,current_dta); }
	
	
private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sSDA {
		uint8_t crit_error_flag;		/* 0x00 Critical Error Flag */
		uint8_t inDOS_flag;		/* 0x01 InDOS flag (count of active INT 21 calls) */
		uint8_t drive_crit_error;		/* 0x02 Drive on which current critical error occurred or FFh */
		uint8_t locus_of_last_error;	/* 0x03 locus of last error */
		uint16_t extended_error_code;	/* 0x04 extended error code of last error */
		uint8_t suggested_action;		/* 0x06 suggested action for last error */
		uint8_t error_class;		/* 0x07 class of last error*/
		uint32_t last_error_pointer; 	/* 0x08 ES:DI pointer for last error */
		uint32_t current_dta;		/* 0x0C current DTA (Disk Transfer Address) */
		uint16_t current_psp; 		/* 0x10 current PSP */
		uint16_t sp_int_23;		/* 0x12 stores SP across an INT 23 */
		uint16_t return_code;		/* 0x14 return code from last process termination (zerod after reading with AH=4Dh) */
		uint8_t current_drive;		/* 0x16 current drive */
		uint8_t extended_break_flag; 	/* 0x17 extended break flag */
		uint8_t fill[2];			/* 0x18 flag: code page switching || flag: copy of previous byte in case of INT 24 Abort*/
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};
extern DOS_InfoBlock dos_infoblock;

struct DOS_Block {
    DOS_Date date = {};
    DOS_Version version = {};
    uint16_t firstMCB = 0;
    uint16_t errorcode = 0;
    uint16_t psp();//{return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetPSP();};
    void psp(uint16_t _seg);//{ DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetPSP(_seg);};
    RealPt dta();//{return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDTA();};
    void dta(RealPt _dta);//{DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDTA(_dta);};
    uint8_t return_code = 0, return_mode = 0;

    uint8_t current_drive = 0;
    bool verify = false;
    bool breakcheck = false;
    bool echo = false;          // if set to true dev_con::read will echo input
    bool direct_output = false;
    bool internal_output = false;
    struct {
        RealPt mediaid = 0;
        RealPt tempdta = 0;
        RealPt tempdta_fcbdelete = 0;
        RealPt dbcs = 0;
        RealPt filenamechar = 0;
        RealPt collatingseq = 0;
        RealPt upcase = 0;
        uint8_t* country = NULL;//Will be copied to dos memory. resides in real mem
        uint16_t dpb = 0; //Fake Disk parameter system using only the first entry so the drive letter matches
        uint16_t dpb_size = 0x21; // bytes per DPB entry (MS-DOS 4.x-6.x size)
        uint16_t mediaid_offset = 0x17; // media ID offset in DPB (MS-DOS 4.x-6.x case)
    } tables;
    uint16_t loaded_codepage = 0;
};

extern DOS_Block dos;

static INLINE uint8_t RealHandle(uint16_t handle) {
	DOS_PSP psp(dos.psp());	
	return psp.GetFileHandle(handle);
}

struct DOS_GetMemLog_Entry {
    uint16_t      segbase = 0;
    uint16_t      pages = 0;
    std::string who;
};

extern std::list<DOS_GetMemLog_Entry> DOS_GetMemLog;

#endif
