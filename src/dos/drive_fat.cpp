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

#define IMGTYPE_FLOPPY 0
#define IMGTYPE_ISO    1
#define IMGTYPE_HDD	   2

#define FAT12		   0
#define FAT16		   1
#define FAT32		   2

static uint16_t dpos[256];
static uint32_t dnum[256];
extern bool wpcolon, force_sfn;
extern int lfn_filefind_handle;
void dos_ver_menu(bool start);
bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);

bool filename_not_8x3(const char *n) {
	unsigned int i;

	i = 0;
	while (*n != 0) {
		if (*n == '.') break;
		if (*n<=32||*n==127||*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||*n=='['||*n==']'||*n=='|'||*n=='?'||*n=='*') return true;
		i++;
		n++;
	}
	if (i > 8) return true;
	if (*n == 0) return false; /* made it past 8 or less normal chars and end of string: normal */

	/* skip dot */
	assert(*n == '.');
	n++;

	i = 0;
	while (*n != 0) {
		if (*n == '.') return true; /* another '.' means LFN */
		if (*n<=32||*n==127||*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||*n=='['||*n==']'||*n=='|'||*n=='?'||*n=='*') return true;
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
	for (unsigned int i=0; i<strlen(n); i++)
		if (n[i]>='a' && n[i]<='z')
			return true;
	return false; /* it is strict 8.3 upper case */
}

char sfn[DOS_NAMELENGTH_ASCII];
/* Generate 8.3 names from LFNs, with tilde usage (from ~1 to ~9999). */
char* fatDrive::Generate_SFN(const char *path, const char *name) {
	if (!filename_not_8x3(name)) {
		strcpy(sfn, name);
		upcase(sfn);
		return sfn;
	}
	char lfn[LFN_NAMELENGTH+1], fullname[DOS_PATHLENGTH+DOS_NAMELENGTH_ASCII], *n;
	if (name==NULL||!*name) return NULL;
	if (strlen(name)>LFN_NAMELENGTH) {
		strncpy(lfn, name, LFN_NAMELENGTH);
		lfn[LFN_NAMELENGTH]=0;
	} else
		strcpy(lfn, name);
	if (!strlen(lfn)) return NULL;
	direntry fileEntry = {};
	uint32_t dirClust, subEntry;
	unsigned int k=1, i, t=10000;
	while (k<10000) {
		n=lfn;
		if (t>strlen(n)||k==1||k==10||k==100||k==1000) {
			i=0;
			*sfn=0;
			while (*n == '.'||*n == ' ') n++;
			while (strlen(n)&&(*(n+strlen(n)-1)=='.'||*(n+strlen(n)-1)==' ')) *(n+strlen(n)-1)=0;
			while (*n != 0 && *n != '.' && i<(k<10?6u:(k<100?5u:(k<1000?4:3u)))) {
				if (*n == ' ') {
					n++;
					continue;
				}
				if (*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||*n=='['||*n==']'||*n=='|'||*n=='?'||*n=='*') {
					sfn[i++]='_';
					n++;
				} else
					sfn[i++]=toupper(*(n++));
			}
			sfn[i++]='~';
			t=i;
		} else
			i=t;
		if (k<10)
			sfn[i++]='0'+k;
		else if (k<100) {
			sfn[i++]='0'+(k/10);
			sfn[i++]='0'+(k%10);
		} else if (k<1000) {
			sfn[i++]='0'+(k/100);
			sfn[i++]='0'+((k%100)/10);
			sfn[i++]='0'+(k%10);
		} else {
			sfn[i++]='0'+(k/1000);
			sfn[i++]='0'+((k%1000)/100);
			sfn[i++]='0'+((k%100)/10);
			sfn[i++]='0'+(k%10);
		}
		if (t>strlen(n)||k==1||k==10||k==100||k==1000) {
			char *p=strrchr(n, '.');
			if (p!=NULL) {
				sfn[i++]='.';
				n=p+1;
				while (*n == '.') n++;
				int j=0;
				while (*n != 0 && j++<3) {
					if (*n == ' ') {
						n++;
						continue;
					}
					if (*n=='"'||*n=='+'||*n=='='||*n==','||*n==';'||*n==':'||*n=='<'||*n=='>'||*n=='['||*n==']'||*n=='|'||*n=='?'||*n=='*') {
						sfn[i++]='_';
						n++;
					} else
						sfn[i++]=toupper(*(n++));
				}
			}
			sfn[i++]=0;
		}
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
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
    void Flush(void);
	bool UpdateDateTimeFromHost(void);   
	uint32_t GetSeekPos(void);
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

#if 0/*unused*/
private:
    enum { NONE,READ,WRITE } last_action;
	uint16_t info;
#endif
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
		myDrive->deleteClustChain(firstCluster, seekpos);
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
					 * this conditon from treating the BOOT SECTOR as a file. */
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
	return ((clustNum - 2) * BPB.v.BPB_SecPerClus) + firstDataSector;
}

uint32_t fatDrive::getClusterValue(uint32_t clustNum) {
	uint32_t fatoffset=0;
	uint32_t fatsectnum;
	uint32_t fatentoff;
	uint32_t clustValue=0;

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
	char dirtoken[DOS_PATHLENGTH];

	char * findDir;
	char * findFile;
	strcpy(dirtoken,fullname);

	//LOG_MSG("Testing for filename %s", fullname);
	findDir = strtok(dirtoken,"\\");
	if (findDir==NULL) {
		return true;	// root always exists
	}
	findFile = findDir;
	while(findDir != NULL) {
		findFile = findDir;
		findDir = strtok(NULL,"\\");
	}
	int j=0;
	for (int i=0; i<(int)strlen(findFile); i++)
		if (findFile[i]!=' '&&findFile[i]!='"'&&findFile[i]!='+'&&findFile[i]!='='&&findFile[i]!=','&&findFile[i]!=';'&&findFile[i]!=':'&&findFile[i]!='<'&&findFile[i]!='>'&&findFile[i]!='['&&findFile[i]!=']'&&findFile[i]!='|'&&findFile[i]!='?'&&findFile[i]!='*') findFile[j++]=findFile[i];
	findFile[j]=0;
	if (strlen(findFile)>12)
		strncpy(entname, findFile, 12);
	else
		strcpy(entname, findFile);
	upcase(entname);
	return true;
}

void fatDrive::UpdateBootVolumeLabel(const char *label) {
    FAT_BootSector bootbuffer = {};

    if (BPB.v.BPB_BootSig == 0x28 || BPB.v.BPB_BootSig == 0x29) {
        unsigned int i = 0;

        loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);

        while (i < 11 && *label != 0) bootbuffer.bpb.v.BPB_VolLab[i++] = toupper(*label++);
        while (i < 11)                bootbuffer.bpb.v.BPB_VolLab[i++] = ' ';

        loadedDisk->Write_AbsoluteSector(0+partSectOff,&bootbuffer);
    }
}

void fatDrive::SetLabel(const char *label, bool /*iscdrom*/, bool /*updatable*/) {
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
				const char *s = label;
				while (j < 11 && *s != 0) sectbuf[entryoffset].entryname[j++] = toupper(*s++);
				while (j < 11)            sectbuf[entryoffset].entryname[j++] = ' ';
			}
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

		if (sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME) {
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
		findDir = strtok(dirtoken,"\\");
		findFile = findDir;
		while(findDir != NULL) {
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			
			findFile = findDir;
			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) break;
			else {
				//Found something. See if it's a directory (findfirst always finds regular files)
                char find_name[DOS_NAMELENGTH_ASCII],lfind_name[LFN_NAMELENGTH];
                uint16_t find_date,find_time;uint32_t find_size;uint8_t find_attr;
                imgDTA->GetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
				if(!(find_attr & DOS_ATTR_DIRECTORY)) break;

				char * findNext;
				findNext = strtok(NULL,"\\");
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
		char * findDir = strtok(dirtoken,"\\");
		while(findDir != NULL) {
			lfn_filefind_handle=uselfn?LFN_FILEFIND_IMG:LFN_FILEFIND_NONE;
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			findDir = strtok(NULL,"\\");
			if(parDir && (findDir == NULL)) {lfn_filefind_handle=fbak;break;}

			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {
				lfn_filefind_handle=fbak;
				return false;
			} else {
                char find_name[DOS_NAMELENGTH_ASCII],lfind_name[LFN_NAMELENGTH];
                uint16_t find_date,find_time;uint32_t find_size;uint8_t find_attr;
				imgDTA->GetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
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

uint8_t fatDrive::readSector(uint32_t sectnum, void * data) {
	if (absolute) return Read_AbsoluteSector(sectnum, data);
    assert(!IS_PC98_ARCH);
	uint32_t cylindersize = (unsigned int)BPB.v.BPB_NumHeads * (unsigned int)BPB.v.BPB_SecPerTrk;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / BPB.v.BPB_SecPerTrk;
	uint32_t sector = sectnum % BPB.v.BPB_SecPerTrk + 1L;
	return loadedDisk->Read_Sector(head, cylinder, sector, data);
}	

uint8_t fatDrive::writeSector(uint32_t sectnum, void * data) {
	if (absolute) return Write_AbsoluteSector(sectnum, data);
    assert(!IS_PC98_ARCH);
	uint32_t cylindersize = (unsigned int)BPB.v.BPB_NumHeads * (unsigned int)BPB.v.BPB_SecPerTrk;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / BPB.v.BPB_SecPerTrk;
	uint32_t sector = sectnum % BPB.v.BPB_SecPerTrk + 1L;
	return loadedDisk->Write_Sector(head, cylinder, sector, data);
}

uint32_t fatDrive::getSectorSize(void) {
	return BPB.v.BPB_BytsPerSec;
}

uint32_t fatDrive::getClusterSize(void) {
	return (unsigned int)BPB.v.BPB_SecPerClus * (unsigned int)BPB.v.BPB_BytsPerSec;
}

uint32_t fatDrive::getAbsoluteSectFromBytePos(uint32_t startClustNum, uint32_t bytePos) {
	return  getAbsoluteSectFromChain(startClustNum, bytePos / BPB.v.BPB_BytsPerSec);
}

uint32_t fatDrive::getAbsoluteSectFromChain(uint32_t startClustNum, uint32_t logicalSector) {
	int32_t skipClust = (int32_t)(logicalSector / BPB.v.BPB_SecPerClus);
	uint32_t sectClust = (uint32_t)(logicalSector % BPB.v.BPB_SecPerClus);

	/* startClustNum == 0 means the file is (likely) zero length and has no allocation chain yet.
	 * Nothing to map. Without this check, this code would permit the FAT file reader/writer to
	 * treat the ROOT DIRECTORY as a file (with disasterous results)
	 *
	 * [https://github.com/joncampbell123/dosbox-x/issues/1517] */
	if (startClustNum == 0) return 0;

	uint32_t currentClust = startClustNum;

	while(skipClust!=0) {
		bool isEOF = false;
		uint32_t testvalue = getClusterValue(currentClust);
		if(testvalue == 0) {
			/* What the crap?  Cluster is already empty - BAIL! */
			LOG(LOG_DOSMISC,LOG_ERROR)("End of cluster chain and cluster value at the end is zero.");
			return 0;
		}
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0x0ffffff8) isEOF = true; /* FAT32 is really FAT28 with 4 reserved bits */
				break;
		}
		if((isEOF) && (skipClust>=1)) {
			//LOG_MSG("End of cluster chain reached before end of logical sector seek!");
			if (skipClust == 1 && fattype == FAT12) {
				//break;
				LOG(LOG_DOSMISC,LOG_ERROR)("End of cluster chain reached, but maybe good afterall ?");
			}
			return 0;
		}
		currentClust = testvalue;
		--skipClust;
	}

	/* this should not happen! */
	assert(currentClust != 0);

	return (getClustFirstSect(currentClust) + sectClust);
}

void fatDrive::deleteClustChain(uint32_t startCluster, uint32_t bytePos) {
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
		currentClust = testvalue;
		countClust++;

		/* this follows setClusterValue() to make sure the end of the chain is zeroed too */
		if (testvalue >= eofClust)
			return;
	}
}

uint32_t fatDrive::appendCluster(uint32_t startCluster) {
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
			break;
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

	/* Can't allocate cluster #0 */
	if(useCluster == 0) return false;

	if(prevCluster != 0) {
		/* Refuse to allocate cluster if previous cluster value is zero (unallocated) */
		if(!getClusterValue(prevCluster)) return false;

		/* Point cluster to new cluster in chain */
		setClusterValue(prevCluster, useCluster);
		//LOG_MSG("Chaining cluser %d to %d", prevCluster, useCluster);
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
		loadedDisk->Release();
		loadedDisk = NULL;
	}
}

/* PC-98 IPL1 partition table entry.
 * Taken from GNU Parted source code.
 * Maximum 16 entries. */
#pragma pack(push,1)
struct _PC98RawPartition {
	uint8_t		mid;		/* 0x80 - boot */
	uint8_t		sid;		/* 0x80 - active */
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
#pragma pack(pop)

FILE * fopen_lock(const char * fname, const char * mode);
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
    diskfile = fopen_lock(fname, readonly||roflag?"rb":"rb+");
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
		loadedDisk = new QCow2Disk(qcow2_header, diskfile, (uint8_t *)fname, filesize, bytesector, (filesize > 2880));
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

        if (ext != NULL && !strcasecmp(ext, ".d88")) {
            fseeko64(diskfile, 0L, SEEK_END);
            filesize = (uint32_t)(ftello64(diskfile) / 1024L);
            loadedDisk = new imageDiskD88(diskfile, (uint8_t *)fname, filesize, (filesize > 2880));
        }
        else if (!memcmp(bootcode,"VFD1.",5)) { /* FDD files */
            fseeko64(diskfile, 0L, SEEK_END);
            filesize = (uint32_t)(ftello64(diskfile) / 1024L);
            loadedDisk = new imageDiskVFD(diskfile, (uint8_t *)fname, filesize, (filesize > 2880));
        }
        else if (!memcmp(bootcode,"T98FDDIMAGE.R0\0\0",16)) {
            fseeko64(diskfile, 0L, SEEK_END);
            filesize = (uint32_t)(ftello64(diskfile) / 1024L);
            loadedDisk = new imageDiskNFD(diskfile, (uint8_t *)fname, filesize, (filesize > 2880), 0);
        }
        else if (!memcmp(bootcode,"T98FDDIMAGE.R1\0\0",16)) {
            fseeko64(diskfile, 0L, SEEK_END);
            filesize = (uint32_t)(ftello64(diskfile) / 1024L);
            loadedDisk = new imageDiskNFD(diskfile, (uint8_t *)fname, filesize, (filesize > 2880), 1);
        }
        else {
            fseeko64(diskfile, 0L, SEEK_END);
            filesize = (uint32_t)(ftello64(diskfile) / 1024L);
            loadedDisk = new imageDisk(diskfile, fname, filesize, (filesize > 2880));
        }
	}

    fatDriveInit(sysFilename, bytesector, cylsector, headscyl, cylinders, filesize, options);
}

fatDrive::fatDrive(imageDisk *sourceLoadedDisk, std::vector<std::string> &options) {
	if (sourceLoadedDisk == 0) {
		created_successfully = false;
		return;
	}
	created_successfully = true;
	
	if(imgDTASeg == 0) {
		imgDTASeg = DOS_GetMemory(4,"imgDTASeg");
		imgDTAPtr = RealMake(imgDTASeg, 0);
		imgDTA    = new DOS_DTA(imgDTAPtr);
	}
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
}

void fatDrive::fatDriveInit(const char *sysFilename, uint32_t bytesector, uint32_t cylsector, uint32_t headscyl, uint32_t cylinders, uint64_t filesize, const std::vector<std::string> &options) {
	uint32_t startSector = 0,countSector = 0;
	bool pc98_512_to_1024_allow = false;
    int opt_partition_index = -1;
	bool is_hdd = (filesize > 2880);
	struct partTable mbrData;

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
        else {
            LOG(LOG_DOSMISC,LOG_DEBUG)("FAT: option '%s' = '%s' ignored, unknown",name.c_str(),value.c_str());
        }

//        LOG_MSG("'%s' = '%s'",name.c_str(),value.c_str());
    }

	loadedDisk->Addref();

    {
        FAT_BootSector bootbuffer = {};

        if (loadedDisk->getSectSize() > sizeof(bootbuffer)) {
            LOG_MSG("Disk sector/bytes (%u) is too large, not attempting FAT filesystem access",loadedDisk->getSectSize());
            created_successfully = false;
            return;
        }

        if(is_hdd) {
            /* Set user specified harddrive parameters */
            if (headscyl > 0 && cylinders > 0 && cylsector > 0 && bytesector > 0)
                loadedDisk->Set_Geometry(headscyl, cylinders,cylsector, bytesector);

            if (loadedDisk->heads == 0 || loadedDisk->sectors == 0 || loadedDisk->cylinders == 0) {
                created_successfully = false;
                LOG_MSG("No geometry");
                return;
            }

            loadedDisk->Read_Sector(0,0,1,&mbrData);

            if(mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) LOG_MSG("Possibly invalid partition table in disk image.");

            startSector = 0;

            /* PC-98 bootloader support.
             * These can be identified by the "IPL1" in the boot sector.
             * These boot sectors do not have a valid partition table though the code below might
             * pick up a false partition #3 with a zero offset. Partition table is in sector 1 */
            if (!memcmp(mbrData.booter+4,"IPL1",4)) {
                unsigned char ipltable[SECTOR_SIZE_MAX];
                unsigned int max_entries = (std::min)(16UL,(unsigned long)(loadedDisk->getSectSize() / sizeof(_PC98RawPartition)));
                Bitu i;

                LOG_MSG("PC-98 IPL1 signature detected");

                assert(sizeof(_PC98RawPartition) == 32);

                memset(ipltable,0,sizeof(ipltable));
                loadedDisk->Read_Sector(0,0,2,ipltable);

                if (opt_partition_index >= 0) {
                    /* user knows best! */
                    if ((unsigned int)opt_partition_index >= max_entries) {
                        LOG_MSG("Partition index out of range");
                        created_successfully = false;
                        return;
                    }

                    i = (unsigned int)opt_partition_index;
                    const _PC98RawPartition *pe = (_PC98RawPartition*)(ipltable+(i * 32));

                    /* unfortunately start and end are in C/H/S geometry, so we have to translate.
                     * this is why it matters so much to read the geometry from the HDI header.
                     *
                     * NOTE: C/H/S values in the IPL1 table are similar to IBM PC except that sectors are counted from 0, not 1 */
                    startSector =
                        (pe->cyl * loadedDisk->sectors * loadedDisk->heads) +
                        (pe->head * loadedDisk->sectors) +
                        pe->sector;

                    /* Many HDI images I've encountered so far indicate 512 bytes/sector,
                     * but then the FAT filesystem itself indicates 1024 bytes per sector. */
                    pc98_512_to_1024_allow = true;

                    {
                        /* FIXME: What if the label contains SHIFT-JIS? */
                        std::string name = std::string(pe->name,sizeof(pe->name));

                        LOG_MSG("Using IPL1 entry %lu name '%s' which starts at sector %lu",
                                (unsigned long)i,name.c_str(),(unsigned long)startSector);
                    }
                }
                else {
                    for (i=0;i < max_entries;i++) {
                        const _PC98RawPartition *pe = (_PC98RawPartition*)(ipltable+(i * 32));

                        if (pe->mid == 0 && pe->sid == 0 &&
                                pe->ipl_sect == 0 && pe->ipl_head == 0 && pe->ipl_cyl == 0 &&
                                pe->sector == 0 && pe->head == 0 && pe->cyl == 0 &&
                                pe->end_sector == 0 && pe->end_head == 0 && pe->end_cyl == 0)
                            continue; /* unused */

                        /* We're looking for MS-DOS partitions.
                         * I've heard that some other OSes were once ported to PC-98, including Windows NT and OS/2,
                         * so I would rather not mistake NTFS or HPFS as FAT and cause damage. --J.C.
                         * FIXME: Is there a better way? */
                        if (!strncasecmp(pe->name,"MS-DOS",6) ||
                                !strncasecmp(pe->name,"MSDOS",5) ||
                                !strncasecmp(pe->name,"Windows",7)) {
                            /* unfortunately start and end are in C/H/S geometry, so we have to translate.
                             * this is why it matters so much to read the geometry from the HDI header.
                             *
                             * NOTE: C/H/S values in the IPL1 table are similar to IBM PC except that sectors are counted from 0, not 1 */
                            startSector =
                                (pe->cyl * loadedDisk->sectors * loadedDisk->heads) +
                                (pe->head * loadedDisk->sectors) +
                                pe->sector;

                            /* Many HDI images I've encountered so far indicate 512 bytes/sector,
                             * but then the FAT filesystem itself indicates 1024 bytes per sector. */
                            pc98_512_to_1024_allow = true;

                            {
                                /* FIXME: What if the label contains SHIFT-JIS? */
                                std::string name = std::string(pe->name,sizeof(pe->name));

                                LOG_MSG("Using IPL1 entry %lu name '%s' which starts at sector %lu",
                                        (unsigned long)i,name.c_str(),(unsigned long)startSector);
                                break;
                            }
                        }
                    }
                }

                if (i == max_entries)
                    LOG_MSG("No partitions found in the IPL1 table");
            }
            else {
                /* IBM PC master boot record search */
                int m=4;

                if (opt_partition_index >= 0) {
                    /* user knows best! */
                    if (opt_partition_index >= 4) {
                        LOG_MSG("Partition index out of range");
                        created_successfully = false;
                        return;
                    }

                    startSector = mbrData.pentry[m=opt_partition_index].absSectStart;
                    countSector = mbrData.pentry[m=opt_partition_index].partSize;
                }
                else {
                    for(m=0;m<4;m++) {
                        /* Pick the first available partition */
                        if (mbrData.pentry[m].parttype == 0x01 || mbrData.pentry[m].parttype == 0x04 || mbrData.pentry[m].parttype == 0x06) {
                            mbrData.pentry[m].absSectStart = var_read(&mbrData.pentry[m].absSectStart);
                            mbrData.pentry[m].partSize = var_read(&mbrData.pentry[m].partSize);
                            LOG_MSG("Using partition %d on drive (type 0x%02x); skipping %d sectors", m, mbrData.pentry[m].parttype, mbrData.pentry[m].absSectStart);
                            startSector = mbrData.pentry[m].absSectStart;
                            countSector = mbrData.pentry[m].partSize;
                            break;
                        }
                        else if (mbrData.pentry[m].parttype == 0x0E/*FAT16B LBA*/) {
                            if (dos.version.major < 7 && systemmessagebox("Mounting LBA disk image","Mounting this type of disk images requires a reported DOS version of 7.0 or higher. Do you want to change the reported DOS version to 7.0 now?","yesno", "question", 1)) {
                                dos.version.major = 7;
                                dos.version.minor = 0;
                                dos_ver_menu(false);
                            }
							if (dos.version.major >= 7) { /* MS-DOS 7.0 or higher */
                                mbrData.pentry[m].absSectStart = var_read(&mbrData.pentry[m].absSectStart);
                                mbrData.pentry[m].partSize = var_read(&mbrData.pentry[m].partSize);
								LOG_MSG("Using partition %d on drive (type 0x%02x); skipping %d sectors", m, mbrData.pentry[m].parttype, mbrData.pentry[m].absSectStart);
								startSector = mbrData.pentry[m].absSectStart;
								countSector = mbrData.pentry[m].partSize;
							} else {
								req_ver_major = 7; req_ver_minor = 0;
                            }
                            break;
                        }
                        else if (mbrData.pentry[m].parttype == 0x0B || mbrData.pentry[m].parttype == 0x0C) { /* FAT32 types */
                            if ((dos.version.major < 7 || ((dos.version.major == 7 && dos.version.minor < 10))) && systemmessagebox("Mounting FAT32 disk image","Mounting this type of disk images requires a reported DOS version of 7.10 or higher. Do you want to change the reported DOS version to 7.10 now?","yesno", "question", 1)) {
                                dos.version.major = 7;
                                dos.version.minor = 10;
                                dos_ver_menu(true);
                            }
							if (dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) { /* MS-DOS 7.10 or higher */
                                mbrData.pentry[m].absSectStart = var_read(&mbrData.pentry[m].absSectStart);
                                mbrData.pentry[m].partSize = var_read(&mbrData.pentry[m].partSize);
								LOG_MSG("Using partition %d on drive (type 0x%02x); skipping %d sectors", m, mbrData.pentry[m].parttype, mbrData.pentry[m].absSectStart);
								startSector = mbrData.pentry[m].absSectStart;
								countSector = mbrData.pentry[m].partSize;
							} else {
								req_ver_major = 7; req_ver_minor = 10;
                            }
                            break;
                        }
                    }
                }

                if(m==4) LOG_MSG("No good partition found in image.");
            }

            partSectOff = startSector;
            partSectSize = countSector;
        } else {
            /* Get floppy disk parameters based on image size */
            loadedDisk->Get_Geometry(&headscyl, &cylinders, &cylsector, &bytesector);
            /* Floppy disks don't have partitions */
            partSectOff = 0;

            if (loadedDisk->heads == 0 || loadedDisk->sectors == 0 || loadedDisk->cylinders == 0) {
                created_successfully = false;
                LOG_MSG("No geometry");
                return;
            }
        }

        BPB = {};
        loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);

        /* If the sector is full of 0xF6, the partition is brand new and was just created with Microsoft FDISK.EXE (Windows 98 behavior)
         * and therefore there is NO FAT filesystem here. We'll go farther and check if all bytes are just the same. */
        {
            unsigned int i=1;

            while (i < 128 && ((uint8_t*)(&bootbuffer))[0] == ((uint8_t*)(&bootbuffer))[i]) i++;

            if (i == 128) {
                LOG_MSG("Boot sector appears to have been created by FDISK.EXE but not formatted");
                created_successfully = false;
                return;
            }
        }

        bootbuffer.bpb.v.BPB_BytsPerSec = var_read(&bootbuffer.bpb.v.BPB_BytsPerSec);
        bootbuffer.bpb.v.BPB_RsvdSecCnt = var_read(&bootbuffer.bpb.v.BPB_RsvdSecCnt);
        bootbuffer.bpb.v.BPB_RootEntCnt = var_read(&bootbuffer.bpb.v.BPB_RootEntCnt);
        bootbuffer.bpb.v.BPB_TotSec16 = var_read(&bootbuffer.bpb.v.BPB_TotSec16);
        bootbuffer.bpb.v.BPB_FATSz16 = var_read(&bootbuffer.bpb.v.BPB_FATSz16);
        bootbuffer.bpb.v.BPB_SecPerTrk = var_read(&bootbuffer.bpb.v.BPB_SecPerTrk);
        bootbuffer.bpb.v.BPB_NumHeads = var_read(&bootbuffer.bpb.v.BPB_NumHeads);
        bootbuffer.bpb.v.BPB_HiddSec = var_read(&bootbuffer.bpb.v.BPB_HiddSec);
        bootbuffer.bpb.v.BPB_TotSec32 = var_read(&bootbuffer.bpb.v.BPB_TotSec32);
        bootbuffer.bpb.v.BPB_VolID = var_read(&bootbuffer.bpb.v.BPB_VolID);

        if (!is_hdd) {
            /* Identify floppy format */
            if ((bootbuffer.BS_jmpBoot[0] == 0x69 || bootbuffer.BS_jmpBoot[0] == 0xe9 ||
                        (bootbuffer.BS_jmpBoot[0] == 0xeb && bootbuffer.BS_jmpBoot[2] == 0x90)) &&
                    (bootbuffer.bpb.v.BPB_Media & 0xf0) == 0xf0) {
                /* DOS 2.x or later format, BPB assumed valid */

                if ((bootbuffer.bpb.v.BPB_Media != 0xf0 && !(bootbuffer.bpb.v.BPB_Media & 0x1)) &&
                        (bootbuffer.BS_OEMName[5] != '3' || bootbuffer.BS_OEMName[6] != '.' || bootbuffer.BS_OEMName[7] < '2')) {
                    /* Fix pre-DOS 3.2 single-sided floppy */
                    bootbuffer.bpb.v.BPB_SecPerClus = 1;
                }
            } else {
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
	if ((BPB.v.BPB_SecPerClus == 0) ||
		(BPB.v.BPB_NumFATs == 0) ||
		(BPB.v.BPB_NumHeads == 0 && !IS_PC98_ARCH) ||
		//(BPB.v.BPB_NumHeads > headscyl && !IS_PC98_ARCH) ||
		(BPB.v.BPB_SecPerTrk == 0 && !IS_PC98_ARCH) ||
		(BPB.v.BPB_SecPerTrk > cylsector && !IS_PC98_ARCH)) {
		LOG_MSG("Sanity checks failed");
		created_successfully = false;
		return;
	}

    /* Sanity check: Root directory count is nonzero if FAT16/FAT12, or is zero if FAT32 */
    if (BPB.is_fat32()) {
        if (BPB.v.BPB_RootEntCnt != 0) {
            LOG_MSG("Sanity check fail: Root directory count != 0 and not FAT32");
            created_successfully = false;
            return;
        }
    }
    else {
        if (BPB.v.BPB_RootEntCnt == 0) {
            LOG_MSG("Sanity check fail: Root directory count == 0 and not FAT32");
            created_successfully = false;
            return;
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
        (BPB.v.BPB_BytsPerSec & (BPB.v.BPB_BytsPerSec - 1)) != 0/*not a power of 2*/) {
        LOG_MSG("FAT bytes/sector value %u not supported",BPB.v.BPB_BytsPerSec);
		created_successfully = false;
        return;
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

        RootDirSectors = 0; /* FAT32 root directory has it's own allocation chain, instead of a fixed location */
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
	for(i=0;i<CountOfClusters;i++) {
		if(!getClusterValue(i+2)) return (i+2);
	}

	/* No free cluster found */
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
}

bool fatDrive::FileCreate(DOS_File **file, const char *name, uint16_t attributes) {
	const char *lfn = NULL;

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
			lfn = strrchr(name,'\\');

			if (lfn != NULL) {
				lfn++; /* step past '\' */
				strcpy(path, name);
				*(strrchr(path,'\\')+1)=0;
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
    direntry fileEntry = {};
	uint32_t dummy1, dummy2;
	if(!getFileDirEntry(name, &fileEntry, &dummy1, &dummy2)) return false;
	return true;
}

bool fatDrive::FileOpen(DOS_File **file, const char *name, uint32_t flags) {
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

bool fatDrive::FindFirst(const char *_dir, DOS_DTA &dta,bool /*fcb_findfirst*/) {
    direntry dummyClust = {};

    // volume label searches always affect root directory, no matter the current directory, at least with FCBs
    if (dta.GetAttr() & DOS_ATTR_VOLUME) {
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
	var_write(&dst->crtTime, src->crtTime);
	var_write(&dst->crtDate, src->crtDate);
	var_write(&dst->accessDate, src->accessDate);
	var_write(&dst->hiFirstClust, src->hiFirstClust);
	var_write(&dst->modTime, src->modTime);
	var_write(&dst->modDate, src->modDate);
	var_write(&dst->loFirstClust, src->loFirstClust);
	var_write(&dst->entrysize, src->entrysize);
}

bool fatDrive::FindNextInternal(uint32_t dirClustNumber, DOS_DTA &dta, direntry *foundEntry) {
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

	dta.GetSearchParams(attrs, srch_pattern,uselfn);
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
	memcpy(find_name,&sectbuf[entryoffset].entryname[0],8);
    memcpy(extension,&sectbuf[entryoffset].entryname[8],3);

    if (!(sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)) {
        trimString(&find_name[0]);
        trimString(&extension[0]);
    }

	if (extension[0]!=0) {
		if (!(sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)) {
			strcat(find_name, ".");
		}
		strcat(find_name, extension);
	}

	if (sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)
        trimString(find_name);

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
			unsigned int stridx = oidx * 13u;

			if ((stridx+13u) <= LFN_NAMELENGTH) {
				for (unsigned int i=0;i < 5;i++)
					lfind_name[stridx+i+0] = (char)(dlfn->LDIR_Name1[i] & 0xFF);
				for (unsigned int i=0;i < 6;i++)
					lfind_name[stridx+i+5] = (char)(dlfn->LDIR_Name2[i] & 0xFF);
				for (unsigned int i=0;i < 2;i++)
					lfind_name[stridx+i+11] = (char)(dlfn->LDIR_Name3[i] & 0xFF);

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
	if (!(WildFileCmp(find_name,srch_pattern) || (lfn_max_ord != 0 && lfind_name[0] != 0 && LWildFileCmp(lfind_name,srch_pattern)))) {
		lfind_name[0] = 0; /* LFN code will memset() it in full upon next dirent */
		lfn_max_ord = 0;
		lfnRange.clear();
		goto nextfile;
	}

	// Drive emulation does not need to require a LFN in case there is no corresponding 8.3 names.
	if (lfind_name[0] == 0) strcpy(lfind_name,find_name);

	copyDirEntry(&sectbuf[entryoffset], foundEntry);

	//dta.SetResult(find_name, foundEntry->entrysize, foundEntry->crtDate, foundEntry->crtTime, foundEntry->attrib);

	dta.SetResult(find_name, lfind_name, foundEntry->entrysize, foundEntry->modDate, foundEntry->modTime, foundEntry->attrib);

	return true;
}

bool fatDrive::FindNext(DOS_DTA &dta) {
    direntry dummyClust = {};

	return FindNextInternal(lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirIDCluster():(dnum[lfn_filefind_handle]?dnum[lfn_filefind_handle]:0), dta, &dummyClust);
}


bool fatDrive::SetFileAttr(const char *name, uint16_t attr) {
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
	direntry sectbuf[MAX_DIRENTS_PER_SECTOR];	/* 16 directory entries per 512 byte sector */
	uint32_t entryoffset = 0;	/* Index offset within sector */
	uint32_t tmpsector;
	uint16_t dirPos = 0;

    (void)start;//UNUSED

    size_t dirent_per_sector = getSectSize() / sizeof(direntry);
    assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
    assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	while(entNum>=0) {
		uint32_t logentsector = ((uint32_t)((size_t)dirPos / dirent_per_sector)); /* Logical entry sector */
		entryoffset = ((uint32_t)((size_t)dirPos % dirent_per_sector));

		if(dirClustNumber==0) {
            assert(!BPB.is_fat32());
            if(dirPos >= BPB.v.BPB_RootEntCnt) return false;
			tmpsector = firstRootDirSect+logentsector;
			readSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen */
			if(tmpsector == 0) return false;
			readSector(tmpsector,sectbuf);
		}
		dirPos++;


		/* End of directory list */
		if (sectbuf[entryoffset].entryname[0] == 0x00) return false;
		--entNum;
	}

	copyDirEntry(&sectbuf[entryoffset], useEntry);
	return true;
}

bool fatDrive::directoryChange(uint32_t dirClustNumber, const direntry *useEntry, int32_t entNum) {
	direntry sectbuf[MAX_DIRENTS_PER_SECTOR];	/* 16 directory entries per 512 byte sector */
	uint32_t entryoffset = 0;	/* Index offset within sector */
	uint32_t tmpsector = 0;
	uint16_t dirPos = 0;
	
    size_t dirent_per_sector = getSectSize() / sizeof(direntry);
    assert(dirent_per_sector <= MAX_DIRENTS_PER_SECTOR);
    assert((dirent_per_sector * sizeof(direntry)) <= SECTOR_SIZE_MAX);

	while(entNum>=0) {		
		uint32_t logentsector = ((uint32_t)((size_t)dirPos / dirent_per_sector)); /* Logical entry sector */
		entryoffset = ((uint32_t)((size_t)dirPos % dirent_per_sector));

		if(dirClustNumber==0) {
            assert(!BPB.is_fat32());
            if(dirPos >= BPB.v.BPB_RootEntCnt) return false;
			tmpsector = firstRootDirSect+logentsector;
			readSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen */
			if(tmpsector == 0) return false;
			readSector(tmpsector,sectbuf);
		}
		dirPos++;


		/* End of directory list */
		if (sectbuf[entryoffset].entryname[0] == 0x00) return false;
		--entNum;
	}
	if(tmpsector != 0) {
		copyDirEntry(useEntry, &sectbuf[entryoffset]);
		writeSector(tmpsector, sectbuf);
		return true;
	} else {
		return false;
	}
}

bool fatDrive::addDirectoryEntry(uint32_t dirClustNumber, const direntry& useEntry,const char *lfn) {
	direntry sectbuf[MAX_DIRENTS_PER_SECTOR]; /* 16 directory entries per 512 byte sector */
	uint32_t tmpsector;
	uint16_t dirPos = 0;
	unsigned int need = 1;
	unsigned int found = 0;
	uint16_t dirPosFound = 0;

	if (lfn != NULL && *lfn != 0) {
		/* 13 characters per LFN entry.
		 * FIXME: When we convert the LFN to wchar using code page, strlen() prior to conversion will not work,
		 *        convert first then count wchar_t characters. */
		need = (unsigned int)(1 + ((strlen(lfn) + 12) / 13))/*round up*/;
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

					assert(lfn != NULL);
					assert(*lfn != 0);

					/* TODO: ANSI LFN convert to wchar here according to code page */

					unsigned int o = 0;
					const char *scan = lfn;

					while (*scan) {
						if (o >= LFN_NAMELENGTH) return false; /* Nope! */
						lfnbuf[o++] = (uint16_t)((unsigned char)(*scan++));
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
		lfn = strrchr(dir,'\\');

		if (lfn != NULL) {
			lfn++; /* step past '\' */
			strcpy(path, dir);
			*(strrchr(path,'\\')+1)=0;
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
	uint32_t filecount = 0;
	/* Set to 2 to skip first 2 entries, [.] and [..] */
	int32_t fileidx = 2;
	while(directoryBrowse(dummyClust, &tmpentry, fileidx)) {
		/* Check for non-deleted files */
		if(tmpentry.entryname[0] != 0xe5) filecount++;
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
		lfn = strrchr(newname,'\\');

		if (lfn != NULL) {
			lfn++; /* step past '\' */
			strcpy(path, newname);
			*(strrchr(path,'\\')+1)=0;
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
	uint32_t dummyClust;

	/* root directory is directory */
	if (*dir == 0) return true;

	return getDirClustNum(dir, &dummyClust, false);
}

uint32_t fatDrive::GetPartitionOffset(void) {
	return partSectOff;
}

uint32_t fatDrive::GetFirstClusterOffset(void) {
    return firstDataSector - partSectOff;
}

uint32_t fatDrive::GetHighestClusterNumber(void) {
    return CountOfClusters + 1ul;
}
