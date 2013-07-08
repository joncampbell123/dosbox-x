/*
 * IDE ATA/ATAPI and controller emulation for DOSBox-X
 * (C) 2012 Jonathan Campbell

 * [insert open source license here]
 */

/* $Id: ide.cpp,v 1.49 2009-04-10 09:53:04 c2woody Exp $ */

/* TODO: Figure out what brand of crack cocaine the Windows 95 devs were smoking and then adjust
	 ATA emulation so Windows 95 is able to make use of the IDE controller for the C: drive.

	 What do you mean, crack cocaine? Well, I wasted half a Sunday trying to get Windows 95
	 to talk to our ATA emulation, and here is what I found:

            * Windows 95 enumerates the IDE bus by issuing DEVICE RESET to everyone. It successfully
	      detects our ATAPI CD-ROM emulation this way, BTW, and it seems to note which ones
	      respond as normal hard drives.

	    * BUT----rather than act on those findings, it instead reads CMOS non-volatile RAM and
	      lets that determine whether or not it even attempts to query the hard disk. If byte
	      0x12 says that the BIOS found a primary drive, then it will blindly no-questions-asked
	      issue a READ to the primary interface. If the byte says a secondary is present, it
	      will blindly issue a READ to the secondary interface. Prior to fixing CMOS emulation,
	      i would see Windows 95 issue IDENTIFY DEVICE to the primary master but then turn right
	      around and issue a READ to the non-existent primary slave (WTF?!?!?!).

            * NOW: If Windows 95 determines the drive is there by CMOS, then it issues a C/H/S read
	      of the boot sector, then it turns around and flags the IDE controller (!?!?!) as faulty
	      and falls back to MS-DOS compatibility mode. It doesn't even bother to ask the drive
	      for identification. It doesn't matter that it got a valid sector read from us.

	      You will lose CD-ROM functionality as well if you attached the virtual CD-ROM drive to
	      the same controller.

	    * If Windows 95 does NOT use the CMOS byte to determine the hard disk, then we will see
	      Windows 95 come around and issue IDENTIFY DEVICE, then move on and completely ignore
	      the hard disk and use MS-DOS compatibility mode. It doesn't matter what we return for
	      IDENTIFY DEVICE. I spent all fucking morning tweaking and twiddling and then finally
	      outright copying an actual hard drive's response to no avail. So tell me Microsoft---
	      if you see a hard disk is there and you're bothering to ask for device details, why the
	      fucking hell aren't you then going out and USING the drive?!??

	 Damned if you do, damned if you don't.

	 What do you expect from a company that was also mostly responsible for the ACPI BIOS standard?
	 A *SANE* industry standard implementation? HAH!

	 It's true at this point I only implemented the READ SECTOR command, but that should not be the
	 reason for such failure since I have yet to see Windows 95 actually put some effort into talking
	 to our ATA emulation. I would exepct to see it at least READ from us and then try to WRITE at
	 some point! Frankly right now I don't fucking care and so Windows 95 is just going to have to
	 use INT 13h and deal with it because I have better things to do than waste my fucking time over
	 whatever stupid minute detail Windows 95 is fussing about.

	 In the mean time I recommend not attaching hard disk images to the IDE emulation.

         ATAPI CD-ROM read delays: spin-up/spin-down and lengthening of delay based on sector count

   Finally: This code is enough to satisfy MSCDEX.EXE and Windows 95 install, but for whatever
   reason Windows 95 appears to issue IDENTIFY COMMAND to each device (including ATAPI CD-ROM)
   and then completely ignore the IDE bus and use "MS-DOS compatibility mode". Why? */

#include <math.h>
#include <assert.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "ide.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "bios_disk.h"
#include "../src/dos/cdrom.h"

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
#endif

#define MAX_IDE_CONTROLLERS 4

static unsigned char init_ide = 0;

static const unsigned char IDE_default_IRQs[4] = {
	14,	/* primary */
	15,	/* secondary */
	11,	/* tertiary */
	10	/* quaternary */
};

static const unsigned short IDE_default_bases[4] = {
	0x1F0,	/* primary */
	0x170,	/* secondary */
	0x1E8,	/* tertiary */
	0x168	/* quaternary */
};

static const unsigned short IDE_default_alts[4] = {
	0x3F6,	/* primary */
	0x376,	/* secondary */
	0x3EE,	/* tertiary */
	0x36E	/* quaternary */
};

static void ide_altio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu ide_altio_r(Bitu port,Bitu iolen);
static void ide_baseio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu ide_baseio_r(Bitu port,Bitu iolen);
bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom);

enum IDEDeviceType {
	IDE_TYPE_NONE,
	IDE_TYPE_HDD=1,
	IDE_TYPE_CDROM
};

enum IDEDeviceState {
	IDE_DEV_READY=0,
	IDE_DEV_SELECT_WAIT,
	IDE_DEV_CONFUSED,
	IDE_DEV_BUSY,
	IDE_DEV_DATA_READ,
	IDE_DEV_DATA_WRITE,
	IDE_DEV_ATAPI_PACKET_COMMAND,
	IDE_DEV_ATAPI_BUSY
};

enum {
	IDE_STATUS_BUSY=0x80,
	IDE_STATUS_DRIVE_READY=0x40,
	IDE_STATUS_DRIVE_SEEK_COMPLETE=0x10,
	IDE_STATUS_DRQ=0x08,
	IDE_STATUS_ERROR=0x01
};

class IDEController;

static inline bool drivehead_is_lba48(uint8_t val) {
	return (val&0xE0) == 0x40;
}

static inline bool drivehead_is_lba(uint8_t val) {
	return (val&0xE0) == 0xE0;
}

static inline bool drivehead_is_chs(uint8_t val) {
	return (val&0xE0) == 0xA0;
}

class IDEDevice {
public:
	IDEController *controller;
	uint16_t feature,count,lba[3];
	uint8_t command,drivehead,status;
	enum IDEDeviceType type;
	bool allow_writing;
	bool motor_on;
	bool asleep;
	IDEDeviceState state;
	/* feature: 0x1F1 (Word 00h in ATA specs)
	     count: 0x1F2 (Word 01h in ATA specs)
	    lba[3]: 0x1F3 (Word 02h) 0x1F4 (Word 03h) and 0x1F5 (Word 04h)
	 drivehead: 0x1F6 (copy of last value written)
	   command: 0x1F7 (Word 05h)
	    status: 0x1F7 (value read back to IDE controller, including busy and drive ready bits as well as error status)

	In C/H/S modes lba[3] becomes lba[0]=sector lba[1]=cylinder-low lba[2]=cylinder-high and
	the code must read the 4-bit head number from drivehead[bits 3:0].

	"drivehead" in this struct is always maintained as a device copy of the controller's
	drivehead value. it is only updated on write, and not returned on read.

	"allow_writing" if set allows the DOS program/OS to write the registers. It is
	clear during command execution, obviously, so the state of the device is not confused
	while executing the command.

	Registers are 16-bit where applicable so future revisions of this code
	can support LBA48 commands */
public:
	/* tweakable parameters */
	double ide_select_delay;	/* time between writing 0x1F6 and drive readiness */
	double ide_spinup_delay;	/* time it takes to spin the hard disk motor up to speed */
	double ide_spindown_delay;	/* time it takes for hard disk motor to spin down */
	double ide_identify_command_delay;
public:
	IDEDevice(IDEController *c);
	virtual ~IDEDevice();
	virtual void host_reset();	/* IDE controller -> upon writing bit 2 of alt (0x3F6) */
	virtual void select(uint8_t ndh,bool switched_to);
	virtual void deselect();
	virtual void abort_error();
	virtual void interface_wakeup();
	virtual void writecommand(uint8_t cmd);
	virtual Bitu data_read(Bitu iolen);	/* read from 1F0h data port from IDE device */
	virtual void data_write(Bitu v,Bitu iolen);/* write to 1F0h data port to IDE device */
};

class IDEATADevice:public IDEDevice {
public:
	IDEATADevice(IDEController *c,unsigned char bios_disk_index);
	virtual ~IDEATADevice();
	virtual void writecommand(uint8_t cmd);
public:
	std::string id_serial;
	std::string id_firmware_rev;
	std::string id_model;
	unsigned char bios_disk_index;
	imageDisk *getBIOSdisk();
	void update_from_biosdisk();
	virtual Bitu data_read(Bitu iolen);	/* read from 1F0h data port from IDE device */
	virtual void data_write(Bitu v,Bitu iolen);/* write to 1F0h data port to IDE device */
	virtual void generate_identify_device();
	virtual void prepare_read(Bitu offset,Bitu size);
	virtual void io_completion();
public:
	Bitu heads,sects,cyls;
	unsigned char sector[512*128];
	Bitu sector_i,sector_total;
};

class IDEATAPICDROMDevice:public IDEDevice {
public:
	IDEATAPICDROMDevice(IDEController *c,unsigned char drive_index);
	virtual ~IDEATAPICDROMDevice();
	virtual void writecommand(uint8_t cmd);
public:
	std::string id_serial;
	std::string id_firmware_rev;
	std::string id_model;
	unsigned char drive_index;
	CDROM_Interface *getMSCDEXDrive();
	void update_from_cdrom();
	virtual Bitu data_read(Bitu iolen);	/* read from 1F0h data port from IDE device */
	virtual void data_write(Bitu v,Bitu iolen);/* write to 1F0h data port to IDE device */
	virtual void generate_identify_device();
	virtual void generate_mmc_inquiry();
	virtual void prepare_read(Bitu offset,Bitu size);
	virtual void atapi_io_completion();
	virtual void io_completion();
	virtual void atapi_cmd_completion();
	virtual void on_atapi_busy_time();
public:
	bool atapi_to_host;			/* if set, PACKET data transfer is to be read by host */
	Bitu host_maximum_byte_count;		/* host maximum byte count during PACKET transfer */
	std::string id_mmc_vendor_id;
	std::string id_mmc_product_id;
	std::string id_mmc_product_rev;
	Bitu LBA,TransferLength;
public:
	unsigned char sense[256];
	unsigned char atapi_cmd[12];
	unsigned char atapi_cmd_i,atapi_cmd_total;
	unsigned char sector[512*128];
	Bitu sector_i,sector_total;
};

class IDEController:public Module_base{
public:
	int IRQ;
	unsigned short alt_io;
	unsigned short base_io;
	unsigned char interface_index;
	IO_ReadHandleObject ReadHandler[8],ReadHandlerAlt[2];
	IO_WriteHandleObject WriteHandler[8],WriteHandlerAlt[2];
public:
	IDEDevice* device[2];		/* IDE devices (master, slave) */
	Bitu select,status,drivehead;	/* which is selected, status register (0x1F7) but ONLY if no device exists at selection, drive/head register (0x1F6) */
	bool interrupt_enable;		/* bit 1 of alt (0x3F6) */
	bool host_reset;		/* bit 2 of alt */
	bool irq_pending;
public:
	IDEController(Section* configuration,unsigned char index);
	void install_io_port();
	void raise_irq();
	void lower_irq();
	~IDEController();
};

static IDEController* idecontroller[MAX_IDE_CONTROLLERS]={NULL,NULL,NULL,NULL};

static void IDE_DelayedCommand(Bitu idx/*which IDE controller*/);

#ifdef C_DEBUG
//#define fprintf silent_fprintf
//static size_t silent_fprintf(FILE *f,const char *fmt,...) {
//}
#endif

/* when the ATAPI command has been accepted, and the timeout has passed */
void IDEATAPICDROMDevice::on_atapi_busy_time() {
	fprintf(stderr,"ATAPI busy time %02X\n",atapi_cmd[0]);
	switch (atapi_cmd[0]) {
		case 0x03: /* REQUEST SENSE */
			prepare_read(0,std::min((unsigned int)18,(unsigned int)host_maximum_byte_count));
			sense[0] = 0x70;/*RESPONSE CODE*/
			sense[2] = 0x00;/*SENSE KEY*/
			sense[7] = 10;
			sense[12] = 0;
			memcpy(sector,sense,18);

			feature = 0x00;
			state = IDE_DEV_DATA_READ;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

			/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
			lba[2] = sector_total >> 8;
			lba[1] = sector_total;

			controller->raise_irq();
			break;
		case 0x12: /* INQUIRY */
			/* NTS: the state of atapi_to_host doesn't seem to matter. */
			generate_mmc_inquiry();
			prepare_read(0,MIN((unsigned int)36,(unsigned int)host_maximum_byte_count));

			feature = 0x00;
			state = IDE_DEV_DATA_READ;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

			/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
			lba[2] = sector_total >> 8;
			lba[1] = sector_total;

			controller->raise_irq();
			break;
		case 0x28: /* READ(10) */
		case 0xA8: /* READ(12) */
			if (TransferLength == 0) {
				/* this is legal. the SCSI MMC standards say so.
				   and apparently, MSCDEX.EXE issues READ(10) commands with transfer length == 0
				   to test the drive, so we have to emulate this */
				feature = 0x00;
				count = 0x03; /* no more transfer */
				sector_total = 0;/*nothing to transfer */
				state = IDE_DEV_READY;
				status = IDE_STATUS_DRIVE_READY;
			}
			else {
				/* OK, try to read */
				CDROM_Interface *cdrom = getMSCDEXDrive();
				bool res = (cdrom != NULL ? cdrom->ReadSectorsHost(/*buffer*/sector,false,LBA,TransferLength) : false);
				if (res) {
					prepare_read(0,MIN((unsigned int)(TransferLength*2048),(unsigned int)host_maximum_byte_count));
					feature = 0x00;
					state = IDE_DEV_DATA_READ;
					status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;
				}
				else {
					feature = 0xF4; /* abort sense=0xF */
					count = 0x03; /* no more transfer */
					sector_total = 0;/*nothing to transfer */
					state = IDE_DEV_READY;
					status = IDE_STATUS_DRIVE_READY|IDE_STATUS_ERROR;
					/* TODO: write sense data */
				}
			}

			/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
			lba[2] = sector_total >> 8;
			lba[1] = sector_total;

			controller->raise_irq();
			break;
		case 0x42: /* READ SUB-CHANNEL */
			/* TODO: Flesh this code out, so far it's here to satisfy the testing code of MS-DOS CD-ROM drivers */
			/* TODO: Use track number in byte 6 */
			if (atapi_cmd[3] == 1 || 1) {
				prepare_read(0,MIN((unsigned int)4,(unsigned int)host_maximum_byte_count));
				sector[0] = 0x00;/*RESERVED*/
				sector[1] = 0x15;/*AUDIO STATUS*/
				sector[2] = 0x00;/*LENGTH*/
				sector[3] = 0x00;/*LENGTH*/
			}

			feature = 0x00;
			state = IDE_DEV_DATA_READ;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

			/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
			lba[2] = sector_total >> 8;
			lba[1] = sector_total;

			controller->raise_irq();
			break;
		case 0x43: /* READ TOC */
			/* TODO: Flesh this code out, so far it's here to satisfy the testing code of MS-DOS CD-ROM drivers */
			/* TODO: Use track number in byte 6, format field [3:0] byte 2, LBA mode (TIME=0) by bit 1 byte 1 */
			/* FIXME: The OAK CD-ROM driver I'm testing against uses this command when you read from CD-ROM.
				  what we're returning now causes it to say "the CD-ROM drive is not ready". Why?
				  what am I doing wrong? */
			if (atapi_cmd[3] == 1 || 1) {
				bool leadout = !(atapi_cmd[9] & 0x40);

				prepare_read(0,std::min((unsigned int)(4+8+(leadout?8:0)),(unsigned int)host_maximum_byte_count));
				sector[0] = 0x00;/*DATA LENGTH*/
				sector[1] = 10+(leadout?8:0);/*DATA LENGTH LSB*/
				sector[2] = 0x01;/*FIRST TRACK*/
				sector[3] = 0x01;/*LAST TRACK*/

				sector[4+0] = 0x00;/*RESERVED*/
				sector[4+1] = (1 << 4) | 4; /*ADR=1 CONTROL=4 (DATA)*/
				sector[4+2] = 1;/*TRACK*/
				sector[4+3] = 0x00;/*?*/
				sector[4+4] = 0x00;/*x*/
				sector[4+5] = 0;/*M*/
				sector[4+6] = 2;/*S*/
				sector[4+7] = 0;/*F*/

				if (leadout) {
					/* FIXME: Actually read the size of the ISO or get M:S:F of leadout */
					/* For now, we fabricate the leadout to make Windows 95 happy, because Win95
					   relies on this command to detect the total size of the CD-ROM  */
					sector[4+8+0] = 0x00;/*RESERVED*/
					sector[4+8+1] = (1 << 4) | 4; /*ADR=1 CONTROL=4 (DATA)*/
					sector[4+8+2] = 0xAA;/*TRACK*/
					sector[4+8+3] = 0x00;/*?*/
					sector[4+8+4] = 0x00;/*x*/
					sector[4+8+5] = 79;/*M*/
					sector[4+8+6] = 59;/*S*/
					sector[4+8+7] = 59;/*F*/
				}
			}

			feature = 0x00;
			state = IDE_DEV_DATA_READ;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

			/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
			lba[2] = sector_total >> 8;
			lba[1] = sector_total;

			controller->raise_irq();
			break;
		case 0x5A: /* MODE SENSE(10) */
			/* TODO: Flesh this out */
			prepare_read(0,MIN((unsigned int)256,(unsigned int)host_maximum_byte_count));
			memset(sector,0,256);

			feature = 0x00;
			state = IDE_DEV_DATA_READ;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

			/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
			lba[2] = sector_total >> 8;
			lba[1] = sector_total;

			controller->raise_irq();
			break;
		default:
			fprintf(stderr,"Unknown ATAPI command after busy wait. Why?\n");
			abort_error();
			controller->raise_irq();
			break;
	};

}

IDEATAPICDROMDevice::IDEATAPICDROMDevice(IDEController *c,unsigned char drive_index) : IDEDevice(c) {
	this->drive_index = drive_index;
	sector_i = sector_total = 0;

	memset(sense,0,sizeof(sense));

	type = IDE_TYPE_CDROM;
	id_serial = "123456789";
	id_firmware_rev = "0.74-X";
	id_model = "DOSBox Virtual CD-ROM";

	/* INQUIRY strings */
	id_mmc_vendor_id = "DOSBox";
	id_mmc_product_id = "Virtual CD-ROM";
	id_mmc_product_rev = "0.74-X";

#if 0
	id_mmc_vendor_id = "VBOX";
	id_mmc_product_id = "CD-ROM";
	id_mmc_product_rev = "1.0";
#endif
}

IDEATAPICDROMDevice::~IDEATAPICDROMDevice() {
}

void IDEATAPICDROMDevice::atapi_io_completion() {
//	fprintf(stderr,"ATAPI PACKET: IO complete (%lu/%lu)\n",
//		(unsigned long)sector_i,(unsigned long)sector_total);

	/* for most ATAPI PACKET commands, the transfer is done and we need to clear
	   all indication of a possible data transfer */

	count = 0x03; /* no more data (command/data=1, input/output=1) */
	status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
	state = IDE_DEV_READY;
	allow_writing = true;

	/* Apparently: real IDE ATAPI controllers fire another IRQ after the transfer.
	   And there are MS-DOS CD-ROM drivers that assume that. */
	controller->raise_irq();
}
	
void IDEATAPICDROMDevice::io_completion() {
//	fprintf(stderr,"ATAPI: IO complete (%lu/%lu)\n",
//		(unsigned long)sector_i,(unsigned long)sector_total);

	/* lower DRQ */
	status &= ~IDE_STATUS_DRQ;

	/* depending on the command, either continue it or finish up */
	switch (command) {
		case 0xA0:/*ATAPI PACKET*/
			atapi_io_completion();
			break;
		default: /* most commands: signal drive ready, return to ready state */
			/* NTS: Some MS-DOS CD-ROM drivers will loop endlessly if we never set "drive seek complete"
			        because they like to hit the device with DEVICE RESET (08h) whether or not it's
				a hard disk or CD-ROM drive */
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
			state = IDE_DEV_READY;
			allow_writing = true;
			break;
	}
}

void IDEATADevice::io_completion() {
//	fprintf(stderr,"HDD: IO complete (%lu/%lu)\n",
//		(unsigned long)sector_i,(unsigned long)sector_total);

	/* lower DRQ */
	status &= ~IDE_STATUS_DRQ;

	/* depending on the command, either continue it or finish up */
	switch (command) {
		case 0x20:/* READ SECTOR */
			/* having read the sector, increment the disk address. we do it in a way
			   the host will read back the "current" position.

			   believe it or not, Windows 95 expects this behavior, and will flag our
			   ATA emulation as faulty if it doesn't see it, making this our form of
			   sucking Microsoft cock. It also fits in with this link:

			   http://www.os2museum.com/wp/?p=935 */
			if (drivehead_is_lba(drivehead)) {
				if (((++lba[0])&0xFF) == 0) {/* increment. carry? */
					if (((++lba[1])&0xFF) == 0) {/*increment, carry? */
						if (((++lba[2])&0xFF) == 0) {/* increment, carry? */
							if ((drivehead&0xF) != 0xF) {/*bits 27:24 in drive/head */
								drivehead++;
							}
							else {
								abort_error();
								return;
							}
						}
					}
				}
			}
			else {
				/* Oh, joy. Address incrementation C/H/S style */
				if (((++lba[0])&0xFF) > sects) { /* increment sector */
					lba[0] = 1;
					/* if sector carry, increment head */
					if ((drivehead&0xF) == 0xF) {	/* if head carry, increment 7:0 of track */
						drivehead &= 0xF0;
						if (((++lba[1])&0xFF) == 0) { /* if 7:0 carry then incrment 15:8 */
							if (((++lba[2])&0xFF) == 0) {
								abort_error();
								return;
							}
						}
					}
					else {
						drivehead++;
					}
				}
			}

			/* OK, decrement count, increment address */
			/* NTS: Remember that count == 0 means the host wanted to transfer 256 sectors */
			if ((count&0xFF) == 0) count = 255;
			else if ((count&0xFF) == 1) {
				/* end of the transfer */
				count = 0;
				status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
				state = IDE_DEV_READY;
				allow_writing = true;
				return;
			}
			else count--;

			/* cause another delay, another sector read */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,2/*ms*/,controller->interface_index);
			break;
		default: /* most commands: signal drive ready, return to ready state */
			/* NTS: Some MS-DOS CD-ROM drivers will loop endlessly if we never set "drive seek complete"
			        because they like to hit the device with DEVICE RESET (08h) whether or not it's
				a hard disk or CD-ROM drive */
			count = 0;
			drivehead &= 0xF0;
			lba[0] = 0;
			lba[1] = lba[2] = 0;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
			state = IDE_DEV_READY;
			allow_writing = true;
			break;
	}
}

Bitu IDEATAPICDROMDevice::data_read(Bitu iolen) {
	Bitu w;

	if (state != IDE_DEV_DATA_READ) {
		fprintf(stderr,"IDE: Data read when not expecting it\n");
		return 0xFFFFUL;
	}
	if (!(status & IDE_STATUS_DRQ)) {
		fprintf(stderr,"IDE: Data read when DRQ=0\n");
		return 0xFFFFUL;
	}

	if (sector_i >= sector_total) {
		fprintf(stderr,"Sector read ptr beyond total\n");
		return 0xFFFFUL;
	}

//	fprintf(stderr,"READ %u/%u\n",sector_i,sector_total);

	/* TODO: The MS-DOS CD-ROM driver I'm testing against uses byte-wide I/O during identification?!? REALLY?!?
		 I thought you weren't supposed to do that! */
	if (iolen >= 2) {
		w = host_readw(sector+sector_i);
		sector_i += 2;
	}
	else if (iolen == 1) {
		w = sector[sector_i++];
	}

	if (sector_i >= sector_total)
		io_completion();

	return w;
}

/* TODO: Your code should also be paying attention to the "transfer length" field
         in many of the commands here. Right now it doesn't matter. */
void IDEATAPICDROMDevice::atapi_cmd_completion() {
#if 0
	fprintf(stderr,"ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
		atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11]);
#endif

	switch (atapi_cmd[0]) {
		case 0x00: /* TEST UNIT READY */
			count = 0x03;
			feature = 0x00;
			state = IDE_DEV_READY;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
			controller->raise_irq();
			break;
		case 0x03: /* REQUEST SENSE */
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,1/*ms*/,controller->interface_index);
			break;
		case 0x12: /* INQUIRY */
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,1/*ms*/,controller->interface_index);
			break;
		case 0xA8: /* READ(12) */
			/* FIXME: MSCDEX.EXE appears to test the drive by issuing READ(10) with transfer length == 0.
			          This is all well and good but our response seems to cause a temporary 2-3 second
				  pause for each attempt. Why? */
			LBA = ((Bitu)atapi_cmd[2] << 24UL) |
				((Bitu)atapi_cmd[3] << 16UL) |
				((Bitu)atapi_cmd[4] << 8UL) |
				((Bitu)atapi_cmd[5] << 0UL);
			TransferLength = ((Bitu)atapi_cmd[6] << 24UL) |
				((Bitu)atapi_cmd[7] << 16UL) |
				((Bitu)atapi_cmd[8] << 8UL) |
				((Bitu)atapi_cmd[9]);

			/* FIXME: We actually should NOT be capping the transfer length, but instead should
				  be breaking the larger transfer into smaller DRQ block transfers like
				  most IDE ATAPI drives do. Writing the test IDE code taught me that if you
				  go to most drives and request a transfer length of 0xFFFE the drive will
				  happily set itself up to transfer that many sectors in one IDE command! */
			/* NTS: In case you're wondering, it's legal to issue READ(10) with transfer length == 0.
				MSCDEX.EXE does it when starting up, for example */
			if ((TransferLength*2048) > sizeof(sector))
				TransferLength = sizeof(sector)/2048;

			fprintf(stderr,"Read(12) sector %lu len=%lu\n",(unsigned long)LBA,(unsigned long)TransferLength);
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			/* TODO: Emulate CD-ROM spin-up delay, and seek delay */
			PIC_AddEvent(IDE_DelayedCommand,3/*ms*/,controller->interface_index);
			break;
		case 0x28: /* READ(10) */
			/* FIXME: MSCDEX.EXE appears to test the drive by issuing READ(10) with transfer length == 0.
			          This is all well and good but our response seems to cause a temporary 2-3 second
				  pause for each attempt. Why? */
			LBA = ((Bitu)atapi_cmd[2] << 24UL) |
				((Bitu)atapi_cmd[3] << 16UL) |
				((Bitu)atapi_cmd[4] << 8UL) |
				((Bitu)atapi_cmd[5] << 0UL);
			TransferLength = ((Bitu)atapi_cmd[7] << 8) |
				((Bitu)atapi_cmd[8]);

			/* FIXME: We actually should NOT be capping the transfer length, but instead should
				  be breaking the larger transfer into smaller DRQ block transfers like
				  most IDE ATAPI drives do. Writing the test IDE code taught me that if you
				  go to most drives and request a transfer length of 0xFFFE the drive will
				  happily set itself up to transfer that many sectors in one IDE command! */
			/* NTS: In case you're wondering, it's legal to issue READ(10) with transfer length == 0.
				MSCDEX.EXE does it when starting up, for example */
			if ((TransferLength*2048) > sizeof(sector))
				TransferLength = sizeof(sector)/2048;

			fprintf(stderr,"Read(10) sector %lu len=%lu\n",(unsigned long)LBA,(unsigned long)TransferLength);
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			/* TODO: Emulate CD-ROM spin-up delay, and seek delay */
			PIC_AddEvent(IDE_DelayedCommand,3/*ms*/,controller->interface_index);
			break;
		case 0x42: /* READ SUB-CHANNEL */
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,1/*ms*/,controller->interface_index);
			break;
		case 0x43: /* READ TOC */
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,1/*ms*/,controller->interface_index);
			break;
		case 0x5A: /* MODE SENSE(10) */
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,1/*ms*/,controller->interface_index);
			break;
		default:
#if 1
			fprintf(stderr,"---UNKNOWN\n");
#else
			/* we don't know the command, immediately return an error */
			fprintf(stderr,"Unknown ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
				atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11]);
#endif
			abort_error();
			count = 0x03; /* no more data (command/data=1, input/output=1) */
			feature = 0xF4;
			controller->raise_irq();
			break;
	};
}

void IDEATAPICDROMDevice::data_write(Bitu v,Bitu iolen) {
	if (state == IDE_DEV_ATAPI_PACKET_COMMAND) {
		if (atapi_cmd_i < atapi_cmd_total)
			atapi_cmd[atapi_cmd_i++] = v;
		if (iolen >= 2 && atapi_cmd_i < atapi_cmd_total)
			atapi_cmd[atapi_cmd_i++] = v >> 8;

		if (atapi_cmd_i >= atapi_cmd_total)
			atapi_cmd_completion();
	}
	else {
	}
}

Bitu IDEATADevice::data_read(Bitu iolen) {
	Bitu w;

	if (state != IDE_DEV_DATA_READ) {
		fprintf(stderr,"IDE: Data read when not expecting it\n");
		return 0xFFFFUL;
	}
	if (!(status & IDE_STATUS_DRQ)) {
		fprintf(stderr,"IDE: Data read when DRQ=0\n");
		return 0xFFFFUL;
	}

	if (sector_i >= sector_total) {
		fprintf(stderr,"Sector read ptr beyond total\n");
		return 0xFFFFUL;
	}

//	fprintf(stderr,"READ %u/%u len=%u\n",sector_i,sector_total,iolen);

	/* TODO: The MS-DOS CD-ROM driver I'm testing against uses byte-wide I/O during identification?!? REALLY?!?
		 I thought you weren't supposed to do that! */
	if (iolen >= 2) {
		w = host_readw(sector+sector_i);
		sector_i += 2;
	}
	else if (iolen == 1) {
		w = sector[sector_i++];
	}

	if (sector_i >= sector_total)
		io_completion();

	return w;
}

void IDEATADevice::data_write(Bitu v,Bitu iolen) {
}
		
void IDEATAPICDROMDevice::prepare_read(Bitu offset,Bitu size) {
	/* I/O must be WORD ALIGNED */
	assert((offset&1) == 0);
	assert((size&1) == 0);

	sector_i = offset;
	sector_total = size;
	assert(sector_i <= sector_total);
	assert(sector_total <= sizeof(sector));
}

void IDEATADevice::prepare_read(Bitu offset,Bitu size) {
	/* I/O must be WORD ALIGNED */
	assert((offset&1) == 0);
	assert((size&1) == 0);

	sector_i = offset;
	sector_total = size;
	assert(sector_i <= sector_total);
	assert(sector_total <= sizeof(sector));
}

void IDEATAPICDROMDevice::generate_mmc_inquiry() {
	Bitu i;

	/* IN RESPONSE TO ATAPI COMMAND 0x12: INQUIRY */
	memset(sector,0,36);
	sector[0] = (0 << 5) | 5;	/* Peripheral qualifier=0   device type=5 (CDROM) */
	sector[1] = 0x80;		/* RMB=1 removable media */
	sector[3] = 0x21;
	sector[4] = 36 - 5;		/* additional length */

	for (i=0;i < 8 && i < id_mmc_vendor_id.length();i++)
		sector[i+8] = id_mmc_vendor_id[i];
	for (;i < 8;i++)
		sector[i+8] = ' ';

	for (i=0;i < 16 && i < id_mmc_product_id.length();i++)
		sector[i+16] = id_mmc_product_id[i];
	for (;i < 16;i++)
		sector[i+16] = ' ';

	for (i=0;i < 4 && i < id_mmc_product_rev.length();i++)
		sector[i+32] = id_mmc_product_rev[i];
	for (;i < 4;i++)
		sector[i+32] = ' ';
}

void IDEATAPICDROMDevice::generate_identify_device() {
	unsigned char csum;
	Bitu i;

	/* IN RESPONSE TO IDENTIFY DEVICE (0xA1)
	   GENERATE 512-BYTE REPLY */
	memset(sector,0,512);

	host_writew(sector+(0*2),0x85C0U);	/* ATAPI device, command set #5 (what the fuck does that mean?), removable, */

	for (i=0;i < 20 && i < id_serial.length();i++)
		sector[(i^1)+(10*2)] = id_serial[i];
	for (;i < 20;i++)
		sector[(i^1)+(10*2)] = ' ';

	for (i=0;i < 8 && i < id_firmware_rev.length();i++)
		sector[(i^1)+(23*2)] = id_firmware_rev[i];
	for (;i < 8;i++)
		sector[(i^1)+(23*2)] = ' ';

	for (i=0;i < 40 && i < id_model.length();i++)
		sector[(i^1)+(27*2)] = id_model[i];
	for (;i < 40;i++)
		sector[(i^1)+(27*2)] = ' ';

	host_writew(sector+(49*2),
		0x0800UL|/*IORDY supported*/
		0x0200UL|/*must be one*/
		0);
	host_writew(sector+(50*2),
		0x4000UL);
	host_writew(sector+(51*2),
		0x00F0UL);
	host_writew(sector+(52*2),
		0x00F0UL);
	host_writew(sector+(53*2),
		0x0006UL);
	host_writew(sector+(64*2),		/* PIO modes supported */
		0x0003UL);
	host_writew(sector+(67*2),		/* PIO cycle time */
		0x0078UL);
	host_writew(sector+(68*2),		/* PIO cycle time */
		0x0078UL);
	host_writew(sector+(80*2),0x007E); /* major version number. Here we say we support ATA-1 through ATA-8 */
	host_writew(sector+(81*2),0x0022); /* minor version */
	host_writew(sector+(82*2),0x4008); /* command set: NOP, DEVICE RESET[XXXXX], POWER MANAGEMENT */
	host_writew(sector+(83*2),0x0000); /* command set: LBA48[XXXX] */
	host_writew(sector+(85*2),0x4208); /* commands in 82 enabled */
	host_writew(sector+(86*2),0x0000); /* commands in 83 enabled */

	/* ATA-8 integrity checksum */
	sector[510] = 0xA5;
	csum = 0; for (i=0;i < 511;i++) csum += sector[i];
	sector[511] = 0 - csum;
}

void IDEATADevice::generate_identify_device() {
	imageDisk *disk = getBIOSdisk();
	unsigned char csum;
	uint64_t total;
	Bitu i;

	/* IN RESPONSE TO IDENTIFY DEVICE (0xEC)
	   GENERATE 512-BYTE REPLY */
	memset(sector,0,512);

	/* total disk capacity in sectors */
	total = sects * cyls * heads;

	host_writew(sector+(0*2),0x0040);	/* bit 6: 1=fixed disk */
	host_writew(sector+(1*2),cyls);
	host_writew(sector+(3*2),heads);
	host_writew(sector+(4*2),sects * 512);	/* unformatted bytes per track */
	host_writew(sector+(5*2),512);		/* unformatted bytes per sector */
	host_writew(sector+(6*2),sects);

	for (i=0;i < 20 && i < id_serial.length();i++)
		sector[(i^1)+(10*2)] = id_serial[i];
	for (;i < 20;i++)
		sector[(i^1)+(10*2)] = ' ';

	host_writew(sector+(20*2),1);		/* ATA-1: single-ported single sector buffer */
	host_writew(sector+(21*2),4);		/* ATA-1: ECC bytes on read/write long */

	for (i=0;i < 8 && i < id_firmware_rev.length();i++)
		sector[(i^1)+(23*2)] = id_firmware_rev[i];
	for (;i < 8;i++)
		sector[(i^1)+(23*2)] = ' ';

	for (i=0;i < 40 && i < id_model.length();i++)
		sector[(i^1)+(27*2)] = id_model[i];
	for (;i < 40;i++)
		sector[(i^1)+(27*2)] = ' ';

	host_writew(sector+(47*2),0x8080);	/* <- READ/WRITE MULTIPLE MAX SECTORS */
	host_writew(sector+(48*2),0x0001);
	host_writew(sector+(49*2),0x0B00);
	host_writew(sector+(50*2),0x4000);
	host_writew(sector+(51*2),0x00F0);	/* PIO data transfer cycle timing mode */
	host_writew(sector+(52*2),0x00F0);	/* DMA data transfer cycle timing mode */
	host_writew(sector+(53*2),0x0007);
	host_writew(sector+(54*2),cyls);	/* current cylinders */
	host_writew(sector+(55*2),heads);	/* current heads */
	host_writew(sector+(56*2),sects);	/* current sectors per track */
	host_writed(sector+(57*2),total);	/* current capacity in sectors */
	host_writew(sector+(59*2),0x0101);	/* CURRENT READ/WRITE MULTIPLE SECTOR SETTING with valid=1 */
	host_writed(sector+(60*2),total);	/* total user addressable sectors (LBA) */
//	host_writew(sector+(62*2),0x0007);
//	host_writew(sector+(63*2),0x0007);
//	host_writew(sector+(64*2),0x0003);
//	host_writew(sector+(65*2),0x0078);
//	host_writew(sector+(66*2),0x0078);
//	host_writew(sector+(67*2),0x0078);
//	host_writew(sector+(68*2),0x0078);
//	host_writew(sector+(80*2),0x007E);
//	host_writew(sector+(81*2),0x0022);
//	host_writew(sector+(82*2),0x0068);
//	host_writew(sector+(83*2),0x5000);
//	host_writew(sector+(84*2),0x4000);
//	host_writew(sector+(85*2),0x0068);
//	host_writew(sector+(86*2),0x1000);
//	host_writew(sector+(87*2),0x4000);
//	host_writew(sector+(88*2),0x047F);
//	host_writew(sector+(93*3),0x6003);

	/* ATA-8 integrity checksum */
	sector[510] = 0xA5;
	csum = 0; for (i=0;i < 511;i++) csum += sector[i];
	sector[511] = 0 - csum;
}

IDEATADevice::IDEATADevice(IDEController *c,unsigned char bios_disk_index) : IDEDevice(c) {
	this->bios_disk_index = bios_disk_index;
	sector_i = sector_total = 0;

	type = IDE_TYPE_HDD;
	id_serial = "666";
	id_firmware_rev = "666";
	id_model = "Fuck Windows 95";
}

IDEATADevice::~IDEATADevice() {
}

imageDisk *IDEATADevice::getBIOSdisk() {
	if (bios_disk_index >= (2 + MAX_HDD_IMAGES)) return NULL;
	return imageDiskList[bios_disk_index];
}

CDROM_Interface *IDEATAPICDROMDevice::getMSCDEXDrive() {
	CDROM_Interface *cdrom=NULL;

	if (!GetMSCDEXDrive(drive_index,&cdrom))
		return NULL;

	return cdrom;
}

void IDEATAPICDROMDevice::update_from_cdrom() {
	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == NULL) {
		fprintf(stderr,"WARNING: IDE update from CD-ROM failed, disk not available\n");
		return;
	}

	fprintf(stderr,"IDE: update from cdrom complete\n");
}

void IDEATADevice::update_from_biosdisk() {
	imageDisk *dsk = getBIOSdisk();
	if (dsk == NULL) {
		fprintf(stderr,"WARNING: IDE update from BIOS disk failed, disk not available\n");
		return;
	}

	cyls = dsk->cylinders;
	heads = dsk->heads;
	sects = dsk->sectors;

	fprintf(stderr,"IDE: update from disk geometry: C/H/S %u/%u/%u\n",
		cyls,heads,sects);

	/* One additional correction: The disk image is probably using BIOS-style geometry
	   translation (such as C/H/S 1024/64/63) which is impossible given that the IDE
	   standard only allows up to 16 heads. So we have to translate the geometry. */
	while (heads > 16 && (heads & 1) == 0) {
		cyls *= 2;
		heads >>= 1UL;
	}
	while (heads > 16 && (heads % 3) == 0) {
		cyls *= 3;
		heads /= 3UL;
	}

	if (heads > 16)
		fprintf(stderr,"IDE WARNING: Translating geometry to keep headcount at 16 or below is impossible. Expect problems.\n");

	fprintf(stderr,"IDE: final translated geometry: C/H/S %u/%u/%u\n",
		cyls,heads,sects);
}

void IDE_Auto(signed char &index,bool &slave) {
	IDEController *c;
	unsigned int i;

	index = -1;
	slave = false;
	for (i=0;i < MAX_IDE_CONTROLLERS;i++) {
		if ((c=idecontroller[i]) == NULL) continue;
		index = (signed char)(i >> 1);

		if (c->device[0] == NULL) {
			slave = false;
			break;
		}
		if (c->device[1] == NULL) {
			slave = true;
			break;
		}
	}
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_CDROM_Attach(signed char index,bool slave,unsigned char drive_index) {
	IDEController *c;
	IDEATAPICDROMDevice *dev;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS) return;
	c = idecontroller[index];
	if (c == NULL) return;

	if (c->device[slave?1:0] != NULL) {
		fprintf(stderr,"IDE: Controller %u %s already taken\n",index,slave?"slave":"master");
		return;
	}

	if (!GetMSCDEXDrive(drive_index,NULL)) {
		fprintf(stderr,"IDE: Asked to attach CD-ROM that does not exist\n");
		return;
	}

	fprintf(stderr,"Adding IDE CD-ROM\n");
	dev = new IDEATAPICDROMDevice(c,drive_index);
	if (dev == NULL) return;
	dev->update_from_cdrom();
	c->device[slave?1:0] = (IDEDevice*)dev;
}

/* bios_disk_index = index into BIOS INT 13h disk array: imageDisk *imageDiskList[MAX_DISK_IMAGES]; */
void IDE_Hard_Disk_Attach(signed char index,bool slave,unsigned char bios_disk_index/*not INT13h, the index into DOSBox's BIOS drive emulation*/) {
	IDEController *c;
	IDEATADevice *dev;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS) return;
	c = idecontroller[index];
	if (c == NULL) return;

	if (c->device[slave?1:0] != NULL) {
		fprintf(stderr,"IDE: Controller %u %s already taken\n",index,slave?"slave":"master");
		return;
	}

	if (imageDiskList[bios_disk_index] == NULL) {
		fprintf(stderr,"IDE: Asked to attach bios disk that does not exist\n");
		return;
	}

	dev = new IDEATADevice(c,bios_disk_index);
	if (dev == NULL) return;
	dev->update_from_biosdisk();
	c->device[slave?1:0] = (IDEDevice*)dev;
}

static IDEController* GetIDEController(Bitu idx) {
	if (idx >= MAX_IDE_CONTROLLERS) return NULL;
	return idecontroller[idx];
}

static IDEDevice* GetIDESelectedDevice(IDEController *ide) {
	if (ide == NULL) return NULL;
	return ide->device[ide->select];
}

static void IDE_DelayedCommand(Bitu idx/*which IDE controller*/) {
	IDEDevice *dev = GetIDESelectedDevice(GetIDEController(idx));
	if (dev == NULL) return;

	if (dev->type == IDE_TYPE_HDD) {
		IDEATADevice *ata = (IDEATADevice*)dev;
		uint32_t sectorn = 0;/* FIXME: expand to uint64_t when adding LBA48 emulation */
		unsigned int sectcount;
		imageDisk *disk;
		int i;

		switch (dev->command) {
			case 0x20:/* READ SECTOR */
				disk = ata->getBIOSdisk();
				if (disk == NULL) {
					fprintf(stderr,"ATA READ fail, bios disk N/A\n");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				sectcount = ata->count & 0xFF;
				if (sectcount == 0) sectcount = 256;
				if (drivehead_is_lba(ata->drivehead)) {
					/* LBA */
					sectorn = ((ata->drivehead & 0xF) << 24) | ata->lba[0] |
						(ata->lba[1] << 8) |
						(ata->lba[2] << 16);

					fprintf(stderr,"ATA READ LBA sector %lu count %lu\n",
						(unsigned long)sectorn,(unsigned long)sectcount);
				}
				else {
					/* C/H/S */
					if (ata->lba[0] == 0) {
						fprintf(stderr,"WARNING C/H/S access mode and sector==0\n");
						ata->abort_error();
						dev->controller->raise_irq();
						return;
					}
					else if ((ata->drivehead & 0xF) >= ata->heads ||
						ata->lba[0] > ata->sects ||
						(ata->lba[1] | (ata->lba[2] << 8)) >= ata->cyls) {
						fprintf(stderr,"C/H/S out of bounds\n");
						ata->abort_error();
						dev->controller->raise_irq();
						return;
					}

					sectorn = ((ata->drivehead & 0xF) * ata->sects) +
						((ata->lba[1] | (ata->lba[2] << 8)) * ata->sects * ata->heads) +
						(ata->lba[0] - 1);

					fprintf(stderr,"ATA READ C/H/S (%u/%u/%u) sector %lu count %lu\n",
						ata->lba[1] | (ata->lba[2] << 8),
						(ata->drivehead & 0xF),ata->lba[0],
						(unsigned long)sectorn,(unsigned long)sectcount);
				}

				if (disk->Read_AbsoluteSector(sectorn, ata->sector) != 0) {
					fprintf(stderr,"ATA read failed\n");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				/* NTS: the way this command works is that the drive reads ONE sector, then fires the IRQ
				        and lets the host read it, then reads another sector, fires the IRQ, etc. One
					IRQ signal per sector. We emulate that here by adding another event to trigger this
					call unless the sector count has just dwindled to zero, then we let it stop. */
				/* NTS: The sector advance + count decrement is done in the I/O completion function */
				dev->state = IDE_DEV_DATA_READ;
				dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
				ata->prepare_read(0,512);
				dev->controller->raise_irq();
				break;
			case 0xEC:/*IDENTIFY DEVICE (CONTINUED) */
				dev->state = IDE_DEV_DATA_READ;
				dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
				ata->generate_identify_device();
				ata->prepare_read(0,512);
				dev->count = 0x01;
				dev->lba[0] = 0x00;
				dev->feature = 0x00;
				dev->lba[1] = 0x00;
				dev->lba[2] = 0x00;
				dev->controller->raise_irq();
				break;
			default:
				fprintf(stderr,"Unknown delayed IDE/ATA command\n");
				dev->abort_error();
				dev->controller->raise_irq();
				break;
		}
	}
	else if (dev->type == IDE_TYPE_CDROM) {
		IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

		if (dev->state == IDE_DEV_ATAPI_BUSY) {
			switch (dev->command) {
				case 0xA0:/*ATAPI PACKET*/
					atapi->on_atapi_busy_time();
					break;
				default:
					fprintf(stderr,"Unknown delayed IDE/ATAPI busy wait command\n");
					dev->abort_error();
					dev->controller->raise_irq();
					break;
			}
		}
		else {
			switch (dev->command) {
				case 0xA0:/*ATAPI PACKET*/
					dev->state = IDE_DEV_ATAPI_PACKET_COMMAND;
					dev->status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE|IDE_STATUS_DRQ;
					dev->count = 0x01;	/* input/output == 0, command/data == 1 */
					atapi->atapi_cmd_total = 12; /* NTS: do NOT raise IRQ */
					atapi->atapi_cmd_i = 0;
					break;
				case 0xA1:/*IDENTIFY PACKET DEVICE (CONTINUED) */
					dev->state = IDE_DEV_DATA_READ;
					dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
					atapi->generate_identify_device();
					atapi->prepare_read(0,512);
					dev->controller->raise_irq();
					break;
				default:
					fprintf(stderr,"Unknown delayed IDE/ATAPI command\n");
					dev->abort_error();
					dev->controller->raise_irq();
					break;
			}
		}
	}
	else {
		fprintf(stderr,"Unknown delayed command\n");
		dev->abort_error();
		dev->controller->raise_irq();
	}
}

void IDEController::raise_irq() {
//	fprintf(stderr,"Fire IRQ en=%u\n",interrupt_enable);
	irq_pending = true;
	if (IRQ >= 0 && interrupt_enable) PIC_ActivateIRQ(IRQ);
}

void IDEController::lower_irq() {
//	if (irq_pending) fprintf(stderr,"Clear IRQ\n");
	irq_pending = false;
	if (IRQ >= 0) PIC_DeActivateIRQ(IRQ);
}

IDEController *match_ide_controller(Bitu port) {
	unsigned int i;

	for (i=0;i < MAX_IDE_CONTROLLERS;i++) {
		IDEController *ide = idecontroller[i];
		if (ide == NULL) continue;
		if (ide->base_io != 0U && ide->base_io == (port&0xFFF8U)) return ide;
		if (ide->alt_io != 0U && ide->alt_io == (port&0xFFFEU)) return ide;
	}

	return NULL;
}

Bitu IDEDevice::data_read(Bitu iolen) {
	return 0xAAAAU;
}

void IDEDevice::data_write(Bitu v,Bitu iolen) {
}

IDEDevice::IDEDevice(IDEController *c) {
	type = IDE_TYPE_NONE;
	status = 0x00;
	controller = c;
	asleep = false;
	motor_on = true;
	allow_writing = true;
	state = IDE_DEV_READY;
	feature = count = lba[0] = lba[1] = lba[2] = command = drivehead = 0;

	ide_select_delay = 0.5; /* 500us */
	ide_spinup_delay = 3000; /* 3 seconds */
	ide_spindown_delay = 1000; /* 1 second */
	ide_identify_command_delay = 1; /* 1ms */
}

/* IDE controller -> upon writing bit 2 of alt (0x3F6) */
void IDEDevice::host_reset() {
	status = 0x00;
	asleep = false;
	allow_writing = true;
	state = IDE_DEV_READY;
}

IDEDevice::~IDEDevice() {
}

void IDEDevice::abort_error() {
	/* a command was written while another is in progress */
	state = IDE_DEV_READY;
	allow_writing = true;
	command = 0x00;
	status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY/* | IDE_STATUS_DRIVE_SEEK_COMPLETE*/;
}

void IDEDevice::interface_wakeup() {
	if (asleep) {
		fprintf(stderr,"IDE device interface awoken\n");
		asleep = false;
	}
}

void IDEDevice::writecommand(uint8_t cmd) {
	if (state != IDE_DEV_READY) {
		fprintf(stderr,"Command %02x written while another (%02x) is in progress (state=%u). Aborting current command\n",cmd,command,state);
		abort_error();
		return;
	}

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* drive is ready to accept command */
	switch (cmd) {
		default:
			fprintf(stderr,"Unknown IDE command %02X\n",cmd);
			abort_error();
			break;
	}
}

void IDEATAPICDROMDevice::writecommand(uint8_t cmd) {
	if (state != IDE_DEV_READY) {
		fprintf(stderr,"Command %02x written while another (%02x) is in progress (state=%u). Aborting current command\n",cmd,command,state);
		abort_error();
		return;
	}

	LOG(LOG_SB,LOG_NORMAL)("IDE ATAPI command %02x",cmd);

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* drive is ready to accept command */
//	fprintf(stderr,"ATAPI Command %02x\n",cmd);
	allow_writing = false;
	command = cmd;
	switch (cmd) {
		case 0x08: /* DEVICE RESET */
			/* magical incantation taken from QEMU source code.
			   NOTE to ATA standards body: Your documentation sucks */
			status = 0x00;
			drivehead &= 0x30; controller->drivehead = drivehead;
			count = 0x01;
			lba[0] = 0x01;
			feature = 0x01;
			lba[1] = 0x14;	/* <- magic ATAPI identification */
			lba[2] = 0xEB;
			/* NTS: Testing suggests that ATAPI devices do NOT trigger an IRQ on receipt of this command */
			allow_writing = true;
			break;
		case 0x20: /* READ SECTOR */
			abort_error();
			status = IDE_STATUS_ERROR|IDE_STATUS_DRIVE_READY;
			drivehead &= 0x30; controller->drivehead = drivehead;
			count = 0x01;
			lba[0] = 0x01;
			feature = 0x04;	/* abort */
			lba[1] = 0x14;	/* <- magic ATAPI identification */
			lba[2] = 0xEB;
			controller->raise_irq();
			allow_writing = true;
			break;
		case 0xA0: /* ATAPI PACKET */
			if (feature & 1) {
				/* this code does not support DMA packet commands */
				fprintf(stderr,"Attempted DMA transfer\n");
				abort_error();
				count = 0x03; /* no more data (command/data=1, input/output=1) */
				feature = 0xF4;
				controller->raise_irq();
			}
			else {
				state = IDE_DEV_BUSY;
				status = IDE_STATUS_BUSY;
				atapi_to_host = (feature >> 2) & 1;	/* 0=to device 1=to host */
				host_maximum_byte_count = (lba[2] << 8) + lba[1]; /* LBA field bits 23:8 are byte count */
				if (host_maximum_byte_count == 0) host_maximum_byte_count = 0x10000UL;
//				fprintf(stderr,"ATAPI PACKET start. ToHost=%u max=%u\n",atapi_to_host,host_maximum_byte_count);
				PIC_AddEvent(IDE_DelayedCommand,0.25/*ms*/,controller->interface_index);
			}
			break;
		case 0xA1: /* IDENTIFY PACKET DEVICE */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,ide_identify_command_delay,controller->interface_index);
			break;
		case 0xEC: /* IDENTIFY DEVICE */
			/* "devices that implement the PACKET command set shall post command aborted and place PACKET command feature
			   set in the appropriate fields". We have to do this. Unlike OAKCDROM.SYS Windows 95 appears to autodetect
			   IDE devices by what they do when they're sent command 0xEC out of the blue---Microsoft didn't write their
			   IDE drivers to use command 0x08 DEVICE RESET. */
			abort_error();
			status = IDE_STATUS_ERROR|IDE_STATUS_DRIVE_READY;
			drivehead &= 0x30; controller->drivehead = drivehead;
			count = 0x01;
			lba[0] = 0x01;
			feature = 0x04;	/* abort */
			lba[1] = 0x14;	/* <- magic ATAPI identification */
			lba[2] = 0xEB;
			controller->raise_irq();
			allow_writing = true;
			break;
		default:
			fprintf(stderr,"Unknown IDE/ATAPI command %02X\n",cmd);
			abort_error();
			allow_writing = true;
			count = 0x03; /* no more data (command/data=1, input/output=1) */
			feature = 0xF4;
			controller->raise_irq();
			break;
	}
}

void IDEATADevice::writecommand(uint8_t cmd) {
	if (state != IDE_DEV_READY) {
		fprintf(stderr,"Command %02x written while another (%02x) is in progress. Aborting current command\n",cmd,command);
		abort_error();
		return;
	}

	LOG(LOG_SB,LOG_NORMAL)("IDE ATA command %02x",cmd);

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* drive is ready to accept command */
	fprintf(stderr,"ATA Command %02x\n",cmd);
	allow_writing = false;
	command = cmd;
	switch (cmd) {
		case 0x08: /* DEVICE RESET */
			/* magical incantation taken from QEMU source code.
			   NOTE to ATA standards body: Your documentation sucks */
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_ERROR;
			drivehead &= 0xF0; controller->drivehead = drivehead;
			count = 0x01; lba[0] = 0x01;
			lba[1] = lba[2] = 0;
			/* NTS: Testing suggests that ATA hard drives DO fire an IRQ at this stage.
			        In fact, Windows 95 won't detect hard drives that don't fire an IRQ in desponse */
			controller->raise_irq();
			allow_writing = true;
			break;
		case 0x20: /* READ SECTOR */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,2/*ms*/,controller->interface_index);
			break;
		case 0xEC: /* IDENTIFY DEVICE */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,ide_identify_command_delay,controller->interface_index);
			break;
		default:
			fprintf(stderr,"Unknown IDE/ATA command %02X\n",cmd);
			abort_error();
			allow_writing = true;
			controller->raise_irq();
			break;
	}
}

void IDEDevice::deselect() {
}

/* the hard disk or CD-ROM class override of this member is responsible for checking
   the head value and clamping within range if C/H/S mode is selected */
void IDEDevice::select(uint8_t ndh,bool switched_to) {
	/* NTS: I thought there was some delay between selecting a drive and sending a command.
		Apparently I was wrong. */
	drivehead = ndh;
//	status = (!asleep)?(IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE):0;
	allow_writing = !asleep;
	state = IDE_DEV_READY;
}

IDEController::IDEController(Section* configuration,unsigned char index):Module_base(configuration){
	Section_prop * section=static_cast<Section_prop *>(configuration);

	status = 0x00;
	host_reset = false;
	irq_pending = false;
	interrupt_enable = true;
	interface_index = index;
	device[0] = NULL;
	device[1] = NULL;
	base_io = 0;
	select = 0;
	alt_io = 0;
	IRQ = -1;

	if (index < sizeof(IDE_default_IRQs)) {
		base_io = IDE_default_bases[index];
		alt_io = IDE_default_alts[index];
		IRQ = IDE_default_IRQs[index];
	}

	if (base_io != 0 || alt_io != 0 || IRQ >= 0)
		fprintf(stderr,"IDE: Adding IDE controller to port %03x/%03x IRQ %d\n",
				base_io,alt_io,IRQ);
}

void IDEController::install_io_port(){
	unsigned int i;

	if (base_io != 0) {
		for (i=0;i < 8;i++) {
			WriteHandler[i].Install(base_io+i,ide_baseio_w,IO_MA);
			ReadHandler[i].Install(base_io+i,ide_baseio_r,IO_MA);
		}
	}

	if (alt_io != 0) {
		WriteHandlerAlt[0].Install(alt_io,ide_altio_w,IO_MA);
		ReadHandlerAlt[0].Install(alt_io,ide_altio_r,IO_MA);

		WriteHandlerAlt[1].Install(alt_io+1,ide_altio_w,IO_MA);
		ReadHandlerAlt[1].Install(alt_io+1,ide_altio_r,IO_MA);
	}
}

IDEController::~IDEController(){
}

static void ide_altio_w(Bitu port,Bitu val,Bitu iolen) {
	IDEController *ide = match_ide_controller(port);
	if (ide == NULL) {
		fprintf(stderr,"WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
		return;
	}

//	fprintf(stderr,"W-%03X %02X\n",port,val);

	port &= 1;
	if (port == 0) {/*3F6*/
		ide->interrupt_enable = (val&2)?0:1;
		if (ide->interrupt_enable) {
			if (ide->irq_pending) ide->raise_irq();
		}
		else {
			if (ide->IRQ >= 0) PIC_DeActivateIRQ(ide->IRQ);
		}

		if ((val&4) && !ide->host_reset) {
			fprintf(stderr,"IDE: Host reset initiated\n");
			if (ide->device[0]) ide->device[0]->host_reset();
			if (ide->device[1]) ide->device[1]->host_reset();
			ide->host_reset=1;
		}
		else if (!(val&4)) {
			ide->host_reset=0;
		}
	}
}

static Bitu _ide_altio_r(Bitu port,Bitu iolen) {
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == NULL) {
		fprintf(stderr,"WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
		return ~(0UL);
	}
	dev = ide->device[ide->select];

//	fprintf(stderr,"R-%03X\n",port);

	port &= 1;
	if (port == 0)/*3F6(R) status, does NOT clear interrupt*/
		return (dev != NULL) ? dev->status : ide->status;
	else /*3F7(R) Drive Address Register*/
		return 0x00;
//		return 0x80|(ide->select==0?0:1)|(ide->select==1?0:2)|
//			((dev != NULL) ? (((dev->drivehead&0xF)^0xF) << 2) : 0x3C);

	return ~(0UL);
}

static Bitu ide_altio_r(Bitu port,Bitu iolen) {
	Bitu r = _ide_altio_r(port,iolen);
//	fprintf(stderr," > %02X\n",r);
	return r;
}

static Bitu _ide_baseio_r(Bitu port,Bitu iolen) {
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == NULL) {
		fprintf(stderr,"WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
		return ~(0UL);
	}
	dev = ide->device[ide->select];

//	fprintf(stderr,"R-%03X len=%u\n",port,iolen);

	port &= 7;
	switch (port) {
		case 0:	/* 1F0 */
			return (dev != NULL) ? dev->data_read(iolen) : 0xFFFFFFFFUL;
		case 1:	/* 1F1 */
			return (dev != NULL) ? dev->feature : 0x00;
		case 2:	/* 1F2 */
			return (dev != NULL) ? dev->count : 0x00;
		case 3:	/* 1F3 */
			return (dev != NULL) ? dev->lba[0] : 0x00;
		case 4:	/* 1F4 */
			return (dev != NULL) ? dev->lba[1] : 0x00;
		case 5:	/* 1F5 */
			return (dev != NULL) ? dev->lba[2] : 0x00;
		case 6:	/* 1F6 */
			return ide->drivehead;
		case 7:	/* 1F7 */
			/* if an IDE device exists at selection return it's status, else return our status */
			if (dev && dev->status & IDE_STATUS_BUSY) {
			}
			else if (dev == NULL && ide->status & IDE_STATUS_BUSY) {
			}
			else {
				ide->lower_irq();
			}

			return (dev != NULL) ? dev->status : ide->status;
	}

	return ~(0UL);
}

static Bitu ide_baseio_r(Bitu port,Bitu iolen) {
	Bitu r = _ide_baseio_r(port,iolen);
	return r;
}

static void ide_baseio_w(Bitu port,Bitu val,Bitu iolen) {
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == NULL) {
		fprintf(stderr,"WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
		return;
	}
	dev = ide->device[ide->select];

	/* ignore I/O writes if the controller is busy */
	if (dev) {
		if (dev->status & IDE_STATUS_BUSY) {
			fprintf(stderr,"W-%03X %02X BUSY DROP\n",port,val);
			return;
		}
	}
	else if (ide->status & IDE_STATUS_BUSY) {
		fprintf(stderr,"W-%03X %02X BUSY DROP\n",port,val);
		return;
	}

//	fprintf(stderr,"W-%03X %02X\n",port,val);

	port &= 7;
	switch (port) {
		case 0:	/* 1F0 */
			if (dev) dev->data_write(val,iolen); /* <- TODO: what about 32-bit PIO modes? */
			break;
		case 1:	/* 1F1 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->feature = val;
			else
				fprintf(stderr,"IDE: xx1 write ignored\n");
			break;
		case 2:	/* 1F2 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->count = val;
			else
				fprintf(stderr,"IDE: xx1 write ignored\n");
			break;
		case 3:	/* 1F3 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->lba[0] = val;
			else
				fprintf(stderr,"IDE: xx1 write ignored\n");
			break;
		case 4:	/* 1F4 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->lba[1] = val;
			else
				fprintf(stderr,"IDE: xx1 write ignored\n");
			break;
		case 5:	/* 1F5 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->lba[2] = val;
			else
				fprintf(stderr,"IDE: xx1 write ignored\n");
			break;
		case 6:	/* 1F6 */
			if (((val>>4)&1) != ide->select) {
				ide->lower_irq();
				/* update select pointer if bit 4 changes.
				   also emulate IDE busy state when changing drives */
				if (dev) dev->deselect();
				ide->select = (val>>4)&1;
				dev = ide->device[ide->select];
				if (dev) dev->select(val,1);
				else ide->status = 0; /* NTS: if there is no drive there you're supposed to not have anything set */
			}
			else if (dev) {
				dev->select(val,0);
			}
			else {
				ide->status = 0; /* NTS: if there is no drive there you're supposed to not have anything set */
			}

			ide->drivehead = val;
			break;
		case 7:	/* 1F7 */
			if (dev) dev->writecommand(val);
			break;
	}
}

static void IDE_Destroy(Section* sec) {
	if (init_ide) {
		/* TODO: Free each IDE object */
		init_ide = 0;
	}
}

static void IDE_Init(Section* sec,unsigned char interface) {
	Section_prop *section=static_cast<Section_prop *>(sec);
	IDEController *ide;

	assert(interface < sizeof(IDE_default_IRQs));

	if (!section->Get_bool("enable"))
		return;

	if (!init_ide) {
		sec->AddDestroyFunction(&IDE_Destroy);
		init_ide = 1;
	}

	ide = idecontroller[interface] = new IDEController(sec,interface);
	ide->install_io_port();
}

void IDE_Primary_Init(Section *sec) {
	IDE_Init(sec,0);
}

void IDE_Secondary_Init(Section *sec) {
	IDE_Init(sec,1);
}

void IDE_Tertiary_Init(Section *sec) {
	IDE_Init(sec,2);
}

void IDE_Quaternary_Init(Section *sec) {
	IDE_Init(sec,3);
}

