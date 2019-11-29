/*
 * Floppy controller emulation for DOSBox-X
 * (C) 2012 Jonathan Campbell

 * [insert open source license here]
 */

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

// largest sector size supported here. Size code represents size as 2 << (7 + code) i.e. code == 2 is 2 << 9 = 512 bytes
#define FLOPPY_MAX_SECTOR_SIZE              (2048u)
#define FLOPPY_MAX_SECTOR_SIZE_SZCODE       (11u/*2^11=2048*/ - 7u)

#include <math.h>
#include <assert.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "cpu.h"
#include "dma.h"
#include "ide.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "control.h"
#include "callback.h"
#include "bios_disk.h"

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
#endif

#define MAX_FLOPPY_CONTROLLERS 1

static unsigned char init_floppy = 0;

class FloppyController;

class FloppyDevice {
public:
	FloppyController *controller = NULL;
public:
	unsigned char current_track;
	bool select,motor;
	bool track0;
public:
	int		int13_disk;
public:
	FloppyDevice(FloppyController *c);
	void set_select(bool enable);	/* set selection on/off */
	void set_motor(bool enable);	/* set motor on/off */
	void motor_step(int dir);
	imageDisk *getImage();
	virtual ~FloppyDevice();
	double floppy_image_motor_position();
};

class FloppyController:public Module_base{
public:
	int IRQ;
	int DMA;
	unsigned short base_io;
	unsigned char interface_index;
	IO_ReadHandleObject ReadHandler[8];
	IO_WriteHandleObject WriteHandler[8];
	uint8_t digital_output_register;
	bool int13fakev86io;		/* on certain INT 13h calls in virtual 8086 mode, trigger fake CPU I/O traps */
	bool instant_mode;		/* make floppy operations instantaneous if true */
	bool data_register_ready;	/* 0x3F4 bit 7 */
	bool data_read_expected;	/* 0x3F4 bit 6 (DIO) if set CPU is expected to read from the controller */
	bool non_dma_mode;		/* 0x3F4 bit 5 (NDMA) */
	bool busy_status;		/* 0x3F4 bit 4 (BUSY). By "busy" the floppy controller is in the middle of an instruction */
	bool positioning[4];		/* 0x3F4 bit 0-3 floppy A...D in positioning mode */
	bool irq_pending;
	bool register_pnp;
/* FDC internal registers */
	uint8_t ST[4];			/* ST0..ST3 */
    uint8_t current_cylinder[4] = {};
/* buffers */
    uint8_t in_cmd[16] = {};
	uint8_t in_cmd_len;
	uint8_t in_cmd_pos;
    uint8_t out_res[16] = {};
	uint8_t out_res_len;
	uint8_t out_res_pos;
	unsigned int motor_steps;
	int motor_dir;
	float fdc_motor_step_delay;
	bool in_cmd_state;
	bool out_res_state;
public:
    void register_isapnp();
	bool dma_enabled();
	bool irq_enabled();
    bool drive_motor_on(unsigned char index);
	int drive_selected();
	void reset_cmd();
	void reset_res();
	void reset_io();
	void update_ST3();
	uint8_t fdc_data_read();
	void fdc_data_write(uint8_t b);
	void prepare_res_phase(uint8_t len);
	void on_dor_change(unsigned char b);
	void invalid_command_code();
	void on_fdc_in_command();
	void on_reset();
public:
	DmaChannel* dma;
	FloppyDevice* device[4];	/* Floppy devices */
public:
	FloppyController(Section* configuration,unsigned char index);
	void install_io_port();
	void raise_irq();
	void lower_irq();
	~FloppyController();
};

static FloppyController* floppycontroller[MAX_FLOPPY_CONTROLLERS]={NULL};

bool FDC_AssignINT13Disk(unsigned char drv) {
	if (drv >= 2) return false;
	FloppyController *fdc = floppycontroller[0];
	if (fdc == NULL) return false;
	FloppyDevice *dev = fdc->device[drv];

	if (dev != NULL) {
		/* sorry. move it aside */
		delete dev;
		fdc->device[drv] = NULL;
	}

	dev = fdc->device[drv] = new FloppyDevice(fdc);
	if (dev == NULL) return false;
	dev->int13_disk = drv;
	dev->set_select(fdc->drive_selected() == drv);

    if (IS_PC98_ARCH) {
        // HACK: Enable motor by default, until motor enable port 0xBE is implemented
        dev->set_motor(true);
    }

	LOG_MSG("FDC: Primary controller, drive %u assigned to INT 13h drive %u",drv,drv);
	return true;
}

bool FDC_UnassignINT13Disk(unsigned char drv) {
	if (drv >= 2) return false;
	FloppyController *fdc = floppycontroller[0];
	if (fdc == NULL) return false;
	FloppyDevice *dev = fdc->device[drv];

	if (dev != NULL) {
		/* sorry. move it aside */
		delete dev;
		fdc->device[drv] = NULL;
	}

	LOG_MSG("FDC: Primary controller, drive %u unassigned from INT 13h drive %u",drv,drv);
	return true;
}

static void fdc_baseio98_w(Bitu port,Bitu val,Bitu iolen);
static Bitu fdc_baseio98_r(Bitu port,Bitu iolen);
static void fdc_baseio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu fdc_baseio_r(Bitu port,Bitu iolen);

void FDC_MotorStep(Bitu idx/*which IDE controller*/) {
	FloppyController *fdc;
	FloppyDevice *dev;
	int devidx;

	if (idx >= MAX_FLOPPY_CONTROLLERS) return;
	fdc = floppycontroller[idx];
	if (fdc == NULL) return;

	devidx = fdc->drive_selected()&3;
	dev = fdc->device[devidx];

#if 0
	LOG_MSG("FDC: motor step. if=%u dev=%u rem=%u dir=%d current=%u\n",
		(int)idx,(int)devidx,(int)fdc->motor_steps,(int)fdc->motor_dir,(int)fdc->current_cylinder[devidx]);
#endif

	if (dev != NULL && dev->track0 && fdc->motor_dir < 0) {
		fdc->motor_steps = 0;
		fdc->current_cylinder[devidx] = 0;
	}

    if (fdc->motor_steps > 0) {
        fdc->motor_steps--;
        if ((fdc->motor_dir < 0 && fdc->current_cylinder[devidx] != 0x00) ||
            (fdc->motor_dir > 0 && fdc->current_cylinder[devidx] != 0xFF)) {
            fdc->current_cylinder[devidx] += fdc->motor_dir;
        }

        if (dev != NULL) {
            dev->motor_step(fdc->motor_dir);
            if (dev->track0) {
                fdc->motor_steps = 0;
                fdc->current_cylinder[devidx] = 0;
            }
        }
    }

	fdc->update_ST3();
	if (fdc->motor_steps != 0) {
		/* step again */
		PIC_AddEvent(FDC_MotorStep,fdc->fdc_motor_step_delay,idx);
	}
	else {
		/* done stepping */
		fdc->data_register_ready = 1;
		fdc->busy_status = 0;
		fdc->ST[0] &= 0x1F;
		fdc->ST[0] |= 0x20; /* seek completed (bit 5) */
		/* fire IRQ */
		fdc->raise_irq();
		/* no result phase */
		fdc->reset_io();

		/* real FDC's don't have this insight, but for DOSBox-X debugging... */
		if (dev != NULL && dev->current_track != fdc->current_cylinder[devidx])
			LOG_MSG("FDC: warning, after motor step FDC and drive are out of sync (fdc=%u drive=%u). OS or App needs to recalibrate\n",
				fdc->current_cylinder[devidx],dev->current_track);

//		LOG_MSG("FDC: motor step finished. current=%u\n",(int)(fdc->current_cylinder[devidx]));
	}
}

double FloppyDevice::floppy_image_motor_position() {
	const unsigned int motor_rpm = 300;

	if (!motor) return 0.0;
	return fmod((PIC_FullIndex() * motor_rpm/*rotations/minute*/) / 1000/*convert to seconds from milliseconds*/ / 60/*rotations/min to rotations/sec*/,1.0);
}

imageDisk *FloppyDevice::getImage() {
	if (int13_disk >= 0)
		return GetINT13FloppyDrive((unsigned char)int13_disk);

	return NULL;
}

void FloppyController::on_reset() {
	/* TODO: cancel DOSBox events corresponding to read/seek/etc */
	PIC_RemoveSpecificEvents(FDC_MotorStep,interface_index);
	motor_dir = 0;
	motor_steps = 0;
	for (size_t i=0;i < 4;i++) current_cylinder[i] = 0;
	busy_status = 0;
	ST[0] &= 0x3F;
	reset_io();
	lower_irq();
}

FloppyDevice::~FloppyDevice() {
}

FloppyDevice::FloppyDevice(FloppyController *c) {
    (void)c;//UNUSED
	motor = select = false;
	current_track = 0;
	int13_disk = -1;
	track0 = false;
}

void FloppyDevice::set_select(bool enable) {
	select = enable;
}

void FloppyDevice::set_motor(bool enable) {
	motor = enable;
}

void FloppyDevice::motor_step(int dir) {
	current_track += dir;
//	if (current_track < 0) current_track = 0;
	if (current_track > 84) current_track = 84;
	track0 = (current_track == 0);
}

bool FloppyController::drive_motor_on(unsigned char index) {
    if (index < 4) return !!((digital_output_register >> (4u + index)) & 1);
    return false;
}

int FloppyController::drive_selected() {
	return (digital_output_register & 3);
}

bool FloppyController::dma_enabled() {
	return (digital_output_register & 0x08); /* bit 3 of DOR controls DMA/IRQ enable */
}

bool FloppyController::irq_enabled() {
    /* IRQ seems to be enabled, always, on PC-98. There does not seem to be an enable bit. */
    if (IS_PC98_ARCH) return true;

	return (digital_output_register & 0x08); /* bit 3 of DOR controls DMA/IRQ enable */
}

static void FDC_Destroy(Section* sec) {
    (void)sec;//UNUSED
	for (unsigned int i=0;i < MAX_FLOPPY_CONTROLLERS;i++) {
		if (floppycontroller[i] != NULL) {
			delete floppycontroller[i];
			floppycontroller[i] = NULL;
		}
	}

	init_floppy = 0;
}

static void FDC_Init(Section* sec,unsigned char fdc_interface) {
	Section_prop *section=static_cast<Section_prop *>(sec);

	assert(fdc_interface < MAX_FLOPPY_CONTROLLERS);

	if (!section->Get_bool("enable"))
		return;

	if (!init_floppy) {
		AddExitFunction(AddExitFunctionFuncPair(FDC_Destroy));
		init_floppy = 1;
	}

	LOG(LOG_MISC,LOG_DEBUG)("Initializing floppy controller interface %u",fdc_interface);

    {
        FloppyController *fdc = floppycontroller[fdc_interface] = new FloppyController(sec,fdc_interface);
        fdc->install_io_port();

		PIC_SetIRQMask((unsigned int)(fdc->IRQ), false);
	}
}

void FDC_OnReset(Section *sec) {
    (void)sec;//UNUSED
	FDC_Init(control->GetSection("fdc, primary"),0);
}

void FDC_Primary_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing floppy controller emulation");

	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(FDC_OnReset));
}

void FloppyController::update_ST3() {
	FloppyDevice *dev = device[drive_selected()&3];

	/* FIXME: Should assemble bits from FloppyDevice objects and their boolean "signal" variables */
	ST[3] =
		0x20/*RDY*/ +
		0x08/*DOUBLE-SIDED SIGNAL*/+
		(drive_selected()&3);/*DRIVE SELECT*/

	if (dev != NULL)
		ST[3] |= (dev->track0 ? 0x10 : 0)/*TRACK 0 signal from device*/;
}

void FloppyController::reset_io() {
	reset_cmd();
	reset_res();
	busy_status=0;
	data_read_expected=0;
}

void FloppyController::reset_cmd() {
	in_cmd_len=0;
	in_cmd_pos=0;
	in_cmd_state=false;
}

void FloppyController::reset_res() {
	out_res_len=0;
	out_res_pos=0;
	out_res_state=false;
}

void FloppyController::register_isapnp() {
	if (register_pnp && base_io > 0) {
		unsigned char tmp[256];
		unsigned int i;

		const unsigned char h1[9] = {
			ISAPNP_SYSDEV_HEADER(
				ISAPNP_ID('P','N','P',0x0,0x7,0x0,0x0), /* PNP0700 Generic floppy controller */
				ISAPNP_TYPE(0x01,0x02,0x00),		/* type: Mass Storage Device / Floppy / Generic */
				0x0001 | 0x0002)
		};

		i = 0;
		memcpy(tmp+i,h1,9); i += 9;			/* can't disable, can't configure */
		/*----------allocated--------*/
		tmp[i+0] = (8 << 3) | 7;			/* IO resource */
		tmp[i+1] = 0x01;				/* 16-bit decode */
		host_writew(tmp+i+2,base_io);			/* min */
		host_writew(tmp+i+4,base_io);			/* max */
		tmp[i+6] = 0x01;				/* align */
		tmp[i+7] = 0x06;				/* length (FIXME: Emit as 7 unless we know IDE interface will not conflict) */
		i += 7+1;

		tmp[i+0] = (8 << 3) | 7;			/* IO resource (FIXME: Don't emit this if we know IDE interface will take port 0x3F7) */
		tmp[i+1] = 0x01;				/* 16-bit decode */
		host_writew(tmp+i+2,base_io+7);			/* min */
		host_writew(tmp+i+4,base_io+7);			/* max */
		tmp[i+6] = 0x01;				/* align */
		tmp[i+7] = 0x01;				/* length */
		i += 7+1;

		if (IRQ > 0) {
			tmp[i+0] = (4 << 3) | 3;		/* IRQ resource */
			host_writew(tmp+i+1,1 << IRQ);
			tmp[i+3] = 0x09;			/* HTE=1 LTL=1 */
			i += 3+1;
		}

		if (DMA >= 0) {
			tmp[i+0] = (5 << 3) | 2;		/* IRQ resource */
			tmp[i+1] = 1 << DMA;
			tmp[i+2] = 0x00;			/* 8-bit */
			i += 2+1;
		}

		tmp[i+0] = 0x79;				/* END TAG */
		tmp[i+1] = 0x00;
		i += 2;
		/*-------------possible-----------*/
		tmp[i+0] = 0x79;				/* END TAG */
		tmp[i+1] = 0x00;
		i += 2;
		/*-------------compatible---------*/
		tmp[i+0] = 0x79;				/* END TAG */
		tmp[i+1] = 0x00;
		i += 2;

		if (!ISAPNP_RegisterSysDev(tmp,i))
			LOG_MSG("ISAPNP register failed\n");
	}
}

FloppyController::FloppyController(Section* configuration,unsigned char index):Module_base(configuration){
	Section_prop * section=static_cast<Section_prop *>(configuration);
	int i;

	fdc_motor_step_delay = 400.0f / 80; /* FIXME: Based on 400ms seek time from track 0 to 80 */
	interface_index = index;
	data_register_ready = 1;
	data_read_expected = 0;
	non_dma_mode = 0;
	busy_status = 0;
	for (i=0;i < 4;i++) positioning[i] = false;
	for (i=0;i < 4;i++) ST[i] = 0x00;
    irq_pending = false;
    motor_steps = 0;
    motor_dir = 0;
	reset_io();

	digital_output_register = 0;
	device[0] = NULL;
	device[1] = NULL;
	device[2] = NULL;
	device[3] = NULL;
	base_io = 0;
	IRQ = -1;
	DMA = -1;

	update_ST3();

	int13fakev86io = section->Get_bool("int13fakev86io");
	instant_mode = section->Get_bool("instant mode");
	register_pnp = section->Get_bool("pnp");

	i = section->Get_int("irq");
	if (i > 0 && i <= 15) IRQ = i;

	i = section->Get_int("dma");
	if (i >= 0 && i <= 15) DMA = i;

	i = section->Get_hex("io");
	if (i >= 0x90 && i <= 0x3FF) base_io = i & ~7;

    if (IS_PC98_ARCH) {
        /* According to Neko Project II source code, and the Undocumented PC-98 reference:
         *
         * The primary controller at 0x90-0x94 is for 3.5" drives, uses IRQ 11 (INT42), and DMA channel 2.
         * The secondary controller at 0xC8-0xCC is for 5.25" drives, uses IRQ 10 (INT41), and DMA channel 3. */
        if (IRQ < 0) IRQ = (index == 1) ? 10/*INT41*/ : 11/*INT42*/;
        if (DMA < 0) DMA = (index == 1) ? 3 : 2;

        if (base_io == 0) {
            if (index == 0) base_io = 0x90;
            if (index == 1) base_io = 0xC8;
        }
    }
    else {
        if (IRQ < 0) IRQ = 6;
        if (DMA < 0) DMA = 2;

        if (base_io == 0) {
            if (index == 0) base_io = 0x3F0;
            if (index == 1) base_io = 0x370;
        }
    }

	dma = GetDMAChannel(DMA);
}

void FloppyController::install_io_port(){
	if (base_io != 0) {
		LOG_MSG("FDC installing to io=%03xh IRQ=%d DMA=%d\n",base_io,IRQ,DMA);
        if (IS_PC98_ARCH) {
            WriteHandler[0].Install(base_io+0,fdc_baseio98_w,IO_MA);    // 0x90 / 0xC8
            ReadHandler[0].Install(base_io+0,fdc_baseio98_r,IO_MA);     // 0x90 / 0xC8
            WriteHandler[1].Install(base_io+2,fdc_baseio98_w,IO_MA);    // 0x92 / 0xCA
            ReadHandler[1].Install(base_io+2,fdc_baseio98_r,IO_MA);     // 0x92 / 0xCA
            WriteHandler[2].Install(base_io+4,fdc_baseio98_w,IO_MA);    // 0x94 / 0xCC
            ReadHandler[2].Install(base_io+4,fdc_baseio98_r,IO_MA);     // 0x94 / 0xCC
        }
        else {
            for (unsigned int i=0;i < 8;i++) {
                if (i != 6) { /* does not use port 0x3F6 */
                    WriteHandler[i].Install(base_io+i,fdc_baseio_w,IO_MA);
                    ReadHandler[i].Install(base_io+i,fdc_baseio_r,IO_MA);
                }
            }
        }
	}
}

FloppyController::~FloppyController() {
	unsigned int i;

	for (i=0;i < 4;i++) {
		if (device[i] != NULL) {
			delete device[i];
			device[i] = NULL;
		}
	}
}

FloppyController *match_fdc_controller(Bitu port) {
	unsigned int i;

	for (i=0;i < MAX_FLOPPY_CONTROLLERS;i++) {
		FloppyController *fdc = floppycontroller[i];
		if (fdc == NULL) continue;
		if (fdc->base_io != 0U && fdc->base_io == (port&0xFFF8U)) return fdc;
	}

	return NULL;
}

/* when DOR port is written */
void FloppyController::on_dor_change(unsigned char b) {
	unsigned char chg = b ^ digital_output_register;

	/* !RESET line */
	if (chg & 0x04) {
		if (!(b&0x04)) { /* if bit 2 == 0 s/w is resetting the controller */
			LOG_MSG("FDC: Reset\n");
			on_reset();
		}
		else {
			LOG_MSG("FDC: Reset complete\n");
			/* resetting the FDC on real hardware apparently fires another IRQ */
			raise_irq();
		}
	}

	/* drive select */
	if (chg & 0x03) {
		int o,n;

		o = drive_selected();
		n = b & 3;
		if (o >= 0) {
			LOG_MSG("FDC: Drive select from %c to %c\n",o+'A',n+'A');
			if (device[o] != NULL) device[o]->set_select(false);
			if (device[n] != NULL) device[n]->set_select(true);
			update_ST3();
		}
	}

	/* DMA/IRQ enable */
	if (chg & 0x08 && IRQ >= 0) {
		if ((b&0x08) && irq_pending) PIC_ActivateIRQ((unsigned int)IRQ);
		else PIC_DeActivateIRQ((unsigned int)IRQ);
	}

	/* drive motors */
	if (chg & 0xF0) {
		LOG_MSG("FDC: Motor control {A,B,C,D} = {%u,%u,%u,%u}\n",
			(b>>4)&1,(b>>5)&1,(b>>6)&1,(b>>7)&1);

		for (unsigned int i=0;i < 4;i++) {
			if (device[i] != NULL) device[i]->set_motor((b&(0x10<<i))?true:false);
		}
	}

	digital_output_register = b;
}

/* IDE code needs to know if port 3F7 will be taken by FDC emulation */
bool fdc_takes_port_3F7() {
	return (match_fdc_controller(0x3F7) != NULL);
}

void FloppyController::prepare_res_phase(uint8_t len) {
	data_read_expected = 1;
	out_res_pos = 0;
	out_res_len = len;
	out_res_state = true;
}

void FloppyController::invalid_command_code() {
	reset_res();
	prepare_res_phase(1);
	out_res[0] = ST[0] = 0x80;
}

uint8_t FloppyController::fdc_data_read() {
	if (busy_status) {
		if (out_res_state) {
			if (out_res_pos < out_res_len) {
				uint8_t b = out_res[out_res_pos++];
				if (out_res_pos >= out_res_len) reset_io();
				return b;
			}
			else {
				reset_io();
			}
		}
		else {
			reset_io();
		}
	}

	return 0xFF;
}

void FloppyController::on_fdc_in_command() {
	imageDisk *image=NULL;
	FloppyDevice *dev;
	int devidx;

	in_cmd_state = false;
	devidx = drive_selected();
	dev = device[devidx];
	if (dev != NULL) image = dev->getImage();

	switch (in_cmd[0]&0x1F) {
		case 0x0A:
			break;
		default:
			LOG_MSG("FDC: Command len=%u %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				in_cmd_len,
				in_cmd[0],in_cmd[1],in_cmd[2],in_cmd[3],in_cmd[4],
				in_cmd[5],in_cmd[6],in_cmd[7],in_cmd[8],in_cmd[9]);
			break;
	}

	switch (in_cmd[0]&0x1F) {
		case 0x03: /* Specify */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |   0    0    0    0    0    0    1    1
			 *   1 |   <---- SRT ----->    <----- HUT ---->
			 *   2 |   <-------------- HLT ---------->   ND
			 * -----------------------------------------------
			 *   3     total
			 */
			reset_res(); // TODO: Do something with this
			reset_io();
			break;
		case 0x04: /* Check Drive Status */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST3
			 * -----------------------------------------------
			 *   1     total
			 */
			reset_res();
			prepare_res_phase(1);
			out_res[0] = ST[3];
			ST[0] = 0x00 | devidx;
			break;
		case 0x05: /* Write data (0101) */
		case 0x09: /* Write deleted data (1001) */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST0
			 *   1 |               Register ST1
			 *   2 |               Register ST2
			 *   3 |             Logical cylinder
			 *   4 |               Logical head
			 *   5 |              Logical sector
			 *   6 |           Logical sector size
			 * -----------------------------------------------
			 *   7     total
			 */
			/* must have a device present. must have an image. device motor and select must be enabled.
			 * current physical cylinder position must be within range of the image. request must have MFM bit set. */
			dma = GetDMAChannel(DMA);
			if (dev != NULL && dma != NULL && dev->motor && dev->select && image != NULL && (in_cmd[0]&0x40)/*MFM=1*/ &&
				current_cylinder[devidx] < image->cylinders && (in_cmd[1]&4U?1U:0U) <= image->heads &&
				(in_cmd[1]&4U?1U:0U) == in_cmd[3] && in_cmd[2] == current_cylinder[devidx] &&
				in_cmd[5] <= FLOPPY_MAX_SECTOR_SIZE_SZCODE && in_cmd[4] > 0U && in_cmd[4] <= image->sectors) {
				unsigned char sector[FLOPPY_MAX_SECTOR_SIZE];
				bool fail = false;

                const unsigned int sector_size_bytes = (1u << (in_cmd[5]+7u));
                assert(sector_size_bytes <= FLOPPY_MAX_SECTOR_SIZE);

				/* TODO: delay related to how long it takes for the disk to rotate around to the sector requested */
				reset_res();
				ST[0] = 0x00 | devidx;

                out_res[3] = in_cmd[2];
                out_res[4] = in_cmd[3];
                out_res[5] = in_cmd[4];		/* the sector passing under the head at this time */
                {
                    unsigned int sz = (unsigned int)image->sector_size;
                    out_res[6] = 0;			    /* 128 << 2 == 512 bytes/sector */
                    while (sz > 128u && out_res[6] < FLOPPY_MAX_SECTOR_SIZE_SZCODE) {
                        out_res[6]++;
                        sz >>= 1u;
                    }
                }

				while (!fail && !dma->tcount/*terminal count*/) {
					/* if we're reading past the track, fail */
					if (in_cmd[4] > image->sectors) {
						fail = true;
						break;
					}

					/* DMA transfer */
					dma->Register_Callback(0);
					if (dma->Read(sector_size_bytes,sector) != sector_size_bytes) {
                        LOG(LOG_MISC,LOG_DEBUG)("FDC: DMA read failed");
                        fail = true;
                        break;
                    }

					/* write sector */
					Bit8u err = image->Write_Sector(in_cmd[3]/*head*/,in_cmd[2]/*cylinder*/,in_cmd[4]/*sector*/,sector,sector_size_bytes);
					if (err != 0x00) {
						fail = true;
						break;
					}

					/* if we're at the last sector of the track according to program, then stop */
					if (in_cmd[4] == in_cmd[6]) break;

                    /* next sector (TODO "multi-track" mode) */
                    if (in_cmd[4] == image->sectors)
                        in_cmd[4] = 1;
                    else
                        in_cmd[4]++;
				}

				if (fail) {
					ST[0] = (ST[0] & 0x3F) | 0x80;
					ST[1] = (1 << 0)/*missing address mark*/ | (1 << 2)/*no data*/;
					ST[2] = (1 << 0)/*missing data address mark*/;
				}
				{
					prepare_res_phase(7);
					out_res[0] = ST[0];
					out_res[1] = ST[1];
					out_res[2] = ST[2];
				}
			}
			else {
				/* TODO: real floppy controllers will pause for up to 1/2 a second before erroring out */
				reset_res();
				ST[0] = (ST[0] & 0x1F) | 0x80;
				ST[1] = (1 << 0)/*missing address mark*/ | (1 << 2)/*no data*/;
				ST[2] = (1 << 0)/*missing data address mark*/;

                prepare_res_phase(7);
                out_res[0] = ST[0];
                out_res[1] = ST[1];
                out_res[2] = ST[2];
            }
			raise_irq();
			break;
		case 0x06: /* Read data (0110) */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST0
			 *   1 |               Register ST1
			 *   2 |               Register ST2
			 *   3 |             Logical cylinder
			 *   4 |               Logical head
			 *   5 |              Logical sector
			 *   6 |           Logical sector size
			 * -----------------------------------------------
			 *   7     total
			 */
			/* must have a device present. must have an image. device motor and select must be enabled.
			 * current physical cylinder position must be within range of the image. request must have MFM bit set. */
			dma = GetDMAChannel(DMA);
			if (dev != NULL && dma != NULL && dev->motor && dev->select && image != NULL && (in_cmd[0]&0x40)/*MFM=1*/ &&
				current_cylinder[devidx] < image->cylinders && (in_cmd[1]&4U?1U:0U) <= image->heads &&
				(in_cmd[1]&4U?1U:0U) == in_cmd[3] && in_cmd[2] == current_cylinder[devidx] &&
				in_cmd[5] <= FLOPPY_MAX_SECTOR_SIZE_SZCODE && in_cmd[4] > 0U && in_cmd[4] <= image->sectors) {
				unsigned char sector[FLOPPY_MAX_SECTOR_SIZE];
				bool fail = false;

                const unsigned int sector_size_bytes = (1u << (in_cmd[5]+7u));
                assert(sector_size_bytes <= FLOPPY_MAX_SECTOR_SIZE);

				/* TODO: delay related to how long it takes for the disk to rotate around to the sector requested */
				reset_res();
				ST[0] = 0x00 | devidx;

                out_res[3] = in_cmd[2];
                out_res[4] = in_cmd[3];
                out_res[5] = in_cmd[4];		/* the sector passing under the head at this time */
                {
                    unsigned int sz = (unsigned int)image->sector_size;
                    out_res[6] = 0;			    /* 128 << 2 == 512 bytes/sector */
                    while (sz > 128u && out_res[6] < FLOPPY_MAX_SECTOR_SIZE_SZCODE) {
                        out_res[6]++;
                        sz >>= 1u;
                    }
                }

				while (!fail && !dma->tcount/*terminal count*/) {
					/* if we're reading past the track, fail */
					if (in_cmd[4] > image->sectors) {
						fail = true;
						break;
					}

					/* read sector */
					Bit8u err = image->Read_Sector(in_cmd[3]/*head*/,in_cmd[2]/*cylinder*/,in_cmd[4]/*sector*/,sector,sector_size_bytes);
					if (err != 0x00) {
						fail = true;
						break;
					}

					/* DMA transfer */
					dma->Register_Callback(0);
					if (dma->Write(sector_size_bytes,sector) != sector_size_bytes) {
                        LOG(LOG_MISC,LOG_DEBUG)("FDC: DMA write failed during read");
                        fail = true;
                        break;
                    }

					/* if we're at the last sector of the track according to program, then stop */
					if (in_cmd[4] == in_cmd[6]) break;

                    /* next sector (TODO "multi-track" mode) */
                    if (in_cmd[4] == image->sectors)
                        in_cmd[4] = 1;
                    else
                        in_cmd[4]++;
				}

				if (fail) {
					ST[0] = (ST[0] & 0x3F) | 0x80;
					ST[1] = (1 << 0)/*missing address mark*/ | (1 << 2)/*no data*/;
					ST[2] = (1 << 0)/*missing data address mark*/;
				}
				{
					prepare_res_phase(7);
					out_res[0] = ST[0];
					out_res[1] = ST[1];
					out_res[2] = ST[2];
				}
			}
			else {
				/* TODO: real floppy controllers will pause for up to 1/2 a second before erroring out */
				reset_res();
				ST[0] = (ST[0] & 0x1F) | 0x80;
				ST[1] = (1 << 0)/*missing address mark*/ | (1 << 2)/*no data*/;
				ST[2] = (1 << 0)/*missing data address mark*/;

                prepare_res_phase(7);
                out_res[0] = ST[0];
                out_res[1] = ST[1];
                out_res[2] = ST[2];
            }
			raise_irq();
			break;
		case 0x07: /* Calibrate drive */
			ST[0] = 0x20 | devidx;
			if (instant_mode) {
				/* move head to track 0 */
				current_cylinder[devidx] = 0;
				/* fire IRQ */
				raise_irq();
				/* no result phase */
				reset_io();
			}
			else {
				/* delay due to stepping the head to the desired cylinder */
				motor_steps = 79; /* calibrate is said to max out at 79. motor step routine will STOP when drive sets track0 signal */
				motor_dir = -1; /* always step backwards */

				/* the command takes time to move the head */
				data_register_ready = 0;
				busy_status = 1;

				/* and make it happen */
				PIC_AddEvent(FDC_MotorStep,(motor_steps > 0 ? fdc_motor_step_delay : 0.1)/*ms*/,interface_index);

				/* return now */
				return;
			}
			break;
		case 0x08: /* Check Interrupt Status */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST0
			 *   1 |              Current Cylinder
			 * -----------------------------------------------
			 *   2     total
			 */
			if (irq_pending) {
				lower_irq(); /* doesn't cause IRQ, clears IRQ */
				reset_res();
				prepare_res_phase(2);
				out_res[0] = ST[0];
				out_res[1] = current_cylinder[devidx];
			}
			else {
				/* if no pending IRQ, signal error.
				 * this is considered standard floppy controller behavior.
				 * this also fixes an issue where Windows 3.1 QIC tape backup software like Norton Backup (Norton Desktop 3.0)
				 * will "hang" Windows 3.1 polling the FDC in an endless loop if we don't return this error to say that no
				 * more IRQs are pending. */
				reset_res();
				ST[0] = (ST[0] & 0x3F) | 0x80;
				out_res[0] = ST[0];
				prepare_res_phase(1);
			}
			break;
		case 0x0A: /* Read ID */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST0
			 *   1 |               Register ST1
			 *   2 |               Register ST2
			 *   3 |             Logical cylinder
			 *   4 |               Logical head
			 *   5 |              Logical sector
			 *   6 |           Logical sector size
			 * -----------------------------------------------
			 *   7     total
			 */
			/* must have a device present. must have an image. device motor and select must be enabled.
			 * current physical cylinder position must be within range of the image. request must have MFM bit set. */
            ST[0] &= ~0x20;
			if (dev != NULL && dev->motor && dev->select && image != NULL && (in_cmd[0]&0x40)/*MFM=1*/ &&
				current_cylinder[devidx] < image->cylinders && (in_cmd[1]&4U?1U:0U) <= image->heads) {
				int ns = (int)floor(dev->floppy_image_motor_position() * image->sectors);
				/* TODO: minor delay to emulate time for one sector to pass under the head */
				reset_res();
				out_res[0] = ST[0];
				out_res[1] = ST[1];
				out_res[2] = ST[2];
				out_res[3] = current_cylinder[devidx];
				out_res[4] = (in_cmd[1]&4?1:0);
				out_res[5] = ns + 1;		/* the sector passing under the head at this time */
                {
                    unsigned int sz = (unsigned int)image->sector_size;
                    out_res[6] = 0;			    /* 128 << 2 == 512 bytes/sector */
                    while (sz > 128u && out_res[6] < FLOPPY_MAX_SECTOR_SIZE_SZCODE) {
                        out_res[6]++;
                        sz >>= 1u;
                    }
                }
				prepare_res_phase(7);
			}
			else {
				/* TODO: real floppy controllers will pause for up to 1/2 a second before erroring out */
				reset_res();
				ST[0] = (ST[0] & 0x3F) | 0x80;
				ST[1] = (1 << 0)/*missing address mark*/ | (1 << 2)/*no data*/;
				ST[2] = (1 << 0)/*missing data address mark*/;
				prepare_res_phase(1);
				out_res[0] = ST[0];
			}
			raise_irq();
			break;
		case 0x0C: /* Read deleted data (1100) */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST0
			 *   1 |               Register ST1
			 *   2 |               Register ST2
			 *   3 |             Logical cylinder
			 *   4 |               Logical head
			 *   5 |              Logical sector
			 *   6 |           Logical sector size
			 * -----------------------------------------------
			 *   7     total
			 */
			/* the raw images used by DOSBox cannot represent "deleted sectors", so always fail */
			reset_res();
			ST[0] = (ST[0] & 0x1F) | 0x80;
			ST[1] = (1 << 0)/*missing address mark*/ | (1 << 2)/*no data*/;
			ST[2] = (1 << 0)/*missing data address mark*/;
			prepare_res_phase(1);
			break;
		case 0x0E: /* Dump registers (response) */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |                PCN-Drive 0
			 *   1 |                PCN-Drive 1
			 *   2 |                PCN-Drive 2
			 *   3 |                PCN-Drive 3
			 *   4 |   <----- SRT ---->    <----- HUT ---->
			 *   5 |   <------------ HLT ------------>   ND
			 *   6 |   <------------- SC/EOT ------------->
			 *   7 |  LOCK  0   D3   D2   D1   D0   GAP WGATE
			 *   8 |   0   EIS EFIFO POLL  <--- FIFOTHR -->
			 *   9 |                   PRETRK
			 * -----------------------------------------------
			 *  10     total
			 */
			reset_res();
			prepare_res_phase(10);
			out_res[0] = current_cylinder[0];
			out_res[1] = current_cylinder[1];
			out_res[2] = current_cylinder[2];
			out_res[3] = current_cylinder[3];
			out_res[4] = 0x88;			// FIXME
			out_res[5] = 0x40;			// FIXME
			out_res[6] = 18;			// FIXME
			out_res[7] = 0;				// FIXME
			out_res[8] = 0x78;			// FIXME
			out_res[9] = 0x00;			// FIXME
			break;
		case 0x0F: /* Seek Head */
			ST[0] = 0x00 | drive_selected();
			if (instant_mode) {
				/* move head to whatever track was wanted */
				current_cylinder[devidx] = in_cmd[2]; /* from 3rd byte of command */
				/* fire IRQ */
				raise_irq();
				/* no result phase */
				reset_io();
			}
			else {
				/* delay due to stepping the head to the desired cylinder */
				motor_steps = (unsigned int)abs((int)in_cmd[2] - (int)current_cylinder[devidx]);
				motor_dir = in_cmd[2] > current_cylinder[devidx] ? 1 : -1;

				/* the command takes time to move the head */
				data_register_ready = 0;
				busy_status = 1;

				/* and make it happen */
				PIC_AddEvent(FDC_MotorStep,(motor_steps > 0 ? fdc_motor_step_delay : 0.1)/*ms*/,interface_index);

				/* return now */
				return;
			}
			break;
		case 0x13: /* Configure */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |   0    0    0    1    0    0    1    1
			 *   1 |   0    0    0    0    0    0    0    0
			 *   2 |   0   EIS EFIFO POLL  <--- FIFOTHR -->
			 *   3 |                 PRETRK
			 * -----------------------------------------------
			 *   4     total
			 */
			reset_res(); // TODO: Do something with this
			reset_io();
			break;
		default:
			LOG_MSG("FDC: Unknown command %02xh (somehow passed first check)\n",in_cmd[0]);
			reset_io();
			break;
	}

	switch (in_cmd[0]&0x1F) {
		case 0x0A:
			break;
		default:
			LOG_MSG("FDC: Response len=%u %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				out_res_len,
				out_res[0],out_res[1],out_res[2],out_res[3],out_res[4],
				out_res[5],out_res[6],out_res[7],out_res[8],out_res[9]);
			break;
	}
}

void FloppyController::fdc_data_write(uint8_t b) {
	if (!busy_status) {
		/* we're starting another command */
		reset_io();
		in_cmd[0] = b;
		in_cmd_len = 1;
		in_cmd_pos = 1;
		busy_status = true;
		in_cmd_state = true;

		/* right away.. how long is the command going to be? */
		switch (in_cmd[0]&0x1F) {
			case 0x03: /* Specify */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    0    0    0    1    1
				 *   1 |   <---- SRT ----->    <----- HUT ---->
				 *   2 |   <-------------- HLT ---------->   ND
				 * -----------------------------------------------
				 *   3     total
				 */
				in_cmd_len = 3;
				break;
			case 0x04: /* Check Drive Status */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    0    0    1    0    0
				 *   1 |   x    x    x    x    x   HD  DR1  DR0
				 * -----------------------------------------------
				 *   2     total
				 */
				in_cmd_len = 2;
				break;
			case 0x05: /* Write data (0101) */
			case 0x09: /* Write deleted data (1001) */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |  MT  MFM    0    0    0    1    0    1   
				 *   1 |   0    0    0    0    0   HD  DR1  DR0
				 *   2 |             Logical cylinder
				 *   3 |               Logical head
				 *   4 |          Logical sector (start)
				 *   5 |           Logical sector size
				 *   6 |               End of track               
				 *   7 |               Gap 3 length
				 *   8 |            Special sector size
				 * -----------------------------------------------
				 *   9     total
				 */
				in_cmd_len = 9;
				break;
			case 0x06: /* Read data (0110) */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |  MT  MFM   SK    0    0    1    1    0   
				 *   1 |   0    0    0    0    0   HD  DR1  DR0
				 *   2 |             Logical cylinder
				 *   3 |               Logical head
				 *   4 |          Logical sector (start)
				 *   5 |           Logical sector size
				 *   6 |               End of track               
				 *   7 |               Gap 3 length
				 *   8 |            Special sector size
				 * -----------------------------------------------
				 *   9     total
				 */
				in_cmd_len = 9;
				break;
			case 0x07: /* Calibrate drive (move head to track 0) */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    0    0    1    1    1
				 *   1 |   x    x    x    x    x    0  DR1  DR0
				 * -----------------------------------------------
				 *   2     total
				 */
				in_cmd_len = 2;
				break;
			case 0x08: /* Check Interrupt Status */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    0    1    0    0    0
				 * -----------------------------------------------
				 *   1     total
				 */
				break;
			case 0x0A: /* Read ID */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0  MFM    0    0    1    0    1    0
				 *   1 |   0    0    0    0    0   HD  DR1  DR0
				 * -----------------------------------------------
				 *   2     total
				 */
				in_cmd_len = 2;
				break;
			case 0x0C: /* Read deleted data (1100) */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |  MT  MFM   SK    0    0    1    1    0   
				 *   1 |   0    0    0    0    0   HD  DR1  DR0
				 *   2 |             Logical cylinder
				 *   3 |               Logical head
				 *   4 |          Logical sector (start)
				 *   5 |           Logical sector size
				 *   6 |               End of track               
				 *   7 |               Gap 3 length
				 *   8 |            Special sector size
				 * -----------------------------------------------
				 *   9     total
				 */
				in_cmd_len = 9;
				break;
			case 0x0E: /* Dump registers */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    0    1    1    1    0
				 * -----------------------------------------------
				 *   1     total
				 */
				in_cmd_len = 1;
				break;
			case 0x0F: /* Seek Head */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    0    1    1    1    1
				 *   1 |   x    x    x    x    x   HD  DR1  DR0
				 *   2 |                Cylinder
				 * -----------------------------------------------
				 *   3     total
				 */
				if (in_cmd[0]&0x80) { /* Reject Seek Relative (1xfh) most FDC's I've tested don't support it */
					LOG_MSG("FDC: Seek Relative not supported\n");
					/* give Invalid Command Code 0x80 as response */
					invalid_command_code();
					reset_cmd();
				}
				else {
					in_cmd_len = 3;
				}
				break;
			case 0x13: /* Configure */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0    0    0    1    0    0    1    1
				 *   1 |   0    0    0    0    0    0    0    0
				 *   2 |   0   EIS EFIFO POLL  <--- FIFOTHR -->
				 *   3 |                 PRETRK
				 * -----------------------------------------------
				 *   4     total
				 */
				in_cmd_len = 4;
				break;
			default:
				LOG_MSG("FDC: Unknown command (first byte %02xh)\n",in_cmd[0]);
				/* give Invalid Command Code 0x80 as response */
				invalid_command_code();
				reset_cmd();
				break;
		}

		if (in_cmd_state && in_cmd_pos >= in_cmd_len)
			on_fdc_in_command();
	}
	else if (in_cmd_state) {
		if (in_cmd_pos < in_cmd_len)
			in_cmd[in_cmd_pos++] = b;

		if (in_cmd_pos >= in_cmd_len)
			on_fdc_in_command();
	}
	else {
		LOG_MSG("FDC: Unknown state!\n");
		on_reset();
	}
}

static void fdc_baseio98_w(Bitu port,Bitu val,Bitu iolen) {
	FloppyController *fdc = match_fdc_controller(port);
	if (fdc == NULL) {
		LOG_MSG("WARNING: port read from I/O port not registered to FDC, yet callback triggered\n");
		return;
	}

//	LOG_MSG("FDC: Write port 0x%03x data 0x%02x irq_at_time=%u\n",port,val,fdc->irq_pending);

	if (iolen > 1) {
		LOG_MSG("WARNING: FDC unusual port write %03xh val=%02xh len=%u, port I/O should be 8-bit\n",(int)port,(int)val,(int)iolen);
	}

	switch (port&7) {
		case 2: /* data */
			if (!fdc->data_register_ready) {
				LOG_MSG("WARNING: FDC data write when data port not ready\n");
			}
			else if (fdc->data_read_expected) {
				LOG_MSG("WARNING: FDC data write when data port ready but expecting I/O read\n");
			}
			else {
				fdc->fdc_data_write(val&0xFF);
			}
			break;
		default:
			LOG_MSG("DEBUG: FDC write port %03xh val %02xh len=%u\n",(int)port,(int)val,(int)iolen);
			break;
	}
}

static Bitu fdc_baseio98_r(Bitu port,Bitu iolen) {
	FloppyController *fdc = match_fdc_controller(port);
	unsigned char b;

	if (fdc == NULL) {
		LOG_MSG("WARNING: port read from I/O port not registered to FDC, yet callback triggered\n");
		return ~(0UL);
	}

//	LOG_MSG("FDC: Read port 0x%03x irq_at_time=%u\n",port,fdc->irq_pending);

	if (iolen > 1) {
		LOG_MSG("WARNING: FDC unusual port read %03xh len=%u, port I/O should be 8-bit\n",(int)port,(int)iolen);
	}

	switch (port&7) {
		case 0: /* main status */
			b =	(fdc->data_register_ready ? 0x80 : 0x00) +
				(fdc->data_read_expected ? 0x40 : 0x00) +
				(fdc->non_dma_mode ? 0x20 : 0x00) +
				(fdc->busy_status ? 0x10 : 0x00) +
				(fdc->positioning[3] ? 0x08 : 0x00) +
				(fdc->positioning[2] ? 0x04 : 0x00) +
				(fdc->positioning[1] ? 0x02 : 0x00) +
				(fdc->positioning[0] ? 0x01 : 0x00);
//			LOG_MSG("FDC: read status 0x%02x\n",b);
			return b;
		case 2: /* data */
			if (!fdc->data_register_ready) {
				LOG_MSG("WARNING: FDC data read when data port not ready\n");
				return ~(0UL);
			}
			else if (!fdc->data_read_expected) {
				LOG_MSG("WARNING: FDC data read when data port ready but expecting I/O write\n");
				return ~(0UL);
			}

			b = fdc->fdc_data_read();
//			LOG_MSG("FDC: read data 0x%02x\n",b);
			return b;
		default:
			LOG_MSG("DEBUG: FDC read port %03xh len=%u\n",(int)port,(int)iolen);
			break;
	}

	return ~(0UL);
}

static void fdc_baseio_w(Bitu port,Bitu val,Bitu iolen) {
	FloppyController *fdc = match_fdc_controller(port);
	if (fdc == NULL) {
		LOG_MSG("WARNING: port read from I/O port not registered to FDC, yet callback triggered\n");
		return;
	}

//	LOG_MSG("FDC: Write port 0x%03x data 0x%02x irq_at_time=%u\n",port,val,fdc->irq_pending);

	if (iolen > 1) {
		LOG_MSG("WARNING: FDC unusual port write %03xh val=%02xh len=%u, port I/O should be 8-bit\n",(int)port,(int)val,(int)iolen);
	}

	switch (port&7) {
		case 2: /* digital output port */
			fdc->on_dor_change(val&0xFF);
			break;
		case 5: /* data */
			if (!fdc->data_register_ready) {
				LOG_MSG("WARNING: FDC data write when data port not ready\n");
			}
			else if (fdc->data_read_expected) {
				LOG_MSG("WARNING: FDC data write when data port ready but expecting I/O read\n");
			}
			else {
				fdc->fdc_data_write(val&0xFF);
			}
			break;
		default:
			LOG_MSG("DEBUG: FDC write port %03xh val %02xh len=%u\n",(int)port,(int)val,(int)iolen);
			break;
	}
}

static Bitu fdc_baseio_r(Bitu port,Bitu iolen) {
	FloppyController *fdc = match_fdc_controller(port);
	unsigned char b;

	if (fdc == NULL) {
		LOG_MSG("WARNING: port read from I/O port not registered to FDC, yet callback triggered\n");
		return ~(0UL);
	}

//	LOG_MSG("FDC: Read port 0x%03x irq_at_time=%u\n",port,fdc->irq_pending);

	if (iolen > 1) {
		LOG_MSG("WARNING: FDC unusual port read %03xh len=%u, port I/O should be 8-bit\n",(int)port,(int)iolen);
	}

	switch (port&7) {
		case 4: /* main status */
			b =	(fdc->data_register_ready ? 0x80 : 0x00) +
				(fdc->data_read_expected ? 0x40 : 0x00) +
				(fdc->non_dma_mode ? 0x20 : 0x00) +
				(fdc->busy_status ? 0x10 : 0x00) +
				(fdc->positioning[3] ? 0x08 : 0x00) +
				(fdc->positioning[2] ? 0x04 : 0x00) +
				(fdc->positioning[1] ? 0x02 : 0x00) +
				(fdc->positioning[0] ? 0x01 : 0x00);
//			LOG_MSG("FDC: read status 0x%02x\n",b);
			return b;
		case 5: /* data */
			if (!fdc->data_register_ready) {
				LOG_MSG("WARNING: FDC data read when data port not ready\n");
				return ~(0UL);
			}
			else if (!fdc->data_read_expected) {
				LOG_MSG("WARNING: FDC data read when data port ready but expecting I/O write\n");
				return ~(0UL);
			}

			b = fdc->fdc_data_read();
//			LOG_MSG("FDC: read data 0x%02x\n",b);
			return b;
		default:
			LOG_MSG("DEBUG: FDC read port %03xh len=%u\n",(int)port,(int)iolen);
			break;
	}

	return ~(0UL);
}

void FloppyController::raise_irq() {
	irq_pending = true;
	if (irq_enabled() && IRQ >= 0) PIC_ActivateIRQ((unsigned int)IRQ);
}

void FloppyController::lower_irq() {
	irq_pending = false;
	if (IRQ >= 0) PIC_DeActivateIRQ((unsigned int)IRQ);
}

void BIOS_Post_register_FDC() {
	for (size_t i=0;i < MAX_FLOPPY_CONTROLLERS;i++) {
        if (floppycontroller[i] != NULL)
            floppycontroller[i]->register_isapnp();
    }
}

