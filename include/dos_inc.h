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
  Bit8u count;				/* number of bytes returned */
  char buffer[CTBUF];		/* the buffer itself */
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

extern Bit16u first_umb_seg;
extern Bit16u first_umb_size;

bool MEM_unmap_physmem(Bitu start,Bitu end);
bool MEM_map_RAM_physmem(Bitu start,Bitu end);

struct BuiltinFileBlob {
	const char		*recommended_file_name;
	const unsigned char	*data;
	size_t			length;
};

struct DOS_Date {
	Bit16u year;
	Bit8u month;
	Bit8u day;
};

struct DOS_Version {
	Bit8u major,minor,revision;
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
		Bit8u jump[3];
		Bit8u oem_name[8];
		Bit16u bytesect;
		Bit8u sectclust;
		Bit16u reserve_sect;
	} bootdata;
	Bit8u rawdata[SECTOR_SIZE_MAX];
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
extern Bit16u DOS_INFOBLOCK_SEG;// 0x80	// sysvars (list of lists)
extern Bit16u DOS_CONDRV_SEG;// 0xa0
extern Bit16u DOS_CONSTRING_SEG;// 0xa8
extern Bit16u DOS_SDA_SEG;// 0xb2		// dos swappable area
extern Bit16u DOS_SDA_SEG_SIZE;
extern Bit16u DOS_SDA_OFS;// 0
extern Bit16u DOS_CDS_SEG;// 0x108
extern Bit16u DOS_MEM_START;// 0x158	 // regression to r3437 fixes nascar 2 colors

extern Bit16u DOS_PRIVATE_SEGMENT;// 0xc800
extern Bit16u DOS_PRIVATE_SEGMENT_END;// 0xd000

/* internal Dos Tables */

extern DOS_File ** Files;
extern DOS_Drive * Drives[DOS_DRIVES];
extern DOS_Device * Devices[DOS_DEVICES];

extern Bit8u dos_copybuf[0x10000];


void DOS_SetError(Bit16u code);

/* File Handling Routines */

enum { STDIN=0,STDOUT=1,STDERR=2,STDAUX=3,STDPRN=4};
enum { HAND_NONE=0,HAND_FILE,HAND_DEVICE};

/* Routines for File Class */
void DOS_SetupFiles (void);
bool DOS_ReadFile(Bit16u entry,Bit8u * data,Bit16u * amount, bool fcb = false);
bool DOS_WriteFile(Bit16u entry,Bit8u * data,Bit16u * amount,bool fcb = false);
bool DOS_SeekFile(Bit16u entry,Bit32u * pos,Bit32u type,bool fcb = false);
/* ert, 20100711: Locking extensions */
bool DOS_LockFile(Bit16u entry,Bit8u mode,Bit32u pos,Bit32u size);
bool DOS_CloseFile(Bit16u entry,bool fcb = false);
bool DOS_FlushFile(Bit16u entry);
bool DOS_DuplicateEntry(Bit16u entry,Bit16u * newentry);
bool DOS_ForceDuplicateEntry(Bit16u entry,Bit16u newentry);
bool DOS_GetFileDate(Bit16u entry, Bit16u* otime, Bit16u* odate);
bool DOS_SetFileDate(Bit16u entry, Bit16u ntime, Bit16u ndate);

/* Routines for Drive Class */
bool DOS_OpenFile(char const * name,Bit8u flags,Bit16u * entry,bool fcb = false);
bool DOS_OpenFileExtended(char const * name, Bit16u flags, Bit16u createAttr, Bit16u action, Bit16u *entry, Bit16u* status);
bool DOS_CreateFile(char const * name,Bit16u attributes,Bit16u * entry, bool fcb = false);
bool DOS_UnlinkFile(char const * const name);
bool DOS_GetSFNPath(char const * const path, char *SFNpath, bool LFN);
bool DOS_FindFirst(char *search,Bit16u attr,bool fcb_findfirst=false);
bool DOS_FindNext(void);
bool DOS_Canonicalize(char const * const name,char * const big);
bool DOS_CreateTempFile(char * const name,Bit16u * entry);
bool DOS_FileExists(char const * const name);

/* Helper Functions */
bool DOS_MakeName(char const * const name,char * const fullname,Bit8u * drive);
/* Drive Handing Routines */
Bit8u DOS_GetDefaultDrive(void);
void DOS_SetDefaultDrive(Bit8u drive);
bool DOS_SetDrive(Bit8u drive);
bool DOS_GetCurrentDir(Bit8u drive,char * const buffer, bool LFN);
bool DOS_ChangeDir(char const * const dir);
bool DOS_MakeDir(char const * const dir);
bool DOS_RemoveDir(char const * const dir);
bool DOS_Rename(char const * const oldname,char const * const newname);
bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free);
bool DOS_GetFileAttr(char const * const name,Bit16u * attr);
bool DOS_SetFileAttr(char const * const name,Bit16u attr);
bool DOS_GetFileAttrEx(char const* const name, struct stat *status, Bit8u hdrive=-1);
unsigned long DOS_GetCompressedFileSize(char const* const name);
#if defined (WIN32)
HANDLE DOS_CreateOpenFile(char const* const name);
#endif

/* IOCTL Stuff */
bool DOS_IOCTL(void);
bool DOS_GetSTDINStatus();
Bit8u DOS_FindDevice(char const * name);
void DOS_SetupDevices(void);

/* Execute and new process creation */
bool DOS_NewPSP(Bit16u segment,Bit16u size);
bool DOS_ChildPSP(Bit16u segment,Bit16u size);
bool DOS_Execute(char * name,PhysPt block_pt,Bit8u flags);
void DOS_Terminate(Bit16u pspseg,bool tsr,Bit8u exitcode);

/* Memory Handling Routines */
void DOS_SetupMemory(void);
bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks);
bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks);
bool DOS_FreeMemory(Bit16u segment);
void DOS_FreeProcessMemory(Bit16u pspseg);
Bit16u DOS_GetMemory(Bit16u pages,const char *who=NULL);
bool DOS_SetMemAllocStrategy(Bit16u strat);
Bit16u DOS_GetMemAllocStrategy(void);
void DOS_BuildUMBChain(bool umb_active,bool ems_active);
bool DOS_LinkUMBsToMemChain(Bit16u linkstate);

/* FCB stuff */
bool DOS_FCBOpen(Bit16u seg,Bit16u offset);
bool DOS_FCBCreate(Bit16u seg,Bit16u offset);
bool DOS_FCBClose(Bit16u seg,Bit16u offset);
bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset);
bool DOS_FCBFindNext(Bit16u seg,Bit16u offset);
Bit8u DOS_FCBRead(Bit16u seg,Bit16u offset, Bit16u recno);
Bit8u DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u recno);
Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u * numRec,bool restore);
Bit8u DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u * numRec,bool restore);
bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset);
bool DOS_FCBDeleteFile(Bit16u seg,Bit16u offset);
bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset);
void DOS_FCBSetRandomRecord(Bit16u seg, Bit16u offset);
Bit8u FCB_Parsename(Bit16u seg,Bit16u offset,Bit8u parser ,char *string, Bit8u *change);
bool DOS_GetAllocationInfo(Bit8u drive,Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters);

/* Extra DOS Interrupts */
void DOS_SetupMisc(void);

/* The DOS Tables */
void DOS_SetupTables(void);

/* Internal DOS Setup Programs */
void DOS_SetupPrograms(void);

/* Initialize Keyboard Layout */
void DOS_KeyboardLayout_Init(Section* sec);

bool DOS_LayoutKey(Bitu key, Bit8u flags1, Bit8u flags2, Bit8u flags3);

enum {
	KEYB_NOERROR=0,
	KEYB_FILENOTFOUND,
	KEYB_INVALIDFILE,
	KEYB_LAYOUTNOTFOUND,
	KEYB_INVALIDCPFILE
};


static INLINE Bit16u long2para(Bit32u size) {
	if (size>0xFFFF0) return 0xffff;
	if (size&0xf) return (Bit16u)((size>>4)+1);
	else return (Bit16u)(size>>4);
}


static INLINE Bit16u DOS_PackTime(Bit16u hour,Bit16u min,Bit16u sec) {
	return (hour&0x1f)<<11 | (min&0x3f) << 5 | ((sec/2)&0x1f);
}

static INLINE Bit16u DOS_PackDate(Bit16u year,Bit16u mon,Bit16u day) {
	return ((year-1980)&0x7f)<<9 | (mon&0x3f) << 5 | (day&0x1f);
}

/* fopen64, ftello64, fseeko64 */
#if defined(__APPLE__)
 #define fopen64 fopen
 #define ftello64 ftell
 #define fseeko64 fseek
 #define fseek_ofs_t off_t
#elif defined (__HAIKU__)
 #define fopen64 fopen
 #define ftello64 ftello
 #define fseeko64 fseeko
 #define fseek_ofs_t off_t
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
#else
 #define fseek_ofs_t long
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
    inline Bit32u GetIt(const Bit32u size, const PhysPt addr) {
		switch (size) {
		case 1:return mem_readb(pt+addr);
		case 2:return mem_readw(pt+addr);
		case 4:return mem_readd(pt+addr);
		}
		return 0;
	}
	inline void SaveIt(const Bit32u size, const PhysPt addr, const Bit32u val) {
		switch (size) {
		case 1:mem_writeb(pt+addr,(Bit8u)val);break;
		case 2:mem_writew(pt+addr,(Bit16u)val);break;
		case 4:mem_writed(pt+addr,(Bit32u)val);break;
		}
	}
    inline void SetPt(const Bit16u seg) { pt=PhysMake(seg,0);}
    inline void SetPt(const Bit16u seg, const Bit16u off) { pt=PhysMake(seg,off);}
    inline void SetPt(const RealPt addr) { pt=Real2Phys(addr);}
    inline PhysPt GetPtPhys(void) const { return pt; }
    inline void SetPtPhys(const PhysPt _pt) { pt=_pt; }
protected:
	PhysPt pt;
};

class DOS_PSP :public MemStruct {
public:
	DOS_PSP						(Bit16u segment)		{ SetPt(segment);seg=segment;};
	void	MakeNew				(Bit16u mem_size);
	void	CopyFileTable		(DOS_PSP* srcpsp,bool createchildpsp);
	Bit16u	FindFreeFileEntry	(void);
	void	CloseFiles			(void);

	void	SaveVectors			(void);
	void	RestoreVectors		(void);
	void	SetSize				(Bit16u size)			{ sSave(sPSP,next_seg,size);		};
	Bit16u	GetSize				(void)					{ return (Bit16u)sGet(sPSP,next_seg);		};
	void	SetEnvironment		(Bit16u envseg)			{ sSave(sPSP,environment,envseg);	};
	Bit16u	GetEnvironment		(void)					{ return (Bit16u)sGet(sPSP,environment);	};
	Bit16u	GetSegment			(void)					{ return seg;						};
	void	SetFileHandle		(Bit16u index, Bit8u handle);
	Bit8u	GetFileHandle		(Bit16u index);
	void	SetParent			(Bit16u parent)			{ sSave(sPSP,psp_parent,parent);	};
	Bit16u	GetParent			(void)					{ return (Bit16u)sGet(sPSP,psp_parent);		};
	void	SetStack			(RealPt stackpt)		{ sSave(sPSP,stack,stackpt);		};
	RealPt	GetStack			(void)					{ return sGet(sPSP,stack);			};
	void	SetInt22			(RealPt int22pt)		{ sSave(sPSP,int_22,int22pt);		};
	RealPt	GetInt22			(void)					{ return sGet(sPSP,int_22);			};
	void	SetFCB1				(RealPt src);
	void	SetFCB2				(RealPt src);
	void	SetCommandTail		(RealPt src);
	void    StoreCommandTail    (void);
	void    RestoreCommandTail  (void);
	bool	SetNumFiles			(Bit16u fileNum);
	Bit16u	FindEntryByHandle	(Bit8u handle);
			
private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sPSP {
		Bit8u	exit[2];			/* CP/M-like exit poimt */
		Bit16u	next_seg;			/* Segment of first byte beyond memory allocated or program */
		Bit8u	fill_1;				/* single char fill */
		Bit8u	far_call;			/* far call opcode */
		RealPt	cpm_entry;			/* CPM Service Request address*/
		RealPt	int_22;				/* Terminate Address */
		RealPt	int_23;				/* Break Address */
		RealPt	int_24;				/* Critical Error Address */
		Bit16u	psp_parent;			/* Parent PSP Segment */
		Bit8u	files[20];			/* File Table - 0xff is unused */
		Bit16u	environment;		/* Segment of evironment table */
		RealPt	stack;				/* SS:SP Save point for int 0x21 calls */
		Bit16u	max_files;			/* Maximum open files */
		RealPt	file_table;			/* Pointer to File Table PSP:0x18 */
		RealPt	prev_psp;			/* Pointer to previous PSP */
		Bit8u interim_flag;
		Bit8u truename_flag;
		Bit16u nn_flags;
		Bit16u dos_version;
		Bit8u	fill_2[14];			/* Lot's of unused stuff i can't care aboue */
		Bit8u	service[3];			/* INT 0x21 Service call int 0x21;retf; */
		Bit8u	fill_3[9];			/* This has some blocks with FCB info */
		Bit8u	fcb1[16];			/* first FCB */
		Bit8u	fcb2[16];			/* second FCB */
		Bit8u	fill_4[4];			/* unused */
		CommandTail cmdtail;		
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
	Bit16u	seg;
public:
	static	Bit16u rootpsp;
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
		Bit16u loadseg;
		Bit16u relocation;
	} GCC_ATTRIBUTE(packed);
	struct sExec {
		Bit16u envseg;
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
	void SetLocation(Bit16u  segment);
    void SetFirstDPB(Bit32u _first_dpb);
	void SetFirstMCB(Bit16u _firstmcb);
	void SetBuffers(Bit16u x,Bit16u y);
	void SetCurDirStruct(Bit32u _curdirstruct);
	void SetFCBTable(Bit32u _fcbtable);
	void SetDeviceChainStart(Bit32u _devchain);
	void SetDiskBufferHeadPt(Bit32u _dbheadpt);
	void SetStartOfUMBChain(Bit16u _umbstartseg);
	void SetUMBChainState(Bit8u _umbchaining);
	void SetBlockDevices(Bit8u _count);
	Bit16u	GetStartOfUMBChain(void);
	Bit8u	GetUMBChainState(void);
	RealPt	GetPointer(void);
	Bit32u GetDeviceChain(void);

	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDIB {		
		Bit8u	unknown1[4];
		Bit16u	magicWord;			// -0x22 needs to be 1
		Bit8u	unknown2[8];
		Bit16u	regCXfrom5e;		// -0x18 CX from last int21/ah=5e
		Bit16u	countLRUcache;		// -0x16 LRU counter for FCB caching
		Bit16u	countLRUopens;		// -0x14 LRU counter for FCB openings
		Bit8u	stuff[6];		// -0x12 some stuff, hopefully never used....
		Bit16u	sharingCount;		// -0x0c sharing retry count
		Bit16u	sharingDelay;		// -0x0a sharing retry delay
		RealPt	diskBufPtr;		// -0x08 pointer to disk buffer
		Bit16u	ptrCONinput;		// -0x04 pointer to con input
		Bit16u	firstMCB;		// -0x02 first memory control block
		RealPt	firstDPB;		//  0x00 first drive parameter block
		RealPt	firstFileTable;		//  0x04 first system file table
		RealPt	activeClock;		//  0x08 active clock device header
		RealPt	activeCon;		//  0x0c active console device header
		Bit16u	maxSectorLength;	//  0x10 maximum bytes per sector of any block device;
		RealPt	diskInfoBuffer;		//  0x12 pointer to disk info buffer
		RealPt  curDirStructure;	//  0x16 pointer to current array of directory structure
		RealPt	fcbTable;		//  0x1a pointer to system FCB table
		Bit16u	protFCBs;		//  0x1e protected fcbs
		Bit8u	blockDevices;		//  0x20 installed block devices
		Bit8u	lastdrive;		//  0x21 lastdrive
		Bit32u	nulNextDriver;	//  0x22 NUL driver next pointer
		Bit16u	nulAttributes;	//  0x26 NUL driver aattributes
        Bit16u  nulStrategy;    //  0x28 NUL driver strategy routine
        Bit16u  nulInterrupt;   //  0x2A NUL driver interrupt routine
		Bit8u	nulString[8];	//  0x2c NUL driver name string
		Bit8u	joindedDrives;		//  0x34 joined drives
		Bit16u	specialCodeSeg;		//  0x35 special code segment
		RealPt  setverPtr;		//  0x37 pointer to setver
		Bit16u  a20FixOfs;		//  0x3b a20 fix routine offset
		Bit16u  pspLastIfHMA;		//  0x3d psp of last program (if dos in hma)
		Bit16u	buffers_x;		//  0x3f x in BUFFERS x,y
		Bit16u	buffers_y;		//  0x41 y in BUFFERS x,y
		Bit8u	bootDrive;		//  0x43 boot drive
		Bit8u	useDwordMov;		//  0x44 use dword moves
		Bit16u	extendedSize;		//  0x45 size of extended memory
		Bit32u	diskBufferHeadPt;	//  0x47 pointer to least-recently used buffer header
		Bit16u	dirtyDiskBuffers;	//  0x4b number of dirty disk buffers
		Bit32u	lookaheadBufPt;		//  0x4d pointer to lookahead buffer
		Bit16u	lookaheadBufNumber;		//  0x51 number of lookahead buffers
		Bit8u	bufferLocation;			//  0x53 workspace buffer location
		Bit32u	workspaceBuffer;		//  0x54 pointer to workspace buffer
		Bit8u	unknown3[11];			//  0x58
		Bit8u	chainingUMB;			//  0x63 bit0: UMB chain linked to MCB chain
		Bit16u	minMemForExec;			//  0x64 minimum paragraphs needed for current program
		Bit16u	startOfUMBChain;		//  0x66 segment of first UMB-MCB
		Bit16u	memAllocScanStart;		//  0x68 start paragraph for memory allocation
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
	Bit16u	seg;
};

class DOS_DTA:public MemStruct{
public:
	DOS_DTA(RealPt addr) { SetPt(addr); }

    int GetFindData(int fmt,char * finddata);
	
	void SetupSearch(Bit8u _sdrive,Bit8u _sattr,char * pattern);
	void SetResult(const char * _name,const char * _lname,Bit32u _size,Bit16u _date,Bit16u _time,Bit8u _attr);
	
	Bit8u GetSearchDrive(void);
	void GetSearchParams(Bit8u & _sattr,char * _spattern,bool lfn);
    void GetResult(char * _name,char * _lname,Bit32u & _size,Bit16u & _date,Bit16u & _time,Bit8u & _attr);

	void	SetDirID(Bit16u entry)			{ sSave(sDTA,dirID,entry); };
	void	SetDirIDCluster(Bit16u entry)	{ sSave(sDTA,dirCluster,entry); };
	Bit16u	GetDirID(void)				{ return (Bit16u)sGet(sDTA,dirID); };
	Bit16u	GetDirIDCluster(void)		{ return (Bit16u)sGet(sDTA,dirCluster); };
    Bit8u   GetAttr(void)               { return (Bit8u)sGet(sDTA,sattr); }
private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDTA {
		Bit8u sdrive;						/* The Drive the search is taking place */
        Bit8u spname[8];                    /* The Search pattern for the filename */              
        Bit8u spext[3];                     /* The Search pattern for the extension */
		Bit8u sattr;						/* The Attributes that need to be found */
		Bit16u dirID;						/* custom: dir-search ID for multiple searches at the same time */
		Bit16u dirCluster;					/* custom (drive_fat only): cluster number for multiple searches at the same time */
		Bit8u fill[4];
		Bit8u attr;
		Bit16u time;
		Bit16u date;
		Bit32u size;
		char name[DOS_NAMELENGTH_ASCII];
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

class DOS_FCB: public MemStruct {
public:
	DOS_FCB(Bit16u seg,Bit16u off,bool allow_extended=true);
	void Create(bool _extended);
    void SetName(Bit8u _drive, const char* _fname, const char* _ext);
	void SetSizeDateTime(Bit32u _size,Bit16u _date,Bit16u _time);
	void GetSizeDateTime(Bit32u & _size,Bit16u & _date,Bit16u & _time);
    void GetVolumeName(char * fillname);
	void GetName(char * fillname);
	void FileOpen(Bit8u _fhandle);
	void FileClose(Bit8u & _fhandle);
	void GetRecord(Bit16u & _cur_block,Bit8u & _cur_rec);
	void SetRecord(Bit16u _cur_block,Bit8u _cur_rec);
	void GetSeqData(Bit8u & _fhandle,Bit16u & _rec_size);
	void SetSeqData(Bit8u _fhandle,Bit16u _rec_size);
	void GetRandom(Bit32u & _random);
	void SetRandom(Bit32u  _random);
	Bit8u GetDrive(void);
	bool Extended(void);
	void GetAttr(Bit8u & attr);
	void SetAttr(Bit8u attr);
	void SetResult(Bit32u size,Bit16u date,Bit16u time,Bit8u attr);
	bool Valid(void);
	void ClearBlockRecsize(void);
private:
	bool extended = false;
	PhysPt real_pt;
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sFCB {
		Bit8u drive;			/* Drive number 0=default, 1=A, etc */
		Bit8u filename[8];		/* Space padded name */
		Bit8u ext[3];			/* Space padded extension */
		Bit16u cur_block;		/* Current Block */
		Bit16u rec_size;		/* Logical record size */
		Bit32u filesize;		/* File Size */
		Bit16u date;
		Bit16u time;
		/* Reserved Block should be 8 bytes */
		Bit8u sft_entries;
		Bit8u share_attributes;
		Bit8u extra_info;
		/* Maybe swap file_handle and sft_entries now that fcbs 
		 * aren't stored in the psp filetable anymore */
		Bit8u file_handle;
		Bit8u reserved[4];
		/* end */
		Bit8u  cur_rec;			/* Current record in current block */
		Bit32u rndm;			/* Current relative record number */
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

class DOS_MCB : public MemStruct{
public:
	DOS_MCB(Bit16u seg) { SetPt(seg); }
	void SetFileName(const char * const _name) { MEM_BlockWrite(pt+offsetof(sMCB,filename),_name,8); }
	void GetFileName(char * const _name) { MEM_BlockRead(pt+offsetof(sMCB,filename),_name,8);_name[8]=0;}
	void SetType(Bit8u _type) { sSave(sMCB,type,_type);}
	void SetSize(Bit16u _size) { sSave(sMCB,size,_size);}
	void SetPSPSeg(Bit16u _pspseg) { sSave(sMCB,psp_segment,_pspseg);}
	Bit8u GetType(void) { return (Bit8u)sGet(sMCB,type);}
	Bit16u GetSize(void) { return (Bit16u)sGet(sMCB,size);}
	Bit16u GetPSPSeg(void) { return (Bit16u)sGet(sMCB,psp_segment);}
private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sMCB {
		Bit8u type;
		Bit16u psp_segment;
		Bit16u size;	
		Bit8u unused[3];
		Bit8u filename[8];
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

class DOS_SDA : public MemStruct {
public:
	DOS_SDA(Bit16u _seg,Bit16u _offs) { SetPt(_seg,_offs); }
	void Init();   
	void SetDrive(Bit8u _drive) { sSave(sSDA,current_drive, _drive); }
	void SetDTA(Bit32u _dta) { sSave(sSDA,current_dta, _dta); }
	void SetPSP(Bit16u _psp) { sSave(sSDA,current_psp, _psp); }
	Bit8u GetDrive(void) { return (Bit8u)sGet(sSDA,current_drive); }
	Bit16u GetPSP(void) { return (Bit16u)sGet(sSDA,current_psp); }
	Bit32u GetDTA(void) { return (Bit32u)sGet(sSDA,current_dta); }
	
	
private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sSDA {
		Bit8u crit_error_flag;		/* 0x00 Critical Error Flag */
		Bit8u inDOS_flag;		/* 0x01 InDOS flag (count of active INT 21 calls) */
		Bit8u drive_crit_error;		/* 0x02 Drive on which current critical error occurred or FFh */
		Bit8u locus_of_last_error;	/* 0x03 locus of last error */
		Bit16u extended_error_code;	/* 0x04 extended error code of last error */
		Bit8u suggested_action;		/* 0x06 suggested action for last error */
		Bit8u error_class;		/* 0x07 class of last error*/
		Bit32u last_error_pointer; 	/* 0x08 ES:DI pointer for last error */
		Bit32u current_dta;		/* 0x0C current DTA (Disk Transfer Address) */
		Bit16u current_psp; 		/* 0x10 current PSP */
		Bit16u sp_int_23;		/* 0x12 stores SP across an INT 23 */
		Bit16u return_code;		/* 0x14 return code from last process termination (zerod after reading with AH=4Dh) */
		Bit8u current_drive;		/* 0x16 current drive */
		Bit8u extended_break_flag; 	/* 0x17 extended break flag */
		Bit8u fill[2];			/* 0x18 flag: code page switching || flag: copy of previous byte in case of INT 24 Abort*/
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};
extern DOS_InfoBlock dos_infoblock;

struct DOS_Block {
    DOS_Date date = {};
    DOS_Version version = {};
    Bit16u firstMCB = 0;
    Bit16u errorcode = 0;
    Bit16u psp();//{return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetPSP();};
    void psp(Bit16u _seg);//{ DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetPSP(_seg);};
    RealPt dta();//{return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDTA();};
    void dta(RealPt _dta);//{DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDTA(_dta);};
    Bit8u return_code = 0, return_mode = 0;

    Bit8u current_drive = 0;
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
        Bit8u* country = NULL;//Will be copied to dos memory. resides in real mem
        Bit16u dpb = 0; //Fake Disk parameter system using only the first entry so the drive letter matches
        Bit16u dpb_size = 0x21; // bytes per DPB entry (MS-DOS 4.x-6.x size)
        Bit16u mediaid_offset = 0x17; // media ID offset in DPB (MS-DOS 4.x-6.x case)
    } tables;
    Bit16u loaded_codepage = 0;
};

extern DOS_Block dos;

static INLINE Bit8u RealHandle(Bit16u handle) {
	DOS_PSP psp(dos.psp());	
	return psp.GetFileHandle(handle);
}

struct DOS_GetMemLog_Entry {
    Bit16u      segbase = 0;
    Bit16u      pages = 0;
    std::string who;
};

extern std::list<DOS_GetMemLog_Entry> DOS_GetMemLog;

#endif
