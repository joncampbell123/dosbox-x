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
/* VIM ref for tabbing:
 *
 * set tabstop=8 | set softtabstop=8 | set shiftwidth=8 | set expandtab
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"
#include "bios.h"
#include "bios_disk.h"
#include "qcow2_disk.h"
#include "bitop.h"
#include "callback.h"
#include "regs.h"

#include <algorithm>

#define IMGTYPE_FLOPPY          0
#define IMGTYPE_ISO             1
#define IMGTYPE_HDD             2

#define FAT12                   0
#define FAT16                   1
#define FAT32                   2

static uint16_t dpos[256];
static uint32_t dnum[256];
extern bool wpcolon, force_sfn;
extern int lfn_filefind_handle, fat32setver;
extern void dos_ver_menu(bool start);
extern char * DBCS_upcase(char * str), sfn[DOS_NAMELENGTH_ASCII];
extern bool gbk, isDBCSCP(), isKanji1_gbk(uint8_t chr), shiftjis_lead_byte(int c);
extern bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
extern bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);
extern bool wild_match(const char* haystack, char* needle);
bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);

int PC98AutoChoose_FAT(const std::vector<_PC98RawPartition> &parts,imageDisk *loadedDisk) {
        for (size_t i=0;i < parts.size();i++) {
                const _PC98RawPartition &pe = parts[i];
                /* skip partitions already in use */
                if (loadedDisk->partitionInUse(i)) continue;
                uint8_t syss = parts[i].sid;

                /* We're looking for MS-DOS partitions.
                 * I've heard that some other OSes were once ported to PC-98, including Windows NT and OS/2,
                 * so I would rather not mistake NTFS or HPFS as FAT and cause damage. --J.C.
                 * FIXME: Is there a better way? */
                if(!strncasecmp(pe.name, "MS-DOS", 6) ||
                    !strncasecmp(pe.name, "MSDOS", 5) ||
                    !strncasecmp(pe.name, "Windows", 7) ||
                    ((parts[i].mid == 0x20 || (parts[i].mid >= 0xa1 && parts[i].mid <= 0xaf)) // bootable or non-bootable MS-DOS partition
                        && (syss == 0x81 || syss == 0x91 || syss == 0xa1 || syss == 0xe1) // is an active MS-DOS partition
                        && (parts[i].end_cyl <= loadedDisk->cylinders))) {   // end_cyl is a valid value 
                    return (int)i;
                }
        }

        return -1;
}

int MBRAutoChoose_FAT(const std::vector<partTable::partentry_t> &parts,imageDisk *loadedDisk,uint8_t use_ver_maj=0,uint8_t use_ver_min=0) {
        uint16_t n=1;
        const char *msg;
        bool prompt1 = false, prompt2 = false;
        if (use_ver_maj == 0 && use_ver_min == 0) {
                use_ver_maj = dos.version.major;
                use_ver_min = dos.version.minor;
                prompt1 = prompt2 = true;
        }

        for (size_t i=0;i < parts.size();i++) {
                const partTable::partentry_t &pe = parts[i];

                /* skip partitions already in use */
                if (loadedDisk->partitionInUse(i)) continue;

                if (    pe.parttype == 0x01/*FAT12*/ ||
                        pe.parttype == 0x04/*FAT16*/ ||
                        pe.parttype == 0x06/*FAT16*/) {
                        return (int)i;
                }
                else if (pe.parttype == 0x0E/*FAT16B LBA*/) {
                        if (use_ver_maj < 7 && prompt1) {
                            if (fat32setver == 1 || (fat32setver == -1 && systemmessagebox("Mounting LBA disk image","Mounting this type of disk images requires a reported DOS version of 7.0 or higher. Do you want to auto-change the reported DOS version to 7.0 now and mount the disk image?","yesno", "question", 1))) {
                                use_ver_maj = dos.version.major = 7;
                                use_ver_min = dos.version.minor = 0;
                                dos_ver_menu(false);
                            } else {
                                msg = "LBA disk images are supported but require a higher reported DOS version.\r\n";
                                n = (uint16_t)strlen(msg);
                                DOS_WriteFile (STDOUT,(uint8_t *)msg, &n);
                                msg = "Please set the reported DOS version to at least 7.0 to mount this disk image.\r\n";
                                n = (uint16_t)strlen(msg);
                                DOS_WriteFile (STDOUT,(uint8_t *)msg, &n);
                            }
                        }
                        if (use_ver_maj >= 7) /* MS-DOS 7.0 or higher */
                                return (int)i;
                        else
                                prompt1 = false;
                }
                else if (pe.parttype == 0x0B || pe.parttype == 0x0C) { /* FAT32 types */
                        if ((use_ver_maj < 7 || (use_ver_maj == 7 && use_ver_min < 10)) && prompt2) {
                            if (fat32setver == 1 || (fat32setver == -1 && systemmessagebox("Mounting FAT32 disk image","Mounting this type of disk images requires a reported DOS version of 7.10 or higher. Do you want to auto-change the reported DOS version to 7.10 now and mount the disk image?","yesno", "question", 1))) {
                                use_ver_maj = dos.version.major = 7;
                                use_ver_min = dos.version.minor = 10;
                                dos_ver_menu(true);
                            } else {
                                msg = "FAT32 disk images are supported but require a higher reported DOS version.\r\n";
                                n = (uint16_t)strlen(msg);
                                DOS_WriteFile (STDOUT,(uint8_t *)msg, &n);
                                msg = "Please set the reported DOS version to at least 7.10 to mount this disk image.\r\n";
                                n = (uint16_t)strlen(msg);
                                DOS_WriteFile (STDOUT,(uint8_t *)msg, &n);
                            }
                        }
                        if (use_ver_maj > 7 || (use_ver_maj == 7 && use_ver_min >= 10)) /* MS-DOS 7.10 or higher */
                                return (int)i;
                        else
                                prompt2 = false;
                }
        }

        return -1;
}

bool filename_not_8x3(const char *n) {
        bool lead;
        unsigned int i;

        i = 0;
        lead = false;
        while (*n != 0) {
                if (*n == '.') break;
                if ((*n&0xFF)<=32||*n==127||*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||((*n=='['||*n==']'||*n=='|'||*n=='\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*n=='?'||*n=='*') return true;
                if (lead) lead = false;
                else if ((IS_PC98_ARCH && shiftjis_lead_byte(*n&0xFF)) || (isDBCSCP() && isKanji1_gbk(*n&0xFF))) lead = true;
                i++;
                n++;
        }
        if (!i || i > 8) return true;
        if (*n == 0) return false; /* made it past 8 or less normal chars and end of string: normal */

        /* skip dot */
        assert(*n == '.');
        n++;

        i = 0;
        lead = false;
        while (*n != 0) {
                if (*n == '.') return true; /* another '.' means LFN */
                if ((*n&0xFF)<=32||*n==127||*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||((*n=='['||*n==']'||*n=='|'||*n=='\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*n=='?'||*n=='*') return true;
                if (lead) lead = false;
                else if ((IS_PC98_ARCH && shiftjis_lead_byte(*n&0xFF)) || (isDBCSCP() && isKanji1_gbk(*n&0xFF))) lead = true;
                i++;
                n++;
        }
        if (i > 3) return true;

        return false; /* it is 8.3 case */
}

/* Assuming an LFN call, if the name is not strict 8.3 uppercase, return true.
 * If the name is strict 8.3 uppercase like "FILENAME.TXT" there is no point making an LFN because it is a waste of space */
bool filename_not_strict_8x3(const char *n) {
        if (filename_not_8x3(n)) return true;
        bool lead = false;
        for (unsigned int i=0; i<strlen(n); i++) {
                if (lead) lead = false;
                else if ((IS_PC98_ARCH && shiftjis_lead_byte(n[i]&0xFF)) || (isDBCSCP() && isKanji1_gbk(n[i]&0xFF))) lead = true;
                else if (n[i]>='a' && n[i]<='z') return true;
        }
        return false; /* it is strict 8.3 upper case */
}

void GenerateSFN(char *lfn, unsigned int k, unsigned int &i, unsigned int &t);
/* Generate 8.3 names from LFNs, with tilde usage (from ~1 to ~999999). */
char* fatDrive::Generate_SFN(const char *path, const char *name) {
        if (!filename_not_8x3(name)) {
                strcpy(sfn, name);
                DBCS_upcase(sfn);
                return sfn;
        }
        char lfn[LFN_NAMELENGTH+1], fullname[DOS_PATHLENGTH+DOS_NAMELENGTH_ASCII];
        if (name==NULL||!*name) return NULL;
        if (strlen(name)>LFN_NAMELENGTH) {
                strncpy(lfn, name, LFN_NAMELENGTH);
                lfn[LFN_NAMELENGTH]=0;
        } else
                strcpy(lfn, name);
        if (!strlen(lfn)) return NULL;
        direntry fileEntry = {};
        uint32_t dirClust, subEntry;
        unsigned int k=1, i, t=1000000;
        while (k<1000000) {
                GenerateSFN(lfn, k, i, t);
                strcpy(fullname, path);
                strcat(fullname, sfn);
                if(!getFileDirEntry(fullname, &fileEntry, &dirClust, &subEntry,/*dirOk*/true)) return sfn;
                k++;
        }
        return NULL;
}

class fatFile : public DOS_File {
        public:
                fatFile(const char* name, uint32_t startCluster, uint32_t fileLen, fatDrive *useDrive);
                bool Read(uint8_t * data,uint16_t * size) override;
                bool Write(const uint8_t * data,uint16_t * size) override;
                bool Seek(uint32_t * pos,uint32_t type) override;
                bool Close() override;
                uint16_t GetInformation(void) override;
                void Flush(void) override;
                bool UpdateDateTimeFromHost(void) override;
                uint32_t GetSeekPos(void) override;
                uint32_t firstCluster;
                uint32_t seekpos = 0;
                uint32_t filelength;
                uint32_t currentSector = 0;
                uint32_t curSectOff = 0;
                uint8_t sectorBuffer[SECTOR_SIZE_MAX];
                /* Record of where in the directory structure this file is located */
                uint32_t dirCluster = 0;
                uint32_t dirIndex = 0;

                bool modified = false;
                bool loadedSector = false;
                fatDrive *myDrive;
};

void time_t_to_DOS_DateTime(uint16_t &t,uint16_t &d,time_t unix_time) {
        struct tm time;
        time.tm_isdst = -1;

        uint16_t oldax=reg_ax, oldcx=reg_cx, olddx=reg_dx;
        reg_ah=0x2a; // get system date
        CALLBACK_RunRealInt(0x21);

        time.tm_year = reg_cx-1900;
        time.tm_mon = reg_dh-1;
        time.tm_mday = reg_dl;

        reg_ah=0x2c; // get system time
        CALLBACK_RunRealInt(0x21);

        time.tm_hour = reg_ch;
        time.tm_min = reg_cl;
        time.tm_sec = reg_dh;

        reg_ax=oldax;
        reg_cx=oldcx;
        reg_dx=olddx;

        time_t timet = mktime(&time);
        const struct tm *tm = localtime(timet == -1?&unix_time:&timet);
        if (tm == NULL) return;

        /* NTS: tm->tm_year = years since 1900,
         *      tm->tm_mon = months since January therefore January == 0
         *      tm->tm_mday = day of the month, starting with 1 */

        t = ((unsigned int)tm->tm_hour << 11u) + ((unsigned int)tm->tm_min << 5u) + ((unsigned int)tm->tm_sec >> 1u);
        d = (((unsigned int)tm->tm_year - 80u) << 9u) + (((unsigned int)tm->tm_mon + 1u) << 5u) + (unsigned int)tm->tm_mday;
}

/* IN - char * filename: Name in regular filename format, e.g. bob.txt */
/* OUT - char * filearray: Name in DOS directory format, eleven char, e.g. bob     txt */
static void convToDirFile(const char *filename, char *filearray) {
        uint32_t charidx = 0;
        uint32_t flen,i;
        flen = (uint32_t)strlen(filename);
        memset(filearray, 32, 11);
        for(i=0;i<flen;i++) {
                if(charidx >= 11) break;
                if(filename[i] != '.') {
                        filearray[charidx] = filename[i];
                        if (charidx == 0 && filearray[0] == (char)0xe5) {
                                // SFNs beginning with E5 gets stored as 05
                                // to distinguish with a free directory entry
                                filearray[0] = 0x05;
                        }
                        charidx++;
                } else {
                        charidx = 8;
                }
        }
}

fatFile::fatFile(const char* /*name*/, uint32_t startCluster, uint32_t fileLen, fatDrive *useDrive) : firstCluster(startCluster), filelength(fileLen), myDrive(useDrive) {
	uint32_t seekto = 0;
	open = true;
	memset(&sectorBuffer[0], 0, sizeof(sectorBuffer));
	
	if(filelength > 0) {
		Seek(&seekto, DOS_SEEK_SET);
	}
}

void fatFile::Flush(void) {
	if (loadedSector) {
		myDrive->writeSector(currentSector, sectorBuffer);
		loadedSector = false;
	}

	if (modified || newtime) {
		direntry tmpentry = {};

		myDrive->directoryBrowse(dirCluster, &tmpentry, (int32_t)dirIndex);

		if (newtime) {
			tmpentry.modTime = time;
			tmpentry.modDate = date;
		}
		else {
			uint16_t ct,cd;

			time_t_to_DOS_DateTime(/*&*/ct,/*&*/cd,::time(NULL));

			tmpentry.modTime = ct;
			tmpentry.modDate = cd;
		}

		myDrive->directoryChange(dirCluster, &tmpentry, (int32_t)dirIndex);
		modified = false;
		newtime = false;
	}
}

bool fatFile::Read(uint8_t * data, uint16_t *size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {	// check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	uint16_t sizedec, sizecount;
	if(seekpos >= filelength) {
		*size = 0;
		return true;
	}

	if (!loadedSector) {
		currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
		if(currentSector == 0) {
			/* EOC reached before EOF */
			*size = 0;
			loadedSector = false;
			return true;
		}
		curSectOff = seekpos % myDrive->getSectorSize();
		myDrive->readSector(currentSector, sectorBuffer);
		loadedSector = true;
	}

	sizedec = *size;
	sizecount = 0;
	while(sizedec != 0) {
		if(seekpos >= filelength) {
			*size = sizecount;
			return true; 
		}
		data[sizecount++] = sectorBuffer[curSectOff++];
		seekpos++;
		if(curSectOff >= myDrive->getSectorSize()) {
			currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
			if(currentSector == 0) {
				/* EOC reached before EOF */
				//LOG_MSG("EOC reached before EOF, seekpos %d, filelen %d", seekpos, filelength);
				*size = sizecount;
				loadedSector = false;
				return true;
			}
			curSectOff = 0;
			myDrive->readSector(currentSector, sectorBuffer);
			loadedSector = true;
			//LOG_MSG("Reading absolute sector at %d for seekpos %d", currentSector, seekpos);
		}
		--sizedec;
	}
	*size =sizecount;
	return true;
}

bool fatFile::Write(const uint8_t * data, uint16_t *size) {
	if ((this->flags & 0xf) == OPEN_READ) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

    direntry tmpentry = {};
	uint16_t sizedec, sizecount;
	sizedec = *size;
	sizecount = 0;

	if(seekpos < filelength && *size == 0) {
		/* Truncate file to current position */
		if(firstCluster != 0) myDrive->deleteClustChain(firstCluster, seekpos);
		if(seekpos == 0) firstCluster = 0;
		filelength = seekpos;
		if (filelength == 0) firstCluster = 0; /* A file of length zero has a starting cluster of zero as well */
		modified = true;
		goto finalizeWrite;
	}

	if(seekpos > filelength) {
		/* Extend file to current position */
		uint32_t clustSize = myDrive->getClusterSize();
		if(filelength == 0) {
			firstCluster = myDrive->getFirstFreeClust();
			if(firstCluster == 0) goto finalizeWrite; // out of space
			myDrive->allocateCluster(firstCluster, 0);
			filelength = clustSize;
		}

		/* round up */
		filelength += clustSize - 1;
		filelength -= filelength % clustSize;

		/* add clusters until the file length is correct */
		while(filelength < seekpos) {
			if(myDrive->appendCluster(firstCluster) == 0) goto finalizeWrite; // out of space
			filelength += clustSize;
		}
		assert(filelength < (seekpos+clustSize));

		/* limit file length to seekpos, then bail out if write count is zero */
		modified = true;
		if(filelength > seekpos) filelength = seekpos;
		if(*size == 0) goto finalizeWrite;
	}

	while(sizedec != 0) {
		/* Increase filesize if necessary */
		if(seekpos >= filelength) {
			if(filelength == 0) {
				firstCluster = myDrive->getFirstFreeClust();
				if(firstCluster == 0) goto finalizeWrite; // out of space
				myDrive->allocateCluster(firstCluster, 0);
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				if (currentSector == 0) {
					/* I guess allocateCluster() didn't work after all. This check is necessary to prevent
					 * this condition from treating the BOOT SECTOR as a file. */
					LOG(LOG_DOSMISC,LOG_WARN)("FAT file write: unable to allocate first cluster, erroring out");
					goto finalizeWrite;
				}
				myDrive->readSector(currentSector, sectorBuffer);
				loadedSector = true;
			}
			if (!loadedSector) {
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				if(currentSector == 0) {
					/* EOC reached before EOF - try to increase file allocation */
					myDrive->appendCluster(firstCluster);
					/* Try getting sector again */
					currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
					if(currentSector == 0) {
						/* No can do. lets give up and go home.  We must be out of room */
						goto finalizeWrite;
					}
				}
				curSectOff = seekpos % myDrive->getSectorSize();
					myDrive->readSector(currentSector, sectorBuffer);
					loadedSector = true;
				}
			filelength = seekpos+1;
		}
		--sizedec;
		modified = true;
		sectorBuffer[curSectOff++] = data[sizecount++];
		seekpos++;
		if(curSectOff >= myDrive->getSectorSize()) {
			if(loadedSector) myDrive->writeSector(currentSector, sectorBuffer);
			loadedSector = false;

			currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
			if(currentSector == 0) {
			    if (sizedec == 0) goto finalizeWrite;
				/* EOC reached before EOF - try to increase file allocation */
				myDrive->appendCluster(firstCluster);
				/* Try getting sector again */
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				if(currentSector == 0) {
					/* No can do. lets give up and go home.  We must be out of room */
					goto finalizeWrite;
				}
			}
			curSectOff = 0;
			myDrive->readSector(currentSector, sectorBuffer);
			loadedSector = true;
		}
	}
	if(curSectOff>0 && loadedSector) myDrive->writeSector(currentSector, sectorBuffer);

finalizeWrite:
	myDrive->directoryBrowse(dirCluster, &tmpentry, (int32_t)dirIndex);
	tmpentry.entrysize = filelength;

	if (myDrive->GetBPB().is_fat32())
		tmpentry.SetCluster32(firstCluster);
	else
		tmpentry.loFirstClust = (uint16_t)firstCluster;

	myDrive->directoryChange(dirCluster, &tmpentry, (int32_t)dirIndex);

	*size =sizecount;
	return true;
}

bool fatFile::Seek(uint32_t *pos, uint32_t type) {
	int32_t seekto=0;
	
	switch(type) {
		case DOS_SEEK_SET:
			seekto = (int32_t)*pos;
			break;
		case DOS_SEEK_CUR:
			/* Is this relative seek signed? */
			seekto = (int32_t)*pos + (int32_t)seekpos;
			break;
		case DOS_SEEK_END:
			seekto = (int32_t)filelength + (int32_t)*pos;
			break;
	}
//	LOG_MSG("Seek to %d with type %d (absolute value %d)", *pos, type, seekto);

	if(seekto<0) seekto = 0;
	seekpos = (uint32_t)seekto;
	currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
	if (currentSector == 0) {
		/* not within file size, thus no sector is available */
		loadedSector = false;
	} else {
		curSectOff = seekpos % myDrive->getSectorSize();
		myDrive->readSector(currentSector, sectorBuffer);
		loadedSector = true;
	}
	*pos = seekpos;
	return true;
}

bool fatFile::Close() {
	/* Flush buffer */
	if (loadedSector) myDrive->writeSector(currentSector, sectorBuffer);

	if (modified || newtime) {
		direntry tmpentry = {};

		myDrive->directoryBrowse(dirCluster, &tmpentry, (int32_t)dirIndex);

		if (newtime) {
			tmpentry.modTime = time;
			tmpentry.modDate = date;
		}
		else {
			uint16_t ct,cd;

			time_t_to_DOS_DateTime(/*&*/ct,/*&*/cd,::time(NULL));

			tmpentry.modTime = ct;
			tmpentry.modDate = cd;
		}

		myDrive->directoryChange(dirCluster, &tmpentry, (int32_t)dirIndex);
	}

	return false;
}

uint16_t fatFile::GetInformation(void) {
	return 0;
}

bool fatFile::UpdateDateTimeFromHost(void) {
	return true;
}

uint32_t fatFile::GetSeekPos() {
	return seekpos;
}

uint32_t fatDrive::getClustFirstSect(uint32_t clustNum) {
	if (unformatted) return 0;
	return ((clustNum - 2) * BPB.v.BPB_SecPerClus) + firstDataSector;
}

uint32_t fatDrive::getClusterValue(uint32_t clustNum) {
	uint32_t fatoffset=0;
	uint32_t fatsectnum;
	uint32_t fatentoff;
	uint32_t clustValue=0;

	if (unformatted) return 0xFFFFFFFFu;

	switch(fattype) {
		case FAT12:
			fatoffset = clustNum + (clustNum / 2);
			break;
		case FAT16:
			fatoffset = clustNum * 2;
			break;
		case FAT32:
			fatoffset = clustNum * 4;
			break;
	}
	fatsectnum = BPB.v.BPB_RsvdSecCnt + (fatoffset / BPB.v.BPB_BytsPerSec) + partSectOff;
	fatentoff = fatoffset % BPB.v.BPB_BytsPerSec;

    if (BPB.is_fat32()) {
        if (fatsectnum >= (BPB.v.BPB_RsvdSecCnt + BPB.v32.BPB_FATSz32 + partSectOff)) {
            LOG(LOG_DOSMISC,LOG_ERROR)("Attempt to read cluster entry from FAT that out of range (outside the FAT table) cluster %u",(unsigned int)clustNum);
            return 0;
        }
    }
    else {
        if (fatsectnum >= (BPB.v.BPB_RsvdSecCnt + BPB.v.BPB_FATSz16 + partSectOff)) {
            LOG(LOG_DOSMISC,LOG_ERROR)("Attempt to read cluster entry from FAT that out of range (outside the FAT table) cluster %u",(unsigned int)clustNum);
            return 0;
        }
    }

    assert((BPB.v.BPB_BytsPerSec * (Bitu)2) <= sizeof(fatSectBuffer));

	if(curFatSect != fatsectnum) {
		/* Load two sectors at once for FAT12 */
		readSector(fatsectnum, &fatSectBuffer[0]);
		if (fattype==FAT12)
			readSector(fatsectnum+1, &fatSectBuffer[BPB.v.BPB_BytsPerSec]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12:
			clustValue = var_read((uint16_t*)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) {
				clustValue >>= 4;
			} else {
				clustValue &= 0xfff;
			}
			break;
		case FAT16:
			clustValue = var_read((uint16_t*)&fatSectBuffer[fatentoff]);
			break;
		case FAT32:
			clustValue = var_read((uint32_t*)&fatSectBuffer[fatentoff]) & 0x0FFFFFFFul; /* Well, actually it's FAT28. Upper 4 bits are "reserved". */
			break;
	}

	return clustValue;
}

void fatDrive::setClusterValue(uint32_t clustNum, uint32_t clustValue) {
	uint32_t fatoffset=0;
	uint32_t fatsectnum;
	uint32_t fatentoff;

	if (unformatted) return;

	switch(fattype) {
		case FAT12:
			fatoffset = clustNum + (clustNum / 2);
			break;
		case FAT16:
			fatoffset = clustNum * 2;
			break;
		case FAT32:
			fatoffset = clustNum * 4;
			break;
	}
	fatsectnum = BPB.v.BPB_RsvdSecCnt + (fatoffset / BPB.v.BPB_BytsPerSec) + partSectOff;
	fatentoff = fatoffset % BPB.v.BPB_BytsPerSec;

    if (BPB.is_fat32()) {
        if (fatsectnum >= (BPB.v.BPB_RsvdSecCnt + BPB.v32.BPB_FATSz32 + partSectOff)) {
            LOG(LOG_DOSMISC,LOG_ERROR)("Attempt to write cluster entry from FAT that out of range (outside the FAT table) cluster %u",(unsigned int)clustNum);
            return;
        }
    }
    else {
        if (fatsectnum >= (BPB.v.BPB_RsvdSecCnt + BPB.v.BPB_FATSz16 + partSectOff)) {
            LOG(LOG_DOSMISC,LOG_ERROR)("Attempt to write cluster entry from FAT that out of range (outside the FAT table) cluster %u",(unsigned int)clustNum);
            return;
        }
    }

    assert((BPB.v.BPB_BytsPerSec * (Bitu)2) <= sizeof(fatSectBuffer));

	if(curFatSect != fatsectnum) {
		/* Load two sectors at once for FAT12 */
		readSector(fatsectnum, &fatSectBuffer[0]);
		if (fattype==FAT12)
			readSector(fatsectnum+1, &fatSectBuffer[BPB.v.BPB_BytsPerSec]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12: {
			uint16_t tmpValue = var_read((uint16_t *)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) {
				clustValue &= 0xfff;
				clustValue <<= 4;
				tmpValue &= 0xf;
				tmpValue |= (uint16_t)clustValue;

			} else {
				clustValue &= 0xfff;
				tmpValue &= 0xf000;
				tmpValue |= (uint16_t)clustValue;
			}
			var_write((uint16_t *)&fatSectBuffer[fatentoff], tmpValue);
			break;
			}
		case FAT16:
			var_write(((uint16_t *)&fatSectBuffer[fatentoff]), (uint16_t)clustValue);
			break;
		case FAT32:
			var_write(((uint32_t *)&fatSectBuffer[fatentoff]), clustValue);
			break;
	}
	for(unsigned int fc=0;fc<BPB.v.BPB_NumFATs;fc++) {
		writeSector(fatsectnum + (fc * (BPB.is_fat32() ? BPB.v32.BPB_FATSz32 : BPB.v.BPB_FATSz16)), &fatSectBuffer[0]);
		if (fattype==FAT12) {
			if (fatentoff >= (BPB.v.BPB_BytsPerSec-1U))
				writeSector(fatsectnum+1u+(fc * (BPB.is_fat32() ? BPB.v32.BPB_FATSz32 : BPB.v.BPB_FATSz16)), &fatSectBuffer[BPB.v.BPB_BytsPerSec]);
		}
	}
}

bool fatDrive::getEntryName(const char *fullname, char *entname) {
	if (unformatted) return false;

	char dirtoken[DOS_PATHLENGTH];

	char * findDir;
	char * findFile;
	strcpy(dirtoken,fullname);

	//LOG_MSG("Testing for filename %s", fullname);
	findDir = strtok_dbcs(dirtoken,"\\");
	if (findDir==NULL) {
		return true;	// root always exists
	}
	findFile = findDir;
	while(findDir != NULL) {
		findFile = findDir;
		findDir = strtok_dbcs(NULL,"\\");
	}
	int j=0;
	bool lead = false;
	for (int i=0; i<(int)strlen(findFile); i++) {
		if (findFile[i]!=' '&&findFile[i]!='"'&&findFile[i]!='+'&&findFile[i]!='='&&findFile[i]!=','&&findFile[i]!=';'&&findFile[i]!=':'&&findFile[i]!='<'&&findFile[i]!='>'&&!((findFile[i]=='['||findFile[i]==']'||findFile[i]=='|'||findFile[i]=='\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))&&findFile[i]!='?'&&findFile[i]!='*') findFile[j++]=findFile[i];
		if (lead) lead = false;
		else if ((IS_PC98_ARCH && shiftjis_lead_byte(findFile[i]&0xFF)) || (isDBCSCP() && isKanji1_gbk(findFile[i]&0xFF))) lead = true;
	}
	findFile[j]=0;
	if (strlen(findFile)>12)
		strncpy(entname, DBCS_upcase(findFile), 12);
	else
		strcpy(entname, DBCS_upcase(findFile));
	return true;
}

void fatDrive::UpdateBootVolumeLabel(const char *label) {
	if (unformatted) return;

    FAT_BootSector bootbuffer = {};

    if (BPB.v.BPB_BootSig == 0x28 || BPB.v.BPB_BootSig == 0x29) {
        unsigned int i = 0;
        char upcasebuf[12] = {0};
        const char *upcaseptr = upcasebuf;

        loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);

        strncpy(upcasebuf, label, 11);
        DBCS_upcase(upcasebuf);
        // initial 0xe5 substituted to 0x05 in the same way as other SFN
        // even though this is in BPB and 0xe5 shouldn't matter
        if (upcasebuf[0] == (char)0xe5) upcasebuf[0] = 0x05;
        while (i < 11 && *upcaseptr != 0) bootbuffer.bpb.v.BPB_VolLab[i++] = *upcaseptr++;
        while (i < 11)                    bootbuffer.bpb.v.BPB_VolLab[i++] = ' ';

        loadedDisk->Write_AbsoluteSector(0+partSectOff,&bootbuffer);
    }
}

void fatDrive::SetLabel(const char *label, bool /*iscdrom*/, bool /*updatable*/) {
	if (unformatted) return;

	direntry sectbuf[MAX_DIRENTS_PER_SECTOR]; /* 16 directory entries per 512 byte sector */
	uint32_t dirClustNumber;
	uint32_t logentsector; /* Logical entry sector */
	uint32_t entryoffset;  /* Index offset within sector */
	uint32_t tmpsector;
	uint16_t dirPos = 0;

	size_t dirent_per_sector = getSectSize() / sizeof(direntry);
	assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
	assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	if(!getDirClustNum("\\", &dirClustNumber, false))
		return;

nextfile:
	logentsector = (uint32_t)((size_t)dirPos / dirent_per_sector);
	entryoffset = (uint32_t)((size_t)dirPos % dirent_per_sector);

	if(dirClustNumber==0) {
		assert(!BPB.is_fat32());
		if(dirPos >= BPB.v.BPB_RootEntCnt) return;
		tmpsector = firstRootDirSect+logentsector;
	} else {
		/* A zero sector number can't happen */
		tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
		/* A zero sector number can't happen - we need to allocate more room for this directory*/
		if(tmpsector == 0) {
			if (*label == 0) return; // removing volume label, so stop now
			uint32_t newClust;
			newClust = appendCluster(dirClustNumber);
			if(newClust == 0) return;
			zeroOutCluster(newClust);
			/* Try again to get tmpsector */
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			if(tmpsector == 0) return; /* Give up if still can't get more room for directory */
		}
		readSector(tmpsector,sectbuf);
	}
	readSector(tmpsector,sectbuf);
	dirPos++;

	if (dos.version.major >= 7 || uselfn) {
		/* skip LFN entries */
		if ((sectbuf[entryoffset].attrib & 0x3F) == 0x0F)
			goto nextfile;
	}

	if (*label != 0) {
		/* adding a volume label */
		if (sectbuf[entryoffset].entryname[0] == 0x00 ||
			sectbuf[entryoffset].entryname[0] == 0xE5) {
			memset(&sectbuf[entryoffset],0,sizeof(sectbuf[entryoffset]));
			sectbuf[entryoffset].attrib = DOS_ATTR_VOLUME;
			{
				unsigned int j = 0;
				const char *s;
				char upcasebuf[12] = {0};
				strncpy(upcasebuf, label, 11);
				DBCS_upcase(upcasebuf);
				// initial 0xe5 substituted to 0x05 in the same way as other SFN
				if (upcasebuf[0] == (char)0xe5) upcasebuf[0] = 0x05;
				s = upcasebuf;
				while (j < 11 && *s != 0) sectbuf[entryoffset].entryname[j++] = *s++;
				while (j < 11)            sectbuf[entryoffset].entryname[j++] = ' ';
			}
            uint16_t ct, cd;
            time_t_to_DOS_DateTime(/*&*/ct,/*&*/cd, ::time(NULL));
            sectbuf[entryoffset].modTime = ct;
            sectbuf[entryoffset].modDate = cd;
            sectbuf[entryoffset].accessDate = cd;

            writeSector(tmpsector,sectbuf);
			labelCache.SetLabel(label, false, true);
			UpdateBootVolumeLabel(label);
			return;
		}
	}
	else {
		if (sectbuf[entryoffset].entryname[0] == 0x00)
			return;
		if (sectbuf[entryoffset].entryname[0] == 0xe5)
			goto nextfile;

		if (sectbuf[entryoffset].attrib == DOS_ATTR_VOLUME) {
			/* TODO: There needs to be a way for FCB delete to erase the volume label by name instead
			 *       of just picking the first one */
			/* found one */
			sectbuf[entryoffset].entryname[0] = 0xe5;
			writeSector(tmpsector,sectbuf);
			labelCache.SetLabel("", false, true);
			UpdateBootVolumeLabel("NO NAME");
			return;
		}
	}

	goto nextfile;
}

/* NTS: This function normally will only return files. Every element of the path that is a directory is entered into.
 *      If every element is a directory, then this code will fail to locate anything.
 *
 *      If dirOk is set, and all path elements are directories, it will stop at the last one and look it up as if a file.
 *      The purpose is to clean up this FAT driver by eliminating all the ridiculous "look up getFileDirEntry but if it fails
 *      do a whole different code path that looks it up as if directory" copy-pasta in this code that complicates some functions
 *      like the Rename() method.
 *
 *      useEntry is filled with the SFN direntry of the first search result. dirClust is filled in with the starting cluster of
 *      the parent directory. Note that even if dirOk is set and the result is a directory, dirClust is the parent directory of
 *      that directory. subEntry is the dirent index into the directory.
 *
 *      As a side effect of using FindNextInternal, variable lfnRange will be either cleared or filled in with the subEntry range
 *      of dirents that contain the LFN entries (needed for deletion, renaming, rmdir, etc). Not all paths set or clear it, so
 *      first call the clear() method before calling. After the call, copy off the value because the next call to FindNextInternal
 *      by any part of this code will obliterate the result with a new result. */
bool fatDrive::getFileDirEntry(char const * const filename, direntry * useEntry, uint32_t * dirClust, uint32_t * subEntry,bool dirOk) {
	if (unformatted) return false;

	size_t len = strlen(filename);
	char dirtoken[DOS_PATHLENGTH];
	uint32_t currentClust = 0; /* FAT12/FAT16 root directory */

	direntry foundEntry;
	char * findDir;
	char * findFile;
	strcpy(dirtoken,filename);
	findFile=dirtoken;

	if (BPB.is_fat32()) {
		/* Set to FAT32 root directory */
		currentClust = BPB.v32.BPB_RootClus;
	}

	int fbak=lfn_filefind_handle;
	lfn_filefind_handle=uselfn?LFN_FILEFIND_IMG:LFN_FILEFIND_NONE;
	/* Skip if testing in root directory */
	if ((len>0) && (filename[len-1]!='\\')) {
		//LOG_MSG("Testing for filename %s", filename);
		findDir = strtok_dbcs(dirtoken,"\\");
		findFile = findDir;
		while(findDir != NULL) {
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			
			findFile = findDir;
			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) break;
			else {
				//Found something. See if it's a directory (findfirst always finds regular files)
                char find_name[DOS_NAMELENGTH_ASCII],lfind_name[LFN_NAMELENGTH];
                uint16_t find_date,find_time;uint32_t find_size,find_hsize;uint8_t find_attr;
                imgDTA->GetResult(find_name,lfind_name,find_size,find_hsize,find_date,find_time,find_attr);
				if(!(find_attr & DOS_ATTR_DIRECTORY)) break;

				char * findNext;
				findNext = strtok_dbcs(NULL,"\\");
				if (findNext == NULL && dirOk) break; /* dirOk means that if the last element is a directory, then refer to the directory itself */
				findDir = findNext;
			}

			if (BPB.is_fat32())
				currentClust = foundEntry.Cluster32();
			else
				currentClust = foundEntry.loFirstClust;
		}
	} else {
		/* Set to root directory */
	}

	/* Search found directory for our file */
	imgDTA->SetupSearch(0,0x7 | (dirOk ? DOS_ATTR_DIRECTORY : 0),findFile);
	imgDTA->SetDirID(0);
	if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {lfn_filefind_handle=fbak;return false;}
	lfn_filefind_handle=fbak;

	memcpy(useEntry, &foundEntry, sizeof(direntry));
	*dirClust = (uint32_t)currentClust;
	*subEntry = ((uint32_t)imgDTA->GetDirID()-1);
	return true;
}

bool fatDrive::getDirClustNum(const char *dir, uint32_t *clustNum, bool parDir) {
	if (unformatted) return false;

	uint32_t len = (uint32_t)strlen(dir);
	char dirtoken[DOS_PATHLENGTH];
	direntry foundEntry;
	strcpy(dirtoken,dir);

	int fbak=lfn_filefind_handle;
	/* Skip if testing for root directory */
	if ((len>0) && (dir[len-1]!='\\')) {
		uint32_t currentClust = 0;

        if (BPB.is_fat32()) {
            /* Set to FAT32 root directory */
            currentClust = BPB.v32.BPB_RootClus;
        }

		//LOG_MSG("Testing for dir %s", dir);
		char * findDir = strtok_dbcs(dirtoken,"\\");
		while(findDir != NULL) {
			lfn_filefind_handle=uselfn?LFN_FILEFIND_IMG:LFN_FILEFIND_NONE;
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			findDir = strtok_dbcs(NULL,"\\");
			if(parDir && (findDir == NULL)) {lfn_filefind_handle=fbak;break;}

			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {
				lfn_filefind_handle=fbak;
				return false;
			} else {
                char find_name[DOS_NAMELENGTH_ASCII],lfind_name[LFN_NAMELENGTH];
                uint16_t find_date,find_time;uint32_t find_size,find_hsize;uint8_t find_attr;
				imgDTA->GetResult(find_name,lfind_name,find_size,find_hsize,find_date,find_time,find_attr);
				lfn_filefind_handle=fbak;
				if(!(find_attr &DOS_ATTR_DIRECTORY)) return false;
			}
			if (BPB.is_fat32())
				currentClust = foundEntry.Cluster32();
			else
				currentClust = foundEntry.loFirstClust;
		}
		*clustNum = currentClust;
	} else if (BPB.is_fat32()) {
		/* Set to FAT32 root directory */
		*clustNum = BPB.v32.BPB_RootClus;
	} else {
		/* Set to root directory */
		*clustNum = 0;
	}
	return true;
}

/*
   The following sector# to CHS-sector# conversion is unclear to me, since
   the sector nature would be RELATIVE (but here... to what???), while we
   need an ABSOLUTE (disk-relative) sector number to pass to loadedDisk
   read/write functions, which calculate:
	  sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;
   where heads and sectors are from *DISK* geometry, not *BPB*.
   
   However, if CHS is expressed in terms of the loadedDisk geometry
   (instead of fatDrive's, which can differ), VHD access works fine, and
   RAW images keep working.  2023.05.11 - maxpat78 */
uint8_t fatDrive::readSector(uint32_t sectnum, void * data) {
	if (absolute) return Read_AbsoluteSector(sectnum, data);
    assert(!IS_PC98_ARCH);
#ifdef OLD_CHS_CONVERSION
	uint32_t cylindersize = (unsigned int)BPB.v.BPB_NumHeads * (unsigned int)BPB.v.BPB_SecPerTrk;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / BPB.v.BPB_SecPerTrk;
	uint32_t sector = sectnum % BPB.v.BPB_SecPerTrk + 1L;
#else
	uint32_t cylindersize = (unsigned int)loadedDisk->heads * (unsigned int)loadedDisk->sectors;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / loadedDisk->sectors;
	uint32_t sector = sectnum % loadedDisk->sectors + 1L;
#endif
	return loadedDisk->Read_Sector(head, cylinder, sector, data);
}	

uint8_t fatDrive::writeSector(uint32_t sectnum, void * data) {
	if (absolute) return Write_AbsoluteSector(sectnum, data);
    assert(!IS_PC98_ARCH);
#ifdef OLD_CHS_CONVERSION
	uint32_t cylindersize = (unsigned int)BPB.v.BPB_NumHeads * (unsigned int)BPB.v.BPB_SecPerTrk;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / BPB.v.BPB_SecPerTrk;
	uint32_t sector = sectnum % BPB.v.BPB_SecPerTrk + 1L;
#else
	uint32_t cylindersize = (unsigned int)loadedDisk->heads * (unsigned int)loadedDisk->sectors;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / loadedDisk->sectors;
	uint32_t sector = sectnum % loadedDisk->sectors + 1L;
#endif
	return loadedDisk->Write_Sector(head, cylinder, sector, data);
}

uint32_t fatDrive::getSectorCount(void) {
	if (BPB.v.BPB_TotSec16 != 0)
		return (uint32_t)BPB.v.BPB_TotSec16;
	else
		return BPB.v.BPB_TotSec32;
 }

uint32_t fatDrive::getSectorSize(void) {
	return BPB.v.BPB_BytsPerSec;
}

uint32_t fatDrive::getClusterSize(void) {
	if (unformatted) return 0;
	return (unsigned int)BPB.v.BPB_SecPerClus * (unsigned int)BPB.v.BPB_BytsPerSec;
}

uint32_t fatDrive::getAbsoluteSectFromBytePos(uint32_t startClustNum, uint32_t bytePos,clusterChainMemory *ccm) {
	if (unformatted) return 0;
	return  getAbsoluteSectFromChain(startClustNum, bytePos / BPB.v.BPB_BytsPerSec,ccm);
}

bool fatDrive::iseofFAT(const uint32_t cv) const {
	if (unformatted) return true;
	switch(fattype) {
		case FAT12: return cv < 2 || cv >= 0xff8;
		case FAT16: return cv < 2 || cv >= 0xfff8;
		case FAT32: return cv < 2 || cv >= 0x0ffffff8;
		default: break;
	}

	return true;
}

uint32_t fatDrive::getAbsoluteSectFromChain(uint32_t startClustNum, uint32_t logicalSector,clusterChainMemory *ccm) {
	if (unformatted) return 0;
	uint32_t targClust = (uint32_t)(logicalSector / BPB.v.BPB_SecPerClus);
	uint32_t sectClust = (uint32_t)(logicalSector % BPB.v.BPB_SecPerClus);
	uint32_t indxClust = (uint32_t)0;

	/* startClustNum == 0 means the file is (likely) zero length and has no allocation chain yet.
	 * Nothing to map. Without this check, this code would permit the FAT file reader/writer to
	 * treat the ROOT DIRECTORY as a file (with disastrous results)
	 *
	 * [https://github.com/joncampbell123/dosbox-x/issues/1517] */
	if (startClustNum == 0) return 0;

	uint32_t currentClust = startClustNum;

	if (ccm != NULL && ccm->current_cluster_no >= 2) {
		/* If the cluster index is the same as last time or farther down, avoid re-reading the
		 * entire allocation chain again and start from where we last read from. If the
		 * cluster index is going back from current, then re-read the entire allocation chain again.
		 * This is the nature of a singly-linked file allocation table and is the reason seek()
		 * is faster going forward than backwards in MS-DOS, especially on FAT32 partitions. */
		if (targClust >= ccm->current_cluster_index) {
			indxClust = ccm->current_cluster_index;
			currentClust = ccm->current_cluster_no;
		}
	}

	while(indxClust<targClust) {
		const uint32_t testvalue = getClusterValue(currentClust);
		++indxClust;

		if (iseofFAT(testvalue)) {
			if (indxClust!=targClust) LOG(LOG_MISC,LOG_DEBUG)("FAT: Seek past allocation chain");
			return 0;
		}

		currentClust = testvalue;
	}

	assert(indxClust<=targClust);

	if (ccm != NULL) {
		ccm->current_cluster_index = currentClust;
		ccm->current_cluster_no = indxClust;
	}

	/* this should not happen! */
	assert(currentClust != 0);

	return (getClustFirstSect(currentClust) + sectClust);
}

void fatDrive::deleteClustChain(uint32_t startCluster, uint32_t bytePos) {
	if (unformatted) return;
	if (startCluster < 2) return; /* do not corrupt the FAT media ID. The file has no chain. Do nothing. */

	uint32_t clustSize = getClusterSize();
	uint32_t endClust = (bytePos + clustSize - 1) / clustSize;
	uint32_t countClust = 1;

	uint32_t currentClust = startCluster;
	uint32_t eofClust = 0;

	switch(fattype) {
		case FAT12:
			eofClust = 0xff8;
			break;
		case FAT16:
			eofClust = 0xfff8;
			break;
		case FAT32:
			eofClust = 0x0ffffff8;
			break;
		default:
			abort();
			break;
	}

	/* chain preservation */
	while (countClust < endClust) {
		uint32_t testvalue = getClusterValue(currentClust);
		if (testvalue == 0) {
			LOG(LOG_DOSMISC,LOG_WARN)("deleteClusterChain startCluster=%u countClust=%u endClust=%u currentClust=%u testvalue=%u eof=%u unexpected zero cluster value in FAT table",
					(unsigned int)startCluster,(unsigned int)countClust,(unsigned int)endClust,(unsigned int)currentClust,(unsigned int)testvalue,(unsigned int)eofClust);
			return;
		}
		else if (testvalue >= eofClust)
			return; /* Allocation chain is already shorter than intended */

		currentClust = testvalue;
		countClust++;
	}

	/* cut the chain here, write EOF.
	 * This condition will NOT occur if bytePos == 0 (i.e. complete file truncation)
	 * because countClust == 1 and endClust == 0 */
	if (countClust == endClust) {
		uint32_t testvalue = getClusterValue(currentClust);
		if (testvalue == 0) {
			LOG(LOG_DOSMISC,LOG_WARN)("deleteClusterChain startCluster=%u countClust=%u endClust=%u currentClust=%u testvalue=%u eof=%u unexpected zero cluster value in FAT table",
					(unsigned int)startCluster,(unsigned int)countClust,(unsigned int)endClust,(unsigned int)currentClust,(unsigned int)testvalue,(unsigned int)eofClust);
			return;
		}
		else if (testvalue >= eofClust)
			return; /* No need to write EOF because EOF is already there */

		setClusterValue(currentClust,eofClust);
		if (searchFreeCluster > (currentClust - 2)) searchFreeCluster = currentClust - 2;
		currentClust = testvalue;
		countClust++;
	}

	/* then run the rest of the chain and zero it out */
	while (1) {
		uint32_t testvalue = getClusterValue(currentClust);
		if (testvalue == 0) {
			LOG(LOG_DOSMISC,LOG_WARN)("deleteClusterChain startCluster=%u countClust=%u endClust=%u currentClust=%u testvalue=%u eof=%u unexpected zero cluster value in FAT table",
					(unsigned int)startCluster,(unsigned int)countClust,(unsigned int)endClust,(unsigned int)currentClust,(unsigned int)testvalue,(unsigned int)eofClust);
			return;
		}

		setClusterValue(currentClust,0);
		if (searchFreeCluster > (currentClust - 2)) searchFreeCluster = currentClust - 2;
		currentClust = testvalue;
		countClust++;

		/* this follows setClusterValue() to make sure the end of the chain is zeroed too */
		if (testvalue >= eofClust)
			return;
	}
}

uint32_t fatDrive::appendCluster(uint32_t startCluster) {
	if (unformatted) return 0;
	if (startCluster < 2) return 0; /* do not corrupt the FAT media ID. The file has no chain. Do nothing. */

	uint32_t currentClust = startCluster;
	uint32_t eofClust = 0;

	switch(fattype) {
		case FAT12:
			eofClust = 0xff8;
			break;
		case FAT16:
			eofClust = 0xfff8;
			break;
		case FAT32:
			eofClust = 0x0ffffff8;
			break;
		default:
			abort();
	}

	while (1) {
		uint32_t testvalue = getClusterValue(currentClust);
		if (testvalue == 0) {
			LOG(LOG_DOSMISC,LOG_WARN)("appendCluster currentClust=%u testvalue=%u eof=%u unexpected zero cluster value in FAT table",
					(unsigned int)currentClust,(unsigned int)testvalue,(unsigned int)eofClust);
			return 0;
		}
		else if (testvalue >= eofClust) {
			break; /* found it! */
		}

		currentClust = testvalue;
	}

	uint32_t newClust = getFirstFreeClust();
	if(newClust == 0) return 0; /* Drive is full */

	if(!allocateCluster(newClust, currentClust)) return 0;

	zeroOutCluster(newClust);

	return newClust;
}

bool fatDrive::allocateCluster(uint32_t useCluster, uint32_t prevCluster) {
	if (unformatted) return false;

	/* Can't allocate cluster #0 */
	if(useCluster == 0) return false;

	if(prevCluster != 0) {
		/* Refuse to allocate cluster if previous cluster value is zero (unallocated) */
		if(!getClusterValue(prevCluster)) return false;

		/* Point cluster to new cluster in chain */
		setClusterValue(prevCluster, useCluster);
		//LOG_MSG("Chaining cluster %d to %d", prevCluster, useCluster);
	} 

	switch(fattype) {
		case FAT12:
			setClusterValue(useCluster, 0xfff);
			break;
		case FAT16:
			setClusterValue(useCluster, 0xffff);
			break;
		case FAT32:
			setClusterValue(useCluster, 0x0fffffff);
			break;
	}
	return true;
}

fatDrive::~fatDrive() {
	if (loadedDisk) {
		if (partition_index >= 0) loadedDisk->partitionMarkUse(partition_index,false);
		loadedDisk->Release();
		loadedDisk = NULL;
	}
}

FILE * fopen_lock(const char * fname, const char * mode, bool &readonly);
fatDrive::fatDrive(const char* sysFilename, uint32_t bytesector, uint32_t cylsector, uint32_t headscyl, uint32_t cylinders, std::vector<std::string>& options) {
	FILE *diskfile;
	uint32_t filesize;
	unsigned char bootcode[256];

	if(imgDTASeg == 0) {
		imgDTASeg = DOS_GetMemory(4,"imgDTASeg");
		imgDTAPtr = RealMake(imgDTASeg, 0);
		imgDTA    = new DOS_DTA(imgDTAPtr);
	}

	std::vector<std::string>::iterator it = std::find(options.begin(), options.end(), "readonly");
	bool roflag = it!=options.end();
	readonly = wpcolon&&strlen(sysFilename)>1&&sysFilename[0]==':';
	const char *fname=readonly?sysFilename+1:sysFilename;
	diskfile = fopen_lock(fname, readonly||roflag?"rb":"rb+", readonly);
	if (!diskfile) {created_successfully = false;return;}
	opts.bytesector = bytesector;
	opts.cylsector = cylsector;
	opts.headscyl = headscyl;
	opts.cylinders = cylinders;
	opts.mounttype = 0;

	// all disk I/O is in sector-sized blocks.
	// modern OSes have good caching.
	// there are plenty of cases where this code aborts, exits, or re-execs itself (such as reboot)
	// where stdio buffering can cause loss of data.
	setbuf(diskfile,NULL);

	QCow2Image::QCow2Header qcow2_header = QCow2Image::read_header(diskfile);

	if (qcow2_header.magic == QCow2Image::magic && (qcow2_header.version == 2 || qcow2_header.version == 3)){
		uint32_t cluster_size = 1u << qcow2_header.cluster_bits;
		if ((bytesector < 512) || ((cluster_size % bytesector) != 0)){
			created_successfully = false;
			return;
		}
		filesize = (uint32_t)(qcow2_header.size / 1024L);
		loadedDisk = new QCow2Disk(qcow2_header, diskfile, fname, filesize, bytesector, (filesize > 2880));
	}
	else{
		fseeko64(diskfile, 0L, SEEK_SET);
		assert(sizeof(bootcode) >= 256);
		size_t readResult = fread(bootcode,256,1,diskfile); // look for magic signatures
		if (readResult != 1) {
			LOG(LOG_IO, LOG_ERROR) ("Reading error in fatDrive constructor\n");
			return;
		}

		const char *ext = strrchr(sysFilename,'.');
		bool is_hdd = false;
		if((ext != NULL) && (!strcasecmp(ext, ".hdi") || !strcasecmp(ext, ".nhd"))) is_hdd = true;

		if (ext != NULL && !strcasecmp(ext, ".d88")) {
			fseeko64(diskfile, 0L, SEEK_END);
			filesize = (uint32_t)(ftello64(diskfile) / 1024L);
			loadedDisk = new imageDiskD88(diskfile, fname, filesize, false);
		}
		else if (!memcmp(bootcode,"VFD1.",5)) { /* FDD files */
			fseeko64(diskfile, 0L, SEEK_END);
			filesize = (uint32_t)(ftello64(diskfile) / 1024L);
			loadedDisk = new imageDiskVFD(diskfile, fname, filesize, false);
		}
		else if (!memcmp(bootcode,"T98FDDIMAGE.R0\0\0",16)) {
			fseeko64(diskfile, 0L, SEEK_END);
			filesize = (uint32_t)(ftello64(diskfile) / 1024L);
			loadedDisk = new imageDiskNFD(diskfile, fname, filesize, false, 0);
		}
		else if (!memcmp(bootcode,"T98FDDIMAGE.R1\0\0",16)) {
			fseeko64(diskfile, 0L, SEEK_END);
			filesize = (uint32_t)(ftello64(diskfile) / 1024L);
			loadedDisk = new imageDiskNFD(diskfile, fname, filesize, false, 1);
		}
		else {
			fseeko64(diskfile, 0L, SEEK_END);
			filesize = (uint32_t)(ftello64(diskfile) / 1024L);
			loadedDisk = new imageDisk(diskfile, fname, filesize, (is_hdd | (filesize > 2880)));
		}
	}

	fatDriveInit(sysFilename, bytesector, cylsector, headscyl, cylinders, filesize, options);
}

fatDrive::fatDrive(imageDisk *sourceLoadedDisk, std::vector<std::string> &options) {
	if (!sourceLoadedDisk) {
		created_successfully = false;
		return;
	}
	created_successfully = true;

	if(imgDTASeg == 0) {
		imgDTASeg = DOS_GetMemory(4,"imgDTASeg");
		imgDTAPtr = RealMake(imgDTASeg, 0);
		imgDTA    = new DOS_DTA(imgDTAPtr);
	}
	std::vector<std::string>::iterator it = std::find(options.begin(), options.end(), "readonly");
	if (it!=options.end()) readonly = true;
	opts = {0,0,0,0,-1};
	imageDiskElToritoFloppy *idelt=dynamic_cast<imageDiskElToritoFloppy *>(sourceLoadedDisk);
	imageDiskMemory* idmem=dynamic_cast<imageDiskMemory *>(sourceLoadedDisk);
	imageDiskVHD* idvhd=dynamic_cast<imageDiskVHD *>(sourceLoadedDisk);
	if (idelt!=NULL) {
		readonly = true;
		opts.mounttype = 1;
		el.CDROM_drive = idelt->CDROM_drive;
		el.cdrom_sector_offset = idelt->cdrom_sector_offset;
		el.floppy_emu_type = idelt->floppy_type;
	} else if (idmem!=NULL) {
		opts.mounttype=2;
	} else if (idvhd!=NULL) {
		opts.mounttype=3;
	}

	loadedDisk = sourceLoadedDisk;

	fatDriveInit("", loadedDisk->sector_size, loadedDisk->sectors, loadedDisk->heads, loadedDisk->cylinders, loadedDisk->diskSizeK, options);
}

uint8_t fatDrive::Read_AbsoluteSector(uint32_t sectnum, void * data) {
    if (loadedDisk != NULL) {
        /* this will only work if the logical sector size is larger than the disk sector size */
        const unsigned int lsz = loadedDisk->getSectSize();
        unsigned int c = sector_size / lsz;

        if (c != 0 && (sector_size % lsz) == 0) {
            uint32_t ssect = (sectnum * c) + physToLogAdj;

            while (c-- != 0) {
                if (loadedDisk->Read_AbsoluteSector(ssect++,data) != 0)
                    return 0x05;

                data = (void*)((char*)data + lsz);
            }

            return 0;
        }
    }

    return 0x05;
}

uint8_t fatDrive::Write_AbsoluteSector(uint32_t sectnum, void * data) {
    if (loadedDisk != NULL) {
        /* this will only work if the logical sector size is larger than the disk sector size */
        const unsigned int lsz = loadedDisk->getSectSize();
        unsigned int c = sector_size / lsz;

        if (c != 0 && (sector_size % lsz) == 0) {
            uint32_t ssect = (sectnum * c) + physToLogAdj;

            while (c-- != 0) {
                if (loadedDisk->Write_AbsoluteSector(ssect++,data) != 0)
                    return 0x05;

                data = (void*)((char*)data + lsz);
            }

            return 0;
        }
    }

    return 0x05;
}

uint32_t fatDrive::getSectSize(void) {
    return sector_size;
}

void fatDrive::UpdateDPB(unsigned char dos_drive) {
    PhysPt ptr = DOS_Get_DPB(dos_drive);
    if (ptr != PhysPt(0)) {
        mem_writew(ptr+0x02,BPB.v.BPB_BytsPerSec);                  // +2 = bytes per sector
        mem_writeb(ptr+0x04,BPB.v.BPB_SecPerClus - 1);              // +4 = highest sector within a cluster
        mem_writeb(ptr+0x05,bitop::log2(BPB.v.BPB_SecPerClus));     // +5 = shift count to convert clusters to sectors
        mem_writew(ptr+0x06,BPB.v.BPB_RsvdSecCnt);                  // +6 = number of reserved sectors at start of partition
        mem_writeb(ptr+0x08,BPB.v.BPB_NumFATs);                     // +8 = number of FATs (file allocation tables)
        mem_writew(ptr+0x09,BPB.v.BPB_RootEntCnt);                  // +9 = number of root directory entries
        mem_writew(ptr+0x0B,(uint16_t)(firstDataSector-partSectOff));// +11 = number of first sector containing user data

        if (BPB.is_fat32())
            mem_writew(ptr+0x0D,0);                                 // Windows 98 behavior
        else
            mem_writew(ptr+0x0D,(uint16_t)CountOfClusters + 1);     // +13 = highest cluster number

        if (dos.version.major >= 4) {
            mem_writew(ptr+0x0F,(uint16_t)BPB.v.BPB_FATSz16);           // +15 = sectors per FAT

            if (BPB.is_fat32())
                mem_writew(ptr+0x11,0xFFFF);                            // Windows 98 behavior
            else
                mem_writew(ptr+0x11,(uint16_t)(firstRootDirSect-partSectOff));// +17 = sector number of first directory sector

            mem_writed(ptr+0x13,0xFFFFFFFF);                            // +19 = address of device driver header (NOT IMPLEMENTED) Windows 98 behavior
            mem_writeb(ptr+0x17,GetMediaByte());                        // +23 = media ID byte
            mem_writeb(ptr+0x18,0x00);                                  // +24 = disk accessed
            mem_writew(ptr+0x1F,0xFFFF);                                // +31 = number of free clusters or 0xFFFF if unknown
            // other fields, not implemented
        }
        else {
            mem_writeb(ptr+0x0F,(uint8_t)BPB.v.BPB_FATSz16);            // +15 = sectors per FAT

            if (BPB.is_fat32())
                mem_writew(ptr+0x10,0xFFFF);                            // Windows 98 behavior
            else
                mem_writew(ptr+0x10,(uint16_t)(firstRootDirSect-partSectOff));// +16 = sector number of first directory sector

            mem_writed(ptr+0x12,0xFFFFFFFF);                            // +18 = address of device driver header (NOT IMPLEMENTED) Windows 98 behavior
            mem_writeb(ptr+0x16,GetMediaByte());                        // +22 = media ID byte
            mem_writeb(ptr+0x17,0x00);                                  // +23 = disk accessed
            mem_writew(ptr+0x1E,0xFFFF);                                // +30 = number of free clusters or 0xFFFF if unknown
            // other fields, not implemented
        }
    }
}

void fatDrive::fatDriveInit(const char *sysFilename, uint32_t bytesector, uint32_t cylsector, uint32_t headscyl, uint32_t cylinders, uint64_t filesize, const std::vector<std::string> &options) {
	Bits opt_startsector = Bits(-1),opt_countsector = Bits(-1);
	uint32_t startSector = 0,countSector = 0;
	bool pc98_512_to_1024_allow = false;
	int opt_partition_index = -1;
	int int13 = -1;
	bool is_hdd = (filesize > 2880);
	if(!is_hdd) {
		char* ext = strrchr((char*)sysFilename, '.');
		if(ext != NULL && (!strcasecmp(ext, ".hdi") || !strcasecmp(ext, ".nhd"))) is_hdd = true;
	}
	bool is_pc98fd = (IS_PC98_ARCH && !is_hdd);

	physToLogAdj = 0;
	req_ver_major = req_ver_minor = 0;

	if(!loadedDisk) {
		created_successfully = false;
		return;
	}

	for (const auto &opt : options) {
		size_t equ = opt.find_first_of('=');
		std::string name,value;

		if (equ != std::string::npos) {
			name = opt.substr(0,equ);
			value = opt.substr(equ+1);
		}
		else {
			name = opt;
			value.clear();
		}

		if (name == "partidx") {
			if (!value.empty())
				opt_partition_index = (int)atol(value.c_str());
		}
		else if (name == "sectoff") {
			if (!value.empty())
				opt_startsector = Bits(strtoul(value.c_str(),NULL,0));
		}
		else if (name == "sectlen") {
			if (!value.empty())
				opt_countsector = Bits(strtoul(value.c_str(),NULL,0));
		}
		else if (name == "int13") {
			if (!value.empty())
				int13 = strtoul(value.c_str(),NULL,0);
		}
		else {
			LOG(LOG_DOSMISC,LOG_DEBUG)("FAT: option '%s' = '%s' ignored, unknown",name.c_str(),value.c_str());
		}

//		LOG_MSG("'%s' = '%s'",name.c_str(),value.c_str());
	}

	if (int13 >= 0 && int13 <= 0xFF) {
		imageDiskINT13Drive *x = new imageDiskINT13Drive(loadedDisk);
		x->bios_disk = (uint8_t)int13;
		loadedDisk = x;
	}

	loadedDisk->Addref();
	bool isipl1 = false;

	{
		FAT_BootSector bootbuffer = {};

		if (loadedDisk->getSectSize() > sizeof(bootbuffer)) {
			LOG_MSG("Disk sector/bytes (%u) is too large, not attempting FAT filesystem access",loadedDisk->getSectSize());
			created_successfully = false;
			return;
		}

		if(opt_startsector >= Bits(0)) {
			/* User knows best! */
			startSector = uint32_t(opt_startsector);

			if (opt_countsector > Bits(0))
				countSector = uint32_t(opt_countsector);
			else
				countSector = 0;

			partSectOff = startSector;
			partSectSize = countSector;
		}
		else if(is_hdd) {
			/* Set user specified harddrive parameters */
			if (headscyl > 0 && cylinders > 0 && cylsector > 0 && bytesector > 0)
				loadedDisk->Set_Geometry(headscyl, cylinders,cylsector, bytesector);

			if (loadedDisk->heads == 0 || loadedDisk->sectors == 0 || loadedDisk->cylinders == 0) {
				created_successfully = false;
				LOG_MSG("No geometry");
				return;
			}

			startSector = 0;

			const std::string ptype = PartitionIdentifyType(loadedDisk);
			if (ptype == "IPL1") {
				/* PC-98 IPL1 boot record search */
				std::vector<_PC98RawPartition> parts;
				int chosen_idx = (int)parts.size();

				/* store partitions into a vector, including extended partitions */
				LOG_MSG("FAT: Partition type is IPL1 (PC-98)");
				if (!PartitionLoadIPL1(parts,loadedDisk)) {
					LOG_MSG("Failed to read partition table");
					created_successfully = false;
					return;
				}

				LogPrintPartitionTable(parts);

				/* user knows best! */
				if (opt_partition_index >= 0)
					chosen_idx = opt_partition_index;
				else
					chosen_idx = PC98AutoChoose_FAT(parts,loadedDisk);

				if (chosen_idx < 0 || (size_t)chosen_idx >= parts.size()) {
					LOG_MSG("No partition chosen");
					created_successfully = false;
					return;
				}

				{ /* assume chosen_idx is in range */
					const _PC98RawPartition &pe = parts[(size_t)chosen_idx];

					/* unfortunately start and end are in C/H/S geometry, so we have to translate.
					 * this is why it matters so much to read the geometry from the HDI header.
					 *
					 * NOTE: C/H/S values in the IPL1 table are similar to IBM PC except that sectors are counted from 0, not 1 */
					startSector =
						(pe.cyl * loadedDisk->sectors * loadedDisk->heads) +
						(pe.head * loadedDisk->sectors) +
						pe.sector;

					/* Many HDI images I've encountered so far indicate 512 bytes/sector,
					 * but then the FAT filesystem itself indicates 1024 bytes per sector. */
					pc98_512_to_1024_allow = true;

					{
						/* FIXME: What if the label contains SHIFT-JIS? */
						std::string name = std::string(pe.name,sizeof(pe.name));

						LOG_MSG("Using IPL1 entry %lu name '%s' which starts at sector %lu",
							(unsigned long)chosen_idx,name.c_str(),(unsigned long)startSector);
					}

					partition_index = chosen_idx;
				}
				isipl1 = true;
			}
			else if (ptype == "MBR") {
				/* IBM PC master boot record search */
				std::vector<partTable::partentry_t> parts;
				int chosen_idx = (int)parts.size();

				/* store partitions into a vector, including extended partitions */
				LOG_MSG("FAT: Partition type is MBR (IBM PC)");
				if (!PartitionLoadMBR(parts,loadedDisk)) {
					LOG_MSG("Failed to read partition table");
					created_successfully = false;
					return;
				}

				LogPrintPartitionTable(parts);

				/* user knows best! */
				if (opt_partition_index >= 0) {
					chosen_idx = opt_partition_index;
				}
				else {
					chosen_idx = MBRAutoChoose_FAT(parts,loadedDisk);
					if (chosen_idx < 0) {
						/* If no chosen partition by default, but chosen partition if acting like later DOS version,
						 * then you need to bump the DOS version number to mount it. NTS: Exit this part with
						 * chosen_idx < 0 */
						if (MBRAutoChoose_FAT(parts,loadedDisk,7,0) >= 0 || MBRAutoChoose_FAT(parts,loadedDisk,7,10) >= 0) {
							LOG_MSG("Partitions are available to mount, but a higher DOS version is required");
						}
					}
				}

				if (chosen_idx < 0 || (size_t)chosen_idx >= parts.size()) {
					LOG_MSG("No partition chosen");
					created_successfully = false;
					return;
				}

				{ /* assume chosen_idx is in range */
					const partTable::partentry_t &pe = parts[(size_t)chosen_idx];

					partition_index = chosen_idx;
					startSector = pe.absSectStart;
					countSector = pe.partSize;
				}
			}
			else {
				LOG_MSG("Unknown partition type '%s'",ptype.c_str());
				created_successfully = false;
				return;
			}

			/* if the partition is already in use, do not mount.
			 * two drives using the same partition can cause FAT filesystem corruption! */
			if (loadedDisk->partitionInUse(partition_index)) {
				LOG_MSG("Partition %u already in use",partition_index);
				created_successfully = false;
				return;
			}
			loadedDisk->partitionMarkUse(partition_index,true);

			partSectOff = startSector;
			partSectSize = countSector;
		} else {
			if (headscyl > 0 && cylinders > 0 && cylsector > 0 && bytesector > 0) // manually set geometry if value specified
				loadedDisk->Set_Geometry(headscyl, cylinders,cylsector, bytesector);
			else
				/* Get floppy disk parameters based on image size */
				loadedDisk->Get_Geometry(&headscyl, &cylinders, &cylsector, &bytesector);
			/* Floppy disks don't have partitions */
			partSectOff = 0;

			if (loadedDisk->heads == 0 || loadedDisk->sectors == 0 || loadedDisk->cylinders == 0) {
				/* Get_Geometry fails for some floppies with weird geometries, so try obtaining the geometry from BPB in such case */
				LOG_MSG("drive_fat.cpp: No geometry, check your image. Try obtaining from BPB");
			}
		}

		BPB = {};
		loadedDisk->Read_AbsoluteSector(0 + partSectOff, &bootbuffer);

		/* If the sector is full of 0xF6, the partition is brand new and was just created with Microsoft FDISK.EXE (Windows 98 behavior)
		 * and therefore there is NO FAT filesystem here. We'll go farther and check if all bytes are just the same.
		 *
		 * MS-DOS behavior is to let the drive letter appear, but any attempt to access it results in errors like "invalid media type" until
		 * you run FORMAT.COM on it, which will then set up the filesystem and allow it to work. */
		if (partSectOff != 0) { /* hard drives only */
			unsigned int i=1;

			while (i < 128 && ((uint8_t*)(&bootbuffer))[0] == ((uint8_t*)(&bootbuffer))[i]) i++;

			if (i == 128) {
				LOG_MSG("Boot sector appears to have been created by FDISK.EXE but not formatted. Allowing mount but filesystem access will not work until formatted.");

				sector_size = loadedDisk->getSectSize();
				firstRootDirSect = 0;
				CountOfClusters = 0;
				firstDataSector = 0;
				unformatted = true;

				cwdDirCluster = 0;

				memset(fatSectBuffer,0,1024);
				curFatSect = 0xffffffff;

				strcpy(info, "fatDrive ");
				strcat(info, wpcolon&&strlen(sysFilename)>1&&sysFilename[0]==':'?sysFilename+1:sysFilename);

				/* NTS: Real MS-DOS behavior is that an unformatted partition is inaccessible but INT 21h AX=440Dh CX=0x0860 (Get Device Parameters)
				 *      has a ready-made BPB for use in formatting the partition. FORMAT.COM then formats the filesystem based on the MS-DOS kernel's
				 *      recommended BPB. It's not FORMAT.COM that decides the layout it is MS-DOS itself. */
				uint64_t part_sectors;

				if (partSectOff != 0 && partSectSize != 0) {
					part_sectors = partSectSize;
				}
				else {
					part_sectors = GetSectorCount();
				}

				/* If the partition is 1GB or larger and emulating MS-DOS 7.10 or higher, default FAT32 */
				if ((part_sectors*uint64_t(sector_size)) >= (1000ull*2048ull) && (dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10))) {
					BPB.v32.BPB_BytsPerSec = sector_size;
					BPB.v32.BPB_SecPerClus = 8; // 4096 byte clusters
					BPB.v32.BPB_RsvdSecCnt = 32;
					BPB.v32.BPB_NumFATs = 2;
					BPB.v32.BPB_RootEntCnt = 0;
					BPB.v32.BPB_TotSec16 = 0;
					BPB.v32.BPB_Media = 0xF8;
					BPB.v32.BPB_SecPerTrk = loadedDisk->sectors;
					BPB.v32.BPB_NumHeads = loadedDisk->heads;
					BPB.v32.BPB_HiddSec = partSectOff;
					BPB.v32.BPB_TotSec32 = part_sectors;
					BPB.v32.BPB_ExtFlags = 0;
					BPB.v32.BPB_FSVer = 0;
					BPB.v32.BPB_RootClus = 2;
					BPB.v32.BPB_FSInfo = 0;
					BPB.v32.BPB_BkBootSec = 0;

					while (BPB.v32.BPB_SecPerClus < 64 && (part_sectors / uint64_t(BPB.v32.BPB_SecPerClus)) > uint64_t(0x880000u))
						BPB.v32.BPB_SecPerClus *= 2u;

					/* Get size of root dir in sectors */
					uint32_t DataSectors;

					DataSectors = (Bitu)BPB.v32.BPB_TotSec32 - ((Bitu)BPB.v32.BPB_RsvdSecCnt + ((Bitu)BPB.v32.BPB_NumFATs * (Bitu)BPB.v32.BPB_FATSz32));
					CountOfClusters = DataSectors / BPB.v32.BPB_SecPerClus;

					BPB.v32.BPB_FATSz32 = uint32_t((CountOfClusters * 4ull) + uint64_t(sector_size) - 1ull) / uint64_t(sector_size);

					assert(CountOfClusters >= 0xFFF8);

					assert(BPB.is_fat32());
					fattype = FAT32;
				}
				else {
					BPB.v.BPB_BytsPerSec = sector_size;
					BPB.v.BPB_SecPerClus = 1; // 512 byte clusters
					BPB.v.BPB_RsvdSecCnt = 1;
					BPB.v.BPB_NumFATs = 2;
					BPB.v.BPB_RootEntCnt = 96;
					if (part_sectors > 65535ul) {
						BPB.v.BPB_TotSec16 = 0;
						BPB.v.BPB_TotSec32 = part_sectors;
					}
					else {
						BPB.v.BPB_TotSec16 = part_sectors;
						BPB.v.BPB_TotSec32 = 0;
					}
					BPB.v.BPB_Media = 0xF8;
					BPB.v.BPB_SecPerTrk = loadedDisk->sectors;
					BPB.v.BPB_NumHeads = loadedDisk->heads;
					BPB.v.BPB_HiddSec = partSectOff;

					if ((part_sectors*uint64_t(sector_size)) >= (31ull*1024ull*1024ull)) { // 31MB
						fattype = FAT16;
						while (BPB.v.BPB_SecPerClus < 32 && (part_sectors / uint64_t(BPB.v.BPB_SecPerClus)) > uint64_t(0x8800u))
							BPB.v.BPB_SecPerClus *= 2u;
					}
					else {
						fattype = FAT12;
						while (BPB.v.BPB_SecPerClus < 64 && (part_sectors / uint64_t(BPB.v.BPB_SecPerClus)) > uint64_t(0x880u))
							BPB.v.BPB_SecPerClus *= 2u;
					}

					/* Get size of root dir in sectors */
					uint32_t RootDirSectors;
					uint32_t DataSectors;

					RootDirSectors = ((BPB.v.BPB_RootEntCnt * 32u) + (BPB.v.BPB_BytsPerSec - 1u)) / BPB.v.BPB_BytsPerSec;

					if (BPB.v.BPB_TotSec16 != 0)
						DataSectors = (Bitu)BPB.v.BPB_TotSec16 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);
					else
						DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);

					CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;

					if (fattype == FAT16)
						BPB.v.BPB_FATSz16 = uint32_t((CountOfClusters * 2ull) + uint64_t(sector_size) - 1ull) / uint64_t(sector_size);
					else
						BPB.v.BPB_FATSz16 = uint32_t((((CountOfClusters+1ull)/2ull) * 3ull) + uint64_t(sector_size) - 1ull) / uint64_t(sector_size);

					if (BPB.v.BPB_TotSec16 != 0)
						DataSectors = (Bitu)BPB.v.BPB_TotSec16 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);
					else
						DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);
					CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;

					if (fattype == FAT12) {
						assert(CountOfClusters < 0xFF8);
					}
					else if (fattype == FAT16) {
						assert(CountOfClusters < 0xFFF8);
						assert(CountOfClusters >= 0xFF8);
					}

					assert(!BPB.is_fat32());
				}

				return;
			}
		}

		void* var = &bootbuffer.bpb.v.BPB_BytsPerSec;
		bootbuffer.bpb.v.BPB_BytsPerSec = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_RsvdSecCnt;
		bootbuffer.bpb.v.BPB_RsvdSecCnt = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_RootEntCnt;
		bootbuffer.bpb.v.BPB_RootEntCnt = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_TotSec16;
		bootbuffer.bpb.v.BPB_TotSec16 = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_FATSz16;
		bootbuffer.bpb.v.BPB_FATSz16 = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_SecPerTrk;
		bootbuffer.bpb.v.BPB_SecPerTrk = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_NumHeads;
		bootbuffer.bpb.v.BPB_NumHeads = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_HiddSec;
		bootbuffer.bpb.v.BPB_HiddSec = var_read((uint32_t*)var);
		var = &bootbuffer.bpb.v.BPB_TotSec32;
		bootbuffer.bpb.v.BPB_TotSec32 = var_read((uint32_t*)var);
		var = &bootbuffer.bpb.v.BPB_VolID;
		bootbuffer.bpb.v.BPB_VolID = var_read((uint32_t*)var);

		if(!is_hdd) {
			if (loadedDisk->heads == 0 || loadedDisk->sectors == 0 || loadedDisk->cylinders == 0 || loadedDisk->sector_size == 0) { // Set geometry from BPB
				headscyl = bootbuffer.bpb.v.BPB_NumHeads;
				cylsector = bootbuffer.bpb.v.BPB_SecPerTrk;
				bytesector = bootbuffer.bpb.v.BPB_BytsPerSec;
				if(headscyl == 0 || cylsector == 0 || bytesector == 0 || loadedDisk->diskSizeK == 0 || ((bytesector & (bytesector - 1)) != 0)/*not a power of 2*/){
					LOG_MSG("drive_fat.cpp: Illegal BPB value");
					if(!IS_PC98_ARCH){
						created_successfully = false;
						return;
					}
				}
				else {
					cylinders = loadedDisk->diskSizeK * 1024 / headscyl / cylsector / bytesector;
					loadedDisk->Set_Geometry(headscyl, cylinders, cylsector, bytesector);
					LOG_MSG("drive_fat.cpp: Floppy geometry set from BPB information SS/C/H/S = %u/%u/%u/%u",
							loadedDisk->sector_size,
							loadedDisk->cylinders,
							loadedDisk->heads,
							loadedDisk->sectors);
				}
			}
			/* Identify floppy format */
			if((bootbuffer.BS_jmpBoot[0] == 0x69 || bootbuffer.BS_jmpBoot[0] == 0xe9 ||
						(bootbuffer.BS_jmpBoot[0] == 0xeb && bootbuffer.BS_jmpBoot[2] == 0x90)) &&
					(bootbuffer.bpb.v.BPB_Media & 0xf0) == 0xf0) {
				/* DOS 2.x or later format, BPB assumed valid */

				if((bootbuffer.bpb.v.BPB_Media != 0xf0 && !(bootbuffer.bpb.v.BPB_Media & 0x1)) &&
						(bootbuffer.BS_OEMName[5] != '3' || bootbuffer.BS_OEMName[6] != '.' || bootbuffer.BS_OEMName[7] < '2')) {
					/* Fix pre-DOS 3.2 single-sided floppy */
					bootbuffer.bpb.v.BPB_SecPerClus = 1;
				}
			}
			else if(bootbuffer.BS_jmpBoot[0] == 0x60 && bootbuffer.BS_jmpBoot[1] == 0x1c) {
				LOG_MSG("Experimental: Detected Human68k v1.00 or v2.00 floppy disk. Assuming PC-98 2HD(1.25MB disk).");
				bootbuffer.bpb.v.BPB_BytsPerSec = 0x400;  //Offset 0x12,0x13
				bootbuffer.bpb.v.BPB_SecPerClus = 1;      //Offset 0x14
				bootbuffer.bpb.v.BPB_RsvdSecCnt = 1;      
				bootbuffer.bpb.v.BPB_NumFATs = 2;         //Offset 0x15?
				bootbuffer.bpb.v.BPB_RootEntCnt = 0xc0;   //Offset 0x18,0x19
				bootbuffer.bpb.v.BPB_TotSec16 = 0x4d0;    //Offset 0x1a,0x1b
				bootbuffer.bpb.v.BPB_Media = 0xfe;        //Offset 0x1c
				bootbuffer.bpb.v.BPB_FATSz16 = 2;         //Offset 0x1d?
				bootbuffer.bpb.v.BPB_SecPerTrk = 8;
				bootbuffer.bpb.v.BPB_NumHeads = 2;
				bootbuffer.magic1 = 0x55;	// to silence warning
				bootbuffer.magic2 = 0xaa;
			}
			else if(!IS_PC98_ARCH && loadedDisk->diskSizeK <= 360){
				/* Read media descriptor in FAT */
				uint8_t sectorBuffer[512];
				loadedDisk->Read_AbsoluteSector(1,&sectorBuffer);
				uint8_t mdesc = sectorBuffer[0];

				if (mdesc >= 0xf8) {
					/* DOS 1.x format, create BPB for 160kB floppy */
					bootbuffer.bpb.v.BPB_BytsPerSec = 512;
					bootbuffer.bpb.v.BPB_SecPerClus = 1;
					bootbuffer.bpb.v.BPB_RsvdSecCnt = 1;
					bootbuffer.bpb.v.BPB_NumFATs = 2;
					bootbuffer.bpb.v.BPB_RootEntCnt = 64;
					bootbuffer.bpb.v.BPB_TotSec16 = 320;
					bootbuffer.bpb.v.BPB_Media = mdesc;
					bootbuffer.bpb.v.BPB_FATSz16 = 1;
					bootbuffer.bpb.v.BPB_SecPerTrk = 8;
					bootbuffer.bpb.v.BPB_NumHeads = 1;
					bootbuffer.magic1 = 0x55;	// to silence warning
					bootbuffer.magic2 = 0xaa;
					if (!(mdesc & 0x2)) {
						/* Adjust for 9 sectors per track */
						bootbuffer.bpb.v.BPB_TotSec16 = 360;
						bootbuffer.bpb.v.BPB_FATSz16 = 2;
						bootbuffer.bpb.v.BPB_SecPerTrk = 9;
					}
					if (mdesc & 0x1) {
						/* Adjust for 2 sides */
						bootbuffer.bpb.v.BPB_SecPerClus = 2;
						bootbuffer.bpb.v.BPB_RootEntCnt = 112;
						bootbuffer.bpb.v.BPB_TotSec16 *= 2;
						bootbuffer.bpb.v.BPB_NumHeads = 2;
					}
				} else {
					/* Unknown format */
					created_successfully = false;
					return;
				}
			}
		}

		/* accept BPB.. so far */
		BPB = bootbuffer.bpb;

		/* DEBUG */
		LOG(LOG_DOSMISC,LOG_DEBUG)("FAT: BPB says %u sectors/track %u heads %u bytes/sector",
			BPB.v.BPB_SecPerTrk,
			BPB.v.BPB_NumHeads,
			BPB.v.BPB_BytsPerSec);

		/* NTS: PC-98 floppies (the 1024 byte/sector format) do not have magic bytes */
		if (fatDrive::getSectSize() == 512 && !IS_PC98_ARCH) {
			if ((bootbuffer.magic1 != 0x55) || (bootbuffer.magic2 != 0xaa)) {
				/* Not a FAT filesystem */
				LOG_MSG("Loaded image has no valid magicnumbers at the end!");
				created_successfully = false;
				return;
			}
		}
	}

	/* NTS: Some HDI images of PC-98 games do in fact have BPB_NumHeads == 0. Some like "Amaranth 5" have BPB_SecPerTrk == 0 too! */
	if (!IS_PC98_ARCH) {
		/* a clue that we're not really looking at FAT is invalid or weird values in the boot sector */
		if (BPB.v.BPB_SecPerTrk == 0 || (BPB.v.BPB_SecPerTrk > ((filesize <= 3000) ? 40 : 255)) ||
				(BPB.v.BPB_NumHeads > ((filesize <= 3000) ? 64 : 255))) {
			LOG_MSG("Rejecting image, boot sector has weird values not consistent with FAT filesystem");
			created_successfully = false;
			return;
		}
	}

	/* work at this point in logical sectors */
	sector_size = loadedDisk->getSectSize();

	/* Many HDI images indicate a disk format of 256 or 512 bytes per sector combined with a FAT filesystem
	 * that indicates 1024 bytes per sector. */
	if (pc98_512_to_1024_allow &&
		BPB.v.BPB_BytsPerSec != fatDrive::getSectSize() &&
		BPB.v.BPB_BytsPerSec >  fatDrive::getSectSize() &&
		(BPB.v.BPB_BytsPerSec %  fatDrive::getSectSize()) == 0) {
		unsigned int ratioshift = 1;

		while ((unsigned int)(BPB.v.BPB_BytsPerSec >> ratioshift) > fatDrive::getSectSize())
			ratioshift++;

		unsigned int ratio = 1u << ratioshift;

		LOG_MSG("Disk indicates %u bytes/sector, FAT filesystem indicates %u bytes/sector. Ratio=%u:1 shift=%u",
			fatDrive::getSectSize(),BPB.v.BPB_BytsPerSec,ratio,ratioshift);

		if ((unsigned int)(BPB.v.BPB_BytsPerSec >> ratioshift) == fatDrive::getSectSize()) {
			assert(ratio >= 2);

			/* the best case conversion is one where the starting sector is a multiple
			 * of the ratio, but there are enough PC-98 HDI images (Dragon Knight 4 reported by
			 * shiningforceforever) that don't fit that model, so we have to make do with a
			 * physical sector adjust too. */
			physToLogAdj = partSectOff & (ratio - 1);
			partSectOff >>= ratioshift;
			startSector >>= ratioshift;
			sector_size = BPB.v.BPB_BytsPerSec;
			LOG_MSG("Using logical sector size %u, offset by %u physical sectors",sector_size,physToLogAdj);
		}
	}

	/* Sanity checks */
	/* NTS: DOSBox-X *does* support non-standard sector sizes, though not in IBM PC mode and not through INT 13h.
	 *      In DOSBox-X INT 13h emulation will enforce the standard (512 byte) sector size.
	 *      In PC-98 mode mounting disk images requires "non-standard" sector sizes because PC-98 floppies (other
	 *      than ones formatted 1.44MB) generally use 1024 bytes/sector and MAY use 128 or 256 bytes per sector. */
	/* NTS: Loosen geometry checks for PC-98 mode, for two reasons. One, is that the geometry check will fail
	 *      when logical vs physical sector translation is involved, since it is apparently common for PC-98 HDI
	 *      images to be formatted with 256, 512, 1024, or in rare cases even 2048 bytes per sector, yet the FAT
	 *      file format will report a sector size that is a power of 2 multiple of the disk sector size. The
	 *      most common appears to be 512 byte/sector HDI images formatted with 1024 byte/sector FAT filesystems.
	 *
	 *      Second, there are some HDI images that are valid yet the FAT filesystem reports a head count of 0
	 *      for some reason (Touhou Project) */
	bool pc98fd_sanity_failed = false;
	if ((BPB.v.BPB_SecPerClus == 0) ||
		(BPB.v.BPB_NumFATs == 0) ||
		(BPB.v.BPB_NumHeads == 0 && !IS_PC98_ARCH) ||
		//(BPB.v.BPB_NumHeads > headscyl && !IS_PC98_ARCH) ||
		(BPB.v.BPB_SecPerTrk == 0 && !IS_PC98_ARCH) ||
		(cylsector != 0 && BPB.v.BPB_SecPerTrk > cylsector && !IS_PC98_ARCH)) {
		if (isipl1 && !IS_PC98_ARCH && (BPB.v.BPB_NumHeads == 0 || BPB.v.BPB_SecPerTrk == 0 || BPB.v.BPB_SecPerTrk > cylsector)) {
			const char *msg = "Please restart DOSBox-X in PC-98 mode to mount HDI disk images.\r\n";
			uint16_t n = (uint16_t)strlen(msg);
			DOS_WriteFile (STDOUT,(uint8_t *)msg, &n);
		}
		LOG_MSG("drive_fat.cpp: Sanity checks failed");
		if(!is_pc98fd){ // PC-98 replaces corrupted FDD BPB with correct values
			created_successfully = false;
			return;
		}
		else
			pc98fd_sanity_failed = true;
	}

	/* Sanity check: Root directory count is nonzero if FAT16/FAT12, or is zero if FAT32 */
	if (BPB.is_fat32()) {
		if (BPB.v.BPB_RootEntCnt != 0) {
			LOG_MSG("drive_fat.cpp: Sanity check fail: Root directory count != 0 and FAT32");
			created_successfully = false;
			return;
		}
	}
	else {
		if (BPB.v.BPB_RootEntCnt == 0) {
			LOG_MSG("drive_fat.cpp: Sanity check fail: Root directory count == 0 and not FAT32");
			if(!is_pc98fd){
				created_successfully = false;
				return;
			}
			else
				pc98fd_sanity_failed = true;
		}
	}

	/* too much of this code assumes 512 bytes per sector or more.
	 * MS-DOS itself as I understand it relies on bytes per sector being a power of 2.
	 * this is to protect against errant FAT structures and to help prep this code
	 * later to work with the 1024 bytes/sector used by PC-98 floppy formats.
	 * When done, this code should be able to then handle the FDI/FDD images
	 * PC-98 games are normally distributed in on the internet.
	 *
	 * The value "128" comes from the smallest sector size possible on the floppy
	 * controller of MS-DOS based systems. */
	/* NTS: Power of 2 test: A number is a power of 2 if (x & (x - 1)) == 0
	 *
	 * 15        15 & 14       01111 AND 01110     RESULT: 01110 (15)
	 * 16        16 & 15       10000 AND 01111     RESULT: 00000 (0)
	 * 17        17 & 16       10001 AND 10000     RESULT: 10000 (16) */
	if (BPB.v.BPB_BytsPerSec < 128 || BPB.v.BPB_BytsPerSec > SECTOR_SIZE_MAX ||
		(BPB.v.BPB_BytsPerSec & (BPB.v.BPB_BytsPerSec - 1)) != 0/*not a power of 2*/
		|| pc98fd_sanity_failed) {
		if(is_pc98fd && ((bytesector == 1024)||(bytesector == 512))) {
			LOG_MSG("Experimental: PC-98 autodetect BPB parameters when it is corrupted");
			BPB.v.BPB_BytsPerSec = bytesector;
			BPB.v.BPB_SecPerClus = 1;
			BPB.v.BPB_RsvdSecCnt = 1;
			BPB.v.BPB_NumFATs = 2;
			BPB.v.BPB_SecPerTrk = cylsector;
			BPB.v.BPB_NumHeads = headscyl;
			if((bytesector == 1024) && (cylsector == 8) && (cylinders == 77)) { // PC-98 2HD 1.25MB
				BPB.v.BPB_RootEntCnt = 0xc0;
				BPB.v.BPB_TotSec16 = 0x4d0;
				BPB.v.BPB_FATSz16 = 2;
				BPB.v.BPB_Media = 0xfe;
			}
			else if((bytesector == 512) && (cylsector == 15) && (cylinders == 80)) { // PC-98 2HC 1.21MB
				BPB.v.BPB_RootEntCnt = 0xe0;
				BPB.v.BPB_TotSec16 = 0x960;
				BPB.v.BPB_FATSz16 = 7;
				BPB.v.BPB_Media = 0xf9;
			}
			else if((bytesector == 512) && (cylsector == 18) && (cylinders == 80)) { // PC-98 2HD 1.44MB
				BPB.v.BPB_RootEntCnt = 0xe0;
				BPB.v.BPB_TotSec16 = 0xb40;
				BPB.v.BPB_FATSz16 = 9;
				BPB.v.BPB_Media = 0xf0;
			}
			else if((bytesector == 512) && (cylsector == 8) && (cylinders == 80)) { // PC-98 2DD 640kB
				BPB.v.BPB_RootEntCnt = 0x70;
				BPB.v.BPB_TotSec16 = 0x500;
				BPB.v.BPB_FATSz16 = 2;
				BPB.v.BPB_Media = 0xfb;
			}
			else if((bytesector == 512) && (cylsector == 9) && (cylinders == 80)) { // PC-98 2DD 720kB
				BPB.v.BPB_RootEntCnt = 0x70;
				BPB.v.BPB_TotSec16 = 0x5a0;
				BPB.v.BPB_FATSz16 = 3;
				BPB.v.BPB_Media = 0xf9;
			}
			else {
				LOG_MSG("Experimental: PC-98 assuming BPB parameters to be 1.25MB 2HD floppy, this may not work.");
				BPB.v.BPB_RootEntCnt = 0xc0;
				BPB.v.BPB_TotSec16 = 0x4d0;
				BPB.v.BPB_Media = 0xfe;
				BPB.v.BPB_FATSz16 = 2;
			}
		}
		else {
			LOG_MSG("FAT bytes/sector value %u not supported", BPB.v.BPB_BytsPerSec);
			created_successfully = false;
			return;
		}
	}

	/* another fault of this code is that it assumes the sector size of the medium matches
	 * the BPB_BytsPerSec value of the MS-DOS filesystem. if they don't match, problems
	 * will result. */
	if (BPB.v.BPB_BytsPerSec != fatDrive::getSectSize()) {
		LOG_MSG("FAT bytes/sector %u does not match disk image bytes/sector %u",
			(unsigned int)BPB.v.BPB_BytsPerSec,
			(unsigned int)fatDrive::getSectSize());
		created_successfully = false;
		return;
	}

	/* Filesystem must be contiguous to use absolute sectors, otherwise CHS will be used */
	absolute = IS_PC98_ARCH || ((BPB.v.BPB_NumHeads == headscyl) && (BPB.v.BPB_SecPerTrk == cylsector));
	LOG(LOG_DOSMISC,LOG_DEBUG)("FAT driver: Using %s sector access",absolute ? "absolute" : "C/H/S");

	/* Determine FAT format, 12, 16 or 32 */

	/* Get size of root dir in sectors */
	uint32_t RootDirSectors;
	uint32_t DataSectors;

	if (BPB.is_fat32()) {
		/* FAT32 requires use of TotSec32, TotSec16 must be zero. */
		if (BPB.v.BPB_TotSec32 == 0) {
			LOG_MSG("BPB_TotSec32 == 0 and FAT32 BPB, not valid");
			created_successfully = false;
			return;
		}
		if (BPB.v32.BPB_RootClus < 2) {
			LOG_MSG("BPB_RootClus == 0 and FAT32 BPB, not valid");
			created_successfully = false;
			return;
		}
		if (BPB.v.BPB_FATSz16 != 0) {
			LOG_MSG("BPB_FATSz16 != 0 and FAT32 BPB, not valid");
			created_successfully = false;
			return;
		}
		if (BPB.v32.BPB_FATSz32 == 0) {
			LOG_MSG("BPB_FATSz32 == 0 and FAT32 BPB, not valid");
			created_successfully = false;
			return;
		}

		RootDirSectors = 0; /* FAT32 root directory has its own allocation chain, instead of a fixed location */
		DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v32.BPB_FATSz32) + (Bitu)RootDirSectors);
		CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;
		firstDataSector = ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v32.BPB_FATSz32) + (Bitu)RootDirSectors) + (Bitu)partSectOff;
		firstRootDirSect = 0;
	}
	else {
		if (BPB.v.BPB_FATSz16 == 0) {
			LOG_MSG("BPB_FATSz16 == 0 and not FAT32 BPB, not valid");
			created_successfully = false;
			return;
		}

		RootDirSectors = ((BPB.v.BPB_RootEntCnt * 32u) + (BPB.v.BPB_BytsPerSec - 1u)) / BPB.v.BPB_BytsPerSec;

		if (BPB.v.BPB_TotSec16 != 0)
			DataSectors = (Bitu)BPB.v.BPB_TotSec16 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);
		else
			DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);

		CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;
		firstDataSector = ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors) + (Bitu)partSectOff;
		firstRootDirSect = (Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)partSectOff;

		LOG(LOG_MISC,LOG_DEBUG)("INIT FAT: data=%llu root=%llu rootdirsect=%lu datasect=%lu",
			(unsigned long long)firstDataSector,(unsigned long long)firstRootDirSect,
			(unsigned long)RootDirSectors,(unsigned long)DataSectors);
	}

	if(CountOfClusters < 4085) {
		/* Volume is FAT12 */
		LOG_MSG("Mounted FAT volume is FAT12 with %d clusters", CountOfClusters);
		fattype = FAT12;
	} else if (CountOfClusters < 65525) {
		LOG_MSG("Mounted FAT volume is FAT16 with %d clusters", CountOfClusters);
		fattype = FAT16;
	} else {
		LOG_MSG("Mounted FAT volume is FAT32 with %d clusters", CountOfClusters);
		fattype = FAT32;
	}

	/* just so you know....! */
	if (fattype == FAT32 && (dos.version.major < 7 || (dos.version.major == 7 && dos.version.minor < 10))) {
		LOG_MSG("CAUTION: Mounting FAT32 partition when reported DOS version is less than 7.10. Disk formatting/repair utilities may mis-identify the partition.");
	}

	/* There is no cluster 0, this means we are in the root directory */
	cwdDirCluster = 0;

	memset(fatSectBuffer,0,1024);
	curFatSect = 0xffffffff;

	strcpy(info, "fatDrive ");
	strcat(info, wpcolon&&strlen(sysFilename)>1&&sysFilename[0]==':'?sysFilename+1:sysFilename);
}

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

bool fatDrive::AllocationInfo32(uint32_t * _bytes_sector,uint32_t * _sectors_cluster,uint32_t * _total_clusters,uint32_t * _free_clusters) {
	uint32_t countFree = 0;
	uint32_t i;

	if (unformatted) return false;

	for(i=0;i<CountOfClusters;i++) {
		if(!getClusterValue(i+2))
			countFree++;
	}

	*_bytes_sector = getSectSize();
	*_sectors_cluster = BPB.v.BPB_SecPerClus;
	*_total_clusters = CountOfClusters;
	*_free_clusters = countFree;

	return true;
}

bool fatDrive::AllocationInfo(uint16_t *_bytes_sector, uint8_t *_sectors_cluster, uint16_t *_total_clusters, uint16_t *_free_clusters) {
	if (unformatted) return false;

	if (BPB.is_fat32()) {
		uint32_t bytes32,sectors32,clusters32,free32;
		if (AllocationInfo32(&bytes32,&sectors32,&clusters32,&free32) &&
			DOS_CommonFAT32FAT16DiskSpaceConv(_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,bytes32,sectors32,clusters32,free32))
			return true;

		return false;
	}
	else {
		uint32_t countFree = 0;
		uint32_t i;

		for(i=0;i<CountOfClusters;i++) {
			if(!getClusterValue(i+2))
				countFree++;
		}

		/* FAT12/FAT16 should never allow more than 0xFFF6 clusters and partitions larger than 2GB */
		*_bytes_sector = (uint16_t)getSectSize();
		*_sectors_cluster = BPB.v.BPB_SecPerClus;
		*_total_clusters = (uint16_t)MIN(CountOfClusters,0xFFFFu);
		*_free_clusters = (uint16_t)MIN(countFree,0xFFFFu);
	}

	return true;
}

uint32_t fatDrive::getFirstFreeClust(void) {
	uint32_t i;

	if (unformatted) return 0;

	for(i=searchFreeCluster;i<CountOfClusters;i++) {
		if(!getClusterValue(i+2)) return ((searchFreeCluster=i)+2);
	}
	for(i=0;i<CountOfClusters;i++) {
		if(!getClusterValue(i+2)) return ((searchFreeCluster=i)+2);
	}

	/* No free cluster found */
	searchFreeCluster = 0;
	return 0;
}

bool fatDrive::isRemote(void) {	return false; }
bool fatDrive::isRemovable(void) { return false; }

Bits fatDrive::UnMount(void) {
	delete this;
	return 0;
}

uint8_t fatDrive::GetMediaByte(void) { return BPB.v.BPB_Media; }
const FAT_BootSector::bpb_union_t &fatDrive::GetBPB(void) { return BPB; }

void fatDrive::SetBPB(const FAT_BootSector::bpb_union_t &bpb) {
	if (readonly) return;
	unformatted = false;
	BPB.v.BPB_BytsPerSec = bpb.v.BPB_BytsPerSec;
	BPB.v.BPB_SecPerClus = bpb.v.BPB_SecPerClus;
	BPB.v.BPB_RsvdSecCnt = bpb.v.BPB_RsvdSecCnt;
	BPB.v.BPB_NumFATs = bpb.v.BPB_NumFATs;
	BPB.v.BPB_RootEntCnt = bpb.v.BPB_RootEntCnt;
	BPB.v.BPB_TotSec16 = bpb.v.BPB_TotSec16;
	BPB.v.BPB_Media = bpb.v.BPB_Media;
	BPB.v.BPB_FATSz16 = bpb.v.BPB_FATSz16;
	BPB.v.BPB_SecPerTrk = bpb.v.BPB_SecPerTrk;
	BPB.v.BPB_NumHeads = bpb.v.BPB_NumHeads;
	BPB.v.BPB_HiddSec = bpb.v.BPB_HiddSec;
	BPB.v.BPB_TotSec32 = bpb.v.BPB_TotSec32;
	if (!bpb.is_fat32() && (bpb.v.BPB_BootSig == 0x28 || bpb.v.BPB_BootSig == 0x29))
		BPB.v.BPB_VolID = bpb.v.BPB_VolID;
	if (bpb.is_fat32() && (bpb.v32.BS_BootSig == 0x28 || bpb.v32.BS_BootSig == 0x29))
		BPB.v32.BS_VolID = bpb.v32.BS_VolID;
	if (bpb.is_fat32()) {
		BPB.v32.BPB_BytsPerSec = bpb.v32.BPB_BytsPerSec;
		BPB.v32.BPB_SecPerClus = bpb.v32.BPB_SecPerClus;
		BPB.v32.BPB_RsvdSecCnt = bpb.v32.BPB_RsvdSecCnt;
		BPB.v32.BPB_NumFATs = bpb.v32.BPB_NumFATs;
		BPB.v32.BPB_RootEntCnt = bpb.v32.BPB_RootEntCnt;
		BPB.v32.BPB_TotSec16 = bpb.v32.BPB_TotSec16;
		BPB.v32.BPB_Media = bpb.v32.BPB_Media;
		BPB.v32.BPB_FATSz32 = bpb.v32.BPB_FATSz32;
		BPB.v32.BPB_SecPerTrk = bpb.v32.BPB_SecPerTrk;
		BPB.v32.BPB_NumHeads = bpb.v32.BPB_NumHeads;
		BPB.v32.BPB_HiddSec = bpb.v32.BPB_HiddSec;
		BPB.v32.BPB_TotSec32 = bpb.v32.BPB_TotSec32;
		BPB.v32.BPB_FATSz32 = bpb.v32.BPB_FATSz32;
		BPB.v32.BPB_ExtFlags = bpb.v32.BPB_ExtFlags;
		BPB.v32.BPB_FSVer = bpb.v32.BPB_FSVer;
		BPB.v32.BPB_RootClus = bpb.v32.BPB_RootClus;
		BPB.v32.BPB_FSInfo = bpb.v32.BPB_FSInfo;
		BPB.v32.BPB_BkBootSec = bpb.v32.BPB_BkBootSec;
	}

	FAT_BootSector bootbuffer = {};
	loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);
	if (BPB.is_fat32()) bootbuffer.bpb.v32=BPB.v32;
	bootbuffer.bpb.v=BPB.v;
	loadedDisk->Write_AbsoluteSector(0+partSectOff,&bootbuffer);

	/* Get size of root dir in sectors */
	uint32_t RootDirSectors;
	uint32_t DataSectors;

	if (BPB.is_fat32()) {
		RootDirSectors = 0; /* FAT32 root directory has its own allocation chain, instead of a fixed location */
		DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v32.BPB_FATSz32) + (Bitu)RootDirSectors);
		CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;
		firstDataSector = ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v32.BPB_FATSz32) + (Bitu)RootDirSectors) + (Bitu)partSectOff;
		firstRootDirSect = 0;
	}
	else {
		RootDirSectors = ((BPB.v.BPB_RootEntCnt * 32u) + (BPB.v.BPB_BytsPerSec - 1u)) / BPB.v.BPB_BytsPerSec;

		if (BPB.v.BPB_TotSec16 != 0)
			DataSectors = (Bitu)BPB.v.BPB_TotSec16 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);
		else
			DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);

		CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;
		firstDataSector = ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors) + (Bitu)partSectOff;
		firstRootDirSect = (Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)partSectOff;
	}

	if(CountOfClusters < 4085) {
		/* Volume is FAT12 */
		LOG_MSG("Mounted FAT volume is now FAT12 with %d clusters", CountOfClusters);
		fattype = FAT12;
	} else if (CountOfClusters < 65525 || !bpb.is_fat32()) {
		LOG_MSG("Mounted FAT volume is now FAT16 with %d clusters", CountOfClusters);
		fattype = FAT16;
	} else {
		LOG_MSG("Mounted FAT volume is now FAT32 with %d clusters", CountOfClusters);
		fattype = FAT32;
	}
}

bool fatDrive::FileCreate(DOS_File **file, const char *name, uint16_t attributes) {
	const char *lfn = NULL;

	if (unformatted) return false;

	checkDiskChange();

    if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
    direntry fileEntry = {};
	uint32_t dirClust, subEntry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11], path[DOS_PATHLENGTH];

	uint16_t save_errorcode=dos.errorcode;

	if (attributes & DOS_ATTR_VOLUME) {
		SetLabel(name,false,true);
		return true;
	}
	if (attributes & DOS_ATTR_DIRECTORY) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* you cannot create root directory */
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* Check if file already exists */
	if(getFileDirEntry(name, &fileEntry, &dirClust, &subEntry, true/*dirOk*/)) {
		/* You can't create/truncate a directory! */
		if (fileEntry.attrib & DOS_ATTR_DIRECTORY) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}

		/* Truncate file allocation chain */
		{
			const uint32_t chk = BPB.is_fat32() ? fileEntry.Cluster32() : fileEntry.loFirstClust;
			if(chk != 0) deleteClustChain(chk, 0);
		}
		/* Update directory entry */
		fileEntry.entrysize=0;
		fileEntry.SetCluster32(0);
		directoryChange(dirClust, &fileEntry, (int32_t)subEntry);
	} else {
		/* Can we even get the name of the file itself? */
		if(!getEntryName(name, &dirName[0])||!strlen(trim(dirName))) return false;
		convToDirFile(&dirName[0], &pathName[0]);

		/* Can we find the base directory? */
		if(!getDirClustNum(name, &dirClust, true)) return false;

		/* NTS: "name" is the full relative path. For LFN creation to work we need only the final element of the path */
		if (uselfn && !force_sfn) {
			lfn = strrchr_dbcs((char *)name,'\\');

			if (lfn != NULL) {
				lfn++; /* step past '\' */
				strcpy(path, name);
				*(strrchr_dbcs(path,'\\')+1)=0;
			} else {
				lfn = name; /* no path elements */
				*path=0;
			}

			if (filename_not_strict_8x3(lfn)) {
				char *sfn=Generate_SFN(path, lfn);
				if (sfn!=NULL) convToDirFile(sfn, &pathName[0]);
			} else
				lfn = NULL;
		}

		memset(&fileEntry, 0, sizeof(direntry));
		memcpy(&fileEntry.entryname, &pathName[0], 11);
        {
            uint16_t ct,cd;
            time_t_to_DOS_DateTime(/*&*/ct,/*&*/cd,time(NULL));
            fileEntry.modTime = ct;
            fileEntry.modDate = cd;
        }
        fileEntry.attrib = (uint8_t)(attributes & 0xff);
		addDirectoryEntry(dirClust, fileEntry, lfn);

		/* Check if file exists now */
		if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;
	}

	/* Empty file created, now lets open it */
	/* TODO: check for read-only flag and requested write access */
	*file = new fatFile(name, BPB.is_fat32() ? fileEntry.Cluster32() : fileEntry.loFirstClust, fileEntry.entrysize, this);
	(*file)->flags=OPEN_READWRITE;
	((fatFile *)(*file))->dirCluster = dirClust;
	((fatFile *)(*file))->dirIndex = subEntry;
	/* Maybe modTime and date should be used ? (crt matches findnext) */
	((fatFile *)(*file))->time = fileEntry.modTime;
	((fatFile *)(*file))->date = fileEntry.modDate;

	dos.errorcode=save_errorcode;
	return true;
}

bool fatDrive::FileExists(const char *name) {
	if (unformatted) return false;

	checkDiskChange();

    direntry fileEntry = {};
	uint32_t dummy1, dummy2;
	uint16_t save_errorcode = dos.errorcode;
	bool found = getFileDirEntry(name, &fileEntry, &dummy1, &dummy2);
	dos.errorcode = save_errorcode;
	return found;
}

bool fatDrive::FileOpen(DOS_File **file, const char *name, uint32_t flags) {
	if (unformatted) return false;

	checkDiskChange();

    direntry fileEntry = {};
	uint32_t dirClust, subEntry;

	/* you cannot open root directory */
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;
	/* TODO: check for read-only flag and requested write access */
	*file = new fatFile(name, BPB.is_fat32() ? fileEntry.Cluster32() : fileEntry.loFirstClust, fileEntry.entrysize, this);
    (*file)->SetName(name);
	(*file)->flags = flags;
	((fatFile *)(*file))->dirCluster = dirClust;
	((fatFile *)(*file))->dirIndex = subEntry;
	/* Maybe modTime and date should be used ? (crt matches findnext) */
	((fatFile *)(*file))->time = fileEntry.modTime;
	((fatFile *)(*file))->date = fileEntry.modDate;
	return true;
}

bool fatDrive::FileStat(const char * /*name*/, FileStat_Block *const /*stat_block*/) {
	/* TODO: Stub */
	return false;
}

bool fatDrive::FileUnlink(const char * name) {
	if (unformatted) return false;

	checkDiskChange();

    if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
    direntry tmpentry = {};
    direntry fileEntry = {};
	uint32_t dirClust, subEntry;

	/* you cannot delete root directory */
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	lfnRange.clear();
	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false; /* Do not use dirOk, DOS should never call this unless a file */
	lfnRange_t dir_lfn_range = lfnRange; /* copy down LFN results before they are obliterated by the next call to FindNextInternal. */

	/* delete LFNs */
	if (!dir_lfn_range.empty() && (dos.version.major >= 7 || uselfn)) {
		/* last LFN entry should be fileidx */
		assert(dir_lfn_range.dirPos_start < dir_lfn_range.dirPos_end);
		if (dir_lfn_range.dirPos_end != subEntry) LOG_MSG("FAT warning: LFN dirPos_end=%u fileidx=%u (mismatch)",dir_lfn_range.dirPos_end,subEntry);
		for (unsigned int didx=dir_lfn_range.dirPos_start;didx < dir_lfn_range.dirPos_end;didx++) {
			if (directoryBrowse(dirClust,&tmpentry,didx)) {
				tmpentry.entryname[0] = 0xe5;
				directoryChange(dirClust,&tmpentry,didx);
			}
		}
	}

	/* remove primary 8.3 SFN */
	fileEntry.entryname[0] = 0xe5;
	directoryChange(dirClust, &fileEntry, (int32_t)subEntry);

	/* delete allocation chain */
	{
		const uint32_t chk = BPB.is_fat32() ? fileEntry.Cluster32() : fileEntry.loFirstClust;
		if(chk != 0) deleteClustChain(chk, 0);
	}

	if(getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;

	return true;
}

bool fatDrive::FindFirst(const char *_dir, DOS_DTA &dta,bool fcb_findfirst) {
	if (unformatted) return false;

	checkDiskChange();

    direntry dummyClust = {};

    // volume label searches always affect root directory, no matter the current directory, at least with FCBs
    if (dta.GetAttr() == DOS_ATTR_VOLUME || ((dta.GetAttr() & DOS_ATTR_VOLUME) && (fcb_findfirst || !(_dir && *_dir && dta.GetAttr() == 0x3F)))) {
        if(!getDirClustNum("\\", &cwdDirCluster, false)) {
            DOS_SetError(DOSERR_PATH_NOT_FOUND);
            return false;
        }
    }
    else {
        if(!getDirClustNum(_dir, &cwdDirCluster, false)) {
            DOS_SetError(DOSERR_PATH_NOT_FOUND);
            return false;
        }
    }

	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) {
		dta.SetDirID(0);
		dta.SetDirIDCluster(cwdDirCluster);
	} else {
		dpos[lfn_filefind_handle]=0;
		dnum[lfn_filefind_handle]=cwdDirCluster;
	}

	return FindNextInternal(cwdDirCluster, dta, &dummyClust);
}

char* removeTrailingSpaces(char* str) {
	char* end = str + strlen(str) - 1;
	while (end >= str && *end == ' ') end--;
    /* NTS: The loop will exit with 'end' one char behind the last ' ' space character.
     *      So to ASCIIZ snip off the space, step forward one and overwrite with NUL.
     *      The loop may end with 'end' one char behind 'ptr' if the string was empty ""
     *      or nothing but spaces. This is OK because after the step forward, end >= str
     *      in all cases. */
	*(++end) = '\0';
	return str;
}

char* removeLeadingSpaces(char* str) {
	size_t len = strlen(str);
	size_t pos = strspn(str," ");
	memmove(str,str + pos,len - pos + 1);
	return str;
}

char* trimString(char* str) {
	return removeTrailingSpaces(removeLeadingSpaces(str));
}

uint32_t fatDrive::GetSectorCount(void) {
    return (loadedDisk->heads * loadedDisk->sectors * loadedDisk->cylinders) - partSectOff;
}

uint32_t fatDrive::GetSectorSize(void) {
    return getSectorSize();
}

uint8_t fatDrive::Read_AbsoluteSector_INT25(uint32_t sectnum, void * data) {
    return readSector(sectnum+partSectOff,data);
}

uint8_t fatDrive::Write_AbsoluteSector_INT25(uint32_t sectnum, void * data) {
    return writeSector(sectnum+partSectOff,data);
}

static void copyDirEntry(const direntry *src, direntry *dst) {
	memcpy(dst, src, 14); // single byte fields
    void* var = &dst->crtTime;
	var_write((uint16_t*)var, src->crtTime);
    var = &dst->crtDate;
	var_write((uint16_t*)var, src->crtDate);
    var = &dst->accessDate;
	var_write((uint16_t*)var, src->accessDate);
    var = &dst->hiFirstClust;
	var_write((uint16_t*)var, src->hiFirstClust);
    var = &dst->modTime;
	var_write((uint16_t*)var, src->modTime);
    var = &dst->modDate;
	var_write((uint16_t*)var, src->modDate);
    var = &dst->loFirstClust;
	var_write((uint16_t*)var, src->loFirstClust);
    var = &dst->entrysize;
	var_write((uint32_t*)var, src->entrysize);
}

bool fatDrive::FindNextInternal(uint32_t dirClustNumber, DOS_DTA &dta, direntry *foundEntry) {
	if (unformatted) return false;

	direntry sectbuf[MAX_DIRENTS_PER_SECTOR]; /* 16 directory entries per 512 byte sector */
	uint32_t logentsector; /* Logical entry sector */
	uint32_t entryoffset;  /* Index offset within sector */
	uint32_t tmpsector;
	uint8_t attrs;
	uint16_t dirPos;
	char srch_pattern[CROSS_LEN];
	char find_name[DOS_NAMELENGTH_ASCII];
	char lfind_name[LFN_NAMELENGTH+1];
	unsigned int lfn_max_ord = 0;
	unsigned char lfn_checksum = 0;
	bool lfn_ord_found[0x40];
	char extension[4];

    size_t dirent_per_sector = getSectSize() / sizeof(direntry);
    assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
    assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	dta.GetSearchParams(attrs, srch_pattern,false);
	dirPos = lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():dpos[lfn_filefind_handle]; /* NTS: Windows 9x is said to have a 65536 dirent limit even for FAT32, so dirPos as 16-bit is acceptable */

	memset(lfind_name,0,LFN_NAMELENGTH);
	lfnRange.clear();

nextfile:
	logentsector = (uint32_t)((size_t)dirPos / dirent_per_sector);
	entryoffset = (uint32_t)((size_t)dirPos % dirent_per_sector);

	if(dirClustNumber==0) {
        if (BPB.is_fat32()) return false;

		if(dirPos >= BPB.v.BPB_RootEntCnt) {
			if (lfn_filefind_handle<LFN_FILEFIND_MAX) {
				dpos[lfn_filefind_handle]=0;
				dnum[lfn_filefind_handle]=0;
			}
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		readSector(firstRootDirSect+logentsector,sectbuf);
	} else {
		tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
		/* A zero sector number can't happen */
		if(tmpsector == 0) {
			if (lfn_filefind_handle<LFN_FILEFIND_MAX) {
				dpos[lfn_filefind_handle]=0;
				dnum[lfn_filefind_handle]=0;
			}
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		readSector(tmpsector,sectbuf);
	}
	dirPos++;
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) dta.SetDirID(dirPos);
	else dpos[lfn_filefind_handle]=dirPos;

    /* Deleted file entry */
    if (sectbuf[entryoffset].entryname[0] == 0xe5) {
        lfind_name[0] = 0; /* LFN code will memset() it in full upon next dirent */
        lfn_max_ord = 0;
        lfnRange.clear();
        goto nextfile;
    }

	/* End of directory list */
	if (sectbuf[entryoffset].entryname[0] == 0x00) {
			if (lfn_filefind_handle<LFN_FILEFIND_MAX) {
				dpos[lfn_filefind_handle]=0;
				dnum[lfn_filefind_handle]=0;
			}
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	memset(find_name,0,DOS_NAMELENGTH_ASCII);
	memset(extension,0,4);

    if (sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)
        memcpy(find_name, &sectbuf[entryoffset].entryname[0], 11);
    else
    {
        memcpy(find_name, &sectbuf[entryoffset].entryname[0], 8);
        memcpy(extension, &sectbuf[entryoffset].entryname[8], 3);
    }

	// recover the SFN initial E5, which was converted to 05
	// to distinguish with a free directory entry
	if (find_name[0] == 0x05) find_name[0] = 0xe5;

    if (!(sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)) {
        trimString(&find_name[0]);
        trimString(&extension[0]);
    }

	if (extension[0]!=0) {
		strcat(find_name, ".");
		strcat(find_name, extension);
	}

    /* Compare attributes to search attributes */

    //TODO What about attrs = DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY ?
	if (attrs == DOS_ATTR_VOLUME) {
		if (dos.version.major >= 7 || uselfn) {
			/* skip LFN entries */
			if ((sectbuf[entryoffset].attrib & 0x3F) == 0x0F)
				goto nextfile;
		}

		if (!(sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)) goto nextfile;
		labelCache.SetLabel(find_name, false, true);
	} else if ((dos.version.major >= 7 || uselfn) && (sectbuf[entryoffset].attrib & 0x3F) == 0x0F) { /* long filename piece */
		struct direntry_lfn *dlfn = (struct direntry_lfn*)(&sectbuf[entryoffset]);

		/* assume last entry comes first, because that's how Windows 9x does it and that is how you're supposed to do it according to Microsoft */
		if (dlfn->LDIR_Ord & 0x40) {
			lfn_max_ord = (dlfn->LDIR_Ord & 0x3F); /* NTS: First entry has ordinal 1, this is the HIGHEST ordinal in the LFN. The other entries follow in descending ordinal. */
			for (unsigned int i=0;i < 0x40;i++) lfn_ord_found[i] = false;
			lfn_checksum = dlfn->LDIR_Chksum;
			memset(lfind_name,0,LFN_NAMELENGTH);
			lfnRange.clear();
			lfnRange.dirPos_start = dirPos - 1; /* NTS: The code above has already incremented dirPos */
		}

		if (lfn_max_ord != 0 && (dlfn->LDIR_Ord & 0x3F) > 0 && (dlfn->LDIR_Ord & 0x3Fu) <= lfn_max_ord && dlfn->LDIR_Chksum == lfn_checksum) {
			unsigned int oidx = (dlfn->LDIR_Ord & 0x3Fu) - 1u;
			unsigned int stridx = oidx * 13u, len = 0;
			uint16_t lchar = 0;
			char lname[27] = {0};
            char text[10];
            uint16_t uname[4];

            for (unsigned int i=0;i < 5;i++) {
                text[0] = text[1] = text[2] = 0;
                lchar = (uint16_t)(dlfn->LDIR_Name1[i]);
                if (lchar < 0x100 || lchar == 0xFFFF)
                    lname[len++] = lchar != 0xFFFF && CodePageHostToGuestUTF16(text,&lchar) && text[0] && !text[1] ? text[0] : (char)(lchar & 0xFF);
                else {
                    uname[0]=lchar;
                    uname[1]=0;
                    if (CodePageHostToGuestUTF16(text,uname)) {
                        lname[len++] = (char)(text[0] & 0xFF);
                        lname[len++] = (char)(text[1] & 0xFF);
                    } else
                        lname[len++] = '_';
                }
            }
            for (unsigned int i=0;i < 6;i++) {
                text[0] = text[1] = text[2] = 0;
                lchar = (uint16_t)(dlfn->LDIR_Name2[i]);
                if (lchar < 0x100 || lchar == 0xFFFF)
                    lname[len++] = lchar != 0xFFFF && CodePageHostToGuestUTF16(text,&lchar) && text[0] && !text[1] ? text[0] : (char)(lchar & 0xFF);
                else {
                    char text[10];
                    uint16_t uname[4];
                    uname[0]=lchar;
                    uname[1]=0;
                    text[0] = 0;
                    text[1] = 0;
                    text[2] = 0;
                    if (CodePageHostToGuestUTF16(text,uname)) {
                        lname[len++] = (char)(text[0] & 0xFF);
                        lname[len++] = (char)(text[1] & 0xFF);
                    } else
                        lname[len++] = '_';
                }
            }
            for (unsigned int i=0;i < 2;i++) {
                text[0] = text[1] = text[2] = 0;
                lchar = (uint16_t)(dlfn->LDIR_Name3[i]);
                if (lchar < 0x100 || lchar == 0xFFFF)
                    lname[len++] = lchar != 0xFFFF && CodePageHostToGuestUTF16(text,&lchar) && text[0] && !text[1] ? text[0] : (char)(lchar & 0xFF);
                else {
                    char text[10];
                    uint16_t uname[4];
                    uname[0]=lchar;
                    uname[1]=0;
                    text[0] = 0;
                    text[1] = 0;
                    text[2] = 0;
                    if (CodePageHostToGuestUTF16(text,uname)) {
                        lname[len++] = (char)(text[0] & 0xFF);
                        lname[len++] = (char)(text[1] & 0xFF);
                    } else
                        lname[len++] = '_';
                }
            }
            lname[len] = 0;
            if ((stridx+len) <= LFN_NAMELENGTH) {
                std::string full = std::string(lname) + std::string(lfind_name);
                strcpy(lfind_name, full.c_str());
                lfn_ord_found[oidx] = true;
            }
		}

		goto nextfile;
	} else {
        if (~attrs & sectbuf[entryoffset].attrib & (DOS_ATTR_DIRECTORY | DOS_ATTR_VOLUME) ) {
            lfind_name[0] = 0; /* LFN code will memset() it in full upon next dirent */
            lfn_max_ord = 0;
            lfnRange.clear();
            goto nextfile;
        }
	}

	if (lfn_max_ord != 0) {
		bool ok = false;
		unsigned int complete = 0;
		for (unsigned int i=0;i < lfn_max_ord;i++) complete += lfn_ord_found[i]?1:0;

		if (complete == lfn_max_ord) {
			unsigned char chk = 0;
			for (unsigned int i=0;i < 11;i++) {
				chk = ((chk & 1u) ? 0x80u : 0x00u) + (chk >> 1u) + sectbuf[entryoffset].entryname[i];
			}

			if (lfn_checksum == chk) {
				lfnRange.dirPos_end = dirPos - 1; /* NTS: The code above has already incremented dirPos */
				ok = true;
			}
		}

		if (!ok) {
			lfind_name[0] = 0; /* LFN code will memset() it in full upon next dirent */
			lfn_max_ord = 0;
			lfnRange.clear();
		}
	}
	else {
		lfind_name[0] = 0; /* LFN code will memset() it in full upon next dirent */
		lfn_max_ord = 0;
		lfnRange.clear();
	}

	/* Compare name to search pattern. Skip long filename match if no long filename given. */
    if(attrs == DOS_ATTR_VOLUME) {
        if (!(wild_match(find_name, srch_pattern)))
            goto nextfile;
    }
	else if (!(WildFileCmp(find_name,srch_pattern) || (lfn_max_ord != 0 && lfind_name[0] != 0 && LWildFileCmp(lfind_name,srch_pattern)))) {
		lfind_name[0] = 0; /* LFN code will memset() it in full upon next dirent */
		lfn_max_ord = 0;
		lfnRange.clear();
		goto nextfile;
	}

    if(sectbuf[entryoffset].attrib == DOS_ATTR_VOLUME)
        trimString(find_name);

    // Drive emulation does not need to require a LFN in case there is no corresponding 8.3 names.
    if (lfind_name[0] == 0) strcpy(lfind_name,find_name);

	copyDirEntry(&sectbuf[entryoffset], foundEntry);

	//dta.SetResult(find_name, foundEntry->entrysize, foundEntry->crtDate, foundEntry->crtTime, foundEntry->attrib);

	dta.SetResult(find_name, lfind_name, foundEntry->entrysize, 0, foundEntry->modDate, foundEntry->modTime, foundEntry->attrib);

	return true;
}

bool fatDrive::FindNext(DOS_DTA &dta) {
	if (unformatted) return false;

    direntry dummyClust = {};

	return FindNextInternal(lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirIDCluster():(dnum[lfn_filefind_handle]?dnum[lfn_filefind_handle]:0), dta, &dummyClust);
}


bool fatDrive::SetFileAttr(const char *name, uint16_t attr) {
	if (unformatted) return false;

	checkDiskChange();

    if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
    direntry fileEntry = {};
	uint32_t dirClust, subEntry;

	/* you cannot set file attr root directory (right?) */
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry, /*dirOk*/true)) {
		return false;
	} else {
		fileEntry.attrib=(uint8_t)attr;
		directoryChange(dirClust, &fileEntry, (int32_t)subEntry);
	}
	return true;
}

bool fatDrive::GetFileAttr(const char *name, uint16_t *attr) {
	if (unformatted) return false;

	checkDiskChange();

    direntry fileEntry = {};
	uint32_t dirClust, subEntry;

	/* you CAN get file attr root directory */
	if (*name == 0) {
		*attr=DOS_ATTR_DIRECTORY;
		return true;
	}

	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry, /*dirOk*/true)) {
		return false;
	} else *attr=fileEntry.attrib;
	return true;
}

bool fatDrive::GetFileAttrEx(char* name, struct stat *status) {
    (void)name;
    (void)status;
	return false;
}

unsigned long fatDrive::GetCompressedSize(char* name) {
    (void)name;
	return 0;
}

#if defined (WIN32)
HANDLE fatDrive::CreateOpenFile(const char* name) {
    (void)name;
	DOS_SetError(1);
	return INVALID_HANDLE_VALUE;
}
#endif

unsigned long fatDrive::GetSerial() {
	if (BPB.is_fat32())
		return BPB.v32.BS_VolID?BPB.v32.BS_VolID:0x1234;
	else
		return BPB.v.BPB_VolID?BPB.v.BPB_VolID:0x1234;
}

bool fatDrive::directoryBrowse(uint32_t dirClustNumber, direntry *useEntry, int32_t entNum, int32_t start/*=0*/) {
	if (unformatted) return false;

	direntry sectbuf[MAX_DIRENTS_PER_SECTOR];	/* 16 directory entries per 512 byte sector */
	uint32_t tmpsector;

	(void)start;//UNUSED

	const size_t dirent_per_sector = getSectSize() / sizeof(direntry);
	assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
	assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	/* NTS: This change provides a massive performance boost for the FAT driver. The previous code, inherited
	 *      from SVN, would read every directory entry in sequence up to entNum. It would also re-read the
	 *      sector every dirent even if the same sector, and it called getAbsoluteSectFromChain() every single
	 *      time. Which means that for every directory entry this was asked to scan (and called a LOT especially
	 *      in a directory with more than 100 files!) this code would read the sector, re-read the allocation
	 *      chain up to the desired offset, and do it entNum times! No wonder it was so slow! --J.C. 2023/04/11 */
	const uint16_t dirPos = (uint16_t)entNum;
	const uint32_t logentsector = ((uint32_t)((size_t)dirPos / dirent_per_sector)); /* Logical entry sector */
	const uint32_t entryoffset = ((uint32_t)((size_t)dirPos % dirent_per_sector));

	if(dirClustNumber==0) {
		assert(!BPB.is_fat32());
		if(dirPos >= BPB.v.BPB_RootEntCnt) return false;
		tmpsector = firstRootDirSect+logentsector;
	} else {
		tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
		if(tmpsector == 0) return false;
	}

	readSector(tmpsector,sectbuf);
	if (sectbuf[entryoffset].entryname[0] == 0x00) return false; /* End of directory list? */
	copyDirEntry(&sectbuf[entryoffset], useEntry);
	return true;
}

bool fatDrive::directoryChange(uint32_t dirClustNumber, const direntry *useEntry, int32_t entNum) {
	if (unformatted) return false;

	direntry sectbuf[MAX_DIRENTS_PER_SECTOR];	/* 16 directory entries per 512 byte sector */
	uint32_t tmpsector = 0;

	const size_t dirent_per_sector = getSectSize() / sizeof(direntry);
	assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
	assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	/* NTS: This change provides a massive performance boost for the FAT driver. The previous code, inherited
	 *      from SVN, would read every directory entry in sequence up to entNum. It would also re-read the
	 *      sector every dirent even if the same sector, and it called getAbsoluteSectFromChain() every single
	 *      time. Which means that for every directory entry this was asked to scan (and called a LOT especially
	 *      in a directory with more than 100 files!) this code would read the sector, re-read the allocation
	 *      chain up to the desired offset, and do it entNum times! No wonder it was so slow! --J.C. 2023/04/11 */
	const uint16_t dirPos = (uint16_t)entNum;
	const uint32_t logentsector = ((uint32_t)((size_t)dirPos / dirent_per_sector)); /* Logical entry sector */
	const uint32_t entryoffset = ((uint32_t)((size_t)dirPos % dirent_per_sector));

	if(dirClustNumber==0) {
		assert(!BPB.is_fat32());
		if(dirPos >= BPB.v.BPB_RootEntCnt) return false;
		tmpsector = firstRootDirSect+logentsector;
	} else {
		tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
		if(tmpsector == 0) return false;
	}

	readSector(tmpsector,sectbuf);
	if (sectbuf[entryoffset].entryname[0] == 0x00) return false; /* End of directory list? */
	copyDirEntry(useEntry, &sectbuf[entryoffset]);
	writeSector(tmpsector, sectbuf);
	return true;
}

bool fatDrive::addDirectoryEntry(uint32_t dirClustNumber, const direntry& useEntry,const char *lfn) {
	if (unformatted) return false;

	direntry sectbuf[MAX_DIRENTS_PER_SECTOR]; /* 16 directory entries per 512 byte sector */
	uint32_t tmpsector;
	uint16_t dirPos = 0;
	unsigned int need = 1;
	unsigned int found = 0;
	uint16_t dirPosFound = 0;

	unsigned int len = 0;
	uint16_t lfnw[LFN_NAMELENGTH+13] = {0};
	if (lfn != NULL && *lfn != 0) {
		/* 13 characters per LFN entry */
		bool lead = false;
        char text[3];
        uint16_t uname[4];
        for (const char *scan = lfn; *scan; scan++) {
            if (lead) {
                lead = false;
                text[0]=*(scan-1)&0xFF;
                text[1]=*scan&0xFF;
                text[2]=0;
                uname[0]=0;
                uname[1]=0;
                if (CodePageGuestToHostUTF16(uname,text)&&uname[0]!=0&&uname[1]==0) {
                    lfnw[len++] = uname[0];
                } else {
                    lfnw[len++] = *(scan-1)&0xFF;
                    if (len < LFN_NAMELENGTH) lfnw[len++] = *scan&0xFF;
                }
            } else if (*(scan+1) && ((IS_PC98_ARCH && shiftjis_lead_byte(*scan&0xFF)) || (isDBCSCP() && isKanji1_gbk(*scan&0xFF)))) lead = true;
            else if (dos.loaded_codepage != 437) {
                text[0]=*scan&0xFF;
                text[1]=0;
                lfnw[len++] = CodePageGuestToHostUTF16(uname,text)&&uname[0]!=0&&uname[1]==0 ? uname[0] : (uint16_t)((unsigned char)(*scan));
            } else
                lfnw[len++] = (uint16_t)((unsigned char)(*scan));
        }
        lfnw[len] = 0;
        need = (unsigned int)(1 + (len + 12) / 13); /*round up*/;
	}

	size_t dirent_per_sector = getSectSize() / sizeof(direntry);
	assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
	assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	for(;;) {		
		uint32_t logentsector = ((uint32_t)((size_t)dirPos / dirent_per_sector)); /* Logical entry sector */
		uint32_t entryoffset = ((uint32_t)((size_t)dirPos % dirent_per_sector)); /* Index offset within sector */

		if(dirClustNumber==0) {
			assert(!BPB.is_fat32());
			if(dirPos >= BPB.v.BPB_RootEntCnt) return false;
			tmpsector = firstRootDirSect+logentsector;
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen - we need to allocate more room for this directory*/
			if(tmpsector == 0) {
				uint32_t newClust;
				newClust = appendCluster(dirClustNumber);
				if(newClust == 0) return false;
				zeroOutCluster(newClust);
				/* Try again to get tmpsector */
				tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
				if(tmpsector == 0) return false; /* Give up if still can't get more room for directory */
			}
		}
		readSector(tmpsector,sectbuf);

		/* Deleted file entry or end of directory list */
		if ((sectbuf[entryoffset].entryname[0] == 0xe5) || (sectbuf[entryoffset].entryname[0] == 0x00)) {
			if (found == 0) dirPosFound = dirPos;

			if ((++found) >= need) {
				copyDirEntry(&useEntry, &sectbuf[entryoffset]);
				writeSector(tmpsector,sectbuf);

				/* Add LFN entries */
				if (need != 1/*LFN*/) {
					uint16_t lfnbuf[LFN_NAMELENGTH+13]; /* on disk, LFNs are WCHAR unicode (UCS-16) */

					assert(lfn != NULL && len != 0);

					unsigned int o = 0;
					const uint16_t *scan = lfnw;

					while (*scan) {
						if (o >= LFN_NAMELENGTH) return false; /* Nope! */
						lfnbuf[o++] = (uint16_t)(*scan++);
					}

					/* on disk, LFNs are padded with 0x0000 followed by a run of 0xFFFF to fill the dirent */
					lfnbuf[o++] = 0x0000;
					for (unsigned int i=0;i < 13;i++) lfnbuf[o++] = 0xFFFF;
					assert(o <= (LFN_NAMELENGTH+13));

					unsigned char chk = 0;
					for (unsigned int i=0;i < 11;i++) {
						chk = ((chk & 1u) ? 0x80u : 0x00u) + (chk >> 1u) + useEntry.entryname[i];
					}

					dirPos = dirPosFound;
					for (unsigned int s=0;s < (need-1u);s++) {
						unsigned int lfnsrci = (need-2u-s);
						unsigned int lfnsrc = lfnsrci * 13;

						logentsector = ((uint32_t)((size_t)dirPos / dirent_per_sector)); /* Logical entry sector */
						entryoffset = ((uint32_t)((size_t)dirPos % dirent_per_sector)); /* Index offset within sector */

						if(dirClustNumber==0) {
							assert(!BPB.is_fat32());
							if(dirPos >= BPB.v.BPB_RootEntCnt) return false;
							tmpsector = firstRootDirSect+logentsector;
						} else {
							tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
							/* A zero sector number can't happen - we need to allocate more room for this directory*/
							if(tmpsector == 0) return false;
						}
						readSector(tmpsector,sectbuf);

						direntry_lfn *dlfn = (direntry_lfn*)(&sectbuf[entryoffset]);

						memset(dlfn,0,sizeof(*dlfn));

						dlfn->LDIR_Ord = (s == 0 ? 0x40 : 0x00) + lfnsrci + 1;
						dlfn->LDIR_Chksum = chk;
						dlfn->attrib = 0x0F;

						for (unsigned int i=0;i < 5;i++) dlfn->LDIR_Name1[i] = lfnbuf[lfnsrc++];
						for (unsigned int i=0;i < 6;i++) dlfn->LDIR_Name2[i] = lfnbuf[lfnsrc++];
						for (unsigned int i=0;i < 2;i++) dlfn->LDIR_Name3[i] = lfnbuf[lfnsrc++];

						writeSector(tmpsector,sectbuf);
						dirPos++;
					}
				}

				break;
			}
		}
		else {
			found = 0;
		}

		dirPos++;
	}

	return true;
}

void fatDrive::zeroOutCluster(uint32_t clustNumber) {
	uint8_t secBuffer[SECTOR_SIZE_MAX];

	memset(&secBuffer[0], 0, SECTOR_SIZE_MAX);

	unsigned int i;
	for(i=0;i<BPB.v.BPB_SecPerClus;i++) {
		writeSector(getAbsoluteSectFromChain(clustNumber,i), &secBuffer[0]);
	}
}

bool fatDrive::MakeDir(const char *dir) {
	if (unformatted) return false;

	checkDiskChange();

	const char *lfn = NULL;

    if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
	uint32_t dummyClust, dirClust, subEntry;
	direntry tmpentry;
	char dirName[DOS_NAMELENGTH_ASCII];
    char pathName[11], path[DOS_PATHLENGTH];
    uint16_t ct,cd;

	/* you cannot mkdir root directory */
	if (*dir == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* Can we even get the name of the directory itself? */
	if(!getEntryName(dir, &dirName[0])||!strlen(trim(dirName))) return false;
	convToDirFile(&dirName[0], &pathName[0]);

	/* Fail to make directory if something of that name already exists */
	if(getFileDirEntry(dir,&tmpentry,&dummyClust,&subEntry,/*dirOk*/true)) return false;

	/* Can we find the base directory? */
	if(!getDirClustNum(dir, &dirClust, true)) return false;

	dummyClust = getFirstFreeClust();
	/* No more space */
	if(dummyClust == 0) return false;

	if(!allocateCluster(dummyClust, 0)) return false;

	/* NTS: "dir" is the full relative path. For LFN creation to work we need only the final element of the path */
	if (uselfn && !force_sfn) {
		lfn = strrchr_dbcs((char *)dir,'\\');

		if (lfn != NULL) {
			lfn++; /* step past '\' */
			strcpy(path, dir);
			*(strrchr_dbcs(path,'\\')+1)=0;
		} else {
			lfn = dir; /* no path elements */
			*path=0;
		}

		if (filename_not_strict_8x3(lfn)) {
			char *sfn=Generate_SFN(path, lfn);
			if (sfn!=NULL) convToDirFile(sfn, &pathName[0]);
		} else
			lfn = NULL;
	}

	zeroOutCluster(dummyClust);

	time_t_to_DOS_DateTime(/*&*/ct,/*&*/cd,::time(NULL));

	/* Add the new directory to the base directory */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, &pathName[0], 11);
	tmpentry.loFirstClust = (uint16_t)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dummyClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
    tmpentry.modTime = ct;
    tmpentry.modDate = cd;
    addDirectoryEntry(dirClust, tmpentry, lfn);

	/* Add the [.] and [..] entries to our new directory*/
	/* [.] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, ".          ", 11);
	tmpentry.loFirstClust = (uint16_t)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dummyClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
    tmpentry.modTime = ct;
    tmpentry.modDate = cd;
	addDirectoryEntry(dummyClust, tmpentry);

	/* [..] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, "..         ", 11);
	if (BPB.is_fat32() && dirClust == BPB.v32.BPB_RootClus) {
		/* Windows 98 SCANDISK.EXE considers it an error for the '..' entry of a top level
		 * directory to point at the actual cluster number of the root directory. The
		 * correct value is 0 apparently. */
		tmpentry.loFirstClust = (uint16_t)0;
		tmpentry.hiFirstClust = (uint16_t)0;
	}
	else {
		tmpentry.loFirstClust = (uint16_t)(dirClust & 0xffff);
		tmpentry.hiFirstClust = (uint16_t)(dirClust >> 16);
	}
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
    tmpentry.modTime = ct;
    tmpentry.modDate = cd;
	addDirectoryEntry(dummyClust, tmpentry);
	//if(!getDirClustNum(dir, &dummyClust, false)) return false;

	return true;
}

bool fatDrive::RemoveDir(const char *dir) {
	if (unformatted) return false;

	checkDiskChange();

	if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
		return false;
	}
	uint32_t dummyClust, dirClust, subEntry;
	direntry tmpentry = {};
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11];

	/* you cannot rmdir root directory */
	if (*dir == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* Can we even get the name of the directory itself? */
	if(!getEntryName(dir, &dirName[0])||!strlen(trim(dirName))) return false;
	convToDirFile(&dirName[0], &pathName[0]);

	/* directory must exist */
	lfnRange.clear();
	if(!getFileDirEntry(dir,&tmpentry,&dirClust,&subEntry,/*dirOk*/true)) return false; /* dirClust is parent dir of directory */
	if (!(tmpentry.attrib & DOS_ATTR_DIRECTORY)) return false;
	dummyClust = (BPB.is_fat32() ? tmpentry.Cluster32() : tmpentry.loFirstClust);
	lfnRange_t dir_lfn_range = lfnRange; /* copy down LFN results before they are obliterated by the next call to FindNextInternal. */

	/* Can't remove root directory */
	if(dummyClust == 0) return false;
	if(BPB.is_fat32() && dummyClust==BPB.v32.BPB_RootClus) return false;

	/* Check to make sure directory is empty */
	/* NTS: The code below only cares if there are non-deleted files, not *how many*, therefore
	 *      the loop will now terminate immediately upon finding one. --J.C. 2023/04/11 */
	uint32_t filecount = 0;
	/* Set to 2 to skip first 2 entries, [.] and [..] */
	int32_t fileidx = 2;
	while(directoryBrowse(dummyClust, &tmpentry, fileidx)) {
		/* Check for non-deleted files. NTS: directoryBrowse() will return false if entryname[0] == 0 */
		if(tmpentry.entryname[0] != 0xe5) { filecount++; break; }
		fileidx++;
	}

	/* Return if directory is not empty */
	if(filecount > 0) return false;

	/* delete LFNs */
	if (!dir_lfn_range.empty() && (dos.version.major >= 7 || uselfn)) {
		/* last LFN entry should be fileidx */
		assert(dir_lfn_range.dirPos_start < dir_lfn_range.dirPos_end);
		if (dir_lfn_range.dirPos_end != subEntry) LOG_MSG("FAT warning: LFN dirPos_end=%u fileidx=%u (mismatch)",dir_lfn_range.dirPos_end,subEntry);
		for (unsigned int didx=dir_lfn_range.dirPos_start;didx < dir_lfn_range.dirPos_end;didx++) {
			if (directoryBrowse(dirClust,&tmpentry,didx)) {
				tmpentry.entryname[0] = 0xe5;
				directoryChange(dirClust,&tmpentry,didx);
			}
		}
	}

	/* remove primary 8.3 entry */
	if (!directoryBrowse(dirClust, &tmpentry, subEntry)) return false;
	tmpentry.entryname[0] = 0xe5;
	if (!directoryChange(dirClust, &tmpentry, subEntry)) return false;

	/* delete allocation chain */
	deleteClustChain(dummyClust, 0);
	return true;
}

bool fatDrive::Rename(const char * oldname, const char * newname) {
	if (unformatted) return false;

	checkDiskChange();

	const char *lfn = NULL;

    if (readonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	/* you cannot rename root directory */
	if (*oldname == 0 || *newname == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

    direntry fileEntry1 = {}, fileEntry2 = {};
	uint32_t dirClust1, subEntry1, dirClust2, subEntry2;
	char dirName2[DOS_NAMELENGTH_ASCII];
	char pathName2[11], path[DOS_PATHLENGTH];
	lfnRange_t dir_lfn_range;

	/* Check that old name exists (file or directory) */
	lfnRange.clear();
	if(!getFileDirEntry(oldname, &fileEntry1, &dirClust1, &subEntry1, /*dirOk*/true)) return false;
	dir_lfn_range = lfnRange;

	/* Check if new name (file or directory) already exists, fail if so */
	if(getFileDirEntry(newname, &fileEntry2, &dirClust2, &subEntry2, /*dirOk*/true)&&!(uselfn&&!force_sfn&&strcmp(oldname, newname)&&!strcasecmp(oldname, newname))) return false;

	/* Can we even get the name of the file itself? */
	if(!getEntryName(newname, &dirName2[0])||!strlen(trim(dirName2))) return false;
	convToDirFile(&dirName2[0], &pathName2[0]);

	/* Can we find the base directory of the new name? (we know the parent dir of oldname in dirClust1) */
	if(!getDirClustNum(newname, &dirClust2, true)) return false;

	/* NTS: "newname" is the full relative path. For LFN creation to work we need only the final element of the path */
	if (uselfn && !force_sfn) {
		lfn = strrchr_dbcs((char *)newname,'\\');

		if (lfn != NULL) {
			lfn++; /* step past '\' */
			strcpy(path, newname);
			*(strrchr_dbcs(path,'\\')+1)=0;
		} else {
			lfn = newname; /* no path elements */
			*path=0;
		}

		if (filename_not_strict_8x3(lfn)) {
			char oldchar=fileEntry1.entryname[0];
			fileEntry1.entryname[0] = 0xe5;
			directoryChange(dirClust1, &fileEntry1, (int32_t)subEntry1);
			char *sfn=Generate_SFN(path, lfn);
			if (sfn!=NULL) convToDirFile(sfn, &pathName2[0]);
			fileEntry1.entryname[0] = oldchar;
			directoryChange(dirClust1, &fileEntry1, (int32_t)subEntry1);
		} else
			lfn = NULL;
	}

	/* add new dirent */
	memcpy(&fileEntry2, &fileEntry1, sizeof(direntry));
	memcpy(&fileEntry2.entryname, &pathName2[0], 11);
	addDirectoryEntry(dirClust2, fileEntry2, lfn);

	/* Remove old 8.3 SFN entry */
	fileEntry1.entryname[0] = 0xe5;
	directoryChange(dirClust1, &fileEntry1, (int32_t)subEntry1);

	/* remove LFNs of old entry only if emulating LFNs or DOS version 7.0.
	 * Earlier DOS versions ignore LFNs. */
	if (!dir_lfn_range.empty() && (dos.version.major >= 7 || uselfn)) {
		/* last LFN entry should be fileidx */
		assert(dir_lfn_range.dirPos_start < dir_lfn_range.dirPos_end);
		if (dir_lfn_range.dirPos_end != subEntry1) LOG_MSG("FAT warning: LFN dirPos_end=%u fileidx=%u (mismatch)",dir_lfn_range.dirPos_end,subEntry1);
		for (unsigned int didx=dir_lfn_range.dirPos_start;didx < dir_lfn_range.dirPos_end;didx++) {
			if (directoryBrowse(dirClust1,&fileEntry1,didx)) {
				fileEntry1.entryname[0] = 0xe5;
				directoryChange(dirClust1,&fileEntry1,didx);
			}
		}
	}

	return true;
}

bool fatDrive::TestDir(const char *dir) {
	if (unformatted) return false;

	uint32_t dummyClust;

	/* root directory is directory */
	if (*dir == 0) return true;

	return getDirClustNum(dir, &dummyClust, false);
}

uint32_t fatDrive::GetPartitionOffset(void) {
	if (unformatted) return 0;
	return partSectOff;
}

uint32_t fatDrive::GetFirstClusterOffset(void) {
	if (unformatted) return 0;
    return firstDataSector - partSectOff;
}

uint32_t fatDrive::GetHighestClusterNumber(void) {
	if (unformatted) return 0;
    return CountOfClusters + 1ul;
}

void fatDrive::clusterChainMemory::clear(void) {
	current_cluster_no = 0;
	current_cluster_index = 0;
}

void fatDrive::checkDiskChange(void) {
	bool chg = false;

	/* Hack for "Bliss" by DeathStar (1995).
	 * The demo runs A:\GO.EXE, but the floppy disk doesn't actually exist, it's brought into
	 * existence by intercepting INT 13h for floppy I/O and then expecting MS-DOS to call INT 13h
	 * to read it. Furthermore, at some parts of the demo, the INT 13h hook changes the root
	 * directory and FAT table to make the next "disk" appear, even if the volume label does not. */
	if (loadedDisk->detectDiskChange() && !BPB.is_fat32() && partSectOff == 0) {
		LOG(LOG_MISC,LOG_DEBUG)("FAT: disk change");

		FAT_BootSector bootbuffer = {};
		loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);

		void* var = &bootbuffer.bpb.v.BPB_BytsPerSec;
		bootbuffer.bpb.v.BPB_BytsPerSec = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_RsvdSecCnt;
		bootbuffer.bpb.v.BPB_RsvdSecCnt = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_RootEntCnt;
		bootbuffer.bpb.v.BPB_RootEntCnt = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_TotSec16;
		bootbuffer.bpb.v.BPB_TotSec16 = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_FATSz16;
		bootbuffer.bpb.v.BPB_FATSz16 = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_SecPerTrk;
		bootbuffer.bpb.v.BPB_SecPerTrk = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_NumHeads;
		bootbuffer.bpb.v.BPB_NumHeads = var_read((uint16_t*)var);
		var = &bootbuffer.bpb.v.BPB_HiddSec;
		bootbuffer.bpb.v.BPB_HiddSec = var_read((uint32_t*)var);
		var = &bootbuffer.bpb.v.BPB_TotSec32;
		bootbuffer.bpb.v.BPB_TotSec32 = var_read((uint32_t*)var);
		var = &bootbuffer.bpb.v.BPB_VolID;
		bootbuffer.bpb.v.BPB_VolID = var_read((uint32_t*)var);

		BPB = bootbuffer.bpb;

		if (BPB.v.BPB_FATSz16 == 0) {
			LOG_MSG("BPB_FATSz16 == 0 and not FAT32 BPB, not valid");
			return;
		}

		/* sometimes as part of INT 13h interception the new image will differ from the format we mounted against (DS_BLISS.EXE) */
		/* TODO: Add any other format here */
		if (BPB.v.BPB_Media == 0xF9) {
			LOG(LOG_MISC,LOG_DEBUG)("NEW FAT: changing floppy geometry to 720k format according to media byte");
			loadedDisk->Set_Geometry(2/*heads*/,80/*cylinders*/,9/*sectors*/,512/*sector size*/);
		}
		else if (BPB.v.BPB_Media == 0xF0) {
			/* TODO: By the time DOS supported the 1.44MB format the BPB also provided the geometry too */
			LOG(LOG_MISC,LOG_DEBUG)("NEW FAT: changing floppy geometry to 1.44M format according to media byte");
			loadedDisk->Set_Geometry(2/*heads*/,80/*cylinders*/,18/*sectors*/,512/*sector size*/);
		}

		uint32_t RootDirSectors;
		uint32_t DataSectors;

		RootDirSectors = ((BPB.v.BPB_RootEntCnt * 32u) + (BPB.v.BPB_BytsPerSec - 1u)) / BPB.v.BPB_BytsPerSec;

		if (BPB.v.BPB_TotSec16 != 0)
			DataSectors = (Bitu)BPB.v.BPB_TotSec16 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);
		else
			DataSectors = (Bitu)BPB.v.BPB_TotSec32 - ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors);

		CountOfClusters = DataSectors / BPB.v.BPB_SecPerClus;
		firstDataSector = ((Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)RootDirSectors) + (Bitu)partSectOff;
		firstRootDirSect = (Bitu)BPB.v.BPB_RsvdSecCnt + ((Bitu)BPB.v.BPB_NumFATs * (Bitu)BPB.v.BPB_FATSz16) + (Bitu)partSectOff;

		cwdDirCluster = 0;

		memset(fatSectBuffer,0,1024);
		curFatSect = 0xffffffff;

		LOG(LOG_MISC,LOG_DEBUG)("NEW FAT: data=%llu root=%llu rootdirsect=%lu datasect=%lu",
			(unsigned long long)firstDataSector,(unsigned long long)firstRootDirSect,
			(unsigned long)RootDirSectors,(unsigned long)DataSectors);
	}
}

