/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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

#define IMGTYPE_FLOPPY 0
#define IMGTYPE_ISO    1
#define IMGTYPE_HDD	   2

#define FAT12		   0
#define FAT16		   1
#define FAT32		   2

class fatFile : public DOS_File {
public:
	fatFile(const char* name, Bit32u startCluster, Bit32u fileLen, fatDrive *useDrive);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
	bool UpdateDateTimeFromHost(void);   
	Bit32u GetSeekPos(void);
public:
	Bit32u firstCluster;
	Bit32u seekpos;
	Bit32u filelength;
	Bit32u currentSector;
	Bit32u curSectOff;
	Bit8u sectorBuffer[512];
	/* Record of where in the directory structure this file is located */
	Bit32u dirCluster;
	Bit32u dirIndex;

	bool loadedSector;
	fatDrive *myDrive;
private:
	enum { NONE,READ,WRITE } last_action;
	Bit16u info;
};


/* IN - char * filename: Name in regular filename format, e.g. bob.txt */
/* OUT - char * filearray: Name in DOS directory format, eleven char, e.g. bob     txt */
static void convToDirFile(const char *filename, char *filearray) {
	Bit32u charidx = 0;
	Bit32u flen,i;
	flen = (Bit32u)strlen(filename);
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

fatFile::fatFile(const char* /*name*/, Bit32u startCluster, Bit32u fileLen, fatDrive *useDrive) {
	Bit32u seekto = 0;
	firstCluster = startCluster;
	myDrive = useDrive;
	filelength = fileLen;
	open = true;
	loadedSector = false;
	curSectOff = 0;
	seekpos = 0;
	memset(&sectorBuffer[0], 0, sizeof(sectorBuffer));
	
	if(filelength > 0) {
		Seek(&seekto, DOS_SEEK_SET);
		myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);
		loadedSector = true;
	}
}

bool fatFile::Read(Bit8u * data, Bit16u *size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {	// check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	Bit16u sizedec, sizecount;
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
		curSectOff = 0;
		myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);
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
			myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);
			loadedSector = true;
			//LOG_MSG("Reading absolute sector at %d for seekpos %d", currentSector, seekpos);
		}
		--sizedec;
	}
	*size =sizecount;
	return true;
}

bool fatFile::Write(Bit8u * data, Bit16u *size) {
	/* TODO: Check for read-only bit */

	if ((this->flags & 0xf) == OPEN_READ) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	direntry tmpentry;
	Bit16u sizedec, sizecount;
	sizedec = *size;
	sizecount = 0;

	while(sizedec != 0) {
		/* Increase filesize if necessary */
		if(seekpos >= filelength) {
			if(filelength == 0) {
				firstCluster = myDrive->getFirstFreeClust();
				myDrive->allocateCluster(firstCluster, 0);
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);
				loadedSector = true;
			}
			filelength = seekpos+1;
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
				curSectOff = 0;
				myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);

				loadedSector = true;
			}
		}
		sectorBuffer[curSectOff++] = data[sizecount++];
		seekpos++;
		if(curSectOff >= myDrive->getSectorSize()) {
			if(loadedSector) myDrive->loadedDisk->Write_AbsoluteSector(currentSector, sectorBuffer);

			currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
			if(currentSector == 0) {
				/* EOC reached before EOF - try to increase file allocation */
				myDrive->appendCluster(firstCluster);
				/* Try getting sector again */
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				if(currentSector == 0) {
					/* No can do. lets give up and go home.  We must be out of room */
					loadedSector = false;
					goto finalizeWrite;
				}
			}
			curSectOff = 0;
			myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);

			loadedSector = true;
		}
		--sizedec;
	}
	if(curSectOff>0 && loadedSector) myDrive->loadedDisk->Write_AbsoluteSector(currentSector, sectorBuffer);

finalizeWrite:
	myDrive->directoryBrowse(dirCluster, &tmpentry, dirIndex);
	tmpentry.entrysize = filelength;
	tmpentry.loFirstClust = (Bit16u)firstCluster;
	myDrive->directoryChange(dirCluster, &tmpentry, dirIndex);

	*size =sizecount;
	return true;
}

bool fatFile::Seek(Bit32u *pos, Bit32u type) {
	Bit32s seekto=0;
	
	switch(type) {
		case DOS_SEEK_SET:
			seekto = (Bit32s)*pos;
			break;
		case DOS_SEEK_CUR:
			/* Is this relative seek signed? */
			seekto = (Bit32s)*pos + (Bit32s)seekpos;
			break;
		case DOS_SEEK_END:
			seekto = (Bit32s)filelength + (Bit32s)*pos;
			break;
	}
//	LOG_MSG("Seek to %d with type %d (absolute value %d)", *pos, type, seekto);

	if((Bit32u)seekto > filelength) seekto = (Bit32s)filelength;
	if(seekto<0) seekto = 0;
	seekpos = (Bit32u)seekto;
	currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
	if (currentSector == 0) {
		/* not within file size, thus no sector is available */
		loadedSector = false;
	} else {
		curSectOff = seekpos % myDrive->getSectorSize();
		myDrive->loadedDisk->Read_AbsoluteSector(currentSector, sectorBuffer);
	}
	*pos = seekpos;
	return true;
}

bool fatFile::Close() {
	/* Flush buffer */
	if (loadedSector) myDrive->loadedDisk->Write_AbsoluteSector(currentSector, sectorBuffer);

	return false;
}

Bit16u fatFile::GetInformation(void) {
	return 0;
}

bool fatFile::UpdateDateTimeFromHost(void) {
	return true;
}

Bit32u fatFile::GetSeekPos() {
	return seekpos;
}

Bit32u fatDrive::getClustFirstSect(Bit32u clustNum) {
	return ((clustNum - 2) * bootbuffer.sectorspercluster) + firstDataSector;
}

Bit32u fatDrive::getClusterValue(Bit32u clustNum) {
	Bit32u fatoffset=0;
	Bit32u fatsectnum;
	Bit32u fatentoff;
	Bit32u clustValue=0;

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
	fatsectnum = bootbuffer.reservedsectors + (fatoffset / bootbuffer.bytespersector) + partSectOff;
	fatentoff = fatoffset % bootbuffer.bytespersector;

	if(curFatSect != fatsectnum) {
		/* Load two sectors at once for FAT12 */
		loadedDisk->Read_AbsoluteSector(fatsectnum, &fatSectBuffer[0]);
		if (fattype==FAT12)
			loadedDisk->Read_AbsoluteSector(fatsectnum+1, &fatSectBuffer[512]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12:
			clustValue = *((Bit16u *)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) {
				clustValue >>= 4;
			} else {
				clustValue &= 0xfff;
			}
			break;
		case FAT16:
			clustValue = *((Bit16u *)&fatSectBuffer[fatentoff]);
			break;
		case FAT32:
			clustValue = *((Bit32u *)&fatSectBuffer[fatentoff]);
			break;
	}

	return clustValue;
}

void fatDrive::setClusterValue(Bit32u clustNum, Bit32u clustValue) {
	Bit32u fatoffset=0;
	Bit32u fatsectnum;
	Bit32u fatentoff;

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
	fatsectnum = bootbuffer.reservedsectors + (fatoffset / bootbuffer.bytespersector) + partSectOff;
	fatentoff = fatoffset % bootbuffer.bytespersector;

	if(curFatSect != fatsectnum) {
		/* Load two sectors at once for FAT12 */
		loadedDisk->Read_AbsoluteSector(fatsectnum, &fatSectBuffer[0]);
		if (fattype==FAT12)
			loadedDisk->Read_AbsoluteSector(fatsectnum+1, &fatSectBuffer[512]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12: {
			Bit16u tmpValue = *((Bit16u *)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) {
				clustValue &= 0xfff;
				clustValue <<= 4;
				tmpValue &= 0xf;
				tmpValue |= (Bit16u)clustValue;

			} else {
				clustValue &= 0xfff;
				tmpValue &= 0xf000;
				tmpValue |= (Bit16u)clustValue;
			}
			*((Bit16u *)&fatSectBuffer[fatentoff]) = tmpValue;
			break;
			}
		case FAT16:
			*((Bit16u *)&fatSectBuffer[fatentoff]) = (Bit16u)clustValue;
			break;
		case FAT32:
			*((Bit32u *)&fatSectBuffer[fatentoff]) = clustValue;
			break;
	}
	for(int fc=0;fc<bootbuffer.fatcopies;fc++) {
		loadedDisk->Write_AbsoluteSector(fatsectnum + (fc * bootbuffer.sectorsperfat), &fatSectBuffer[0]);
		if (fattype==FAT12) {
			if (fatentoff>=511)
				loadedDisk->Write_AbsoluteSector(fatsectnum+1+(fc * bootbuffer.sectorsperfat), &fatSectBuffer[512]);
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
	strcpy(entname, findFile);
	return true;
}

bool fatDrive::getFileDirEntry(char const * const filename, direntry * useEntry, Bit32u * dirClust, Bit32u * subEntry) {
	size_t len = strlen(filename);
	char dirtoken[DOS_PATHLENGTH];
	Bit32u currentClust = 0;

	direntry foundEntry;
	char * findDir;
	char * findFile;
	strcpy(dirtoken,filename);
	findFile=dirtoken;

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
				char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;Bit8u find_attr;
				imgDTA->GetResult(find_name,find_size,find_date,find_time,find_attr);
				if(!(find_attr & DOS_ATTR_DIRECTORY)) break;
			}

			currentClust = foundEntry.loFirstClust;
			findDir = strtok(NULL,"\\");
		}
	} else {
		/* Set to root directory */
	}

	/* Search found directory for our file */
	imgDTA->SetupSearch(0,0x7,findFile);
	imgDTA->SetDirID(0);
	if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) return false;

	memcpy(useEntry, &foundEntry, sizeof(direntry));
	*dirClust = (Bit32u)currentClust;
	*subEntry = ((Bit32u)imgDTA->GetDirID()-1);
	return true;
}

bool fatDrive::getDirClustNum(const char *dir, Bit32u *clustNum, bool parDir) {
	Bit32u len = (Bit32u)strlen(dir);
	char dirtoken[DOS_PATHLENGTH];
	Bit32u currentClust = 0;
	direntry foundEntry;
	char * findDir;
	strcpy(dirtoken,dir);

	/* Skip if testing for root directory */
	if ((len>0) && (dir[len-1]!='\\')) {
		//LOG_MSG("Testing for dir %s", dir);
		findDir = strtok(dirtoken,"\\");
		while(findDir != NULL) {
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			findDir = strtok(NULL,"\\");
			if(parDir && (findDir == NULL)) break;

			char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;Bit8u find_attr;
			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {
				return false;
			} else {
				imgDTA->GetResult(find_name,find_size,find_date,find_time,find_attr);
				if(!(find_attr &DOS_ATTR_DIRECTORY)) return false;
			}
			currentClust = foundEntry.loFirstClust;

		}
		*clustNum = currentClust;
	} else {
		/* Set to root directory */
		*clustNum = 0;
	}
	return true;
}

Bit32u fatDrive::getSectorSize(void) {
	return bootbuffer.bytespersector;
}

Bit32u fatDrive::getAbsoluteSectFromBytePos(Bit32u startClustNum, Bit32u bytePos) {
	return  getAbsoluteSectFromChain(startClustNum, bytePos / bootbuffer.bytespersector);
}

Bit32u fatDrive::getAbsoluteSectFromChain(Bit32u startClustNum, Bit32u logicalSector) {
	Bit32s skipClust = logicalSector / bootbuffer.sectorspercluster;
	Bit32u sectClust = logicalSector % bootbuffer.sectorspercluster;

	Bit32u currentClust = startClustNum;
	Bit32u testvalue;

	while(skipClust!=0) {
		bool isEOF = false;
		testvalue = getClusterValue(currentClust);
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0xfffffff8) isEOF = true;
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

	return (getClustFirstSect(currentClust) + sectClust);
}

void fatDrive::deleteClustChain(Bit32u startCluster) {
	Bit32u testvalue;
	Bit32u currentClust = startCluster;
	bool isEOF = false;
	while(!isEOF) {
		testvalue = getClusterValue(currentClust);
		if(testvalue == 0) {
			/* What the crap?  Cluster is already empty - BAIL! */
			break;
		}
		/* Mark cluster as empty */
		setClusterValue(currentClust, 0);
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0xfffffff8) isEOF = true;
				break;
		}
		if(isEOF) break;
		currentClust = testvalue;
	}
}

Bit32u fatDrive::appendCluster(Bit32u startCluster) {
	Bit32u testvalue;
	Bit32u currentClust = startCluster;
	bool isEOF = false;
	
	while(!isEOF) {
		testvalue = getClusterValue(currentClust);
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0xfffffff8) isEOF = true;
				break;
		}
		if(isEOF) break;
		currentClust = testvalue;
	}

	Bit32u newClust = getFirstFreeClust();
	/* Drive is full */
	if(newClust == 0) return 0;

	if(!allocateCluster(newClust, currentClust)) return 0;

	zeroOutCluster(newClust);

	return newClust;
}

bool fatDrive::allocateCluster(Bit32u useCluster, Bit32u prevCluster) {

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
			setClusterValue(useCluster, 0xffffffff);
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

fatDrive::fatDrive(const char *sysFilename, Bit32u bytesector, Bit32u cylsector, Bit32u headscyl, Bit32u cylinders, Bit32u startSector) {
	created_successfully = true;
	FILE *diskfile;
	Bit32u filesize;
	struct partTable mbrData;
	
	if(imgDTASeg == 0) {
		imgDTASeg = DOS_GetMemory(2,"imgDTASeg");
		imgDTAPtr = RealMake(imgDTASeg, 0);
		imgDTA    = new DOS_DTA(imgDTAPtr);
	}

	diskfile = fopen64(sysFilename, "rb+");
	if(!diskfile) {created_successfully = false;return;}
	fseeko64(diskfile, 0L, SEEK_END);
	filesize = (Bit32u)(ftello64(diskfile) / 1024L);

	/* Load disk image */
	loadedDisk = new imageDisk(diskfile, (Bit8u *)sysFilename, filesize, (filesize > 2880));
	if(!loadedDisk) {
		created_successfully = false;
		return;
	}
	loadedDisk->Addref();

	if(filesize > 2880) {
		/* Set user specified harddrive parameters */
		loadedDisk->Set_Geometry(headscyl, cylinders,cylsector, bytesector);

		loadedDisk->Read_Sector(0,0,1,&mbrData);

		if(mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) LOG_MSG("Possibly invalid partition table in disk image.");

		startSector = 63;
		int m;
		for(m=0;m<4;m++) {
			/* Pick the first available partition */
			if(mbrData.pentry[m].partSize != 0x00) {
				LOG_MSG("Using partition %d on drive; skipping %d sectors", m, mbrData.pentry[m].absSectStart);
				startSector = mbrData.pentry[m].absSectStart;
				break;
			}
		}

		if(m==4) LOG_MSG("No good partiton found in image.");

		partSectOff = startSector;
	} else {
		/* Floppy disks don't have partitions */
		partSectOff = 0;
	}

	loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);
	if ((bootbuffer.magic1 != 0x55) || (bootbuffer.magic2 != 0xaa)) {
		/* Not a FAT filesystem */
		LOG_MSG("Loaded image has no valid magicnumbers at the end!");
	}

	if(!bootbuffer.sectorsperfat) {
		/* FAT32 not implemented yet */
		LOG_MSG("FAT32 not implemented yet, mounting image only");
		fattype = FAT32;	// Avoid parsing dir entries, see fatDrive::FindFirst()...should work for unformatted images as well
		return;
	}


	/* Determine FAT format, 12, 16 or 32 */

	/* Get size of root dir in sectors */
	/* TODO: Get 32-bit total sector count if needed */
	Bit32u RootDirSectors = ((bootbuffer.rootdirentries * 32) + (bootbuffer.bytespersector - 1)) / bootbuffer.bytespersector;
	Bit32u DataSectors;
	if(bootbuffer.totalsectorcount != 0) {
		DataSectors = bootbuffer.totalsectorcount - (bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + RootDirSectors);
	} else {
		DataSectors = bootbuffer.totalsecdword - (bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + RootDirSectors);

	}
	CountOfClusters = DataSectors / bootbuffer.sectorspercluster;

	firstDataSector = (bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + RootDirSectors) + partSectOff;
	firstRootDirSect = bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + partSectOff;

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

	/* There is no cluster 0, this means we are in the root directory */
	cwdDirCluster = 0;

	memset(fatSectBuffer,0,1024);
	curFatSect = 0xffffffff;

	strcpy(info, "fatDrive ");
	strcat(info, sysFilename);
}

bool fatDrive::AllocationInfo(Bit16u *_bytes_sector, Bit8u *_sectors_cluster, Bit16u *_total_clusters, Bit16u *_free_clusters) {
	Bit32u hs, cy, sect,sectsize;
	Bit32u countFree = 0;
	Bit32u i;
	
	loadedDisk->Get_Geometry(&hs, &cy, &sect, &sectsize);
	*_bytes_sector = (Bit16u)sectsize;
	*_sectors_cluster = bootbuffer.sectorspercluster;
	if (CountOfClusters<65536) *_total_clusters = (Bit16u)CountOfClusters;
	else {
		// maybe some special handling needed for fat32
		*_total_clusters = 65535;
	}
	for(i=0;i<CountOfClusters;i++) if(!getClusterValue(i+2)) countFree++;
	if (countFree<65536) *_free_clusters = (Bit16u)countFree;
	else {
		// maybe some special handling needed for fat32
		*_free_clusters = 65535;
	}
	
	return true;
}

Bit32u fatDrive::getFirstFreeClust(void) {
	Bit32u i;
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

Bit8u fatDrive::GetMediaByte(void) { return loadedDisk->GetBiosType(); }

bool fatDrive::FileCreate(DOS_File **file, const char *name, Bit16u attributes) {
	direntry fileEntry;
	Bit32u dirClust, subEntry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11];

	Bit16u save_errorcode=dos.errorcode;

	/* Check if file already exists */
	if(getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		/* Truncate file */
		fileEntry.entrysize=0;
		directoryChange(dirClust, &fileEntry, subEntry);
	} else {
		/* Can we even get the name of the file itself? */
		if(!getEntryName(name, &dirName[0])) return false;
		convToDirFile(&dirName[0], &pathName[0]);

		/* Can we find the base directory? */
		if(!getDirClustNum(name, &dirClust, true)) return false;
		memset(&fileEntry, 0, sizeof(direntry));
		memcpy(&fileEntry.entryname, &pathName[0], 11);
		fileEntry.attrib = (Bit8u)(attributes & 0xff);
		addDirectoryEntry(dirClust, fileEntry);

		/* Check if file exists now */
		if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;
	}

	/* Empty file created, now lets open it */
	/* TODO: check for read-only flag and requested write access */
	*file = new fatFile(name, fileEntry.loFirstClust, fileEntry.entrysize, this);
	(*file)->flags=OPEN_READWRITE;
	((fatFile *)(*file))->dirCluster = dirClust;
	((fatFile *)(*file))->dirIndex = subEntry;
	/* Maybe modTime and date should be used ? (crt matches findnext) */
	((fatFile *)(*file))->time = fileEntry.crtTime;
	((fatFile *)(*file))->date = fileEntry.crtDate;

	dos.errorcode=save_errorcode;
	return true;
}

bool fatDrive::FileExists(const char *name) {
	direntry fileEntry;
	Bit32u dummy1, dummy2;
	if(!getFileDirEntry(name, &fileEntry, &dummy1, &dummy2)) return false;
	return true;
}

bool fatDrive::FileOpen(DOS_File **file, const char *name, Bit32u flags) {
	direntry fileEntry;
	Bit32u dirClust, subEntry;
	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;
	/* TODO: check for read-only flag and requested write access */
	*file = new fatFile(name, fileEntry.loFirstClust, fileEntry.entrysize, this);
	(*file)->flags = flags;
	((fatFile *)(*file))->dirCluster = dirClust;
	((fatFile *)(*file))->dirIndex = subEntry;
	/* Maybe modTime and date should be used ? (crt matches findnext) */
	((fatFile *)(*file))->time = fileEntry.crtTime;
	((fatFile *)(*file))->date = fileEntry.crtDate;
	return true;
}

bool fatDrive::FileStat(const char * /*name*/, FileStat_Block *const /*stat_block*/) {
	/* TODO: Stub */
	return false;
}

bool fatDrive::FileUnlink(const char * name) {
	direntry fileEntry;
	Bit32u dirClust, subEntry;

	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;

	fileEntry.entryname[0] = 0xe5;
	directoryChange(dirClust, &fileEntry, subEntry);

	if(fileEntry.loFirstClust != 0) deleteClustChain(fileEntry.loFirstClust);

	return true;
}

bool fatDrive::FindFirst(const char *_dir, DOS_DTA &dta,bool /*fcb_findfirst*/) {
	direntry dummyClust;
	if(fattype==FAT32) return false;
#if 0
	Bit8u attr;char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr,pattern);
	if(attr==DOS_ATTR_VOLUME) {
		if (strcmp(GetLabel(), "") == 0 ) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		dta.SetResult(GetLabel(),0,0,0,DOS_ATTR_VOLUME);
		return true;
	}
	if(attr & DOS_ATTR_VOLUME) //check for root dir or fcb_findfirst
		LOG(LOG_DOSMISC,LOG_WARN)("findfirst for volumelabel used on fatDrive. Unhandled!!!!!");
#endif
	if(!getDirClustNum(_dir, &cwdDirCluster, false)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	dta.SetDirID(0);
	dta.SetDirIDCluster((Bit16u)(cwdDirCluster&0xffff));
	return FindNextInternal(cwdDirCluster, dta, &dummyClust);
}

char* removeTrailingSpaces(char* str) {
	char* end = str + strlen(str);
	while((*--end == ' ') && (end > str)) {};
	*++end = '\0';
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

bool fatDrive::FindNextInternal(Bit32u dirClustNumber, DOS_DTA &dta, direntry *foundEntry) {
	direntry sectbuf[16]; /* 16 directory entries per sector */
	Bit32u logentsector; /* Logical entry sector */
	Bit32u entryoffset;  /* Index offset within sector */
	Bit32u tmpsector;
	Bit8u attrs;
	Bit16u dirPos;
	char srch_pattern[DOS_NAMELENGTH_ASCII];
	char find_name[DOS_NAMELENGTH_ASCII];
	char extension[4];

	dta.GetSearchParams(attrs, srch_pattern);
	dirPos = dta.GetDirID();

nextfile:
	logentsector = dirPos / 16;
	entryoffset = dirPos % 16;

	if(dirClustNumber==0) {
		loadedDisk->Read_AbsoluteSector(firstRootDirSect+logentsector,sectbuf);
	} else {
		tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
		/* A zero sector number can't happen */
		if(tmpsector == 0) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
	}
	dirPos++;
	dta.SetDirID(dirPos);

	/* Deleted file entry */
	if (sectbuf[entryoffset].entryname[0] == 0xe5) goto nextfile;

	/* End of directory list */
	if (sectbuf[entryoffset].entryname[0] == 0x00) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	memset(find_name,0,DOS_NAMELENGTH_ASCII);
	memset(extension,0,4);
	memcpy(find_name,&sectbuf[entryoffset].entryname[0],8);
	memcpy(extension,&sectbuf[entryoffset].entryname[8],3);
	trimString(&find_name[0]);
	trimString(&extension[0]);
	
	//if(!(sectbuf[entryoffset].attrib & DOS_ATTR_DIRECTORY))
	if (extension[0]!=0) {
		strcat(find_name, ".");
		strcat(find_name, extension);
	}

	/* Compare attributes to search attributes */

	//TODO What about attrs = DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY ?
	if (attrs == DOS_ATTR_VOLUME) {
		if (!(sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)) goto nextfile;
		DOS_Drive_Cache dirCache;
		dirCache.SetLabel(find_name, false, true);
	} else {
		if (~attrs & sectbuf[entryoffset].attrib & (DOS_ATTR_DIRECTORY | DOS_ATTR_VOLUME | DOS_ATTR_SYSTEM | DOS_ATTR_HIDDEN) ) goto nextfile;
	}


	/* Compare name to search pattern */
	if(!WildFileCmp(find_name,srch_pattern)) goto nextfile;

	//dta.SetResult(find_name, sectbuf[entryoffset].entrysize, sectbuf[entryoffset].crtDate, sectbuf[entryoffset].crtTime, sectbuf[entryoffset].attrib);

	dta.SetResult(find_name, sectbuf[entryoffset].entrysize, sectbuf[entryoffset].modDate, sectbuf[entryoffset].modTime, sectbuf[entryoffset].attrib);

	memcpy(foundEntry, &sectbuf[entryoffset], sizeof(direntry));

	return true;
}

bool fatDrive::FindNext(DOS_DTA &dta) {
	direntry dummyClust;

	return FindNextInternal(dta.GetDirIDCluster(), dta, &dummyClust);
}

bool fatDrive::GetFileAttr(const char *name, Bit16u *attr) {
	direntry fileEntry;
	Bit32u dirClust, subEntry;
	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		char dirName[DOS_NAMELENGTH_ASCII];
		char pathName[11];

		/* Can we even get the name of the directory itself? */
		if(!getEntryName(name, &dirName[0])) return false;
		convToDirFile(&dirName[0], &pathName[0]);

		/* Get parent directory starting cluster */
		if(!getDirClustNum(name, &dirClust, true)) return false;

		/* Find directory entry in parent directory */
		Bit32s fileidx = 2;
		if (dirClust==0) fileidx = 0;	// root directory
		Bit32s last_idx=0; 
		while(directoryBrowse(dirClust, &fileEntry, fileidx, last_idx)) {
			if(memcmp(&fileEntry.entryname, &pathName[0], 11) == 0) {
				*attr=fileEntry.attrib;
				return true;
			}
			fileidx++;
		}
		return false;
	} else *attr=fileEntry.attrib;
	return true;
}

bool fatDrive::directoryBrowse(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum, Bit32s start/*=0*/) {
	direntry sectbuf[16];	/* 16 directory entries per sector */
	Bit32u logentsector;	/* Logical entry sector */
	Bit32u entryoffset = 0;	/* Index offset within sector */
	Bit32u tmpsector;
	Bit16u dirPos = 0;

	while(entNum>=0) {

		logentsector = dirPos / 16;
		entryoffset = dirPos % 16;

		if(dirClustNumber==0) {
			if(dirPos >= bootbuffer.rootdirentries) return false;
			tmpsector = firstRootDirSect+logentsector;
			loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen */
			if(tmpsector == 0) return false;
			loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
		}
		dirPos++;


		/* End of directory list */
		if (sectbuf[entryoffset].entryname[0] == 0x00) return false;
		--entNum;
	}

	memcpy(useEntry, &sectbuf[entryoffset],sizeof(direntry));
	return true;
}

bool fatDrive::directoryChange(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum) {
	direntry sectbuf[16];	/* 16 directory entries per sector */
	Bit32u logentsector;	/* Logical entry sector */
	Bit32u entryoffset = 0;	/* Index offset within sector */
	Bit32u tmpsector = 0;
	Bit16u dirPos = 0;
	
	while(entNum>=0) {
		
		logentsector = dirPos / 16;
		entryoffset = dirPos % 16;

		if(dirClustNumber==0) {
			if(dirPos >= bootbuffer.rootdirentries) return false;
			tmpsector = firstRootDirSect+logentsector;
			loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen */
			if(tmpsector == 0) return false;
			loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
		}
		dirPos++;


		/* End of directory list */
		if (sectbuf[entryoffset].entryname[0] == 0x00) return false;
		--entNum;
	}
	if(tmpsector != 0) {
        memcpy(&sectbuf[entryoffset], useEntry, sizeof(direntry));
		loadedDisk->Write_AbsoluteSector(tmpsector, sectbuf);
        return true;
	} else {
		return false;
	}
}

bool fatDrive::addDirectoryEntry(Bit32u dirClustNumber, direntry useEntry) {
	direntry sectbuf[16]; /* 16 directory entries per sector */
	Bit32u logentsector; /* Logical entry sector */
	Bit32u entryoffset;  /* Index offset within sector */
	Bit32u tmpsector;
	Bit16u dirPos = 0;
	
	for(;;) {
		
		logentsector = dirPos / 16;
		entryoffset = dirPos % 16;

		if(dirClustNumber==0) {
			if(dirPos >= bootbuffer.rootdirentries) return false;
			tmpsector = firstRootDirSect+logentsector;
			loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen - we need to allocate more room for this directory*/
			if(tmpsector == 0) {
				Bit32u newClust;
				newClust = appendCluster(dirClustNumber);
				if(newClust == 0) return false;
				/* Try again to get tmpsector */
				tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
				if(tmpsector == 0) return false; /* Give up if still can't get more room for directory */
			}
			loadedDisk->Read_AbsoluteSector(tmpsector,sectbuf);
		}
		dirPos++;

		/* Deleted file entry or end of directory list */
		if ((sectbuf[entryoffset].entryname[0] == 0xe5) || (sectbuf[entryoffset].entryname[0] == 0x00)) {
			sectbuf[entryoffset] = useEntry;
			loadedDisk->Write_AbsoluteSector(tmpsector,sectbuf);
			break;
		}
	}

	return true;
}

void fatDrive::zeroOutCluster(Bit32u clustNumber) {
	Bit8u secBuffer[512];

	memset(&secBuffer[0], 0, 512);

	int i;
	for(i=0;i<bootbuffer.sectorspercluster;i++) {
		loadedDisk->Write_AbsoluteSector(getAbsoluteSectFromChain(clustNumber,i), &secBuffer[0]);
	}
}

bool fatDrive::MakeDir(const char *dir) {
	Bit32u dummyClust, dirClust;
	direntry tmpentry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11];

	/* Can we even get the name of the directory itself? */
	if(!getEntryName(dir, &dirName[0])) return false;
	convToDirFile(&dirName[0], &pathName[0]);

	/* Fail to make directory if already exists */
	if(getDirClustNum(dir, &dummyClust, false)) return false;

	dummyClust = getFirstFreeClust();
	/* No more space */
	if(dummyClust == 0) return false;
	
	if(!allocateCluster(dummyClust, 0)) return false;

	zeroOutCluster(dummyClust);

	/* Can we find the base directory? */
	if(!getDirClustNum(dir, &dirClust, true)) return false;
	
	/* Add the new directory to the base directory */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, &pathName[0], 11);
	tmpentry.loFirstClust = (Bit16u)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (Bit16u)(dummyClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
	addDirectoryEntry(dirClust, tmpentry);

	/* Add the [.] and [..] entries to our new directory*/
	/* [.] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, ".          ", 11);
	tmpentry.loFirstClust = (Bit16u)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (Bit16u)(dummyClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
	addDirectoryEntry(dummyClust, tmpentry);

	/* [..] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, "..         ", 11);
	tmpentry.loFirstClust = (Bit16u)(dirClust & 0xffff);
	tmpentry.hiFirstClust = (Bit16u)(dirClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
	addDirectoryEntry(dummyClust, tmpentry);

	return true;
}

bool fatDrive::RemoveDir(const char *dir) {
	Bit32u dummyClust, dirClust;
	direntry tmpentry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11];

	/* Can we even get the name of the directory itself? */
	if(!getEntryName(dir, &dirName[0])) return false;
	convToDirFile(&dirName[0], &pathName[0]);

	/* Get directory starting cluster */
	if(!getDirClustNum(dir, &dummyClust, false)) return false;

	/* Can't remove root directory */
	if(dummyClust == 0) return false;

	/* Get parent directory starting cluster */
	if(!getDirClustNum(dir, &dirClust, true)) return false;

	/* Check to make sure directory is empty */
	Bit32u filecount = 0;
	/* Set to 2 to skip first 2 entries, [.] and [..] */
	Bit32s fileidx = 2;
	while(directoryBrowse(dummyClust, &tmpentry, fileidx)) {
		/* Check for non-deleted files */
		if(tmpentry.entryname[0] != 0xe5) filecount++;
		fileidx++;
	}

	/* Return if directory is not empty */
	if(filecount > 0) return false;

	/* Find directory entry in parent directory */
	if (dirClust==0) fileidx = 0;	// root directory
	else fileidx = 2;
	bool found = false;
	while(directoryBrowse(dirClust, &tmpentry, fileidx)) {
		if(memcmp(&tmpentry.entryname, &pathName[0], 11) == 0) {
			found = true;
			tmpentry.entryname[0] = 0xe5;
			directoryChange(dirClust, &tmpentry, fileidx);
			deleteClustChain(dummyClust);

			break;
		}
		fileidx++;
	}

	if(!found) return false;

	return true;
}

bool fatDrive::Rename(const char * oldname, const char * newname) {
	direntry fileEntry1;
	Bit32u dirClust1, subEntry1;
	if(!getFileDirEntry(oldname, &fileEntry1, &dirClust1, &subEntry1)) return false;
	/* File to be renamed really exists */

	direntry fileEntry2;
	Bit32u dirClust2, subEntry2;

	/* Check if file already exists */
	if(!getFileDirEntry(newname, &fileEntry2, &dirClust2, &subEntry2)) {
		/* Target doesn't exist, can rename */

		char dirName2[DOS_NAMELENGTH_ASCII];
		char pathName2[11];
		/* Can we even get the name of the file itself? */
		if(!getEntryName(newname, &dirName2[0])) return false;
		convToDirFile(&dirName2[0], &pathName2[0]);

		/* Can we find the base directory? */
		if(!getDirClustNum(newname, &dirClust2, true)) return false;
		memcpy(&fileEntry2, &fileEntry1, sizeof(direntry));
		memcpy(&fileEntry2.entryname, &pathName2[0], 11);
		addDirectoryEntry(dirClust2, fileEntry2);

		/* Check if file exists now */
		if(!getFileDirEntry(newname, &fileEntry2, &dirClust2, &subEntry2)) return false;

		/* Remove old entry */
		fileEntry1.entryname[0] = 0xe5;
		directoryChange(dirClust1, &fileEntry1, subEntry1);

		return true;
	}

	/* Target already exists, fail */
	return false;
}

bool fatDrive::TestDir(const char *dir) {
	Bit32u dummyClust;
	return getDirClustNum(dir, &dummyClust, false);
}

