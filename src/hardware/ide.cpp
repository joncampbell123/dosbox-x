/*
 * IDE ATA/ATAPI and controller emulation for DOSBox-X
 * (C) 2012 Jonathan Campbell

 * [insert open source license here]
 */

/* $Id: ide.cpp,v 1.49 2009-04-10 09:53:04 c2woody Exp $ */

#include <math.h>
#include <assert.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "cpu.h"
#include "ide.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "callback.h"
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
	virtual void abort_normal();
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
	virtual void prepare_write(Bitu offset,Bitu size);
	virtual void io_completion();
public:
	Bitu heads,sects,cyls,headshr;
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
	bool int13fakeio;		/* on certain INT 13h calls, force IDE state as if BIOS had carried them out */
	bool int13fakev86io;		/* on certain INT 13h calls in virtual 8086 mode, trigger fake CPU I/O traps */
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

/* when the ATAPI command has been accepted, and the timeout has passed */
void IDEATAPICDROMDevice::on_atapi_busy_time() {
	switch (atapi_cmd[0]) {
		case 0x03: /* REQUEST SENSE */
			prepare_read(0,MIN((unsigned int)18,(unsigned int)host_maximum_byte_count));
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

				prepare_read(0,MIN((unsigned int)(4+8+(leadout?8:0)),(unsigned int)host_maximum_byte_count));
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
	/* lower DRQ */
	status &= ~IDE_STATUS_DRQ;

	/* depending on the command, either continue it or finish up */
	switch (command) {
		case 0x20:/* READ SECTOR */
			/* OK, decrement count, increment address */
			/* NTS: Remember that count == 0 means the host wanted to transfer 256 sectors */
			if ((count&0xFF) == 1) {
				/* end of the transfer */
				count = 0;
				status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
				state = IDE_DEV_READY;
				allow_writing = true;
				return;
			}
			else if ((count&0xFF) == 0) count = 255;
			else count--;

			/* ATA-1 behavior: increment the LBA address or the C/H/S address */
			if (drivehead_is_lba(drivehead)) {
				if (((++lba[0])&0xFF) == 0) {/* increment. carry? */
					if (((++lba[1])&0xFF) == 0) {/*increment, carry? */
						if (((++lba[2])&0xFF) == 0) {/* increment, carry? */
							if ((drivehead&0xF) != 0xF) {/*bits 27:24 in drive/head */
								drivehead++;
							}
							else {
								fprintf(stderr,"READ LBA advance error\n");
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
								fprintf(stderr,"READ C/H/S advance error\n");
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

			/* cause another delay, another sector read */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,0.1/*ms*/,controller->interface_index);
			break;
		case 0x30:/* WRITE SECTOR */
			/* this is where the drive has accepted the sector, lowers DRQ, and begins executing the command */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,0.15/*ms*/,controller->interface_index);
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

	if (state != IDE_DEV_DATA_READ)
		return 0xFFFFUL;

	if (!(status & IDE_STATUS_DRQ)) {
		fprintf(stderr,"IDE: Data read when DRQ=0\n");
		return 0xFFFFUL;
	}

	if (sector_i >= sector_total)
		return 0xFFFFUL;

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
			/* we don't know the command, immediately return an error */
			fprintf(stderr,"Unknown ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
				atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11]);

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

	if (state != IDE_DEV_DATA_READ)
		return 0xFFFFUL;

	if (!(status & IDE_STATUS_DRQ)) {
		fprintf(stderr,"IDE: Data read when DRQ=0\n");
		return 0xFFFFUL;
	}

	if (sector_i >= sector_total)
		return 0xFFFFUL;

	/* NTS: Some MS-DOS CD-ROM drivers like OAKCDROM.SYS use byte-wide I/O for the initial identification */
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
	if (state != IDE_DEV_DATA_WRITE) {
		fprintf(stderr,"IDE ATA warning: data write when device not in DATA_WRITE state\n");
		return;
	}
	if (!(status & IDE_STATUS_DRQ)) {
		fprintf(stderr,"IDE ATA warning: data write with DRQ=0\n");
		return;
	}
	if (sector_i >= sector_total) {
		fprintf(stderr,"IDE ATA warning: sector already full\n");
		return;
	}

	if (iolen >= 2) {
		host_writew(sector+sector_i,v);
		sector_i += 2;
	}
	else if (iolen == 1) {
		sector[sector_i++] = v;
	}

	if (sector_i >= sector_total)
		io_completion();
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

void IDEATADevice::prepare_write(Bitu offset,Bitu size) {
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

	headshr = 0;
	type = IDE_TYPE_HDD;
	id_serial = "8086";
	id_firmware_rev = "8086";
	id_model = "DOSBox IDE disk";
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
}

void IDEATADevice::update_from_biosdisk() {
	imageDisk *dsk = getBIOSdisk();
	if (dsk == NULL) {
		fprintf(stderr,"WARNING: IDE update from BIOS disk failed, disk not available\n");
		return;
	}

	headshr = 0;
	cyls = dsk->cylinders;
	heads = dsk->heads;
	sects = dsk->sectors;

	/* One additional correction: The disk image is probably using BIOS-style geometry
	   translation (such as C/H/S 1024/64/63) which is impossible given that the IDE
	   standard only allows up to 16 heads. So we have to translate the geometry. */
	while (heads > 16 && (heads & 1) == 0) {
		cyls <<= 1U;
		heads >>= 1U;
		headshr++;
	}

	fprintf(stderr,"Mapping BIOS DISK C/H/S %u/%u/%u as IDE %u/%u/%u\n",
		dsk->cylinders,dsk->heads,dsk->sectors,
		cyls,heads,sects);

	if (heads > 16) fprintf(stderr,"ERROR: Heads > 16 with no translation available\n");
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

static bool IDE_CPU_Is_Vm86() {
	return (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM)));
}

static void ide_baseio_w(Bitu port,Bitu val,Bitu iolen);

static Bitu IDE_SelfIO_In(IDEController *ide,Bitu port,Bitu len) {
	if (ide->int13fakev86io && IDE_CPU_Is_Vm86()) {
		/* Trigger I/O in virtual 8086 mode, where the OS can trap it and act on it.
		 * Windows 95 uses V86 traps to help "autodetect" what IDE drive and port the
		 * BIOS uses on INT 13h so that it's internal IDE driver can take over, which
		 * is the whole reason for this hack. */
		return CPU_ForceV86FakeIO_In(port,len);
	}
	else {
		return ide_baseio_r(port,len);
	}
}

static void IDE_SelfIO_Out(IDEController *ide,Bitu port,Bitu val,Bitu len) {
	if (ide->int13fakev86io && IDE_CPU_Is_Vm86()) {
		/* Trigger I/O in virtual 8086 mode, where the OS can trap it and act on it.
		 * Windows 95 uses V86 traps to help "autodetect" what IDE drive and port the
		 * BIOS uses on INT 13h so that it's internal IDE driver can take over, which
		 * is the whole reason for this hack. */
		CPU_ForceV86FakeIO_Out(port,val,len);
	}
	else {
		ide_baseio_w(port,val,len);
	}
}

/* this is called after INT 13h AH=0x02 READ DISK to change IDE state to simulate the BIOS in action.
 * this is needed for old "32-bit disk drivers" like WDCTRL in Windows 3.11 Windows for Workgroups,
 * which issues INT 13h to read-test and then reads IDE registers to see if they match expectations */
void IDE_EmuINT13DiskReadByBIOS(unsigned char disk,unsigned int cyl,unsigned int head,unsigned sect) {
	IDEController *ide;
	IDEDevice *dev;
	Bitu idx,ms;

	if (disk < 0x80) return;

	for (idx=0;idx < MAX_IDE_CONTROLLERS;idx++) {
		ide = GetIDEController(idx);
		if (ide == NULL) continue;
		if (!ide->int13fakeio) continue;

		/* TODO: Print a warning message if the IDE controller is busy (debug/warning message) */

		/* TODO: Force IDE state to readiness, abort command, etc. */

		/* for master/slave device... */
		for (ms=0;ms < 2;ms++) {
			dev = ide->device[ms];
			if (dev == NULL) continue;

			/* TODO: Print a warning message if the IDE device is busy or in the middle of a command */

			/* TODO: Forcibly device-reset the IDE device */

			/* Issue I/O to ourself to select drive */
			IDE_SelfIO_In(ide,ide->base_io+7,1);
			IDE_SelfIO_Out(ide,ide->base_io+6,0x00+(ms<<4),1);

			if (dev->type == IDE_TYPE_HDD) {
				IDEATADevice *ata = (IDEATADevice*)dev;
				static bool vm86_warned = false;
				bool vm86 = IDE_CPU_Is_Vm86();

				if ((ata->bios_disk_index-2) == (disk-0x80)) {
					if (ata->headshr != 0)
						fprintf(stderr,"FIXME: Untested BIOS geometry translation with IDE hack\n");

					/* translate BIOS INT 13h geometry to IDE geometry */
					if (ata->headshr != 0) {
						unsigned long lba;

						fprintf(stderr,"xlate %u/%u/%u to ",cyl,head,sect);

						imageDisk *dsk = ata->getBIOSdisk();
						if (dsk == NULL) return;
						lba = (head * dsk->sectors) + (cyl * dsk->sectors * dsk->heads) + sect - 1;
						sect = (lba % ata->sects) + 1;
						head = (lba / ata->sects) % ata->heads;
						cyl = (lba / ata->sects / ata->heads);

						fprintf(stderr,"%u/%u/%u\n",cyl,head,sect);
					}

					if (ide->int13fakev86io && vm86) {
						/* we MUST clear interrupts.
						 * leaving them enabled causes Win95 (or DOSBox?) to recursively
						 * pagefault and DOSBox to crash. In any case it seems Win95's
						 * IDE driver assumes the BIOS INT 13h code will do this since
						 * it's customary for the BIOS to do it at some point, usually
						 * just before reading the sector data. */
						CPU_CLI();

						/* We're in virtual 8086 mode and we're asked to fake I/O as if
						 * executing a BIOS subroutine. Some OS's like Windows 95 rely on
						 * executing INT 13h in virtual 8086 mode: on startup, the ESDI
						 * driver traps IDE ports and then executes INT 13h to watch what
						 * I/O ports it uses. It then uses that information to decide
						 * what IDE hard disk and controller corresponds to what DOS
						 * drive. So to get 32-bit disk access to work in Windows 95,
						 * we have to put on a good show to convince Windows 95 we're
						 * a legitimate BIOS INT 13h call doing it's job. */
						IDE_SelfIO_In(ide,ide->base_io+7,1);		/* dum de dum reading status */
						IDE_SelfIO_Out(ide,ide->base_io+6,(ms<<4)+0xA0+head,1); /* drive and head */
						IDE_SelfIO_In(ide,ide->base_io+7,1);		/* dum de dum reading status */
						IDE_SelfIO_Out(ide,ide->base_io+2,0x01,1);	/* sector count */
						IDE_SelfIO_Out(ide,ide->base_io+3,sect,1);	/* sector number */
						IDE_SelfIO_Out(ide,ide->base_io+4,cyl&0xFF,1);	/* cylinder lo */
						IDE_SelfIO_Out(ide,ide->base_io+5,(cyl>>8)&0xFF,1); /* cylinder hi */
						IDE_SelfIO_Out(ide,ide->base_io+6,(ms<<4)+0xA0+head,1); /* drive and head */
						IDE_SelfIO_In(ide,ide->base_io+7,1);		/* dum de dum reading status */
						IDE_SelfIO_Out(ide,ide->base_io+7,0x20,1);	/* issue READ */

						do {
							/* TODO: Timeout needed */
							unsigned int i = IDE_SelfIO_In(ide,ide->base_io+7,1);
							if ((i&0x80) == 0) break;
						} while (1);

						/* for brevity assume it worked. we're here to bullshit Windows 95 after all */
						for (unsigned int i=0;i < 256;i++)
							IDE_SelfIO_In(ide,ide->base_io+0,2); /* 16-bit IDE data read */

						/* one more */
						IDE_SelfIO_In(ide,ide->base_io+7,1);		/* dum de dum reading status */

						/* assume IRQ 14 happened and clear it */
						IDE_SelfIO_Out(ide,0xA0,0x66,1);		/* specific EOI IRQ 14 */
					}
					else {
						/* hack IDE state as if a BIOS executing IDE disk routines.
						 * This is required if we want IDE emulation to work with Windows 3.11 Windows for Workgroups 32-bit disk access (WDCTRL),
						 * because the driver "tests" the controller by issuing INT 13h calls then reading back IDE registers to see if
						 * they match the C/H/S it requested */
						dev->feature = 0x00;		/* clear error (WDCTRL test phase 5/C/13) */
						dev->count = 0x00;		/* clear sector count (WDCTRL test phase 6/D/14) */
						dev->lba[0] = sect;		/* leave sector number the same (WDCTRL test phase 7/E/15) */
						dev->lba[1] = cyl;		/* leave cylinder the same (WDCTRL test phase 8/F/16) */
						dev->lba[2] = cyl >> 8;		/* ...ditto */
						ide->drivehead = dev->drivehead = 0xA0 | (ms<<4) | head; /* drive head and master/slave (WDCTRL test phase 9/10/17) */
						dev->status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE; /* status (WDCTRL test phase A/11/18) */
						dev->allow_writing = true;

						if (vm86 && !vm86_warned) {
							fprintf(stderr,"IDE warning: INT 13h read from virtual 8086 mode.\n");
							fprintf(stderr,"             If using Windows 95, please set int13fakev86io=true for proper 32-bit disk access\n");
							vm86_warned = true;
						}
					}

					/* break out, we're done */
					idx = MAX_IDE_CONTROLLERS;
					break;
				}
			}
		}
	}
}

/* this is called by src/ints/bios_disk.cpp whenever INT 13h AH=0x00 is called on a hard disk.
 * this gives us a chance to update IDE state as if the BIOS had gone through with a full disk reset as requested. */
void IDE_ResetDiskByBIOS(unsigned char disk) {
	IDEController *ide;
	IDEDevice *dev;
	Bitu idx,ms;

	if (disk < 0x80) return;

	for (idx=0;idx < MAX_IDE_CONTROLLERS;idx++) {
		ide = GetIDEController(idx);
		if (ide == NULL) continue;
		if (!ide->int13fakeio) continue;

		/* TODO: Print a warning message if the IDE controller is busy (debug/warning message) */

		/* TODO: Force IDE state to readiness, abort command, etc. */

		/* for master/slave device... */
		for (ms=0;ms < 2;ms++) {
			dev = ide->device[ms];
			if (dev == NULL) continue;

			/* TODO: Print a warning message if the IDE device is busy or in the middle of a command */

			/* TODO: Forcibly device-reset the IDE device */

			/* Issue I/O to ourself to select drive */
			IDE_SelfIO_In(ide,ide->base_io+7,1);
			IDE_SelfIO_Out(ide,ide->base_io+6,0x00+(ms<<4),1);

			/* TODO: Forcibly device-reset the IDE device */

			if (dev->type == IDE_TYPE_HDD) {
				IDEATADevice *ata = (IDEATADevice*)dev;

				if ((ata->bios_disk_index-2) == (disk-0x80)) {
					fprintf(stderr,"IDE %d%c reset by BIOS disk 0x%02x\n",
						idx+1,ms?'s':'m',
						disk);

					if (ide->int13fakev86io && IDE_CPU_Is_Vm86()) {
						/* issue the DEVICE RESET command */
						IDE_SelfIO_In(ide,ide->base_io+7,1);
						IDE_SelfIO_Out(ide,ide->base_io+7,0x08,1);

						IDE_SelfIO_In(ide,ide->base_io+7,1);

						/* assume IRQ 14 happened and clear it */
						IDE_SelfIO_Out(ide,0xA0,0x66,1);		/* specific EOI IRQ 14 */
					}
					else {
						/* Windows 3.1 WDCTRL needs this, or else, it will read the
						 * status register and see something other than DRIVE_READY|SEEK_COMPLETE */
						dev->writecommand(0x08);

						/* and then immediately clear the IRQ */
						ide->lower_irq();
					}
				}
			}
		}
	}
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
			case 0x30:/* WRITE SECTOR */
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
				}
				else {
					/* C/H/S */
					if (ata->lba[0] == 0) {
						fprintf(stderr,"ATA sector 0 does not exist\n");
						ata->abort_error();
						dev->controller->raise_irq();
						return;
					}
					else if ((ata->drivehead & 0xF) >= ata->heads ||
						ata->lba[0] > ata->sects ||
						(ata->lba[1] | (ata->lba[2] << 8)) >= ata->cyls) {
						ata->abort_error();
						dev->controller->raise_irq();
						return;
					}

					sectorn = ((ata->drivehead & 0xF) * ata->sects) +
						((ata->lba[1] | (ata->lba[2] << 8)) * ata->sects * ata->heads) +
						(ata->lba[0] - 1);
#if 0
					fprintf(stderr,"ATA WRITE C/H/S (%u/%u/%u) sector %lu count %lu\n",
						ata->lba[1] | (ata->lba[2] << 8),
						(ata->drivehead & 0xF),ata->lba[0],
						(unsigned long)sectorn,(unsigned long)sectcount);
#endif
				}

				if (disk->Write_AbsoluteSector(sectorn, ata->sector) != 0) {
					fprintf(stderr,"Failed to write sector\n");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				/* NTS: the way this command works is that the drive writes ONE sector, then fires the IRQ
				        and lets the host read it, then reads another sector, fires the IRQ, etc. One
					IRQ signal per sector. We emulate that here by adding another event to trigger this
					call unless the sector count has just dwindled to zero, then we let it stop. */
				if ((ata->count&0xFF) == 1) {
					/* end of the transfer */
					ata->count = 0;
					ata->status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
					dev->controller->raise_irq();
					ata->state = IDE_DEV_READY;
					ata->allow_writing = true;
					return;
				}
				else if ((ata->count&0xFF) == 0) ata->count = 255;
				else ata->count--;

				/* ATA-1 behavior: increment the LBA address or the C/H/S address */
				if (drivehead_is_lba(ata->drivehead)) {
					if (((++ata->lba[0])&0xFF) == 0) {/* increment. carry? */
						if (((++ata->lba[1])&0xFF) == 0) {/*increment, carry? */
							if (((++ata->lba[2])&0xFF) == 0) {/* increment, carry? */
								if ((ata->drivehead&0xF) != 0xF) {/*bits 27:24 in drive/head */
									ata->drivehead++;
								}
								else {
									ata->abort_error();
									return;
								}
							}
						}
					}
				}
				else {
					/* Oh, joy. Address incrementation C/H/S style */
					if (((++ata->lba[0])&0xFF) > ata->sects) { /* increment sector */
						ata->lba[0] = 1;
						/* if sector carry, increment head */
						if ((ata->drivehead&0xF) == 0xF) {	/* if head carry, increment 7:0 of track */
							ata->drivehead &= 0xF0;
							if (((++ata->lba[1])&0xFF) == 0) { /* if 7:0 carry then incrment 15:8 */
								if (((++ata->lba[2])&0xFF) == 0) {
									ata->abort_error();
									return;
								}
							}
						}
						else {
							ata->drivehead++;
						}
					}
				}

				/* begin another sector */
				dev->state = IDE_DEV_DATA_WRITE;
				dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
				ata->prepare_write(0,512);
				dev->controller->raise_irq();
				break;

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
						fprintf(stderr,"C/H/S %u/%u/%u out of bounds %u/%u/%u\n",
							ata->lba[1] | (ata->lba[2] << 8),
							ata->drivehead&0xF,
							ata->lba[0],
							ata->cyls,
							ata->heads,
							ata->sects);
						ata->abort_error();
						dev->controller->raise_irq();
						return;
					}

					sectorn = ((ata->drivehead & 0xF) * ata->sects) +
						((ata->lba[1] | (ata->lba[2] << 8)) * ata->sects * ata->heads) +
						(ata->lba[0] - 1);
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
	irq_pending = true;
	if (IRQ >= 0 && interrupt_enable) PIC_ActivateIRQ(IRQ);
}

void IDEController::lower_irq() {
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
	assert(controller != NULL);
	fprintf(stderr,"IDE abort dh=0x%02x with error on 0x%03x\n",drivehead,controller->base_io);

	/* a command was written while another is in progress */
	state = IDE_DEV_READY;
	allow_writing = true;
	command = 0x00;
	status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::abort_normal() {
	/* a command was written while another is in progress */
	state = IDE_DEV_READY;
	allow_writing = true;
	command = 0x00;
	status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::interface_wakeup() {
	if (asleep) {
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
	allow_writing = false;
	command = cmd;
	switch (cmd) {
		case 0x08: /* DEVICE RESET */
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
			abort_normal();
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
			abort_normal();
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

	fprintf(stderr,"IDE ATA command %02x dh=0x%02x count=0x%02x chs=%02x/%02x/%02x\n",cmd,
		drivehead,count,(lba[2]<<8)+lba[1],drivehead&0xF,lba[0]);
	LOG(LOG_SB,LOG_NORMAL)("IDE ATA command %02x",cmd);

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* FIXME: OAKCDROM.SYS is sending the hard disk command 0xA0 (ATAPI packet) for some reason. Why? */

	/* drive is ready to accept command */
	allow_writing = false;
	command = cmd;
	switch (cmd) {
		case 0x08: /* DEVICE RESET */
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
			drivehead &= 0xF0; controller->drivehead = drivehead;
			count = 0x01; lba[0] = 0x01; feature = 0x00;
			lba[1] = lba[2] = 0;
			/* NTS: Testing suggests that ATA hard drives DO fire an IRQ at this stage.
			        In fact, Windows 95 won't detect hard drives that don't fire an IRQ in desponse */
			controller->raise_irq();
			allow_writing = true;
			break;
		case 0x20: /* READ SECTOR */
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand,0.1/*ms*/,controller->interface_index);
			break;
		case 0x30: /* WRITE SECTOR */
			/* the drive does NOT signal an interrupt. it sets DRQ and waits for a sector
			 * to be transferred to it before executing the command */
			state = IDE_DEV_DATA_WRITE;
			status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ;
			prepare_write(0,512);
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

	int13fakeio = section->Get_bool("int13fakeio");
	int13fakev86io = section->Get_bool("int13fakev86io");

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
			if (ide->device[0]) ide->device[0]->host_reset();
			if (ide->device[1]) ide->device[1]->host_reset();
			ide->host_reset=1;
		}
		else if (!(val&4)) {
			ide->host_reset=0;
		}
	}
}

static Bitu ide_altio_r(Bitu port,Bitu iolen) {
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == NULL) {
		fprintf(stderr,"WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
		return ~(0UL);
	}
	dev = ide->device[ide->select];

	port &= 1;
	if (port == 0)/*3F6(R) status, does NOT clear interrupt*/
		return (dev != NULL) ? dev->status : ide->status;
	else /*3F7(R) Drive Address Register*/
		return 0x80|(ide->select==0?0:1)|(ide->select==1?0:2)|
			((dev != NULL) ? (((dev->drivehead&0xF)^0xF) << 2) : 0x3C);

	return ~(0UL);
}

static Bitu ide_baseio_r(Bitu port,Bitu iolen) {
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == NULL) {
		fprintf(stderr,"WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
		return ~(0UL);
	}
	dev = ide->device[ide->select];

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

	port &= 7;
	switch (port) {
		case 0:	/* 1F0 */
			if (dev) dev->data_write(val,iolen); /* <- TODO: what about 32-bit PIO modes? */
			break;
		case 1:	/* 1F1 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->feature = val;
			break;
		case 2:	/* 1F2 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->count = val;
			break;
		case 3:	/* 1F3 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->lba[0] = val;
			break;
		case 4:	/* 1F4 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->lba[1] = val;
			break;
		case 5:	/* 1F5 */
			if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
				dev->lba[2] = val;
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

	if (interface == 0)
		PIC_SetIRQMask(14,false);
	else if (interface == 1)
		PIC_SetIRQMask(15,false);
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

