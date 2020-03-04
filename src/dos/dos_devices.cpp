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
#include "regs.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"
#include "parport.h"
#include "drives.h" //Wildcmp
/* Include all the devices */

#include "dev_con.h"


DOS_Device * Devices[DOS_DEVICES] = {NULL};

class device_NUL : public DOS_Device {
public:
	device_NUL() { SetName("NUL"); };
	virtual bool Read(Bit8u * data,Bit16u * size) {
        (void)data; // UNUSED
		*size = 0; //Return success and no data read. 
//		LOG(LOG_IOCTL,LOG_NORMAL)("%s:READ",GetName());
		return true;
	}
	virtual bool Write(const Bit8u * data,Bit16u * size) {
        (void)data; // UNUSED
        (void)size; // UNUSED
//		LOG(LOG_IOCTL,LOG_NORMAL)("%s:WRITE",GetName());
		return true;
	}
	virtual bool Seek(Bit32u * pos,Bit32u type) {
        (void)type;
        (void)pos;
//		LOG(LOG_IOCTL,LOG_NORMAL)("%s:SEEK",GetName());
		return true;
	}
	virtual bool Close() { return true; }
	virtual Bit16u GetInformation(void) { return 0x8084; }
	virtual bool ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
	virtual bool WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
};

class device_PRN : public DOS_Device {
public:
	device_PRN() {
		SetName("PRN");
	}
	bool Read(Bit8u * data,Bit16u * size) {
        (void)data; // UNUSED
        (void)size; // UNUSED
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool Write(const Bit8u * data,Bit16u * size) {
		for(int i = 0; i < 3; i++) {
			// look up a parallel port
			if(parallelPortObjects[i] != NULL) {
				// send the data
				for (Bit16u j=0; j<*size; j++) {
					if(!parallelPortObjects[i]->Putchar(data[j])) return false;
				}
				return true;
			}
		}
		return false;
	}
	bool Seek(Bit32u * pos,Bit32u type) {
        (void)type; // UNUSED
		*pos = 0;
		return true;
	}
	Bit16u GetInformation(void) {
		return 0x80A0;
	}
	bool Close() {
		return false;
	}
};

bool DOS_Device::Read(Bit8u * data,Bit16u * size) {
	return Devices[devnum]->Read(data,size);
}

bool DOS_Device::Write(const Bit8u * data,Bit16u * size) {
	return Devices[devnum]->Write(data,size);
}

bool DOS_Device::Seek(Bit32u * pos,Bit32u type) {
	return Devices[devnum]->Seek(pos,type);
}

bool DOS_Device::Close() {
	return Devices[devnum]->Close();
}

Bit16u DOS_Device::GetInformation(void) { 
	return Devices[devnum]->GetInformation();
}

bool DOS_Device::ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	return Devices[devnum]->ReadFromControlChannel(bufptr,size,retcode);
}

bool DOS_Device::WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	return Devices[devnum]->WriteToControlChannel(bufptr,size,retcode);
}

DOS_File::DOS_File(const DOS_File& orig) {
	flags=orig.flags;
	time=orig.time;
	date=orig.date;
	attr=orig.attr;
	refCtr=orig.refCtr;
	open=orig.open;
	hdrive=orig.hdrive;
    drive = 0;
    newtime = false;
	name=0;
	if(orig.name) {
		name=new char [strlen(orig.name) + 1];strcpy(name,orig.name);
	}
}

DOS_File& DOS_File::operator= (const DOS_File& orig) {
    if (this != &orig) {
        flags = orig.flags;
        time = orig.time;
        date = orig.date;
        attr = orig.attr;
        refCtr = orig.refCtr;
        open = orig.open;
        hdrive = orig.hdrive;
        drive = orig.drive;
        newtime = orig.newtime;
        if (name) {
            delete[] name; name = 0;
        }
        if (orig.name) {
            name = new char[strlen(orig.name) + 1]; strcpy(name, orig.name);
        }
    }
    return *this;
}

Bit8u DOS_FindDevice(char const * name) {
	/* should only check for the names before the dot and spacepadded */
	char fullname[DOS_PATHLENGTH];Bit8u drive;
//	if(!name || !(*name)) return DOS_DEVICES; //important, but makename does it
	if (!DOS_MakeName(name,fullname,&drive)) return DOS_DEVICES;

	char* name_part = strrchr(fullname,'\\');
	if(name_part) {
		*name_part++ = 0;
		//Check validity of leading directory.
		if(!Drives[drive]->TestDir(fullname)) return DOS_DEVICES;
	} else name_part = fullname;
   
	char* dot = strrchr(name_part,'.');
	if(dot) *dot = 0; //no ext checking

	static char com[5] = { 'C','O','M','1',0 };
	static char lpt[5] = { 'L','P','T','1',0 };
	// AUX is alias for COM1 and PRN for LPT1
	// A bit of a hack. (but less then before).
	// no need for casecmp as makename returns uppercase
	if (strcmp(name_part, "AUX") == 0) name_part = com;
	if (strcmp(name_part, "PRN") == 0) name_part = lpt;

	/* loop through devices */
	for(Bit8u index = 0;index < DOS_DEVICES;index++) {
		if (Devices[index]) {
			if (WildFileCmp(name_part,Devices[index]->name)) return index;
		}
	}
	return DOS_DEVICES;
}


void DOS_AddDevice(DOS_Device * adddev) {
//Caller creates the device. We store a pointer to it
//TODO Give the Device a real handler in low memory that responds to calls
	if (adddev == NULL) E_Exit("DOS_AddDevice with null ptr");
	for(Bitu i = 0; i < DOS_DEVICES;i++) {
		if (Devices[i] == NULL){
//			LOG_MSG("DOS_AddDevice %s (%p)\n",adddev->name,(void*)adddev);
			Devices[i] = adddev;
			Devices[i]->SetDeviceNumber(i);
			return;
		}
	}
	E_Exit("DOS:Too many devices added");
}

void DOS_DelDevice(DOS_Device * dev) {
// We will destroy the device if we find it in our list.
// TODO:The file table is not checked to see the device is opened somewhere!
	if (dev == NULL) return E_Exit("DOS_DelDevice with null ptr");
	for (Bitu i = 0; i <DOS_DEVICES;i++) {
		if (Devices[i] == dev) { /* NTS: The mainline code deleted by matching names??? Why? */
//			LOG_MSG("DOS_DelDevice %s (%p)\n",dev->name,(void*)dev);
			delete Devices[i];
			Devices[i] = 0;
			return;
		}
	}

	/* hm. unfortunately, too much code in DOSBox assumes that we delete the object.
	 * prior to this fix, failure to delete caused a memory leak */
	LOG_MSG("WARNING: DOS_DelDevice() failed to match device object '%s' (%p). Deleting anyway\n",dev->name,(void*)dev);
	delete dev;
}

void DOS_ShutdownDevices(void) {
	for (Bitu i=0;i < DOS_DEVICES;i++) {
		if (Devices[i] != NULL) {
//			LOG_MSG("DOS: Shutting down device %s (%p)\n",Devices[i]->name,(void*)Devices[i]);
			delete Devices[i];
			Devices[i] = NULL;
		}
	}

    /* NTS: CON counts as a device */
    if (IS_PC98_ARCH) update_pc98_function_row(0);
}

// INT 29h emulation needs to keep track of CON
device_CON *DOS_CON = NULL;

bool ANSI_SYS_installed() {
    if (DOS_CON != NULL)
        return DOS_CON->ANSI_SYS_installed();

    return false;
}

void DOS_SetupDevices(void) {
	DOS_Device * newdev;
	DOS_CON=new device_CON(); newdev=DOS_CON;
	DOS_AddDevice(newdev);
	DOS_Device * newdev2;
	newdev2=new device_NUL();
	DOS_AddDevice(newdev2);
	DOS_Device * newdev3;
	newdev3=new device_PRN();
	DOS_AddDevice(newdev3);
}

/* PC-98 INT DC CL=0x10 AH=0x00 DL=cjar */
void PC98_INTDC_WriteChar(unsigned char b) {
    if (DOS_CON != NULL) {
        Bit16u sz = 1;

        DOS_CON->Write(&b,&sz);
    }
}

void INTDC_CL10h_AH03h(Bit16u raw) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH03h(raw);
}

void INTDC_CL10h_AH04h(void) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH04h();
}

void INTDC_CL10h_AH05h(void) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH05h();
}

void INTDC_CL10h_AH06h(Bit16u count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH06h(count);
}

void INTDC_CL10h_AH07h(Bit16u count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH07h(count);
}

void INTDC_CL10h_AH08h(Bit16u count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH08h(count);
}

void INTDC_CL10h_AH09h(Bit16u count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH09h(count);
}

Bitu INT29_HANDLER(void) {
    if (DOS_CON != NULL) {
        unsigned char b = reg_al;
        Bit16u sz = 1;

        DOS_CON->Write(&b,&sz);
    }

    return CBRET_NONE;
}

