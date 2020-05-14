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


#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "bios_disk.h"
#include "dos_inc.h"
#include "drives.h"

bool DOS_IOCTL(void) {
	Bitu handle=0;Bit8u drive=0;
	/* calls 0-4,6,7,10,12,16 use a file handle */
	if ((reg_al<4) || (reg_al==0x06) || (reg_al==0x07) || (reg_al==0x0a) || (reg_al==0x0c) || (reg_al==0x10)) {
		handle=RealHandle(reg_bx);
		if (handle>=DOS_FILES) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
		if (!Files[handle]) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
	} else if (reg_al<0x12) { 				/* those use a diskdrive except 0x0b */
		if (reg_al!=0x0b) {
			drive=reg_bl;if (!drive) drive = DOS_GetDefaultDrive();else drive--;
			if( (drive >= 2) && !(( drive < DOS_DRIVES ) && Drives[drive]) ) {
				DOS_SetError(DOSERR_INVALID_DRIVE);
				return false;
			}
		}
	} else {
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS:IOCTL Call %2X unhandled",reg_al);
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	}
	switch(reg_al) {
	case 0x00:		/* Get Device Information */
		if (Files[handle]->GetInformation() & 0x8000) {	//Check for device
			reg_dx=Files[handle]->GetInformation();
		} else {
			Bit8u hdrive=Files[handle]->GetDrive();
			if (hdrive==0xff) {
				LOG(LOG_IOCTL,LOG_NORMAL)("00:No drive set");
				hdrive=2;	// defaulting to C:
			}
			/* return drive number in lower 5 bits for block devices */
			reg_dx=(Files[handle]->GetInformation()&0xffe0)|hdrive;
		}
		reg_ax=reg_dx; //Destroyed officially
		return true;
	case 0x01:		/* Set Device Information */
		if (reg_dh != 0) {
			DOS_SetError(DOSERR_DATA_INVALID);
			return false;
		} else {
			if (Files[handle]->GetInformation() & 0x8000) {	//Check for device
				reg_al=(Bit8u)(Files[handle]->GetInformation() & 0xff);
			} else {
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				return false;
			}
		}
		return true;
	case 0x02:		/* Read from Device Control Channel */
		if (Files[handle]->GetInformation() & 0xc000) {
			/* is character device with IOCTL support */
			PhysPt bufptr=PhysMake(SegValue(ds),reg_dx);
			Bit16u retcode=0;
			if (((DOS_Device*)(Files[handle]))->ReadFromControlChannel(bufptr,reg_cx,&retcode)) {
				reg_ax=retcode;
				return true;
			}
		}
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	case 0x03:		/* Write to Device Control Channel */
		if (Files[handle]->GetInformation() & 0xc000) {
			/* is character device with IOCTL support */
			PhysPt bufptr=PhysMake(SegValue(ds),reg_dx);
			Bit16u retcode=0;
			if (((DOS_Device*)(Files[handle]))->WriteToControlChannel(bufptr,reg_cx,&retcode)) {
				reg_ax=retcode;
				return true;
			}
		}
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	case 0x06:      /* Get Input Status */
		if (Files[handle]->GetInformation() & 0x8000) {		//Check for device
			reg_al=(Files[handle]->GetInformation() & 0x40) ? 0x0 : 0xff;
		} else { // FILE
			Bit32u oldlocation=0;
			Files[handle]->Seek(&oldlocation, DOS_SEEK_CUR);
			Bit32u endlocation=0;
			Files[handle]->Seek(&endlocation, DOS_SEEK_END);
			if(oldlocation < endlocation){//Still data available
				reg_al=0xff;
			} else {
				reg_al=0x0; //EOF or beyond
			}
			Files[handle]->Seek(&oldlocation, DOS_SEEK_SET); //restore filelocation
			LOG(LOG_IOCTL,LOG_NORMAL)("06:Used Get Input Status on regular file with handle %d",(int)handle);
		}
		return true;
	case 0x07:		/* Get Output Status */
		LOG(LOG_IOCTL,LOG_NORMAL)("07:Fakes output status is ready for handle %d",(int)handle);
		reg_al=0xff;
		return true;
	case 0x08:		/* Check if block device removable */
		/* cdrom drives and drive a&b are removable */
		if (drive < 2) {
			if (Drives[drive])
				reg_ax=0;
			else {
				DOS_SetError(DOSERR_INVALID_DRIVE);
				return false;
			}
		} else if (!Drives[drive]->isRemovable()) reg_ax=1;
		else {
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
		return true;
	case 0x09:		/* Check if block device remote */
		if ((drive >= 2) && Drives[drive]->isRemote()) {
			reg_dx=0x1000;	// device is remote
			// undocumented bits always clear
		} else {
			reg_dx=0x0802;	// Open/Close supported; 32bit access supported (any use? fixes Fable installer)
			// undocumented bits from device attribute word
			// TODO Set bit 9 on drives that don't support direct I/O
		}
		reg_ax=0x300;
		return true;
	case 0x0A:		/* Is Device of Handle Remote? */
		reg_dx=0x8000;
		LOG(LOG_IOCTL,LOG_NORMAL)("0A:Faked output: device of handle %d is remote",(int)handle);
		return true;
	case 0x0B:		/* Set sharing retry count */
		if (reg_dx==0) {
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
		return true;
	case 0x0D:		/* Generic block device request */
		{
			if (drive < 2 && !Drives[drive]) {
				DOS_SetError(DOSERR_ACCESS_DENIED);
				return false;
			}
			if (reg_ch != 0x08 || Drives[drive]->isRemovable()) {
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				return false;
			}
			PhysPt ptr	= SegPhys(ds)+reg_dx;
			switch (reg_cl) {
			case 0x60:		/* Get Device parameters */
			{
				//mem_writeb(ptr+0,0);					// special functions (call value)
				mem_writeb(ptr+1,(drive>=2)?0x05:0x07);	// type: hard disk(5), 1.44 floppy(7)
				mem_writew(ptr+2,(drive>=2)?0x01:0x00);	// attributes: bit 0 set for nonremovable
				mem_writew(ptr+4,(drive>=2)?0x3FF:0x50);// num of cylinders
				mem_writeb(ptr+6,0x00);					// media type (00=other type)
				// bios parameter block following
				fatDrive *fdp;
				FAT_BootSector bootbuffer;
				bool usereal=false;
				if (!strncmp(Drives[drive]->GetInfo(),"fatDrive ",9)) {
					fdp = dynamic_cast<fatDrive*>(Drives[drive]);
					if (fdp != NULL) {
						bootbuffer=fdp->GetBootBuffer();
						if (bootbuffer.bpb.v.BPB_BytsPerSec&&bootbuffer.bpb.v.BPB_Media)
							usereal=true;
					}
				}
				if (usereal) {
					if (fdp->loadedDisk != NULL)
						mem_writew(ptr+4,fdp->loadedDisk->cylinders);               // num of cylinders
					mem_writew(ptr+7,bootbuffer.bpb.v.BPB_BytsPerSec);              // bytes per sector (Win3 File Mgr. uses it)
					mem_writew(ptr+9,bootbuffer.bpb.v.BPB_SecPerClus);              // sectors per cluster
					mem_writew(ptr+0xa,bootbuffer.bpb.v.BPB_RsvdSecCnt);			// number of reserved sectors
					mem_writew(ptr+0xc,bootbuffer.bpb.v.BPB_NumFATs);				// number of FATs
					mem_writew(ptr+0xd,bootbuffer.bpb.v.BPB_RootEntCnt);			// number of root entries
					mem_writew(ptr+0xf,bootbuffer.bpb.v.BPB_TotSec16);              // number of small sectors
					mem_writew(ptr+0x11,bootbuffer.bpb.v.BPB_Media);                // media type
					mem_writew(ptr+0x12,(uint16_t)bootbuffer.bpb.v.BPB_FATSz16);    // sectors per FAT
					mem_writew(ptr+0x14,(uint16_t)bootbuffer.bpb.v.BPB_SecPerTrk);  // sectors per track
					mem_writew(ptr+0x16,(uint16_t)bootbuffer.bpb.v.BPB_NumHeads);	// number of heads
					mem_writed(ptr+0x18,(uint32_t)bootbuffer.bpb.v.BPB_HiddSec);    // number of hidden sectors
					mem_writed(ptr+0x1c,(uint32_t)bootbuffer.bpb.v.BPB_TotSec32);   // number of big sectors
				} else {
					mem_writew(ptr+7,0x0200);									// bytes per sector (Win3 File Mgr. uses it)
					mem_writew(ptr+9,(drive>=2)?0x20:0x01);						// sectors per cluster
					mem_writew(ptr+0xa,0x0001);									// number of reserved sectors
					mem_writew(ptr+0xc,0x02);									// number of FATs
					mem_writew(ptr+0xd,(drive>=2)?0x0200:0x00E0);				// number of root entries
					mem_writew(ptr+0xf,(drive>=2)?0x0000:0x0B40);				// number of small sectors
					mem_writew(ptr+0x11,(drive>=2)?0xF8:0xF0);					// media type
					mem_writew(ptr+0x12,(drive>=2)?0x00C9:0x0009);				// sectors per FAT
					mem_writew(ptr+0x14,(drive>=2)?0x003F:0x0012);				// sectors per track
					mem_writew(ptr+0x16,(drive>=2)?0x10:0x02);					// number of heads
					mem_writed(ptr+0x18,0); 									// number of hidden sectors
					mem_writed(ptr+0x1c,(drive>=2)?0x31F11:0x00); 				// number of big sectors
				}
				for (int i=0x20; i<0x22; i++)
					mem_writeb(ptr+i,0);
				break;
			}
			case 0x42:  /* Format and verify logical device track (FORMAT.COM) */
				{
					/* 01h    WORD    number of disk head
					 * 03h    WORD    number of disk cylinder
					 * ---BYTE 00h bit 1 set---
					 * 05h    WORD    number of tracks to format */
					Bit8u flags = mem_readb(ptr+0);
					Bit16u head = mem_readw(ptr+1);
					Bit16u cyl = mem_readw(ptr+3);
					Bit16u ntracks = (flags & 0x1) ? mem_readw(ptr+5) : 1;
					Bit16u sect = 0;

					fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drive]);
					if (fdp == NULL) {
						DOS_SetError(DOSERR_ACCESS_DENIED);
						return false;
					}

					/* BUT: These are C/H/S values relative to the partition!
					 * FIXME: MS-DOS may not adjust sector value, or maybe it does...
					 * perhaps there is a reason Linux fdisk warns about sector alignment to sect/track for MS-DOS partitions? */
					{
						Bit32u adj = fdp->GetPartitionOffset();
						sect += adj % fdp->loadedDisk->sectors;
						adj /= fdp->loadedDisk->sectors;
						head += adj % fdp->loadedDisk->heads;
						adj /= fdp->loadedDisk->heads;
						cyl += adj;

						while (sect >= fdp->loadedDisk->sectors) {
							sect -= fdp->loadedDisk->sectors;
							head++;
						}
						while (head >= fdp->loadedDisk->heads) {
							head -= fdp->loadedDisk->heads;
							cyl++;
						}

						/* finally, MS-DOS counts sectors from 0 and BIOS INT 13h counts from 1 */
						sect++;
					}

					// STUB!
					LOG(LOG_IOCTL,LOG_DEBUG)("DOS:IOCTL Call 0D:42 Drive %2X pretending to format device track C/H/S=%u/%u/%u ntracks=%u",drive,cyl,head,sect,ntracks);
				}
				break;
			case 0x62:	/* Verify logical device track (FORMAT.COM) */
				{
					/* 01h    WORD    number of disk head
					 * 03h    WORD    number of disk cylinder
					 * 05h    WORD    number of tracks to verify */
					Bit16u head = mem_readw(ptr+1);
					Bit16u cyl = mem_readw(ptr+3);
					Bit16u ntracks = mem_readw(ptr+5);
					Bit16u sect = 0;

					fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drive]);
					if (fdp == NULL) {
						DOS_SetError(DOSERR_ACCESS_DENIED);
						return false;
					}

					/* BUT: These are C/H/S values relative to the partition!
					 * FIXME: MS-DOS may not adjust sector value, or maybe it does...
					 * perhaps there is a reason Linux fdisk warns about sector alignment to sect/track for MS-DOS partitions? */
					{
						Bit32u adj = fdp->GetPartitionOffset();
						sect += adj % fdp->loadedDisk->sectors;
						adj /= fdp->loadedDisk->sectors;
						head += adj % fdp->loadedDisk->heads;
						adj /= fdp->loadedDisk->heads;
						cyl += adj;

						while (sect >= fdp->loadedDisk->sectors) {
							sect -= fdp->loadedDisk->sectors;
							head++;
						}
						while (head >= fdp->loadedDisk->heads) {
							head -= fdp->loadedDisk->heads;
							cyl++;
						}

						/* finally, MS-DOS counts sectors from 0 and BIOS INT 13h counts from 1 */
						sect++;
					}

					// STUB!
					LOG(LOG_IOCTL,LOG_DEBUG)("DOS:IOCTL Call 0D:62 Drive %2X pretending to verify device track C/H/S=%u/%u/%u ntracks=%u",drive,cyl,head,sect,ntracks);
				}
				break;
			case 0x40:	/* Set Device parameters */
			case 0x46:	/* Set volume serial number */
				break;
			case 0x66:	/* Get volume serial number */
				{			
					char const* bufin=Drives[drive]->GetLabel();
					char buffer[11];memset(buffer,' ',11);

					char const* find_ext=strchr(bufin,'.');
					if (find_ext) {
						Bitu size=(Bitu)(find_ext-bufin);
						if (size>8) size=8;
						memcpy(buffer,bufin,size);
						find_ext++;
						memcpy(buffer+8,find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
					} else {
						memcpy(buffer,bufin,(strlen(bufin) > 8) ? 8 : strlen(bufin));
					}
			
					char buf2[8]={ 'F','A','T','1','6',' ',' ',' '};
					if(drive<2) buf2[4] = '2'; //FAT12 for floppies

					//mem_writew(ptr+0,0);			//Info level (call value)
					mem_writed(ptr+2,0x1234);		//Serial number
					MEM_BlockWrite(ptr+6,buffer,11);//volumename
					MEM_BlockWrite(ptr+0x11,buf2,8);//filesystem
				}
				break;
			case 0x41:  /* Write logical device track */
				{
					fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drive]);
					if (fdp == NULL) {
						DOS_SetError(DOSERR_ACCESS_DENIED);
						return false;
					}

					Bit8u sectbuf[SECTOR_SIZE_MAX];

					if (fdp->loadedDisk == NULL) {
						DOS_SetError(DOSERR_ACCESS_DENIED);
						return false;
					}

					/* (RBIL) [http://www.ctyme.com/intr/rb-2896.htm]
					 * Offset  Size    Description     (Table 01562)
					 * 00h    BYTE    special functions (reserved, must be zero)
					 * 01h    WORD    number of disk head
					 * 03h    WORD    number of disk cylinder
					 * 05h    WORD    number of first sector to read/write
					 * 07h    WORD    number of sectors
					 * 09h    DWORD   transfer address */
					Bit16u head = mem_readw(ptr+1);
					Bit16u cyl = mem_readw(ptr+3);
					Bit16u sect = mem_readw(ptr+5);
					Bit16u nsect = mem_readw(ptr+7);
					Bit32u xfer_addr = mem_readd(ptr+9);
					PhysPt xfer_ptr = ((xfer_addr>>16u)<<4u)+(xfer_addr&0xFFFFu);
					Bit16u sectsize = fdp->loadedDisk->getSectSize();

					/* BUT: These are C/H/S values relative to the partition!
					 * FIXME: MS-DOS may not adjust sector value, or maybe it does...
					 * perhaps there is a reason Linux fdisk warns about sector alignment to sect/track for MS-DOS partitions? */
					{
						Bit32u adj = fdp->GetPartitionOffset();
						sect += adj % fdp->loadedDisk->sectors;
						adj /= fdp->loadedDisk->sectors;
						head += adj % fdp->loadedDisk->heads;
						adj /= fdp->loadedDisk->heads;
						cyl += adj;

						while (sect >= fdp->loadedDisk->sectors) {
							sect -= fdp->loadedDisk->sectors;
							head++;
						}
						while (head >= fdp->loadedDisk->heads) {
							head -= fdp->loadedDisk->heads;
							cyl++;
						}

						/* finally, MS-DOS counts sectors from 0 and BIOS INT 13h counts from 1 */
						sect++;
					}

					LOG(LOG_IOCTL,LOG_DEBUG)("DOS:IOCTL Call 0D:41 Write Logical Device Track from Drive %2X C/H/S=%u/%u/%u num=%u from %04x:%04x sz=%u",
							drive,cyl,head,sect,nsect,xfer_addr >> 16,xfer_addr & 0xFFFF,sectsize);

					while (nsect > 0) {
						MEM_BlockRead(xfer_ptr,sectbuf,sectsize);

						Bit8u status = fdp->loadedDisk->Write_Sector(head,cyl,sect,sectbuf);
						if (status != 0) {
							LOG(LOG_IOCTL,LOG_DEBUG)("IOCTL 0D:61 write error at C/H/S %u/%u/%u",cyl,head,sect);
							DOS_SetError(DOSERR_ACCESS_DENIED);//FIXME
							return false;
						}

						xfer_ptr += sectsize;
						nsect--;
						sect++;
					}
				}
				break;
			case 0x61:  /* Read logical device track */
				{
					fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drive]);
					if (fdp == NULL) {
						DOS_SetError(DOSERR_ACCESS_DENIED);
						return false;
					}

					Bit8u sectbuf[SECTOR_SIZE_MAX];

					if (fdp->loadedDisk == NULL) {
						DOS_SetError(DOSERR_ACCESS_DENIED);
						return false;
					}

					/* (RBIL) [http://www.ctyme.com/intr/rb-2896.htm]
					 * Offset  Size    Description     (Table 01562)
					 * 00h    BYTE    special functions (reserved, must be zero)
					 * 01h    WORD    number of disk head
					 * 03h    WORD    number of disk cylinder
					 * 05h    WORD    number of first sector to read/write
					 * 07h    WORD    number of sectors
					 * 09h    DWORD   transfer address */
					Bit16u head = mem_readw(ptr+1);
					Bit16u cyl = mem_readw(ptr+3);
					Bit16u sect = mem_readw(ptr+5);
					Bit16u nsect = mem_readw(ptr+7);
					Bit32u xfer_addr = mem_readd(ptr+9);
					PhysPt xfer_ptr = ((xfer_addr>>16u)<<4u)+(xfer_addr&0xFFFFu);
					Bit16u sectsize = fdp->loadedDisk->getSectSize();

					/* BUT: These are C/H/S values relative to the partition!
					 * FIXME: MS-DOS may not adjust sector value, or maybe it does...
					 * perhaps there is a reason Linux fdisk warns about sector alignment to sect/track for MS-DOS partitions? */
					{
						Bit32u adj = fdp->GetPartitionOffset();;
						sect += adj % fdp->loadedDisk->sectors;
						adj /= fdp->loadedDisk->sectors;
						head += adj % fdp->loadedDisk->heads;
						adj /= fdp->loadedDisk->heads;
						cyl += adj;

						while (sect >= fdp->loadedDisk->sectors) {
							sect -= fdp->loadedDisk->sectors;
							head++;
						}
						while (head >= fdp->loadedDisk->heads) {
							head -= fdp->loadedDisk->heads;
							cyl++;
						}

						/* finally, MS-DOS counts sectors from 0 and BIOS INT 13h counts from 1 */
						sect++;
					}

					LOG(LOG_IOCTL,LOG_DEBUG)("DOS:IOCTL Call 0D:61 Read Logical Device Track from Drive %2X C/H/S=%u/%u/%u num=%u to %04x:%04x sz=%u",
							drive,cyl,head,sect,nsect,xfer_addr >> 16,xfer_addr & 0xFFFF,sectsize);

					while (nsect > 0) {
						Bit8u status = fdp->loadedDisk->Read_Sector(head,cyl,sect,sectbuf);
						if (status != 0) {
							LOG(LOG_IOCTL,LOG_DEBUG)("IOCTL 0D:61 read error at C/H/S %u/%u/%u",cyl,head,sect);
							DOS_SetError(DOSERR_ACCESS_DENIED);//FIXME
							return false;
						}

						MEM_BlockWrite(xfer_ptr,sectbuf,sectsize);
						xfer_ptr += sectsize;
						nsect--;
						sect++;
					}
				}
				break;
			case 0x4A:
			case 0x4B:
			case 0x6A:
			case 0x6B:
				LOG(LOG_IOCTL,LOG_ERROR)("DOS:IOCTL Call 0D:%2X Drive %2X volume/drive locking IOCTL, faking it",reg_cl,drive);
				break;
			default	:	
				LOG(LOG_IOCTL,LOG_ERROR)("DOS:IOCTL Call 0D:%2X Drive %2X unhandled",reg_cl,drive);
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				return false;
			}
			reg_ax=0;
			return true;
		}
	case 0x0E:			/* Get Logical Drive Map */
		if (drive < 2) {
			if (Drives[drive]) reg_al=drive+1;
			else reg_al=1;
		} else if (Drives[drive]->isRemovable()) {
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		} else reg_al=0;	/* Only 1 logical drive assigned */
		reg_ah=0x07;
		return true;
	default:
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS:IOCTL Call %2X unhandled",reg_al);
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		break;
	}
	return false;
}


bool DOS_GetSTDINStatus(void) {
	Bit32u handle=RealHandle(STDIN);
	if (handle==0xFF) return false;
	if (Files[handle] && (Files[handle]->GetInformation() & 64)) return false;
	return true;
}
