/*
 *  IDE ATA/ATAPI and controller emulation for DOSBox-X
 *  Copyright (C) 2012-2022 Jonathan Campbell

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

/* $Id: ide.cpp,v 1.49 2009-04-10 09:53:04 c2woody Exp $ */

#include <math.h>
#include <assert.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "cpu.h"
#include "ide.h"
#include "menu.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "control.h"
#include "callback.h"
#include "bios_disk.h"
#include "../src/dos/cdrom.h"
#include "bios.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
# pragma warning(disable:4305) /* truncation from double to float */
#endif

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
#endif

#if defined(_MSC_VER)
# pragma warning(disable:4065) /* switch statement no case labels */
#endif

static inline unsigned int ide_chs_cylinder(const uint16_t lba[3]) {
    return ((unsigned int)lba[1] & 0xffu) | (((unsigned int)lba[2] & 0xffu) << 8u);
}

struct IDEEventPack {
#if defined(HX_DOS) || (defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
	IDEEventPack() = default;
#endif
	IDEEventPack(const unsigned int interface,const unsigned int device) : v(pack(interface,device)) {
	}
	IDEEventPack(const unsigned int nv) : v(nv) {
	}

	static inline constexpr unsigned int pack(const unsigned int interface,const unsigned int device) {
		return (interface << 1u) | device;
	}

	inline constexpr unsigned int get(void) const {
		return v;
	}

	inline constexpr unsigned int interface(void) const {
		return (v >> 1u);
	}

	inline constexpr unsigned int device(void) const {
		return (v & 1u);
	}

	inline void set(const unsigned int pk) {
		v = pk;
	}
private:
	unsigned int		v;
};

extern int bootdrive;
extern bool bootguest, bootvm, use_quick_reboot;
static unsigned char init_ide = 0;
extern bool int13_enable_48bitLBA;

static const unsigned char IDE_default_IRQs[4] = {
    14, /* primary */
    15, /* secondary */
    11, /* tertiary */
    10  /* quaternary */
};

static const unsigned short IDE_default_bases[4] = {
    0x1F0,  /* primary */
    0x170,  /* secondary */
    0x1E8,  /* tertiary */
    0x168   /* quaternary */
};

static const unsigned short IDE_default_alts[4] = {
    0x3F6,  /* primary */
    0x376,  /* secondary */
    0x3EE,  /* tertiary */
    0x36E   /* quaternary */
};

bool fdc_takes_port_3F7();

static void ide_pc98ctlio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu ide_pc98ctlio_r(Bitu port,Bitu iolen);
static void ide_altio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu ide_altio_r(Bitu port,Bitu iolen);
static void ide_baseio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu ide_baseio_r(Bitu port,Bitu iolen);
bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom);
bool IDE_CDROM_Eject(int index,bool slave);

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

#if 0//unused
static inline bool drivehead_is_lba48(uint8_t val) {
    return (val&0xE0) == 0x40;
}
#endif

static inline bool drivehead_is_lba(uint8_t val) {
    return (val&0xE0) == 0xE0;
}

#if 0//unused
static inline bool drivehead_is_chs(uint8_t val) {
    return (val&0xE0) == 0xA0;
}
#endif

class IDEDevice {
public:
    IDEController *controller;
    uint16_t feature,count,lba[3];  /* feature = BASE+1  count = BASE+2   lba[3] = BASE+3,+4,+5 */
    uint8_t hob_feature,hob_count,hob_lba[3]; /* the previous contents of those registers (LBA48 "high order bytes") */
    uint8_t command,drivehead,status; /* command/status = BASE+7  drivehead = BASE+6 */
    enum IDEDeviceType type;
    bool faked_command; /* if set, DOSBox is sending commands to itself */
    bool allow_writing;
    bool irq_signal;
    bool motor_on;
    bool asleep;
    bool slave;
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
    double ide_select_delay;    /* time between writing 0x1F6 and drive readiness */
    double ide_spinup_delay;    /* time it takes to spin the hard disk motor up to speed */
    double ide_spindown_delay;  /* time it takes for hard disk motor to spin down */
    double ide_identify_command_delay;
public:
    void raise_irq();
    void lower_irq();
public:
    IDEDevice(IDEController *c,bool _slave);
    virtual ~IDEDevice();
    virtual void host_reset_begin();    /* IDE controller -> upon setting bit 2 of alt (0x3F6) */
    virtual void host_reset_complete(); /* IDE controller -> upon setting bit 2 of alt (0x3F6) */
    virtual void set_signature();       /* device signature in the registers (after reset, diagnostics) */
    virtual void execute_device_diagnostic(); /* EXECUTE DEVICE DIAGNOSTIC (90h), done by both devices */
    void abort_with_error(uint8_t err); /* end the command with ERR and this error register value, and IRQ */
    virtual void select(uint8_t ndh,bool switched_to);
    virtual void deselect();
    virtual void abort_error();
    virtual void abort_normal();
    virtual void interface_wakeup();
    virtual void writecommand(uint8_t cmd);
    virtual Bitu data_read(Bitu iolen); /* read from 1F0h data port from IDE device */
    virtual void data_write(Bitu v,Bitu iolen);/* write to 1F0h data port to IDE device */
    virtual bool command_interruption_ok(uint8_t cmd);
    virtual void abort_silent();
};

class IDEATADevice:public IDEDevice {
public:
    IDEATADevice(IDEController *c,unsigned char disk_index,bool _slave);
    ~IDEATADevice();
    void writecommand(uint8_t cmd) override;
public:
    std::string id_serial;
    std::string id_firmware_rev;
    std::string id_model;
    unsigned char bios_disk_index;
    imageDisk *getBIOSdisk();
    void update_from_biosdisk();
    virtual Bitu data_read(Bitu iolen) override; /* read from 1F0h data port from IDE device */
    virtual void data_write(Bitu v,Bitu iolen) override;/* write to 1F0h data port to IDE device */
    virtual void generate_identify_device();
    virtual void prepare_read(Bitu offset,Bitu size);
    virtual void prepare_write(Bitu offset,Bitu size);
    virtual void io_completion();
    void host_reset_complete() override;
    void set_signature() override;
    void delayed_command();
public:
    /* command execution: the address and count of the command are taken from the registers when
     * the command starts, and the registers are updated from them when it ends */
    bool decode_address(bool ext,bool check);
    void update_address_registers(uint64_t addr);
    unsigned int block_count();         /* sectors in the next DRQ data block */
    bool command_is_ext();
    bool command_is_multiple();
    bool command_is_write();
    void sector_error(uint8_t err);     /* error at cur_lba: IDNF/UNC/ABRT with the registers pointing at it */
    void read_block();
    void write_block();
    void schedule(double at_ms);        /* run IDE_DelayedCommand at this time (PIC_FullIndex() based) */
    /* drive mechanics */
    void mech_init();
    double media_access(uint64_t addr,unsigned int n,bool write,double t);
    double spin_up(double t);
    double seek_ms(uint64_t to_cyl) const;
    void spin_down(double t,bool sleep);
    void note_activity(double t);
    void set_standby_timer(uint8_t v);
public:
    double bios_access(uint64_t addr,unsigned int n,IDEBiosDiskOp op,double now);
    uint64_t multiple_sector_max,multiple_sector_count;
    uint64_t heads,sects,cyls,progress_count;
    uint64_t phys_heads,phys_sects,phys_cyls; /* CHS value of IDE  */
    uint64_t LBA;
    unsigned char sector[512 * 128] = {};
    uint64_t sector_i,sector_total;
    bool geo_translate;
    /* command in progress */
    uint64_t cur_lba = 0;               /* next sector of the command */
    uint32_t cur_count = 0;             /* sectors left */
    uint32_t cur_block = 0;             /* sectors in the current DRQ data block */
    bool cmd_ext = false;               /* LBA48 command */
    bool cmd_lba = false;               /* LBA addressing (else C/H/S) */
    /* settings */
    bool write_cache = true;            /* SET FEATURES 02h/82h */
    bool read_lookahead = true;         /* SET FEATURES AAh/55h */
    /* drive mechanics: seek, rotation, read-ahead buffer, write-behind, spindle motor */
    bool instant = false;               /* [ide] hard disk instant mode */
    double rpm = 5400;
    double rev_ms = 0;                  /* one revolution */
    double sector_ms = 0;               /* one sector passing under the head */
    unsigned int media_spt = 63;        /* sectors per track on the platter */
    unsigned int media_heads = 4;
    uint64_t media_cyls = 1;
    double spin_origin = 0;             /* the disk is at angle 0 at this time */
    bool spinning = true;
    double spin_ready_at = 0;           /* up to speed at this time */
    double spinup_ms = 4000,spindown_ms = 1500;
    uint64_t head_cyl = 0;              /* cylinder the heads are on */
    bool ra_valid = false;              /* read-ahead buffer */
    uint64_t ra_first = 0,ra_next = 0,ra_limit = 0;
    double ra_time = 0;                 /* sector ra_next is read at this time */
    double mech_free_at = 0;            /* the mechanism is busy (write-behind) until this time */
    /* power management */
    enum { PWR_ACTIVE=0, PWR_IDLE, PWR_STANDBY, PWR_SLEEP } power = PWR_ACTIVE;
    double standby_timer_ms = 0;        /* 0 = disabled */
    double last_activity = 0;
};

enum {
    LOAD_NO_DISC=0,
    LOAD_INSERT_CD,         /* user is "inserting" the CD */
    LOAD_IDLE,          /* disc is stationary, not spinning */
    LOAD_DISC_LOADING,      /* disc is "spinning up" */
    LOAD_DISC_READIED,      /* disc just "became ready" */
    LOAD_READY
};

class IDEATAPICDROMDevice:public IDEDevice {
public:
    IDEATAPICDROMDevice(IDEController *c,unsigned char drive_index,bool _slave,CDROM_Interface *cdrom);
    ~IDEATAPICDROMDevice();
    void writecommand(uint8_t cmd) override;
public:
    std::string id_serial;
    std::string id_firmware_rev;
    std::string id_model;
    unsigned char drive_index;
    Bitu sector_transfer_limit = 16;
    CDROM_Interface *cdrom = NULL;
    CDROM_Interface *getMSCDEXDrive();
    std::vector<CDROM_Interface*> cdrom_swaplist;
    size_t cdrom_swaplist_pos = 0;
    Bitu data_read(Bitu iolen) override; /* read from 1F0h data port from IDE device */
    void data_write(Bitu v,Bitu iolen) override; /* write to 1F0h data port to IDE device */
    virtual void swap_to_next_cd();
    virtual void drop_all_cds();
    virtual void generate_identify_device();
    virtual void generate_mmc_inquiry();
    virtual void prepare_read(Bitu offset,Bitu size);
    virtual void prepare_write(Bitu offset,Bitu size);
    virtual void set_sense(unsigned char SK,unsigned char ASC=0,unsigned char ASCQ=0,unsigned int len=0);
    virtual bool common_spinup_response(bool trigger,bool wait);
    virtual void on_mode_select_io_complete();
    virtual void atapi_io_completion();
    virtual void io_completion();
    virtual void atapi_cmd_completion();
    virtual void on_atapi_busy_time();
    virtual void mechanism_status();
    virtual void read_subchannel();
    virtual void play_audio_msf();
    virtual void pause_resume();
    virtual void play_audio10();
    virtual void mode_sense();
    virtual void read_toc();
    void host_reset_complete() override;
    void set_signature() override;
    /* command completion helpers */
    void atapi_fail();                  /* CHECK CONDITION with the current sense data */
    void atapi_done();                  /* command complete, no data */
    void atapi_data_in(Bitu len);       /* data to the host is in sector[], len bytes (limited by the byte count limit) */
    bool medium_ready_check();          /* sets sense and returns false if there is no disc */
    /* commands added */
    void get_event_status_notification();
    void get_configuration();
    void read_disc_information();
    void read_track_information();
    void set_cd_speed();
    void start_stop_unit();
    void prevent_allow();
    void inquiry();
    void request_sense();
    void read_capacity();
    /* READ CD: build the requested fields of each sector */
    bool read_cd_begin();               /* parse the command, false if invalid (sense set) */
    bool read_cd_sectors(Bitu lba,Bitu count,Bitu &bytes);
    int  read_cd_sector(Bitu lba,unsigned char *out);   /* bytes written, -1 = error (sense set) */
    /* media change notification */
    void post_media_event(uint8_t code);
    /* drive mechanics: tray, spindle, seek, read rate */
    void mech_init();
    double cd_access(Bitu lba,Bitu n,double t);
    double cd_speed_x(Bitu lba);        /* read speed at this position, 1x = 75 sectors/second */
    void schedule(double at_ms);
public:
    /* READ CD request */
    uint8_t rcd_type = 0;               /* expected sector type, byte 1 bits 4-2 */
    uint8_t rcd_flags = 0;              /* byte 9: sync, header codes, user data, EDC/ECC, error field */
    uint8_t rcd_sub = 0;                /* byte 10: sub-channel selection */
    /* tray, lock, events */
    bool tray_open = false;
    bool tray_moving = false;
    bool locked = false;                /* PREVENT ALLOW MEDIUM REMOVAL */
    uint8_t media_event = 0;            /* pending GET EVENT STATUS NOTIFICATION media event, 0 = none */
    uint8_t power_event = 0;            /* pending power management event */
    uint8_t power_state = 1;            /* 1 active, 2 idle, 3 standby, 4 sleep (as GESN reports it) */
    /* mechanics */
    bool instant = false;               /* [ide] cd-rom instant mode */
    unsigned int max_speed = 8;         /* [ide] cd-rom max speed, in x (150KB/s) */
    unsigned int cur_speed = 8;         /* SET CD SPEED */
    double tray_time = 1500;
    Bitu head_lba = 0;                  /* position of the optical pickup */
    bool rb_valid = false;              /* read buffer: sectors read ahead after the last read */
    Bitu rb_first = 0,rb_next = 0,rb_limit = 0;
    double rb_time = 0;
public:
    bool mscdex = false; /* if set, attached to MSCDEX subunit */
    bool atapi_to_host;         /* if set, PACKET data transfer is to be read by host */
    double spinup_time;
    double spindown_timeout;
    double cd_insertion_time;
    Bitu host_maximum_byte_count;       /* host maximum byte count during PACKET transfer */
    std::string id_mmc_vendor_id;
    std::string id_mmc_product_id;
    std::string id_mmc_product_rev;
    Bitu TransferLengthRemaining;
    Bitu LBA,LBAnext,TransferLength;
    uint8_t TransferSectorType;
    uint8_t TransferReadCD9;
    Bitu TransferSectorSize;
    int loading_mode;
    bool has_changed;
public:
    unsigned char sense[256];
    Bitu sense_length;
    unsigned char atapi_cmd[12];
    unsigned char atapi_cmd_i,atapi_cmd_total;
    unsigned char sector[512*128];
    Bitu sector_i,sector_total;
};

class IDEController:public Module_base{
public:
    int IRQ;
    bool int13fakeio;       /* on certain INT 13h calls, force IDE state as if BIOS had carried them out */
    bool int13fakev86io;        /* on certain INT 13h calls in virtual 8086 mode, trigger fake CPU I/O traps */
    bool enable_pio32;      /* enable 32-bit PIO (if disabled, attempts at 32-bit PIO are handled as if two 16-bit I/O) */
    bool ignore_pio32;      /* if 32-bit PIO enabled, but ignored, writes do nothing, reads return 0xFFFFFFFF */
    bool register_pnp;
    unsigned short alt_io;
    unsigned short base_io;
    unsigned char interface_index;
    IO_ReadHandleObject ReadHandler[8],ReadHandlerAlt[2];
    IO_WriteHandleObject WriteHandler[8],WriteHandlerAlt[2];
public:
    IDEDevice* device[2];       /* IDE devices (master, slave) */
    Bitu select;   /* which is selected */
    bool interrupt_enable;      /* bit 1 of alt (0x3F6) */
    bool host_reset;        /* bit 2 of alt */
    bool hob;               /* bit 7 of alt: read the previous (high order) register contents */
    bool irq_pending;
    /* defaults for CD-ROM emulation */
    double spinup_time;
    double spindown_timeout;
    double cd_insertion_time;
    bool cd_instant;
    int cd_max_speed;
    int cd_tray_time;
    /* defaults for hard disk emulation */
    bool hd_instant;
    int hd_rpm;
    int hd_spinup_time;
public:
    IDEController(Section* configuration,unsigned char index);
    void register_isapnp();
    void install_io_port();
    void check_device_irq();
    ~IDEController();
private:// Sorry, IDE devices and external code don't get to force IDE IRQs anymore
    void raise_irq();
    void lower_irq();
};

const char* ideslot[] = { "Primary", "Secondary", "Tertiary", "Quaternary", "Quinternary","Sexternary", "Septernary", "Octernary" };
const char* master_slave[] = { "Master", "Slave" };

static IDEController* idecontroller[MAX_IDE_CONTROLLERS]={NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

int IDE_MatchCDROMDrive(char drv) {
	for (unsigned int c=0;c < MAX_IDE_CONTROLLERS;c++) {
		IDEController *cnt = idecontroller[c];
		if (cnt != NULL) {
			for (unsigned int d=0;d < 2;d++) {
				IDEDevice *dev = cnt->device[d];
				if (dev != NULL) {
					if (dev->type == IDE_TYPE_CDROM) {
						IDEATAPICDROMDevice *atapi = reinterpret_cast<IDEATAPICDROMDevice*>(dev);
						if (atapi->drive_index == (unsigned char)(drv - 'A'))
							return (c * 2) + d;
					}
				}
			}
		}
	}

	return -1;
}

static void IDE_DelayedCommand(Bitu pk/*which IDE device*/);
static IDEController* GetIDEController(Bitu idx);

/* ------------------------------------------------------------------------------------------
 * ATAPI CD-ROM: drive mechanics
 *
 * The drive reads at "cd-rom max speed" (or the lower speed asked for with SET CD SPEED), 1x being
 * 75 sectors per second. Drives up to 12x keep the linear velocity constant, faster drives keep the
 * angular velocity constant and read faster towards the edge of the disc. A seek moves the optical
 * pickup across the disc (about 20ms short, 250ms from edge to edge) and waits half a revolution on
 * average. After a read the drive goes on reading into its buffer, so sequential reads stream at
 * the read speed. The disc spins down when idle and takes "cd-rom spinup time" to spin up again,
 * the tray takes "cd-rom tray time" to open or close.
 * ------------------------------------------------------------------------------------------ */

#define IDE_CD_READ_BUFFER_SECTORS      64u     /* read buffer (128KB) */
#define IDE_CD_SEEK_MIN_MS              20.0
#define IDE_CD_SEEK_FULL_MS             250.0
#define IDE_CD_COMMAND_MS               1.0     /* command overhead */
#define IDE_CD_MAX_READCD_SECTOR        (2352u + 296u + 96u)

/* radius (mm) of the spiral at a sector: 25mm at the start of the program area, 58mm at 74 minutes */
static double cd_radius(Bitu lba) {
    return sqrt(25.0 * 25.0 + (double)lba * ((58.0 * 58.0 - 25.0 * 25.0) / 333000.0));
}

void IDEATAPICDROMDevice::mech_init() {
    instant = controller->cd_instant;
    max_speed = controller->cd_max_speed > 0 ? (unsigned int)controller->cd_max_speed : 8u;
    if (max_speed > 72) max_speed = 72;
    cur_speed = max_speed;
    tray_time = controller->cd_tray_time > 0 ? (double)controller->cd_tray_time : 1500.0;
    head_lba = 0;
    rb_valid = false;
}

double IDEATAPICDROMDevice::cd_speed_x(Bitu lba) {
    if (cur_speed <= 12) return (double)cur_speed;
    return (double)cur_speed * cd_radius(lba) / 58.0;
}

/* time at which n sectors from lba have been read, the access starting at time t */
double IDEATAPICDROMDevice::cd_access(Bitu lba,Bitu n,double t) {
    if (instant || faked_command) {
        head_lba = lba + n;
        return t;
    }

    const double sector_ms = 1000.0 / (75.0 * cd_speed_x(lba));

    /* in the read buffer, or being read into it now: no seek, the buffer keeps filling */
    if (rb_valid && lba >= rb_first && lba <= rb_limit && lba + n <= rb_limit + n) {
        double avail = rb_time + ((double)(lba + n) - (double)rb_next) * sector_ms;
        if (avail < t) avail = t;
        if (lba + n + IDE_CD_READ_BUFFER_SECTORS > rb_limit) rb_limit = lba + n + IDE_CD_READ_BUFFER_SECTORS;
        head_lba = lba + n;
        return avail;
    }

    const double r = cd_radius(lba),rh = cd_radius(head_lba);
    t += IDE_CD_SEEK_MIN_MS + (IDE_CD_SEEK_FULL_MS - IDE_CD_SEEK_MIN_MS) * sqrt(fabs(r - rh) / 33.0);

    /* half a revolution on average for the sector to come around (1x = 1.3m/s) */
    const double rev_ms = 2.0 * 3.14159265358979 * (cur_speed <= 12 ? r : 58.0) / (1300.0 * cur_speed) * 1000.0;
    t += rev_ms * 0.5;

    t += n * sector_ms;
    head_lba = lba + n;

    rb_valid = true;
    rb_first = lba;
    rb_next = lba + n;
    rb_time = t;
    rb_limit = lba + n + IDE_CD_READ_BUFFER_SECTORS;
    return t;
}

void IDEATAPICDROMDevice::schedule(double at_ms) {
    const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();
    double d = at_ms - PIC_FullIndex();

    if (faked_command) d = 0.000001;
    else if (d < 0.00001) d = 0.00001;

    PIC_RemoveSpecificEvents(IDE_DelayedCommand,pk);
    PIC_AddEvent(IDE_DelayedCommand,d,pk);
}

/* ------------------------------------------------------------------------------------------
 * CD-ROM sector encoding (ECMA-130): EDC (CRC-32) and the Reed-Solomon P and Q parity of
 * Mode 1 and Mode 2 Form 1 sectors, the Q sub-channel CRC. Used to give READ CD the raw
 * fields of sectors from images that only have the user data.
 * ------------------------------------------------------------------------------------------ */

static uint8_t cd_ecc_f_lut[256];
static uint8_t cd_ecc_b_lut[256];
static uint32_t cd_edc_lut[256];
static bool cd_lut_init = false;

static void cd_init_luts(void) {
    if (cd_lut_init) return;
    for (unsigned int i=0;i < 256;i++) {
        const unsigned int j = (i << 1u) ^ ((i & 0x80u) ? 0x11Du : 0u);
        cd_ecc_f_lut[i] = (uint8_t)j;
        cd_ecc_b_lut[i ^ j] = (uint8_t)i;
        uint32_t edc = i;
        for (unsigned int k=0;k < 8;k++) edc = (edc >> 1u) ^ ((edc & 1u) ? 0xD8018001u : 0u);
        cd_edc_lut[i] = edc;
    }
    cd_lut_init = true;
}

static uint32_t cd_edc(const uint8_t *src,size_t size) {
    uint32_t edc = 0;
    while (size--) edc = (edc >> 8u) ^ cd_edc_lut[(edc ^ (*src++)) & 0xFFu];
    return edc;
}

static void cd_ecc_block(const uint8_t *src,unsigned int major_count,unsigned int minor_count,
    unsigned int major_mult,unsigned int minor_inc,uint8_t *dest) {
    const unsigned int size = major_count * minor_count;
    for (unsigned int major=0;major < major_count;major++) {
        unsigned int index = (major >> 1u) * major_mult + (major & 1u);
        uint8_t ecc_a = 0,ecc_b = 0;
        for (unsigned int minor=0;minor < minor_count;minor++) {
            const uint8_t temp = src[index];
            index += minor_inc;
            if (index >= size) index -= size;
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = cd_ecc_f_lut[ecc_a];
        }
        ecc_a = cd_ecc_b_lut[cd_ecc_f_lut[ecc_a] ^ ecc_b];
        dest[major] = ecc_a;
        dest[major + major_count] = ecc_a ^ ecc_b;
    }
}

/* P and Q parity, over the header (zero for Mode 2) and data */
static void cd_ecc(uint8_t *sector,bool zero_address) {
    uint8_t address[4];
    if (zero_address) {
        memcpy(address,sector + 12,4);
        memset(sector + 12,0,4);
    }
    cd_ecc_block(sector + 0xC,86,24,2,86,sector + 0x81C);
    cd_ecc_block(sector + 0xC,52,43,86,88,sector + 0x8C8);
    if (zero_address) memcpy(sector + 12,address,4);
}

static uint8_t cd_bcd(unsigned int v) {
    return (uint8_t)(((v / 10u) << 4u) | (v % 10u));
}

/* sync and header of a data sector */
static void cd_sector_header(uint8_t *raw,Bitu lba,uint8_t mode) {
    const Bitu a = lba + 150u;
    raw[0] = 0x00;
    memset(raw + 1,0xFF,10);
    raw[11] = 0x00;
    raw[12] = cd_bcd((unsigned int)(a / (75u * 60u)));
    raw[13] = cd_bcd((unsigned int)((a / 75u) % 60u));
    raw[14] = cd_bcd((unsigned int)(a % 75u));
    raw[15] = mode;
}

/* a complete raw Mode 1 sector, or Mode 2 Form 1 sector (data sub-header), from the 2048 bytes of user data */
static void cd_encode_sector(uint8_t *raw,Bitu lba,const uint8_t *data,bool mode2) {
    cd_init_luts();
    memset(raw,0,2352);
    if (!mode2) {
        cd_sector_header(raw,lba,1);
        memcpy(raw + 16,data,2048);
        const uint32_t edc = cd_edc(raw,0x810);
        host_writed(raw + 0x810,edc);
        cd_ecc(raw,false);
    }
    else {
        cd_sector_header(raw,lba,2);
        /* sub-header (twice): file 0, channel 0, submode data, coding 0 */
        raw[18] = raw[22] = 0x08;
        memcpy(raw + 24,data,2048);
        const uint32_t edc = cd_edc(raw + 0x10,0x808);
        host_writed(raw + 0x818,edc);
        cd_ecc(raw,true);
    }
}

/* CRC-16 of the Q sub-channel (x^16 + x^12 + x^5 + 1, inverted) */
static uint16_t cd_q_crc(const uint8_t *q) {
    uint16_t crc = 0;
    for (unsigned int i=0;i < 10;i++) {
        crc ^= (uint16_t)q[i] << 8u;
        for (unsigned int b=0;b < 8;b++) crc = (crc & 0x8000u) ? (uint16_t)((crc << 1u) ^ 0x1021u) : (uint16_t)(crc << 1u);
    }
    return (uint16_t)~crc;
}

/* ------------------------------------------------------------------------------------------
 * ATAPI CD-ROM: spin up, spin down, CD insertion
 * ------------------------------------------------------------------------------------------ */

static void IDE_ATAPI_SpinDown(Bitu pk/*which IDE device*/) {
	IDEEventPack ep(pk);
	const unsigned int idx = ep.interface();
	const unsigned int devidx = ep.device();

	IDEController *ctrl = GetIDEController(idx);
	if (ctrl == NULL) return;

	IDEDevice *dev = ctrl->device[devidx];
	if (dev == NULL) return;

	if (dev->type == IDE_TYPE_HDD) {
	}
	else if (dev->type == IDE_TYPE_CDROM) {
		IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

		if (atapi->loading_mode == LOAD_DISC_READIED || atapi->loading_mode == LOAD_READY) {
			/* not while playing audio */
			CDROM_Interface *cdrom = atapi->getMSCDEXDrive();
			bool playing = false,pause = false;
			if (cdrom != NULL && cdrom->GetAudioStatus(playing,pause) && playing && !pause) {
				PIC_AddEvent(IDE_ATAPI_SpinDown,atapi->spindown_timeout/*ms*/,pk);
				return;
			}

			atapi->loading_mode = LOAD_IDLE;
			atapi->rb_valid = false;
			LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM: spinning down");
		}
	}
	else {
		LOG_MSG("Unknown ATAPI spinup callback\n");
	}
}

static void IDE_ATAPI_SpinUpComplete(Bitu pk/*which IDE device*/);

static void IDE_ATAPI_CDInsertion(Bitu pk/*which IDE device*/) {
	IDEEventPack ep(pk);
	const unsigned int idx = ep.interface();
	const unsigned int devidx = ep.device();

	IDEController *ctrl = GetIDEController(idx);
	if (ctrl == NULL) return;

	IDEDevice *dev = ctrl->device[devidx];
	if (dev == NULL) return;

	if (dev->type == IDE_TYPE_HDD) {
	}
	else if (dev->type == IDE_TYPE_CDROM) {
		IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

		if (atapi->loading_mode == LOAD_INSERT_CD) {
			atapi->loading_mode = LOAD_DISC_LOADING;
			atapi->tray_open = false;
			LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM: insert CD to loading");
			PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
			PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,pk);
			PIC_AddEvent(IDE_ATAPI_SpinUpComplete,atapi->instant ? 0.01 : atapi->spinup_time/*ms*/,pk);
		}
	}
	else {
		LOG_MSG("Unknown ATAPI spinup callback\n");
	}
}

static void IDE_ATAPI_SpinUpComplete(Bitu pk/*which IDE device*/) {
	IDEEventPack ep(pk);
	const unsigned int idx = ep.interface();
	const unsigned int devidx = ep.device();

	IDEController *ctrl = GetIDEController(idx);
	if (ctrl == NULL) return;

	IDEDevice *dev = ctrl->device[devidx];
	if (dev == NULL) return;

	if (dev->type == IDE_TYPE_HDD) {
	}
	else if (dev->type == IDE_TYPE_CDROM) {
		IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

		if (atapi->loading_mode == LOAD_DISC_LOADING) {
			atapi->loading_mode = LOAD_DISC_READIED;
			LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM: spinup complete");
			/* media change notification: a new disc is ready */
			if (atapi->has_changed) atapi->post_media_event(0x02/*NewMedia*/);
			PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
			PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,pk);
			PIC_AddEvent(IDE_ATAPI_SpinDown,atapi->spindown_timeout/*ms*/,pk);
		}
	}
	else {
		LOG_MSG("Unknown ATAPI spinup callback\n");
	}
}

void IDEATAPICDROMDevice::post_media_event(uint8_t code) {
    /* a NewMedia event is more important than a removal before it */
    if (media_event == 0 || code == 0x02/*NewMedia*/ || code == 0x01/*EjectRequest*/) media_event = code;
}

/* returns "true" if command should proceed as normal, "false" if sense data was set and command should not proceed.
 * this function helps to enforce virtual "spin up" and "ready" delays. */
bool IDEATAPICDROMDevice::common_spinup_response(bool trigger,bool wait) {
	const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();

	/* the tray is open: no disc */
	if (tray_open) {
		set_sense(/*SK=*/0x02,/*ASC=*/0x3A,/*ASCQ=*/0x02); /* Medium Not Present - Tray Open */
		return false;
	}

	if (loading_mode == LOAD_IDLE) {
		if (trigger) {
			LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM: triggered to spin up from idle");
			loading_mode = LOAD_DISC_LOADING;
			PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
			PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,pk);
			PIC_AddEvent(IDE_ATAPI_SpinUpComplete,instant ? 0.01 : spinup_time/*ms*/,pk);
		}
	}
	else if (loading_mode == LOAD_READY) {
		if (trigger) {
			PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
			PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,pk);
			PIC_AddEvent(IDE_ATAPI_SpinDown,spindown_timeout/*ms*/,pk);
		}
	}

	/* if the CD-ROM drive has mounted the empty drive, then ALWAYS return Medium Not Present */
	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == NULL || cdrom->class_id == CDROM_Interface::INTERFACE_TYPE::ID_FAKE) {
		/* the guest is told once that the disc went away (unit attention), then that there is none */
		if (has_changed) {
			has_changed = false;
			set_sense(/*SK=*/0x06,/*ASC=*/0x28,/*ASCQ=*/0x00); /* Not Ready to Ready change, medium may have changed */
			return false;
		}
		set_sense(/*SK=*/0x02,/*ASC=*/0x3A,/*ASCQ=*/0x01); /* Medium Not Present - Tray Closed */
		return false;
	}

	switch (loading_mode) {
		case LOAD_NO_DISC:
		case LOAD_INSERT_CD:
			set_sense(/*SK=*/0x02,/*ASC=*/0x3A,/*ASCQ=*/0x01); /* Medium Not Present - Tray Closed */
			return false;
		case LOAD_DISC_LOADING:
			if (has_changed && !wait/*if command will block until LOADING complete*/) {
				set_sense(/*SK=*/0x02,/*ASC=*/0x04,/*ASCQ=*/0x01); /* Medium is becoming available */
				return false;
			}
			break;
		case LOAD_DISC_READIED:
			loading_mode = LOAD_READY;
			if (has_changed) {
				if (trigger) has_changed = false;
				set_sense(/*SK=*/0x06,/*ASC=*/0x28,/*ASCQ=*/0x00); /* UNIT ATTENTION: Not Ready to Ready change, medium may have changed */
				return false;
			}
			break;
		case LOAD_IDLE:
		case LOAD_READY:
			break;
		default:
			abort();
	}

	return true;
}

/* ------------------------------------------------------------------------------------------
 * ATAPI CD-ROM: command completion helpers
 * ------------------------------------------------------------------------------------------ */

/* CHECK CONDITION: the error register has the sense key (and ABRT), the host reads the details with REQUEST SENSE */
void IDEATAPICDROMDevice::atapi_fail() {
    count = 0x03;
    state = IDE_DEV_READY;
    feature = (uint16_t)(((sense[2] & 0xFu) << 4u) | 0x04u);
    status = IDE_STATUS_DRIVE_READY|IDE_STATUS_ERROR;
    sector_total = 0;
    TransferLength = 0;
    TransferLengthRemaining = 0;
    lba[1] = lba[2] = 0;
    raise_irq();
    allow_writing = true;
}

void IDEATAPICDROMDevice::atapi_done() {
    count = 0x03;
    state = IDE_DEV_READY;
    feature = 0x00;
    status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
    sector_total = 0;
    lba[1] = lba[2] = 0;
    raise_irq();
    allow_writing = true;
}

/* data for the host is in sector[]: limited by the byte count the host asked for */
void IDEATAPICDROMDevice::atapi_data_in(Bitu len) {
    if (len > host_maximum_byte_count) len = host_maximum_byte_count & ~((Bitu)1u);
    if (len == 0) {
        atapi_done();
        return;
    }
    prepare_read(0,len);
    feature = 0x00;
    count = 0x02; /* data for computer */
    state = IDE_DEV_DATA_READ;
    status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;
    /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
    lba[2] = sector_total >> 8;
    lba[1] = sector_total;
    raise_irq();
    allow_writing = true;
}

bool IDEATAPICDROMDevice::medium_ready_check() {
    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (tray_open) {
        set_sense(0x02,0x3A,0x02);
        return false;
    }
    if (cdrom == NULL || cdrom->class_id == CDROM_Interface::INTERFACE_TYPE::ID_FAKE) {
        set_sense(0x02,0x3A,0x01);
        return false;
    }
    return true;
}

/* sectors on the disc (up to the lead-out) */
static Bitu atapi_capacity(CDROM_Interface *cdrom) {
    int first,last;
    TMSF leadOut;
    if (cdrom == NULL || !cdrom->GetAudioTracks(first,last,leadOut)) return 0;
    const Bitu f = ((Bitu)leadOut.min * 60u + leadOut.sec) * 75u + leadOut.fr;
    return f >= 150u ? f - 150u : 0u;
}

void IDEATAPICDROMDevice::read_subchannel() {
//  unsigned char Format = atapi_cmd[2] & 0xF;
//  unsigned char Track = atapi_cmd[6];
    unsigned char paramList = atapi_cmd[3];
    unsigned char attr,track,index;
    bool SUBQ = !!(atapi_cmd[2] & 0x40);
    bool TIME = !!(atapi_cmd[1] & 2);
    unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8) + atapi_cmd[8];
    unsigned char *write;
    unsigned char astat;
    bool playing,pause;
    TMSF rel,abs;

    memset(sector,0,48);

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        prepare_read(0,MIN(4u,AllocationLength));
        return;
    }

    if (paramList == 0 || paramList > 3) {
        set_sense(0x05,0x24); /* Invalid Field in CDB */
        sector_total = 0;
        return;
    }

    if (!cdrom->GetAudioStatus(playing,pause))
        playing = pause = false;

    if (playing)
        astat = pause ? 0x12 : 0x11;
    else
        astat = 0x13;

    write = sector;
    *write++ = 0x00;
    *write++ = astat;/* AUDIO STATUS */
    *write++ = 0x00;/* SUBCHANNEL DATA LENGTH */
    *write++ = 0x00;

    if (SUBQ) {
        if (paramList == 2) {
            /* Media Catalog Number: not on these discs (MCVAL=0) */
            *write++ = 0x02;
            write += 3;
            *write++ = 0x00;
            write += 15;
        }
        else if (paramList == 3) {
            /* ISRC: not on these discs (TCVAL=0) */
            *write++ = 0x03;
            *write++ = 0x00;
            *write++ = atapi_cmd[6];
            *write++ = 0x00;
            *write++ = 0x00;
            write += 15;
        }
        else {
            /* get current subchannel position */
            if (!cdrom->GetAudioSub(attr,track,index,rel,abs)) {
                attr = 0x40;
                track = index = 1;
                rel.min = rel.sec = rel.fr = 0;
                abs.min = 0; abs.sec = 2; abs.fr = 0;
            }

            *write++ = 0x01;    /* subchannel data format code */
            *write++ = (attr >> 4) | 0x10;  /* ADR/CONTROL */
            *write++ = track;
            *write++ = index;
            if (TIME) {
                *write++ = 0x00;
                *write++ = abs.min;
                *write++ = abs.sec;
                *write++ = abs.fr;
                *write++ = 0x00;
                *write++ = rel.min;
                *write++ = rel.sec;
                *write++ = rel.fr;
            }
            else {
                uint32_t sec;

                sec = (abs.min*60u*75u)+(abs.sec*75u)+abs.fr - 150u;
                *write++ = (unsigned char)(sec >> 24u);
                *write++ = (unsigned char)(sec >> 16u);
                *write++ = (unsigned char)(sec >> 8u);
                *write++ = (unsigned char)(sec >> 0u);

                sec = (rel.min*60u*75u)+(rel.sec*75u)+rel.fr;
                *write++ = (unsigned char)(sec >> 24u);
                *write++ = (unsigned char)(sec >> 16u);
                *write++ = (unsigned char)(sec >> 8u);
                *write++ = (unsigned char)(sec >> 0u);
            }
        }
    }

    {
        unsigned int x = (unsigned int)(write-sector) - 4;
        sector[2] = x >> 8;
        sector[3] = x;
    }

    prepare_read(0,MIN(MIN((unsigned int)(write-sector),(unsigned int)host_maximum_byte_count),AllocationLength));
}

void IDEATAPICDROMDevice::mechanism_status() {
    unsigned int AllocationLength = ((unsigned int)atapi_cmd[8] << 8) + atapi_cmd[9];
    unsigned char *write;
    bool playing = false,pause = false;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom != NULL) cdrom->GetAudioStatus(playing,pause);

    write = sector;

    /* MECHANISM STATUS PARAMETER LIST */
    /* - Status Header */
    /* - Slot Table(s) */

    /* Status Header */
    *write++ = 0x00; // fault=0 changerstate=0 currentslot=0
    *write++ = (unsigned char)(((playing && !pause) ? 1u : 0u) << 5u) /* mechanism state: idle or playing audio */ | (tray_open ? 0x10 : 0x00)/*door open*/;
    *write++ = (unsigned char)(head_lba >> 16u); // current LBA
    *write++ = (unsigned char)(head_lba >> 8u);
    *write++ = (unsigned char)head_lba;
    *write++ = 0x00; // number of slots available = 0
    *write++ = 0x00; // length of slot table(s)
    *write++ = 0x00; // .

    /* Slot table(s) */
    // None, we're not emulating ourselves as a CD changer.

    // TODO: Actually this command might be a neat way to expose the CD-ROM
    //       "swap chain" the user might have set up with IMGMOUNT before
    //       booting the guest OS. If enabled, we should report each and
    //       every ISO image like we're a CD changer. :)

    prepare_read(0,MIN(MIN((unsigned int)(write-sector),(unsigned int)host_maximum_byte_count),AllocationLength));
}

/* MODE SENSE (6) and (10). Page control: current, changeable (none are), default, saved (not supported) */
void IDEATAPICDROMDevice::mode_sense() {
    const bool ten = (atapi_cmd[0] == 0x5A);
    const unsigned char PC = atapi_cmd[2] >> 6;
    const unsigned char PAGE = atapi_cmd[2] & 0x3F;
    const unsigned int AllocationLength = ten ? (((unsigned int)atapi_cmd[7] << 8) + atapi_cmd[8]) : atapi_cmd[4];
    const unsigned int hdr = ten ? 8u : 4u;
    unsigned char *write;
    unsigned int x;
    bool found = false;

    if (PC == 3) {
        set_sense(0x05,0x39); /* Saving Parameters Not Supported */
        sector_total = 0;
        return;
    }

    memset(sector,0,512);
    write = sector + hdr;

    /* NTS: MMC-3 Table 342 says that BLOCK DESCRIPTOR LENGTH is zero, where it would be 8 for legacy units */

    auto page_begin = [&](unsigned char code,unsigned char len) -> unsigned char * {
        unsigned char *p = write;
        p[0] = code;
        p[1] = len;
        write += 2u + len;
        found = true;
        return p + 2;
    };

    if (PAGE == 0x01 || PAGE == 0x3F) { /* Read error recovery MMC-3 Section 6.3.4 table 344 */
        unsigned char *p = page_begin(0x01,10);
        if (PC != 1) p[1] = 3;          /* +3 Read Retry Count */
    }
    if (PAGE == 0x0D || PAGE == 0x3F) { /* CD device parameters */
        unsigned char *p = page_begin(0x0D,6);
        if (PC != 1) {
            p[1] = 0x0D;                /* +3 inactivity timer multiplier */
            p[3] = 60;                  /* +4 S units per M unit */
            p[5] = 75;                  /* +6 F units per S unit */
        }
    }
    if (PAGE == 0x0E || PAGE == 0x3F) { /* CD-ROM audio control MMC-3 Section 6.3.7 table 354 */
        unsigned char *p = page_begin(0x0E,14);
        if (PC != 1) {
            p[0] = 0x04;                /* +2 Reserved|IMMED=1|SOTC=0|Reserved */
            p[5] = 75;                  /* +7 Obsolete (75) */
            p[6] = 0x01;                /* +8 output port 0 selection (0001b = channel 0) */
            p[7] = 0xFF;                /* +9 output port 0 volume (0xFF = 0dB atten.) */
            p[8] = 0x02;                /* +10 output port 1 selection (0010b = channel 1) */
            p[9] = 0xFF;                /* +11 output port 1 volume (0xFF = 0dB atten.) */
        }
    }
    if (PAGE == 0x2A || PAGE == 0x3F) { /* CD-ROM mechanical status MMC-3 Section 6.3.11 table 361 */
        unsigned char *p = page_begin(0x2A,20);
        if (PC != 1) {
            p[0] = 0x07;    /* +2 DVD-RAM read|DVD-R read|DVD-ROM read|Method 2|CD-RW read|CD-R read */
            p[1] = 0x00;    /* +3 (no writing) */
            p[2] = 0x71;    /* +4 Multisession|Mode 2 form 2|Mode 2 form 1|Audio play */
            p[3] = 0x1F;    /* +5 C2 Pointers|R-W deinterleaved|R-W supported|CDDA accurate|CDDA support */
            p[4] = (unsigned char)(0x29 | (locked ? 0x02 : 0x00)); /* +6 Loading mechanism tray|Eject|Lock state|Lock */
            p[5] = 0x03;    /* +7 Separate channel mute|Separate volume levels */

            x = 176 * max_speed;        /* +8 maximum speed supported in kB/s (obsolete in MMC-3) */
            p[6] = x >> 8; p[7] = x;
            x = 256;                    /* +10 Number of volume levels supported */
            p[8] = x >> 8; p[9] = x;
            x = 128;                    /* +12 buffer size supported by drive in kB */
            p[10] = x >> 8; p[11] = x;
            x = 176 * cur_speed;        /* +14 current read speed selected in kB/s (obsolete in MMC-3) */
            p[12] = x >> 8; p[13] = x;
        }
    }

    if (!found) {
        LOG(LOG_MISC,LOG_DEBUG)("ATAPI: MODE SENSE on page 0x%02x not supported",PAGE);
        set_sense(0x05,0x24); /* Invalid Field in CDB */
        sector_total = 0;
        return;
    }

    /* mode parameter header: data length */
    x = (unsigned int)(write-sector) - (ten ? 2u : 1u);
    if (ten) {
        sector[0] = (unsigned char)(x >> 8u);
        sector[1] = (unsigned char)x;
    }
    else {
        sector[0] = (unsigned char)x;
    }

    prepare_read(0,MIN(MIN((unsigned int)(write-sector),(unsigned int)host_maximum_byte_count),AllocationLength));
}

void IDEATAPICDROMDevice::pause_resume() {
    bool Resume = !!(atapi_cmd[8] & 1);

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        sector_total = 0;
        return;
    }

    cdrom->PauseAudio(Resume);
}

void IDEATAPICDROMDevice::play_audio_msf() {
    uint32_t start_lba,end_lba;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        sector_total = 0;
        return;
    }

    if (atapi_cmd[3] == 0xFF && atapi_cmd[4] == 0xFF && atapi_cmd[5] == 0xFF)
        start_lba = 0xFFFFFFFF;
    else {
        start_lba = (atapi_cmd[3] * 60u * 75u) +
            (atapi_cmd[4] * 75u) +
            atapi_cmd[5];

        if (start_lba >= 150u) start_lba -= 150u; /* LBA sector 0 == M:S:F sector 0:2:0 */
        else start_lba = 0;
    }

    if (atapi_cmd[6] == 0xFF && atapi_cmd[7] == 0xFF && atapi_cmd[8] == 0xFF)
        end_lba = 0xFFFFFFFF;
    else {
        end_lba = (atapi_cmd[6] * 60u * 75u) +
            (atapi_cmd[7] * 75u) +
            atapi_cmd[8];

        if (end_lba >= 150u) end_lba -= 150u; /* LBA sector 0 == M:S:F sector 0:2:0 */
        else end_lba = 0;
    }

    if (start_lba == end_lba) {
        /* The play length field specifies the number of contiguous logical blocks that shall
         * be played. A play length of zero indicates that no audio operation shall occur.
         * This condition is not an error. */
        sector_total = 0;
        return;
    }

    /* LBA 0xFFFFFFFF means start playing wherever the optics of the CD sit */
    if (start_lba != 0xFFFFFFFF) {
        cdrom->PlayAudioSector(start_lba,end_lba - start_lba);
        head_lba = start_lba;
        rb_valid = false;
    }
    else
        cdrom->PauseAudio(true);

    sector_total = 0;
}

void IDEATAPICDROMDevice::play_audio10() {
    uint16_t play_length;
    uint32_t start_lba;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        sector_total = 0;
        return;
    }

    start_lba = ((uint32_t)atapi_cmd[2] << 24) +
        ((uint32_t)atapi_cmd[3] << 16) +
        ((uint32_t)atapi_cmd[4] << 8) +
        ((uint32_t)atapi_cmd[5] << 0);

    play_length = ((uint16_t)atapi_cmd[7] << 8) +
        ((uint16_t)atapi_cmd[8] << 0);

    if (play_length == 0) {
        /* The play length field specifies the number of contiguous logical blocks that shall
         * be played. A play length of zero indicates that no audio operation shall occur.
         * This condition is not an error. */
        sector_total = 0;
        return;
    }

    /* LBA 0xFFFFFFFF means start playing wherever the optics of the CD sit */
    if (start_lba != 0xFFFFFFFF) {
        cdrom->PlayAudioSector(start_lba,play_length);
        head_lba = start_lba;
        rb_valid = false;
    }
    else
        cdrom->PauseAudio(true);

    sector_total = 0;
}

void IDE_Eject_unmount(char drive);

void IDEATAPICDROMDevice::read_toc() {
    /* NTS: The SCSI MMC standards say we're allowed to indicate the return data
     *      is longer than it's allocation length. But here's the thing: some MS-DOS
     *      CD-ROM drivers will ask for the TOC but only provide enough room for one
     *      entry (OAKCDROM.SYS) and if we signal more data than its buffer, it will
     *      reject our response and render the CD-ROM drive inaccessible. So to make
     *      this emulation work, we have to cut our response short to the driver's
     *      allocation length */
    unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8) + atapi_cmd[8];
    unsigned char Format = atapi_cmd[2] & 0xF;
    unsigned char Track = atapi_cmd[6];
    bool TIME = !!(atapi_cmd[1] & 2);
    unsigned char *write;
    int first,last,track;
    TMSF leadOut;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        set_sense(0x02,0x3A,0x01);
        sector_total = 0;
        return;
    }

    memset(sector,0,8);

    if (!cdrom->GetAudioTracks(first,last,leadOut)) {
        LOG(LOG_MISC,LOG_DEBUG)("ATAPI READ TOC failed to get track info");
        set_sense(0x02,0x3A,0x01);
        sector_total = 0;
        return;
    }

    /* the starting track must exist (or be the lead-out) */
    if (Format == 0 && Track > last && Track != 0xAA) {
        set_sense(0x05,0x24);
        sector_total = 0;
        return;
    }

    /* start 2 bytes out. we'll fill in the data length later */
    write = sector + 2;

    if (Format == 1) { /* Read multisession info */
        unsigned char attr;
        TMSF start;
        const int sessions = cdrom->GetSessionCount();

        *write++ = (unsigned char)1;        /* @+2 first complete session */
        *write++ = (unsigned char)sessions; /* @+3 last complete session */

        /* the first track of the last session */
        {
            int sfirst,slast;
            TMSF slead;
            unsigned char dtype;
            if (sessions > 1 && cdrom->GetSessionInfo(sessions,sfirst,slast,slead,dtype)) first = sfirst;
        }

        if (!cdrom->GetAudioTrackInfo(first,start,attr)) {
            LOG(LOG_MISC,LOG_DEBUG)("ATAPI READ TOC unable to read track %u information",first);
            attr = 0x41; /* ADR=1 CONTROL=4 */
            start.min = 0;
            start.sec = 0;
            start.fr = 0;
        }

        *write++ = 0x00;        /* entry+0 RESERVED */
        *write++ = (attr >> 4) | 0x10;  /* entry+1 ADR=1 CONTROL=4 (DATA) */
        *write++ = (unsigned char)first;/* entry+2 TRACK */
        *write++ = 0x00;        /* entry+3 RESERVED */

        /* then, start address of first track in session */
        if (TIME) {
            *write++ = 0x00;
            *write++ = start.min;
            *write++ = start.sec;
            *write++ = start.fr;
        }
        else {
            uint32_t sec = (start.min*60u*75u)+(start.sec*75u)+start.fr - 150u;
            *write++ = (unsigned char)(sec >> 24u);
            *write++ = (unsigned char)(sec >> 16u);
            *write++ = (unsigned char)(sec >> 8u);
            *write++ = (unsigned char)(sec >> 0u);
        }
    }
    else if (Format == 0) { /* Read table of contents */
        *write++ = (unsigned char)first;    /* @+2 */
        *write++ = (unsigned char)last;     /* @+3 */

        for (track=first;track <= last;track++) {
            unsigned char attr;
            TMSF start;

            if (!cdrom->GetAudioTrackInfo(track,start,attr)) {
                LOG(LOG_MISC,LOG_DEBUG)("ATAPI READ TOC unable to read track %u information",track);
                attr = 0x41; /* ADR=1 CONTROL=4 */
                start.min = 0;
                start.sec = 0;
                start.fr = 0;
            }

            if (track < Track)
                continue;
            if ((write+8) > (sector+AllocationLength))
                break;

            *write++ = 0x00;        /* entry+0 RESERVED */
            *write++ = (attr >> 4) | 0x10; /* entry+1 ADR=1 CONTROL=4 (DATA) */
            *write++ = (unsigned char)track;/* entry+2 TRACK */
            *write++ = 0x00;        /* entry+3 RESERVED */
            if (TIME) {
                *write++ = 0x00;
                *write++ = start.min;
                *write++ = start.sec;
                *write++ = start.fr;
            }
            else {
                uint32_t sec = (start.min*60u*75u)+(start.sec*75u)+start.fr - 150u;
                *write++ = (unsigned char)(sec >> 24u);
                *write++ = (unsigned char)(sec >> 16u);
                *write++ = (unsigned char)(sec >> 8u);
                *write++ = (unsigned char)(sec >> 0u);
            }
        }

        if ((write+8) <= (sector+AllocationLength)) {
            *write++ = 0x00;
            *write++ = 0x14;
            *write++ = 0xAA;/*TRACK*/
            *write++ = 0x00;
            if (TIME) {
                *write++ = 0x00;
                *write++ = leadOut.min;
                *write++ = leadOut.sec;
                *write++ = leadOut.fr;
            }
            else {
                uint32_t sec = (leadOut.min*60u*75u)+(leadOut.sec*75u)+leadOut.fr - 150u;
                *write++ = (unsigned char)(sec >> 24u);
                *write++ = (unsigned char)(sec >> 16u);
                *write++ = (unsigned char)(sec >> 8u);
                *write++ = (unsigned char)(sec >> 0u);
            }
        }
    }
    else if (Format == 2) { /* Read full TOC (raw Q subchannel TOC of the lead-ins), always MSF */
        const int sessions = cdrom->GetSessionCount();

        *write++ = (unsigned char)1;        /* @+2 first complete session */
        *write++ = (unsigned char)sessions; /* @+3 last complete session */

        auto entry = [&](int session,unsigned char adr_ctl,unsigned char point,const TMSF &a,unsigned char zero,const TMSF &p) {
            if ((write+11) > (sector+AllocationLength)) return;
            *write++ = (unsigned char)session;
            *write++ = adr_ctl;
            *write++ = 0x00;                /* TNO */
            *write++ = point;
            *write++ = a.min;
            *write++ = a.sec;
            *write++ = a.fr;
            *write++ = zero;
            *write++ = p.min;
            *write++ = p.sec;
            *write++ = p.fr;
        };
        const TMSF none = {0,0,0};
        for (int s=1;s <= sessions;s++) {
            int sfirst,slast;
            TMSF slead;
            unsigned char dtype;
            if (!cdrom->GetSessionInfo(s,sfirst,slast,slead,dtype)) continue;

            TMSF fstart,lstart;
            unsigned char fattr = 0,lattr = 0;
            if (!cdrom->GetAudioTrackInfo(sfirst,fstart,fattr)) fattr = 0;
            if (!cdrom->GetAudioTrackInfo(slast,lstart,lattr)) lattr = 0;
            const unsigned char fctl = 0x10 | (fattr >> 4), lctl = 0x10 | (lattr >> 4);

            TMSF a0 = {(unsigned char)sfirst,dtype,0};
            TMSF a1 = {(unsigned char)slast,0,0};
            entry(s,fctl,0xA0,none,0,a0);
            entry(s,lctl,0xA1,none,0,a1);
            entry(s,lctl,0xA2,none,0,slead);
            for (int t=sfirst;t <= slast;t++) {
                unsigned char attr;
                TMSF start;
                if (!cdrom->GetAudioTrackInfo(t,start,attr)) continue;
                entry(s,0x10 | (attr >> 4),(unsigned char)t,none,0,start);
            }
            if (s < sessions) {
                /* ADR 5 (multisession) pointers: B0 = start of the next program area (the next
                 * session's first pregap) and the maximum lead-out; C0 (session 1 only) = the
                 * start of the disc's first lead-in, as on recorded CD-R media. */
                int nfirst,nlast;
                TMSF nlead,nstart;
                unsigned char ndtype,nattr;
                if (cdrom->GetSessionInfo(s+1,nfirst,nlast,nlead,ndtype) && cdrom->GetAudioTrackInfo(nfirst,nstart,nattr)) {
                    unsigned int f = ((unsigned int)nstart.min*60u+nstart.sec)*75u+nstart.fr - 150u;
                    TMSF next = {(unsigned char)(f/(75u*60u)),(unsigned char)((f/75u)%60u),(unsigned char)(f%75u)};
                    TMSF maxlead = {79,59,74};
                    entry(s,0x50 | (fattr >> 4),0xB0,next,(unsigned char)(s == 1 ? 2 : 1),maxlead);
                    if (s == 1) {
                        TMSF c0p = {97,26,66};
                        entry(s,0x50 | (fattr >> 4),0xC0,none,0,c0p);
                    }
                }
            }
        }
    }
    else {
        LOG(LOG_MISC,LOG_DEBUG)("ATAPI READ TOC Format=%u not supported",Format);
        set_sense(0x05,0x24);
        sector_total = 0;
        return;
    }

    /* update the TOC data length field */
    {
        unsigned int x = (unsigned int)(write-sector) - 2;
        sector[0] = x >> 8;
        sector[1] = x & 0xFF;
    }

    prepare_read(0,MIN(MIN((unsigned int)(write-sector),(unsigned int)host_maximum_byte_count),AllocationLength));
}

/* when the ATAPI command has been accepted, and the timeout has passed */
void IDEATAPICDROMDevice::on_atapi_busy_time() {
    const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();

    /* if the drive is spinning up, then the command waits */
    if (loading_mode == LOAD_DISC_LOADING) {
        switch (atapi_cmd[0]) {
            case 0x03: /* REQUEST SENSE */
            case 0x12: /* INQUIRY */
            case 0x1A: /* MODE SENSE(6) */
            case 0x5A: /* MODE SENSE(10) */
            case 0x1B: /* START/STOP UNIT */
            case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
            case 0xBD: /* MECHANISM STATUS */
            case 0x15: /* MODE SELECT(6) */
            case 0x55: /* MODE SELECT(10) */
                break; /* do not delay */
            default:
                PIC_RemoveSpecificEvents(IDE_DelayedCommand,pk);
                PIC_AddEvent(IDE_DelayedCommand,instant ? 1 : 100/*ms*/,pk);
                return;
        }
    }
    else if (loading_mode == LOAD_DISC_READIED) {
        switch (atapi_cmd[0]) {
            case 0x03: /* REQUEST SENSE */
            case 0x12: /* INQUIRY */
            case 0x1A: /* MODE SENSE(6) */
            case 0x5A: /* MODE SENSE(10) */
            case 0x1B: /* START/STOP UNIT */
            case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
            case 0xBD: /* MECHANISM STATUS */
            case 0x15: /* MODE SELECT(6) */
            case 0x55: /* MODE SELECT(10) */
                break; /* do not delay */
            default:
                if (!common_spinup_response(/*spin up*/true,/*wait*/false)) {
                    atapi_fail();
                    return;
                }
                break;
        }
    }

    switch (atapi_cmd[0]) {
        case 0x03: /* REQUEST SENSE */
            request_sense();
            break;
        case 0x12: /* INQUIRY */
            inquiry();
            break;
        case 0x1B: /* START/STOP UNIT */
            start_stop_unit();
            break;
        case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
            prevent_allow();
            break;
        case 0x25: /* READ CAPACITY */
            read_capacity();
            break;
        case 0x2B: /* SEEK */
            /* Windows 95's CD player expects the SEEK command to interrupt CD audio playback.
             * In fact it depends on it to the exclusion of commands explicitly standardized to... you know...
             * stop or pause playback. Oh Microsoft, you twits... */
            {
                CDROM_Interface *cdrom = getMSCDEXDrive();
                if (cdrom) {
                    bool playing,pause;

                    if (!cdrom->GetAudioStatus(playing,pause))
                        playing = true;

                    if (playing) {
                        LOG(LOG_MISC,LOG_DEBUG)("ATAPI: Interrupting CD audio playback due to SEEK");
                        cdrom->StopAudio();
                    }
                }
            }
            atapi_done();
            break;
        case 0x28: /* READ(10) */
        case 0xA8: /* READ(12) */
            {
                CDROM_Interface *cdrom = getMSCDEXDrive();
                bool res = (cdrom != NULL ? cdrom->ReadSectorsHost(/*buffer*/sector,false,LBA,TransferLength) : false);
                if (res) {
                    LBAnext = LBA + TransferLength;
                    atapi_data_in(TransferLength*2048);
                }
                else {
                    if (cdrom == NULL || cdrom->class_id == CDROM_Interface::INTERFACE_TYPE::ID_FAKE) {
                        set_sense(/*SK=*/0x02,/*ASC=*/0x3A,/*ASCQ=*/0x01); /* Not Ready: Medium Not Present */
                    }
                    else {
                        set_sense(/*SK=*/0x03,/*ASC=*/0x11); /* Medium Error: Unrecovered Read Error */
                        LOG(LOG_MISC,LOG_DEBUG)("ATAPI: Failed to read %lu sectors at %lu",
                            (unsigned long)TransferLength,(unsigned long)LBA);
                    }
                    atapi_fail();
                }
            }
            break;
        case 0xBE: /* READ CD */
        case 0xB9: /* READ CD MSF */
            {
                Bitu bytes = 0;
                if (read_cd_sectors(LBA,TransferLength,bytes)) {
                    LBAnext = LBA + TransferLength;
                    atapi_data_in(bytes);
                }
                else {
                    atapi_fail();
                }
            }
            break;
        case 0x42: /* READ SUB-CHANNEL */
            read_subchannel();
            if (sense[2] & 0xF) atapi_fail();
            else atapi_data_in(sector_total);
            break;
        case 0x43: /* READ TOC */
            read_toc();
            if (sense[2] & 0xF) atapi_fail();
            else atapi_data_in(sector_total);
            break;
        case 0x45: /* PLAY AUDIO(10) */
            play_audio10();
            atapi_done();
            break;
        case 0x47: /* PLAY AUDIO MSF */
            play_audio_msf();
            atapi_done();
            break;
        case 0x4B: /* PAUSE/RESUME */
            pause_resume();
            atapi_done();
            break;
        case 0x4E: /* STOP PLAY/SCAN */
            {
                CDROM_Interface *cdrom = getMSCDEXDrive();
                if (cdrom) cdrom->StopAudio();
            }
            atapi_done();
            break;
        case 0x51: /* READ DISC INFORMATION */
            read_disc_information();
            atapi_data_in(sector_total);
            break;
        case 0x52: /* READ TRACK INFORMATION */
            read_track_information();
            if (sense[2] & 0xF) atapi_fail();
            else atapi_data_in(sector_total);
            break;
        case 0x15: /* MODE SELECT(6) */
        case 0x55: /* MODE SELECT(10) */
            /* we need the data written first, will act in I/O completion routine */
            {
                unsigned int x;

                if (atapi_cmd[0] == 0x55) x = ((unsigned int)atapi_cmd[7] << 8u) + atapi_cmd[8];
                else x = atapi_cmd[4];

                /* Windows 95 likes to set 0xFFFF here for whatever reason.
                 * Negotiate it down to a maximum of 512 for sanity's sake */
                if (x > 512) x = 512;
                if (x > host_maximum_byte_count) x = (unsigned int)host_maximum_byte_count;
                if (x == 0) {
                    atapi_done();
                    break;
                }
                lba[2] = x >> 8u;
                lba[1] = x;

                prepare_write(0,(x+1u)&(~1u));
            }

            feature = 0x00;
            count = 0x00; /* data from the host */
            state = IDE_DEV_DATA_WRITE;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            raise_irq();
            allow_writing = true;
            break;
        case 0x1A: /* MODE SENSE(6) */
        case 0x5A: /* MODE SENSE(10) */
            mode_sense();
            if (sense[2] & 0xF) atapi_fail();
            else atapi_data_in(sector_total);
            break;
        case 0xBD: /* MECHANISM STATUS */
            mechanism_status();
            atapi_data_in(sector_total);
            break;
        default:
            LOG_MSG("Unknown ATAPI command after busy wait. Why?\n");
            set_sense(0x05,0x20);
            atapi_fail();
            break;
    }
}

void IDEATAPICDROMDevice::set_sense(unsigned char SK,unsigned char ASC,unsigned char ASCQ,unsigned int len) {
    if (len < 18) len = 18;
    memset(sense,0,len);
    sense_length = len;

    sense[0] = 0x70;    /* RESPONSE CODE */
    sense[2] = SK&0xF;  /* SENSE KEY */
    sense[7] = len - 8;    /* additional sense length */
    sense[12] = ASC;
    sense[13] = ASCQ;
}

/* REQUEST SENSE: the sense data (up to the allocation length), which is then cleared */
void IDEATAPICDROMDevice::request_sense() {
    const unsigned int AllocationLength = atapi_cmd[4];
    const unsigned int len = MIN((unsigned int)sense_length,AllocationLength);

    memcpy(sector,sense,sense_length);
    set_sense(0);
    atapi_data_in(len);
}

/* INQUIRY: standard data, or vital product data pages 00h (supported pages) and 80h (serial number) */
void IDEATAPICDROMDevice::inquiry() {
    const unsigned int AllocationLength = ((unsigned int)atapi_cmd[3] << 8u) + atapi_cmd[4];
    unsigned int len;

    if (atapi_cmd[1] & 1) {
        memset(sector,0,64);
        sector[0] = 0x05;
        sector[1] = atapi_cmd[2];
        if (atapi_cmd[2] == 0x00) {
            sector[3] = 2;
            sector[4] = 0x00;
            sector[5] = 0x80;
            len = 6;
        }
        else if (atapi_cmd[2] == 0x80) {
            sector[3] = (unsigned char)id_serial.length();
            memcpy(sector + 4,id_serial.c_str(),id_serial.length());
            len = 4u + (unsigned int)id_serial.length();
        }
        else {
            set_sense(0x05,0x24); /* Invalid Field in CDB */
            atapi_fail();
            return;
        }
    }
    else {
        if (atapi_cmd[2] != 0) {
            set_sense(0x05,0x24);
            atapi_fail();
            return;
        }
        generate_mmc_inquiry();
        len = 36;
    }

    atapi_data_in(MIN(len,AllocationLength));
}

void IDEATAPICDROMDevice::read_capacity() {
    const unsigned int secsize = 2048;
    CDROM_Interface *cdrom = getMSCDEXDrive();
    const Bitu cap = atapi_capacity(cdrom);

    /* the address of the last sector */
    const uint32_t sec = cap > 0 ? (uint32_t)(cap - 1u) : 0u;

    sector[0] = sec >> 24u;
    sector[1] = sec >> 16u;
    sector[2] = sec >> 8u;
    sector[3] = sec & 0xFF;
    sector[4] = secsize >> 24u;
    sector[5] = secsize >> 16u;
    sector[6] = secsize >> 8u;
    sector[7] = secsize & 0xFF;
    atapi_data_in(8);
}

/* START STOP UNIT: spin up/down, load/eject, power condition. The time it takes (tray, spin up)
 * passed before this is called, unless IMMED was set. */
void IDEATAPICDROMDevice::start_stop_unit() {
    const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();
    const bool START = !!(atapi_cmd[4] & 0x01);
    const bool LOEJ = !!(atapi_cmd[4] & 0x02);
    const unsigned char POWER = atapi_cmd[4] >> 4u;
    CDROM_Interface *cdrom = getMSCDEXDrive();

    if (POWER != 0) {
        /* power condition: 1 active, 2 idle, 3 standby */
        if (POWER >= 1 && POWER <= 3) {
            power_state = POWER;
            power_event = 0x02; /* power change successful */
            if (POWER == 3 && (loading_mode == LOAD_READY || loading_mode == LOAD_DISC_READIED)) {
                loading_mode = LOAD_IDLE;
                rb_valid = false;
            }
        }
        atapi_done();
        return;
    }

    if (LOEJ) {
        LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM drive is being asked to %s the media",START ? "LOAD" : "EJECT");

        if (!START) {
            /* the tray opens: the disc is out. Treated as unmounting the drive (an "empty" CD-ROM image) */
            if (cdrom) cdrom->StopAudio();
            LOG(LOG_MISC,LOG_DEBUG)("Interpreting EJECT as a command to unmount the drive (replace with empty image)");
            if (mscdex) IDE_Eject_unmount(drive_index+'A');
            else IDE_CDROM_Eject(controller->interface_index,slave);
            PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
            PIC_RemoveSpecificEvents(IDE_ATAPI_SpinUpComplete,pk);
            PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,pk);
            loading_mode = LOAD_NO_DISC;
            tray_open = true;
            rb_valid = false;
            post_media_event(0x03/*MediaRemoval*/);
        }
        else {
            /* the tray closes, and the disc (if any) spins up */
            tray_open = false;
            cdrom = getMSCDEXDrive();
            if (cdrom != NULL && cdrom->class_id != CDROM_Interface::INTERFACE_TYPE::ID_FAKE &&
                (loading_mode == LOAD_NO_DISC || loading_mode == LOAD_IDLE)) {
                has_changed = true;
                loading_mode = LOAD_DISC_LOADING;
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinUpComplete,pk);
                PIC_AddEvent(IDE_ATAPI_SpinUpComplete,instant ? 0.01 : spinup_time/*ms*/,pk);
            }
        }
    }
    else if (START) {
        LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM drive is being asked to START the media");
        /* the disc spins up (the time it takes passed already, unless IMMED) */
        if (loading_mode == LOAD_IDLE) {
            if (atapi_cmd[1] & 1) {
                loading_mode = LOAD_DISC_LOADING;
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinUpComplete,pk);
                PIC_AddEvent(IDE_ATAPI_SpinUpComplete,instant ? 0.01 : spinup_time/*ms*/,pk);
            }
            else {
                loading_mode = LOAD_READY;
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
                PIC_AddEvent(IDE_ATAPI_SpinDown,spindown_timeout/*ms*/,pk);
            }
        }
        power_state = 1;
    }
    else {
        LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM drive is being asked to STOP the media");
        if (cdrom) cdrom->StopAudio();
        if (loading_mode == LOAD_READY || loading_mode == LOAD_DISC_READIED) {
            loading_mode = LOAD_IDLE;
            rb_valid = false;
            PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
        }
    }

    atapi_done();
}

/* PREVENT ALLOW MEDIUM REMOVAL: lock or unlock the tray */
void IDEATAPICDROMDevice::prevent_allow() {
    const uint8_t PREVENT = atapi_cmd[4] & 3;

    locked = (PREVENT & 1) != 0;
    LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM PREVENT/ALLOW MEDIA REMOVAL: prevent=%u locked=%u",PREVENT,locked);
    atapi_done();
}

/* GET EVENT STATUS NOTIFICATION (polled): operational change, power management and media events */
void IDEATAPICDROMDevice::get_event_status_notification() {
    const unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8u) + atapi_cmd[8];
    const uint8_t request = atapi_cmd[4];
    const uint8_t supported = 0x16; /* operational change, power management, media */
    unsigned int len = 4;

    if (!(atapi_cmd[1] & 1)) {
        /* asynchronous notification is not supported */
        set_sense(0x05,0x24);
        atapi_fail();
        return;
    }

    memset(sector,0,16);
    sector[3] = supported;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    const bool present = !tray_open && cdrom != NULL && cdrom->class_id != CDROM_Interface::INTERFACE_TYPE::ID_FAKE &&
        loading_mode != LOAD_NO_DISC && loading_mode != LOAD_INSERT_CD;

    /* the class with a pending event first, otherwise "no change" of the first class asked for */
    uint8_t cls = 0;
    if ((request & 0x04) && power_event != 0) cls = 2;
    else if ((request & 0x10) && media_event != 0) cls = 4;
    else if (request & 0x02) cls = 1;
    else if (request & 0x04) cls = 2;
    else if (request & 0x10) cls = 4;

    if (cls == 0) {
        sector[2] = 0x80; /* NEA: no event available */
    }
    else {
        sector[2] = cls;
        len = 8;
        if (cls == 1) {
            /* operational change: none */
        }
        else if (cls == 2) {
            sector[4] = power_event;
            sector[5] = power_state;
            power_event = 0;
        }
        else if (cls == 4) {
            sector[4] = media_event;
            sector[5] = (uint8_t)((present ? 0x02 : 0x00) | (tray_open ? 0x01 : 0x00));
            media_event = 0;
        }
    }

    sector[0] = 0;
    sector[1] = (uint8_t)(len - 2u);
    atapi_data_in(MIN(len,AllocationLength));
}

/* GET CONFIGURATION: a CD-ROM drive (profile 0008h) */
void IDEATAPICDROMDevice::get_configuration() {
    const unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8u) + atapi_cmd[8];
    const unsigned int RT = atapi_cmd[1] & 3u;
    const unsigned int start = ((unsigned int)atapi_cmd[2] << 8u) + atapi_cmd[3];
    CDROM_Interface *cdrom = getMSCDEXDrive();
    const bool present = !tray_open && cdrom != NULL && cdrom->class_id != CDROM_Interface::INTERFACE_TYPE::ID_FAKE;
    unsigned char *w = sector + 8;

    memset(sector,0,256);

    auto feature_desc = [&](unsigned int code,unsigned int version,bool persistent,bool current,const unsigned char *data,unsigned int len) {
        if (RT == 2 && code != start) return;
        if (code < start) return;
        if (RT == 1 && !current) return;
        w[0] = (unsigned char)(code >> 8u);
        w[1] = (unsigned char)code;
        w[2] = (unsigned char)((version << 2u) | (persistent ? 2u : 0u) | (current ? 1u : 0u));
        w[3] = (unsigned char)len;
        memcpy(w + 4,data,len);
        w += 4u + len;
    };

    const unsigned char profiles[4] = { 0x00, 0x08, (unsigned char)(present ? 0x01 : 0x00), 0x00 };
    const unsigned char core[4] = { 0x00, 0x00, 0x00, 0x02 };              /* ATAPI */
    const unsigned char morphing[4] = { 0x00, 0x00, 0x00, 0x00 };
    const unsigned char removable[4] = { 0x29, 0x00, 0x00, 0x00 };         /* tray, eject, lock */
    const unsigned char random_read[8] = { 0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00 }; /* 2048 bytes/block */
    const unsigned char cd_read[4] = { 0x02, 0x00, 0x00, 0x00 };           /* C2 error pointers */
    const unsigned char cd_audio[4] = { 0x03, 0x00, 0x01, 0x00 };          /* separate volume and mute, 256 levels */

    feature_desc(0x0000,0,true,true,profiles,4);
    feature_desc(0x0001,0,true,true,core,4);
    feature_desc(0x0002,0,true,true,morphing,4);
    feature_desc(0x0003,0,true,true,removable,4);
    feature_desc(0x0010,0,false,present,random_read,8);
    feature_desc(0x001E,0,false,present,cd_read,4);
    feature_desc(0x0100,0,true,true,NULL,0);
    feature_desc(0x0103,0,false,present,cd_audio,4);

    const unsigned int len = (unsigned int)(w - sector);
    sector[0] = (unsigned char)((len - 4u) >> 24u);
    sector[1] = (unsigned char)((len - 4u) >> 16u);
    sector[2] = (unsigned char)((len - 4u) >> 8u);
    sector[3] = (unsigned char)(len - 4u);
    sector[6] = 0x00;
    sector[7] = present ? 0x08 : 0x00; /* current profile */
    atapi_data_in(MIN(len,AllocationLength));
}

/* READ DISC INFORMATION: a complete (finalized) disc */
void IDEATAPICDROMDevice::read_disc_information() {
    const unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8u) + atapi_cmd[8];
    CDROM_Interface *cdrom = getMSCDEXDrive();
    int first = 1,last = 1,sessions = 1,sfirst = 1,slast = 1;
    TMSF leadOut,slead;
    unsigned char dtype = 0;

    memset(sector,0,34);
    if (cdrom != NULL && cdrom->GetAudioTracks(first,last,leadOut)) {
        sessions = cdrom->GetSessionCount();
        if (!cdrom->GetSessionInfo(sessions,sfirst,slast,slead,dtype)) {
            sfirst = first;
            slast = last;
        }
    }

    sector[1] = 32;
    sector[2] = 0x0E;                       /* last session complete, disc complete */
    sector[3] = (unsigned char)first;
    sector[4] = (unsigned char)sessions;
    sector[5] = (unsigned char)sfirst;
    sector[6] = (unsigned char)slast;
    sector[7] = 0x20;                       /* unrestricted use */
    sector[8] = dtype;                      /* CD-DA/CD-ROM or CD-ROM XA */
    memset(sector + 16,0xFF,8);             /* no next session lead-in / last possible lead-out */
    atapi_data_in(MIN(34u,AllocationLength));
}

/* READ TRACK INFORMATION: address type 0 = LBA, 1 = track, 2 = session */
void IDEATAPICDROMDevice::read_track_information() {
    const unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8u) + atapi_cmd[8];
    const unsigned int type = atapi_cmd[1] & 3u;
    const uint32_t value = ((uint32_t)atapi_cmd[2] << 24u) | ((uint32_t)atapi_cmd[3] << 16u) | ((uint32_t)atapi_cmd[4] << 8u) | atapi_cmd[5];
    CDROM_Interface *cdrom = getMSCDEXDrive();
    int first,last,track = -1;
    TMSF leadOut;

    if (cdrom == NULL || !cdrom->GetAudioTracks(first,last,leadOut)) {
        set_sense(0x02,0x3A,0x01);
        return;
    }

    auto track_start = [&](int t) -> Bitu {
        TMSF s;
        unsigned char a;
        if (t > last) return atapi_capacity(cdrom);
        if (!cdrom->GetAudioTrackInfo(t,s,a)) return 0;
        const Bitu f = ((Bitu)s.min * 60u + s.sec) * 75u + s.fr;
        return f >= 150u ? f - 150u : 0u;
    };

    if (type == 0) {
        for (int t=first;t <= last;t++) if (value >= track_start(t)) track = t;
    }
    else if (type == 1) {
        if ((int)value >= first && (int)value <= last) track = (int)value;
    }
    else if (type == 2) {
        int sf,sl;
        TMSF sld;
        unsigned char dt;
        if (cdrom->GetSessionInfo((int)value,sf,sl,sld,dt)) track = sf;
    }

    if (track < 0) {
        set_sense(0x05,0x24);
        return;
    }

    TMSF s;
    unsigned char attr = 0;
    cdrom->GetAudioTrackInfo(track,s,attr);
    const Bitu tstart = track_start(track);
    const Bitu tsize = track_start(track + 1) - tstart;
    const int mode = cdrom->GetSectorMode((unsigned long)tstart);

    memset(sector,0,36);
    sector[1] = 34;
    sector[2] = (unsigned char)track;
    sector[3] = 1;                                      /* session */
    sector[5] = (unsigned char)(attr >> 4u);            /* track mode (control) */
    sector[6] = (unsigned char)(mode == 1 ? 0x01 : (mode == 2 ? 0x02 : 0x0F)); /* data mode */
    host_writed(sector + 8,0); sector[8] = (unsigned char)(tstart >> 24u); sector[9] = (unsigned char)(tstart >> 16u);
    sector[10] = (unsigned char)(tstart >> 8u); sector[11] = (unsigned char)tstart;
    sector[24] = (unsigned char)(tsize >> 24u); sector[25] = (unsigned char)(tsize >> 16u);
    sector[26] = (unsigned char)(tsize >> 8u); sector[27] = (unsigned char)tsize;
    prepare_read(0,MIN(36u,AllocationLength));
}

/* SET CD SPEED: read speed in kB/s (FFFFh = as fast as possible), up to the drive's maximum */
void IDEATAPICDROMDevice::set_cd_speed() {
    const unsigned int kbps = ((unsigned int)atapi_cmd[2] << 8u) + atapi_cmd[3];
    unsigned int x = kbps == 0xFFFFu ? max_speed : (kbps + 88u) / 176u;

    if (x < 1) x = 1;
    if (x > max_speed) x = max_speed;
    cur_speed = x;
    LOG(LOG_MISC,LOG_DEBUG)("ATAPI CD-ROM: read speed set to %ux",cur_speed);
    atapi_done();
}

IDEATAPICDROMDevice::IDEATAPICDROMDevice(IDEController *c,unsigned char drive_index,bool _slave,CDROM_Interface *n_cdrom) : IDEDevice(c,_slave) {
    this->drive_index = drive_index;
    sector_i = sector_total = 0;
    atapi_to_host = false;
    host_maximum_byte_count = 0;
    LBA = 0;
    LBAnext = 0;
    TransferLength = 0;
    TransferLengthRemaining = 0;
    memset(atapi_cmd, 0, sizeof(atapi_cmd));
    atapi_cmd_i = 0;
    atapi_cmd_total = 0;
    memset(sector, 0, sizeof(sector));

    if (n_cdrom) (cdrom = n_cdrom)->Addref();

    memset(sense,0,sizeof(sense));
    IDEATAPICDROMDevice::set_sense(/*SK=*/0);

    cd_insertion_time = 4000; /* a quick user that can switch CDs in 4 seconds */
    if (c->cd_insertion_time > 0) cd_insertion_time = c->cd_insertion_time;

    spinup_time = 1000; /* drive takes 1 second to spin up from idle */
    if (c->spinup_time > 0) spinup_time = c->spinup_time;

    spindown_timeout = 10000; /* drive spins down automatically after 10 seconds */
    if (c->spindown_timeout > 0) spindown_timeout = c->spindown_timeout;

    loading_mode = LOAD_IDLE;
    has_changed = false;
    mech_init();

    type = IDE_TYPE_CDROM;
    id_serial = "123456789";
    id_firmware_rev = "0.84-X";
    id_model = "DOSBox-X ATAPI CD-ROM";

    /* INQUIRY strings */
    if (IS_PC98_ARCH) {
        /* The OAK IDE CD-ROM driver that most PC-98 HDI game images use?
         * It will NOT talk to this emulation unless we report ourself as an NEC CD-ROM drive.
         * Yes, really. It will issue an INQUIRY and then do a REP CMPSW on the first 20 bytes
         * to make sure it says "NEC CD-ROM DRIVE". */
        id_mmc_vendor_id = "NEC";
        id_mmc_product_id = "CD-ROM DRIVE";
        id_mmc_product_rev = "FAKE";
        /* Second thing: That particular driver seems to assume the CD-ROM drive only transfers
         * one CD-ROM sector per DRQ (like many CD-ROM drives in the 1990s did because they had
         * small transfer buffers).
         *
         * We HAVE to emulate this behavior or else the PC-98 driver will stall and error out
         * if any attempt is made to read more than one sector at a time! */
        sector_transfer_limit = 1;
    }
    else {
        id_mmc_vendor_id = "DOSBox-X";
        id_mmc_product_id = "ATAPI CD-ROM";
        id_mmc_product_rev = "0.84-X";
    }
}

IDEATAPICDROMDevice::~IDEATAPICDROMDevice() {
    for (auto &cd : cdrom_swaplist) {
        cd->Release();
        cd = NULL;
    }
    cdrom_swaplist.clear();
    cdrom_swaplist_pos = 0;
    if (cdrom) {
        cdrom->Release();
        cdrom = NULL;
    }
}

void IDEATAPICDROMDevice::host_reset_complete() {
    IDEDevice::host_reset_complete();
    if (power_state == 4) power_state = 3; /* a reset wakes the drive from sleep */
}

void IDEATAPICDROMDevice::set_signature() {
    count = 0x01;
    lba[0] = 0x01;
    lba[1] = 0x14;  /* <- magic ATAPI identification */
    lba[2] = 0xEB;
    drivehead &= 0x10;
}

void IDEATAPICDROMDevice::on_mode_select_io_complete() {
    const bool ten = (atapi_cmd[0] == 0x55);
    unsigned int AllocationLength = ten ? (((unsigned int)atapi_cmd[7] << 8) + atapi_cmd[8]) : atapi_cmd[4];
    unsigned char *scan,*fence;

    /* the mode parameter header (8 bytes for MODE SELECT(10), 4 bytes for (6)) is followed by
     * the block descriptors and the mode pages. Windows 95 seems to send all zeros in the
     * header, so ignore it. Nothing in the mode pages is changeable, they are only logged. */

    scan = sector + (ten ? 8 : 4);
    fence = sector + MIN((unsigned int)sector_total,(unsigned int)AllocationLength);

    while ((scan+2) < fence) {
        unsigned char PAGE = *scan++;
        unsigned int LEN = (unsigned int)(*scan++);

        if ((scan+LEN) > fence) {
            LOG(LOG_MISC,LOG_DEBUG)("ATAPI MODE SELECT warning, page_0 length extends %u bytes past buffer",(unsigned int)(scan+LEN-fence));
            break;
        }

        LOG(LOG_MISC,LOG_DEBUG)("ATAPI MODE SELECT, PAGE 0x%02x len=%u",PAGE,LEN);
        scan += LEN;
    }
}

void IDEATAPICDROMDevice::atapi_io_completion() {
    /* for most ATAPI PACKET commands, the transfer is done and we need to clear
       all indication of a possible data transfer */

    if (count != 0x03) { /* the command was expecting data. now it can act on it */
        switch (atapi_cmd[0]) {
            case 0x28:/*READ(10)*/
            case 0xA8:/*READ(12)*/
            case 0xBE:/*READ CD*/
            case 0xB9:/*READ CD MSF*/
                /* more sectors to transfer: the next DRQ data block, when the sectors have been read */
                if (TransferLengthRemaining != 0) {
                    LBA = LBAnext;
                    TransferLength = TransferLengthRemaining;
                    if (TransferLength > sector_transfer_limit)
                        TransferLength = sector_transfer_limit;
                    {
                        const Bitu per = (atapi_cmd[0] == 0x28 || atapi_cmd[0] == 0xA8) ? 2048u : IDE_CD_MAX_READCD_SECTOR;
                        Bitu fit = MIN((Bitu)sizeof(sector),(Bitu)host_maximum_byte_count) / per;
                        if (fit == 0) fit = 1;
                        if (TransferLength > fit) TransferLength = fit;
                    }
                    TransferLengthRemaining -= TransferLength;

                    count = 0x02;
                    state = IDE_DEV_ATAPI_BUSY;
                    status = IDE_STATUS_BUSY;
                    schedule(cd_access(LBA,TransferLength,PIC_FullIndex()));
                    return;
                }
                break;

            case 0x15: /* MODE SELECT(6) */
            case 0x55: /* MODE SELECT(10) */
                on_mode_select_io_complete();
                break;
        }
    }

    count = 0x03; /* no more data (command/data=1, input/output=1) */
    status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
    state = IDE_DEV_READY;
    allow_writing = true;

    /* Apparently: real IDE ATAPI controllers fire another IRQ after the transfer.
       And there are MS-DOS CD-ROM drivers that assume that. */
    raise_irq();
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
            count = 0x03; /* no more data (command/data=1, input/output=1) */
	    break;
    }
}

/* ------------------------------------------------------------------------------------------
 * ATA hard disk: drive mechanics
 *
 * The disk turns at "hard disk rpm". The media data rate is the "hard drive data rate limit"
 * setting, which decides how many sectors pass under the head per revolution. Seeks take from
 * about 1.5ms (next cylinder) to about 18ms (full stroke), then the drive waits for the sector to
 * come around. After a read the drive goes on reading ahead into its buffer, so that sequential
 * reads do not have to wait a revolution. Writes go to the write cache (unless disabled with SET
 * FEATURES) and are written behind, the next access waits for that. A disk that is spun down
 * (STANDBY, SLEEP, or the standby timer) spins up again on the next access.
 * ------------------------------------------------------------------------------------------ */

extern int disk_data_rate;

#define IDE_HD_READAHEAD_SECTORS        256u    /* read-ahead buffer */
#define IDE_HD_WRITE_CACHE_SECTORS      512u    /* write cache */
#define IDE_HD_TRACK_TO_TRACK_MS        1.5
#define IDE_HD_FULL_STROKE_MS           18.0
#define IDE_HD_COMMAND_MS               0.05    /* command overhead */

static void IDE_ATA_StandbyTimer(Bitu pk);

void IDEATADevice::mech_init() {
    instant = controller->hd_instant;
    rpm = controller->hd_rpm > 0 ? (double)controller->hd_rpm : 5400.0;
    spinup_ms = controller->hd_spinup_time > 0 ? (double)controller->hd_spinup_time : 4000.0;
    rev_ms = 60000.0 / rpm;

    if (disk_data_rate > 0) {
        double spt = rev_ms / (512.0 * 1000.0 / (double)disk_data_rate);
        if (spt < 17) spt = 17;
        if (spt > 2000) spt = 2000;
        media_spt = (unsigned int)spt;
        sector_ms = rev_ms / media_spt;
    }
    else {
        /* no data rate limit: the data comes off the disk as fast as it is asked for */
        media_spt = sects != 0 ? (unsigned int)sects : 63u;
        sector_ms = 0;
    }

    media_heads = 4;
    media_cyls = LBA / ((uint64_t)media_spt * media_heads) + 1u;
    spinning = true;
    spin_origin = 0;
    spin_ready_at = 0;
    power = PWR_ACTIVE;
    ra_valid = false;
    mech_free_at = 0;
    head_cyl = 0;
}

/* spin up the disk if it is stopped, returns when it is up to speed */
double IDEATADevice::spin_up(double t) {
    if (instant) {
        spinning = true;
        if (power != PWR_IDLE) power = PWR_ACTIVE;
        return t;
    }

    if (!spinning) {
        LOG(LOG_MISC,LOG_DEBUG)("IDE ATA: spinning up");
        spinning = true;
        spin_ready_at = t + spinup_ms;
        spin_origin = spin_ready_at;
    }
    if (power != PWR_IDLE) power = PWR_ACTIVE;
    if (t < spin_ready_at) t = spin_ready_at;
    return t;
}

/* time to move the heads from the cylinder they are on to another */
double IDEATADevice::seek_ms(uint64_t to_cyl) const {
    const uint64_t d = to_cyl > head_cyl ? to_cyl - head_cyl : head_cyl - to_cyl;
    if (d == 0) return 0;
    return IDE_HD_TRACK_TO_TRACK_MS + (IDE_HD_FULL_STROKE_MS - IDE_HD_TRACK_TO_TRACK_MS) * sqrt((double)(d - 1u) / (double)media_cyls);
}

void IDEATADevice::spin_down(double t,bool sleep) {
    (void)t;
    if (spinning) LOG(LOG_MISC,LOG_DEBUG)("IDE ATA: spinning down%s",sleep ? " (sleep)" : "");
    spinning = false;
    ra_valid = false;
    power = sleep ? PWR_SLEEP : PWR_STANDBY;
}

/* standby timer: the disk spins down after this long without access */
void IDEATADevice::set_standby_timer(uint8_t v) {
    if (v == 0) standby_timer_ms = 0;
    else if (v <= 240) standby_timer_ms = v * 5000.0;
    else if (v <= 251) standby_timer_ms = (v - 240) * 30.0 * 60000.0;
    else if (v == 252) standby_timer_ms = 21.0 * 60000.0;
    else if (v == 253) standby_timer_ms = 8.0 * 3600000.0;
    else if (v == 255) standby_timer_ms = 21.0 * 60000.0 + 15000.0;
    else standby_timer_ms = 0;
    note_activity(PIC_FullIndex());
}

void IDEATADevice::note_activity(double t) {
    const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();

    if (t > last_activity) last_activity = t;
    PIC_RemoveSpecificEvents(IDE_ATA_StandbyTimer,pk);
    if (standby_timer_ms > 0 && spinning) {
        double d = last_activity + standby_timer_ms - PIC_FullIndex();
        if (d < 1) d = 1;
        PIC_AddEvent(IDE_ATA_StandbyTimer,d,pk);
    }
}

/* time at which n sectors from addr have been read off (or written to) the disk, the access
 * starting at time t. Updates the head position and the read-ahead buffer. */
double IDEATADevice::media_access(uint64_t addr,unsigned int n,bool write,double t) {
    if (instant || faked_command) {
        /* no delays, but an access still spins the disk up */
        if (!spinning) {
            spinning = true;
            spin_ready_at = t;
        }
        if (power != PWR_IDLE) power = PWR_ACTIVE;
        return t;
    }

    t = spin_up(t);

    /* sectors in the read-ahead buffer, or being read into it now */
    if (!write && ra_valid && addr >= ra_first && addr + n <= ra_limit) {
        double avail = ra_time + ((double)(addr + n) - (double)ra_next) * sector_ms;
        if (avail < t) avail = t;
        /* the drive keeps reading ahead as the buffer is emptied */
        if (addr + n + IDE_HD_READAHEAD_SECTORS > ra_limit) ra_limit = addr + n + IDE_HD_READAHEAD_SECTORS;
        note_activity(avail);
        return avail;
    }

    /* the mechanism finishes writing what is in the write cache first */
    if (t < mech_free_at) t = mech_free_at;

    t += seek_ms((addr / media_spt) / media_heads);

    /* wait for the sector to come around */
    double a = fmod((t - spin_origin) / rev_ms,1.0);
    if (a < 0) a += 1.0;
    double w = (double)(addr % media_spt) / media_spt - a;
    if (w < 0) w += 1.0;
    t += w * rev_ms;

    /* the sectors pass under the head */
    t += n * sector_ms;
    head_cyl = ((addr + n - 1u) / media_spt) / media_heads;

    if (!write && read_lookahead) {
        ra_valid = true;
        ra_first = addr;
        ra_next = addr + n;
        ra_time = t;
        ra_limit = addr + n + IDE_HD_READAHEAD_SECTORS;
    }
    else {
        ra_valid = false;
    }

    note_activity(t);
    return t;
}

/* A BIOS disk call (INT 13h, PC-98 INT 1Bh) carried out with ATA commands: returns the time at which
 * the BIOS would return. Reads return when the last sector has come off the disk (or out of the read-ahead
 * buffer), writes when the data is in the write cache (or on the disk, without it). */
double IDEATADevice::bios_access(uint64_t addr,unsigned int n,IDEBiosDiskOp op,double now) {
    const double t = now + (instant ? 0 : IDE_HD_COMMAND_MS);

    if (n == 0) n = 1;
    if (LBA != 0 && addr >= LBA) addr = LBA - 1u; /* the drive still seeks before it reports IDNF */
    if (addr + n > LBA && LBA != 0) n = (unsigned int)(LBA - addr);

    switch (op) {
        case IDEBiosDiskOp::Read:
        case IDEBiosDiskOp::Verify:
            return media_access(addr,n,false,t);
        case IDEBiosDiskOp::Write: {
            const double written = media_access(addr,n,true,t);
            if (!write_cache || instant) return written;

            /* write behind: the mechanism is busy writing the cache until "written" */
            mech_free_at = written;
            const double done = written - IDE_HD_WRITE_CACHE_SECTORS * sector_ms;
            return done > t ? done : t;
        }
        case IDEBiosDiskOp::Seek:
        case IDEBiosDiskOp::Recalibrate: {
            const uint64_t cyl = op == IDEBiosDiskOp::Seek ? (addr / media_spt) / media_heads : 0u;
            double r = spin_up(t);
            if (!instant) {
                if (r < mech_free_at) r = mech_free_at;
                r += seek_ms(cyl);
            }
            head_cyl = cyl;
            if (op == IDEBiosDiskOp::Recalibrate) ra_valid = false;
            note_activity(r);
            return r;
        }
    }

    return t;
}

void IDEATADevice::schedule(double at_ms) {
    const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();
    double d = at_ms - PIC_FullIndex();

    if (faked_command) d = 0.000001;
    else if (d < 0.00001) d = 0.00001;

    PIC_RemoveSpecificEvents(IDE_DelayedCommand,pk);
    PIC_AddEvent(IDE_DelayedCommand,d,pk);
}

static void IDE_ATA_StandbyTimer(Bitu pk) {
    IDEEventPack ep(pk);
    IDEController *ctrl = GetIDEController(ep.interface());
    if (ctrl == NULL) return;

    IDEDevice *dev = ctrl->device[ep.device()];
    if (dev == NULL || dev->type != IDE_TYPE_HDD) return;

    IDEATADevice *ata = (IDEATADevice*)dev;
    if (ata->state != IDE_DEV_READY || !ata->spinning) return;
    LOG(LOG_MISC,LOG_DEBUG)("IDE ATA: standby timer expired");
    ata->spin_down(PIC_FullIndex(),false);
}

/* ------------------------------------------------------------------------------------------
 * ATA hard disk: command execution
 * ------------------------------------------------------------------------------------------ */

bool IDEATADevice::command_is_ext() {
    switch (command) {
        case 0x24: case 0x29: case 0x34: case 0x39: case 0x42:
            return true;
    }
    return false;
}

bool IDEATADevice::command_is_multiple() {
    return command == 0xC4 || command == 0xC5 || command == 0x29 || command == 0x39;
}

bool IDEATADevice::command_is_write() {
    return command == 0x30 || command == 0x31 || command == 0x34 || command == 0xC5 || command == 0x39;
}

/* take the address and sector count of the command from the registers */
bool IDEATADevice::decode_address(bool ext,bool check) {
    cmd_ext = ext;

    if (ext) {
        /* LBA48: the previous register contents are the high order bytes */
        if (!int13_enable_48bitLBA || !(drivehead & 0x40)) return false;
        cmd_lba = true;
        cur_lba = ((uint64_t)(lba[0] & 0xFFu)) | ((uint64_t)(lba[1] & 0xFFu) << 8u) | ((uint64_t)(lba[2] & 0xFFu) << 16u) |
            ((uint64_t)hob_lba[0] << 24u) | ((uint64_t)hob_lba[1] << 32u) | ((uint64_t)hob_lba[2] << 40u);
        cur_count = ((uint32_t)hob_count << 8u) | (count & 0xFFu);
        if (cur_count == 0) cur_count = 65536;
    }
    else {
        cur_count = count & 0xFFu;
        if (cur_count == 0) cur_count = 256;

        if (drivehead & 0x40) {
            cmd_lba = true;
            cur_lba = ((uint64_t)(drivehead & 0xFu) << 24u) | (lba[0] & 0xFFu) |
                ((uint64_t)(lba[1] & 0xFFu) << 8u) | ((uint64_t)(lba[2] & 0xFFu) << 16u);
        }
        else {
            const unsigned int s = lba[0] & 0xFFu,h = drivehead & 0xFu,c = ide_chs_cylinder(lba);

            cmd_lba = false;
            if (s == 0 || s > sects || h >= heads || c >= cyls) {
                LOG(LOG_MISC,LOG_DEBUG)("IDE ATA: C/H/S %u/%u/%u out of bounds %u/%u/%u",
                    c,h,s,(unsigned int)cyls,(unsigned int)heads,(unsigned int)sects);
                return false;
            }
            cur_lba = ((uint64_t)c * heads + h) * sects + (s - 1u);
        }
    }

    if (check && cur_lba >= LBA) return false;
    return true;
}

/* the address registers show this sector, in the addressing mode of the command */
void IDEATADevice::update_address_registers(uint64_t addr) {
    if (cmd_ext) {
        lba[0] = addr & 0xFFu;
        lba[1] = (addr >> 8u) & 0xFFu;
        lba[2] = (addr >> 16u) & 0xFFu;
        hob_lba[0] = (uint8_t)(addr >> 24u);
        hob_lba[1] = (uint8_t)(addr >> 32u);
        hob_lba[2] = (uint8_t)(addr >> 40u);
    }
    else if (cmd_lba) {
        lba[0] = addr & 0xFFu;
        lba[1] = (addr >> 8u) & 0xFFu;
        lba[2] = (addr >> 16u) & 0xFFu;
        drivehead = (uint8_t)((drivehead & 0xF0u) | ((addr >> 24u) & 0xFu));
    }
    else if (sects != 0 && heads != 0) {
        const uint64_t c = addr / (sects * heads);
        lba[0] = (uint16_t)((addr % sects) + 1u);
        lba[1] = c & 0xFFu;
        lba[2] = (c >> 8u) & 0xFFu;
        drivehead = (uint8_t)((drivehead & 0xF0u) | ((addr / sects) % heads));
    }
}

/* sectors in the next DRQ data block */
unsigned int IDEATADevice::block_count() {
    uint64_t n = 1;

    if (command_is_multiple()) n = multiple_sector_count;
    if (command == 0x40 || command == 0x41 || command == 0x42) n = sizeof(sector) / 512u; /* verify: no data transfer */
    if (n > cur_count) n = cur_count;
    if (n > sizeof(sector) / 512u) n = sizeof(sector) / 512u;
    if (n == 0) n = 1;
    return (unsigned int)n;
}

/* the command ends with an error at sector cur_lba: the registers show where and how many sectors are left */
void IDEATADevice::sector_error(uint8_t err) {
    update_address_registers(cur_lba);
    count = cur_count & 0xFFu;
    if (cmd_ext) hob_count = (uint8_t)(cur_count >> 8u);
    LOG(LOG_MISC,LOG_DEBUG)("IDE ATA: command %02x error %02x at sector %llu",command,err,(unsigned long long)cur_lba);
    abort_with_error(err);
}

/* read cur_block sectors from cur_lba into the sector buffer */
void IDEATADevice::read_block() {
    imageDisk *disk = getBIOSdisk();
    if (disk == NULL) {
        sector_error(0x04/*ABRT*/);
        return;
    }

    for (unsigned int i=0;i < cur_block;i++) {
        if (cur_lba + i >= LBA) {
            cur_lba += i;
            cur_count -= i;
            sector_error(0x10/*IDNF*/);
            return;
        }
        if (disk->Read_AbsoluteSector((uint32_t)(cur_lba + i),sector + (i * 512u)) != Int13Status::NoError) {
            cur_lba += i;
            cur_count -= i;
            sector_error(0x40/*UNC*/);
            return;
        }
    }

    if (command == 0x40 || command == 0x41 || command == 0x42) {
        /* verify: no data transfer, the command is done when all sectors are verified */
        cur_lba += cur_block;
        cur_count -= cur_block;
        progress_count += cur_block;
        if (cur_count == 0) {
            update_address_registers(cur_lba - 1u);
            count = 0;
            if (cmd_ext) hob_count = 0;
            state = IDE_DEV_READY;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            allow_writing = true;
            raise_irq();
        }
        else {
            cur_block = block_count();
            schedule(media_access(cur_lba,cur_block,false,PIC_FullIndex()));
        }
        return;
    }

    /* the data block is ready for the host */
    state = IDE_DEV_DATA_READ;
    status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
    prepare_read(0,512u * cur_block);
    raise_irq();
}

/* the host has written cur_block sectors into the sector buffer: write them to cur_lba */
void IDEATADevice::write_block() {
    imageDisk *disk = getBIOSdisk();
    if (disk == NULL) {
        sector_error(0x04/*ABRT*/);
        return;
    }

    for (unsigned int i=0;i < cur_block;i++) {
        if (cur_lba + i >= LBA) {
            cur_lba += i;
            cur_count -= i;
            sector_error(0x10/*IDNF*/);
            return;
        }
        if (disk->Write_AbsoluteSector((uint32_t)(cur_lba + i),sector + (i * 512u)) != Int13Status::NoError) {
            cur_lba += i;
            cur_count -= i;
            sector_error(0x10/*IDNF*/);
            return;
        }
    }

    cur_lba += cur_block;
    cur_count -= cur_block;
    progress_count += cur_block;

    if (cur_count == 0) {
        update_address_registers(cur_lba - 1u);
        count = 0;
        if (cmd_ext) hob_count = 0;
        state = IDE_DEV_READY;
        status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
        allow_writing = true;
        raise_irq();
        return;
    }

    /* ready for the next data block */
    cur_block = block_count();
    state = IDE_DEV_DATA_WRITE;
    status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
    prepare_write(0,512u * cur_block);
    raise_irq();
}

void IDEATADevice::io_completion() {
    /* lower DRQ */
    status &= ~IDE_STATUS_DRQ;

    switch (command) {
        case 0x20: case 0x21: case 0x24: /* READ SECTOR(S) (EXT) */
        case 0xC4: case 0x29: /* READ MULTIPLE (EXT) */
            /* the host has read the data block */
            cur_lba += cur_block;
            cur_count -= cur_block;
            progress_count += cur_block;
            if (cur_count == 0) {
                /* end of the transfer: the registers show the last sector */
                update_address_registers(cur_lba - 1u);
                count = 0;
                if (cmd_ext) hob_count = 0;
                status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
                state = IDE_DEV_READY;
                allow_writing = true;
                return;
            }

            /* the next block, as soon as it has come off the disk */
            cur_block = block_count();
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            schedule(media_access(cur_lba,cur_block,false,PIC_FullIndex()));
            break;
        case 0x30: case 0x31: case 0x34: /* WRITE SECTOR(S) (EXT) */
        case 0xC5: case 0x39: /* WRITE MULTIPLE (EXT) */
            /* the drive has the data block, it goes to the write cache (or straight to the disk) */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            {
                const double now = PIC_FullIndex();
                const double written = media_access(cur_lba,cur_block,true,now + (instant ? 0 : IDE_HD_COMMAND_MS));
                double done = written;

                if (write_cache && !instant) {
                    /* write behind: done when the data is in the cache, unless the cache is full */
                    mech_free_at = written;
                    done = written - IDE_HD_WRITE_CACHE_SECTORS * sector_ms;
                    if (done < now + IDE_HD_COMMAND_MS) done = now + IDE_HD_COMMAND_MS;
                }
                schedule(done);
            }
            break;
        case 0xEC: /* IDENTIFY */
            feature = 0;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
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

/* the delayed part of an ATA command (data ready, written, seek complete, ...) */
void IDEATADevice::delayed_command() {
    switch (command) {
        case 0x20: case 0x21: case 0x24: /* READ SECTOR(S) (EXT) */
        case 0xC4: case 0x29: /* READ MULTIPLE (EXT) */
        case 0x40: case 0x41: case 0x42: /* READ VERIFY SECTOR(S) (EXT) */
            read_block();
            break;
        case 0x30: case 0x31: case 0x34: /* WRITE SECTOR(S) (EXT) */
        case 0xC5: case 0x39: /* WRITE MULTIPLE (EXT) */
            write_block();
            break;
        case 0xEC:/*IDENTIFY DEVICE (CONTINUED) */
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            generate_identify_device();
            prepare_read(0,512);
            count = 0x01;
            lba[0] = 0x00;
            feature = 0x00;
            lba[1] = 0x00;
            lba[2] = 0x00;
            raise_irq();
            break;
        default:
            /* seek, recalibrate, flush cache, power management: complete */
            state = IDE_DEV_READY;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            allow_writing = true;
            raise_irq();
            break;
    }
}

void IDEATADevice::host_reset_complete() {
    IDEDevice::host_reset_complete();
    /* a reset wakes the device from sleep, the disk stays stopped until it is accessed */
    if (power == PWR_SLEEP) power = PWR_STANDBY;
}

void IDEATADevice::set_signature() {
    IDEDevice::set_signature();
}

Bitu IDEATAPICDROMDevice::data_read(Bitu iolen) {
    Bitu w = ~0u;

    if (state != IDE_DEV_DATA_READ)
        return 0xFFFFUL;

    if (!(status & IDE_STATUS_DRQ)) {
        LOG_MSG("IDE: Data read when DRQ=0\n");
        return 0xFFFFUL;
    }

    if (sector_i >= sector_total)
        return 0xFFFFUL;

    if (iolen >= 4) {
        w = host_readd(sector+sector_i);
        sector_i += 4;
    }
    else if (iolen >= 2) {
        w = host_readw(sector+sector_i);
        sector_i += 2;
    }
    /* NTS: Some MS-DOS CD-ROM drivers like OAKCDROM.SYS use byte-wide I/O for the initial identification */
    else if (iolen == 1) {
        w = sector[sector_i++];
    }

    if (sector_i >= sector_total)
        io_completion();

    return w;
}

static const uint16_t ReadCDTransferSectorSizeTable[5/*SectorType-1*/][0x20/*READ CD byte 9 >> 3*/] = {
        /* Sector type 0: Any
         * Sector type 1: CDDA */
        {
                0,    /* 00h */ 0,    /* 08h */ 2352, /* 10h */ 2352, /* 18h */
                2352, /* 20h */ 2352, /* 28h */ 2352, /* 30h */ 2352, /* 38h */
                2352, /* 40h */ 2352, /* 48h */ 2352, /* 50h */ 2352, /* 58h */
                2352, /* 60h */ 2352, /* 68h */ 2352, /* 70h */ 2352, /* 78h */
                0,    /* 80h */ 0,    /* 88h */ 2352, /* 90h */ 2352, /* 98h */
                2352, /* A0h */ 2352, /* A8h */ 2352, /* B0h */ 2352, /* B8h */
                2352, /* C0h */ 2352, /* C8h */ 2352, /* D0h */ 2352, /* D8h */
                2352, /* E0h */ 2352, /* E8h */ 2352, /* F0h */ 2352  /* F8h */
        },
        /* Sector type 2: Mode 1 */
        {
                0,    /* 00h */ 0,    /* 08h */ 2048, /* 10h */ 2336, /* 18h */
                4,    /* 20h */ 0,    /* 28h */ 2052, /* 30h */ 2340, /* 38h */
                0,    /* 40h */ 0,    /* 48h */ 2048, /* 50h */ 2336, /* 58h */
                4,    /* 60h */ 0,    /* 68h */ 2052, /* 70h */ 2340, /* 78h */
                0,    /* 80h */ 0,    /* 88h */ 0,    /* 90h */ 0,    /* 98h */
                16,   /* A0h */ 0,    /* A8h */ 2064, /* B0h */ 2352, /* B8h */
                0,    /* C0h */ 0,    /* C8h */ 0,    /* D0h */ 0,    /* D8h */
                16,   /* E0h */ 0,    /* E8h */ 2064, /* F0h */ 2352  /* F8h */
        },
        /* Sector type 3: Mode 2 formless */
        {
                0,    /* 00h */ 0,    /* 08h */ 2336, /* 10h */ 2336, /* 18h */
                4,    /* 20h */ 0,    /* 28h */ 2340, /* 30h */ 2340, /* 38h */
                0,    /* 40h */ 0,    /* 48h */ 2336, /* 50h */ 2336, /* 58h */
                4,    /* 60h */ 4,    /* 68h */ 12,   /* 70h */ 12,   /* 78h */
                0,    /* 80h */ 0,    /* 88h */ 0,    /* 90h */ 0,    /* 98h */
                16,   /* A0h */ 0,    /* A8h */ 2352, /* B0h */ 2352, /* B8h */
                0,    /* C0h */ 0,    /* C8h */ 0,    /* D0h */ 0,    /* D8h */
                16,   /* E0h */ 0,    /* E8h */ 2352, /* F0h */ 2352  /* F8h */
        },
        /* Sector type 4: Mode 2 form 1 */
        {
                0,    /* 00h */ 0,    /* 08h */ 2048, /* 10h */ 2328, /* 18h */
                4,    /* 20h */ 0,    /* 28h */ 0,    /* 30h */ 0,    /* 38h */
                8,    /* 40h */ 0,    /* 48h */ 2056, /* 50h */ 2336, /* 58h */
                12,   /* 60h */ 0,    /* 68h */ 2060, /* 70h */ 2340, /* 78h */
                0,    /* 80h */ 0,    /* 88h */ 0,    /* 90h */ 0,    /* 98h */
                16,   /* A0h */ 0,    /* A8h */ 0,    /* B0h */ 0,    /* B8h */
                0,    /* C0h */ 0,    /* C8h */ 0,    /* D0h */ 0,    /* D8h */
                24,   /* E0h */ 0,    /* E8h */ 2072, /* F0h */ 2352  /* F8h */
        },
        /* Sector type 5: Mode 2 form 2 */
        {
                0,    /* 00h */ 0,    /* 08h */ 2328, /* 10h */ 2328, /* 18h */
                4,    /* 20h */ 0,    /* 28h */ 0,    /* 30h */ 0,    /* 38h */
                8,    /* 40h */ 0,    /* 48h */ 2336, /* 50h */ 2336, /* 58h */
                12,   /* 60h */ 0,    /* 68h */ 2340, /* 70h */ 2340, /* 78h */
                0,    /* 80h */ 0,    /* 88h */ 0,    /* 90h */ 0,    /* 98h */
                16,   /* A0h */ 0,    /* A8h */ 0,    /* B0h */ 0,    /* B8h */
                0,    /* C0h */ 0,    /* C8h */ 0,    /* D0h */ 0,    /* D8h */
                24,   /* E0h */ 0,    /* E8h */ 2352, /* F0h */ 2352  /* F8h */
        }
};

/* READ CD / READ CD MSF: parse the command */
bool IDEATAPICDROMDevice::read_cd_begin() {
    rcd_type = (atapi_cmd[1] >> 2) & 7u;    /* expected sector type */
    rcd_flags = atapi_cmd[9];               /* SYNC=[7:7] HeaderCodes=[6:5] UserData=[4:4] EDCECC=[3:3] ErrorField=[2:1] */
    rcd_sub = atapi_cmd[10] & 7u;           /* sub-channel: 0 none, 1 raw P-W, 2 formatted Q, 4 corrected R-W */

    if (rcd_type > 5 || ((rcd_flags >> 1) & 3u) == 3 ||
        (rcd_sub != 0 && rcd_sub != 1 && rcd_sub != 2 && rcd_sub != 4)) {
        set_sense(0x05,0x24); /* Invalid Field in CDB */
        return false;
    }

    /* a combination of fields that no sector of the expected type has */
    if (rcd_type >= 2 && (rcd_flags >> 3) != 0 && ReadCDTransferSectorSizeTable[rcd_type-1][rcd_flags >> 3] == 0) {
        set_sense(0x05,0x24);
        return false;
    }

    if (atapi_cmd[0] == 0xB9) {
        const Bitu s = ((Bitu)atapi_cmd[3] * 60u + atapi_cmd[4]) * 75u + atapi_cmd[5];
        const Bitu e = ((Bitu)atapi_cmd[6] * 60u + atapi_cmd[7]) * 75u + atapi_cmd[8];
        if (s < 150u || e < s) {
            set_sense(0x05,0x24);
            return false;
        }
        LBA = s - 150u;
        TransferLength = e - s;
    }
    else {
        LBA = ((Bitu)atapi_cmd[2] << 24UL) | ((Bitu)atapi_cmd[3] << 16UL) | ((Bitu)atapi_cmd[4] << 8UL) | ((Bitu)atapi_cmd[5] << 0UL);
        TransferLength = ((Bitu)atapi_cmd[6] << 16UL) | ((Bitu)atapi_cmd[7] << 8UL) | ((Bitu)atapi_cmd[8]);
    }

    return true;
}

/* READ CD: one sector with the fields asked for, in the order MMC gives them: sync, header,
 * sub-header, user data, EDC/ECC, C2 error information, sub-channel. Returns the number of
 * bytes, or -1 (sense set). */
int IDEATAPICDROMDevice::read_cd_sector(Bitu lba,unsigned char *out) {
    CDROM_Interface *cdrom = getMSCDEXDrive();
    unsigned char *w = out;
    uint8_t raw[2352];
    int kind; /* 1 CD-DA, 2 Mode 1, 3 Mode 2 formless, 4 Mode 2 Form 1, 5 Mode 2 Form 2 */

    if (cdrom == NULL) {
        set_sense(0x02,0x3A,0x01);
        return -1;
    }

    const int mode = cdrom->GetSectorMode((unsigned long)lba);
    if (mode < 0) {
        set_sense(0x05,0x21); /* Logical Block Address out of range */
        return -1;
    }

    if (cdrom->ReadSectorsHost(raw,true,(unsigned long)lba,1)) {
        if (mode == 0) kind = 1;
        else if (raw[15] == 2) {
            /* CD-ROM XA (Form 1/2) sectors have the sub-header twice */
            if (memcmp(raw + 16,raw + 20,4) == 0) kind = (raw[18] & 0x20) ? 5 : 4;
            else kind = 3;
        }
        else kind = 2;
    }
    else {
        /* the image has only the user data: make up the rest of the sector */
        uint8_t data[2048];
        if (mode == 0 || !cdrom->ReadSectorsHost(data,false,(unsigned long)lba,1)) {
            set_sense(0x03,0x11); /* Medium Error: Unrecovered Read Error */
            return -1;
        }
        cd_encode_sector(raw,lba,data,mode == 2);
        kind = (mode == 2) ? 4 : 2;
    }

    if (rcd_type != 0 && rcd_type != kind) {
        set_sense(0x05,0x64); /* Illegal Mode For This Track */
        return -1;
    }

    const uint8_t f = rcd_flags;
    if (kind == 1) {
        /* CD-DA: any of the data fields means the 2352 bytes of audio */
        if (f & 0xF8) {
            memcpy(w,raw,2352);
            w += 2352;
        }
    }
    else {
        if ((f >> 3) != 0 && ReadCDTransferSectorSizeTable[kind-1][f >> 3] == 0) {
            set_sense(0x05,0x24);
            return -1;
        }

        if (f & 0x80) { memcpy(w,raw,12); w += 12; }                                   /* sync */
        if (f & 0x20) { memcpy(w,raw + 12,4); w += 4; }                                /* header */
        if ((f & 0x40) && (kind == 4 || kind == 5)) { memcpy(w,raw + 16,8); w += 8; }  /* sub-header */
        switch (kind) {
            case 2: /* Mode 1 */
                if (f & 0x10) { memcpy(w,raw + 16,2048); w += 2048; }
                if (f & 0x08) { memcpy(w,raw + 2064,288); w += 288; }  /* EDC, zero, ECC */
                break;
            case 3: /* Mode 2 formless */
                if (f & 0x10) { memcpy(w,raw + 16,2336); w += 2336; }
                break;
            case 4: /* Mode 2 Form 1 */
                if (f & 0x10) { memcpy(w,raw + 24,2048); w += 2048; }
                if (f & 0x08) { memcpy(w,raw + 2072,280); w += 280; }  /* EDC, ECC */
                break;
            case 5: /* Mode 2 Form 2 (the user data includes the spare/EDC bytes) */
                if (f & 0x10) { memcpy(w,raw + 24,2328); w += 2328; }
                break;
        }
    }

    /* C2 error information: no errors (294 bytes), plus the block error byte and a pad byte */
    const unsigned int ef = (f >> 1) & 3u;
    if (ef == 1) { memset(w,0,294); w += 294; }
    else if (ef == 2) { memset(w,0,296); w += 296; }

    /* sub-channel: the Q channel tells the position, from the table of contents */
    if (rcd_sub != 0) {
        uint8_t q[12];
        int first,last,track = 1;
        unsigned char attr = (kind == 1) ? 0x00 : 0x40;
        Bitu tstart = 0;
        TMSF leadOut;

        if (cdrom->GetAudioTracks(first,last,leadOut)) {
            for (int t=first;t <= last;t++) {
                TMSF s;
                unsigned char a;
                if (!cdrom->GetAudioTrackInfo(t,s,a)) continue;
                const Bitu st = ((Bitu)s.min * 60u + s.sec) * 75u + s.fr - 150u;
                if (lba >= st) {
                    track = t;
                    attr = a;
                    tstart = st;
                }
            }
        }

        const Bitu rel = lba - tstart,abs = lba + 150u;
        q[0] = (uint8_t)((attr & 0xF0u) | 0x01u);  /* CONTROL, ADR 1 (position) */
        q[1] = cd_bcd((unsigned int)track);
        q[2] = cd_bcd(1);
        q[3] = cd_bcd((unsigned int)(rel / (75u * 60u)));
        q[4] = cd_bcd((unsigned int)((rel / 75u) % 60u));
        q[5] = cd_bcd((unsigned int)(rel % 75u));
        q[6] = 0;
        q[7] = cd_bcd((unsigned int)(abs / (75u * 60u)));
        q[8] = cd_bcd((unsigned int)((abs / 75u) % 60u));
        q[9] = cd_bcd((unsigned int)(abs % 75u));
        const uint16_t crc = cd_q_crc(q);
        q[10] = (uint8_t)(crc >> 8u);
        q[11] = (uint8_t)crc;

        if (rcd_sub == 2) {
            /* formatted Q */
            memcpy(w,q,12);
            memset(w + 12,0,4);
            w += 16;
        }
        else if (rcd_sub == 1) {
            /* raw P-W: one bit of each channel per byte, P in bit 7, Q in bit 6, R-W empty */
            for (unsigned int i=0;i < 96;i++)
                w[i] = (uint8_t)(((q[i >> 3u] >> (7u - (i & 7u))) & 1u) << 6u);
            w += 96;
        }
        else {
            /* R-W: no CD+G or CD-TEXT data */
            memset(w,0,96);
            w += 96;
        }
    }

    return (int)(w - out);
}

bool IDEATAPICDROMDevice::read_cd_sectors(Bitu lba,Bitu count,Bitu &bytes) {
    bytes = 0;
    for (Bitu i=0;i < count;i++) {
        const int n = read_cd_sector(lba + i,sector + bytes);
        if (n < 0) return false;
        bytes += (Bitu)n;
    }
    return true;
}

/* the PACKET command has been received */
void IDEATAPICDROMDevice::atapi_cmd_completion() {
    const double now = PIC_FullIndex();
    const double overhead = faked_command ? 0.000001 : (instant ? 0.01 : IDE_CD_COMMAND_MS);
    const uint8_t op = atapi_cmd[0];
    CDROM_Interface *cdrom = getMSCDEXDrive();
    double at = now + overhead;

#if 0
    LOG_MSG("ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x to_host=%u\n",
        atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
        atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11],
        atapi_to_host);
#endif

    /* DRQ data blocks of READ: as many sectors as fit the buffer and the byte count limit */
    auto plan_chunk = [&](Bitu per) {
        TransferLengthRemaining = TransferLength;
        if (TransferLength > sector_transfer_limit)
            TransferLength = sector_transfer_limit;
        Bitu fit = MIN((Bitu)sizeof(sector),(Bitu)host_maximum_byte_count) / per;
        if (fit == 0) fit = 1;
        if (TransferLength > fit) TransferLength = fit;
        TransferLengthRemaining -= TransferLength;
        LBAnext = LBA;
    };

    switch (op) {
        case 0x00: /* TEST UNIT READY */
            if (common_spinup_response(/*spin up*/false,/*wait*/false))
                set_sense(0); /* <- nothing wrong */
            if (sense[2] & 0xF) atapi_fail();
            else atapi_done();
            return;
        case 0x4A: /* GET EVENT STATUS NOTIFICATION (not held up by unit attention) */
            get_event_status_notification();
            return;
        case 0x46: /* GET CONFIGURATION (not held up by unit attention) */
            get_configuration();
            return;
        case 0xBB: /* SET CD SPEED */
            set_cd_speed();
            return;
        case 0x03: /* REQUEST SENSE */
        case 0x12: /* INQUIRY */
        case 0x1A: /* MODE SENSE(6) */
        case 0x5A: /* MODE SENSE(10) */
        case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
        case 0xBD: /* MECHANISM STATUS */
        case 0x15: /* MODE SELECT(6) */
        case 0x55: /* MODE SELECT(10) */
            break;
        case 0x1B: /* START/STOP UNIT */
            {
                const bool IMMED = !!(atapi_cmd[1] & 1);
                const bool START = !!(atapi_cmd[4] & 0x01);
                const bool LOEJ = !!(atapi_cmd[4] & 0x02);
                const unsigned char POWER = atapi_cmd[4] >> 4u;

                /* the tray is locked: the disc does not come out */
                if (POWER == 0 && LOEJ && !START && locked) {
                    set_sense(0x05,0x53,0x02); /* Medium Removal Prevented */
                    atapi_fail();
                    return;
                }
                set_sense(0);

                /* the tray moves or the disc spins up before the command completes, unless IMMED */
                if (!IMMED && !instant && !faked_command) {
                    if (POWER == 0 && LOEJ) at += tray_time;
                    else if ((POWER == 0 && START) || POWER == 1) {
                        if (loading_mode == LOAD_IDLE) at += spinup_time;
                    }
                }
            }
            break;
        case 0x25: /* READ CAPACITY */
        case 0x28: /* READ(10) */
        case 0xA8: /* READ(12) */
        case 0xBE: /* READ CD */
        case 0xB9: /* READ CD MSF */
        case 0x2B: /* SEEK */
        case 0x42: /* READ SUB-CHANNEL */
        case 0x43: /* READ TOC */
        case 0x45: /* PLAY AUDIO(10) */
        case 0x47: /* PLAY AUDIO MSF */
        case 0x4B: /* PAUSE/RESUME */
        case 0x4E: /* STOP PLAY/SCAN */
        case 0x51: /* READ DISC INFORMATION */
        case 0x52: /* READ TRACK INFORMATION */
            /* these need a disc, and wait for it to spin up */
            if (!common_spinup_response(/*spin up*/true,/*wait*/true)) {
                atapi_fail();
                return;
            }
            set_sense(0); /* <- nothing wrong */
            cdrom = getMSCDEXDrive();
            break;
        default:
            /* we don't know the command, immediately return an error */
            LOG_MSG("Unknown ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
                atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11]);
            set_sense(0x05,0x20); /* Invalid Command Operation Code */
            atapi_fail();
            return;
    }

    switch (op) {
        case 0x28: /* READ(10) */
        case 0xA8: /* READ(12) */
            {
                LBA = ((Bitu)atapi_cmd[2] << 24UL) | ((Bitu)atapi_cmd[3] << 16UL) | ((Bitu)atapi_cmd[4] << 8UL) | ((Bitu)atapi_cmd[5] << 0UL);
                if (op == 0x28)
                    TransferLength = ((Bitu)atapi_cmd[7] << 8) | ((Bitu)atapi_cmd[8]);
                else
                    TransferLength = ((Bitu)atapi_cmd[6] << 24UL) | ((Bitu)atapi_cmd[7] << 16UL) | ((Bitu)atapi_cmd[8] << 8UL) | ((Bitu)atapi_cmd[9]);

                /* NTS: In case you're wondering, it's legal to issue READ(10) with transfer length == 0.
                   MSCDEX.EXE does it when starting up, for example */
                if (TransferLength == 0) {
                    atapi_done();
                    return;
                }

                const Bitu cap = atapi_capacity(cdrom);
                if (LBA >= cap || LBA + TransferLength > cap) {
                    set_sense(0x05,0x21); /* Logical Block Address out of range */
                    atapi_fail();
                    return;
                }
                if (cdrom->GetSectorMode((unsigned long)LBA) == 0) {
                    set_sense(0x05,0x64); /* Illegal Mode For This Track (CD-DA) */
                    atapi_fail();
                    return;
                }

                plan_chunk(2048);
                at = cd_access(LBA,TransferLength,at);
            }
            break;
        case 0xBE: /* READ CD */
        case 0xB9: /* READ CD MSF */
            if (!read_cd_begin()) {
                atapi_fail();
                return;
            }
            /* NTS: This is required to work correctly with Windows NT 4.0. Windows NT will emit a READ CD
             *      command at startup with transfer length == 0. If an error is returned, NT ignores the
             *      CD-ROM drive entirely and acts like it's in a perpetual error state. */
            if (TransferLength == 0) {
                atapi_done();
                return;
            }
            if (LBA >= atapi_capacity(cdrom)) {
                set_sense(0x05,0x21);
                atapi_fail();
                return;
            }
            plan_chunk(IDE_CD_MAX_READCD_SECTOR);
            at = cd_access(LBA,TransferLength,at);
            break;
        case 0x2B: /* SEEK */
            LBA = ((Bitu)atapi_cmd[2] << 24UL) | ((Bitu)atapi_cmd[3] << 16UL) | ((Bitu)atapi_cmd[4] << 8UL) | ((Bitu)atapi_cmd[5] << 0UL);
            if (LBA >= atapi_capacity(cdrom)) {
                set_sense(0x05,0x21);
                atapi_fail();
                return;
            }
            at = cd_access(LBA,0,at);
            rb_valid = false;
            break;
        case 0x45: /* PLAY AUDIO(10): seek to the start first */
            {
                const Bitu s = ((Bitu)atapi_cmd[2] << 24UL) | ((Bitu)atapi_cmd[3] << 16UL) | ((Bitu)atapi_cmd[4] << 8UL) | ((Bitu)atapi_cmd[5] << 0UL);
                if (s != 0xFFFFFFFFul) at = cd_access(s,0,at);
            }
            break;
        case 0x47: /* PLAY AUDIO MSF: seek to the start first */
            if (!(atapi_cmd[3] == 0xFF && atapi_cmd[4] == 0xFF && atapi_cmd[5] == 0xFF)) {
                Bitu s = ((Bitu)atapi_cmd[3] * 60u + atapi_cmd[4]) * 75u + atapi_cmd[5];
                s = s >= 150u ? s - 150u : 0u;
                at = cd_access(s,0,at);
            }
            break;
        default:
            break;
    }

    count = 0x02;
    state = IDE_DEV_ATAPI_BUSY;
    status = IDE_STATUS_BUSY;
    schedule(at);
}

void IDEATAPICDROMDevice::data_write(Bitu v,Bitu iolen) {
    if (state == IDE_DEV_ATAPI_PACKET_COMMAND) {
        if (atapi_cmd_i < atapi_cmd_total)
            atapi_cmd[atapi_cmd_i++] = v;
        if (iolen >= 2 && atapi_cmd_i < atapi_cmd_total)
            atapi_cmd[atapi_cmd_i++] = v >> 8;
        if (iolen >= 4 && atapi_cmd_i < atapi_cmd_total) {
            atapi_cmd[atapi_cmd_i++] = v >> 16;
            atapi_cmd[atapi_cmd_i++] = v >> 24;
        }

        if (atapi_cmd_i >= atapi_cmd_total)
            atapi_cmd_completion();
    }
    else {
        if (state != IDE_DEV_DATA_WRITE) {
            LOG_MSG("ide atapi warning: data write when device not in data_write state\n");
            return;
        }
        if (!(status & IDE_STATUS_DRQ)) {
            LOG_MSG("ide atapi warning: data write with drq=0\n");
            return;
        }
        if ((sector_i+iolen) > sector_total) {
            LOG_MSG("ide atapi warning: sector already full %lu / %lu\n",(unsigned long)sector_i,(unsigned long)sector_total);
            return;
        }

        if (iolen >= 4) {
            host_writed(sector+sector_i,v);
            sector_i += 4;
        }
        else if (iolen >= 2) {
            host_writew(sector+sector_i,v);
            sector_i += 2;
        }
        else if (iolen == 1) {
            sector[sector_i++] = v;
        }

        if (sector_i >= sector_total)
            io_completion();
    }
}

Bitu IDEATADevice::data_read(Bitu iolen) {
    Bitu w = ~0u;

    if (state != IDE_DEV_DATA_READ)
        return 0xFFFFUL;

    if (!(status & IDE_STATUS_DRQ)) {
        LOG_MSG("IDE: Data read when DRQ=0\n");
        return 0xFFFFUL;
    }

    if ((sector_i+iolen) > sector_total) {
        LOG_MSG("ide ata warning: sector already read %lu / %lu\n",(unsigned long)sector_i,(unsigned long)sector_total);
        return 0xFFFFUL;
    }

    if (iolen >= 4) {
        w = host_readd(sector+sector_i);
        sector_i += 4;
    }
    else if (iolen >= 2) {
        w = host_readw(sector+sector_i);
        sector_i += 2;
    }
    /* NTS: Some MS-DOS CD-ROM drivers like OAKCDROM.SYS use byte-wide I/O for the initial identification */
    else if (iolen == 1) {
        w = sector[sector_i++];
    }

    if (sector_i >= sector_total)
        io_completion();

    return w;
}

void IDEATADevice::data_write(Bitu v,Bitu iolen) {
    if (state != IDE_DEV_DATA_WRITE) {
        LOG_MSG("ide ata warning: data write when device not in data_write state\n");
        return;
    }
    if (!(status & IDE_STATUS_DRQ)) {
        LOG_MSG("ide ata warning: data write with drq=0\n");
        return;
    }
    if ((sector_i+iolen) > sector_total) {
        LOG_MSG("ide ata warning: sector already full %lu / %lu\n",(unsigned long)sector_i,(unsigned long)sector_total);
        return;
    }

    if (iolen >= 4) {
        host_writed(sector+sector_i,v);
        sector_i += 4;
    }
    else if (iolen >= 2) {
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
//  assert((size&1) == 0);

    sector_i = offset;
    sector_total = size;
    assert(sector_i <= sector_total);
    assert(sector_total <= sizeof(sector));
}

void IDEATAPICDROMDevice::prepare_write(Bitu offset,Bitu size) {
    /* I/O must be WORD ALIGNED */
    assert((offset&1) == 0);
//  assert((size&1) == 0);

    sector_i = offset;
    sector_total = size;
    assert(sector_i <= sector_total);
    assert(sector_total <= sizeof(sector));
}

void IDEATADevice::prepare_write(Bitu offset,Bitu size) {
    /* I/O must be WORD ALIGNED */
    assert((offset&1) == 0);
//  assert((size&1) == 0);

    sector_i = offset;
    sector_total = size;
    assert(sector_i <= sector_total);
    assert(sector_total <= sizeof(sector));
}

void IDEATADevice::prepare_read(Bitu offset,Bitu size) {
    /* I/O must be WORD ALIGNED */
    assert((offset&1) == 0);
//  assert((size&1) == 0);

    sector_i = offset;
    sector_total = size;
    assert(sector_i <= sector_total);
    assert(sector_total <= sizeof(sector));
}

void IDEATAPICDROMDevice::drop_all_cds() {
    for (auto &cd : cdrom_swaplist) {
        cd->Release();
        cd = NULL;
    }
    cdrom_swaplist.clear();
    cdrom_swaplist_pos = 0;
    if (cdrom) {
        cdrom->Release();
        cdrom = NULL;
    }
}

void IDEATAPICDROMDevice::swap_to_next_cd() {
    if (cdrom_swaplist.empty()) return;
    if (mscdex) return; // the drive manager manages swapping for us

    if (cdrom) cdrom->Release();
    cdrom = NULL;

    if (++cdrom_swaplist_pos >= cdrom_swaplist.size())
        cdrom_swaplist_pos = 0;

    (cdrom = cdrom_swaplist[cdrom_swaplist_pos])->Addref();
}

void IDEATAPICDROMDevice::generate_mmc_inquiry() {
    Bitu i;

    /* IN RESPONSE TO ATAPI COMMAND 0x12: INQUIRY */
    memset(sector,0,36);
    sector[0] = (0 << 5) | 5;   /* Peripheral qualifier=0   device type=5 (CDROM) */
    sector[1] = 0x80;       /* RMB=1 removable media */
    sector[3] = 0x21;
    sector[4] = 36 - 5;     /* additional length */

    for (i=0;i < 8 && i < id_mmc_vendor_id.length();i++)
        sector[i+8] = (unsigned char)id_mmc_vendor_id[i];
    for (;i < 8;i++)
        sector[i+8] = ' ';

    for (i=0;i < 16 && i < id_mmc_product_id.length();i++)
        sector[i+16] = (unsigned char)id_mmc_product_id[i];
    for (;i < 16;i++)
        sector[i+16] = ' ';

    for (i=0;i < 4 && i < id_mmc_product_rev.length();i++)
        sector[i+32] = (unsigned char)id_mmc_product_rev[i];
    for (;i < 4;i++)
        sector[i+32] = ' ';
}

void IDEATAPICDROMDevice::generate_identify_device() {
    unsigned char csum;
    Bitu i;

    /* IN RESPONSE TO IDENTIFY DEVICE (0xA1)
       GENERATE 512-BYTE REPLY */
    memset(sector,0,512);

    host_writew(sector+(0*2),0x85C0U);  /* ATAPI device, command set #5 (what the fuck does that mean?), removable, */

    for (i=0;i < 20 && i < id_serial.length();i++)
        sector[(i^1)+(10*2)] = (unsigned char)id_serial[i];
    for (;i < 20;i++)
        sector[(i^1)+(10*2)] = ' ';

    for (i=0;i < 8 && i < id_firmware_rev.length();i++)
        sector[(i^1)+(23*2)] = (unsigned char)id_firmware_rev[i];
    for (;i < 8;i++)
        sector[(i^1)+(23*2)] = ' ';

    for (i=0;i < 40 && i < id_model.length();i++)
        sector[(i^1)+(27*2)] = (unsigned char)id_model[i];
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
    host_writew(sector+(64*2),      /* PIO modes supported */
        0x0003UL);
    host_writew(sector+(67*2),      /* PIO cycle time */
        0x0078UL);
    host_writew(sector+(68*2),      /* PIO cycle time */
        0x0078UL);
    host_writew(sector+(80*2),0x007E); /* major version number. Here we say we support ATA-1 through ATA-8 */
    host_writew(sector+(81*2),0x0022); /* minor version */
    /* command sets: word 82 bit 14 NOP, bit 9 DEVICE RESET, bit 4 PACKET, bit 3 power management.
     * words 83 and 84 bit 14 must be 1. words 85-87 say which are enabled. */
    host_writew(sector+(82*2),0x4218);
    host_writew(sector+(83*2),0x4000);
    host_writew(sector+(84*2),0x4000);
    host_writew(sector+(85*2),0x4218);
    host_writew(sector+(86*2),0x0000);
    host_writew(sector+(87*2),0x4000);

    /* ATA-8 integrity checksum */
    sector[510] = 0xA5;
    csum = 0; for (i=0;i < 511;i++) csum += sector[i];
    sector[511] = 0 - csum;
}

void IDEATADevice::generate_identify_device() {
//  imageDisk *disk = getBIOSdisk();
    unsigned char csum;
    uint64_t ptotal;
    uint64_t total;
    Bitu i;

    /* IN RESPONSE TO IDENTIFY DEVICE (0xEC)
       GENERATE 512-BYTE REPLY */
    memset(sector,0,512);

    /* total disk capacity in sectors */
    total = sects * cyls * heads;
    ptotal = phys_sects * phys_cyls * phys_heads;
    uint32_t lba28 = LBA > 0x0FFFFFFF ? 0x0FFFFFFF : (uint32_t)LBA;
    //uint32_t lba28 = (uint32_t)LBA;


    host_writew(sector+(0*2),0x0040);   /* bit 6: 1=fixed disk */
    host_writew(sector+(1*2),phys_cyls);
    host_writew(sector+(3*2),phys_heads);
    host_writew(sector+(4*2),phys_sects * 512); /* unformatted bytes per track */
    host_writew(sector+(5*2),512);      /* unformatted bytes per sector */
    host_writew(sector+(6*2),phys_sects);

    for (i=0;i < 20 && i < id_serial.length();i++)
        sector[(i^1)+(10*2)] = (unsigned char)id_serial[i];
    for (;i < 20;i++)
        sector[(i^1)+(10*2)] = ' ';

    host_writew(sector+(20*2),1);       /* ATA-1: single-ported single sector buffer */
    host_writew(sector+(21*2),4);       /* ATA-1: ECC bytes on read/write long */

    for (i=0;i < 8 && i < id_firmware_rev.length();i++)
        sector[(i^1)+(23*2)] = (unsigned char)id_firmware_rev[i];
    for (;i < 8;i++)
        sector[(i^1)+(23*2)] = ' ';

    for (i=0;i < 40 && i < id_model.length();i++)
        sector[(i^1)+(27*2)] = (unsigned char)id_model[i];
    for (;i < 40;i++)
        sector[(i^1)+(27*2)] = ' ';

    if (multiple_sector_max != 0)
        host_writew(sector+(47*2),0x8000|multiple_sector_max); /* <- READ/WRITE MULTIPLE MAX SECTORS (bits 15:8 = 80h) */

    host_writew(sector+(48*2),0x0000);  /* :0  0=we do not support doubleword (32-bit) PIO */
    host_writew(sector+(49*2),0x2A00);  /* :13 1=Standby timer values as specified by the standard */
                        /* :11 1=IORDY supported */
                        /* :10 0=IORDY not disabled */
                        /* :9  1=LBA supported */
                        /* :8  0=DMA not supported */
    host_writew(sector+(50*2),0x4000);  /* FIXME: ??? */
    host_writew(sector+(51*2),0x00F0);  /* PIO data transfer cycle timing mode */
    host_writew(sector+(52*2),0x00F0);  /* DMA data transfer cycle timing mode */
    host_writew(sector+(53*2),0x0007);  /* :2  1=the fields in word 88 are valid */
                        /* :1  1=the fields in word (70:64) are valid */
                        /* :0  1= ??? */
    host_writew(sector+(54*2),cyls);    /* current cylinders */
    host_writew(sector+(55*2),heads);   /* current heads */
    host_writew(sector+(56*2),sects);   /* current sectors per track */
    host_writed(sector+(57*2),total);   /* current capacity in sectors */

    if (multiple_sector_count != 0)
        host_writew(sector+(59*2),0x0100|multiple_sector_count); /* :8  multiple sector setting is valid */
                        /* 7:0 current setting for number of log. sectors per DRQ of READ/WRITE MULTIPLE */

    host_writed(sector+(60*2),lba28);  /* total user addressable sectors (LBA) */
    host_writew(sector+(62*2),0x0000);  /* FIXME: ??? */
    host_writew(sector+(63*2),0x0000);  /* :10 0=Multiword DMA mode 2 not selected */
                        /* TODO: Basically, we don't do DMA. Fill out this comment */
    host_writew(sector+(64*2),0x0003);  /* 7:0 PIO modes supported (FIXME ???) */
    host_writew(sector+(65*2),0x0000);  /* FIXME: ??? */
    host_writew(sector+(66*2),0x0000);  /* FIXME: ??? */
    host_writew(sector+(67*2),0x0078);  /* FIXME: ??? */
    host_writew(sector+(68*2),0x0078);  /* FIXME: ??? */
    host_writew(sector+(80*2),0x007E);  /* major version number. Here we say we support ATA-1 through ATA-8 */
    host_writew(sector+(81*2),0x0022);  /* minor version */
    /* command sets: word 82 bit 14 NOP, bit 6 read look-ahead, bit 5 write cache, bit 3 power management.
     * word 83 bit 14 must be 1, bit 13 FLUSH CACHE EXT, bit 12 FLUSH CACHE, bit 10 48-bit address.
     * word 84 bit 14 must be 1. words 85-87 say which are enabled. */
    host_writew(sector+(82*2),0x4068);
    host_writew(sector+(83*2),0x5000 | (int13_enable_48bitLBA ? 0x2400 : 0x0000));
    host_writew(sector+(84*2),0x4000);
    host_writew(sector+(85*2),0x4008 | (read_lookahead ? 0x0040 : 0x0000) | (write_cache ? 0x0020 : 0x0000));
    host_writew(sector+(86*2),0x1000 | (int13_enable_48bitLBA ? 0x2400 : 0x0000));
    host_writew(sector+(87*2),0x4000);
    host_writew(sector+(88*2),0x0000);  /* FIXME: ??? */
    host_writew(sector+(93*2),0x0000);  /* FIXME: ??? */
    host_writed(sector+(100*2), int13_enable_48bitLBA ? (uint32_t)(LBA & 0xFFFFFFFF):0); // 48bit LBA lower 32 bits
    host_writed(sector+(102*2), int13_enable_48bitLBA ? (uint32_t)(LBA >> 32):0);        // 48bit LBA upper 32 bits 

    /* ATA-8 integrity checksum */
    sector[510] = 0xA5;
    csum = 0; for (i=0;i < 511;i++) csum += sector[i];
    sector[511] = 0 - csum;
}

IDEATADevice::IDEATADevice(IDEController *c,unsigned char disk_index,bool _slave)
    : IDEDevice(c,_slave), id_serial("8086"), id_firmware_rev("8086"), id_model("DOSBox-X IDE disk"), bios_disk_index(disk_index) {
    sector_i = sector_total = 0;

    type = IDE_TYPE_HDD;
    multiple_sector_max = sizeof(sector) / 512;
    multiple_sector_count = 1;
    geo_translate = false;
    heads = 0;
    sects = 0;
    cyls = 0;
    progress_count = 0;
    phys_heads = 0;
    phys_sects = 0;
    phys_cyls = 0;
}

IDEATADevice::~IDEATADevice() {
}

imageDisk *IDEATADevice::getBIOSdisk() {
    if (bios_disk_index >= (2 + MAX_HDD_IMAGES)) return NULL;
    return imageDiskList[bios_disk_index];
}

CDROM_Interface *IDEATAPICDROMDevice::getMSCDEXDrive() {
    return cdrom;
}

void IDEATADevice::update_from_biosdisk() {
	imageDisk *dsk = getBIOSdisk();
	if (dsk == NULL) {
		LOG_MSG("WARNING: IDE update from BIOS disk failed, disk not available\n");
		return;
	}

	geo_translate = false;
	cyls = dsk->cylinders;
	heads = dsk->heads;
	sects = dsk->sectors;
    LBA = dsk->getLBA();

	if (heads > 16) {
		geo_translate = true;

		/* One additional correction: The disk image is probably using BIOS-style geometry
		   translation (such as C/H/S 1024/64/63) which is impossible given that the IDE
		   standard only allows up to 16 heads. So we have to translate the geometry. */

		/* If we can't divide the heads down, then pick a LBA-like mapping that is good enough.
		 * Note that if what we pick does not evenly map to the INT 13h geometry, and the partition
		 * contained within is not an LBA type FAT16/FAT32 partition, then Windows 95's IDE driver
		 * will ignore this device and fall back to using INT 13h. For user convenience we will
		 * print a warning to reminder the user of exactly that. */
        if(LBA <= 17ULL * 1024ULL * 4ULL) {
            sects = 17;
            heads = 4;
        }
        else if(LBA <= 17ULL * 1024ULL * 8ULL) {
            sects = 17;
            heads = 8;
        }
        else if(LBA <= 32ULL * 1024ULL * 16ULL) {
            sects = 32;
            heads = 16;
        }
        else if(LBA > 63ULL * 1024ULL * 255ULL && LBA > 65535ULL * 16ULL * 63ULL) { // Over 8.4GB, use max CHS values
            sects = 255;
            heads = 16;
        }
        else {
            sects = 63;  // Up to 8.4GB
            heads = 16;
        }


    	cyls = (uint32_t)(LBA / (sects * heads));
        if(cyls >= 65535UL) cyls = 65535; // max cyls for IDE CHS is 65535

	}
    LOG_MSG("IDE: Disk size: %gMB, BIOS C/H/S: %u/%u/%u, IDE: %u/%u/%u\n",
        (double)LBA * 512.0 / (1024.0 * 1024.0),
        (unsigned int)dsk->cylinders,
        (unsigned int)dsk->heads,
        (unsigned int)dsk->sectors,
        (unsigned int)cyls,
        (unsigned int)heads,
        (unsigned int)sects);

	phys_heads = heads;
	phys_sects = sects;
	phys_cyls = cyls;

	mech_init();
}

void IDE_Auto(signed char &index,bool &slave) {
    unsigned int i;

    index = -1;
    slave = false;
    for (i=0;i < MAX_IDE_CONTROLLERS;i++) {
        IDEController* c;
        if((c = idecontroller[i]) == NULL) continue;
        index = (signed char)i;

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

bool IDE_controller_occupied(signed char index, bool slave) { // Return true if specified slot is occupied 
    if(idecontroller[index] == NULL) return false;
    const uint8_t ide_device = slave ? 1 : 0;
    if(idecontroller[index]->device[ide_device] == NULL) {
        return false;
    }
    else {
        return true;
    }
}

/* the user changed the disc (DOSBox-X swap/mount): the drive goes through removal and insertion */
static void IDE_ATAPI_MediaChanged(IDEATAPICDROMDevice *atapi,unsigned int pk,bool immediate) {
	bool empty = false;

	if (atapi->cdrom == NULL || atapi->cdrom->class_id == CDROM_Interface::ID_FAKE) {
		if (atapi->cdrom == NULL) empty = true;
		else {
			CDROM_Interface_Fake *fake = (CDROM_Interface_Fake*)(atapi->cdrom);
			if (fake->isEmpty) empty = true;
		}
	}

	/* a guest that locked the tray would not have let the disc out. The change is made anyway,
	 * the guest sees it as if the user pressed the eject button and then changed the disc. */
	if (atapi->locked) {
		LOG_MSG("IDE ATAPI: the disc was changed while the guest had the tray locked\n");
		atapi->post_media_event(0x01/*EjectRequest*/);
	}
	atapi->post_media_event(0x03/*MediaRemoval*/);
	atapi->tray_open = false;
	atapi->rb_valid = false;

	/* the guest learns of the change through UNIT ATTENTION (and media events) */
	atapi->has_changed = true;

	PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
	PIC_RemoveSpecificEvents(IDE_ATAPI_SpinUpComplete,pk);
	PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,pk);

	if (empty) {
		/* no disc: medium not present */
		atapi->loading_mode = LOAD_READY;
	}
	else if (immediate) {
		/* the new disc is ready right away */
		atapi->loading_mode = LOAD_DISC_READIED;
		atapi->post_media_event(0x02/*NewMedia*/);
		PIC_AddEvent(IDE_ATAPI_SpinDown,atapi->spindown_timeout/*ms*/,pk);
	}
	else {
		atapi->loading_mode = LOAD_INSERT_CD;
		PIC_AddEvent(IDE_ATAPI_CDInsertion,atapi->cd_insertion_time/*ms*/,pk);
	}
}

void IDE_ATAPI_MediaChangeNotify(signed char index, bool slave, bool immediate) {
	IDEController *c;
	IDEDevice *dev;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return;

	c = idecontroller[index];
	if (c == NULL)
		return;

	dev = c->device[slave?1:0];
	if (dev == NULL)
		return;

	if (dev->type == IDE_TYPE_CDROM) {
		IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;
		const unsigned int pk = IDEEventPack(index,slave).get();

		LOG_MSG("IDE ATAPI acknowledge media change\n");
		if (immediate) LOG_MSG("--media change is immediate");

		atapi->swap_to_next_cd();
		IDE_ATAPI_MediaChanged(atapi,pk,immediate);
	}

	DOS_EnableDriveIDEMenu((unsigned char)index,slave);
}

void IDE_ATAPI_MediaChangeNotifyAll(bool immediate) {
	for (unsigned int ide=0;ide < MAX_IDE_CONTROLLERS;ide++) {
		for (unsigned int ms=0;ms < 2;ms++) {
			IDE_ATAPI_MediaChangeNotify(ide,ms,immediate);
		}
	}
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_ATAPI_MediaChangeNotify(unsigned char drive_index,bool immediate) {
	for (unsigned int ide=0;ide < MAX_IDE_CONTROLLERS;ide++) {
		IDEController *c = idecontroller[ide];
		if (c == NULL) continue;
		for (unsigned int ms=0;ms < 2;ms++) {
			const unsigned int pk = IDEEventPack(c->interface_index,ms).get();
			IDEDevice *dev = c->device[ms];
			if (dev == NULL) continue;
			if (dev->type == IDE_TYPE_CDROM) {
				IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;
				if (drive_index == atapi->drive_index) {
					LOG_MSG("IDE ATAPI acknowledge media change for drive %c\n",drive_index+'A');
					if (immediate) LOG_MSG("--media change is immediate");

					CDROM_Interface *cdrom = NULL;

					if (atapi->mscdex) {
						if (GetMSCDEXDrive(drive_index,&cdrom)) {
							if (atapi->cdrom) atapi->cdrom->Release();
							(atapi->cdrom = cdrom)->Addref();
						}
					}

					IDE_ATAPI_MediaChanged(atapi,pk,immediate);
				}
			}
		}
	}
}

int CDROM_AllocateInterface(const char* physicalPath,int forceCD,uint16_t numDrive,CDROM_Interface **cdrom);

bool IDE_CDROM_Eject(int index,bool slave) {
	IDEATAPICDROMDevice *dev;
	IDEController *c;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return false;

	c = idecontroller[index];
	if(c == NULL)
		return false;

	if (c->device[slave?1:0] == NULL)
		return false;

	dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
	if (!dev)
		return false;

        CDROM_Interface *cdrom = NULL;
        if (CDROM_AllocateInterface("empty",true,0xFFFF/*invalid subunit*/,&cdrom))/*Addrefs*/
                return false;

        dev->drop_all_cds();
        (dev->cdrom = cdrom)->Addref();
        cdrom->Release();

        return true;
}

bool IDE_CDROM_Attach(signed char index,bool slave,const std::vector<CDROM_Interface*> &cds,bool opt_replace) {
	IDEATAPICDROMDevice *dev;
	IDEController *c;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return false;
	if (cds.empty())
		return false;

	c = idecontroller[index];
	if(c == NULL) {
		LOG_MSG("IDE: WARNING: IDE %s controller not available. Check setting or specify another controller.", ideslot[index]);
		return false;
	}

	if (opt_replace && c->device[slave?1:0] != NULL) {
		dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave?1:0]);
		if(dev == NULL) {
			LOG_MSG("IDE: Unable to attach CD-ROM images");
			return false;
		}

		dev->drop_all_cds();
		(dev->cdrom = cds[0])->Addref();
	}
	else {
		if (c->device[slave?1:0] != NULL) {
			LOG_MSG("IDE: WARNING: IDE controller %s %s already occupied, specify another slot.",ideslot[index],master_slave[slave?1:0]);
			return false;
		}

		dev = new IDEATAPICDROMDevice(c,0xFFu/*no drive*/,slave,cds[0]);
		if(dev == NULL) {
			LOG_MSG("IDE: Unable to attach CD-ROM images");
			return false;
		}
	}

	if (cds.size() > 1) {
		assert(dev->cdrom_swaplist.empty());
		assert(dev->cdrom_swaplist_pos == 0);
		for (auto &cd : cds) {
			cd->Addref();
			dev->cdrom_swaplist.push_back(cd);
		}
	}

	c->device[slave?1:0] = (IDEDevice*)dev;
	LOG_MSG("IMGMOUNT: CD-ROM image mounted to (IDE %s %s)", ideslot[index], master_slave[slave ? 1 : 0]);
	DOS_EnableDriveIDEMenu((unsigned char)index,slave);
	return true;
}

struct ide_opt_spec_t {
	signed char index = -1;
	bool slave = false;
};

bool IDE_CDROM_ParseOptSpec(struct ide_opt_spec_t &spec,const std::string &opts) {
	const char *opts_c = opts.c_str();

	/* opts: "1m" "1s" "2m" "2s" etc */
	if (isdigit(*opts_c)) {
		const unsigned int c = strtoul(opts_c,(char**)(&opts_c),10);
		if (c <= 128) spec.index = (signed char)(c - 1u);

		if (*opts_c == 'm') {
			spec.slave = false;
			opts_c++;
		}
		else if (*opts_c == 's') {
			spec.slave = true;
			opts_c++;
		}
	}

	return true;
}

bool IDE_CDROM_Attach(const std::string &opts,const std::vector<CDROM_Interface*> &cds,bool opt_replace) {
	struct ide_opt_spec_t spec;

	if (!IDE_CDROM_ParseOptSpec(spec,opts)) return false;
	return IDE_CDROM_Attach(spec.index,spec.slave,cds,opt_replace);
}

bool IDE_CDROM_Detach(signed char index,bool slave) {
	IDEATAPICDROMDevice *dev;
	IDEController *c;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return false;

	c = idecontroller[index];
	if(c == NULL)
		return false;

	if (c->device[slave?1:0] == NULL)
		return false;

	dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
	if (dev) {
		delete dev;
		c->device[slave] = NULL;
	}

	DOS_EnableDriveIDEMenu((unsigned char)index,slave);
	return true;
}

bool IDE_CDROM_Detach(const std::string &opts) {
	struct ide_opt_spec_t spec;

	if (!IDE_CDROM_ParseOptSpec(spec,opts)) return false;
	return IDE_CDROM_Detach(spec.index,spec.slave);
}

bool IDE_is_CDROM(signed char index,bool slave) {
	IDEATAPICDROMDevice *dev;
	IDEController *c;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return false;

	c = idecontroller[index];
	if(c == NULL)
		return false;

	if (c->device[slave?1:0] == NULL)
		return false;

	dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
	if (dev) return true;

	return false;
}

bool IDE_is_CDROM(const std::string &opts) {
	struct ide_opt_spec_t spec;

	if (!IDE_CDROM_ParseOptSpec(spec,opts)) return false;
	return IDE_is_CDROM(spec.index,spec.slave);
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_CDROM_Attach(signed char index,bool slave,unsigned char drive_index) {
    IDEController *c;
    IDEATAPICDROMDevice *dev;

    if (index < 0 || index >= MAX_IDE_CONTROLLERS) return;
    c = idecontroller[index];
    if(c == NULL) {
        LOG_MSG("IDE: WARNING: IDE %s controller not available. Check setting or specify another controller.", ideslot[index]);
        return;
    }

    if (c->device[slave?1:0] != NULL) {
        LOG_MSG("IDE: WARNING: IDE controller %s %s already occupied, specify another slot.",ideslot[index],master_slave[slave?1:0]);
        return;
    }

    CDROM_Interface *cdrom = NULL;

    if (!GetMSCDEXDrive(drive_index,&cdrom)) {
        LOG_MSG("IDE: Asked to attach CD-ROM that does not exist\n");
        return;
    }

    dev = new IDEATAPICDROMDevice(c,drive_index,slave,cdrom);
    if(dev == NULL) {
        LOG_MSG("IMGMOUNT: Failed to allocate CD-ROM drive %c to IDE %s %s", drive_index + 'A', ideslot[index], master_slave[slave ? 1 : 0]);
        return;
    }
    dev->mscdex = true; /* mark that the cdrom object is attached to MSCDEX emulation */
    cdrom->Release();
    c->device[slave?1:0] = (IDEDevice*)dev;
    LOG_MSG("IMGMOUNT: CD-ROM image mounted to drive %c (IDE %s %s)", drive_index + 'A', ideslot[index], master_slave[slave ? 1 : 0]);
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_CDROM_Detach(unsigned char drive_index) {
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = idecontroller[index];
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            IDEATAPICDROMDevice *dev;
            dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
            if (dev && dev->drive_index == drive_index) {
                delete dev;
                c->device[slave] = NULL;
            }
        }
    }
}

void IDE_CDROM_Detach_Ret(signed char &indexret,bool &slaveret,unsigned char drive_index) {
    indexret = -1;
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = idecontroller[index];
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            IDEATAPICDROMDevice *dev;
            dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
            if (dev && dev->drive_index == drive_index) {
                delete dev;
                c->device[slave] = NULL;
                slaveret = slave;
                indexret = index;
            }
        }
    }
}

void IDE_CDROM_DetachAll() {
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = idecontroller[index];
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            IDEATAPICDROMDevice *dev;
            dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
            if (dev) {
                delete dev;
                c->device[slave] = NULL;
            }
        }
    }
}

/* bios_disk_index = index into BIOS INT 13h disk array: imageDisk *imageDiskList[MAX_DISK_IMAGES]; */
void IDE_Hard_Disk_Attach(signed char index,bool slave,unsigned char bios_disk_index/*not INT13h, the index into DOSBox-X's BIOS drive emulation*/) {
    IDEController *c;
    IDEATADevice *dev;

    if (index < 0 || index >= MAX_IDE_CONTROLLERS) return;
    c = idecontroller[index];
    if(c == NULL) {
        LOG_MSG("IDE: WARNING: IDE %s controller not available. Check setting or specify another controller.", ideslot[index]);
        return;
    }

    if (c->device[slave?1:0] != NULL) {
        LOG_MSG("IDE: WARNING: IDE controller %s %s already occupied, specify another slot.", ideslot[index], master_slave[slave ? 1 : 0]);
        return;
    }

    if (imageDiskList[bios_disk_index] == NULL) {
        LOG_MSG("IDE: Asked to attach bios disk that does not exist\n");
        return;
    }

    dev = new IDEATADevice(c,bios_disk_index,slave);
    if (dev == NULL) return;
    dev->update_from_biosdisk();
    c->device[slave?1:0] = (IDEDevice*)dev;
    LOG_MSG("IMGMOUNT: HDD image mounted to drive no. %d (IDE %s %s)", bios_disk_index, ideslot[index], master_slave[slave?1:0]);
}

/* bios_disk_index = index into BIOS INT 13h disk array: imageDisk *imageDiskList[MAX_DISK_IMAGES]; */
void IDE_Hard_Disk_Detach(unsigned char bios_disk_index) {
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = idecontroller[index];
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            IDEATADevice *dev;
            dev = dynamic_cast<IDEATADevice*>(c->device[slave]);
            if (dev && dev->bios_disk_index == bios_disk_index) {
                delete dev;
                c->device[slave] = NULL;
            }
        }
    }
}


std::string GetIDEPosition(unsigned char bios_disk_index) {
    std::string idepos;
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = GetIDEController(index);
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            IDEATADevice *dev = dynamic_cast<IDEATADevice*>(c->device[slave]);
            if (dev && dev->bios_disk_index == bios_disk_index) {
                idepos = std::to_string(index + 1) + (slave ? 's' : 'm');
                return idepos;
            }
        }
    }
    idepos = "n/a";
    return idepos;
}

std::string GetIDEInfo() {
    std::string info;
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = GetIDEController(index);
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            //IDEATADevice *dev = dynamic_cast<IDEATADevice*>(c->device[slave]); UNUSED
            info+="IDE position "+std::to_string(index+1)+(slave?'s':'m')+": ";
            if (dynamic_cast<IDEATADevice*>(c->device[slave])) info+="disk image";
            else if (dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave])) info+="CD image";
            else info+="none";
            info+="\n";
        }
    }
    return info;
}

void Get_IDECD_drives(std::vector<int> &v) {
    for (int index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = GetIDEController(index);
        if (c) for (int slave = 0; slave < 2; slave++) {
            IDEATAPICDROMDevice* d = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
            if (d) v.push_back(d->drive_index);
        }
    }
}

static IDEController* GetIDEController(Bitu idx) {
    if (idx >= MAX_IDE_CONTROLLERS) return NULL;
    return idecontroller[idx];
}

bool isIDEControllerPresent(int8_t idx) {
    return (idecontroller[idx] != NULL);
}

static bool IDE_CPU_Is_Vm86() {
    return (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM)));
}

static void ide_baseio_w(Bitu port,Bitu val,Bitu iolen);

static Bitu IDE_SelfIO_In(IDEController *ide,Bitu port,Bitu len) {
    if (ide->int13fakev86io && IDE_CPU_Is_Vm86()) {
        /* Trigger I/O in virtual 8086 mode, where the OS can trap it and act on it.
         * Windows 95 uses V86 traps to help "autodetect" what IDE drive and port the
         * BIOS uses on INT 13h so that its internal IDE driver can take over, which
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

/* Get physical (not logical INT 13h) geometry */
bool IDE_GetPhysGeometry(unsigned char disk,uint32_t &heads,uint32_t &cyl,uint32_t &sect,uint32_t &size, uint64_t &LBA) {
    IDEController *ide;
    IDEDevice *dev;
    Bitu idx,ms;

    for (idx=0;idx < MAX_IDE_CONTROLLERS;idx++) {
        ide = GetIDEController(idx);
        if (ide == NULL) continue;

        /* for master/slave device... */
        for (ms=0;ms < 2;ms++) {
            dev = ide->device[ms];
            if (dev == NULL) continue;

            if (dev->type == IDE_TYPE_HDD) {
                IDEATADevice *ata = (IDEATADevice*)dev;
                if (ata->bios_disk_index == disk && ata->phys_heads != 0 && ata->phys_sects != 0 && ata->phys_cyls != 0) {
                    heads = ata->phys_heads;
                    sect = ata->phys_sects;
                    cyl = ata->phys_cyls;
                    LBA = ata->LBA;
                    size = 512;
                    return true;
                }
            }
        }
    }

    return false;
}



/* INT 13h extensions */
void IDE_EmuINT13DiskReadByBIOS_LBA(unsigned char disk,uint64_t lba) {
    IDEController *ide;
    IDEDevice *dev;
    Bitu idx,ms;

    if (disk < 0x80) return;
    if (lba >= (1ULL << 28ULL)) return; /* this code does not support LBA48 */

    for (idx=0;idx < MAX_IDE_CONTROLLERS;idx++) {
        ide = GetIDEController(idx);
        if (ide == NULL) continue;
        if (!ide->int13fakeio && !ide->int13fakev86io) continue;

        /* TODO: Print a warning message if the IDE controller is busy (debug/warning message) */

        /* TODO: Force IDE state to readiness, abort command, etc. */

        /* for master/slave device... */
        for (ms=0;ms < 2;ms++) {
            dev = ide->device[ms];
            if (dev == NULL) continue;

            /* TODO: Print a warning message if the IDE device is busy or in the middle of a command */

            /* TODO: Forcibly device-reset the IDE device */

            /* Issue I/O to ourself to select drive */
            dev->faked_command = true;
            IDE_SelfIO_In(ide,ide->base_io+7u,1);
            IDE_SelfIO_Out(ide,ide->base_io+6u,0x00+(ms<<4u),1);
            dev->faked_command = false;

            if (dev->type == IDE_TYPE_HDD) {
                IDEATADevice *ata = (IDEATADevice*)dev;
//              static bool int13_fix_wrap_warned = false;
                bool vm86 = IDE_CPU_Is_Vm86();

                if ((ata->bios_disk_index-2) == (disk-0x80)) {
//                  imageDisk *dsk = ata->getBIOSdisk();

                    if (ide->int13fakev86io && vm86) {
                        dev->faked_command = true;

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
                         * a legitimate BIOS INT 13h call doing its job. */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */
                        IDE_SelfIO_Out(ide,ide->base_io+6u,(ms<<4u)+0xE0u+(lba>>24u),1); /* drive and head */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */
                        IDE_SelfIO_Out(ide,ide->base_io+2u,0x01,1);  /* sector count */
                        IDE_SelfIO_Out(ide,ide->base_io+3u,lba&0xFF,1);  /* sector number */
                        IDE_SelfIO_Out(ide,ide->base_io+4u,(lba>>8u)&0xFF,1); /* cylinder lo */
                        IDE_SelfIO_Out(ide,ide->base_io+5u,(lba>>16u)&0xFF,1); /* cylinder hi */
                        IDE_SelfIO_Out(ide,ide->base_io+6u,(ms<<4u)+0xE0+(lba>>24u),1); /* drive and head */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */
                        IDE_SelfIO_Out(ide,ide->base_io+7u,0x20,1);  /* issue READ */

                        do {
                            /* TODO: Timeout needed */
                            unsigned int i = IDE_SelfIO_In(ide,ide->alt_io,1);
                            if ((i&0x80) == 0) break;
                        } while (1);
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);

                        /* for brevity assume it worked. we're here to bullshit Windows 95 after all */
                        for (unsigned int i=0;i < 256;i++)
                            IDE_SelfIO_In(ide,ide->base_io+0u,2); /* 16-bit IDE data read */

                        /* one more */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */

                        /* assume IRQ happened and clear it */
                        if (ide->IRQ >= 8)
                            IDE_SelfIO_Out(ide,0xA0,0x60u+(unsigned int)ide->IRQ-8u,1);     /* specific EOI */
                        else
                            IDE_SelfIO_Out(ide,0x20,0x60u+(unsigned int)ide->IRQ,1);       /* specific EOI */

                        ata->abort_normal();
                        dev->faked_command = false;
                    }
                    else {
                        /* hack IDE state as if a BIOS executing IDE disk routines.
                         * This is required if we want IDE emulation to work with Windows 3.11 Windows for Workgroups 32-bit disk access (WDCTRL),
                         * because the driver "tests" the controller by issuing INT 13h calls then reading back IDE registers to see if
                         * they match the C/H/S it requested */
                        dev->feature = 0x00;        /* clear error (WDCTRL test phase 5/C/13) */
                        dev->count = 0x00;      /* clear sector count (WDCTRL test phase 6/D/14) */
                        dev->lba[0] = lba&0xFF;     /* leave sector number the same (WDCTRL test phase 7/E/15) */
                        dev->lba[1] = (lba>>8u)&0xFF;    /* leave cylinder the same (WDCTRL test phase 8/F/16) */
                        dev->lba[2] = (lba>>16u)&0xFF;   /* ...ditto */
                        dev->drivehead = 0xE0u | (ms<<4u) | (lba>>24u); /* drive head and master/slave (WDCTRL test phase 9/10/17) */
                        dev->status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE; /* status (WDCTRL test phase A/11/18) */
                        dev->allow_writing = true;
                        static bool vm86_warned = false;

                        if (vm86 && !vm86_warned) {
                            LOG_MSG("IDE warning: INT 13h extensions read from virtual 8086 mode.\n");
                            LOG_MSG("             If using Windows 95 OSR2, please set int13fakev86io=true for proper 32-bit disk access\n");
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
        if (!ide->int13fakeio && !ide->int13fakev86io) continue;

        /* TODO: Print a warning message if the IDE controller is busy (debug/warning message) */

        /* TODO: Force IDE state to readiness, abort command, etc. */

        /* for master/slave device... */
        for (ms=0;ms < 2;ms++) {
            dev = ide->device[ms];
            if (dev == NULL) continue;

            /* TODO: Print a warning message if the IDE device is busy or in the middle of a command */

            /* TODO: Forcibly device-reset the IDE device */

            /* Issue I/O to ourself to select drive */
            dev->faked_command = true;
            IDE_SelfIO_In(ide,ide->base_io+7u,1);
            IDE_SelfIO_Out(ide,ide->base_io+6u,0x00u+(ms<<4u),1);
            dev->faked_command = false;

            if (dev->type == IDE_TYPE_HDD) {
                IDEATADevice *ata = (IDEATADevice*)dev;
                bool vm86 = IDE_CPU_Is_Vm86();

                if ((ata->bios_disk_index-2) == (disk-0x80)) {
                    imageDisk *dsk = ata->getBIOSdisk();

                    /* print warning if INT 13h is being called after the OS changed logical geometry */
                    if (ata->sects != ata->phys_sects || ata->heads != ata->phys_heads || ata->cyls != ata->phys_cyls)
                        LOG_MSG("INT 13 WARNING: I/O issued on drive attached to IDE emulation with changed logical geometry!\n");

                    /* HACK: src/ints/bios_disk.cpp implementation doesn't correctly
                     *       wrap sector numbers across tracks. it fulfills the read
                     *       by counting sectors and reading from C,H,S+i which means
                     *       that if the OS assumes the ability to read across track
                     *       boundaries (as Windows 95 does) we will get invalid
                     *       sector numbers, which in turn fouls up our emulation.
                     *
                     *       Windows 95 OSR2 for example, will happily ask for 63
                     *       sectors starting at C/H/S 30/9/42 without regard for
                     *       track boundaries. */
                    if (sect > dsk->sectors) {
#if 0 /* this warning is pointless */
                        static bool int13_fix_wrap_warned = false;
                        if (!int13_fix_wrap_warned) {
                            LOG_MSG("INT 13h implementation warning: we were given over-large sector number.\n");
                            LOG_MSG("This is normally the fault of DOSBox INT 13h emulation that counts sectors\n");
                            LOG_MSG("without consideration of track boundaries\n");
                            int13_fix_wrap_warned = true;
                        }
#endif

                        do {
                            sect -= dsk->sectors;
                            if ((++head) >= dsk->heads) {
                                head -= dsk->heads;
                                cyl++;
                            }
                        } while (sect > dsk->sectors);
                    }

                    /* translate BIOS INT 13h geometry to IDE geometry */
                    if (ata->geo_translate) {
                        unsigned long lba;

                        if (dsk == NULL) return;
                        lba = (head * dsk->sectors) + (cyl * dsk->sectors * dsk->heads) + sect - 1;
                        sect = (lba % ata->sects) + 1;
                        head = (lba / ata->sects) % ata->heads;
                        cyl = (lba / ata->sects / ata->heads);
                    }

                    if (ide->int13fakev86io && vm86) {
                        dev->faked_command = true;

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
                         * a legitimate BIOS INT 13h call doing its job. */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */
                        IDE_SelfIO_Out(ide,ide->base_io+6u,(ms<<4u)+0xA0u+head,1); /* drive and head */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */
                        IDE_SelfIO_Out(ide,ide->base_io+2u,0x01,1);  /* sector count */
                        IDE_SelfIO_Out(ide,ide->base_io+3u,sect,1);  /* sector number */
                        IDE_SelfIO_Out(ide,ide->base_io+4u,cyl&0xFF,1);  /* cylinder lo */
                        IDE_SelfIO_Out(ide,ide->base_io+5u,(cyl>>8u)&0xFF,1); /* cylinder hi */
                        IDE_SelfIO_Out(ide,ide->base_io+6u,(ms<<4u)+0xA0u+head,1); /* drive and head */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */
                        IDE_SelfIO_Out(ide,ide->base_io+7u,0x20u,1);  /* issue READ */

                        do {
                            /* TODO: Timeout needed */
                            unsigned int i = IDE_SelfIO_In(ide,ide->alt_io,1);
                            if ((i&0x80) == 0) break;
                        } while (1);
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);

                        /* for brevity assume it worked. we're here to bullshit Windows 95 after all */
                        for (unsigned int i=0;i < 256;i++)
                            IDE_SelfIO_In(ide,ide->base_io+0,2); /* 16-bit IDE data read */

                        /* one more */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);        /* dum de dum reading status */

                        /* assume IRQ happened and clear it */
                        if (ide->IRQ >= 8)
                            IDE_SelfIO_Out(ide,0xA0,0x60u+(unsigned int)ide->IRQ-8u,1);     /* specific EOI */
                        else
                            IDE_SelfIO_Out(ide,0x20,0x60u+(unsigned int)ide->IRQ,1);       /* specific EOI */

                        ata->abort_normal();
                        dev->faked_command = false;
                    }
                    else {
                        /* hack IDE state as if a BIOS executing IDE disk routines.
                         * This is required if we want IDE emulation to work with Windows 3.11 Windows for Workgroups 32-bit disk access (WDCTRL),
                         * because the driver "tests" the controller by issuing INT 13h calls then reading back IDE registers to see if
                         * they match the C/H/S it requested */
                        dev->feature = 0x00;        /* clear error (WDCTRL test phase 5/C/13) */
                        dev->count = 0x00;      /* clear sector count (WDCTRL test phase 6/D/14) */
                        dev->lba[0] = sect;     /* leave sector number the same (WDCTRL test phase 7/E/15) */
                        dev->lba[1] = cyl;      /* leave cylinder the same (WDCTRL test phase 8/F/16) */
                        dev->lba[2] = cyl >> 8u;     /* ...ditto */
                        dev->drivehead = 0xA0u | (ms<<4u) | head; /* drive head and master/slave (WDCTRL test phase 9/10/17) */
                        dev->status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE; /* status (WDCTRL test phase A/11/18) */
                        dev->allow_writing = true;
                        static bool vm86_warned = false;

                        if (vm86 && !vm86_warned) {
                            LOG_MSG("IDE warning: INT 13h read from virtual 8086 mode.\n");
                            LOG_MSG("             If using Windows 95, please set int13fakev86io=true for proper 32-bit disk access\n");
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

/* BIOS hard disk access to a disk attached to IDE emulation (see ide.h) */
static IDEATADevice *IDE_FindBIOSDisk(unsigned int disk_index) {
    for (unsigned int idx=0;idx < MAX_IDE_CONTROLLERS;idx++) {
        IDEController *ide = GetIDEController(idx);
        if (ide == NULL) continue;

        for (unsigned int ms=0;ms < 2;ms++) {
            IDEDevice *dev = ide->device[ms];
            if (dev == NULL || dev->type != IDE_TYPE_HDD) continue;

            IDEATADevice *ata = (IDEATADevice*)dev;
            if (ata->bios_disk_index == disk_index) return ata;
        }
    }

    return NULL;
}

bool IDE_BIOS_DiskAttached(unsigned int disk_index) {
    return IDE_FindBIOSDisk(disk_index) != NULL;
}

bool IDE_BIOS_DiskAccess(unsigned int disk_index,uint64_t lba,unsigned int count,IDEBiosDiskOp op) {
    IDEATADevice *ata = IDE_FindBIOSDisk(disk_index);
    if (ata == NULL) return false;

    const double done = ata->bios_access(lba,count,op,PIC_FullIndex());

    /* the BIOS waits for the drive with interrupts enabled */
    if (done > PIC_FullIndex()) {
        CPU_STI();
        while (PIC_FullIndex() < done)
            CALLBACK_Idle();
    }

    return true;
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
        if (!ide->int13fakeio && !ide->int13fakev86io) continue;

        /* TODO: Print a warning message if the IDE controller is busy (debug/warning message) */

        /* TODO: Force IDE state to readiness, abort command, etc. */

        /* for master/slave device... */
        for (ms=0;ms < 2;ms++) {
            dev = ide->device[ms];
            if (dev == NULL) continue;

            /* TODO: Print a warning message if the IDE device is busy or in the middle of a command */

            /* TODO: Forcibly device-reset the IDE device */

            /* Issue I/O to ourself to select drive */
            IDE_SelfIO_In(ide,ide->base_io+7u,1);
            IDE_SelfIO_Out(ide,ide->base_io+6u,0x00+(ms<<4u),1);

            /* TODO: Forcibly device-reset the IDE device */

            if (dev->type == IDE_TYPE_HDD) {
                IDEATADevice *ata = (IDEATADevice*)dev;

                if ((ata->bios_disk_index-2) == (disk-0x80)) {
                    LOG_MSG("IDE %d%c reset by BIOS disk 0x%02x\n",
                        (unsigned int)(idx+1),
                        ms?'s':'m',
                        (unsigned int)disk);

                    if (ide->int13fakev86io && IDE_CPU_Is_Vm86()) {
                        /* issue the DEVICE RESET command */
                        IDE_SelfIO_In(ide,ide->base_io+7u,1);
                        IDE_SelfIO_Out(ide,ide->base_io+7u,0x08,1);

                        IDE_SelfIO_In(ide,ide->base_io+7u,1);

                        /* assume IRQ happened and clear it */
                        if (ide->IRQ >= 8)
                            IDE_SelfIO_Out(ide,0xA0,0x60u+(unsigned int)ide->IRQ-8u,1);     /* specific EOI */
                        else
                            IDE_SelfIO_Out(ide,0x20,0x60u+(unsigned int)ide->IRQ,1);       /* specific EOI */
                    }
                    else {
                        /* Windows 3.1 WDCTRL needs this, or else, it will read the
                         * status register and see something other than DRIVE_READY|SEEK_COMPLETE */
                        dev->writecommand(0x08);

                        /* and then immediately clear the IRQ */
                        if (ide->device[ide->select] != NULL)
                            ide->device[ide->select]->lower_irq();

                        ide->check_device_irq();
                    }
                }
            }
        }
    }
}

static void IDE_DelayedCommand(Bitu pk/*which IDE device*/) {
    IDEEventPack ep(pk);
    const unsigned int idx = ep.interface();
    const unsigned int devidx = ep.device();

    IDEController *ctrl = GetIDEController(idx);
    if (ctrl == NULL) return;

    IDEDevice *dev = ctrl->device[devidx];
    if (dev == NULL) return;

    if (dev->type == IDE_TYPE_HDD) {
        ((IDEATADevice*)dev)->delayed_command();
    }
    else if (dev->type == IDE_TYPE_CDROM) {
        IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

        if (dev->state == IDE_DEV_ATAPI_BUSY) {
            switch (dev->command) {
                case 0xA0:/*ATAPI PACKET*/
                    atapi->on_atapi_busy_time();
                    break;
                default:
                    LOG_MSG("Unknown delayed IDE/ATAPI busy wait command\n");
                    dev->abort_error();
                    dev->raise_irq();
                    break;
            }
        }
        else {
            switch (dev->command) {
                case 0xA0:/*ATAPI PACKET*/
                    dev->state = IDE_DEV_ATAPI_PACKET_COMMAND;
                    dev->status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE|IDE_STATUS_DRQ;
                    dev->count = 0x01;  /* input/output == 0, command/data == 1 */
                    atapi->atapi_cmd_total = 12; /* NTS: do NOT raise IRQ */
                    atapi->atapi_cmd_i = 0;
                    break;
                case 0xA1:/*IDENTIFY PACKET DEVICE (CONTINUED) */
                    dev->state = IDE_DEV_DATA_READ;
                    dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
                    atapi->generate_identify_device();
                    atapi->prepare_read(0,512);
                    dev->raise_irq();
                    break;
                default:
                    LOG_MSG("Unknown delayed IDE/ATAPI command\n");
                    dev->abort_error();
                    dev->raise_irq();
                    break;
            }
        }
    }
    else {
        LOG_MSG("Unknown delayed command\n");
        dev->abort_error();
        dev->raise_irq();
    }
}

void PC98_IDE_UpdateIRQ(void);

void IDEController::check_device_irq() {
    IDEDevice* dev = device[select];
    bool sig = false;

    if (dev) sig = dev->irq_signal && interrupt_enable;

    if (irq_pending != sig) {
        if (sig) raise_irq();
        else lower_irq();
    }
}

void IDEController::raise_irq() {
    irq_pending = true;
    if (IS_PC98_ARCH) {
        PC98_IDE_UpdateIRQ();
    }
    else {
        if (IRQ >= 0) PIC_ActivateIRQ((unsigned int)IRQ);
    }
}

void IDEController::lower_irq() {
    irq_pending = false;
    if (IS_PC98_ARCH) {
        PC98_IDE_UpdateIRQ();
    }
    else {
        if (IRQ >= 0) PIC_DeActivateIRQ((unsigned int)IRQ);
    }
}

unsigned int pc98_ide_select = 0;

IDEController *match_ide_controller(Bitu port) {
    if (IS_PC98_ARCH) {
        return idecontroller[pc98_ide_select];
    }
    else {
        for (unsigned int i=0;i < MAX_IDE_CONTROLLERS;i++) {
            IDEController *ide = idecontroller[i];
            if (ide == NULL) continue;
            if (ide->base_io != 0U && ide->base_io == (port&0xFFF8U)) return ide;
            if (ide->alt_io != 0U && ide->alt_io == (port&0xFFFEU)) return ide;
        }
    }

    return NULL;
}

void IDEDevice::raise_irq() {
    if (!irq_signal) {
        irq_signal = true;
        controller->check_device_irq();
    }
}

void IDEDevice::lower_irq() {
    if (irq_signal) {
        irq_signal = false;
        controller->check_device_irq();
    }
}

Bitu IDEDevice::data_read(Bitu iolen) {
    (void)iolen;//UNUSED
    return 0xAAAAU;
}

void IDEDevice::data_write(Bitu v,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)v;//UNUSED
}

IDEDevice::IDEDevice(IDEController *c,bool _slave) {
    type = IDE_TYPE_NONE;
    slave = _slave;
    controller = c;
    asleep = false;
    motor_on = true;
    irq_signal = false;
    allow_writing = true;
    state = IDE_DEV_READY;
    feature = count = lba[0] = lba[1] = lba[2] = command = drivehead = 0;
    hob_feature = hob_count = hob_lba[0] = hob_lba[1] = hob_lba[2] = 0;
    status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;

    faked_command = false;
    ide_select_delay = 0.5; /* 500us */
    ide_spinup_delay = 3000; /* 3 seconds */
    ide_spindown_delay = 1000; /* 1 second */
    ide_identify_command_delay = 0.01; /* 10us */
}

/* the registers identify the kind of device after a reset or diagnostics:
 * ATA count=1 sector=1 cylinder=0000h, ATAPI cylinder=EB14h */
void IDEDevice::set_signature() {
    count = 0x01;
    lba[0] = 0x01;
    lba[1] = 0x00;
    lba[2] = 0x00;
    drivehead &= 0x10;
}

/* EXECUTE DEVICE DIAGNOSTIC: the device passes (error register 01h) and shows its signature */
void IDEDevice::execute_device_diagnostic() {
    PIC_RemoveSpecificEvents(IDE_DelayedCommand,IDEEventPack(controller->interface_index,slave?1u:0u).get());
    state = IDE_DEV_READY;
    allow_writing = true;
    command = 0x90;
    set_signature();
    feature = 0x01;
    status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::abort_with_error(uint8_t err) {
    abort_error();
    feature = err;
    raise_irq();
}

/* IDE controller -> upon writing bit 2 of alt (0x3F6) */
void IDEDevice::host_reset_complete() {
    status = 0x00;
    asleep = false;
    allow_writing = true;
    state = IDE_DEV_READY;
    set_signature();

    /* device passed, according to ATA standard regarding result of diagnostics test. */
    /* NTS: The Linux kernel WILL NOT talk to this IDE emulation unless we signal success through
     *      the feature/error register. If we don't respond this way, then according to the
     *      source code, it considers what we provide a possibly phantom drive. See Linux kernel
     *      6.1.29 drivers/ata/libata-sff.c, functions ata_sff_softreset, ata_sff_dev_classify.
     *      Another side effect of failing to set feature is that the Linux kernel will talk to
     *      our hard drive emulation though with a warning about diagnostics failure, but will
     *      then completely ignore any ATAPI CD-ROM emulation we provide.
     *
     *      Windows XP never issues a reset or DIAGNOSTIC to the drive and doesn't care, apparently. */
    feature = 0x01;

    status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::host_reset_begin() {
    status = 0xFF;
    asleep = false;
    allow_writing = true;
    state = IDE_DEV_BUSY;
}

IDEDevice::~IDEDevice() {
}

void IDEDevice::abort_silent() {
    assert(controller != NULL);

    /* a command was written while another is in progress */
    state = IDE_DEV_READY;
    allow_writing = true;
    command = 0x00;
    status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::abort_error() {
    assert(controller != NULL);
    LOG_MSG("IDE abort dh=0x%02x with error on 0x%03x\n",drivehead,controller->base_io);

    /* a command was written while another is in progress */
    state = IDE_DEV_READY;
    allow_writing = true;
    command = 0x00;
    feature = 0x04; /* ABRT */
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

bool IDEDevice::command_interruption_ok(uint8_t cmd) {
    /* apparently this is OK, if the Linux kernel is doing it:
     * writing the same command byte as the one in progress, OR, issuing
     * Device Reset while another command is waiting for data read/write */
    if (cmd == command) return true;
    if (state != IDE_DEV_READY && state != IDE_DEV_BUSY && cmd == 0x08) {
        LOG_MSG("Device reset while another (%02x) is in progress (state=%u). Aborting current command to begin another\n",command,state);
        abort_silent();
        return true;
    }

    if (state != IDE_DEV_READY) {
        LOG_MSG("Command %02x written while another (%02x) is in progress (state=%u). Aborting current command\n",cmd,command,state);
        abort_error();
        return false;
    }

    return true;
}

void IDEDevice::writecommand(uint8_t cmd) {
    if (!command_interruption_ok(cmd))
        return;

    /* if the drive is asleep, then writing a command wakes it up */
    interface_wakeup();

    /* drive is ready to accept command */
    switch (cmd) {
        default:
            LOG_MSG("Unknown IDE command %02X\n",cmd);
            abort_error();
            break;
    }
}

void IDEATAPICDROMDevice::writecommand(uint8_t cmd) {
    const unsigned int pk = IDEEventPack(controller->interface_index,slave?1u:0u).get();

    if (!command_interruption_ok(cmd))
        return;

    /* a device in sleep mode only responds to a reset */
    if (power_state == 4 && cmd != 0x08) {
        command = cmd;
        abort_with_error(0x04/*ABRT*/);
        return;
    }

    /* if the drive is asleep, then writing a command wakes it up */
    interface_wakeup();

    /* drive is ready to accept command */
    allow_writing = false;
    command = cmd;
    switch (cmd) {
        case 0x08: /* DEVICE RESET */
            status = 0x00;
            drivehead &= 0x10;
            count = 0x01;
            lba[0] = 0x01;
            feature = 0x01;
            lba[1] = 0x14;  /* <- magic ATAPI identification */
            lba[2] = 0xEB;
            /* NTS: Testing suggests that ATAPI devices do NOT trigger an IRQ on receipt of this command */
            allow_writing = true;
            break;
        case 0x20: /* READ SECTOR */
            abort_normal();
            status = IDE_STATUS_ERROR|IDE_STATUS_DRIVE_READY;
            drivehead &= 0x30;
            count = 0x01;
            lba[0] = 0x01;
            feature = 0x04; /* abort */
            lba[1] = 0x14;  /* <- magic ATAPI identification */
            lba[2] = 0xEB;
            raise_irq();
            allow_writing = true;
            break;
        case 0xA0: /* ATAPI PACKET */
            if (feature & 1) {
                /* this code does not support DMA packet commands */
                LOG_MSG("Attempted DMA transfer\n");
                abort_error();
                count = 0x03; /* no more data (command/data=1, input/output=1) */
                feature = 0x04; /* ABRT */
                raise_irq();
            }
            else {
                state = IDE_DEV_BUSY;
                status = IDE_STATUS_BUSY;
                atapi_to_host = (feature >> 2) & 1; /* 0=to device 1=to host */
                host_maximum_byte_count = ((unsigned int)lba[2] << 8) + (unsigned int)lba[1]; /* LBA field bits 23:8 are byte count */
                if (host_maximum_byte_count == 0) host_maximum_byte_count = 0x10000UL;
                PIC_RemoveSpecificEvents(IDE_DelayedCommand,pk);
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 0.25)/*ms*/,pk);
            }
            break;
        case 0xA1: /* IDENTIFY PACKET DEVICE */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_RemoveSpecificEvents(IDE_DelayedCommand,pk);
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : ide_identify_command_delay),pk);
            break;
        case 0xEC: /* IDENTIFY DEVICE */
            /* "devices that implement the PACKET command set shall post command aborted and place PACKET command feature
               set in the appropriate fields". We have to do this. Unlike OAKCDROM.SYS Windows 95 appears to autodetect
               IDE devices by what they do when they're sent command 0xEC out of the blue---Microsoft didn't write their
               IDE drivers to use command 0x08 DEVICE RESET. */
            abort_normal();
            status = IDE_STATUS_ERROR|IDE_STATUS_DRIVE_READY;
            drivehead &= 0x30;
            count = 0x01;
            lba[0] = 0x01;
            feature = 0x04; /* abort */
            lba[1] = 0x14;  /* <- magic ATAPI identification */
            lba[2] = 0xEB;
            raise_irq();
            allow_writing = true;
            break;
        case 0xEF: /* SET FEATURES */
            if (feature == 0x66/*Disable reverting to power on defaults*/ ||
                feature == 0xCC/*Enable reverting to power on defaults*/ ||
                feature == 0x03/*Set transfer mode according to sector count register (required by Linux kernel)*/) {
                /* ignore */
                status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
                state = IDE_DEV_READY;
            }
            else {
                LOG_MSG("SET FEATURES %02xh SC=%02x SN=%02x CL=%02x CH=%02x",feature,count,lba[0],lba[1],lba[2]);
                abort_error();
            }
            allow_writing = true;
            raise_irq();
            break;
        case 0xE0: case 0x94: /* STANDBY IMMEDIATE */
        case 0xE6: case 0x99: /* SLEEP */
            /* the disc stops */
            if (loading_mode == LOAD_READY || loading_mode == LOAD_DISC_READIED) {
                loading_mode = LOAD_IDLE;
                rb_valid = false;
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,pk);
            }
            power_state = (cmd == 0xE6 || cmd == 0x99) ? 4 : 3;
            power_event = 0x02;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        case 0xE1: case 0x95: /* IDLE IMMEDIATE */
            power_state = 2;
            power_event = 0x02;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        case 0xE5: case 0x98: /* CHECK POWER MODE */
            /* sector count: 00h standby (disc stopped), 80h idle, FFh active */
            if (loading_mode == LOAD_IDLE || loading_mode == LOAD_NO_DISC || power_state >= 3) count = 0x00;
            else if (power_state == 2) count = 0x80;
            else count = 0xFF;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        default:
            LOG_MSG("Unknown IDE/ATAPI command %02X\n",cmd);
            abort_error();
            allow_writing = true;
            count = 0x03; /* no more data (command/data=1, input/output=1) */
            feature = 0x04; /* ABRT */
            raise_irq();
            break;
    }
}

void IDEATADevice::writecommand(uint8_t cmd) {
    if (!command_interruption_ok(cmd))
        return;

    /* a device in sleep mode only responds to a reset */
    if (power == PWR_SLEEP) {
        LOG(LOG_MISC,LOG_DEBUG)("IDE ATA: command %02x while asleep, aborted (needs a reset)",cmd);
        command = cmd;
        abort_with_error(0x04/*ABRT*/);
        return;
    }

    /* if the drive is asleep, then writing a command wakes it up */
    interface_wakeup();

    const double now = PIC_FullIndex();
    const double overhead = (instant || faked_command) ? 0.00001 : IDE_HD_COMMAND_MS;

    /* drive is ready to accept command */
    allow_writing = false;
    command = cmd;
    switch (cmd) {
        case 0x00: /* NOP */
            abort_with_error(0x04/*ABRT*/);
            break;
        case 0x08: /* DEVICE RESET */
            /* NTS: This is a PACKET device command. Hard disks are supposed to abort it, but
             *      testing suggests ATA hard drives fire an IRQ and report ready. In fact, Windows 95
             *      won't detect hard drives that don't fire an IRQ in response */
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            drivehead &= 0x10;
            count = 0x01; lba[0] = 0x01; feature = 0x00;
            lba[1] = lba[2] = 0;
            raise_irq();
            allow_writing = true;
            break;
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: /* RECALIBRATE (1xh) */
        case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
            /* "if the command is executed in CHS mode, then ... sector number register shall be 1.
             *  if executed in LAB mode, then ... sector number register shall be 0" */
            if (drivehead & 0x40) lba[0] = 0x00;
            else lba[0] = 0x01;
            drivehead &= 0x10;
            lba[1] = lba[2] = 0;
            feature = 0x00;
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            {
                double t = spin_up(now + overhead);
                if (!instant && !faked_command) t += seek_ms(0);
                head_cyl = 0;
                ra_valid = false;
                schedule(t);
            }
            break;
        case 0x20: case 0x21: /* READ SECTOR(S) (with/without retry) */
        case 0x24: /* READ SECTOR(S) EXT */
        case 0xC4: /* READ MULTIPLE */
        case 0x29: /* READ MULTIPLE EXT */
        case 0x40: case 0x41: /* READ VERIFY SECTOR(S) (with/without retry) */
        case 0x42: /* READ VERIFY SECTOR(S) EXT */
            if ((cmd == 0xC4 || cmd == 0x29) && multiple_sector_count == 0) {
                abort_with_error(0x04/*ABRT*/);
                break;
            }
            if (!decode_address(command_is_ext() || cmd == 0x42,false)) {
                if ((command_is_ext() || cmd == 0x42) && !int13_enable_48bitLBA) abort_with_error(0x04/*ABRT*/);
                else abort_with_error(0x10/*IDNF*/);
                break;
            }
            if (cmd == 0x42) cmd_ext = true;
            if (cur_lba >= LBA) {
                sector_error(0x10/*IDNF*/);
                break;
            }
            progress_count = 0;
            cur_block = block_count();
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            schedule(media_access(cur_lba,cur_block,false,now + overhead));
            break;
        case 0x30: case 0x31: /* WRITE SECTOR(S) (with/without retry) */
        case 0x34: /* WRITE SECTOR(S) EXT */
        case 0xC5: /* WRITE MULTIPLE */
        case 0x39: /* WRITE MULTIPLE EXT */
            if ((cmd == 0xC5 || cmd == 0x39) && multiple_sector_count == 0) {
                abort_with_error(0x04/*ABRT*/);
                break;
            }
            if (!decode_address(command_is_ext(),false)) {
                if (command_is_ext() && !int13_enable_48bitLBA) abort_with_error(0x04/*ABRT*/);
                else abort_with_error(0x10/*IDNF*/);
                break;
            }
            if (cur_lba >= LBA) {
                sector_error(0x10/*IDNF*/);
                break;
            }
            /* the drive does NOT signal an interrupt. it sets DRQ and waits for the first data
             * block to be transferred to it before writing */
            progress_count = 0;
            cur_block = block_count();
            state = IDE_DEV_DATA_WRITE;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ;
            prepare_write(0,512u * cur_block);
            break;
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: /* SEEK (7xh) */
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
            if (!decode_address(false,true)) {
                abort_with_error(0x10/*IDNF*/);
                break;
            }
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            {
                double t = spin_up(now + overhead);
                const uint64_t cyl = (cur_lba / media_spt) / media_heads;
                if (!instant && !faked_command) t += seek_ms(cyl);
                head_cyl = cyl;
                note_activity(t);
                schedule(t);
            }
            break;
        case 0x91: /* INITIALIZE DEVICE PARAMETERS */
            if ((unsigned int)count != (unsigned int)sects || (unsigned int)((drivehead&0xF)+1) != (unsigned int)heads) {
                if (count == 0) {
                    LOG_MSG("IDE warning: OS attempted to change geometry to invalid H/S %u/%u\n",
                        count,(drivehead&0xF)+1);
                    abort_with_error(0x04/*ABRT*/);
                    return;
                }
                else {
                    unsigned int ncyls;

                    ncyls = (phys_cyls * phys_heads * phys_sects);
                    ncyls += (count * ((unsigned int)(drivehead&0xF)+1u)) - 1u;
                    ncyls /= count * ((unsigned int)(drivehead&0xF)+1u);

                    /* the OS is changing logical disk geometry, so update our head/sector count (needed for Windows ME) */
                    LOG_MSG("IDE warning: OS is changing logical geometry from C/H/S %u/%u/%u to logical H/S %u/%u/%u\n",
                        (int)cyls,(int)heads,(int)sects,
                        (int)ncyls,(int)((drivehead&0xF)+1),(int)count);
                    LOG_MSG("             Compatibility issues may occur if the OS tries to use INT 13 at the same time!\n");

                    cyls = ncyls;
                    sects = count;
                    heads = (drivehead&0xFu)+1u;
                }
            }

            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            allow_writing = true;
            raise_irq(); // NTS: The Linux kernel will pause for up to 30 seconds waiting for this command to issue an IRQ if we don't do this
            break;
        case 0xC6: /* SET MULTIPLE MODE */
            /* only sector counts 1, 2, 4, 8, 16, 32, 64, and 128 are legal by standard.
             * NTS: There's a bug in VirtualBox that makes 0 legal too! */
            if (count != 0 && count <= multiple_sector_max && is_power_of_2(count)) {
                multiple_sector_count = count;
                status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
                state = IDE_DEV_READY;
                allow_writing = true;
                raise_irq();
            }
            else {
                abort_with_error(0x04/*ABRT*/);
            }
            break;
        case 0xA0:/*ATAPI PACKET*/
            /* We're not an ATAPI packet device!
             * Windows 95 seems to issue this at startup to hard drives. Duh. */
            /* fall through */
        case 0xA1: /* IDENTIFY PACKET DEVICE */
            /* We are not an ATAPI packet device.
             * Most MS-DOS drivers and Windows 95 like to issue both IDENTIFY ATA and IDENTIFY ATAPI commands.
             * I also gather from some contributors on the github comments that people think our "Unknown IDE/ATA command"
             * error message is part of some other error in the emulation. Rather than put up with that, we'll just
             * silently abort the command with an error. The registers keep the ATA signature. */
            abort_normal();
            status = IDE_STATUS_ERROR|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            drivehead &= 0x30;
            count = 0x01;
            lba[0] = 0x01;
            feature = 0x04; /* abort */
            lba[1] = 0x00;
            lba[2] = 0x00;
            raise_irq();
            allow_writing = true;
            break;
        case 0xE7: /* FLUSH CACHE */
        case 0xEA: /* FLUSH CACHE EXT */
            /* NTS: Windows 2000 and Windows XP like this command a lot. They REALLY REALLY like
             *      to issue this command a lot, especially during the install phase. */
            /* done when the write cache has been written to the disk */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            schedule(mech_free_at > now + overhead ? mech_free_at : now + overhead);
            break;
        case 0xE0: case 0x94: /* STANDBY IMMEDIATE */
        case 0xE2: case 0x96: /* STANDBY (sector count = standby timer) */
        case 0xE6: case 0x99: /* SLEEP */
            if (cmd == 0xE2 || cmd == 0x96) set_standby_timer((uint8_t)count);
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            {
                /* the write cache is written first, then the disk spins down */
                double t = mech_free_at > now + overhead ? mech_free_at : now + overhead;
                if (spinning && !instant && !faked_command) t += spindown_ms;
                spin_down(t,cmd == 0xE6 || cmd == 0x99);
                schedule(t);
            }
            break;
        case 0xE1: case 0x95: /* IDLE IMMEDIATE */
        case 0xE3: case 0x97: /* IDLE (sector count = standby timer) */
            if (cmd == 0xE3 || cmd == 0x97) set_standby_timer((uint8_t)count);
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            {
                /* idle: the disk spins, the interface is ready */
                const double t = spin_up(now + overhead);
                power = PWR_IDLE;
                note_activity(t);
                schedule(t);
            }
            break;
        case 0xE5: case 0x98: /* CHECK POWER MODE */
            /* sector count: 00h standby (or spinning down/up), 80h idle, FFh active or idle */
            if (!spinning || now < spin_ready_at) count = 0x00;
            else if (power == PWR_IDLE) count = 0x80;
            else count = 0xFF;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        case 0xEC: /* IDENTIFY DEVICE */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            schedule(now + ((faked_command || instant) ? 0.000001 : ide_identify_command_delay));
            break;
        case 0xEF: /* SET FEATURES */
            switch (feature & 0xFFu) {
                case 0x02: write_cache = true; break;           /* enable write cache */
                case 0x82: write_cache = false; break;          /* disable write cache */
                case 0xAA: read_lookahead = true; break;        /* enable read look-ahead */
                case 0x55: read_lookahead = false; ra_valid = false; break; /* disable read look-ahead */
                case 0x66: /* Disable reverting to power on defaults */
                case 0xCC: /* Enable reverting to power on defaults */
                case 0x03: /* Set transfer mode according to sector count register (required by Linux kernel) */
                    break;
                default:
                    LOG_MSG("SET FEATURES %02xh SC=%02x SN=%02x CL=%02x CH=%02x",feature,count,lba[0],lba[1],lba[2]);
                    abort_with_error(0x04/*ABRT*/);
                    return;
            }
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        case 0xF8: /* READ NATIVE MAX ADDRESS */
            if (!(drivehead & 0x40)) {
                abort_with_error(0x04/*ABRT*/);
                break;
            }
            cmd_ext = false;
            cmd_lba = true;
            update_address_registers(LBA > 0x10000000ull ? 0x0FFFFFFFull : LBA - 1u);
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        case 0x27: /* READ NATIVE MAX ADDRESS EXT */
            if (!int13_enable_48bitLBA || !(drivehead & 0x40)) {
                abort_with_error(0x04/*ABRT*/);
                break;
            }
            cmd_ext = true;
            cmd_lba = true;
            update_address_registers(LBA - 1u);
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            state = IDE_DEV_READY;
            allow_writing = true;
            raise_irq();
            break;
        default:
            LOG_MSG("Unknown IDE/ATA command %02X\n",cmd);
            abort_with_error(0x04/*ABRT*/);
            break;
    }
}

void IDEDevice::deselect() {
}

/* the hard disk or CD-ROM class override of this member is responsible for checking
   the head value and clamping within range if C/H/S mode is selected */
void IDEDevice::select(uint8_t ndh,bool switched_to) {
    (void)switched_to;//UNUSED
    (void)ndh;//UNUSED
    /* NTS: I thought there was some delay between selecting a drive and sending a command.
        Apparently I was wrong. */
    if (allow_writing) drivehead = ndh;
//  status = (!asleep)?(IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE):0;
//  allow_writing = !asleep;
//  state = IDE_DEV_READY;
}

IDEController::IDEController(Section* configuration,unsigned char index):Module_base(configuration){
    Section_prop * section=static_cast<Section_prop *>(configuration);
    int i;

    register_pnp = section->Get_bool("pnp");
    int13fakeio = section->Get_bool("int13fakeio");
    int13fakev86io = section->Get_bool("int13fakev86io");
    enable_pio32 = section->Get_bool("enable pio32");
    ignore_pio32 = section->Get_bool("ignore pio32");
    spinup_time = section->Get_int("cd-rom spinup time");
    spindown_timeout = section->Get_int("cd-rom spindown timeout");
    cd_insertion_time = section->Get_int("cd-rom insertion delay");
    cd_instant = section->Get_bool("cd-rom instant mode");
    cd_max_speed = section->Get_int("cd-rom max speed");
    cd_tray_time = section->Get_int("cd-rom tray time");
    hd_instant = section->Get_bool("hard disk instant mode");
    hd_rpm = section->Get_int("hard disk rpm");
    hd_spinup_time = section->Get_int("hard disk spinup time");

    host_reset = false;
    hob = false;
    irq_pending = false;
    interrupt_enable = true;
    interface_index = index;
    device[0] = NULL;
    device[1] = NULL;
    base_io = 0;
    select = 0;
    alt_io = 0;
    IRQ = -1;

    i = section->Get_int("irq");
    if (i > 0 && i <= 15) IRQ = i;

    i = section->Get_hex("io");
    if (i >= 0x100 && i <= 0x3FF) base_io = (unsigned int)(i & ~7);

    i = section->Get_hex("altio");
    if (i >= 0x100 && i <= 0x3FF) alt_io = (unsigned int)(i & ~1);

    if (IS_PC98_ARCH) {
        /* PC-98 evidently maps all IDE controllers to one set of I/O ports and allows
         * "bank switching" between them through I/O ports 430h-435h. This is also why
         * the IDE emulation will NOT register I/O ports per controller in PC-98 mode.
         * If PC-98 supports IDE controllers outside this scheme, then that shall be
         * implemented later. */
        IRQ = 9; // INT 8+9 = 17 = 11h
        alt_io = 0x74C; // even
        base_io = 0x640; // even
    }
    else if (index < sizeof(IDE_default_IRQs)) {
        if (IRQ < 0) IRQ = IDE_default_IRQs[index];
        if (alt_io == 0) alt_io = IDE_default_alts[index];
        if (base_io == 0) base_io = IDE_default_bases[index];
    }
    else {
        if (IRQ < 0 || alt_io == 0 || base_io == 0)
            LOG_MSG("WARNING: IDE interface %u: Insufficient resources assigned by dosbox-x.conf, and no appropriate default resources for this interface.",index);
    }
}

void IDEController::register_isapnp() {
    if (IS_PC98_ARCH)
        return;

    if (register_pnp && base_io > 0 && alt_io > 0) {
        unsigned char tmp[256];
        unsigned int i;

        const unsigned char h1[9] = {
            ISAPNP_SYSDEV_HEADER(
                ISAPNP_ID('P','N','P',0x0,0x6,0x0,0x0), /* PNP0600 Generic ESDI/IDE/ATA compatible hard disk controller */
                ISAPNP_TYPE(0x01,0x01,0x00),        /* type: Mass Storage Device / IDE / Generic */
                0x0001 | 0x0002)
        };

        i = 0;
        memcpy(tmp+i,h1,9); i += 9;         /* can't disable, can't configure */
        /*----------allocated--------*/
        tmp[i+0] = (8 << 3) | 7;            /* IO resource */
        tmp[i+1] = 0x01;                /* 16-bit decode */
        host_writew(tmp+i+2,base_io);           /* min */
        host_writew(tmp+i+4,base_io);           /* max */
        tmp[i+6] = 0x08;                /* align */
        tmp[i+7] = 0x08;                /* length */
        i += 7+1;

        tmp[i+0] = (8 << 3) | 7;            /* IO resource */
        tmp[i+1] = 0x01;                /* 16-bit decode */
        host_writew(tmp+i+2,alt_io);            /* min */
        host_writew(tmp+i+4,alt_io);            /* max */
        tmp[i+6] = 0x01;                /* align */
        if (alt_io == 0x3F6 && fdc_takes_port_3F7())
            tmp[i+7] = 0x01;            /* length 1 (so as not to conflict with floppy controller at 0x3F7) */
        else
            tmp[i+7] = 0x02;            /* length 2 */
        i += 7+1;

        if (IRQ > 0) {
            /* NTS: Even though the ISA Plug & Play standard documents a 2 byte version (only the IRQ mask)
	     *      and a 3 byte version (IRQ mask + additional bitmask for IRQ edge/level), Microsoft
	     *      Windows only supports the 2 byte version.
	     *
	     *      Windows 2000 apparently does not like the 3 byte version and it's IDE driver will fail
	     *      to detect the IDE ATA drive it booted from. Meaning, for some reason it will only
	     *      probe the ATA drive like a CD-ROM drive and then give up without ever checking for an
	     *      ATA device. Weird. */
            tmp[i+0] = (4 << 3) | 2;        /* IRQ resource */
            host_writew(tmp+i+1,1 << IRQ);
            i += 2+1;
        }

        tmp[i+0] = 0x79;                /* END TAG */
        tmp[i+1] = 0x00;
        i += 2;
        /*-------------possible-----------*/
        tmp[i+0] = 0x79;                /* END TAG */
        tmp[i+1] = 0x00;
        i += 2;
        /*-------------compatible---------*/
        tmp[i+0] = 0x79;                /* END TAG */
        tmp[i+1] = 0x00;
        i += 2;

        if (!ISAPNP_RegisterSysDev(tmp,i))
            LOG_MSG("ISAPNP register failed\n");
    }
}

void IDEController::install_io_port(){
    if (IS_PC98_ARCH)
        return;

    if (base_io != 0) {
        for (unsigned int i=0;i < 8;i++) {
            WriteHandler[i].Install(base_io+i,ide_baseio_w,IO_MA);
            ReadHandler[i].Install(base_io+i,ide_baseio_r,IO_MA);
        }
    }

    if (alt_io != 0) {
        WriteHandlerAlt[0].Install(alt_io,ide_altio_w,IO_MA);
        ReadHandlerAlt[0].Install(alt_io,ide_altio_r,IO_MA);

        /* the floppy controller might take port 0x3F7.
         * don't claim it if so */
        if (alt_io == 0x3F6 && fdc_takes_port_3F7()) {
            LOG_MSG("IDE: Not registering port 3F7h, FDC will occupy it.\n");
        }
        else {
            WriteHandlerAlt[1].Install(alt_io+1u,ide_altio_w,IO_MA);
            ReadHandlerAlt[1].Install(alt_io+1u,ide_altio_r,IO_MA);
        }
    }
}

IDEController::~IDEController() {
    unsigned int i;

    for (i=0;i < 2;i++) {
        if (device[i] != NULL) {
            delete device[i];
            device[i] = NULL;
        }
    }
}

static void IDE_PC98_Select(Bitu val) {
    val &= 1;
    pc98_ide_select = val;
    // TODO: IRQ raise by signal
}

static void ide_pc98ctlio_w(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;

    switch (port & 7) {
        case 0:     // 430h
            // ???
            break;
        case 2:     // 432h
            if (val & 0x80) {
                // test write?
            }
            else {
                IDE_PC98_Select(val & 1);
            }
            break;
        case 5:     // 435h
            // ???
            break;
    }
}

static Bitu ide_pc98ctlio_r(Bitu port,Bitu iolen) {
    (void)iolen;

    switch (port & 7) {
        case 0:     // 430h
            return pc98_ide_select;
        case 2:     // 432h
            return pc98_ide_select;
        case 5:     // 435h
            {
                Bitu bf = ~0ul;
                if (idecontroller[0] != NULL)
                    bf &= ~(1u << 0u);
                if (idecontroller[1] != NULL)
                    bf &= ~(1u << 1u);

                return bf;
            }
    }

    return ~0ul;
}

static void ide_altio_w(Bitu port,Bitu val,Bitu iolen) {
    IDEController *ide = match_ide_controller(port);
    if (ide == NULL) {
        LOG_MSG("WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
        return;
    }

    if (!ide->enable_pio32 && iolen == 4) {
        ide_altio_w(port,val&0xFFFF,2);
        ide_altio_w(port+2u,val>>16u,2);
        return;
    }
    else if (ide->ignore_pio32 && iolen == 4)
        return;

    if (IS_PC98_ARCH)
        port = (port >> 1) & 1;
    else
        port &= 1;

    if (port == 0) {/*3F6*/
        ide->interrupt_enable = (val&2u)?0:1;
        ide->hob = (val&0x80u) != 0;
        if (IS_PC98_ARCH)
            PC98_IDE_UpdateIRQ();
        else
            ide->check_device_irq();

        if ((val&4) && !ide->host_reset) {
            if (ide->device[0]) ide->device[0]->host_reset_begin();
            if (ide->device[1]) ide->device[1]->host_reset_begin();
            ide->host_reset=1;
        }
        else if (!(val&4) && ide->host_reset) {
            if (ide->device[0]) ide->device[0]->host_reset_complete();
            if (ide->device[1]) ide->device[1]->host_reset_complete();
            ide->host_reset=0;
        }
    }
}

static Bitu ide_altio_r(Bitu port,Bitu iolen) {
    IDEController *ide = match_ide_controller(port);
    IDEDevice *dev;

    if (ide == NULL) {
        LOG_MSG("WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
        return ~(0UL);
    }

    if (!ide->enable_pio32 && iolen == 4)
        return ide_altio_r(port,2) + (ide_altio_r(port+2u,2) << 16u);
    else if (ide->ignore_pio32 && iolen == 4)
        return ~0ul;

    dev = ide->device[ide->select];

    if (IS_PC98_ARCH)
        port = (port >> 1) & 1;
    else
        port &= 1;

    if (port == 0)/*3F6(R) status, does NOT clear interrupt*/
        return (dev != NULL) ? dev->status : 0x00;
    else /*3F7(R) Drive Address Register*/
        return 0x80u|(ide->select==0?0u:1u)|(ide->select==1?0u:2u)|
            ((dev != NULL) ? (((dev->drivehead&0xFu)^0xFu) << 2u) : 0x3Cu);

    return ~(0UL);
}

static Bitu ide_baseio_r(Bitu port,Bitu iolen) {
    IDEController *ide = match_ide_controller(port);
    IDEDevice *dev;
    Bitu ret = ~0ul;

    if (ide == NULL) {
        LOG_MSG("WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
        return ~(0UL);
    }

    if (!ide->enable_pio32 && iolen == 4)
        return ide_baseio_r(port,2) + (ide_baseio_r(port+2,2) << 16);
    else if (ide->ignore_pio32 && iolen == 4)
        return ~0ul;

    dev = ide->device[ide->select];

    if (IS_PC98_ARCH)
        port = (port >> 1) & 7;
    else
        port &= 7;

    /* ATA-1 Section 7.2.13 Status Register: BSY (Busy) bit.
     *
     * BSY(Busy) is set whenever the drive has access to the Command Block Registers.
     * The host should not access the Command Block Register when BSY=1. When BSY=1,
     * a read of any Command Block Register shall return the contents of the Status
     * Register. */
    if (dev != NULL && (dev->status & IDE_STATUS_BUSY))
        port = 7;

    switch (port) {
        case 0: /* 1F0 */
            ret = (dev != NULL) ? dev->data_read(iolen) : 0xFFFFFFFFUL;
            break;
        /* with HOB set in the device control register, the previous contents (LBA48 high order bytes) are read */
        case 1: /* 1F1 */
            ret = (dev != NULL) ? (ide->hob ? dev->hob_feature : (dev->feature & 0xFFu)) : 0x00;
            break;
        case 2: /* 1F2 */
            ret = (dev != NULL) ? (ide->hob ? dev->hob_count : (dev->count & 0xFFu)) : 0x00;
            break;
        case 3: /* 1F3 */
            ret = (dev != NULL) ? (ide->hob ? dev->hob_lba[0] : (dev->lba[0] & 0xFFu)) : 0x00;
            break;
        case 4: /* 1F4 */
            ret = (dev != NULL) ? (ide->hob ? dev->hob_lba[1] : (dev->lba[1] & 0xFFu)) : 0x00;
            break;
        case 5: /* 1F5 */
            ret = (dev != NULL) ? (ide->hob ? dev->hob_lba[2] : (dev->lba[2] & 0xFFu)) : 0x00;
            break;
        case 6: /* 1F6 */
            ret = (dev != NULL) ? dev->drivehead : 0x00;
            break;
        case 7: /* 1F7 */
            /* reading this port clears the device pending IRQ */
            if (dev && !(dev->status & IDE_STATUS_BUSY))
                dev->lower_irq();

            ret = (dev != NULL) ? dev->status : 0x00;
            ide->check_device_irq();
            break;
    }

#if 0
    if (ide == idecontroller[1])
        LOG_MSG("IDE: baseio read port %u ret %02x\n",(unsigned int)port,(unsigned int)ret);
#endif

    return ret;
}

static void ide_baseio_w(Bitu port,Bitu val,Bitu iolen) {
    IDEController *ide = match_ide_controller(port);
    IDEDevice *dev;

    if (ide == NULL) {
        LOG_MSG("WARNING: port read from I/O port not registered to IDE, yet callback triggered\n");
        return;
    }

    if (!ide->enable_pio32 && iolen == 4) {
        ide_baseio_w(port,val&0xFFFF,2);
        ide_baseio_w(port+2,val>>16,2);
        return;
    }
    else if (ide->ignore_pio32 && iolen == 4)
        return;

    dev = ide->device[ide->select];

    if (IS_PC98_ARCH)
        port = (port >> 1) & 7;
    else
        port &= 7;

    /* ignore I/O writes if the controller is busy */
    if (dev) {
        if (dev->status & IDE_STATUS_BUSY) {
            if (port == 6 && ((val>>4)&1) == ide->select) {
                /* some MS-DOS drivers like ATAPICD.SYS are just very pedantic about writing to port +6 to ensure the right drive is selected */
                return;
            }
            else {
                LOG_MSG("W-%03X %02X BUSY DROP [DEV]\n",(int)(port+ide->base_io),(int)val);
                return;
            }
        }
    }

#if 0
    if (ide == idecontroller[1])
        LOG_MSG("IDE: baseio write port %u val %02x\n",(unsigned int)port,(unsigned int)val);
#endif

    if (port >= 1 && port <= 5 && dev && !dev->allow_writing) {
        LOG_MSG("IDE WARNING: Write to port %u val %02x when device not ready to accept writing\n",
            (unsigned int)port,(unsigned int)val);
    }

    /* writing a command block register clears HOB */
    if (port >= 1 && port <= 6) ide->hob = false;

    /* The command block registers are written to both devices, the selected one only decides which
     * device answers reads. Each keeps the previous contents of the register for LBA48 commands. */
    if (port >= 1 && port <= 5) {
        for (unsigned int d=0;d < 2;d++) {
            IDEDevice *wd = ide->device[d];
            if (wd == NULL || !wd->allow_writing) continue;
            if (wd->status & IDE_STATUS_BUSY) continue;

            switch (port) {
                case 1: wd->hob_feature = (uint8_t)wd->feature; wd->feature = val & 0xFFu; break;
                case 2: wd->hob_count = (uint8_t)wd->count; wd->count = val & 0xFFu; break;
                case 3: wd->hob_lba[0] = (uint8_t)wd->lba[0]; wd->lba[0] = val & 0xFFu; break;
                case 4: wd->hob_lba[1] = (uint8_t)wd->lba[1]; wd->lba[1] = val & 0xFFu; break;
                case 5: wd->hob_lba[2] = (uint8_t)wd->lba[2]; wd->lba[2] = val & 0xFFu; break;
            }
        }
        return;
    }

    switch (port) {
        case 0: /* 1F0 */
            if (dev) dev->data_write(val,iolen); /* <- TODO: what about 32-bit PIO modes? */
            break;
        case 6: /* 1F6 */
            if (((val>>4)&1) != ide->select) {
                /* update select pointer if bit 4 changes.
                   also emulate IDE busy state when changing drives */
                if (dev) dev->deselect();
                ide->select = (val>>4)&1;
                dev = ide->device[ide->select];
                if (dev) dev->select(val,1);
            }
            else if (dev) {
                dev->select(val,0);
            }

            /* the other device latches the register too */
            {
                IDEDevice *od = ide->device[ide->select ^ 1u];
                if (od != NULL && od->allow_writing && !(od->status & IDE_STATUS_BUSY)) od->drivehead = (uint8_t)val;
            }

            ide->check_device_irq();
            break;
        case 7: /* 1F7 */
            /* EXECUTE DEVICE DIAGNOSTIC is carried out by both devices, whichever is selected */
            if (val == 0x90) {
                if (ide->device[0]) ide->device[0]->execute_device_diagnostic();
                if (ide->device[1]) ide->device[1]->execute_device_diagnostic();
                /* device 0 reports, including whether device 1 passed (bit 7 clear) */
                if (ide->device[0]) {
                    ide->device[0]->feature = 0x01;
                    ide->device[0]->raise_irq();
                }
                else if (ide->device[1]) {
                    ide->device[1]->raise_irq();
                }
                break;
            }
            if (dev) dev->writecommand(val);
            break;
    }
}

static void IDE_Destroy(Section* sec) {
    (void)sec;//UNUSED
    for (unsigned int i=0;i < MAX_IDE_CONTROLLERS;i++) {
        if (idecontroller[i] != NULL) {
            delete idecontroller[i];
            idecontroller[i] = NULL;
        }
    }

    init_ide = 0;
}

static void IDE_Init(Section* sec,unsigned char ide_interface) {
    Section_prop *section=static_cast<Section_prop *>(sec);
    IDEController *ide;

    assert(ide_interface < MAX_IDE_CONTROLLERS);

    if (IS_PC98_ARCH && ide_interface >= 2)
        return;

    if (!section->Get_bool("enable"))
        return;

    if (!init_ide) {
        AddExitFunction(AddExitFunctionFuncPair(IDE_Destroy));
        init_ide = 1;
    }

    LOG(LOG_MISC,LOG_DEBUG)("Initializing IDE controller %u",ide_interface);

    if (idecontroller[ide_interface] != NULL) {
        delete idecontroller[ide_interface];
        idecontroller[ide_interface] = NULL;
    }

    ide = idecontroller[ide_interface] = new IDEController(sec,ide_interface);
    ide->install_io_port();

    PIC_SetIRQMask((unsigned int)ide->IRQ,false);
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

void IDE_Quinternary_Init(Section *sec) {
    IDE_Init(sec,4);
}

void IDE_Sexternary_Init(Section *sec) {
    IDE_Init(sec,5);
}

void IDE_Septernary_Init(Section *sec) {
    IDE_Init(sec,6);
}

void IDE_Octernary_Init(Section *sec) {
    IDE_Init(sec,7);
}

const char *ide_names[MAX_IDE_CONTROLLERS] = {
    "ide, primary",
    "ide, secondary",
    "ide, tertiary",
    "ide, quaternary",
    "ide, quinternary",
    "ide, sexternary",
    "ide, septernary",
    "ide, octernary"
};
void (*ide_inits[MAX_IDE_CONTROLLERS])(Section *) = {
    &IDE_Primary_Init,
    &IDE_Secondary_Init,
    &IDE_Tertiary_Init,
    &IDE_Quaternary_Init,
    &IDE_Quinternary_Init,
    &IDE_Sexternary_Init,
    &IDE_Septernary_Init,
    &IDE_Octernary_Init
};

IO_ReadHandleObject  PC98_ReadHandler[8], PC98_ReadHandlerAlt[2], PC98_ReadHandlerSel[3];
IO_WriteHandleObject PC98_WriteHandler[8],PC98_WriteHandlerAlt[2],PC98_WriteHandlerSel[3];

void PC98_IDE_UpdateIRQ(void) {
    if (IS_PC98_ARCH) {
        bool raise = false;
        IDEController *ide = match_ide_controller(0);
        if (ide != NULL)
            raise = ide->irq_pending && ide->interrupt_enable;

        if (raise)
            PIC_ActivateIRQ(9);
        else
            PIC_DeActivateIRQ(9);
    }
}

void IDE_OnReset(Section *sec) {
    if ((bootguest||(use_quick_reboot&&!bootvm))&&bootdrive>=0) return;
    (void)sec;//UNUSED

    for (size_t i=0;i < MAX_IDE_CONTROLLERS;i++) ide_inits[i](control->GetSection(ide_names[i]));

    if (IS_PC98_ARCH) {//TODO: Only if any IDE interfaces are enabled
        for (size_t i=0;i < 8;i++) {
            PC98_WriteHandler[i].Uninstall();
            PC98_ReadHandler[i].Uninstall();
            PC98_WriteHandler[i].Install(0x640+(i*2),ide_baseio_w,IO_MA);
            PC98_ReadHandler[i].Install(0x640+(i*2),ide_baseio_r,IO_MA);
        }

        for (size_t i=0;i < 2;i++) {
            PC98_WriteHandlerAlt[i].Uninstall();
            PC98_ReadHandlerAlt[i].Uninstall();
        }

        PC98_WriteHandlerAlt[0].Install(0x74C,ide_altio_w,IO_MA);
        PC98_ReadHandlerAlt[0].Install(0x74C,ide_altio_r,IO_MA);

        PC98_WriteHandlerAlt[1].Install(0x74E,ide_altio_w,IO_MA);
        PC98_ReadHandlerAlt[1].Install(0x74E,ide_altio_r,IO_MA);

        for (size_t i=0;i < 3;i++) {
            PC98_WriteHandlerSel[i].Uninstall();
            PC98_ReadHandlerSel[i].Uninstall();
        }

        PC98_WriteHandlerSel[0].Install(0x430,ide_pc98ctlio_w,IO_MA);
        PC98_ReadHandlerSel[0].Install(0x430,ide_pc98ctlio_r,IO_MA);

        PC98_WriteHandlerSel[1].Install(0x432,ide_pc98ctlio_w,IO_MA);
        PC98_ReadHandlerSel[1].Install(0x432,ide_pc98ctlio_r,IO_MA);

        PC98_WriteHandlerSel[2].Install(0x435,ide_pc98ctlio_w,IO_MA);
        PC98_ReadHandlerSel[2].Install(0x435,ide_pc98ctlio_r,IO_MA);
    }
}

void IDE_Init() {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing IDE controllers");

    AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(IDE_OnReset));
}

void BIOS_Post_register_IDE() {
    for (size_t i=0;i < MAX_IDE_CONTROLLERS;i++) {
        if (idecontroller[i] != NULL)
            idecontroller[i]->register_isapnp();
    }
}

void DOS_EnableDriveIDEMenu(unsigned int idx,unsigned char ms) {
	if (idx < MAX_IDE_CONTROLLERS) {
		IDEController *ctl = idecontroller[idx];
		IDEDevice *dev = ctl ? ctl->device[ms] : NULL;
        IDEATAPICDROMDevice* cdrom = nullptr;
		bool cdromchange = false;
		bool do_hide = true;
		bool empty = true;
		std::string name;
		char bname[128];

		sprintf(bname,"IDEDrive%u%c",idx+1,ms?'s':'m');

		if (dev) {
			if (dev->type == IDE_TYPE_CDROM) {
				cdrom = dynamic_cast<IDEATAPICDROMDevice*>(dev);
				if (cdrom) {
					if (!cdrom->mscdex) {
						do_hide = false;
						empty = false;
					}
				}
			}
		}

		if (cdrom) {
			cdromchange = true;
		}

		/* why even show the drive if booted into a guest OS and no drive attached? */
		/* NTS: The vertical menu divide between A-M and N-Z might get weird depending on
		 *      the menu API involved so to prevent that, always show drives A, B, and Z */
		name = bname;
		mainMenu.get_item(name).hide(do_hide).refresh_item(mainMenu);

#if defined (WIN32)
		name = std::string(bname) + "_mountauto";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
#endif
		name = std::string(bname) + "_mounthd";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_mountcd";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_mountfd";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_mountfro";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_mountarc";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_mountimg";
		mainMenu.get_item(name).enable(cdromchange).refresh_item(mainMenu);
		name = std::string(bname) + "_mountimgs";
		mainMenu.get_item(name).enable(cdromchange).refresh_item(mainMenu);
		name = std::string(bname) + "_mountiro";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_unmount";
		mainMenu.get_item(name).enable(cdromchange).refresh_item(mainMenu);
		name = std::string(bname) + "_swap";
		mainMenu.get_item(name).enable(cdromchange).refresh_item(mainMenu);
		name = std::string(bname) + "_rescan";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_info";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_boot";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_bootimg";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
		name = std::string(bname) + "_saveimg";
		mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
	}
}

