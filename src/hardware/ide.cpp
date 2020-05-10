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
#include "control.h"
#include "callback.h"
#include "bios_disk.h"
#include "../src/dos/cdrom.h"

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

static unsigned char init_ide = 0;

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
    uint8_t command,drivehead,status; /* command/status = BASE+7  drivehead = BASE+6 */
    enum IDEDeviceType type;
    bool faked_command; /* if set, DOSBox is sending commands to itself */
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
    double ide_select_delay;    /* time between writing 0x1F6 and drive readiness */
    double ide_spinup_delay;    /* time it takes to spin the hard disk motor up to speed */
    double ide_spindown_delay;  /* time it takes for hard disk motor to spin down */
    double ide_identify_command_delay;
public:
    IDEDevice(IDEController *c);
    virtual ~IDEDevice();
    virtual void host_reset_begin();    /* IDE controller -> upon setting bit 2 of alt (0x3F6) */
    virtual void host_reset_complete(); /* IDE controller -> upon setting bit 2 of alt (0x3F6) */
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
    IDEATADevice(IDEController *c,unsigned char disk_index);
    virtual ~IDEATADevice();
    virtual void writecommand(uint8_t cmd);
public:
    std::string id_serial;
    std::string id_firmware_rev;
    std::string id_model;
    unsigned char bios_disk_index;
    imageDisk *getBIOSdisk();
    void update_from_biosdisk();
    virtual Bitu data_read(Bitu iolen); /* read from 1F0h data port from IDE device */
    virtual void data_write(Bitu v,Bitu iolen);/* write to 1F0h data port to IDE device */
    virtual void generate_identify_device();
    virtual void prepare_read(Bitu offset,Bitu size);
    virtual void prepare_write(Bitu offset,Bitu size);
    virtual void io_completion();
    virtual bool increment_current_address(Bitu count=1);
public:
    Bitu multiple_sector_max,multiple_sector_count;
    Bitu heads,sects,cyls,headshr,progress_count;
    Bitu phys_heads,phys_sects,phys_cyls;
    unsigned char sector[512 * 128] = {};
    Bitu sector_i,sector_total;
    bool geo_translate;
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
    virtual Bitu data_read(Bitu iolen); /* read from 1F0h data port from IDE device */
    virtual void data_write(Bitu v,Bitu iolen);/* write to 1F0h data port to IDE device */
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
    virtual void read_subchannel();
    virtual void play_audio_msf();
    virtual void pause_resume();
    virtual void play_audio10();
    virtual void mode_sense();
    virtual void read_toc();
public:
    bool atapi_to_host;         /* if set, PACKET data transfer is to be read by host */
    double spinup_time;
    double spindown_timeout;
    double cd_insertion_time;
    Bitu host_maximum_byte_count;       /* host maximum byte count during PACKET transfer */
    std::string id_mmc_vendor_id;
    std::string id_mmc_product_id;
    std::string id_mmc_product_rev;
    Bitu LBA,TransferLength;
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
    Bitu select,status,drivehead;   /* which is selected, status register (0x1F7) but ONLY if no device exists at selection, drive/head register (0x1F6) */
    bool interrupt_enable;      /* bit 1 of alt (0x3F6) */
    bool host_reset;        /* bit 2 of alt */
    bool irq_pending;
    /* defaults for CD-ROM emulation */
    double spinup_time;
    double spindown_timeout;
    double cd_insertion_time;
public:
    IDEController(Section* configuration,unsigned char index);
    void register_isapnp();
    void install_io_port();
    void raise_irq();
    void lower_irq();
    ~IDEController();
};

static IDEController* idecontroller[MAX_IDE_CONTROLLERS]={NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

static void IDE_DelayedCommand(Bitu idx/*which IDE controller*/);
static IDEController* GetIDEController(Bitu idx);

static void IDE_ATAPI_SpinDown(Bitu idx/*which IDE controller*/) {
    IDEController *ctrl = GetIDEController(idx);
    if (ctrl == NULL) return;

    for (unsigned int i=0;i < 2;i++) {
        IDEDevice *dev = ctrl->device[i];
        if (dev == NULL) continue;

        if (dev->type == IDE_TYPE_HDD) {
        }
        else if (dev->type == IDE_TYPE_CDROM) {
            IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

            if (atapi->loading_mode == LOAD_DISC_READIED || atapi->loading_mode == LOAD_READY) {
                atapi->loading_mode = LOAD_IDLE;
                LOG_MSG("ATAPI CD-ROM: spinning down\n");
            }
        }
        else {
            LOG_MSG("Unknown ATAPI spinup callback\n");
        }
    }
}

static void IDE_ATAPI_SpinUpComplete(Bitu idx/*which IDE controller*/);

static void IDE_ATAPI_CDInsertion(Bitu idx/*which IDE controller*/) {
    IDEController *ctrl = GetIDEController(idx);
    if (ctrl == NULL) return;

    for (unsigned int i=0;i < 2;i++) {
        IDEDevice *dev = ctrl->device[i];
        if (dev == NULL) continue;

        if (dev->type == IDE_TYPE_HDD) {
        }
        else if (dev->type == IDE_TYPE_CDROM) {
            IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

            if (atapi->loading_mode == LOAD_INSERT_CD) {
                atapi->loading_mode = LOAD_DISC_LOADING;
                LOG_MSG("ATAPI CD-ROM: insert CD to loading\n");
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,idx);
                PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,idx);
                PIC_AddEvent(IDE_ATAPI_SpinUpComplete,atapi->spinup_time/*ms*/,idx);
            }
        }
        else {
            LOG_MSG("Unknown ATAPI spinup callback\n");
        }
    }
}

static void IDE_ATAPI_SpinUpComplete(Bitu idx/*which IDE controller*/) {
    IDEController *ctrl = GetIDEController(idx);
    if (ctrl == NULL) return;

    for (unsigned int i=0;i < 2;i++) {
        IDEDevice *dev = ctrl->device[i];
        if (dev == NULL) continue;

        if (dev->type == IDE_TYPE_HDD) {
        }
        else if (dev->type == IDE_TYPE_CDROM) {
            IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;

            if (atapi->loading_mode == LOAD_DISC_LOADING) {
                atapi->loading_mode = LOAD_DISC_READIED;
                LOG_MSG("ATAPI CD-ROM: spinup complete\n");
                PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,idx);
                PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,idx);
                PIC_AddEvent(IDE_ATAPI_SpinDown,atapi->spindown_timeout/*ms*/,idx);
            }
        }
        else {
            LOG_MSG("Unknown ATAPI spinup callback\n");
        }
    }
}

/* returns "true" if command should proceed as normal, "false" if sense data was set and command should not proceed.
 * this function helps to enforce virtual "spin up" and "ready" delays. */
bool IDEATAPICDROMDevice::common_spinup_response(bool trigger,bool wait) {
    if (loading_mode == LOAD_IDLE) {
        if (trigger) {
            LOG_MSG("ATAPI CD-ROM: triggered to spin up from idle\n");
            loading_mode = LOAD_DISC_LOADING;
            PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,controller->interface_index);
            PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,controller->interface_index);
            PIC_AddEvent(IDE_ATAPI_SpinUpComplete,spinup_time/*ms*/,controller->interface_index);
        }
    }
    else if (loading_mode == LOAD_READY) {
        if (trigger) {
            PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,controller->interface_index);
            PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,controller->interface_index);
            PIC_AddEvent(IDE_ATAPI_SpinDown,spindown_timeout/*ms*/,controller->interface_index);
        }
    }

    switch (loading_mode) {
        case LOAD_NO_DISC:
        case LOAD_INSERT_CD:
            set_sense(/*SK=*/0x02,/*ASC=*/0x3A); /* Medium Not Present */
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
                set_sense(/*SK=*/0x02,/*ASC=*/0x28,/*ASCQ=*/0x00); /* Medium is ready (has changed) */
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

void IDEATAPICDROMDevice::read_subchannel() {
//  unsigned char Format = atapi_cmd[2] & 0xF;
//  unsigned char Track = atapi_cmd[6];
    unsigned char paramList = atapi_cmd[3];
    unsigned char attr,track,index;
    bool SUBQ = !!(atapi_cmd[2] & 0x40);
    bool TIME = !!(atapi_cmd[1] & 2);
    unsigned char *write;
    unsigned char astat;
    bool playing,pause;
    TMSF rel,abs;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        LOG_MSG("WARNING: ATAPI READ TOC unable to get CDROM drive\n");
        prepare_read(0,8);
        return;
    }

    if (paramList == 0 || paramList > 3) {
        LOG_MSG("ATAPI READ SUBCHANNEL unknown param list\n");
        prepare_read(0,8);
        return;
    }
    else if (paramList == 2) {
        LOG_MSG("ATAPI READ SUBCHANNEL Media Catalog Number not supported\n");
        prepare_read(0,8);
        return;
    }
    else if (paramList == 3) {
        LOG_MSG("ATAPI READ SUBCHANNEL ISRC not supported\n");
        prepare_read(0,8);
        return;
    }

    /* get current subchannel position */
    if (!cdrom->GetAudioSub(attr,track,index,rel,abs)) {
        LOG_MSG("ATAPI READ SUBCHANNEL unable to read current pos\n");
        prepare_read(0,8);
        return;
    }

    if (!cdrom->GetAudioStatus(playing,pause))
        playing = pause = false;

    if (playing)
        astat = pause ? 0x12 : 0x11;
    else
        astat = 0x13;

    memset(sector,0,8);
    write = sector;
    *write++ = 0x00;
    *write++ = astat;/* AUDIO STATUS */
    *write++ = 0x00;/* SUBCHANNEL DATA LENGTH */
    *write++ = 0x00;

    if (SUBQ) {
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

            sec = (rel.min*60u*75u)+(rel.sec*75u)+rel.fr - 150u;
            *write++ = (unsigned char)(sec >> 24u);
            *write++ = (unsigned char)(sec >> 16u);
            *write++ = (unsigned char)(sec >> 8u);
            *write++ = (unsigned char)(sec >> 0u);
        }
    }

    {
        unsigned int x = (unsigned int)(write-sector) - 4;
        sector[2] = x >> 8;
        sector[3] = x;
    }

    prepare_read(0,MIN((unsigned int)(write-sector),(unsigned int)host_maximum_byte_count));
#if 0
    LOG_MSG("SUBCH ");
    for (size_t i=0;i < sector_total;i++) LOG_MSG("%02x ",sector[i]);
    LOG_MSG("\n");
#endif
}

void IDEATAPICDROMDevice::mode_sense() {
    unsigned char PAGE = atapi_cmd[2] & 0x3F;
//  unsigned char SUBPAGE = atapi_cmd[3];
    unsigned char *write;
    unsigned int x;

    write = sector;

    /* some header. not well documented */
    *write++ = 0x00;    /* ?? */
    *write++ = 0x00;    /* length */
    *write++ = 0x00;    /* ?? */
    *write++ = 0x00;
    *write++ = 0x00;
    *write++ = 0x00;
    *write++ = 0x00;
    *write++ = 0x00;

    *write++ = PAGE;    /* page code */
    *write++ = 0x00;    /* page length (fill in later) */
    switch (PAGE) {
        case 0x01: /* Read error recovery */
            *write++ = 0x00;    /* maximum error correction */
            *write++ = 3;       /* read retry count */
            *write++ = 0x00;
            *write++ = 0x00;
            *write++ = 0x00;
            *write++ = 0x00;
            break;
        case 0x0E: /* CD-ROM audio control */
            *write++ = 0x04;    /* ?? */
            *write++ = 0x00;    /* reserved @+3 */
            *write++ = 0x00;    /* reserved @+4 */
            *write++ = 0x00;    /* reserved @+5 */
            *write++ = 0x00;
            *write++ = 75;      /* logical blocks per second */

            *write++ = 0x01;    /* output port 0 selection */
            *write++ = 0xD8;    /* output port 0 volume (?) */
            *write++ = 0x02;    /* output port 1 selection */
            *write++ = 0xD8;    /* output port 1 volume (?) */
            *write++ = 0x00;    /* output port 2 selection */
            *write++ = 0x00;    /* output port 2 volume (?) */
            *write++ = 0x00;    /* output port 3 selection */
            *write++ = 0x00;    /* output port 3 volume (?) */
            break;
        case 0x2A: /* CD-ROM mechanical status */
            *write++ = 0x00;    /* reserved @+2 ?? */
            *write++ = 0x00;    /* reserved @+3 ?? */
            *write++ = 0xF1;    /* multisession=0 mode2form2=1 mode2form=1 audioplay=1 */
            *write++ = 0xFF;    /* ISRC=1 UPC=1 C2=1 RWDeinterleave=1 RWSupported=1 CDDAAccurate=1 CDDASupported=1 */
            *write++ = 0x29;    /* loading mechanism type=tray  eject=1  prevent jumper=0  lockstate=0  lock=1 */
            *write++ = 0x03;    /* separate channel mute=1 separate channel volume levels=1 */

            x = 176 * 8;        /* maximum speed supported: 8X */
            *write++ = x>>8;
            *write++ = x;

            x = 256;        /* (?) */
            *write++ = x>>8;
            *write++ = x;

            x = 6 * 256;        /* (?) */
            *write++ = x>>8;
            *write++ = x;

            x = 176 * 8;        /* current speed supported: 8X */
            *write++ = x>>8;
            *write++ = x;
            break;
        default:
            memset(write,0,6); write += 6;
            LOG_MSG("WARNING: MODE SENSE on page 0x%02x not supported\n",PAGE);
            break;
    }

    /* fill in page length */
    sector[1] = (unsigned int)(write-sector) - 2;
    sector[8+1] = (unsigned int)(write-sector) - 2 - 8;

    prepare_read(0,MIN((unsigned int)(write-sector),(unsigned int)host_maximum_byte_count));
#if 0
    printf("SENSE ");
    for (size_t i=0;i < sector_total;i++) printf("%02x ",sector[i]);
    printf("\n");
#endif
}

void IDEATAPICDROMDevice::pause_resume() {
    bool Resume = !!(atapi_cmd[8] & 1);

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        LOG_MSG("WARNING: ATAPI READ TOC unable to get CDROM drive\n");
        sector_total = 0;
        return;
    }

    cdrom->PauseAudio(Resume);
}

void IDEATAPICDROMDevice::play_audio_msf() {
    uint32_t start_lba,end_lba;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        LOG_MSG("WARNING: ATAPI READ TOC unable to get CDROM drive\n");
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
        else end_lba = 0;
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
        /* TODO: How do we interpret that? Does that mean audio playback stops? Or does it
         * mean we do nothing to the state of audio playback? */
        sector_total = 0;
        return;
    }

    /* LBA 0xFFFFFFFF means start playing wherever the optics of the CD sit */
    if (start_lba != 0xFFFFFFFF)
        cdrom->PlayAudioSector(start_lba,end_lba - start_lba);
    else
        cdrom->PauseAudio(true);

    sector_total = 0;
}

void IDEATAPICDROMDevice::play_audio10() {
    uint16_t play_length;
    uint32_t start_lba;

    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        LOG_MSG("WARNING: ATAPI READ TOC unable to get CDROM drive\n");
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
        /* TODO: How do we interpret that? Does that mean audio playback stops? Or does it
         * mean we do nothing to the state of audio playback? */
        sector_total = 0;
        return;
    }

    /* LBA 0xFFFFFFFF means start playing wherever the optics of the CD sit */
    if (start_lba != 0xFFFFFFFF)
        cdrom->PlayAudioSector(start_lba,play_length);
    else
        cdrom->PauseAudio(true);

    sector_total = 0;
}

#if 0 /* TODO move to library */
static unsigned char dec2bcd(unsigned char c) {
    return ((c / 10) << 4) + (c % 10);
}
#endif

void IDEATAPICDROMDevice::read_toc() {
    /* NTS: The SCSI MMC standards say we're allowed to indicate the return data
     *      is longer than it's allocation length. But here's the thing: some MS-DOS
     *      CD-ROM drivers will ask for the TOC but only provide enough room for one
     *      entry (OAKCDROM.SYS) and if we signal more data than it's buffer, it will
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
        LOG_MSG("WARNING: ATAPI READ TOC unable to get CDROM drive\n");
        prepare_read(0,8);
        return;
    }

    memset(sector,0,8);

    if (!cdrom->GetAudioTracks(first,last,leadOut)) {
        LOG_MSG("WARNING: ATAPI READ TOC failed to get track info\n");
        prepare_read(0,8);
        return;
    }

    /* start 2 bytes out. we'll fill in the data length later */
    write = sector + 2;

    if (Format == 1) { /* Read multisession info */
        unsigned char attr;
        TMSF start;

        *write++ = (unsigned char)1;        /* @+2 first complete session */
        *write++ = (unsigned char)1;        /* @+3 last complete session */

        if (!cdrom->GetAudioTrackInfo(first,start,attr)) {
            LOG_MSG("WARNING: ATAPI READ TOC unable to read track %u information\n",first);
            attr = 0x41; /* ADR=1 CONTROL=4 */
            start.min = 0;
            start.sec = 0;
            start.fr = 0;
        }

        LOG_MSG("Track %u attr=0x%02x\n",first,attr);

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
                LOG_MSG("WARNING: ATAPI READ TOC unable to read track %u information\n",track);
                attr = 0x41; /* ADR=1 CONTROL=4 */
                start.min = 0;
                start.sec = 0;
                start.fr = 0;
            }

            if (track < Track)
                continue;
            if ((write+8) > (sector+AllocationLength))
                break;

            LOG_MSG("Track %u attr=0x%02x\n",track,attr);

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
    else {
        LOG_MSG("WARNING: ATAPI READ TOC Format=%u not supported\n",Format);
        prepare_read(0,8);
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
    /* if the drive is spinning up, then the command waits */
    if (loading_mode == LOAD_DISC_LOADING) {
        switch (atapi_cmd[0]) {
            case 0x00: /* TEST UNIT READY */
            case 0x03: /* REQUEST SENSE */
                allow_writing = true;
                break; /* do not delay */
            default:
                PIC_AddEvent(IDE_DelayedCommand,100/*ms*/,controller->interface_index);
                return;
        }
    }
    else if (loading_mode == LOAD_DISC_READIED) {
        switch (atapi_cmd[0]) {
            case 0x00: /* TEST UNIT READY */
            case 0x03: /* REQUEST SENSE */
                allow_writing = true;
                break; /* do not delay */
            default:
                if (!common_spinup_response(/*spin up*/true,/*wait*/false)) {
                    count = 0x03;
                    state = IDE_DEV_READY;
                    feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                    status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                    controller->raise_irq();
                    allow_writing = true;
                    return;
                }
                break;
        }
    }

    switch (atapi_cmd[0]) {
        case 0x03: /* REQUEST SENSE */
            prepare_read(0,MIN((unsigned int)sense_length,(unsigned int)host_maximum_byte_count));
            memcpy(sector,sense,sense_length);

            feature = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
            count = 0x03;
            feature = 0x00;
            sector_total = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* Don't care. Do nothing. */

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x25: /* READ CAPACITY */ {
            const unsigned int secsize = 2048;
            int first,last;
            TMSF leadOut;

            CDROM_Interface *cdrom = getMSCDEXDrive();

            if (!cdrom->GetAudioTracks(first,last,leadOut))
                LOG_MSG("WARNING: ATAPI READ TOC failed to get track info\n");

            uint32_t sec = (leadOut.min*60u*75u)+(leadOut.sec*75u)+leadOut.fr - 150u;

            prepare_read(0,MIN((unsigned int)8,(unsigned int)host_maximum_byte_count));
            sector[0] = sec >> 24u;
            sector[1] = sec >> 16u;
            sector[2] = sec >> 8u;
            sector[3] = sec & 0xFF;
            sector[4] = secsize >> 24u;
            sector[5] = secsize >> 16u;
            sector[6] = secsize >> 8u;
            sector[7] = secsize & 0xFF;
//          LOG_MSG("sec=%lu secsize=%lu\n",sec,secsize);

            feature = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            } break;
        case 0x2B: /* SEEK */
            count = 0x03;
            feature = 0x00;
            sector_total = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* Don't care. Do nothing. */

            /* Except... Windows 95's CD player expects the SEEK command to interrupt CD audio playback.
             * In fact it depends on it to the exclusion of commands explicitly standardized to... you know...
             * stop or pause playback. Oh Microsoft, you twits... */
            {
                CDROM_Interface *cdrom = getMSCDEXDrive();
                if (cdrom) {
                    bool playing,pause;

                    if (!cdrom->GetAudioStatus(playing,pause))
                        playing = true;

                    if (playing) {
                        LOG_MSG("ATAPI: Interrupting CD audio playback due to SEEK\n");
                        cdrom->StopAudio();
                    }
                }
            }

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
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
            allow_writing = true;
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
                    LOG_MSG("ATAPI: Failed to read %lu sectors at %lu\n",
                        (unsigned long)TransferLength,(unsigned long)LBA);
                    /* TODO: write sense data */
                }
            }

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x42: /* READ SUB-CHANNEL */
            read_subchannel();

            feature = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x43: /* READ TOC */
            read_toc();

            feature = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x45: /* PLAY AUDIO(10) */
            play_audio10();

            count = 0x03;
            feature = 0x00;
            sector_total = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x47: /* PLAY AUDIO MSF */
            play_audio_msf();

            count = 0x03;
            feature = 0x00;
            sector_total = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x4B: /* PAUSE/RESUME */
            pause_resume();

            count = 0x03;
            feature = 0x00;
            sector_total = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x55: /* MODE SELECT(10) */
            /* we need the data written first, will act in I/O completion routine */
            {
                unsigned int x;

                x = (unsigned int)lba[1] + ((unsigned int)lba[2] << 8u);

                /* Windows 95 likes to set 0xFFFF here for whatever reason.
                 * Negotiate it down to a maximum of 512 for sanity's sake */
                if (x > 512) x = 512;
                lba[2] = x >> 8u;
                lba[1] = x;

//              LOG_MSG("MODE SELECT expecting %u bytes\n",x);
                prepare_write(0,(x+1u)&(~1u));
            }

            feature = 0x00;
            state = IDE_DEV_DATA_WRITE;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x5A: /* MODE SENSE(10) */
            mode_sense();

            feature = 0x00;
            state = IDE_DEV_DATA_READ;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ|IDE_STATUS_DRIVE_SEEK_COMPLETE;

            /* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
            lba[2] = sector_total >> 8;
            lba[1] = sector_total;

            controller->raise_irq();
            allow_writing = true;
            break;
        default:
            LOG_MSG("Unknown ATAPI command after busy wait. Why?\n");
            abort_error();
            controller->raise_irq();
            allow_writing = true;
            break;
    }

}

void IDEATAPICDROMDevice::set_sense(unsigned char SK,unsigned char ASC,unsigned char ASCQ,unsigned int len) {
    if (len < 18) len = 18;
    memset(sense,0,len);
    sense_length = len;

    sense[0] = 0x70;    /* RESPONSE CODE */
    sense[2] = SK&0xF;  /* SENSE KEY */
    sense[7] = len - 18;    /* additional sense length */
    sense[12] = ASC;
    sense[13] = ASCQ;
}

IDEATAPICDROMDevice::IDEATAPICDROMDevice(IDEController *c,unsigned char drive_index) : IDEDevice(c) {
    this->drive_index = drive_index;
    sector_i = sector_total = 0;
    atapi_to_host = false;
    host_maximum_byte_count = 0;
    LBA = 0;
    TransferLength = 0;
    memset(atapi_cmd, 0, sizeof(atapi_cmd));
    atapi_cmd_i = 0;
    atapi_cmd_total = 0;
    memset(sector, 0, sizeof(sector));

    memset(sense,0,sizeof(sense));
    IDEATAPICDROMDevice::set_sense(/*SK=*/0);

    /* FIXME: Spinup/down times should be dosbox.conf configurable, if the DOSBox gamers
     *        care more about loading times than emulation accuracy. */
    cd_insertion_time = 4000; /* a quick user that can switch CDs in 4 seconds */
    if (c->cd_insertion_time > 0) cd_insertion_time = c->cd_insertion_time;

    spinup_time = 1000; /* drive takes 1 second to spin up from idle */
    if (c->spinup_time > 0) spinup_time = c->spinup_time;

    spindown_timeout = 10000; /* drive spins down automatically after 10 seconds */
    if (c->spindown_timeout > 0) spindown_timeout = c->spindown_timeout;

    loading_mode = LOAD_IDLE;
    has_changed = false;

    type = IDE_TYPE_CDROM;
    id_serial = "123456789";
    id_firmware_rev = "0.83-X";
    id_model = "DOSBox-X Virtual CD-ROM";

    /* INQUIRY strings */
    id_mmc_vendor_id = "DOSBox-X";
    id_mmc_product_id = "Virtual CD-ROM";
    id_mmc_product_rev = "0.83-X";
}

IDEATAPICDROMDevice::~IDEATAPICDROMDevice() {
}

void IDEATAPICDROMDevice::on_mode_select_io_complete() {
    unsigned int AllocationLength = ((unsigned int)atapi_cmd[7] << 8) + atapi_cmd[8];
    unsigned char *scan,*fence;
    size_t i;

    /* the first 8 bytes are a mode parameter header.
     * It's supposed to provide length, density, etc. or whatever the hell
     * it means. Windows 95 seems to send all zeros there, so ignore it.
     *
     * we care about the bytes following it, which contain page_0 mode
     * pages */

    scan = sector + 8;
    fence = sector + MIN((unsigned int)sector_total,(unsigned int)AllocationLength);

    while ((scan+2) < fence) {
        unsigned char PAGE = *scan++;
        unsigned int LEN = (unsigned int)(*scan++);

        if ((scan+LEN) > fence) {
            LOG_MSG("ATAPI MODE SELECT warning, page_0 length extends %u bytes past buffer\n",(unsigned int)(scan+LEN-fence));
            break;
        }

        LOG_MSG("ATAPI MODE SELECT, PAGE 0x%02x len=%u\n",PAGE,LEN);
        LOG_MSG("  ");
        for (i=0;i < LEN;i++) LOG_MSG("%02x ",scan[i]);
        LOG_MSG("\n");

        scan += LEN;
    }
}

void IDEATAPICDROMDevice::atapi_io_completion() {
    /* for most ATAPI PACKET commands, the transfer is done and we need to clear
       all indication of a possible data transfer */

    if (count == 0x00) { /* the command was expecting data. now it can act on it */
        switch (atapi_cmd[0]) {
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

bool IDEATADevice::increment_current_address(Bitu count) {
    if (count == 0) return false;

    if (drivehead_is_lba(drivehead)) {
        /* 28-bit LBA:
         *    drivehead: 27:24
         *    lba[2]:    23:16
         *    lba[1]:    15:8
         *    lba[0]:    7:0 */
        do {
            if (((++lba[0])&0xFF) == 0x00) {
                lba[0] = 0x00;
                if (((++lba[1])&0xFF) == 0x00) {
                    lba[1] = 0x00;
                    if (((++lba[2])&0xFF) == 0x00) {
                        lba[2] = 0x00;
                        if (((++drivehead)&0xF) == 0) {
                            drivehead -= 0x10;
                            return false;
                        }
                    }
                }
            }
        } while ((--count) != 0);
    }
    else {
        /* C/H/S increment with rollover */
        do {
            /* increment sector */
            if (((++lba[0])&0xFF) == ((sects+1)&0xFF)) {
                lba[0] = 1;
                /* increment head */
                if (((++drivehead)&0xF) == (heads&0xF)) {
                    drivehead &= 0xF0;
                    if (heads == 16) drivehead -= 0x10;
                    /* increment cylinder */
                    if (((++lba[1])&0xFF) == 0x00) {
                        if (((++lba[2])&0xFF) == 0x00) {
                            return false;
                        }
                    }
                }

            }
        } while ((--count) != 0);
    }

    return true;
}

void IDEATADevice::io_completion() {
    /* lower DRQ */
    status &= ~IDE_STATUS_DRQ;

    /* depending on the command, either continue it or finish up */
    switch (command) {
        case 0x20:/* READ SECTOR */
            /* OK, decrement count, increment address */
            /* NTS: Remember that count == 0 means the host wanted to transfer 256 sectors */
            progress_count++;
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

            if (!increment_current_address()) {
                LOG_MSG("READ advance error\n");
                abort_error();
                return;
            }

            /* cause another delay, another sector read */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,0.00001/*ms*/,controller->interface_index);
            break;
        case 0x30:/* WRITE SECTOR */
            /* this is where the drive has accepted the sector, lowers DRQ, and begins executing the command */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,((progress_count == 0 && !faked_command) ? 0.1 : 0.00001)/*ms*/,controller->interface_index);
            break;
        case 0xC4:/* READ MULTIPLE */
            /* OK, decrement count, increment address */
            /* NTS: Remember that count == 0 means the host wanted to transfer 256 sectors */
            for (unsigned int cc=0;cc < multiple_sector_count;cc++) {
                progress_count++;
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

                if (!increment_current_address()) {
                    LOG_MSG("READ advance error\n");
                    abort_error();
                    return;
                }
            }

            /* cause another delay, another sector read */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,0.00001/*ms*/,controller->interface_index);
            break;
        case 0xC5:/* WRITE MULTIPLE */
            /* this is where the drive has accepted the sector, lowers DRQ, and begins executing the command */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,((progress_count == 0 && !faked_command) ? 0.1 : 0.00001)/*ms*/,controller->interface_index);
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

/* TODO: Your code should also be paying attention to the "transfer length" field
         in many of the commands here. Right now it doesn't matter. */
void IDEATAPICDROMDevice::atapi_cmd_completion() {
#if 0
    LOG_MSG("ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x to_host=%u\n",
        atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
        atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11],
        atapi_to_host);
#endif

    switch (atapi_cmd[0]) {
        case 0x00: /* TEST UNIT READY */
            if (common_spinup_response(/*spin up*/false,/*wait*/false))
                set_sense(0); /* <- nothing wrong */

            count = 0x03;
            state = IDE_DEV_READY;
            feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
            status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x03: /* REQUEST SENSE */
            count = 0x02;
            state = IDE_DEV_ATAPI_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            break;
        case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
            count = 0x02;
            state = IDE_DEV_ATAPI_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            break;
        case 0x25: /* READ CAPACITY */
            count = 0x02;
            state = IDE_DEV_ATAPI_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            break;
        case 0x2B: /* SEEK */
            if (common_spinup_response(/*spin up*/true,/*wait*/true)) {
                set_sense(0); /* <- nothing wrong */
                count = 0x02;
                state = IDE_DEV_ATAPI_BUSY;
                status = IDE_STATUS_BUSY;
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            }
            else {
                count = 0x03;
                state = IDE_DEV_READY;
                feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                controller->raise_irq();
                allow_writing = true;
            }
            break;
        case 0x12: /* INQUIRY */
            count = 0x02;
            state = IDE_DEV_ATAPI_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            break;
        case 0xA8: /* READ(12) */
            if (common_spinup_response(/*spin up*/true,/*wait*/true)) {
                set_sense(0); /* <- nothing wrong */

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
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 3)/*ms*/,controller->interface_index);
            }
            else {
                count = 0x03;
                state = IDE_DEV_READY;
                feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                controller->raise_irq();
                allow_writing = true;
            }
            break;
        case 0x28: /* READ(10) */
            if (common_spinup_response(/*spin up*/true,/*wait*/true)) {
                set_sense(0); /* <- nothing wrong */

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
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 3)/*ms*/,controller->interface_index);
            }
            else {
                count = 0x03;
                state = IDE_DEV_READY;
                feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                controller->raise_irq();
                allow_writing = true;
            }
            break;
        case 0x42: /* READ SUB-CHANNEL */
            if (common_spinup_response(/*spin up*/true,/*wait*/true)) {
                set_sense(0); /* <- nothing wrong */

                count = 0x02;
                state = IDE_DEV_ATAPI_BUSY;
                status = IDE_STATUS_BUSY;
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            }
            else {
                count = 0x03;
                state = IDE_DEV_READY;
                feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                controller->raise_irq();
                allow_writing = true;
            }
            break;
        case 0x43: /* READ TOC */
            if (common_spinup_response(/*spin up*/true,/*wait*/true)) {
                set_sense(0); /* <- nothing wrong */

                count = 0x02;
                state = IDE_DEV_ATAPI_BUSY;
                status = IDE_STATUS_BUSY;
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            }
            else {
                count = 0x03;
                state = IDE_DEV_READY;
                feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                controller->raise_irq();
                allow_writing = true;
            }
            break;
        case 0x45: /* PLAY AUDIO (1) */
        case 0x47: /* PLAY AUDIO MSF */
        case 0x4B: /* PAUSE/RESUME */
            if (common_spinup_response(/*spin up*/true,/*wait*/true)) {
                set_sense(0); /* <- nothing wrong */

                count = 0x02;
                state = IDE_DEV_ATAPI_BUSY;
                status = IDE_STATUS_BUSY;
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            }
            else {
                count = 0x03;
                state = IDE_DEV_READY;
                feature = ((sense[2]&0xF) << 4) | ((sense[2]&0xF) ? 0x04/*abort*/ : 0x00);
                status = IDE_STATUS_DRIVE_READY|((sense[2]&0xF) ? IDE_STATUS_ERROR:IDE_STATUS_DRIVE_SEEK_COMPLETE);
                controller->raise_irq();
                allow_writing = true;
            }
            break;
        case 0x55: /* MODE SELECT(10) */
            count = 0x00;   /* we will be accepting data */
            state = IDE_DEV_ATAPI_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            break;
        case 0x5A: /* MODE SENSE(10) */
            count = 0x02;
            state = IDE_DEV_ATAPI_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 1)/*ms*/,controller->interface_index);
            break;
        default:
            /* we don't know the command, immediately return an error */
            LOG_MSG("Unknown ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
                atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11]);

            abort_error();
            count = 0x03; /* no more data (command/data=1, input/output=1) */
            feature = 0xF4;
            controller->raise_irq();
            allow_writing = true;
            break;
    }
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
        host_writew(sector+(47*2),0x80|multiple_sector_max); /* <- READ/WRITE MULTIPLE MAX SECTORS */

    host_writew(sector+(48*2),0x0000);  /* :0  0=we do not support doubleword (32-bit) PIO */
    host_writew(sector+(49*2),0x0A00);  /* :13 0=Standby timer values managed by device */
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

    host_writed(sector+(60*2),ptotal);  /* total user addressable sectors (LBA) */
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
    host_writew(sector+(82*2),0x4208);  /* command set: NOP, DEVICE RESET[XXXXX], POWER MANAGEMENT */
    host_writew(sector+(83*2),0x4000);  /* command set: LBA48[XXXX] */
    host_writew(sector+(84*2),0x4000);  /* FIXME: ??? */
    host_writew(sector+(85*2),0x4208);  /* commands in 82 enabled */
    host_writew(sector+(86*2),0x4000);  /* commands in 83 enabled */
    host_writew(sector+(87*2),0x4000);  /* FIXME: ??? */
    host_writew(sector+(88*2),0x0000);  /* FIXME: ??? */
    host_writew(sector+(93*3),0x0000);  /* FIXME: ??? */

    /* ATA-8 integrity checksum */
    sector[510] = 0xA5;
    csum = 0; for (i=0;i < 511;i++) csum += sector[i];
    sector[511] = 0 - csum;
}

IDEATADevice::IDEATADevice(IDEController *c,unsigned char disk_index)
    : IDEDevice(c), id_serial("8086"), id_firmware_rev("8086"), id_model("DOSBox IDE disk"), bios_disk_index(disk_index) {
    sector_i = sector_total = 0;

    headshr = 0;
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
    CDROM_Interface *cdrom=NULL;

    if (!GetMSCDEXDrive(drive_index,&cdrom))
        return NULL;

    return cdrom;
}

void IDEATAPICDROMDevice::update_from_cdrom() {
    CDROM_Interface *cdrom = getMSCDEXDrive();
    if (cdrom == NULL) {
        LOG_MSG("WARNING: IDE update from CD-ROM failed, disk not available\n");
        return;
    }
}

void IDEATADevice::update_from_biosdisk() {
    imageDisk *dsk = getBIOSdisk();
    if (dsk == NULL) {
        LOG_MSG("WARNING: IDE update from BIOS disk failed, disk not available\n");
        return;
    }

    headshr = 0;
    geo_translate = false;
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

    /* If we can't divide the heads down, then pick a LBA-like mapping that is good enough.
     * Note that if what we pick does not evenly map to the INT 13h geometry, and the partition
     * contained within is not an LBA type FAT16/FAT32 partition, then Windows 95's IDE driver
     * will ignore this device and fall back to using INT 13h. For user convenience we will
     * print a warning to reminder the user of exactly that. */
    if (heads > 16) {
        unsigned long tmp;

        geo_translate = true;

        tmp = heads * cyls * sects;
        sects = 63;
        heads = 16;
        cyls = (tmp + ((63 * 16) - 1)) / (63 * 16);
        LOG_MSG("WARNING: Unable to reduce heads to 16 and below\n");
        LOG_MSG("If at all possible, please consider using INT 13h geometry with a head\n");
        LOG_MSG("cound that is easier to map to the BIOS, like 240 heads or 128 heads/track.\n");
        LOG_MSG("Some OSes, such as Windows 95, will not enable their 32-bit IDE driver if\n");
        LOG_MSG("a clean mapping does not exist between IDE and BIOS geometry.\n");
        LOG_MSG("Mapping BIOS DISK C/H/S %u/%u/%u as IDE %u/%u/%u (non-straightforward mapping)\n",
            (unsigned int)dsk->cylinders,
            (unsigned int)dsk->heads,
            (unsigned int)dsk->sectors,
            (unsigned int)cyls,
            (unsigned int)heads,
            (unsigned int)sects);
    }
    else {
        LOG_MSG("Mapping BIOS DISK C/H/S %u/%u/%u as IDE %u/%u/%u\n",
            (unsigned int)dsk->cylinders,
            (unsigned int)dsk->heads,
            (unsigned int)dsk->sectors,
            (unsigned int)cyls,
            (unsigned int)heads,
            (unsigned int)sects);
    }

    phys_heads = heads;
    phys_sects = sects;
    phys_cyls = cyls;
}

void IDE_Auto(signed char &index,bool &slave) {
    unsigned int i;

    index = -1;
    slave = false;
    for (i=0;i < MAX_IDE_CONTROLLERS;i++) {
        IDEController* c;
        if ((c=idecontroller[i]) == NULL) continue;
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

/* drive_index = drive letter 0...A to 25...Z */
void IDE_ATAPI_MediaChangeNotify(unsigned char drive_index) {
    for (unsigned int ide=0;ide < MAX_IDE_CONTROLLERS;ide++) {
        IDEController *c = idecontroller[ide];
        if (c == NULL) continue;
        for (unsigned int ms=0;ms < 2;ms++) {
            IDEDevice *dev = c->device[ms];
            if (dev == NULL) continue;
            if (dev->type == IDE_TYPE_CDROM) {
                IDEATAPICDROMDevice *atapi = (IDEATAPICDROMDevice*)dev;
                if (drive_index == atapi->drive_index) {
                    LOG_MSG("IDE ATAPI acknowledge media change for drive %c\n",drive_index+'A');
                    atapi->has_changed = true;
                    atapi->loading_mode = LOAD_INSERT_CD;
                    PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown,c->interface_index);
                    PIC_RemoveSpecificEvents(IDE_ATAPI_SpinUpComplete,c->interface_index);
                    PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion,c->interface_index);
                    PIC_AddEvent(IDE_ATAPI_CDInsertion,atapi->cd_insertion_time/*ms*/,c->interface_index);
                }
            }
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
        LOG_MSG("IDE: Controller %u %s already taken\n",index,slave?"slave":"master");
        return;
    }

    if (!GetMSCDEXDrive(drive_index,NULL)) {
        LOG_MSG("IDE: Asked to attach CD-ROM that does not exist\n");
        return;
    }

    dev = new IDEATAPICDROMDevice(c,drive_index);
    if (dev == NULL) return;
    dev->update_from_cdrom();
    c->device[slave?1:0] = (IDEDevice*)dev;
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

/* bios_disk_index = index into BIOS INT 13h disk array: imageDisk *imageDiskList[MAX_DISK_IMAGES]; */
void IDE_Hard_Disk_Attach(signed char index,bool slave,unsigned char bios_disk_index/*not INT13h, the index into DOSBox's BIOS drive emulation*/) {
    IDEController *c;
    IDEATADevice *dev;

    if (index < 0 || index >= MAX_IDE_CONTROLLERS) return;
    c = idecontroller[index];
    if (c == NULL) return;

    if (c->device[slave?1:0] != NULL) {
        LOG_MSG("IDE: Controller %u %s already taken\n",index,slave?"slave":"master");
        return;
    }

    if (imageDiskList[bios_disk_index] == NULL) {
        LOG_MSG("IDE: Asked to attach bios disk that does not exist\n");
        return;
    }

    dev = new IDEATADevice(c,bios_disk_index);
    if (dev == NULL) return;
    dev->update_from_biosdisk();
    c->device[slave?1:0] = (IDEDevice*)dev;
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
                         * a legitimate BIOS INT 13h call doing it's job. */
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
                        ide->drivehead = dev->drivehead = 0xE0u | (ms<<4u) | (lba>>24u); /* drive head and master/slave (WDCTRL test phase 9/10/17) */
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
                     *       wrap sector numbers across tracks. it fullfills the read
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
                    if (ata->headshr != 0 || ata->geo_translate) {
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
                         * a legitimate BIOS INT 13h call doing it's job. */
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
                        ide->drivehead = dev->drivehead = 0xA0u | (ms<<4u) | head; /* drive head and master/slave (WDCTRL test phase 9/10/17) */
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
//      int i;

        switch (dev->command) {
            case 0x30:/* WRITE SECTOR */
                disk = ata->getBIOSdisk();
                if (disk == NULL) {
                    LOG_MSG("ATA READ fail, bios disk N/A\n");
                    ata->abort_error();
                    dev->controller->raise_irq();
                    return;
                }

                sectcount = ata->count & 0xFF;
                if (sectcount == 0) sectcount = 256;
                if (drivehead_is_lba(ata->drivehead)) {
                    /* LBA */
                    sectorn = ((ata->drivehead & 0xFu) << 24u) | (unsigned int)ata->lba[0] |
                        ((unsigned int)ata->lba[1] << 8u) |
                        ((unsigned int)ata->lba[2] << 16u);
                }
                else {
                    /* C/H/S */
                    if (ata->lba[0] == 0) {
                        LOG_MSG("ATA sector 0 does not exist\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                    else if ((unsigned int)(ata->drivehead & 0xFu) >= (unsigned int)ata->heads ||
                        (unsigned int)ata->lba[0] > (unsigned int)ata->sects ||
                        (unsigned int)(ata->lba[1] | (ata->lba[2] << 8u)) >= (unsigned int)ata->cyls) {
                        LOG_MSG("C/H/S %u/%u/%u out of bounds %u/%u/%u\n",
                            (unsigned int)(ata->lba[1] | (ata->lba[2] << 8u)),
                            (unsigned int)(ata->drivehead&0xFu),
                            (unsigned int)(ata->lba[0]),
                            (unsigned int)ata->cyls,
                            (unsigned int)ata->heads,
                            (unsigned int)ata->sects);
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }

                    sectorn = ((ata->drivehead & 0xF) * ata->sects) +
                        (((unsigned int)ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) * ata->sects * ata->heads) +
                        ((unsigned int)ata->lba[0] - 1u);
                }

                if (disk->Write_AbsoluteSector(sectorn, ata->sector) != 0) {
                    LOG_MSG("Failed to write sector\n");
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
                ata->progress_count++;

                if (!ata->increment_current_address()) {
                    LOG_MSG("READ advance error\n");
                    ata->abort_error();
                    return;
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
                    LOG_MSG("ATA READ fail, bios disk N/A\n");
                    ata->abort_error();
                    dev->controller->raise_irq();
                    return;
                }

                sectcount = ata->count & 0xFF;
                if (sectcount == 0) sectcount = 256;
                if (drivehead_is_lba(ata->drivehead)) {
                    /* LBA */
                    sectorn = (((unsigned int)ata->drivehead & 0xFu) << 24u) | (unsigned int)ata->lba[0] |
                        ((unsigned int)ata->lba[1] << 8u) |
                        ((unsigned int)ata->lba[2] << 16u);
                }
                else {
                    /* C/H/S */
                    if (ata->lba[0] == 0) {
                        LOG_MSG("WARNING C/H/S access mode and sector==0\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                    else if ((unsigned int)(ata->drivehead & 0xF) >= (unsigned int)ata->heads ||
                        (unsigned int)ata->lba[0] > (unsigned int)ata->sects ||
                        (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) >= (unsigned int)ata->cyls) {
                        LOG_MSG("C/H/S %u/%u/%u out of bounds %u/%u/%u\n",
                            (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)),
                            (unsigned int)(ata->drivehead&0xF),
                            (unsigned int)ata->lba[0],
                            (unsigned int)ata->cyls,
                            (unsigned int)ata->heads,
                            (unsigned int)ata->sects);
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }

                    sectorn = ((ata->drivehead & 0xFu) * ata->sects) +
                        (((unsigned int)ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) * ata->sects * ata->heads) +
                        ((unsigned int)ata->lba[0] - 1u);
                }

                if (disk->Read_AbsoluteSector(sectorn, ata->sector) != 0) {
                    LOG_MSG("ATA read failed\n");
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

            case 0x40:/* READ SECTOR VERIFY WITH RETRY */
            case 0x41: /* READ SECTOR VERIFY WITHOUT RETRY */
                disk = ata->getBIOSdisk();
                if (disk == NULL) {
                    LOG_MSG("ATA READ fail, bios disk N/A\n");
                    ata->abort_error();
                    dev->controller->raise_irq();
                    return;
                }

                sectcount = ata->count & 0xFF;
                if (sectcount == 0) sectcount = 256;
                if (drivehead_is_lba(ata->drivehead)) {
                    /* LBA */
                    sectorn = (((unsigned int)ata->drivehead & 0xFu) << 24u) | (unsigned int)ata->lba[0] |
                        ((unsigned int)ata->lba[1] << 8u) |
                        ((unsigned int)ata->lba[2] << 16u);
                }
                else {
                    /* C/H/S */
                    if (ata->lba[0] == 0) {
                        LOG_MSG("WARNING C/H/S access mode and sector==0\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                    else if ((unsigned int)(ata->drivehead & 0xF) >= (unsigned int)ata->heads ||
                        (unsigned int)ata->lba[0] > (unsigned int)ata->sects ||
                        (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) >= (unsigned int)ata->cyls) {
                        LOG_MSG("C/H/S %u/%u/%u out of bounds %u/%u/%u\n",
                            (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)),
                            (unsigned int)(ata->drivehead&0xFu),
                            (unsigned int)ata->lba[0],
                            (unsigned int)ata->cyls,
                            (unsigned int)ata->heads,
                            (unsigned int)ata->sects);
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }

                    sectorn = (((unsigned int)ata->drivehead & 0xFu) * ata->sects) +
                        (((unsigned int)ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) * ata->sects * ata->heads) +
                        ((unsigned int)ata->lba[0] - 1u);
                }

                if (disk->Read_AbsoluteSector(sectorn, ata->sector) != 0) {
                    LOG_MSG("ATA read failed\n");
                    ata->abort_error();
                    dev->controller->raise_irq();
                    return;
                }

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
                ata->progress_count++;

                if (!ata->increment_current_address()) {
                    LOG_MSG("READ advance error\n");
                    ata->abort_error();
                    return;
                }

                ata->state = IDE_DEV_BUSY;
                ata->status = IDE_STATUS_BUSY;
                PIC_AddEvent(IDE_DelayedCommand,0.00001/*ms*/,dev->controller->interface_index);
                break;

            case 0xC4:/* READ MULTIPLE */
                disk = ata->getBIOSdisk();
                if (disk == NULL) {
                    LOG_MSG("ATA READ fail, bios disk N/A\n");
                    ata->abort_error();
                    dev->controller->raise_irq();
                    return;
                }

                sectcount = ata->count & 0xFF;
                if (sectcount == 0) sectcount = 256;
                if (drivehead_is_lba(ata->drivehead)) {
                    /* LBA */
                    sectorn = (((unsigned int)ata->drivehead & 0xFu) << 24u) | (unsigned int)ata->lba[0] |
                        ((unsigned int)ata->lba[1] << 8u) |
                        ((unsigned int)ata->lba[2] << 16u);
                }
                else {
                    /* C/H/S */
                    if (ata->lba[0] == 0) {
                        LOG_MSG("WARNING C/H/S access mode and sector==0\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                    else if ((unsigned int)(ata->drivehead & 0xF) >= (unsigned int)ata->heads ||
                        (unsigned int)ata->lba[0] > (unsigned int)ata->sects ||
                        (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) >= (unsigned int)ata->cyls) {
                        LOG_MSG("C/H/S %u/%u/%u out of bounds %u/%u/%u\n",
                            (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)),
                            (unsigned int)(ata->drivehead&0xF),
                            (unsigned int)ata->lba[0],
                            (unsigned int)ata->cyls,
                            (unsigned int)ata->heads,
                            (unsigned int)ata->sects);
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }

                    sectorn = ((ata->drivehead & 0xF) * ata->sects) +
                        (((unsigned int)ata->lba[1] | ((unsigned int)ata->lba[2] << 8u)) * ata->sects * ata->heads) +
                        ((unsigned int)ata->lba[0] - 1);
                }

                if ((512*ata->multiple_sector_count) > sizeof(ata->sector))
                    E_Exit("SECTOR OVERFLOW");

                for (unsigned int cc=0;cc < MIN((Bitu)ata->multiple_sector_count,(Bitu)sectcount);cc++) {
                    /* it would be great if the disk object had a "read multiple sectors" member function */
                    if (disk->Read_AbsoluteSector(sectorn+cc, ata->sector+(cc*512)) != 0) {
                        LOG_MSG("ATA read failed\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                }

                /* NTS: the way this command works is that the drive reads ONE sector, then fires the IRQ
                        and lets the host read it, then reads another sector, fires the IRQ, etc. One
                    IRQ signal per sector. We emulate that here by adding another event to trigger this
                    call unless the sector count has just dwindled to zero, then we let it stop. */
                /* NTS: The sector advance + count decrement is done in the I/O completion function */
                dev->state = IDE_DEV_DATA_READ;
                dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
                ata->prepare_read(0,512*MIN((Bitu)ata->multiple_sector_count,(Bitu)sectcount));
                dev->controller->raise_irq();
                break;

            case 0xC5:/* WRITE MULTIPLE */
                disk = ata->getBIOSdisk();
                if (disk == NULL) {
                    LOG_MSG("ATA READ fail, bios disk N/A\n");
                    ata->abort_error();
                    dev->controller->raise_irq();
                    return;
                }

                sectcount = ata->count & 0xFF;
                if (sectcount == 0) sectcount = 256;
                if (drivehead_is_lba(ata->drivehead)) {
                    /* LBA */
                    sectorn = (((unsigned int)ata->drivehead & 0xF) << 24) | (unsigned int)ata->lba[0] |
                        ((unsigned int)ata->lba[1] << 8) |
                        ((unsigned int)ata->lba[2] << 16);
                }
                else {
                    /* C/H/S */
                    if (ata->lba[0] == 0) {
                        LOG_MSG("ATA sector 0 does not exist\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                    else if ((unsigned int)(ata->drivehead & 0xF) >= (unsigned int)ata->heads ||
                        (unsigned int)ata->lba[0] > (unsigned int)ata->sects ||
                        (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8)) >= (unsigned int)ata->cyls) {
                        LOG_MSG("C/H/S %u/%u/%u out of bounds %u/%u/%u\n",
                            (unsigned int)(ata->lba[1] | ((unsigned int)ata->lba[2] << 8)),
                            (unsigned int)(ata->drivehead&0xF),
                            (unsigned int)ata->lba[0],
                            (unsigned int)ata->cyls,
                            (unsigned int)ata->heads,
                            (unsigned int)ata->sects);
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }

                    sectorn = ((unsigned int)(ata->drivehead & 0xF) * ata->sects) +
                        (((unsigned int)ata->lba[1] | ((unsigned int)ata->lba[2] << 8)) * ata->sects * ata->heads) +
                        ((unsigned int)ata->lba[0] - 1);
                }

                for (unsigned int cc=0;cc < MIN((Bitu)ata->multiple_sector_count,(Bitu)sectcount);cc++) {
                    /* it would be great if the disk object had a "write multiple sectors" member function */
                    if (disk->Write_AbsoluteSector(sectorn+cc, ata->sector+(cc*512)) != 0) {
                        LOG_MSG("Failed to write sector\n");
                        ata->abort_error();
                        dev->controller->raise_irq();
                        return;
                    }
                }

                for (unsigned int cc=0;cc < MIN((Bitu)ata->multiple_sector_count,(Bitu)sectcount);cc++) {
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
                    ata->progress_count++;

                    if (!ata->increment_current_address()) {
                        LOG_MSG("READ advance error\n");
                        ata->abort_error();
                        return;
                    }
                }

                /* begin another sector */
                sectcount = ata->count & 0xFF;
                if (sectcount == 0) sectcount = 256;
                dev->state = IDE_DEV_DATA_WRITE;
                dev->status = IDE_STATUS_DRQ|IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
                ata->prepare_write(0,512*MIN((Bitu)ata->multiple_sector_count,(Bitu)sectcount));
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
                LOG_MSG("Unknown delayed IDE/ATA command\n");
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
                    LOG_MSG("Unknown delayed IDE/ATAPI busy wait command\n");
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
                    dev->count = 0x01;  /* input/output == 0, command/data == 1 */
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
                    LOG_MSG("Unknown delayed IDE/ATAPI command\n");
                    dev->abort_error();
                    dev->controller->raise_irq();
                    break;
            }
        }
    }
    else {
        LOG_MSG("Unknown delayed command\n");
        dev->abort_error();
        dev->controller->raise_irq();
    }
}

void PC98_IDE_UpdateIRQ(void);

void IDEController::raise_irq() {
    irq_pending = true;
    if (IS_PC98_ARCH) {
        PC98_IDE_UpdateIRQ();
    }
    else {
        if (IRQ >= 0 && interrupt_enable) PIC_ActivateIRQ((unsigned int)IRQ);
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

Bitu IDEDevice::data_read(Bitu iolen) {
    (void)iolen;//UNUSED
    return 0xAAAAU;
}

void IDEDevice::data_write(Bitu v,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)v;//UNUSED
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

    faked_command = false;
    ide_select_delay = 0.5; /* 500us */
    ide_spinup_delay = 3000; /* 3 seconds */
    ide_spindown_delay = 1000; /* 1 second */
    ide_identify_command_delay = 0.01; /* 10us */
}

/* IDE controller -> upon writing bit 2 of alt (0x3F6) */
void IDEDevice::host_reset_complete() {
    status = 0x00;
    asleep = false;
    allow_writing = true;
    state = IDE_DEV_READY;
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
    if (!command_interruption_ok(cmd))
        return;

    /* if the drive is asleep, then writing a command wakes it up */
    interface_wakeup();

    /* drive is ready to accept command */
    allow_writing = false;
    command = cmd;
    switch (cmd) {
        case 0x08: /* DEVICE RESET */
            status = 0x00;
            drivehead &= 0x10; controller->drivehead = drivehead;
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
            drivehead &= 0x30; controller->drivehead = drivehead;
            count = 0x01;
            lba[0] = 0x01;
            feature = 0x04; /* abort */
            lba[1] = 0x14;  /* <- magic ATAPI identification */
            lba[2] = 0xEB;
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0xA0: /* ATAPI PACKET */
            if (feature & 1) {
                /* this code does not support DMA packet commands */
                LOG_MSG("Attempted DMA transfer\n");
                abort_error();
                count = 0x03; /* no more data (command/data=1, input/output=1) */
                feature = 0xF4;
                controller->raise_irq();
            }
            else {
                state = IDE_DEV_BUSY;
                status = IDE_STATUS_BUSY;
                atapi_to_host = (feature >> 2) & 1; /* 0=to device 1=to host */
                host_maximum_byte_count = ((unsigned int)lba[2] << 8) + (unsigned int)lba[1]; /* LBA field bits 23:8 are byte count */
                if (host_maximum_byte_count == 0) host_maximum_byte_count = 0x10000UL;
                PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 0.25)/*ms*/,controller->interface_index);
            }
            break;
        case 0xA1: /* IDENTIFY PACKET DEVICE */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : ide_identify_command_delay),controller->interface_index);
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
            feature = 0x04; /* abort */
            lba[1] = 0x14;  /* <- magic ATAPI identification */
            lba[2] = 0xEB;
            controller->raise_irq();
            allow_writing = true;
            break;
        default:
            LOG_MSG("Unknown IDE/ATAPI command %02X\n",cmd);
            abort_error();
            allow_writing = true;
            count = 0x03; /* no more data (command/data=1, input/output=1) */
            feature = 0xF4;
            controller->raise_irq();
            break;
    }
}

void IDEATADevice::writecommand(uint8_t cmd) {
    if (!command_interruption_ok(cmd))
        return;

    if (!faked_command) {
        if (drivehead_is_lba(drivehead)) {
            uint64_t n;

            n = ((unsigned int)(drivehead&0xF)<<24)+((unsigned int)lba[2]<<16)+((unsigned int)lba[1]<<8)+(unsigned int)lba[0];
            LOG(LOG_SB,LOG_DEBUG)("IDE ATA command %02x dh=0x%02x count=0x%02x lba=%07llx/%07llx\n",cmd,
                drivehead,count,(unsigned long long)n,
                (unsigned long long)(phys_sects * phys_cyls * phys_heads));
        }
        else {
            LOG(LOG_SB,LOG_DEBUG)("IDE ATA command %02x dh=0x%02x count=0x%02x chs=%02x/%02x/%02x\n",cmd,
                drivehead,count,((unsigned int)lba[2]<<8)+(unsigned int)lba[1],(unsigned int)(drivehead&0xF),(unsigned int)lba[0]);
        }

        LOG(LOG_SB,LOG_NORMAL)("IDE ATA command %02x",cmd);
    }

    /* if the drive is asleep, then writing a command wakes it up */
    interface_wakeup();

    /* FIXME: OAKCDROM.SYS is sending the hard disk command 0xA0 (ATAPI packet) for some reason. Why? */

    /* drive is ready to accept command */
    allow_writing = false;
    command = cmd;
    switch (cmd) {
        case 0x00: /* NOP */
            feature = 0x04;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_ERROR;
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x08: /* DEVICE RESET */
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            drivehead &= 0x10; controller->drivehead = drivehead;
            count = 0x01; lba[0] = 0x01; feature = 0x00;
            lba[1] = lba[2] = 0;
            /* NTS: Testing suggests that ATA hard drives DO fire an IRQ at this stage.
                    In fact, Windows 95 won't detect hard drives that don't fire an IRQ in desponse */
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: /* RECALIBRATE (1xh) */
        case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            /* "if the command is executed in CHS mode, then ... sector number register shall be 1.
             *  if executed in LAB mode, then ... sector number register shall be 0" */
            if (drivehead_is_lba(drivehead)) lba[0] = 0x00;
            else lba[0] = 0x01;
            drivehead &= 0x10; controller->drivehead = drivehead;
            lba[1] = lba[2] = 0;
            feature = 0x00;
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0x20: /* READ SECTOR */
            progress_count = 0;
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 0.1)/*ms*/,controller->interface_index);
            break;
        case 0x30: /* WRITE SECTOR */
            /* the drive does NOT signal an interrupt. it sets DRQ and waits for a sector
             * to be transferred to it before executing the command */
            progress_count = 0;
            state = IDE_DEV_DATA_WRITE;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ;
            prepare_write(0,512);
            break;
        case 0x40: /* READ SECTOR VERIFY WITH RETRY */
        case 0x41: /* READ SECTOR VERIFY WITHOUT RETRY */
            progress_count = 0;
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 0.1)/*ms*/,controller->interface_index);
            break;
        case 0x91: /* INITIALIZE DEVICE PARAMETERS */
            if ((unsigned int)count != (unsigned int)sects || (unsigned int)((drivehead&0xF)+1) != (unsigned int)heads) {
                if (count == 0) {
                    LOG_MSG("IDE warning: OS attempted to change geometry to invalid H/S %u/%u\n",
                        count,(drivehead&0xF)+1);
                    abort_error();
                    allow_writing = true;
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
            break;
        case 0xC4: /* READ MULTIPLE */
            progress_count = 0;
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : 0.1)/*ms*/,controller->interface_index);
            break;
        case 0xC5: /* WRITE MULTIPLE */
            /* the drive does NOT signal an interrupt. it sets DRQ and waits for a sector
             * to be transferred to it before executing the command */
            progress_count = 0;
            state = IDE_DEV_DATA_WRITE;
            status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRQ;
            prepare_write(0UL,512UL*MIN((unsigned long)multiple_sector_count,(unsigned long)(count == 0 ? 256 : count)));
            break;
        case 0xC6: /* SET MULTIPLE MODE */
            /* only sector counts 1, 2, 4, 8, 16, 32, 64, and 128 are legal by standard.
             * NTS: There's a bug in VirtualBox that makes 0 legal too! */
            if (count != 0 && count <= multiple_sector_max && is_power_of_2(count)) {
                multiple_sector_count = count;
                status = IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE;
            }
            else {
                feature = 0x04; /* abort error */
                abort_error();
            }
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0xA0:/*ATAPI PACKET*/
            /* We're not an ATAPI packet device!
             * Windows 95 seems to issue this at startup to hard drives. Duh. */
            /* fall through */
        case 0xA1: /* IDENTIFY PACKET DEVICE */
            /* We are not an ATAPI packet device.
             * Most MS-DOS drivers and Windows 95 like to issue both IDENTIFY ATA and IDENTIFY ATAPI commands.
             * I also gather from some contributers on the github comments that people think our "Unknown IDE/ATA command"
             * error message is part of some other error in the emulation. Rather than put up with that, we'll just
             * silently abort the command with an error. */
            abort_normal();
            status = IDE_STATUS_ERROR|IDE_STATUS_DRIVE_READY;
            drivehead &= 0x30; controller->drivehead = drivehead;
            count = 0x01;
            lba[0] = 0x01;
            feature = 0x04; /* abort */
            lba[1] = 0x00;
            lba[2] = 0x00;
            controller->raise_irq();
            allow_writing = true;
            break;
        case 0xEC: /* IDENTIFY DEVICE */
            state = IDE_DEV_BUSY;
            status = IDE_STATUS_BUSY;
            PIC_AddEvent(IDE_DelayedCommand,(faked_command ? 0.000001 : ide_identify_command_delay),controller->interface_index);
            break;
        default:
            LOG_MSG("Unknown IDE/ATA command %02X\n",cmd);
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
    drivehead = 0;

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
            LOG_MSG("WARNING: IDE interface %u: Insufficient resources assigned by dosbox.conf, and no appropriate default resources for this interface.",index);
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
            tmp[i+0] = (4 << 3) | 3;        /* IRQ resource */
            host_writew(tmp+i+1,1 << IRQ);
            tmp[i+3] = 0x09;            /* HTE=1 LTL=1 */
            i += 3+1;
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
        if (ide->interrupt_enable) {
            if (ide->irq_pending) ide->raise_irq();
        }
        else {
            if (IS_PC98_ARCH) {
                PC98_IDE_UpdateIRQ();
            }
            else {
                if (ide->IRQ >= 0) PIC_DeActivateIRQ((unsigned int)ide->IRQ);
            }
        }

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
        return (dev != NULL) ? dev->status : ide->status;
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

    switch (port) {
        case 0: /* 1F0 */
            ret = (dev != NULL) ? dev->data_read(iolen) : 0xFFFFFFFFUL;
            break;
        case 1: /* 1F1 */
            ret = (dev != NULL) ? dev->feature : 0x00;
            break;
        case 2: /* 1F2 */
            ret = (dev != NULL) ? dev->count : 0x00;
            break;
        case 3: /* 1F3 */
            ret = (dev != NULL) ? dev->lba[0] : 0x00;
            break;
        case 4: /* 1F4 */
            ret = (dev != NULL) ? dev->lba[1] : 0x00;
            break;
        case 5: /* 1F5 */
            ret = (dev != NULL) ? dev->lba[2] : 0x00;
            break;
        case 6: /* 1F6 */
            ret = ide->drivehead;
            break;
        case 7: /* 1F7 */
            /* if an IDE device exists at selection return it's status, else return our status */
            if (dev && dev->status & IDE_STATUS_BUSY) {
            }
            else if (dev == NULL && ide->status & IDE_STATUS_BUSY) {
            }
            else {
                ide->lower_irq();
            }

            ret = (dev != NULL) ? dev->status : ide->status;
            break;
    }

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
    else if (ide->status & IDE_STATUS_BUSY) {
        if (port == 6 && ((val>>4)&1) == ide->select) {
            /* some MS-DOS drivers like ATAPICD.SYS are just very pedantic about writing to port +6 to ensure the right drive is selected */
            return;
        }
        else {
            LOG_MSG("W-%03X %02X BUSY DROP [IDE]\n",(int)(port+ide->base_io),(int)val);
            return;
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

    switch (port) {
        case 0: /* 1F0 */
            if (dev) dev->data_write(val,iolen); /* <- TODO: what about 32-bit PIO modes? */
            break;
        case 1: /* 1F1 */
            if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
                dev->feature = val;
            break;
        case 2: /* 1F2 */
            if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
                dev->count = val;
            break;
        case 3: /* 1F3 */
            if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
                dev->lba[0] = val;
            break;
        case 4: /* 1F4 */
            if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
                dev->lba[1] = val;
            break;
        case 5: /* 1F5 */
            if (dev && dev->allow_writing) /* TODO: LBA48 16-bit wide register */
                dev->lba[2] = val;
            break;
        case 6: /* 1F6 */
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
        case 7: /* 1F7 */
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

