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

#include <assert.h>
#include <cmath>

#include "dosbox.h"
#include "callback.h"
#include "bios.h"
#include "bios_disk.h"
#include "timer.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h" /* for Drives[] */
#include "../dos/drives.h"
#include "mapper.h"
#include "ide.h"
#include "cpu.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#if defined(__linux__) && !defined(__GLIBC__)
// musl libc does not need 64 suffix to work with files > 2 GiB
#define fopen64 fopen
#define fseeko64 fseeko
#define ftello64 ftello
#endif

extern unsigned long freec;
extern const uint8_t freedos_mbr[];
extern int bootdrive, tryconvertcp;
extern bool int13_disk_change_detect_enable, skipintprog, rsize;
extern bool int13_extensions_enable, bootguest, bootvm, use_quick_reboot;
extern bool int13_enable_48bitLBA;
bool isDBCSCP(), isKanji1_gbk(uint8_t chr), shiftjis_lead_byte(int c), CheckDBCSCP(int32_t codepage);
extern bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);

#define STATIC_ASSERTM(A,B) static_assertion_##A##_##B
#define STATIC_ASSERTN(A,B) STATIC_ASSERTM(A,B)
#define STATIC_ASSERT(cond) typedef char STATIC_ASSERTN(__LINE__,__COUNTER__)[(cond)?1:-1]

uint32_t DriveCalculateCRC32(const uint8_t *ptr, size_t len, uint32_t crc)
{
	// Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C implementation that balances processor cache usage against speed": http://www.geocities.com/malbrain/
	static const uint32_t s_crc32[16] = { 0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c };
	uint32_t crcu32 = ~crc;
	while (len--) { uint8_t b = *ptr++; crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)]; crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)]; }
	return ~crcu32;
}

bool DriveFileIterator(DOS_Drive* drv, void(*func)(const char* path, bool is_dir, uint32_t size, uint16_t date, uint16_t time, uint8_t attr, Bitu data), Bitu data, int timeout)
{
	if (!drv) return true;
	uint32_t starttick = GetTicks();
	struct Iter
	{
		static bool ParseDir(DOS_Drive* drv, uint32_t startticks, const std::string& dir, std::vector<std::string>& dirs, void(*func)(const char* path, bool is_dir, uint32_t size, uint16_t date, uint16_t time, uint8_t attr, Bitu data), Bitu data, uint32_t timeout)
		{
			size_t dirlen = dir.length();
			if (dirlen + DOS_NAMELENGTH >= DOS_PATHLENGTH) return true;
			char full_path[DOS_PATHLENGTH+4];
			if (dirlen)
			{
				memcpy(full_path, &dir[0], dirlen);
				full_path[dirlen++] = '\\';
			}
			full_path[dirlen] = '\0';

			RealPt save_dta = dos.dta();
			dos.dta(dos.tables.tempdta);
			DOS_DTA dta(dos.dta());
			dta.SetupSearch(255, (uint8_t)(0xffff & ~DOS_ATTR_VOLUME), (char*)"*.*");
			for (bool more = drv->FindFirst((char*)dir.c_str(), dta); more; more = drv->FindNext(dta))
			{
                if (startticks && timeout > 0 && GetTicks()-startticks > timeout * 1000) {
                    LOG_MSG("Timeout iterating directories");
                    dos.dta(save_dta);
                    return false;
                }
				char dta_name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH+1]; uint32_t dta_size, dta_hsize; uint16_t dta_date, dta_time; uint8_t dta_attr;
				dta.GetResult(dta_name, lname, dta_size, dta_hsize, dta_date, dta_time, dta_attr);
				strcpy(full_path + dirlen, dta_name);
				bool is_dir = !!(dta_attr & DOS_ATTR_DIRECTORY);
				//if (is_dir) printf("[%s] [%s] %s (size: %u - date: %u - time: %u - attr: %u)\n", (const char*)data, (dta_attr == 8 ? "V" : (is_dir ? "D" : "F")), full_path, dta_size, dta_date, dta_time, dta_attr);
				if (dta_name[0] == '.' && dta_name[dta_name[1] == '.' ? 2 : 1] == '\0') continue;
				if (is_dir) dirs.emplace_back(full_path);
				func(full_path, is_dir, dta_size, dta_date, dta_time, dta_attr, data);
			}
			dos.dta(save_dta);
            return true;
		}
	};
	std::vector<std::string> dirs;
	dirs.emplace_back("");
	std::string dir;
	while (dirs.size())
	{
		std::swap(dirs.back(), dir);
		dirs.pop_back();
		if (!Iter::ParseDir(drv, starttick, dir.c_str(), dirs, func, data, timeout)) return false;
	}
	return true;
}

template <typename TVal> struct StringToPointerHashMap
{
	StringToPointerHashMap() : len(0), maxlen(0), keys(NULL), vals(NULL) { }
	~StringToPointerHashMap() { free(keys); free(vals); }

	static uint32_t Hash(const char* str, uint32_t str_limit = 0xFFFF, uint32_t hash_init = (uint32_t)0x811c9dc5)
	{
		for (const char* e = str + str_limit; *str && str != e;)
			hash_init = ((hash_init * (uint32_t)0x01000193) ^ (uint32_t)*(str++));
		return hash_init;
	}

	TVal* Get(const char* str, uint32_t str_limit = 0xFFFF, uint32_t hash_init = (uint32_t)0x811c9dc5) const
	{
		if (len == 0) return NULL;
		for (uint32_t key0 = Hash(str, str_limit, hash_init), key = (key0 ? key0 : 1), i = key;; i++)
		{
			if (keys[i &= maxlen] == key) return vals[i];
			if (!keys[i]) return NULL;
		}
	}

	void Put(const char* str, TVal* val, uint32_t str_limit = 0xFFFF, uint32_t hash_init = (uint32_t)0x811c9dc5)
	{
		if (len * 2 >= maxlen) Grow();
		for (uint32_t key0 = Hash(str, str_limit, hash_init), key = (key0 ? key0 : 1), i = key;; i++)
		{
			if (!keys[i &= maxlen]) { len++; keys[i] = key; vals[i] = val; return; }
			if (keys[i] == key) { vals[i] = val; return; }
		}
	}

	bool Remove(const char* str, uint32_t str_limit = 0xFFFF, uint32_t hash_init = (uint32_t)0x811c9dc5)
	{
		if (len == 0) return false;
		for (uint32_t key0 = Hash(str, str_limit, hash_init), key = (key0 ? key0 : 1), i = key;; i++)
		{
			if (keys[i &= maxlen] == key)
			{
				keys[i] = 0;
				len--;
				while ((key = keys[i = (i + 1) & maxlen]) != 0)
				{
					for (uint32_t j = key;; j++)
					{
						if (keys[j &= maxlen] == key) break;
						if (!keys[j]) { keys[i] = 0; keys[j] = key; vals[j] = vals[i]; break; }
					}
				}
				return true;
			}
			if (!keys[i]) return false;
		}
	}

	void Clear() { memset(keys, len = 0, (maxlen + 1) * sizeof(uint32_t)); }

	uint32_t Len() const { return len; }
	uint32_t Capacity() const { return (maxlen ? maxlen + 1 : 0); }
	TVal* GetAtIndex(uint32_t idx) const { return (keys[idx] ? vals[idx] : NULL); }

	struct Iterator
	{
		Iterator(StringToPointerHashMap<TVal>& _map, uint32_t _index) : map(_map), index(_index - 1) { this->operator++(); }
		StringToPointerHashMap<TVal>& map;
		uint32_t index;
		TVal* operator *() const { return map.vals[index]; }
		bool operator ==(const Iterator &other) const { return index == other.index; }
		bool operator !=(const Iterator &other) const { return index != other.index; }
		Iterator& operator ++()
		{
			if (!map.maxlen) { index = 0; return *this; }
			if (++index > map.maxlen) index = map.maxlen + 1;
			while (index <= map.maxlen && !map.keys[index]) index++;
			return *this;
		}
	};

	Iterator begin() { return Iterator(*this, 0); }
	Iterator end() { return Iterator(*this, (maxlen ? maxlen + 1 : 0)); }

private:
	uint32_t len, maxlen, *keys;
	TVal** vals;

	void Grow()
	{
		uint32_t oldMax = maxlen, oldCap = (maxlen ? oldMax + 1 : 0), *oldKeys = keys;
		TVal **oldVals = vals;
		maxlen  = (maxlen ? maxlen * 2 + 1 : 15);
		keys = (uint32_t*)calloc(maxlen + 1, sizeof(uint32_t));
		vals = (TVal**)malloc((maxlen + 1) * sizeof(TVal*));
		for (uint32_t i = 0; i != oldCap; i++)
		{
			if (!oldKeys[i]) continue;
			for (uint32_t key = oldKeys[i], j = key;; j++)
			{
				if (!keys[j &= maxlen]) { keys[j] = key; vals[j] = oldVals[i]; break; }
			}
		}
		free(oldKeys);
		free(oldVals);
	}

	// not copyable
	StringToPointerHashMap(const StringToPointerHashMap&);
	StringToPointerHashMap& operator=(const StringToPointerHashMap&);
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct sector {
	uint8_t  content[512];
} GCC_ATTRIBUTE(packed);

typedef struct {
	uint8_t sectors;
	uint8_t surfaces;
	uint16_t cylinders;
} SASIHDD;

struct bootstrap {
	uint8_t  nearjmp[3];
	uint8_t  oemname[8];
	uint8_t  bytespersector[2];
	uint8_t  sectorspercluster;
	uint16_t reservedsectors;
	uint8_t  fatcopies;
	uint16_t rootdirentries;
	uint16_t totalsectorcount;
	uint8_t  mediadescriptor;
	uint16_t sectorsperfat;
	uint16_t sectorspertrack;
	uint16_t headcount;
	uint32_t hiddensectorcount;
	uint32_t totalsecdword;
	uint8_t  bootcode[474];
	uint8_t  magic1; /* 0x55 */
	uint8_t  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

struct lfndirentry {
	uint8_t ord;
	uint8_t name1[10];
	uint8_t attrib;
	uint8_t type;
	uint8_t chksum;
	uint8_t name2[12];
	uint16_t loFirstClust;
	uint8_t name3[4];
	char* Name(int j) { return (char*)(j < 5 ? name1 + j*2 : j < 11 ? name2 + (j-5)*2 : name3 + (j-11)*2); }
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

/* .HDI and .FDI header (NP2) */
#pragma pack(push,1)
typedef struct {
    uint8_t dummy[4];           // +0x00
    uint8_t hddtype[4];         // +0x04
    uint8_t headersize[4];      // +0x08
    uint8_t hddsize[4];         // +0x0C
    uint8_t sectorsize[4];      // +0x10
    uint8_t sectors[4];         // +0x14
    uint8_t surfaces[4];        // +0x18
    uint8_t cylinders[4];       // +0x1C
} HDIHDR;                       // =0x20

typedef struct {
	uint8_t	dummy[4];           // +0x00
	uint8_t	fddtype[4];         // +0x04
	uint8_t	headersize[4];      // +0x08
	uint8_t	fddsize[4];         // +0x0C
	uint8_t	sectorsize[4];      // +0x10
	uint8_t	sectors[4];         // +0x14
	uint8_t	surfaces[4];        // +0x18
	uint8_t	cylinders[4];       // +0x1C
} FDIHDR;                       // =0x20

typedef struct {
    char        szFileID[15];                 // 識別ID "T98HDDIMAGE.R0"
    char        Reserve1[1];                  // 予約
    char        szComment[0x100];             // イメージコメント(ASCIIz)
    uint32_t    dwHeadSize;                   // ヘッダ部のサイズ
    uint32_t    dwCylinder;                   // シリンダ数
    uint16_t    wHead;                        // ヘッド数
    uint16_t    wSect;                        // １トラックあたりのセクタ数
    uint16_t    wSectLen;                     // セクタ長
    char        Reserve2[2];                  // 予約
    char        Reserve3[0xe0];               // 予約
}NHD_FILE_HEAD,*LP_NHD_FILE_HEAD;
#pragma pack(pop)

#define	STOREINTELDWORD(a, b) *((a)+0) = (uint8_t)((b)); *((a)+1) = (uint8_t)((b)>>8); *((a)+2) = (uint8_t)((b)>>16); *((a)+3) = (uint8_t)((b)>>24)

STATIC_ASSERT(sizeof(direntry) == sizeof(lfndirentry));
enum
{
	DOS_ATTR_LONG_NAME = (DOS_ATTR_READ_ONLY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM | DOS_ATTR_VOLUME),
	DOS_ATTR_LONG_NAME_MASK = (DOS_ATTR_READ_ONLY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM | DOS_ATTR_VOLUME | DOS_ATTR_DIRECTORY | DOS_ATTR_ARCHIVE),
	DOS_ATTR_PENDING_SHORT_NAME = 0x80,
};

static const uint8_t hdddiskboot[] = {
    0xeb,0x0a,0x90,0x90,0x49,0x50,0x4c,0x31,0x00,0x00,0x00,0x1e,
    0xb8,0x04,0x0a,0xcd,0x18,0xb4,0x16,0xba,0x20,0xe1,0xcd,0x18,
    0xfa,0xfc,0xb8,0x00,0xa0,0x8e,0xc0,0xbe,0x3c,0x00,0x31,0xff,
    0xe8,0x09,0x00,0xbf,0xa0,0x00,0xe8,0x03,0x00,0xf4,0xeb,0xfd,
    0x2e,0xad,0x85,0xc0,0x74,0x05,0xab,0x47,0x47,0xeb,0xf5,0xc3,
    0x04,0x33,0x04,0x4e,0x05,0x4f,0x01,0x3c,0x05,0x49,0x05,0x47,
    0x05,0x23,0x05,0x39,0x05,0x2f,0x05,0x24,0x05,0x61,0x01,0x3c,
    0x05,0x38,0x04,0x4f,0x05,0x55,0x05,0x29,0x01,0x3c,0x05,0x5e,
    0x05,0x43,0x05,0x48,0x04,0x35,0x04,0x6c,0x04,0x46,0x04,0x24,
    0x04,0x5e,0x04,0x3b,0x04,0x73,0x01,0x25,0x00,0x00,0x05,0x47,
    0x05,0x23,0x05,0x39,0x05,0x2f,0x05,0x24,0x05,0x61,0x01,0x3c,
    0x05,0x38,0x04,0x72,0x21,0x5e,0x26,0x7e,0x18,0x65,0x01,0x24,
    0x05,0x6a,0x05,0x3b,0x05,0x43,0x05,0x48,0x04,0x37,0x04,0x46,
    0x12,0x3c,0x04,0x35,0x04,0x24,0x01,0x25,0x00,0x00,
};

struct fatFromDOSDrive
{
	DOS_Drive* drive;

	enum ffddDefs : uint32_t
	{
		BYTESPERSECTOR    = 512,
		HEADCOUNT         = 255, // needs to be >128 to fit 4GB into CHS
		SECTORSPERTRACK   = 63,
		SECT_MBR          = 0,
		SECT_BOOT         = 32,
		CACHECOUNT        = 256,
		KEEPOPENCOUNT     = 8,
		NULL_CURSOR       = (uint32_t)-1,
	};

	partTable  mbr;
	bootstrap  bootsec;
	sector     header;
	sector     ipl;
	sector     pt;
	SASIHDD    sasi;
	uint8_t    fatSz;
	uint8_t    fsinfosec[BYTESPERSECTOR];
	uint32_t   sectorsPerCluster, codepage = 0;
	bool       tryconvcp = false, readOnly = false, success = false, tomany = false;

	struct ffddFile { char path[DOS_PATHLENGTH+1]; uint32_t firstSect; };
	std::vector<direntry> root, dirs;
	std::vector<ffddFile> files;
	std::vector<uint32_t>   fileAtSector;
	std::vector<uint8_t>    fat;
	uint32_t sect_boot_pc98, sect_disk_end, sect_files_end, sect_files_start, sect_dirs_start, sect_root_start, sect_fat2_start, sect_fat1_start;

	struct ffddBuf { uint8_t data[BYTESPERSECTOR]; };
	struct ffddSec { uint32_t cursor = NULL_CURSOR; };
	std::vector<ffddBuf>  diffSectorBufs;
	std::vector<ffddSec>  diffSectors;
	std::vector<uint32_t>   diffFreeCursors;
	uint32_t                saveEndCursor = 0;
	uint8_t                 cacheSectorData[CACHECOUNT][BYTESPERSECTOR];
	uint32_t                cacheSectorNumber[CACHECOUNT];
	DOS_File*             openFiles[KEEPOPENCOUNT];
	uint32_t                openIndex[KEEPOPENCOUNT];
	uint32_t                openCursor = 0;

	~fatFromDOSDrive()
	{
		for (DOS_File* df : openFiles)
			if (df) { df->Close(); delete df; }
	}

	fatFromDOSDrive(DOS_Drive* drv, uint32_t freeMB, int timeout) : drive(drv)
	{
		cacheSectorNumber[0] = 1; // must not state that sector 0 is already cached
		memset(&cacheSectorNumber[1], 0, sizeof(cacheSectorNumber) - sizeof(cacheSectorNumber[0]));
		memset(openFiles, 0, sizeof(openFiles));

		struct Iter
		{
			static void SetFAT(fatFromDOSDrive& ffdd, size_t idx, uint32_t val)
			{
				while (idx >= (uint64_t)ffdd.fat.size() * 8 / ffdd.fatSz)
				{
					// FAT12 table grows in steps of 3 sectors otherwise the table doesn't align
					size_t addSz = (ffdd.fatSz != 12 ? BYTESPERSECTOR : (BYTESPERSECTOR * 3));
					ffdd.fat.resize(ffdd.fat.size() + addSz);
					memset(&ffdd.fat[ffdd.fat.size() - addSz], 0, addSz);
				}
				if (ffdd.fatSz == 32) // FAT32
					var_write((uint32_t *)&ffdd.fat[idx * 4], val);
				else if (ffdd.fatSz == 16) // FAT 16
					var_write((uint16_t *)&ffdd.fat[idx * 2], (uint16_t)val);
				else if (idx & 1) // FAT12 odd cluster
					var_write((uint16_t *)&ffdd.fat[idx + idx / 2], (uint16_t)((var_read((uint16_t *)&ffdd.fat[idx + idx / 2]) & 0xF) | ((val & 0xFFF) << 4)));
				else // FAT12 even cluster
					var_write((uint16_t *)&ffdd.fat[idx + idx / 2], (uint16_t)((var_read((uint16_t *)&ffdd.fat[idx + idx / 2]) & 0xF000) | (val & 0xFFF)));
			}

			static direntry* AddDirEntry(fatFromDOSDrive& ffdd, bool useFAT16Root, size_t& diridx)
			{
				const uint32_t entriesPerCluster = ffdd.sectorsPerCluster * BYTESPERSECTOR / sizeof(direntry);
				if (!useFAT16Root && (diridx % entriesPerCluster) == 0)
				{
					// link fat (was set to 0xFFFF before but now we knew the chain continues)
					if (diridx) SetFAT(ffdd, 2 + (diridx - 1) / entriesPerCluster, (uint32_t)(2 + ffdd.dirs.size() / entriesPerCluster));
					diridx = ffdd.dirs.size();
					ffdd.dirs.resize(diridx + entriesPerCluster);
					memset(&ffdd.dirs[diridx], 0, sizeof(direntry) * entriesPerCluster);
					SetFAT(ffdd, 2 + diridx / entriesPerCluster, (uint32_t)0xFFFFFFFF); // set as last cluster in chain for now
				}
				else if (useFAT16Root && diridx && (diridx % 512) == 0)
				{
					// this actually should never be larger than 512 for some FAT16 drivers
					ffdd.root.resize(diridx + 512);
					memset(&ffdd.root[diridx], 0, sizeof(direntry) * 512);
				}
				return &(!useFAT16Root ? ffdd.dirs : ffdd.root)[diridx++];
			}

			static void ParseDir(fatFromDOSDrive& ffdd, char* dir, const StringToPointerHashMap<void>* filter, int dirlen = 0, uint16_t parentFirstCluster = 0)
			{
				if (ffdd.tomany) return;
				const bool useFAT16Root = (!dirlen && ffdd.fatSz != 32), readOnly = ffdd.readOnly;
				const size_t firstidx = (!useFAT16Root ? ffdd.dirs.size() : 0);
				const uint32_t sectorsPerCluster = ffdd.sectorsPerCluster, bytesPerCluster = sectorsPerCluster * BYTESPERSECTOR, entriesPerCluster = bytesPerCluster / sizeof(direntry);
				const uint16_t myFirstCluster = (dirlen ? (uint16_t)(2 + firstidx / entriesPerCluster) : (uint16_t)0) ;

				char finddir[DOS_PATHLENGTH+4];
				memcpy(finddir, dir, dirlen); // because FindFirst can modify this...
				finddir[dirlen] = '\0';
				if (dirlen) dir[dirlen++] = '\\';

				size_t diridx = 0;
				RealPt save_dta = dos.dta();
				dos.dta(dos.tables.tempdta);
				DOS_DTA dta(dos.dta());
				dta.SetupSearch(255, 0xFF, (char*)"*.*");
				skipintprog = true;
				for (bool more = ffdd.drive->FindFirst(finddir, dta); more; more = ffdd.drive->FindNext(dta))
				{
					char dta_name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH+1]; uint32_t dta_size, dta_hsize; uint16_t dta_date, dta_time; uint8_t dta_attr;
					dta.GetResult(dta_name, lname, dta_size, dta_hsize, dta_date, dta_time, dta_attr);
                    //LOG_MSG("dta_name %s lname %s\n", dta_name, lname);
					const char *fend = dta_name + strlen(dta_name);
					const bool dot = (dta_name[0] == '.' && dta_name[1] == '\0'), dotdot = (dta_name[0] == '.' && dta_name[1] == '.' && dta_name[2] == '\0');
					if (!dirlen && (dot || dotdot)) continue; // root shouldn't have dot entries (yet localDrive does...)

					ffddFile f;
					memcpy(f.path, dir, dirlen);
					memcpy(f.path + dirlen, dta_name, fend - dta_name + 1);
					if (filter && filter->Get(f.path, sizeof(f.path))) continue;

					const bool isLongFileName = (!dot && !dotdot && !(dta_attr & DOS_ATTR_VOLUME));
					if (isLongFileName)
					{
						bool lead = false;
						size_t len = 0, lfnlen = strlen(lname);
                        uint16_t *lfnw = (uint16_t *)malloc((lfnlen + 1) * sizeof(uint16_t));
                        if (lfnw == NULL) continue;
                        char text[3];
                        uint16_t uname[4];
#if defined(WIN32)
                        uint16_t cp = GetACP(), cpbak = dos.loaded_codepage;
                        if (tryconvertcp && cpbak == 437 && CheckDBCSCP(cp))
                            dos.loaded_codepage = cp;
#endif
                        for (size_t i=0; i < lfnlen; i++) {
                            if (lead) {
                                lead = false;
                                text[0]=lname[i-1]&0xFF;
                                text[1]=lname[i]&0xFF;
                                text[2]=0;
                                uname[0]=0;
                                uname[1]=0;
                                if (CodePageGuestToHostUTF16(uname,text)&&uname[0]!=0&&uname[1]==0)
                                    lfnw[len++] = uname[0];
                                else {
                                    lfnw[len++] = lname[i-1];
                                    lfnw[len++] = lname[i];
                                }
                            } else if (i+1<lfnlen && ((IS_PC98_ARCH && shiftjis_lead_byte(lname[i]&0xFF)) || (isDBCSCP() && isKanji1_gbk(lname[i]&0xFF)))) lead = true;
                            else if (dos.loaded_codepage != 437) {
                                text[0]=lname[i]&0xFF;
                                text[1]=0;
                                lfnw[len++] = CodePageGuestToHostUTF16(uname,text)&&uname[0]!=0&&uname[1]==0 ? uname[0] : lname[i];
                            } else
                                lfnw[len++] = lname[i];
                        }
#if defined(WIN32)
                        dos.loaded_codepage = cpbak;
#endif
						uint16_t *lfn_end = lfnw + len;
						for (size_t i = 0, lfnblocks = (len + 12) / 13; i != lfnblocks; i++)
						{
							lfndirentry* le = (lfndirentry*)AddDirEntry(ffdd, useFAT16Root, diridx);
							le->ord = (uint8_t)((lfnblocks - i)|(i == 0 ? 0x40 : 0x0));
							le->attrib = DOS_ATTR_LONG_NAME;
							le->type = 0;
							le->loFirstClust = 0;
							uint16_t* plfn = lfnw + (lfnblocks - i - 1) * 13;
							for (int j = 0; j != 13; j++, plfn++)
							{
								char* p = le->Name(j);
								if (plfn > lfn_end)
                                    p[0] = p[1] = (char)0xFF;
								else if (plfn == lfn_end)
                                    p[0] = p[1] = 0;
                                else if (*plfn > 0xFF) {
                                    p[0] = (uint8_t)(*plfn%0x100);
                                    p[1] = (uint8_t)(*plfn/0x100);
                                } else {
                                    p[0] = *plfn;
                                    p[1] = 0;
                                }
							}
						}
                        free(lfnw);
					}

					const char *fext = (dot || dotdot ? NULL : strrchr(dta_name, '.'));
					direntry* e = AddDirEntry(ffdd, useFAT16Root, diridx);
					memset(e->entryname, ' ', sizeof(e->entryname));
					memcpy(e->entryname, dta_name, (fext ? fext : fend) - dta_name);
					if (fext++) memcpy(e->entryname + 8, fext, fend - fext);

					e->attrib = dta_attr | (readOnly ? DOS_ATTR_READ_ONLY : 0) | (isLongFileName ? DOS_ATTR_PENDING_SHORT_NAME : 0);
                    if (dos.version.major >= 7) {
                        var_write(&e->crtTime,    dta_time); // create date/time is DOS 7.0 and up only
                        var_write(&e->crtDate,    dta_date); // create date/time is DOS 7.0 and up only
                    }
					var_write(&e->accessDate, dta_date);
					var_write(&e->modTime,    dta_time);
					var_write(&e->modDate,    dta_date);

					if (dot)
					{
						e->attrib |= DOS_ATTR_DIRECTORY; // make sure
						var_write(&e->loFirstClust, myFirstCluster);
					}
					else if (dotdot)
					{
						e->attrib |= DOS_ATTR_DIRECTORY; // make sure
						var_write(&e->loFirstClust, parentFirstCluster);
					}
					else if (dta_attr & DOS_ATTR_VOLUME)
					{
						if (dirlen || (e->attrib & DOS_ATTR_DIRECTORY) || dta_size)
							LOG_MSG("Invalid volume entry - %s\n", e->entryname);
					}
					else if (!(dta_attr & DOS_ATTR_DIRECTORY))
					{
						var_write(&e->entrysize, dta_size);

						uint32_t fileIdx = (uint32_t)ffdd.files.size();
						ffdd.files.push_back(f);

						uint32_t numSects = (dta_size + bytesPerCluster - 1) / bytesPerCluster * sectorsPerCluster;
                        try {
                            ffdd.fileAtSector.resize(ffdd.fileAtSector.size() + numSects, fileIdx);
                        } catch (...) {
                            LOG_MSG("Too many sectors needed, will discard remaining files (from %s)", lname);
                            ffdd.tomany = ffdd.readOnly = true;
                            var_write((uint32_t *)&ffdd.fsinfosec[488], (uint32_t)0x0);
                            break;
                        }
					}
				}
				skipintprog = false;
				dos.dta(save_dta);
				if (dirlen && diridx < firstidx + 2) {
					LOG_MSG("Directory need at least . and .. entries - %s\n", finddir);
					return;
				}

				// Now fill out the subdirectories (can't be done above because only one dos.dta can run simultaneously
				std::vector<direntry>& entries = (!useFAT16Root ? ffdd.dirs : ffdd.root);
				for (size_t ei = firstidx; ei != diridx; ei++)
				{
					direntry& e = entries[ei];
					uint8_t* entryname = e.entryname;
					int totlen = dirlen;
					if (e.attrib & DOS_ATTR_DIRECTORY) // copy name before modifying SFN
					{
						if (entryname[0] == '.' && entryname[entryname[1] == '.' ? 2 : 1] == ' ') continue;
						for (int i = 0; i != 8 && entryname[i] != ' '; i++) dir[totlen++] = entryname[i];
						if (entryname[8] != ' ') dir[totlen++] = '.';
						for (int i = 8; i != 11 && entryname[i] != ' '; i++) dir[totlen++] = entryname[i];
					}
					if (e.attrib & DOS_ATTR_PENDING_SHORT_NAME) // convert LFN to SFN
					{
						uint8_t chksum = 0;
						for (int i = 0; i != 11;) chksum = (chksum >> 1) + (chksum << 7) + e.entryname[i++];
						for (lfndirentry* le = (lfndirentry*)&e; le-- == (lfndirentry*)&e || !(le[1].ord & 0x40);) le->chksum = chksum;
						e.attrib &= ~DOS_ATTR_PENDING_SHORT_NAME;
					}
					if (e.attrib & DOS_ATTR_DIRECTORY) // this reallocates ffdd.dirs so do this last
					{
						var_write(&e.loFirstClust, (uint16_t)(2 + ffdd.dirs.size() / entriesPerCluster));
						ParseDir(ffdd, dir, filter, totlen, myFirstCluster);
					}
				}
			}

			struct SumInfo { uint64_t used_bytes; const StringToPointerHashMap<void>* filter; };
			static void SumFileSize(const char* path, bool /*is_dir*/, uint32_t size, uint16_t, uint16_t, uint8_t, Bitu data)
			{
				if (!((SumInfo*)data)->filter || !((SumInfo*)data)->filter->Get(path))
					((SumInfo*)data)->used_bytes += (size + (32*1024-1)) / (32*1024) * (32*1024); // count as 32 kb clusters
			}
		};

		drv->EmptyCache();
		Iter::SumInfo sum = { 0, NULL };
		uint64_t freeSpace = 0, freeSpaceMB = 0;
        uint32_t free_clusters = 0;
        uint16_t drv_bytes_sector; uint8_t drv_sectors_cluster;  uint16_t drv_total_clusters, drv_free_clusters;
        rsize=true;
        freec=0;
        drv->AllocationInfo(&drv_bytes_sector, &drv_sectors_cluster, &drv_total_clusters, &drv_free_clusters);
        free_clusters = freec?freec:drv_free_clusters;
        freeSpace = (uint64_t)drv_bytes_sector * drv_sectors_cluster * (freec?freec:free_clusters);
        freeSpaceMB = freeSpace / (1024*1024);
        if (freeMB < freeSpaceMB) freeSpaceMB = freeMB;
        rsize=false;
        tomany=false;
        readOnly = free_clusters == 0 || freeSpaceMB == 0;
        if (!DriveFileIterator(drv, Iter::SumFileSize, (Bitu)&sum, timeout)) return;

        uint32_t usedMB = sum.used_bytes / (1024*1024), addFreeMB, totalMB, tsizeMB;
        uint64_t tsize = 0;
        if (IS_PC98_ARCH) {
            if (usedMB < 6) {
                sasi.sectors = 33;
                sasi.surfaces = 4;
                sasi.cylinders = 153;
            } else if (usedMB < 16) {
                sasi.sectors = 33;
                sasi.surfaces = 4;
                sasi.cylinders = 310;
            } else if (usedMB < 26) {
                sasi.sectors = 33;
                sasi.surfaces = 6;
                sasi.cylinders = 310;
            } else if (usedMB < 36) {
                sasi.sectors = 33;
                sasi.surfaces = 8;
                sasi.cylinders = 310;
            } else {
                sasi.sectors = 33;
                uint32_t heads = std::ceil((double)(usedMB+(readOnly?0:(usedMB>=2047?freeSpaceMB:5)))/10);
                if (heads > 255) {
                    sasi.surfaces = 255;
                    sasi.cylinders = heads * 615 / 255;
                } else {
                    sasi.surfaces = heads;
                    sasi.cylinders = 615;
                }
            }
            tsize = BYTESPERSECTOR * sasi.sectors * sasi.surfaces * sasi.cylinders;
            tsizeMB = sasi.sectors * sasi.surfaces * sasi.cylinders / (1024 * 1024 / BYTESPERSECTOR);
            if (tsizeMB < usedMB) readOnly = true;
            addFreeMB = readOnly ? 0 : (usedMB >= 2047 ? freeSpaceMB : (std::ceil((double)tsize - sum.used_bytes) / (1024 * 1024) + 1));
        } else
            addFreeMB = (readOnly ? 0 : freeSpaceMB);
        totalMB = usedMB + (addFreeMB ? (1 + addFreeMB) : 0);
		if      (totalMB >= 3072) { fatSz = 32; sectorsPerCluster = 64; } // 32 kb clusters ( 98304 ~        FAT entries)
		else if (totalMB >= 2048) { fatSz = 32; sectorsPerCluster = 32; } // 16 kb clusters (131072 ~ 196608 FAT entries)
		else if (totalMB >=  384) { fatSz = 16; sectorsPerCluster = 64; } // 32 kb clusters ( 12288 ~  65504 FAT entries)
		else if (totalMB >=  192) { fatSz = 16; sectorsPerCluster = 32; } // 16 kb clusters ( 12288 ~  24576 FAT entries)
		else if (totalMB >=   96) { fatSz = 16; sectorsPerCluster = 16; } //  8 kb clusters ( 12288 ~  24576 FAT entries)
		else if (totalMB >=   48) { fatSz = 16; sectorsPerCluster =  8; } //  4 kb clusters ( 12288 ~  24576 FAT entries)
		else if (totalMB >=   12) { fatSz = 16; sectorsPerCluster =  4; } //  2 kb clusters (  6144 ~  24576 FAT entries)
		else if (totalMB >=    4) { fatSz = 16; sectorsPerCluster =  1; } // .5 kb clusters (  8192 ~  24576 FAT entries)
		else if (totalMB >=    2) { fatSz = 12; sectorsPerCluster =  4; } //  2 kb clusters (  1024 ~   2048 FAT entries)
		else if (totalMB >=    1) { fatSz = 12; sectorsPerCluster =  2; } //  1 kb clusters (  1024 ~   2048 FAT entries)
		else                      { fatSz = 12; sectorsPerCluster =  1; } // .5 kb clusters (       ~   2048 FAT entries)

		// mediadescriptor in very first byte of FAT table
		Iter::SetFAT(*this, 0, (uint32_t)0xFFFFFF8);
		Iter::SetFAT(*this, 1, (uint32_t)0xFFFFFFF);
        
		if (fatSz != 32)
		{
			// this actually should never be anything but 512 for some FAT16 drivers
			root.resize(512);
			memset(&root[0], 0, sizeof(direntry) * 512);
		}

		char dirbuf[DOS_PATHLENGTH+4];
		Iter::ParseDir(*this, dirbuf, NULL);

		const uint32_t bytesPerCluster = sectorsPerCluster * BYTESPERSECTOR;
		const uint32_t entriesPerCluster = bytesPerCluster / sizeof(direntry);
		uint32_t fileCluster = (uint32_t)(2 + dirs.size() / entriesPerCluster);
		for (uint32_t fileSect = 0, rootOrDir = 0; rootOrDir != 2; rootOrDir++)
		{
			for (direntry& e : (rootOrDir ? dirs : root))
			{
				if (!e.entrysize || (e.attrib & DOS_ATTR_LONG_NAME_MASK) == DOS_ATTR_LONG_NAME) continue;
				var_write(&e.hiFirstClust, (uint16_t)(fileCluster >> 16));
				var_write(&e.loFirstClust, (uint16_t)(fileCluster));

				// Write FAT link chain
				uint32_t numClusters = (var_read(&e.entrysize) + bytesPerCluster - 1) / bytesPerCluster;
				for (uint32_t i = fileCluster, iEnd = i + numClusters - 1; i != iEnd; i++) Iter::SetFAT(*this, i, i + 1);
				Iter::SetFAT(*this, fileCluster + numClusters - 1, (uint32_t)0xFFFFFFF);

				files[fileAtSector[fileSect]].firstSect = fileSect;

				fileCluster += numClusters;
				fileSect += numClusters * sectorsPerCluster;
			}
		}

        if (IS_PC98_ARCH) {
            HDIHDR hdi;
            memset(&hdi, 0, sizeof(hdi));
        //	STOREINTELDWORD(hdi.hddtype, 0);
            STOREINTELDWORD(hdi.headersize, 4096);
            STOREINTELDWORD(hdi.hddsize, (uint32_t)tsize);
            STOREINTELDWORD(hdi.sectorsize, BYTESPERSECTOR);
            STOREINTELDWORD(hdi.sectors, sasi.sectors);
            STOREINTELDWORD(hdi.surfaces, sasi.surfaces);
            STOREINTELDWORD(hdi.cylinders, sasi.cylinders);
            memset(&header, 0, sizeof(header));
            memcpy(&header,&hdi,sizeof(hdi));
            memset(&ipl, 0, sizeof(ipl));
            memcpy(&ipl,&hdddiskboot,sizeof(hdddiskboot));
            ipl.content[0xFE] = 0x55;
            ipl.content[0xFF] = 0xaa;
            ipl.content[0x1FE] = 0x55;
            ipl.content[0x1FF] = 0xaa;
            struct _PC98RawPartition pe;
            memset(&pe, 0, sizeof(pe));
            STOREINTELDWORD(&pe.mid, 0xa0);
            STOREINTELDWORD(&pe.sid, 0xa1);
            STOREINTELDWORD(&pe.ipl_cyl, 1);
            STOREINTELDWORD(&pe.cyl, 1);
            STOREINTELDWORD(&pe.end_cyl, sasi.cylinders);
            strncpy(pe.name, "MS-DOS          ", 16);
            memset(&pt, 0, sizeof(pt));
            memcpy(&pt,&pe,sizeof(pe));
        }

		// Add at least one page after the last file or FAT spec minimum to make ScanDisk happy (even on read-only disks)
		const uint32_t FATPageClusters = BYTESPERSECTOR * 8 / fatSz, FATMinCluster = (fatSz == 32 ? 65525 : (fatSz == 16 ? 4085 : 0)) + FATPageClusters;
		const uint32_t addFreeClusters = ((addFreeMB * (1024*1024/BYTESPERSECTOR)) + sectorsPerCluster - 1) / sectorsPerCluster;
		const uint32_t targetClusters = fileCluster + (addFreeClusters < FATPageClusters ? FATPageClusters : addFreeClusters);
		Iter::SetFAT(*this, (targetClusters < FATMinCluster ? FATMinCluster : targetClusters) - 1, 0);
		const uint32_t totalClusters = (uint32_t)((uint64_t)fat.size() * 8 / fatSz); // as set by Iter::SetFAT

		// on read-only disks, fill up the end of the FAT table with "Bad sector in cluster or reserved cluster" markers
		if (readOnly)
			for (uint32_t cluster = fileCluster; cluster != totalClusters; cluster++)
				Iter::SetFAT(*this, cluster, 0xFFFFFF7);

		const uint32_t sectorsPerFat = (uint32_t)(fat.size() / BYTESPERSECTOR);
		const uint16_t reservedSectors = (fatSz == 32 ? 32 : 1);
		const uint32_t partSize = totalClusters * sectorsPerCluster + reservedSectors;

		sect_boot_pc98 = sasi.surfaces * sasi.sectors;
		sect_fat1_start = (IS_PC98_ARCH ? sect_boot_pc98 : SECT_BOOT) + reservedSectors;
		sect_fat2_start = sect_fat1_start + sectorsPerFat;
		sect_root_start = sect_fat2_start + sectorsPerFat;
		sect_dirs_start = sect_root_start + ((root.size() * sizeof(direntry) + BYTESPERSECTOR - 1) / BYTESPERSECTOR);
		sect_files_start = sect_dirs_start + ((dirs.size() * sizeof(direntry) + BYTESPERSECTOR - 1) / BYTESPERSECTOR);
		sect_files_end = sect_files_start + fileAtSector.size();
		sect_disk_end = (IS_PC98_ARCH ? sect_boot_pc98 : SECT_BOOT) + partSize;
		if (IS_PC98_ARCH && tsize/BYTESPERSECTOR-8 > sect_disk_end) sect_disk_end = tsize/BYTESPERSECTOR-8;
		if (sect_disk_end < sect_files_end) return;

		for (ffddFile& f : files)
			f.firstSect += sect_files_start;

		uint32_t serial = 0;
		if (!serial)
		{
			serial = DriveCalculateCRC32(&fat[0], fat.size(), 0);
			if (root.size()) serial = DriveCalculateCRC32((uint8_t*)&root[0], root.size() * sizeof(direntry), serial);
			if (dirs.size()) serial = DriveCalculateCRC32((uint8_t*)&dirs[0], dirs.size() * sizeof(direntry), serial);
		}

		memset(&mbr, 0, sizeof(mbr));
		//memcpy(&mbr,freedos_mbr,512);
		var_write((uint32_t *)&mbr.booter[440], serial); //4 byte disk serial number
		var_write(&mbr.pentry[0].bootflag, 0x80); //Active bootable
		if ((sect_disk_end - 1) / (HEADCOUNT * SECTORSPERTRACK) > 0x3FF)
		{
			mbr.pentry[0].beginchs[0] = mbr.pentry[0].beginchs[1] = mbr.pentry[0].beginchs[2] = 0;
			mbr.pentry[0].endchs[0] = mbr.pentry[0].endchs[1] = mbr.pentry[0].endchs[2] = 0;
		}
		else
		{
			chs_write(mbr.pentry[0].beginchs, IS_PC98_ARCH ? sect_boot_pc98 : SECT_BOOT);
			chs_write(mbr.pentry[0].endchs, sect_disk_end - 1);
		}
		var_write(&mbr.pentry[0].absSectStart, IS_PC98_ARCH ? sect_boot_pc98 : SECT_BOOT);
		var_write(&mbr.pentry[0].partSize, partSize);
		mbr.magic1 = 0x55; mbr.magic2 = 0xaa;

		memset(&bootsec, 0, sizeof(bootsec));
		memcpy(bootsec.nearjmp, "\xEB\x3C\x90", sizeof(bootsec.nearjmp));
		memcpy(bootsec.oemname, fatSz == 32 ? "MSWIN4.1" : "MSDOS5.0", sizeof(bootsec.oemname));
		var_write((uint16_t *)&bootsec.bytespersector, (uint16_t)BYTESPERSECTOR);
		var_write(&bootsec.sectorspercluster, sectorsPerCluster);
		var_write(&bootsec.reservedsectors, reservedSectors);
		var_write(&bootsec.fatcopies, 2);
		var_write(&bootsec.totalsectorcount, 0); // 16 bit field is 0, actual value is in totalsecdword
		var_write(&bootsec.mediadescriptor, 0xF8); //also in FAT[0]
		var_write(&bootsec.sectorspertrack, IS_PC98_ARCH ? sasi.sectors : SECTORSPERTRACK);
		var_write(&bootsec.headcount, IS_PC98_ARCH ? sasi.surfaces : HEADCOUNT);
		var_write(&bootsec.hiddensectorcount, IS_PC98_ARCH ? sect_boot_pc98 : SECT_BOOT);
		var_write(&bootsec.totalsecdword, partSize);
		bootsec.magic1 = 0x55; bootsec.magic2 = 0xaa;
		if (fatSz != 32) // FAT12/FAT16
		{
			var_write(&mbr.pentry[0].parttype, (fatSz == 12 ? 0x01 : (sect_disk_end < 65536 ? 0x04 : 0x06))); // FAT12/16
			var_write(&bootsec.rootdirentries, (uint16_t)root.size());
			var_write(&bootsec.sectorsperfat, (uint16_t)sectorsPerFat);
			bootsec.bootcode[0] = 0x80; //Physical drive (harddisk) flag
			bootsec.bootcode[2] = 0x29; //Extended boot signature
			var_write((uint32_t *)&bootsec.bootcode[3], serial + 1); //4 byte partition serial number
			memcpy(&bootsec.bootcode[7], "NO NAME    ", 11); // volume label
			memcpy(&bootsec.bootcode[18], "FAT1    ", 8); // file system string name
			bootsec.bootcode[22] = (char)('0' + (fatSz % 10)); // '2' or '6'
		}
		else // FAT32
		{
			var_write(&mbr.pentry[0].parttype, 0x0C); //FAT32
			var_write((uint32_t *)&bootsec.bootcode[0], sectorsPerFat);
			var_write((uint32_t *)&bootsec.bootcode[8], (uint32_t)2); // First cluster number of the root directory
			var_write((uint16_t *)&bootsec.bootcode[12], (uint16_t)1); // Sector of FSInfo structure in offset from top of the FAT32 volume
			var_write((uint16_t *)&bootsec.bootcode[14], (uint16_t)6); // Sector of backup boot sector in offset from top of the FAT32 volume
			bootsec.bootcode[28] = 0x80; //Physical drive (harddisk) flag
			bootsec.bootcode[30] = 0x29; //Extended boot signature
			var_write((uint32_t *)&bootsec.bootcode[31], serial + 1); //4 byte partition serial number
			memcpy(&bootsec.bootcode[35], "NO NAME    ", 11); // volume label
			memcpy(&bootsec.bootcode[46], "FAT32   ", 8); // file system string name

			memset(fsinfosec, 0, sizeof(fsinfosec));
			var_write((uint32_t *)&fsinfosec[0], (uint32_t)0x41615252); //lead signature
			var_write((uint32_t *)&fsinfosec[484], (uint32_t)0x61417272); //Another signature
			bool ver71 = dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10);
			//Bitu freeclusters = readOnly ? 0x0 : (ver71 ? (Bitu)freeSpace / (BYTESPERSECTOR * sectorsPerCluster) : 0xFFFFFFFF);
			Bitu freeclusters = readOnly ? 0x0 : (ver71 ? (Bitu)((sect_disk_end - sect_files_end) / sectorsPerCluster): 0xFFFFFFFF);
			var_write((uint32_t *)&fsinfosec[488], (uint32_t)(freeclusters < 0xFFFFFFFF ? freeclusters : 0xFFFFFFFF)); //last known free cluster count (all FF is unknown)
			var_write((uint32_t *)&fsinfosec[492], (ver71 ? (sect_files_end / sectorsPerCluster): 0xFFFFFFFF)); //the cluster number at which the driver should start looking for free clusters (all FF is unknown)
			var_write((uint32_t *)&fsinfosec[508], 0xAA550000); //ending signature
		}

		codepage = dos.loaded_codepage;
		tryconvcp = tryconvertcp>0;
		success = true;
	}

	static void chs_write(uint8_t* chs, uint32_t lba)
	{
		uint32_t cylinder = lba / (HEADCOUNT * SECTORSPERTRACK);
		uint32_t head = (lba / SECTORSPERTRACK) % HEADCOUNT;
		uint32_t sector = (lba % SECTORSPERTRACK) + 1;
		if (head > 0xFF || sector > 0x3F || cylinder > 0x3FF)
            LOG_MSG("Warning: Invalid CHS data - %X, %X, %X\n", head, sector, cylinder);
		chs[0] = (uint8_t)(head & 0xFF);
		chs[1] = (uint8_t)((sector & 0x3F) | ((cylinder >> 2) & 0xC0));
		chs[2] = (uint8_t)(cylinder & 0xFF);
	}

	Int13Status WriteSector(uint32_t sectnum, const void* data)
	{
		if (sectnum >= sect_disk_end) return Int13Status::SectorNotFound;
		if (sectnum == SECT_MBR)
		{
			// Windows 9x writes the disk timestamp into the booter area on startup.
			// Just copy that part over so it doesn't get treated as a difference that needs to be stored.
			memcpy(mbr.booter, data, sizeof(mbr.booter));
		}

		if (readOnly) return Int13Status::NoError; // just return without error to avoid bluescreens in Windows 9x

		if (sectnum >= diffSectors.size()) diffSectors.resize(sectnum + 128);
		uint32_t *cursor_ptr = &diffSectors[sectnum].cursor, cursor_val = *cursor_ptr;

		int is_different;
		uint8_t filebuf[BYTESPERSECTOR];
		void* unmodified = GetUnmodifiedSector(sectnum, filebuf);
		if (!unmodified)
		{
			is_different = false; // to be equal it must be filled with zeroes
			for (uint64_t* p = (uint64_t*)data, *pEnd = p + (BYTESPERSECTOR / sizeof(uint64_t)); p != pEnd; p++)
				if (*p) { is_different = true; break; }
		}
		else is_different = memcmp(unmodified, data, BYTESPERSECTOR);

		if (is_different)
		{
			if (cursor_val == NULL_CURSOR && diffFreeCursors.size())
			{
				*cursor_ptr = cursor_val = diffFreeCursors.back();
				diffFreeCursors.pop_back();
			}
            if (cursor_val == NULL_CURSOR)
            {
                *cursor_ptr = cursor_val = (uint32_t)diffSectorBufs.size();
                diffSectorBufs.resize(cursor_val + 1);
            }
            memcpy(diffSectorBufs[cursor_val].data, data, BYTESPERSECTOR);
			cacheSectorNumber[sectnum % CACHECOUNT] = (uint32_t)-1; // invalidate cache
		}
		else if (cursor_val != NULL_CURSOR)
		{
			diffFreeCursors.push_back(cursor_val);
			*cursor_ptr = NULL_CURSOR;
			cacheSectorNumber[sectnum % CACHECOUNT] = (uint32_t)-1; // invalidate cache
		}
		return Int13Status::NoError;
	}

	void* GetUnmodifiedSector(uint32_t sectnum, void* filebuf)
	{
		if (sectnum >= sect_files_end) {}
		else if (sectnum >= sect_files_start)
		{
			uint32_t idx = fileAtSector[sectnum - sect_files_start];
			ffddFile& f = files[idx];
			DOS_File* df = NULL;
			for (uint32_t i = 0; i != KEEPOPENCOUNT; i++)
				if (openIndex[i] == idx && openFiles[i])
					{ df = openFiles[i]; break; }
			if (!df)
			{
				openCursor = (openCursor + 1) % KEEPOPENCOUNT;
				DOS_File*& cachedf = openFiles[openCursor];
				if (cachedf)
				{
					cachedf->Close();
					delete cachedf;
					cachedf = NULL;
				}
                bool res = drive->FileOpen(&df, f.path, OPEN_READ);
                if (!res && codepage && (codepage != dos.loaded_codepage || (tryconvcp && codepage == 437))) {
                    uint32_t cp = dos.loaded_codepage;
                    dos.loaded_codepage = codepage;
#if defined(WIN32)
                    if (tryconvcp && dos.loaded_codepage == 437) dos.loaded_codepage = GetACP();
#endif
                    res = drive->FileOpen(&df, f.path, OPEN_READ);
                    dos.loaded_codepage = cp;
                }
				if (res)
				{
					df->AddRef();
					cachedf = df;
					openIndex[openCursor] = idx;
				}
				else return NULL;
			}
			if (df)
			{
				uint32_t pos = (sectnum - f.firstSect) * BYTESPERSECTOR;
				uint16_t read = (uint16_t)BYTESPERSECTOR;
				df->Seek(&pos, DOS_SEEK_SET);
				if (!df->Read((uint8_t*)filebuf, &read)) { read = 0; assert(0); }
				if (read != BYTESPERSECTOR)
					memset((uint8_t*)filebuf + read, 0, BYTESPERSECTOR - read);
				return filebuf;
			}
		}
		else if (sectnum >= sect_dirs_start) return &dirs[(sectnum - sect_dirs_start) * (BYTESPERSECTOR / sizeof(direntry))];
		else if (sectnum >= sect_root_start) return &root[(sectnum - sect_root_start) * (BYTESPERSECTOR / sizeof(direntry))];
		else if (sectnum >= sect_fat2_start) return &fat[(sectnum - sect_fat2_start) * BYTESPERSECTOR];
		else if (sectnum >= sect_fat1_start) return &fat[(sectnum - sect_fat1_start) * BYTESPERSECTOR];
		else if (IS_PC98_ARCH) {
            if (sectnum == 0) return &ipl;
            else if (sectnum == 1) return &pt;
            else if (sectnum == sect_boot_pc98) return &bootsec;
            else if (sectnum == sect_boot_pc98+1) return fsinfosec;
            else if (sectnum == sect_boot_pc98+2) return fsinfosec; // additional boot loader code (anything is ok for us but needs 0x55AA footer signature)
            else if (sectnum == sect_boot_pc98+6) return &bootsec; // boot sector copy
            else if (sectnum == sect_boot_pc98+7) return fsinfosec; // boot sector copy
            else if (sectnum == sect_boot_pc98+8) return fsinfosec; // boot sector copy
            return NULL;
        }
		else if (sectnum == SECT_BOOT) return &bootsec;
		else if (sectnum == SECT_MBR) return &mbr;
		else if (sectnum == SECT_BOOT+1) return fsinfosec;
		else if (sectnum == SECT_BOOT+2) return fsinfosec; // additional boot loader code (anything is ok for us but needs 0x55AA footer signature)
		else if (sectnum == SECT_BOOT+6) return &bootsec; // boot sector copy
		else if (sectnum == SECT_BOOT+7) return fsinfosec; // boot sector copy
		else if (sectnum == SECT_BOOT+8) return fsinfosec; // boot sector copy
		return NULL;
	}

	Int13Status ReadSector(uint32_t sectnum, void* data)
	{
		uint32_t sectorHash = sectnum % CACHECOUNT;
		void *cachedata = cacheSectorData[sectorHash];
		if (cacheSectorNumber[sectorHash] == sectnum)
		{
			memcpy(data, cachedata, BYTESPERSECTOR);
			return Int13Status::NoError;
		}
		cacheSectorNumber[sectorHash] = sectnum;

		void *src;
		uint32_t cursor = (sectnum >= diffSectors.size() ? NULL_CURSOR : diffSectors[sectnum].cursor);
		if (cursor != NULL_CURSOR)
			src = diffSectorBufs[cursor].data;
		else
			src = GetUnmodifiedSector(sectnum, cachedata);

		if (src) memcpy(data, src, BYTESPERSECTOR);
		else memset(data, 0, BYTESPERSECTOR);
		if (src != cachedata) memcpy(cachedata, data, BYTESPERSECTOR);
		return Int13Status::NoError;
	}

    bool SaveImage(const char *name)
    {
        FILE* f = fopen_wrap(name, "wb");
        if (f) {
            uint8_t filebuf[BYTESPERSECTOR];
            if (IS_PC98_ARCH) {
                memcpy(filebuf, &header, BYTESPERSECTOR);
                if (fwrite(filebuf, 1, BYTESPERSECTOR, f) != BYTESPERSECTOR) {fclose(f);return false;}
                memset(filebuf, 0, BYTESPERSECTOR);
                for (int i = 0; i < 7; i++)
                    if (fwrite(filebuf, 1, BYTESPERSECTOR, f) != BYTESPERSECTOR) {fclose(f);return false;}
            }
            for (unsigned int i = 0; i < sect_disk_end; i++) {
                ReadSector(i, filebuf);
                if (fwrite(filebuf, 1, BYTESPERSECTOR, f) != BYTESPERSECTOR) {
                    fclose(f);
                    return false;
                }
            }
            fclose(f);
            return true;
        } else
            return false;
    }
};

bool saveDiskImage(imageDisk *image, const char *name) {
    return image && image->ffdd && image->ffdd->SaveImage(name);
}

diskGeo DiskGeometryList[] = {
    /* KB   S  H  C type SS  root clus ID  fatsz */
    { 160,  8, 1, 40, 0, 512,  64, 1, 0xFE, 1},      // IBM PC double density 5.25" single-sided 160KB
    { 180,  9, 1, 40, 0, 512,  64, 1, 0xFC, 2},      // IBM PC double density 5.25" single-sided 180KB
    { 200, 10, 1, 40, 0, 512,  64, 2, 0xFC, 2},      // DEC Rainbow double density 5.25" single-sided 200KB (I think...)
    { 320,  8, 2, 40, 1, 512, 112, 2, 0xFF, 1},      // IBM PC double density 5.25" double-sided 320KB
    { 360,  9, 2, 40, 1, 512, 112, 2, 0xFD, 2},      // IBM PC double density 5.25" double-sided 360KB
    { 400, 10, 2, 40, 1, 512, 112, 2, 0xFD, 2},      // DEC Rainbow double density 5.25" double-sided 400KB (I think...)
    { 640,  8, 2, 80, 3, 512, 112, 2, 0xFB, 2},      // IBM PC double density 3.5" double-sided 640KB
    { 720,  9, 2, 80, 3, 512, 112, 2, 0xF9, 3},      // IBM PC double density 3.5" double-sided 720KB
    {1200, 15, 2, 80, 2, 512, 224, 1, 0xF9, 7},      // IBM PC double density 5.25" double-sided 1.2MB
    {1440, 18, 2, 80, 4, 512, 224, 1, 0xF0, 9},      // IBM PC high density 3.5" double-sided 1.44MB
    {1680, 21, 2, 80, 4, 512,  16, 4, 0xF0, 3},      // IBM PC high density 3.5" double-sided 1.68MB (DMF)
    {2880, 36, 2, 80, 6, 512, 240, 2, 0xF0, 9},      // IBM PC high density 3.5" double-sided 2.88MB

    {1232,  8, 2, 77, 7, 1024,192, 1, 0xFE, 2},      // NEC PC-98 high density 3.5" double-sided 1.2MB "3-mode"
    {1520, 19, 2, 80, 2, 512, 224, 1, 0xF9, 10},      // IBM PC high density 5.25" double-sided 1.52MB (XDF)
    {1840, 23, 2, 80, 4, 512, 224, 1, 0xF0, 12},      // IBM PC high density 3.5" double-sided 1.84MB (XDF)

    {   0,  0, 0,  0, 0,    0,  0, 0,    0}
};

Bitu call_int13 = 0;
Bitu diskparm0 = 0, diskparm1 = 0;
PhysPt floppyparm0 = 0;/* this is a physical memory address! */
static Int13Status last_status;
static uint8_t last_drive;
uint16_t imgDTASeg;
RealPt imgDTAPtr;
DOS_DTA *imgDTA;
bool killRead;
static bool swapping_requested;

void CMOS_SetRegister(Bitu regNr, uint8_t val); //For setting equipment word

/* 2 floppies and 2 harddrives, max */
bool imageDiskChange[MAX_DISK_IMAGES]={false};
imageDisk *imageDiskList[MAX_DISK_IMAGES]={NULL};
imageDisk *diskSwap[MAX_SWAPPABLE_DISKS]={NULL};
int32_t swapPosition;

imageDisk *GetINT13FloppyDrive(unsigned char drv) {
    if (drv >= 2)
        return NULL;
    return imageDiskList[drv];
}

imageDisk *GetINT13HardDrive(unsigned char drv) {
    if (drv < 0x80 || drv >= (0x80+MAX_DISK_IMAGES-2))
        return NULL;

    return imageDiskList[drv-0x80];
}

void FreeBIOSDiskList() {
    for (int i=0;i < MAX_DISK_IMAGES;i++) {
        if (imageDiskList[i] != NULL) {
            if (i >= 2) IDE_Hard_Disk_Detach(i);
            imageDiskList[i]->Release();
            imageDiskList[i] = NULL;
        }
    }

    for (int j=0;j < MAX_SWAPPABLE_DISKS;j++) {
        if (diskSwap[j] != NULL) {
            diskSwap[j]->Release();
            diskSwap[j] = NULL;
        }
    }
}

//update BIOS disk parameter tables for first two hard drives
void updateDPT(void) {
    uint32_t tmpheads, tmpcyl, tmpsect, tmpsize;
    PhysPt dpphysaddr[2] = { CALLBACK_PhysPointer(diskparm0), CALLBACK_PhysPointer(diskparm1) };
    for (int i = 0; i < 2; i++) {
        tmpheads = 0; tmpcyl = 0; tmpsect = 0; tmpsize = 0;
        if (imageDiskList[i + 2] != NULL) {
            imageDiskList[i + 2]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
        }
        phys_writew(dpphysaddr[i], (uint16_t)tmpcyl);
        phys_writeb(dpphysaddr[i] + 0x2, (uint8_t)tmpheads);
        phys_writew(dpphysaddr[i] + 0x3, 0);
        phys_writew(dpphysaddr[i] + 0x5, tmpcyl == 0 ? 0 : (uint16_t)-1);
        phys_writeb(dpphysaddr[i] + 0x7, 0);
        phys_writeb(dpphysaddr[i] + 0x8, tmpcyl == 0 ? 0 : (0xc0 | ((tmpheads > 8) << 3)));
        phys_writeb(dpphysaddr[i] + 0x9, 0);
        phys_writeb(dpphysaddr[i] + 0xa, 0);
        phys_writeb(dpphysaddr[i] + 0xb, 0);
        phys_writew(dpphysaddr[i] + 0xc, (uint16_t)tmpcyl);
        phys_writeb(dpphysaddr[i] + 0xe, (uint8_t)tmpsect);
    }
}


void updateFloppyDPT(void) {
    if (floppyparm0) {
        LOG(LOG_MISC,LOG_DEBUG)("Updating floppy DPT");
        for (unsigned int fi=0;fi < 2;fi++) {
            PhysPt tp = floppyparm0 + (fi * 11);

            if (imageDiskList[fi]) {
                uint32_t tmpheads, tmpcyl, tmpsect, tmpsize;
                imageDiskList[fi]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);

                /* taken from a QEMU VM */
                phys_writeb(tp+0,0xAF);
                phys_writeb(tp+1,0x02);
                phys_writeb(tp+2,0x25);

                /* sector size as a power of 2 from 128 onward */
                if (tmpsize == 128)
                    phys_writeb(tp+3,0);
                else if (tmpsize == 256)
                    phys_writeb(tp+3,1);
                else if (tmpsize == 512)
                    phys_writeb(tp+3,2);
                else if (tmpsize == 1024)
                    phys_writeb(tp+3,3);
                else if (tmpsize == 2048)
                    phys_writeb(tp+3,4);
                else
                    phys_writeb(tp+3,2);

                phys_writeb(tp+4,tmpsect);/*last sector on track*/
                phys_writeb(tp+5,0x1B);/*Gap length*/
                phys_writeb(tp+6,0xFF);/*Data transfer length max transfer when length not set*/
                phys_writeb(tp+7,0x6C);/*Gap length for format operation*/
                phys_writeb(tp+8,0xF6);/*Fill char for format operation*/
                phys_writeb(tp+9,0x0F);/*Head settle time in millseconds*/
                phys_writeb(tp+10,0x08);/*Motor on startup time in 1/8 second units*/
            }
            else if (fi == 0) {
                /* Present it as a 1.44MB drive */
                uint32_t tmpheads = 2, tmpcyl = 80, tmpsect = 18, tmpsize = 512;

                /* taken from a QEMU VM */
                phys_writeb(tp+0,0xAF);
                phys_writeb(tp+1,0x02);
                phys_writeb(tp+2,0x25);

                /* sector size as a power of 2 from 128 onward */
                if (tmpsize == 128)
                    phys_writeb(tp+3,0);
                else if (tmpsize == 256)
                    phys_writeb(tp+3,1);
                else if (tmpsize == 512)
                    phys_writeb(tp+3,2);
                else if (tmpsize == 1024)
                    phys_writeb(tp+3,3);
                else if (tmpsize == 2048)
                    phys_writeb(tp+3,4);
                else
                    phys_writeb(tp+3,2);

                phys_writeb(tp+4,tmpsect);/*last sector on track*/
                phys_writeb(tp+5,0x1B);/*Gap length*/
                phys_writeb(tp+6,0xFF);/*Data transfer length max transfer when length not set (GUESS)*/
                phys_writeb(tp+7,0x6C);/*Gap length for format operation*/
                phys_writeb(tp+8,0xF6);/*Fill char for format operation*/
                phys_writeb(tp+9,0x0F);/*Head settle time in millseconds*/
                phys_writeb(tp+10,0x88);/*Motor on startup time in 1/8 second units*/
            }
            else {
                for (unsigned int i=0;i < 11;i++)
                    phys_writeb(tp+i,0);
            }
        }
    }
}

void incrementFDD(void) {
    uint16_t equipment=mem_readw(BIOS_CONFIGURATION);
    if(equipment&1) {
        Bitu numofdisks = (equipment>>6)&3;
        numofdisks++;
        if(numofdisks > 1) numofdisks=1;//max 2 floppies at the moment
        equipment&=~0x00C0;
        equipment|=(numofdisks<<6);
    } else equipment|=1;
    mem_writew(BIOS_CONFIGURATION,equipment);
    if(IS_EGAVGA_ARCH) equipment &= ~0x30; //EGA/VGA startup display mode differs in CMOS
    CMOS_SetRegister(0x14, (uint8_t)(equipment&0xff));
}

int swapInDisksSpecificDrive = -1;
// -1 = swap across A: and B: (DOSBox / DOSBox-X default behavior)
//  0 = swap across A: only
//  1 = swap across B: only

void swapInDisks(int drive) {
    bool allNull = true;
    int32_t diskcount = 0;
    Bits diskswapcount = 2;
    Bits diskswapdrive = 0;
    int32_t swapPos = swapPosition;
    int32_t i;

    /* Check to make sure that  there is at least one setup image */
    for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
        if(diskSwap[i]!=NULL) {
            allNull = false;
            break;
        }
    }

    /* No disks setup... fail */
    if (allNull) return;

    /* if a specific drive is to be swapped, then adjust to focus on it */
    if (swapInDisksSpecificDrive >= 0 && swapInDisksSpecificDrive <= 1 && (drive == -1 || drive == swapInDisksSpecificDrive)) {
        diskswapdrive = swapInDisksSpecificDrive;
        diskswapcount = 1;
    } else if (swapInDisksSpecificDrive != -1 || drive != -1) /* Swap A: and B: drives */
        return;

    /* If only one disk is loaded, this loop will load the same disk in dive A and drive B */
    while(diskcount < diskswapcount) {
        if(diskSwap[swapPos] != NULL) {
            LOG_MSG("Loaded drive %d disk %d from swaplist position %d - \"%s\"", (int)diskswapdrive, (int)diskcount, (int)swapPos, diskSwap[swapPos]->diskname.c_str());

            if (imageDiskList[diskswapdrive] != NULL)
                imageDiskList[diskswapdrive]->Release();

            imageDiskList[diskswapdrive] = diskSwap[swapPos];
            imageDiskList[diskswapdrive]->Addref();

            imageDiskChange[diskswapdrive] = true;

            diskcount++;
            diskswapdrive++;
        }

        swapPos++;
        if(swapPos>=MAX_SWAPPABLE_DISKS) swapPos=0;
    }
}

bool getSwapRequest(void) {
    bool sreq=swapping_requested;
    swapping_requested = false;
    return sreq;
}

void swapInDrive(int drive, unsigned int position=0) {
#if 0  /* FIX_ME: Disabled to swap CD by IMGSWAP command (Issue #4932). Revert this if any regression occurs */
    //if (drive>1||swapInDisksSpecificDrive!=drive) return;
#endif
    if (position<1) swapPosition++;
    else swapPosition=position-1;
    if(diskSwap[swapPosition] == NULL) swapPosition = 0;
    swapInDisks(drive);
    swapping_requested = true;
    DriveManager::CycleDisks(drive, true, position);
    /* Hack/feature: rescan all disks as well */
    LOG_MSG("Diskcaching reset for drive %c.", drive+'A');
    if (Drives[drive] != NULL) {
        Drives[drive]->EmptyCache();

        isoDrive *isodrv = dynamic_cast<isoDrive*>(Drives[drive]);

        if (isodrv && !dos_kernel_disabled)
            isodrv->MediaChangeImmediate(); // for IDE, without the media change delay
        else
            Drives[drive]->MediaChange(); // for IDE
    }
}

void swapInNextDisk(bool pressed) {
    if (!pressed)
        return;

    DriveManager::CycleAllDisks();
    /* Hack/feature: rescan all disks as well */
    LOG_MSG("Diskcaching reset for floppy drives.");
    for(Bitu i=0;i<2;i++) { /* Swap A: and B: where DOSBox mainline would run through ALL drive letters */
        if (Drives[i] != NULL) {
            Drives[i]->EmptyCache();
            Drives[i]->MediaChange();
        }
    }
    if (swapInDisksSpecificDrive>1) return;
    swapPosition++;
    if(diskSwap[swapPosition] == NULL) swapPosition = 0;
    swapInDisks(-1);
    swapping_requested = true;
}

void IDE_ATAPI_MediaChangeNotify(signed char index, bool slave, bool immediate);
void IDE_ATAPI_MediaChangeNotifyAll(bool immediate);

void swapInNextCD(bool pressed) {
    if (!pressed)
        return;
    DriveManager::CycleAllCDs();
    /* Hack/feature: rescan all disks as well */
    LOG_MSG("Diskcaching reset for normal mounted drives.");
    for(Bitu i=2;i<DOS_DRIVES;i++) { /* Swap C: D: .... Z: if it is a CD/DVD drive */
        if (Drives[i] != NULL && dynamic_cast<isoDrive*>(Drives[i]) != NULL) {
            Drives[i]->EmptyCache();

            isoDrive *isodrv = dynamic_cast<isoDrive*>(Drives[i]);

            if (isodrv && !dos_kernel_disabled)
                isodrv->MediaChangeImmediate(); // for IDE, without the media change delay
            else
                Drives[i]->MediaChange(); // for IDE
        }
    }

    if (!dos_kernel_disabled)
        IDE_ATAPI_MediaChangeNotifyAll(/*immediate*/true);
    else
        IDE_ATAPI_MediaChangeNotifyAll(/*immediate*/false);
}

Int13Status imageDisk::Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size) {
    uint32_t sectnum;

    if (req_sector_size == 0)
        req_sector_size = sector_size;
    if (req_sector_size != sector_size)
        return Int13Status::SectorNotFound;

    if (sector == 0) {
        LOG_MSG("Attempted to read invalid sector 0.");
        return Int13Status::SectorNotFound;
    }

    sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

    return Read_AbsoluteSector(sectnum, data);
}

Int13Status imageDisk::Read_AbsoluteSector(uint32_t sectnum, void * data) {
	if (ffdd) return ffdd->ReadSector(sectnum, data);

    uint64_t bytenum,res;
    int got;

    bytenum = (uint64_t)sectnum * (uint64_t)sector_size;
    if ((bytenum + sector_size) > this->image_length) {
        LOG_MSG("Attempt to read invalid sector in Read_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
        return Int13Status::SectorNotFound;
    }
    bytenum += image_base;

    //LOG_MSG("Reading sectors %ld at bytenum %I64d", sectnum, bytenum);

    fseeko64(diskimg,(fseek_ofs_t)bytenum,SEEK_SET);
    res = (uint64_t)ftello64(diskimg);
    if (res != bytenum) {
        LOG_MSG("fseek() failed in Read_AbsoluteSector for sector %lu. Want=%llu Got=%llu\n",
            (unsigned long)sectnum,(unsigned long long)bytenum,(unsigned long long)res);
        return Int13Status::SeekFailed;
    }

    got = (int)fread(data, 1, sector_size, diskimg);
    if ((unsigned int)got != sector_size) {
        LOG_MSG("fread() failed in Read_AbsoluteSector for sector %lu. Want=%u got=%d\n",
            (unsigned long)sectnum,sector_size,(unsigned int)got);
        return Int13Status::ControllerFailure;
    }

    return Int13Status::NoError;
}

Int13Status imageDisk::Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size) {
    uint32_t sectnum;

    if (req_sector_size == 0)
        req_sector_size = sector_size;
    if (req_sector_size != sector_size)
        return Int13Status::SectorNotFound;

    sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

    return Write_AbsoluteSector(sectnum, data);
}


Int13Status imageDisk::Write_AbsoluteSector(uint32_t sectnum, const void *data) {
	if (ffdd) return ffdd->WriteSector(sectnum, data);

    uint64_t bytenum;

    bytenum = (uint64_t)sectnum * sector_size;
    if ((bytenum + sector_size) > this->image_length) {
        LOG_MSG("Attempt to read invalid sector in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
        return Int13Status::SectorNotFound;
    }
    bytenum += image_base;

    //LOG_MSG("Writing sectors to %ld at bytenum %d", sectnum, bytenum);

    fseeko64(diskimg,(fseek_ofs_t)bytenum,SEEK_SET);
    if ((uint64_t)ftello64(diskimg) != bytenum)
        LOG_MSG("WARNING: fseek() failed in Write_AbsoluteSector for sector %lu\n",(unsigned long)sectnum);

    size_t ret=fwrite(data, sector_size, 1, diskimg);

    return ((ret>0)?Int13Status::NoError:Int13Status::ControllerFailure);

}

void imageDisk::Set_Reserved_Cylinders(Bitu resCyl) {
    reserved_cylinders = resCyl;
}

uint32_t imageDisk::Get_Reserved_Cylinders() {
    return reserved_cylinders;
}

imageDisk::imageDisk(IMAGE_TYPE class_id) : class_id(class_id) {
}

imageDisk::imageDisk(FILE* diskimg, const char* diskName, uint32_t cylinders, uint32_t heads, uint32_t sectors, uint32_t sector_size, bool hardDrive)
{
    if (diskName) this->diskname = diskName;
    this->cylinders = cylinders;
    this->heads = heads;
    this->sectors = sectors;
    image_base = 0;
    if (image_length == 0) image_length = (uint64_t)cylinders * heads * sectors * sector_size;
    refcount = 0;
    this->sector_size = sector_size;
    this->diskSizeK = this->image_length / 1024;
    LBA = image_length / sector_size;
    if(!int13_enable_48bitLBA && (LBA > 0x0FFFFFFF))
        LOG_MSG("Warning: Disk size (%lf GiB) exceeds 128GiB limit for 28-bit LBA. You may need to enable 48-bit LBA support.", (double)image_length / (1024.0 * 1024 * 1024));
    reserved_cylinders = 0;
    this->diskimg = diskimg;
    class_id = ID_BASE;
    active = true;
    this->hardDrive = hardDrive;
    floppytype = 0;
}


void imageDisk::UpdateFloppyType(void) {
	uint8_t i=0;

	while (DiskGeometryList[i].ksize!=0x0) {
		if ((DiskGeometryList[i].ksize==diskSizeK) || (DiskGeometryList[i].ksize+1==diskSizeK)) {
			floppytype = i;
			LOG_MSG("Updating floppy type to %u BIOS type 0x%02x",floppytype,GetBiosType());
			break;
		}
		i++;
	}
}

imageDisk::imageDisk(FILE* imgFile, const char* imgName, uint64_t imgSize, bool isHardDisk) : diskSizeK(imgSize/1024), diskimg(imgFile), image_length(imgSize) {
    if (imgName != NULL)
        diskname = imgName;
    uint64_t imgSizeK = diskSizeK;
    active = false;
    hardDrive = isHardDisk;
    if(!isHardDisk) {
        bool founddisk = false;

        if (imgName != NULL) {
            const char *ext = strrchr((char*)imgName,'.');
            if (ext != NULL) {
                if (!strcasecmp(ext,".fdi")) {
                    if (imgSizeK >= 160) {
                        FDIHDR fdihdr;

                        // PC-98 .FDI images appear to be 4096 bytes of a short header and many zeros.
                        // followed by a straight sector dump of the disk. The header is NOT NECESSARILY
                        // 4KB in size, but usually is.
                        LOG_MSG("Image file has .FDI extension, assuming FDI image and will take on parameters in header.");

                        assert(sizeof(fdihdr) == 0x20);
                        if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 &&
                            fread(&fdihdr,sizeof(fdihdr),1,imgFile) == 1) {
                            uint32_t ofs = host_readd(fdihdr.headersize);
                            uint32_t fddsize = host_readd(fdihdr.fddsize); /* includes header */
                            uint32_t sectorsize = host_readd(fdihdr.sectorsize);

                            if (sectorsize != 0 && ((sectorsize & (sectorsize - 1)) == 0/*is power of 2*/) &&
                                sectorsize >= 256 && sectorsize <= 1024 &&
                                ofs != 0 && (ofs % sectorsize) == 0/*offset is nonzero and multiple of sector size*/ &&
                                (ofs % 1024) == 0/*offset is a multiple of 1024 because of imgSizeK*/ &&
                                fddsize >= sectorsize && (fddsize/1024) <= (imgSizeK+4)) {

                                founddisk = true;
                                sector_size = sectorsize;
                                imgSizeK -= (ofs / 1024);
                                image_base = ofs;
                                image_length -= ofs;
                                LOG_MSG("FDI header: sectorsize is %u bytes/sector, header is %u bytes, fdd size (plus header) is %u bytes",
                                    sectorsize,ofs,fddsize);

                                /* take on the geometry. */
                                sectors = host_readd(fdihdr.sectors);
                                heads = host_readd(fdihdr.surfaces);
                                cylinders = host_readd(fdihdr.cylinders);
                                LOG_MSG("FDI: Geometry is C/H/S %u/%u/%u",
                                    cylinders,heads,sectors);
                            }
                            else {
                                LOG_MSG("FDI header rejected. sectorsize=%u headersize=%u fddsize=%u",
                                    sectorsize,ofs,fddsize);
                            }
                        }
                        else {
                            LOG_MSG("Unable to read .FDI header");
                        }
                    }
                }
            }
        }

        if (sectors == 0 && heads == 0 && cylinders == 0) {
            uint8_t i=0;
            while (DiskGeometryList[i].ksize!=0x0) {
                if ((DiskGeometryList[i].ksize==imgSizeK) ||
                        (DiskGeometryList[i].ksize+1==imgSizeK)) {
                    if (DiskGeometryList[i].ksize!=imgSizeK)
                        LOG_MSG("ImageLoader: image file with additional data, might not load!");
                    founddisk = true;
                    active = true;
                    floppytype = i;
                    heads = DiskGeometryList[i].headscyl;
                    cylinders = DiskGeometryList[i].cylcount;
                    sectors = DiskGeometryList[i].secttrack;
                    sector_size = DiskGeometryList[i].bytespersect;
                    LOG_MSG("Identified '%s' as C/H/S %u/%u/%u %u bytes/sector",
                            imgName,cylinders,heads,sectors,sector_size);
                    break;
                }
                // Supports cases where the size of a 1.2 Mbytes disk image file is 1.44 Mbytes.
                if(DiskGeometryList[i].ksize == 1200 && (imgSizeK > 1200 && imgSizeK <= 1440)) {
                    char buff[0x20];
                    if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 && fread(buff,sizeof(buff),1,imgFile) == 1) {
                        if(buff[0x18] == DiskGeometryList[i].secttrack) {
                            founddisk = true;
                            active = true;
                            floppytype = i;
                            heads = DiskGeometryList[i].headscyl;
                            cylinders = DiskGeometryList[i].cylcount;
                            sectors = DiskGeometryList[i].secttrack;
                            break;
                        }
                    }
                }
                i++;
            }
        }
        if(!founddisk) {
            active = false;
        }
    }
    else { /* hard disk */
        if (imgName != NULL) {
            char *ext = strrchr((char*)imgName,'.');
            if (ext != NULL) {
                if (!strcasecmp(ext,".nhd")) {
                    if (imgSizeK >= 160) {
                        NHD_FILE_HEAD nhdhdr;

                        LOG_MSG("Image file has .NHD extension, assuming NHD image and will take on parameters in header.");

                        assert(sizeof(nhdhdr) == 0x200);
                        if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 &&
                            fread(&nhdhdr,sizeof(nhdhdr),1,imgFile) == 1 &&
                            host_readd((ConstHostPt)(&nhdhdr.dwHeadSize)) >= 0x200 &&
                            !memcmp(nhdhdr.szFileID,"T98HDDIMAGE.R0\0",15)) {
                            uint32_t ofs = host_readd((ConstHostPt)(&nhdhdr.dwHeadSize));
                            uint32_t sectorsize = host_readw((ConstHostPt)(&nhdhdr.wSectLen));

                            if (sectorsize != 0 && ((sectorsize & (sectorsize - 1)) == 0/*is power of 2*/) &&
                                sectorsize >= 256 && sectorsize <= 1024 &&
                                ofs != 0 && (ofs % sectorsize) == 0/*offset is nonzero and multiple of sector size*/) {

                                sector_size = sectorsize;
                                imgSizeK -= (ofs / 1024);
                                image_base = ofs;
                                image_length -= ofs;
                                LOG_MSG("NHD header: sectorsize is %u bytes/sector, header is %u bytes",
                                        sectorsize,ofs);

                                /* take on the geometry.
                                 * PC-98 IPL1 support will need it to make sense of the partition table. */
                                sectors = host_readw((ConstHostPt)(&nhdhdr.wSect));
                                heads = host_readw((ConstHostPt)(&nhdhdr.wHead));
                                cylinders = host_readd((ConstHostPt)(&nhdhdr.dwCylinder));
                                LOG_MSG("NHD: Geometry is C/H/S %u/%u/%u",
                                        cylinders,heads,sectors);
                            }
                            else {
                                LOG_MSG("NHD header rejected. sectorsize=%u headersize=%u",
                                        sectorsize,ofs);
                            }
                        }
                        else {
                            LOG_MSG("Unable to read .NHD header");
                        }
                    }
                }
                if (!strcasecmp(ext,".hdi")) {
                    if (imgSizeK >= 160) {
                        HDIHDR hdihdr;

                        // PC-98 .HDI images appear to be 4096 bytes of a short header and many zeros.
                        // followed by a straight sector dump of the disk. The header is NOT NECESSARILY
                        // 4KB in size, but usually is.
                        LOG_MSG("Image file has .HDI extension, assuming HDI image and will take on parameters in header.");

                        assert(sizeof(hdihdr) == 0x20);
                        if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 &&
                            fread(&hdihdr,sizeof(hdihdr),1,imgFile) == 1) {
                            uint32_t ofs = host_readd(hdihdr.headersize);
                            uint32_t hddsize = host_readd(hdihdr.hddsize); /* includes header */
                            uint32_t sectorsize = host_readd(hdihdr.sectorsize);

                            if (sectorsize != 0 && ((sectorsize & (sectorsize - 1)) == 0/*is power of 2*/) &&
                                sectorsize >= 256 && sectorsize <= 1024 &&
                                ofs != 0 && (ofs % sectorsize) == 0/*offset is nonzero and multiple of sector size*/ &&
                                (ofs % 1024) == 0/*offset is a multiple of 1024 because of imgSizeK*/ &&
                                hddsize >= sectorsize && (hddsize/1024) <= (imgSizeK+4)) {

                                sector_size = sectorsize;
                                image_base = ofs;
                                image_length -= ofs;
                                LOG_MSG("HDI header: sectorsize is %u bytes/sector, header is %u bytes, hdd size (plus header) is %u bytes",
                                    sectorsize,ofs,hddsize);

                                /* take on the geometry.
                                 * PC-98 IPL1 support will need it to make sense of the partition table. */
                                sectors = host_readd(hdihdr.sectors);
                                heads = host_readd(hdihdr.surfaces);
                                cylinders = host_readd(hdihdr.cylinders);
                                LOG_MSG("HDI: Geometry is C/H/S %u/%u/%u",
                                    cylinders,heads,sectors);
                            }
                            else {
                                LOG_MSG("HDI header rejected. sectorsize=%u headersize=%u hddsize=%u",
                                    sectorsize,ofs,hddsize);
                            }
                        }
                        else {
                            LOG_MSG("Unable to read .HDI header");
                        }
                    }
                }
            }
        }

        LBA = imgSize / sector_size;
        if(!int13_enable_48bitLBA && (LBA > 0x0FFFFFFF))
            LOG_MSG("Warning: Disk size (%lf GiB) exceeds 128GiB limit for 28-bit LBA. You may need to enable 48-bit LBA support.", (double)image_length / (1024.0 * 1024 * 1024));
        if (sectors == 0 || heads == 0 || cylinders == 0)
            active = false;
    }
}

imageDisk::imageDisk(class DOS_Drive *useDrive, unsigned int letter, uint32_t freeMB, int timeout)
{
	ffdd = new fatFromDOSDrive(useDrive, freeMB, timeout);
	if (!ffdd->success) {
		LOG_MSG("FAT conversion failed");
		delete ffdd;
		ffdd = NULL;
		return;
	}
    if (IS_PC98_ARCH) {
        cylinders = ffdd->sasi.cylinders;
        heads = ffdd->sasi.surfaces;
        sectors = ffdd->sasi.sectors;
    }
	drvnum = letter;
	diskimg = NULL;
	diskname[0] = '\0';
	hardDrive = true;
    Set_GeometryForHardDisk();
}

imageDisk::~imageDisk()
{
    if(diskimg != NULL) {
        fclose(diskimg);
        diskimg=NULL;
    }
    if (ffdd)
        delete ffdd;
}

void imageDisk::Set_GeometryForHardDisk()
{
	sector_size = 512;
	partTable mbrData;
	for (int m = ((Read_AbsoluteSector(0, &mbrData) != Int13Status::NoError) ? 0 : 4); m--;)
	{
		if(!mbrData.pentry[m].partSize) continue;
		bootstrap bootbuffer;
		if (Read_AbsoluteSector(mbrData.pentry[m].absSectStart, &bootbuffer) != Int13Status::NoError) continue;
		bootbuffer.sectorspertrack = var_read(&bootbuffer.sectorspertrack);
		bootbuffer.headcount = var_read(&bootbuffer.headcount);
		uint32_t setSect = bootbuffer.sectorspertrack;
		uint32_t setHeads = bootbuffer.headcount;
		uint32_t setCyl = (mbrData.pentry[m].absSectStart + mbrData.pentry[m].partSize) / (setSect * setHeads);
		Set_Geometry(setHeads, setCyl, setSect, 512);
        if(image_length == 0) {
            LBA = (uint64_t)setCyl * setHeads * setSect;
            image_length = (uint64_t)LBA * sector_size;
        }
        return;
	}
	if (!diskimg) return;
    uint32_t diskimgsize;
	fseek(diskimg,0,SEEK_END);
	diskimgsize = (uint32_t)ftell(diskimg);
    fseek(diskimg,current_fpos,SEEK_SET);
    image_length = diskimgsize;
    LBA = (uint64_t)diskimgsize / sector_size;
    Set_Geometry(16, diskimgsize / (512 * 63 * 16), 63, 512);
}

void imageDisk::Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) {

    uint64_t total_sectors = (uint64_t)setHeads * (uint64_t)setCyl * (uint64_t)setSect;
    sectors = 63; // Default to 63 sectors per track.
    heads = 16;   // Default to 16 heads.

    if(((setSectSize & (setSectSize - 1)) != 0)) {
        LOG_MSG("bios_disk: Invalid sector size %u, must be a power of 2.", setSectSize);
        active = false;
        return;
    }

    if(total_sectors == 0) {
        LOG_MSG("bios_disk: Invalid disk geometry C, H, S = %u, %u, %u", setCyl, setHeads, setSect);
        active = false;
        return;
    }
    else if (total_sectors > 1024ULL * 255ULL * 63ULL) {
        LOG_MSG("bios_disk: Disk geometry C, H, S = %u, %u, %u exceeds 8.4GB disk size, setting to max limits", setCyl, setHeads, setSect);
        cylinders = 1024;
        heads = 255;
        sectors = 63;
        sector_size = setSectSize;
        active = true;
        return;
    }

    // Use the provided geometry, if it is valid
    if(setCyl > 0 && setHeads > 0 && setSect > 0 && (setCyl <= 1024 || IS_PC98_ARCH) && setHeads <= 255 && setSect <= 63) {
        cylinders = setCyl;
        heads = setHeads;
        sectors = setSect;
        sector_size = setSectSize;
        active = true;
        return;
    }

    if (IS_PC98_ARCH) {
        /* TODO: Some early PC-98 SCSI BIOS has its own 4096 cylinder (12-bit) limit */
    }

    if(total_sectors < 17ULL * 1024 * 16)
        sectors = 17;
    else if(total_sectors < 32ULL * 1024 * 16)
        sectors = 32;
    else
        sectors = 63;

    while(heads < 255) {
        uint64_t test_cyl =
            total_sectors / ((uint64_t)heads * sectors);
        if(test_cyl <= 1024)
            break;
        heads <<= 1;
        if(heads > 255) {
            heads = 255;
            break;
        }
    }
    cylinders = (uint32_t)(total_sectors / ((uint32_t)heads * sectors));
    sector_size = setSectSize;
    active = true;
}

void imageDisk::Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize) {
    *getHeads = heads;
    *getCyl = cylinders;
    *getSect = sectors;
    *getSectSize = sector_size;
}

uint8_t imageDisk::GetBiosType(void) {
    if(!hardDrive) {
        return (uint8_t)DiskGeometryList[floppytype].biosval;
    } else return 0;
}

uint32_t imageDisk::getSectSize(void) {
    return sector_size;
}

static uint8_t GetDosDriveNumber(uint8_t biosNum) {
    switch(biosNum) {
        case 0x0:
            return 0x0;
        case 0x1:
            return 0x1;
        case 0x80:
            return 0x2;
        case 0x81:
            return 0x3;
        case 0x82:
            return 0x4;
        case 0x83:
            return 0x5;
        default:
            return 0x7f;
    }
}

static bool driveInactive(uint8_t driveNum) {
    if(driveNum>=MAX_DISK_IMAGES) {
        int driveCalledFor = reg_dl & 0x7f;
        LOG(LOG_BIOS,LOG_ERROR)("Disk %d non-existent", driveCalledFor);
        last_status = Int13Status::BadCommand;
        CALLBACK_SCF(true);
        return true;
    }
    if(imageDiskList[driveNum] == NULL) {
        LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", (int)driveNum);
        last_status = Int13Status::BadCommand;
        CALLBACK_SCF(true);
        return true;
    }
    if(!imageDiskList[driveNum]->active) {
        LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", (int)driveNum);
        last_status = Int13Status::BadCommand;
        CALLBACK_SCF(true);
        return true;
    }
    return false;
}

static struct {
    uint8_t sz;
    uint8_t res;
    uint16_t num;
    uint16_t off;
    uint16_t seg;
    uint32_t sector;
} dap;

static void readDAP(uint16_t seg, uint16_t off) {
    dap.sz = real_readb(seg,off++);
    dap.res = real_readb(seg,off++);
    dap.num = real_readw(seg,off); off += 2;
    dap.off = real_readw(seg,off); off += 2;
    dap.seg = real_readw(seg,off); off += 2;

    /* Although sector size is 64-bit, 32-bit 2TB limit should be more than enough */
    dap.sector = real_readd(seg,off); off +=4;

    if (real_readd(seg,off)) {
        LOG(LOG_BIOS,LOG_WARN)("INT13: 64-bit sector addressing not supported");
        dap.num = 0; /* this will cause calling INT 13h code to return an error */
    }
}

void IDE_ResetDiskByBIOS(unsigned char disk);
void IDE_EmuINT13DiskReadByBIOS(unsigned char disk,unsigned int cyl,unsigned int head,unsigned sect);
bool IDE_GetPhysGeometry(unsigned char disk,uint32_t &heads,uint32_t &cyl,uint32_t &sect,uint32_t &size, uint64_t &LBA);
void IDE_EmuINT13DiskReadByBIOS_LBA(unsigned char disk,uint64_t lba);

void diskio_delay(Bits value/*bytes*/, int type = -1);

/* For El Torito "No emulation" INT 13 services */
unsigned char INT13_ElTorito_NoEmuDriveNumber = 0;
CDROM_Interface * INT13_ElTorito_cdrom = NULL;
signed char INT13_ElTorito_IDEInterface = -1; /* (controller * 2) + (is_slave?1:0) */
char INT13_ElTorito_NoEmuCDROMDrive = 0;

bool GetMSCDEXDrive(unsigned char drive_letter, CDROM_Interface **_cdrom);


static Bitu INT13_DiskHandler(void) {
    uint16_t segat, bufptr;
    uint8_t sectbuf[2048/*CD-ROM support*/];
    uint8_t  drivenum;
    Bitu  i,t;
    uint64_t LBA = 0;
    last_drive = reg_dl;
    drivenum = GetDosDriveNumber(reg_dl);
    bool any_images = false;
    for(i = 0;i < MAX_DISK_IMAGES;i++) {
        if(imageDiskList[i]) any_images=true;
    }

    // unconditionally enable the interrupt flag
    CALLBACK_SIF(true);

    /* map out functions 0x40-0x48 if not emulating INT 13h extensions */
    if (!int13_extensions_enable && reg_ah >= 0x40 && reg_ah <= 0x48) {
        LOG_MSG("Warning: Guest is attempting to use INT 13h extensions (AH=0x%02X). Set 'int 13 extensions=1' if you want to enable them.\n",reg_ah);
        reg_ah=0xff;
        CALLBACK_SCF(true);
        return CBRET_NONE;
    }

    //drivenum = 0;
    //LOG_MSG("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);

    // NOTE: the 0xff error code returned in some cases is questionable; 0x01 seems more correct
    switch(reg_ah) {
    case 0x0: /* Reset disk */
        {
            /* if there aren't any diskimages (so only localdrives and virtual drives)
             * always succeed on reset disk. If there are diskimages then and only then
             * do real checks
             */
            if (any_images && driveInactive(drivenum)) {
                /* driveInactive sets carry flag if the specified drive is not available */
                if ((machine==MCH_CGA) || (machine==MCH_AMSTRAD) || (machine==MCH_PCJR) || (machine==MCH_OLIVETTI) || (machine==MCH_3270PC)) {
                    /* those bioses call floppy drive reset for invalid drive values */
                    if (((imageDiskList[0]) && (imageDiskList[0]->active)) || ((imageDiskList[1]) && (imageDiskList[1]->active))) {
                        if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
                        last_status = Int13Status::NoError;
                        CALLBACK_SCF(false);
                    }
                }
                return CBRET_NONE;
            }
            if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
            if (reg_dl >= 0x80) IDE_ResetDiskByBIOS(reg_dl);
            last_status = Int13Status::NoError;
            CALLBACK_SCF(false);
        }
        break;
    case 0x1: /* Get status of last operation */

        if(last_status != Int13Status::NoError) {
            reg_ah = (uint8_t)last_status;
            CALLBACK_SCF(true);
        } else {
            reg_ah = 0x00;
            CALLBACK_SCF(false);
        }
        break;
    case 0x2: /* Read sectors */
        if (reg_al==0) {
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        if (drivenum >= MAX_DISK_IMAGES || imageDiskList[drivenum] == NULL) {
            if (drivenum >= DOS_DRIVES || !Drives[drivenum] || Drives[drivenum]->isRemovable()) {
                reg_ah = 0x01;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            // Inherit the Earth cdrom and Amberstar use it as a disk test
            if (((reg_dl&0x80)==0x80) && (reg_dh==0) && ((reg_cl&0x3f)==1)) {
                if (reg_ch==0) {
                    // write some MBR data into buffer for Amberstar installer
                    real_writeb(SegValue(es),reg_bx+0x1be,0x80); // first partition is active
                    real_writeb(SegValue(es),reg_bx+0x1c2,0x06); // first partition is FAT16B
                }
                reg_ah = 0;
                CALLBACK_SCF(false);
                return CBRET_NONE;
            }
        }
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* INT 13h is limited to 512 bytes/sector (as far as I know).
         * The sector buffer in this function is limited to 512 bytes/sector,
         * so this is also a protection against overrunning the stack if you
         * mount a PC-98 disk image (1024 bytes/sector) and try to read it with INT 13h. */
        if (imageDiskList[drivenum]->sector_size > sizeof(sectbuf)) {
            LOG(LOG_MISC,LOG_DEBUG)("INT 13h: Read failed because disk bytes/sector on drive %c is too large",(char)drivenum+'A');

            imageDiskChange[drivenum] = false;

            reg_ah = 0x80; /* timeout */
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* If the disk changed, the first INT 13h read will signal an error and set AH = 0x06 to indicate disk change */
        if (drivenum < 2 && imageDiskChange[drivenum]) {
            LOG(LOG_MISC,LOG_DEBUG)("INT 13h: Failing first read of drive %c to indicate disk change",(char)drivenum+'A');

            imageDiskChange[drivenum] = false;

            reg_ah = 0x06; /* diskette changed or removed */
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        segat = SegValue(es);
        bufptr = reg_bx;
        for(i=0;i<reg_al;i++) {
            last_status = imageDiskList[drivenum]->Read_Sector((uint32_t)reg_dh, (uint32_t)(reg_ch | ((reg_cl & 0xc0)<< 2)), (uint32_t)((reg_cl & 63)+i), sectbuf);

            if (imageDiskList[drivenum]->class_id == imageDisk::ID_EL_TORITO_FLOPPY)
                diskio_delay(512);
            else if (drivenum < 2)
                diskio_delay(512, 0); // Floppy
            else
                diskio_delay(512);

            /* IDE emulation: simulate change of IDE state that would occur on a real machine after INT 13h */
            IDE_EmuINT13DiskReadByBIOS(reg_dl, (uint32_t)(reg_ch | ((reg_cl & 0xc0)<< 2)), (uint32_t)reg_dh, (uint32_t)((reg_cl & 63)+i));

            if((last_status != Int13Status::NoError) || killRead) {
                LOG_MSG("Error in disk read");
                killRead = false;
                reg_ah = (last_status != Int13Status::NoError) ? (uint8_t)last_status : (uint8_t)Int13Status::SectorNotFound;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            for(t=0;t<512;t++) {
                real_writeb(segat,bufptr,sectbuf[t]);
                bufptr++;
            }
        }
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x3: /* Write sectors */
        
        if(driveInactive(drivenum) || !imageDiskList[drivenum]) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* INT 13h is limited to 512 bytes/sector (as far as I know).
         * The sector buffer in this function is limited to 512 bytes/sector,
         * so this is also a protection against overrunning the stack if you
         * mount a PC-98 disk image (1024 bytes/sector) and try to read it with INT 13h. */
        if (imageDiskList[drivenum]->sector_size > sizeof(sectbuf)) {
            LOG(LOG_MISC,LOG_DEBUG)("INT 13h: Write failed because disk bytes/sector on drive %c is too large",(char)drivenum+'A');

            imageDiskChange[drivenum] = false;

            reg_ah = 0x80; /* timeout */
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        bufptr = reg_bx;
        for(i=0;i<reg_al;i++) {
            for(t=0;t<imageDiskList[drivenum]->getSectSize();t++) {
                sectbuf[t] = real_readb(SegValue(es),bufptr);
                bufptr++;
            }

            if(drivenum < 2)
                diskio_delay(512, 0); // Floppy
            else
                diskio_delay(512);

            last_status = imageDiskList[drivenum]->Write_Sector((uint32_t)reg_dh, (uint32_t)(reg_ch | ((reg_cl & 0xc0) << 2)), (uint32_t)((reg_cl & 63) + i), &sectbuf[0]);
            if(last_status != Int13Status::NoError) {
                reg_ah = (uint8_t)last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
        }
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x04: /* Verify sectors */
        if (reg_al==0) {
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        if(driveInactive(drivenum)) {
            reg_ah = (uint8_t)last_status;
            return CBRET_NONE;
        }

        /* TODO: Finish coding this section */
        /*
        segat = SegValue(es);
        bufptr = reg_bx;
        for(i=0;i<reg_al;i++) {
            last_status = imageDiskList[drivenum]->Read_Sector((uint32_t)reg_dh, (uint32_t)(reg_ch | ((reg_cl & 0xc0)<< 2)), (uint32_t)((reg_cl & 63)+i), sectbuf);
            if(last_status != 0x00) {
                LOG_MSG("Error in disk read");
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            for(t=0;t<512;t++) {
                real_writeb(segat,bufptr,sectbuf[t]);
                bufptr++;
            }
        }*/
        reg_ah = 0x00;
        //Qbix: The following codes don't match my specs. al should be number of sector verified
        //reg_al = 0x10; /* CRC verify failed */
        //reg_al = 0x00; /* CRC verify succeeded */
        CALLBACK_SCF(false);
          
        break;
    case 0x05: /* Format track */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG_MSG("WARNING: Format track ignored\n");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x06: /* Format track set bad sector flags */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG_MSG("WARNING: Format track set bad sector flags ignored (6)\n");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x07: /* Format track set bad sector flags */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG_MSG("WARNING: Format track set bad sector flags ignored (7)\n");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x08: /* Get drive parameters */
        if(driveInactive(drivenum)) {
            if(drivenum == 0) {
                // if no floppy drive 0 mounted, return fixed values for 1.44MB drive
                // This fixes a problem in some DOS applications that occurred when booting from the hard disk without a mounted floppy disk.
                reg_ax = 0x00;
                reg_bl = 4;
                reg_ch = 79;
                reg_cl = 18;
                reg_dh = 1;
                reg_dl = 1;
                last_status = Int13Status::NoError;
                CALLBACK_SCF(false);
                {/*fill in ES:DI*/
                    /* Even though BIOSes document vectors pointed at tables,
                     * return the address in BIOS because Windows 95 apparently
                     * likes to replace INT 0x1E with a pointer to whatever else
                     * it decides, and then complain that the floppy controller
                     * isn't working without ever touching I/O ports. What a jackass. */
                    reg_di = floppyparm0 & 0xFu;
                    CPU_SetSegGeneral(es,floppyparm0 >> 4u);
                }
                return CBRET_NONE;
            }
            last_status = Int13Status::DriveParamFailed;
            reg_ah = (uint8_t)last_status;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        reg_ax = 0x00;
        reg_bl = imageDiskList[drivenum]->GetBiosType();
        uint32_t tmpheads, tmpcyl, tmpsect, tmpsize;
        imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
        if (tmpcyl==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: cylinder count zero!");
        else tmpcyl--;      // cylinder count -> max cylinder
        if (tmpheads==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: head count zero!");
        else tmpheads--;    // head count -> max head

        /* older BIOSes were known to subtract 1 or 2 additional "reserved" cylinders.
         * some code, such as Windows 3.1 WDCTRL, might assume that fact. emulate that here */
        {
            uint32_t reserv = imageDiskList[drivenum]->Get_Reserved_Cylinders();
            if (tmpcyl > reserv) tmpcyl -= reserv;
            else tmpcyl = 0;
        }

        reg_ch = (uint8_t)(tmpcyl & 0xff);
        reg_cl = (uint8_t)(((tmpcyl >> 2) & 0xc0) | (tmpsect & 0x3f)); 
        reg_dh = (uint8_t)tmpheads;
        last_status = Int13Status::NoError;
        if (reg_dl&0x80) {  // harddisks
            /* do NOT fill in ES:DI for hard disks, it causes Windows 95 to crash at startup! */
            reg_dl = 0;
            for (int index = 2; index < MAX_DISK_IMAGES; index++) {
                if (imageDiskList[index] != NULL) reg_dl++;
            }
        } else {        // floppy disks
            {/*fill in ES:DI*/
                /* Even though BIOSes document vectors pointed at tables,
                 * return the address in BIOS because Windows 95 apparently
                 * likes to replace INT 0x1E with a pointer to whatever else
                 * it decides, and then complain that the floppy controller
                 * isn't working without ever touching I/O ports. What a jackass. */
                if (reg_dl < 2) {
                    reg_di = (floppyparm0 & 0xFu) + (reg_dl * 11u);
                    CPU_SetSegGeneral(es,floppyparm0 >> 4u);
                }
                else {
                    reg_di = 0;
                    CPU_SetSegGeneral(es,0);
                }
            }
            reg_dl = 0;
            if(imageDiskList[0] != NULL) reg_dl++;
            if(imageDiskList[1] != NULL) reg_dl++;
        }
        CALLBACK_SCF(false);
        break;
    case 0x11: /* Recalibrate drive */
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x15: /* Get disk type */
        /* Korean Powerdolls uses this to detect harddrives */
        LOG(LOG_BIOS,LOG_WARN)("INT13: Get disktype used!");
        if (any_images) {
            if(driveInactive(drivenum)) {
                last_status = Int13Status::DriveParamFailed;
                reg_ah = (uint8_t)last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
            uint64_t largesize = tmpheads*tmpcyl*tmpsect*tmpsize;
            largesize/=512;
            uint32_t ts = static_cast<uint32_t>(largesize);
            reg_ah = (drivenum < 2)?(int13_disk_change_detect_enable?2:1):3; //With 2 for floppy MSDOS starts calling int 13 ah 16
            if(reg_ah == 3) {
                reg_cx = static_cast<uint16_t>(ts >>16);
                reg_dx = static_cast<uint16_t>(ts & 0xffff);
            }
            CALLBACK_SCF(false);
        } else {
            if (drivenum <DOS_DRIVES && (Drives[drivenum] || drivenum <2)) {
                if (drivenum <2) {
                    //TODO use actual size (using 1.44 for now).
                    reg_ah = (int13_disk_change_detect_enable?2:1); // type
//                  reg_cx = 0;
//                  reg_dx = 2880; //Only set size for harddrives.
                } else {
                    //TODO use actual size (using 105 mb for now).
                    reg_ah = 0x3; // type
                    reg_cx = 3;
                    reg_dx = 0x4800;
                }
                CALLBACK_SCF(false);
            } else { 
                LOG(LOG_BIOS,LOG_WARN)("INT13: no images, but invalid drive for call 15");
                reg_ah=0xff;
                CALLBACK_SCF(true);
            }
        }
        break;
    case 0x16: /* Detect disk change (apparently added to XT BIOSes in 1986 according to RBIL) */
	if (int13_disk_change_detect_enable) {
		LOG(LOG_BIOS,LOG_WARN)("INT 13: Detect disk change");
		if(driveInactive(drivenum)) {
			last_status = Int13Status::Timeout;
			reg_ah = (uint8_t)last_status;
			CALLBACK_SCF(true);
		}
		else if (drivenum < 2) {
			if (imageDiskChange[drivenum]) {
				imageDiskChange[drivenum] = false;
				last_status = Int13Status::DiskChanged; // change line active
				reg_ah = (uint8_t)last_status;
				CALLBACK_SCF(true);
			}
			else {
				last_status = Int13Status::NoError; // no change
				reg_ah = (uint8_t)last_status;
				CALLBACK_SCF(false);
			}
		}
		else {
			last_status = Int13Status::DiskChanged; // not supported (because it's a hard drive)
			reg_ah = (uint8_t)last_status;
			CALLBACK_SCF(true);
		}
	}
	else {
		reg_ah=0xff; // not supported
		CALLBACK_SCF(true);
	}
	break;
    case 0x17: /* Set disk type for format */
        /* Pirates! needs this to load */
        killRead = true;
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x41: /* Check Extensions Present */
        if ((reg_bx == 0x55aa) && !(driveInactive(drivenum))) {
            LOG_MSG("INT13: Check Extensions Present for drive: 0x%x", reg_dl);
            reg_ah=0x1; /* 1.x extension supported */
            reg_bx=0xaa55;  /* Extensions installed */
            reg_cx=0x1; /* Extended disk access functions (AH=42h-44h,47h,48h) supported */
            CALLBACK_SCF(false);
            break;
        }
        LOG_MSG("INT13: AH=41h, Function not supported 0x%x for drive: 0x%x", reg_bx, reg_dl);
        CALLBACK_SCF(true);
        break;
    case 0x42: /* Extended Read Sectors From Drive */
        /* Read Disk Address Packet */
        readDAP(SegValue(ds),reg_si);

        if (dap.num==0) {
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        if (INT13_ElTorito_NoEmuDriveNumber != 0 && INT13_ElTorito_NoEmuDriveNumber == reg_dl) {
		if (!INT13_ElTorito_cdrom) {
                        reg_ah = 0x01;
                        CALLBACK_SCF(true);
                        return CBRET_NONE;
                }

                CDROM_Interface *src_drive = INT13_ElTorito_cdrom;

                segat = dap.seg;
                bufptr = dap.off;
                for(i=0;i<dap.num;i++) {
                        static_assert( sizeof(sectbuf) >= 2048, "not big enough" );
                        diskio_delay(512);
                        if (killRead || !src_drive->ReadSectorsHost(sectbuf, false, dap.sector+i, 1)) {
                                real_writew(SegValue(ds),reg_si+2,i); // According to RBIL this should update the number of blocks field to what was successfully transferred
                                LOG_MSG("Error in CDROM read");
                                killRead = false;
                                reg_ah = 0x04;
                                CALLBACK_SCF(true);
                                src_drive->Release();
                                return CBRET_NONE;
                        }

                        for(t=0;t<2048;t++) {
                                real_writeb(segat,bufptr,sectbuf[t]);
                                bufptr++;
                        }
                }
                reg_ah = 0x00;
                CALLBACK_SCF(false);
                return CBRET_NONE;
        }

        if (!any_images) {
            // Inherit the Earth cdrom (uses it as disk test)
            if (((reg_dl&0x80)==0x80) && (reg_dh==0) && ((reg_cl&0x3f)==1)) {
                reg_ah = 0;
                CALLBACK_SCF(false);
                return CBRET_NONE;
            }
        }
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        segat = dap.seg;
        bufptr = dap.off;
        for(i=0;i<dap.num;i++) {
            last_status = imageDiskList[drivenum]->Read_AbsoluteSector(dap.sector+i, sectbuf);

            if(drivenum < 2)
                diskio_delay(512, 0); // Floppy
            else
                diskio_delay(512);

            IDE_EmuINT13DiskReadByBIOS_LBA(reg_dl,dap.sector+i);

            if((last_status != Int13Status::NoError) || killRead) {
                real_writew(SegValue(ds),reg_si+2,i); // According to RBIL this should update the number of blocks field to what was successfully transferred
                LOG_MSG("Error in disk read");
                killRead = false;
                reg_ah = (last_status != Int13Status::NoError) ? (uint8_t)last_status : (uint8_t)Int13Status::SectorNotFound;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            for(t=0;t<512;t++) {
                real_writeb(segat,bufptr,sectbuf[t]);
                bufptr++;
            }
        }
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x43: /* Extended Write Sectors to Drive */
        if(driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* Read Disk Address Packet */
        readDAP(SegValue(ds),reg_si);

        if (dap.num==0) {
            reg_ah = 0x01;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        bufptr = dap.off;
        for(i=0;i<dap.num;i++) {
            for(t=0;t<imageDiskList[drivenum]->getSectSize();t++) {
                sectbuf[t] = real_readb(dap.seg,bufptr);
                bufptr++;
            }

            if(drivenum < 2)
                diskio_delay(512, 0); // Floppy
            else
                diskio_delay(512);

            last_status = imageDiskList[drivenum]->Write_AbsoluteSector(dap.sector+i, &sectbuf[0]);
            if(last_status != Int13Status::NoError) {
                reg_ah = (uint8_t)last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
        }
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x44: /* Extended Verify Sectors [http://www.ctyme.com/intr/rb-0711.htm] */
        if(driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        /* Just signal success, we don't actually verify anything */
        reg_ah = 0x00;
        CALLBACK_SCF(false);
        break;
    case 0x48: { /* get drive parameters */
        uint16_t bufsz;

        if(driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        segat = SegValue(ds);
        bufptr = reg_si;
        bufsz = real_readw(segat,bufptr+0);
        if (bufsz < 0x1A) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        if (bufsz > 0x1E) bufsz = 0x1E;
        else bufsz = 0x1A;

        tmpheads = tmpcyl = tmpsect = tmpsize = 0;
        if(!IDE_GetPhysGeometry(drivenum, tmpheads, tmpcyl, tmpsect, tmpsize, LBA)) {
            imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
            LBA = imageDiskList[drivenum]->getLBA();
        }
        real_writew(segat,bufptr+0x00,bufsz);
        real_writew(segat,bufptr+0x02,0x0003);  /* C/H/S valid, DMA boundary errors handled */
        real_writed(segat,bufptr+0x04,tmpcyl);
        real_writed(segat,bufptr+0x08,tmpheads);
        real_writed(segat,bufptr+0x0C,tmpsect);
        real_writed(segat,bufptr+0x10, int13_enable_48bitLBA?(uint32_t)(LBA & 0xFFFFFFFF): (uint32_t)(LBA > 0x0FFFFFFF?0x0FFFFFFF:LBA)); /* LBA lower 32bit */
        real_writed(segat,bufptr+0x14, int13_enable_48bitLBA?(uint32_t)(LBA >> 32):0); /* LBA upper 32bit */
        real_writew(segat,bufptr+0x18,512);
        if (bufsz >= 0x1E)
            real_writed(segat,bufptr+0x1A,0xFFFFFFFF); /* no EDD information available */

        reg_ah = 0x00;
        CALLBACK_SCF(false);
        } break;
    case 0x4B: /* Terminate disk emulation or get emulation status */
        /* NTS: Windows XP CD-ROM boot requires this call to work or else setup cannot find its own files. */
        if (reg_dl == 0x7F || (INT13_ElTorito_NoEmuDriveNumber != 0 && INT13_ElTorito_NoEmuDriveNumber == reg_dl)) {
            if (reg_al <= 1) {
                PhysPt p = (SegValue(ds) << 4) + reg_si;
                phys_writeb(p + 0x00,0x13);
                phys_writeb(p + 0x01,(0/*no emulation*/) + ((INT13_ElTorito_IDEInterface >= 0) ? 0x40 : 0));
                phys_writeb(p + 0x02,INT13_ElTorito_NoEmuDriveNumber);
                if (INT13_ElTorito_IDEInterface >= 0) {
                        phys_writeb(p + 0x03,(unsigned char)(INT13_ElTorito_IDEInterface >> 1)); /* which IDE controller */
                        phys_writew(p + 0x08,INT13_ElTorito_IDEInterface & 1);/* bit 0: IDE master/slave */
                }
                else {
                        phys_writeb(p + 0x03,0);
                        phys_writew(p + 0x08,0);
                }
                phys_writed(p + 0x04,0);
                phys_writew(p + 0x0A,0);
                phys_writew(p + 0x0C,0);
                phys_writew(p + 0x0E,0);
                phys_writeb(p + 0x10,0);
                phys_writeb(p + 0x11,0);
                phys_writeb(p + 0x12,0);
                reg_ah = 0x00;
                CALLBACK_SCF(false);
                break;
            }
            else {
                reg_ah=0xff;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
        }
        else {
            reg_ah=0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        break;
    default:
        LOG(LOG_BIOS,LOG_ERROR)("INT13: Function %x called on drive %x (dos drive %d)", (int)reg_ah, (int)reg_dl, (int)drivenum);
        reg_ah=0xff;
        CALLBACK_SCF(true);
    }
    return CBRET_NONE;
}

void CALLBACK_DeAllocate(Bitu in);

void BIOS_UnsetupDisks(void) {
    if (call_int13 != 0) {
        CALLBACK_DeAllocate(call_int13);
        RealSetVec(0x13,0); /* zero INT 13h for now */
        call_int13 = 0;
    }
    if (diskparm0 != 0) {
        CALLBACK_DeAllocate(diskparm0);
        diskparm0 = 0;
    }
    if (diskparm1 != 0) {
        CALLBACK_DeAllocate(diskparm1);
        diskparm1 = 0;
    }
}

void BIOS_SetupDisks(void) {
    unsigned int i;

    if (IS_PC98_ARCH) {
        // TODO
        return;
    }

/* TODO Start the time correctly */
    call_int13=CALLBACK_Allocate(); 
    CALLBACK_Setup(call_int13,&INT13_DiskHandler,CB_INT13,"Int 13 Bios disk");
    RealSetVec(0x13,CALLBACK_RealPointer(call_int13));

    //release the drives after a soft reset
    if ((!bootguest&&(bootvm||!use_quick_reboot))||bootdrive<0) FreeBIOSDiskList();

    /* FIXME: Um... these aren't callbacks. Why are they allocated as callbacks? We have ROM general allocation now. */
    diskparm0 = CALLBACK_Allocate();
    CALLBACK_SetDescription(diskparm0,"BIOS Disk 0 parameter table");
    diskparm1 = CALLBACK_Allocate();
    CALLBACK_SetDescription(diskparm1,"BIOS Disk 1 parameter table");
    swapPosition = 0;

    RealSetVec(0x41,CALLBACK_RealPointer(diskparm0));
    RealSetVec(0x46,CALLBACK_RealPointer(diskparm1));

    PhysPt dp0physaddr=CALLBACK_PhysPointer(diskparm0);
    PhysPt dp1physaddr=CALLBACK_PhysPointer(diskparm1);
    for(i=0;i<16;i++) {
        phys_writeb(dp0physaddr+i,0);
        phys_writeb(dp1physaddr+i,0);
    }

    imgDTASeg = 0;

    floppyparm0 = ROMBIOS_GetMemory(11*2/*two tables*/,"BIOS Floppy parameter tables",1,0);
    RealSetVec(0x1E,RealMake(floppyparm0 >> 4u,floppyparm0 & 0xFu));
    updateFloppyDPT();
/* Setup the Bios Area */
    mem_writeb(BIOS_HARDDISK_COUNT,2);

    killRead = false;
    swapping_requested = false;
}

bool PartitionLoadMBR(std::vector<partTable::partentry_t> &parts,imageDisk *loadedDisk) {
	partTable smbr;

	parts.clear();

	if (loadedDisk->getSectSize() > sizeof(smbr)) return false;
	if (loadedDisk->Read_Sector(0,0,1,&smbr) != Int13Status::NoError) return false;
	if (smbr.magic1 != 0x55 || smbr.magic2 != 0xaa) return false; /* Must have signature */

	/* first copy the main partition table entries */
	for (size_t i=0;i < 4;i++)
		parts.push_back(smbr.pentry[i]);

	/* then, enumerate extended partitions and add the partitions within, doing it in a way that
	 * allows recursive extended partitions */
	{
		size_t i=0;

		do {
			if (parts[i].parttype == 0x05/*extended*/ || parts[i].parttype == 0x0F/*LBA extended*/) {
				unsigned long sect = parts[i].absSectStart;
				unsigned long send = sect + parts[i].partSize;

				/* partitions within an extended partition form a sort of linked list.
				 * first entry is the partition, sector start relative to parent partition.
				 * second entry points to next link in the list. */

				/* parts[i] is the parent partition in this loop.
				 * this loop will add to the parts vector, the parent
				 * loop will continue enumerating through the vector
				 * until all have processed. in this way, extended
				 * partitions will be expanded into the sub partitions
				 * until none is left to do. */

				/* FIXME: Extended partitions within extended partitions not yet tested,
				 *        MS-DOS FDISK.EXE won't generate that. */

				while (sect < send) {
					smbr.magic1 = smbr.magic2 = 0;
					loadedDisk->Read_AbsoluteSector(sect,&smbr);

					if (smbr.magic1 != 0x55 || smbr.magic2 != 0xAA)
						break;
					if (smbr.pentry[0].absSectStart == 0 || smbr.pentry[0].partSize == 0)
						break; // FIXME: Not sure what MS-DOS considers the end of the linked list

					const size_t idx = parts.size();

					/* Right, get this: absolute start sector in entry #0 is relative to this link in the linked list.
					 * The pointer to the next link in the linked list is relative to the parent partition. Blegh. */
					smbr.pentry[0].absSectStart += sect;
					if (smbr.pentry[1].absSectStart != 0)
						smbr.pentry[1].absSectStart += parts[i].absSectStart;

					/* if the partition extends past the parent, then stop */
					if ((smbr.pentry[0].absSectStart+smbr.pentry[0].partSize) > (parts[i].absSectStart+parts[i].partSize))
						break;

					parts.push_back(smbr.pentry[0]);

					/* Based on MS-DOS 5.0, the 2nd entry is a link to the next entry, but only if
					 * start sector is nonzero and type is 0x05/0x0F. I'm not certain if MS-DOS allows
					 * the linked list to go either direction, but for the sake of preventing infinite
					 * loops stop if the link points to a lower sector number. */
					if (idx < 256 && (smbr.pentry[1].parttype == 0x05 || smbr.pentry[1].parttype == 0x0F) &&
						smbr.pentry[1].absSectStart != 0 && smbr.pentry[1].absSectStart > sect) {
						sect = smbr.pentry[1].absSectStart;
					}
					else {
						break;
					}
				}
			}
			i++;
		} while (i < parts.size());
	}

	return true;
}

bool PartitionLoadIPL1(std::vector<_PC98RawPartition> &parts,imageDisk *loadedDisk) {
	unsigned char ipltable[SECTOR_SIZE_MAX];

	parts.clear();

	assert(sizeof(_PC98RawPartition) == 32);
	if (loadedDisk->getSectSize() > sizeof(ipltable)) return false;

	memset(ipltable,0,sizeof(ipltable));
	if (loadedDisk->Read_Sector(0,0,2,ipltable) != Int13Status::NoError) return false;

	const unsigned int max_entries = std::min(16UL,(unsigned long)(loadedDisk->getSectSize() / sizeof(_PC98RawPartition)));

	for (size_t i=0;i < max_entries;i++) {
		const _PC98RawPartition *pe = (_PC98RawPartition*)(ipltable+(i * 32));

		if (pe->mid == 0 && pe->sid == 0 &&
			pe->ipl_sect == 0 && pe->ipl_head == 0 && pe->ipl_cyl == 0 &&
			pe->sector == 0 && pe->head == 0 && pe->cyl == 0 &&
			pe->end_sector == 0 && pe->end_head == 0 && pe->end_cyl == 0)
			continue; /* unused */

		parts.push_back(*pe);
	}

	return true;
}

std::string PartitionIdentifyType(imageDisk *loadedDisk) {
	struct partTable mbrData;

	if (loadedDisk->getSectSize() > sizeof(mbrData))
		return std::string(); /* That would cause a buffer overrun */

	if (loadedDisk->Read_Sector(0,0,1,&mbrData) != Int13Status::NoError)
		return std::string();

	if (!memcmp(mbrData.booter+4,"IPL1",4))
		return "IPL1"; /* PC-98 IPL1 */

	if (mbrData.magic1 == 0x55 && mbrData.magic2 == 0xaa)
		return "MBR"; /* IBM PC MBR */

	return std::string();
}

void LogPrintPartitionTable(const std::vector<_PC98RawPartition> &parts) {
	for (size_t i=0;i < parts.size();i++) {
		const _PC98RawPartition &part = parts[i];

		LOG(LOG_DOSMISC,LOG_DEBUG)("IPL #%u: boot=%u active=%u startchs=%u/%u/%u endchs=%u/%u/%u '%s'",
        (unsigned int)i,(part.mid&0x80)?1:0,(part.sid&0x80)?1:0,
			part.cyl,part.head,part.sector,part.end_cyl,part.end_head,part.end_sector,
			std::string((char*)part.name,sizeof(part.name)).c_str());
	}
}

void LogPrintPartitionTable(const std::vector<partTable::partentry_t> &parts) {
	for (size_t i=0;i < parts.size();i++) {
		const partTable::partentry_t &part = parts[i];

		LOG(LOG_DOSMISC,LOG_DEBUG)("MBR #%u: bootflag=%u parttype=0x%02x beginchs=0x%02x%02x%02x endchs=0x%02x%02x%02x start=%llu size=%llu",
			(unsigned int)i,(part.bootflag&0x80)?1:0,part.parttype,
			part.beginchs[0],part.beginchs[1],part.beginchs[2],
			part.endchs[0],part.endchs[1],part.endchs[2],
			(unsigned long long)part.absSectStart,
			(unsigned long long)part.partSize);
	}
}

