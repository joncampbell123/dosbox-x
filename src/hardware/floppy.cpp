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
#include "bios.h"
#include "floppy.h"
#include "imagedisk_d88.h"

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
#endif

#define MAX_FLOPPY_CONTROLLERS 1

static unsigned char init_floppy = 0;

/* ---------------------------------------------------------------------------------------------
 * Drive mechanics, shared by the FDC emulation and the INT 13h emulation.
 *
 * Indexed by INT 13h floppy drive number, which is also the drive number on the primary FDC.
 * The head position, the motor and the rotation of the disk are kept here so that both see
 * the same drive no matter which of them accessed it last, as on real hardware where the BIOS
 * itself programs the FDC.
 *
 * Timing model:
 *  - The disk turns at 300 RPM (360 RPM for 1.2MB 5.25" media). Its angle is taken from the
 *    time the motor was switched on. Sectors are laid out in order around the track as written
 *    by an IBM (System 34 MFM) format, with the gaps between them (see FloppyTrackLayout), so
 *    that a driver reading one sector at a time can catch the next sector's ID field in the
 *    gap after the previous sector, as on real hardware.
 *  - The data field passes under the head at the data rate of the media, and the FDC hands the
 *    bytes to DMA as they come (FDC_DMA_CHUNK bytes at a time).
 *  - The drive takes FLOPPY_SPINUP_MS after the motor is switched on to come up to speed, unless
 *    the motor was switched off less than FLOPPY_COAST_MS ago and the disk is still turning.
 *  - Stepping takes the step rate time given to the FDC by the SPECIFY command (the BIOS uses
 *    its disk base table for that), plus the head settle time from the disk base table for the BIOS.
 *  - The head is loaded for reads and writes, taking the head load time unless it is still
 *    loaded from an access less than the head unload time ago.
 *  - A sector that is not found costs two revolutions (two index pulses), as on a real FDC.
 * --------------------------------------------------------------------------------------------- */

#define FLOPPY_SPINUP_MS            300.0   /* motor on to disk up to speed */
#define FLOPPY_COAST_MS             500.0   /* a disk still turns this long after the motor goes off */
#define FLOPPY_MAX_TRACK            83u     /* the head cannot step beyond this */
#define FDC_INSTANT_DELAY_MS        0.01    /* instant mode: commands still complete after the OUT that started them */
#define FDC_DMA_CHUNK               16u     /* data bytes moved per DMA step (the 82077AA FIFO is 16 bytes) */

struct FloppyMechanics {
	unsigned int	track = 0;			/* physical head position */
	bool		motor = false;			/* motor is on */
	double		motor_on_at = 0;		/* PIC_FullIndex() when the motor was switched on */
	double		motor_off_at = -1e9;		/* PIC_FullIndex() when the motor was switched off */
	double		head_unload_at = 0;		/* the head unloads at this time */
};

static FloppyMechanics floppy_mech[4];

extern bool imageDiskChange[MAX_DISK_IMAGES];

bool FLOPPY_InstantMode(void) {
	Section_prop *section = static_cast<Section_prop *>(control->GetSection("fdc, primary"));
	return section != NULL && section->Get_bool("instant mode");
}

static unsigned int floppy_spt(imageDisk *image) {
	return (image != NULL && image->sectors > 0) ? image->sectors : 18u;
}

static unsigned int floppy_track_bytes(imageDisk *image) {
	return floppy_spt(image) * (image != NULL ? image->sector_size : 512u);
}

/* D88 images say what kind of disk they are: 2D, 2DD or 2HD (-1 if not a D88 image) */
static int floppy_d88_type(imageDisk *image) {
	if (image == NULL || image->class_id != imageDisk::ID_D88) return -1;
	return static_cast<imageDiskD88*>(image)->fd_type_major;
}

/* 1.2MB 5.25" disks (and PC-98 2HD disks) turn at 360 RPM, everything else at 300 RPM */
static double floppy_rpm(imageDisk *image) {
	const int d88 = floppy_d88_type(image);
	if (d88 >= 0) return d88 == imageDiskD88::DISKTYPE_2HD ? 360.0 : 300.0;

	if (image != NULL && floppy_track_bytes(image) >= 7680u/*15 x 512*/ && floppy_track_bytes(image) <= 8192u/*8 x 1024*/)
		return 360.0;

	return 300.0;
}

static double floppy_rev_ms(imageDisk *image) {
	return 60000.0 / floppy_rpm(image);
}

/* data rate (in the encoding of the FDC data rate select register) the media needs:
 * 0 = 500Kbps (high density), 3 = 1Mbps (extra density), 2 = 250Kbps or 1 = 300Kbps (double density) */
static unsigned int floppy_media_rate(imageDisk *image) {
	const int d88 = floppy_d88_type(image);
	if (d88 >= 0) return d88 == imageDiskD88::DISKTYPE_2HD ? 0 : 2;

	const unsigned int b = floppy_track_bytes(image);

	if (b <= 5120u) return 2;	/* 9 x 512 or less per track: DD (250Kbps, 300Kbps in a 360 RPM drive) */
	if (b <= 10240u) return 0;	/* 15 or 18 x 512: HD */
	return 3;			/* 36 x 512: ED */
}

static bool floppy_rate_matches(imageDisk *image,unsigned int rate) {
	const unsigned int want = floppy_media_rate(image);
	if (want == 2) return rate == 2 || rate == 1;
	return rate == want;
}

/* data rate in Kbps from the data rate select register encoding */
static double floppy_rate_kbps(unsigned int rate) {
	static const double kbps[4] = { 500, 300, 250, 1000 };
	return kbps[rate & 3];
}

/* SPECIFY timings in milliseconds (82077AA datasheet table 5-10), scaled for the data rate */
static double floppy_srt_ms(unsigned int srt,unsigned int rate) {
	return (16.0 - (double)(srt & 15)) * 500.0 / floppy_rate_kbps(rate);
}

static double floppy_hut_ms(unsigned int hut,unsigned int rate) {
	return ((hut & 15) != 0 ? (double)(hut & 15) * 16.0 : 256.0) * 500.0 / floppy_rate_kbps(rate);
}

static double floppy_hlt_ms(unsigned int hlt,unsigned int rate) {
	return ((hlt & 0x7F) != 0 ? (double)(hlt & 0x7F) * 2.0 : 256.0) * 500.0 / floppy_rate_kbps(rate);
}

static void floppy_motor(FloppyMechanics &m,bool on) {
	const double now = PIC_FullIndex();

	/* if the disk is still turning from before, it carries on where it was */
	if (on && !m.motor && (now - m.motor_off_at) >= FLOPPY_COAST_MS) m.motor_on_at = now;
	if (!on && m.motor) m.motor_off_at = now;
	m.motor = on;
}

/* the time the disk is up to speed, if the motor is on */
static double floppy_ready_at(const FloppyMechanics &m) {
	return m.motor_on_at + FLOPPY_SPINUP_MS;
}

/* time from t until the given position (0 = index, 1 = one revolution) passes under the head */
static double floppy_time_to_angle(const FloppyMechanics &m,double t,double target,double rev) {
	double a = fmod((t - m.motor_on_at) / rev,1.0);
	if (a < 0) a += 1.0;

	double w = target - a;
	if (w < -1e-9) w += 1.0;
	if (w < 0) w = 0;
	return w * rev;
}

/* Layout of a track in bytes from the index hole, as written by an IBM System 34 (MFM) format:
 * gap 4a, sync, index address mark and gap 1 (146 bytes), then for each sector the ID field
 * (sync, ID address mark, C/H/R/N, CRC: 22 bytes), gap 2 (22 bytes), the data field (sync and
 * data address mark: 16 bytes, then the data and the CRC) and gap 3. Gap 3 takes up what is
 * left of the track, at most 116 bytes. */
struct FloppyTrackLayout {
	double		rev_ms;			/* time per revolution */
	double		track_bytes;		/* bytes per revolution at the data rate of the media */
	double		byte_ms;		/* time per byte */
	double		first_id;		/* position of the first ID field */
	double		sector_bytes;		/* sector, including gap 3 */
	unsigned int	spt;			/* sectors per track */
	unsigned int	ss;			/* bytes per sector */

	/* ID field of a sector (1-based, sector numbers beyond the track wrap around) */
	double id_start(unsigned int sector) const {
		return first_id + (double)((sector > 0 ? sector - 1u : 0u) % spt) * sector_bytes;
	}
	double id_end(unsigned int sector) const { return id_start(sector) + 22.0; }
	double data_start(unsigned int sector) const { return id_start(sector) + 22.0 + 22.0 + 16.0; }
	double data_end(unsigned int sector) const { return data_start(sector) + ss + 2.0; }
	/* time from t until a position on the track passes under the head */
	double time_to(const FloppyMechanics &m,double t,double pos) const {
		return floppy_time_to_angle(m,t,pos / track_bytes,rev_ms);
	}
};

static FloppyTrackLayout floppy_layout(imageDisk *image) {
	FloppyTrackLayout L;
	const unsigned int media_rate = floppy_media_rate(image);
	double kbps = floppy_rate_kbps(media_rate);

	/* double density media in a 360 RPM drive is read at 300Kbps */
	if (media_rate == 2 && floppy_rpm(image) >= 360.0) kbps = 300;

	L.rev_ms = floppy_rev_ms(image);
	L.track_bytes = kbps * 1000.0 / 8.0 * L.rev_ms / 1000.0;
	L.byte_ms = L.rev_ms / L.track_bytes;
	L.spt = floppy_spt(image);
	L.ss = image != NULL ? image->sector_size : 512u;
	if (L.ss > FLOPPY_MAX_SECTOR_SIZE) L.ss = FLOPPY_MAX_SECTOR_SIZE;

	const double per_sector = 62.0 + L.ss;	/* ID field, gap 2, data field */
	double gap3 = (L.track_bytes - 146.0 - L.spt * per_sector) / L.spt;
	if (gap3 > 116.0) gap3 = 116.0;

	if (gap3 >= 0) {
		L.first_id = 146.0;
		L.sector_bytes = per_sector + gap3;
	}
	else {
		/* more sectors than an IBM format fits (DMF, XDF...): no gaps to speak of */
		L.first_id = 0;
		L.sector_bytes = L.track_bytes / L.spt;
	}

	return L;
}

/* A physical track as the controller sees it: the sectors on it (from the image) and the position
 * of each ID field, laid out like FloppyTrackLayout but with each sector's own size and density
 * (an FM byte takes as long as two MFM bytes). */
struct FloppyTrackMap {
	FloppyTrackLayout			L;
	std::vector<imageDiskTrackSector>	secs;
	std::vector<double>			id_pos;
};

static void floppy_track_map(imageDisk *image,unsigned int cylinder,unsigned int head,FloppyTrackMap &M) {
	M.L = floppy_layout(image);
	M.secs.clear();
	M.id_pos.clear();
	if (image == NULL || !image->Get_Track(cylinder,head,M.secs) || M.secs.empty()) return;

	std::vector<double> len(M.secs.size());
	double total = 0;
	for (size_t k=0;k < M.secs.size();k++) {
		len[k] = (62.0 + M.secs[k].data_size) * (M.secs[k].fm ? 2.0 : 1.0);
		total += len[k];
	}

	double first = 146.0,gap3 = (M.L.track_bytes - first - total) / M.secs.size(),scale = 1.0;
	if (gap3 > 116.0) gap3 = 116.0;
	if (gap3 < 0) {
		/* more on the track than fits: squeeze it */
		first = 0;
		gap3 = 0;
		scale = M.L.track_bytes / total;
	}

	double pos = first;
	for (size_t k=0;k < M.secs.size();k++) {
		M.id_pos.push_back(pos);
		pos += (len[k] + gap3) * scale;
	}
}

class FloppyController;

class FloppyDevice {
public:
	FloppyController *controller = NULL;
public:
	bool select;
public:
	int		int13_disk;
public:
	FloppyDevice(FloppyController *c);
	void set_select(bool enable);	/* set selection on/off */
	void set_motor(bool enable);	/* set motor on/off */
	void motor_step(int dir);
	imageDisk *getImage();
	FloppyMechanics &mech();
	bool track0();
	bool motor();
	virtual ~FloppyDevice();
};

/* ST status registers:
 *
 * ST[0]:
 *    bit[7..6]----interrupt code  0=normal command end 1=abnormal command end 2=invalid command 3=abnormal termination from polling
 *    bit[5]-------seek end (SEEK or RECALIBRATE)
 *    bit[4]-------equipment check, TRK0 failed to signal after 80 step pulses from RECALIBRATE, or RELATIVE SEEK went past track 0
 *    bit[3]-------not ready, set on attempt to read/write when not ready, or to side 1 of single-sided drive
 *    bit[2]-------current head address
 *    bit[1..0]----current drive select
 *
 * ST[1]:
 *    bit[7]-------end of cylinder, tried to read/write past last sector of track, if TC not issued after read/write
 *    bit[6]-------always zero?
 *    bit[5]-------data (CRC) error, either data or ID field
 *    bit[4]-------overrun/underrun (CPU or DMA didn't act fast enough to transfer data)
 *    bit[3]-------always zero?
 *    bit[2]-------no data (sector not found, cannot find the proper track sequence)
 *    bit[1]-------not writeable, write protect set (WP pin)
 *    bit[0]-------missing address mark, did not find ID or data address mark on the track
 *
 * ST[2]:
 *    bit[7]-------always zero
 *    bit[6]-------control mark, read data matched deleted sector, read data deleted matched non-deleted sector
 *    bit[5]-------data field (CRC) error
 *    bit[4]-------wrong track, sector ID field has different track number than expected
 *    bit[3]-------scan equal hit
 *    bit[2]-------scan not satisfied
 *    bit[1]-------track address from sector ID is different than what the chip thinks internally and is equal to 0xFF which means bad track with hard error (IBM)
 *    bit[0]-------missing address mark, cannot detect data address mark
 *
 * ST[3]:
 *    bit[7]-------fault status
 *    bit[6]-------write protect status
 *    bit[5]-------ready status
 *    bit[4]-------track 0 status
 *    bit[3]-------two sided signal
 *    bit[2]-------head select status
 *    bit[1..0]----status of DS1..DS0 pins
 */

/* register layout of the controller ([fdc, primary] mode) */
enum FDCMode {
	FDC_MODE_PS2=0,		/* PS/2: status registers A/B, DIR with data rate */
	FDC_MODE_PS2_MODEL30,	/* PS/2 model 30: inverted disk change, DIR with DMA gate */
	FDC_MODE_AT,		/* PC/AT: DIR has only the disk change bit */
	FDC_MODE_XT		/* PC/XT: no DIR, no data rate select (always 250Kbps) */
};

/* controller chip ([fdc, primary] chip), decides which commands exist */
enum FDCChip {
	FDC_CHIP_82077AA=0,
	FDC_CHIP_82072,
	FDC_CHIP_UPD765
};

/* command in execution phase */
enum FDCExecPhase {
	EXEC_NONE=0,
	EXEC_WAIT_MOTOR,	/* waiting for the disk to turn */
	EXEC_ID,		/* the ID field of the sector of a read/write/verify has passed under the head */
	EXEC_DATA,		/* data field bytes of the sector are passing under the head */
	EXEC_READID,		/* the ID of a sector has passed under the head */
	EXEC_FORMAT,		/* a sector of a format has passed under the head */
	EXEC_FAIL		/* done looking for the sector (two index pulses) */
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
	FDCMode mode;
	FDCChip chip;
	bool int13fakev86io;		/* on certain INT 13h calls in virtual 8086 mode, trigger fake CPU I/O traps */
	bool instant_mode;		/* make floppy operations instantaneous if true */
	bool data_register_ready;	/* 0x3F4 bit 7 */
	bool data_read_expected;	/* 0x3F4 bit 6 (DIO) if set CPU is expected to read from the controller */
	bool non_dma_mode;		/* 0x3F4 bit 5 (NDMA) */
	bool busy_status;		/* 0x3F4 bit 4 (BUSY). By "busy" the floppy controller is in the middle of an instruction */
	bool positioning[4];		/* 0x3F4 bit 0-3 floppy A...D in positioning mode (seek/recalibrate in progress) */
	bool irq_pending;
	bool irq_from_result;		/* the IRQ came from entering the result phase, reading the result clears it */
	bool register_pnp;
	/* FDC internal registers */
	uint8_t ST[4];			/* ST0..ST3 */
	uint8_t current_cylinder[4] = {};	/* PCN, the cylinder the controller thinks each drive is on */
	uint8_t data_rate;		/* data rate select (DSR/CCR) 0=500K 1=300K 2=250K 3=1M */
	uint8_t spec_srt,spec_hut,spec_hlt; /* SPECIFY step rate, head unload, head load */
	bool spec_nd;			/* SPECIFY non-DMA mode */
	bool cfg_eis;			/* CONFIGURE implied seek */
	bool cfg_efifo;			/* CONFIGURE FIFO disabled */
	bool cfg_poll;			/* CONFIGURE drive polling disabled */
	uint8_t cfg_fifothr,cfg_pretrk;
	bool lock;			/* LOCK, CONFIGURE settings survive a reset */
	uint8_t perpendicular;
	uint8_t last_eot;
	/* PC-98 FD interface */
	uint8_t pc98_control;		/* control register (I/O 94h): RST, FRY, DMAE, MTON, TMSK, TTRG */
	uint8_t pc98_mode;		/* dual-use FD I/F (I/O 0BEh): EMTON, FDD EXC (1MB access), PORT EXC (1MB I/F) */
	uint8_t pc98_mode144;		/* 3-mode FD I/F (I/O 4BEh): 1.44MB access mode */
	/* READY of each drive as the uPD765 last polled it */
	bool drive_ready[4];
	/* seek/recalibrate in progress, per drive */
	struct {
		bool active;
		bool recalibrate;
		int dir;
		uint8_t target;
		unsigned int steps_left;
	} seek[4];
	/* interrupt status per drive for SENSE INTERRUPT STATUS (seek/recalibrate end) */
	bool sense_pending[4];
	uint8_t sense_st0[4];
	unsigned int reset_sense_count;	/* drive polling interrupts after a reset, one per drive */
	/* command in execution phase */
	FDCExecPhase exec_phase;
	uint8_t exec_drive,exec_hds;
	uint8_t exec_C,exec_H,exec_R,exec_N;
	unsigned int exec_count;		/* sectors done */
	unsigned char exec_buf[FLOPPY_MAX_SECTOR_SIZE]; /* the sector passing under the head */
	unsigned int exec_pos,exec_size;	/* bytes of it transferred, bytes in it */
	bool exec_tc;				/* terminal count, the transfer is over after this sector */
	bool exec_data_error;			/* the sector has a data CRC error */
	unsigned int exec_index;		/* the sector's position in the track's list of sectors */
	bool exec_diag_nd;			/* READ DIAGNOSTIC: an ID did not match */
	bool exec_cm;				/* the data field has the other kind of address mark (deleted/normal) */
	bool exec_skip;				/* the sector is skipped (SK) */
	double exec_density;			/* 2 for FM, 1 for MFM: time per byte relative to the track layout */
	unsigned int exec_fmt_sc;		/* format: sectors per track */
	uint8_t exec_fmt_fill;
	double exec_slot_ms;			/* format: time per sector */
	/* buffers */
	uint8_t in_cmd[16] = {};
	uint8_t in_cmd_len;
	uint8_t in_cmd_pos;
	uint8_t out_res[16] = {};
	uint8_t out_res_len;
	uint8_t out_res_pos;
	bool in_cmd_state;
	bool out_res_state;
public:
	void register_isapnp();
	bool dma_enabled();
	bool irq_enabled();
	bool in_reset();
	bool drive_motor_on(unsigned char index);
	int drive_selected();
	void reset_cmd();
	void reset_res();
	void reset_io();
	uint8_t make_ST3(unsigned int drive,unsigned int head);
	uint8_t fdc_data_read();
	void fdc_data_write(uint8_t b);
	void prepare_res_phase(uint8_t len);
	void on_dor_change(unsigned char b);
	void set_dor(unsigned char b);
	void set_data_rate(uint8_t rate);
	void invalid_command_code();
	bool command_supported(uint8_t cmd);
	void on_fdc_in_command();
	void on_reset();
	void on_reset_complete();
	void reset_configure();
	uint8_t main_status();
	bool drive_ready_now(unsigned int drive);
	void poll_ready();
	void pc98_update_motors();
	void pc98_control_write(uint8_t val);
	uint8_t pc98_control_read();
	uint8_t digital_input_register();
	uint8_t status_register_a();
	uint8_t status_register_b();
	/* timing */
	double delay(double ms);
	double step_ms();
	double head_load_ms(FloppyMechanics &m,double t);
	/* seek/recalibrate */
	void start_seek(unsigned int drive,uint8_t target,bool recalibrate);
	void seek_step(unsigned int drive);
	/* read/write/verify/read id/format */
	FloppyDevice *exec_device();
	imageDisk *exec_image();
	bool exec_media_ok(bool check_cylinder);
	void exec_begin();
	void exec_event();
	void exec_schedule(FDCExecPhase phase,double at);
	void exec_fail_after_index(uint8_t st1,uint8_t st2);
	void exec_finish(uint8_t st0_ic,uint8_t st1,uint8_t st2);
	void exec_track(FloppyTrackMap &M);
	void exec_find_sector(double t);
	void exec_find_from_index(double t);
	void exec_sector_id();
	void exec_data();
	void exec_sector_done();
	void exec_read_id_done();
	void exec_format_sector();
	/* INT 13h emulation interface */
	void int13_sync(unsigned char drive,uint8_t cylinder,uint8_t rate,uint8_t spec1,uint8_t spec2);
	void int13_reset();
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
	dev->set_motor(fdc->drive_motor_on(drv));

	if (IS_PC98_ARCH) fdc->pc98_update_motors();
	fdc->poll_ready();

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

	fdc->poll_ready();
	LOG_MSG("FDC: Primary controller, drive %u unassigned from INT 13h drive %u",drv,drv);
	return true;
}

static void fdc_baseio98_w(Bitu port,Bitu val,Bitu iolen);
static Bitu fdc_baseio98_r(Bitu port,Bitu iolen);
static void fdc_baseio_w(Bitu port,Bitu val,Bitu iolen);
static Bitu fdc_baseio_r(Bitu port,Bitu iolen);
static void fdc_pc98_mode_w(Bitu port,Bitu val,Bitu iolen);
static Bitu fdc_pc98_mode_r(Bitu port,Bitu iolen);
static void fdc_pc98_mode144_w(Bitu port,Bitu val,Bitu iolen);
static Bitu fdc_pc98_mode144_r(Bitu port,Bitu iolen);
static void FDC_SeekEvent(Bitu val/*interface * 4 + drive*/);
static void FDC_ExecEvent(Bitu interface_index);
static void FDC_ResetEvent(Bitu interface_index);
static void FDC_TimerEvent(Bitu interface_index);

void FDC_ClearEvents(Bitu interface_index) {
	PIC_RemoveSpecificEvents(FDC_ExecEvent,interface_index);
	PIC_RemoveSpecificEvents(FDC_ResetEvent,interface_index);
	PIC_RemoveSpecificEvents(FDC_TimerEvent,interface_index);
	for (Bitu d=0;d < 4;d++) PIC_RemoveSpecificEvents(FDC_SeekEvent,interface_index * 4u + d);
}

static void FDC_SeekEvent(Bitu val) {
	const Bitu idx = val >> 2u;
	if (idx >= MAX_FLOPPY_CONTROLLERS || floppycontroller[idx] == NULL) return;
	floppycontroller[idx]->seek_step((unsigned int)(val & 3u));
}

static void FDC_ExecEvent(Bitu interface_index) {
	if (interface_index >= MAX_FLOPPY_CONTROLLERS || floppycontroller[interface_index] == NULL) return;
	floppycontroller[interface_index]->exec_event();
}

static void FDC_ResetEvent(Bitu interface_index) {
	if (interface_index >= MAX_FLOPPY_CONTROLLERS || floppycontroller[interface_index] == NULL) return;
	floppycontroller[interface_index]->on_reset_complete();
}

/* PC-98 VFO timer: interrupts on the FDC's interrupt line 100ms after TTRG, if TMSK is set */
static void FDC_TimerEvent(Bitu interface_index) {
	if (interface_index >= MAX_FLOPPY_CONTROLLERS || floppycontroller[interface_index] == NULL) return;
	FloppyController *fdc = floppycontroller[interface_index];
	if ((fdc->pc98_control & 0x04) && fdc->IRQ >= 0) PIC_ActivateIRQ((unsigned int)fdc->IRQ);
}

FloppyDevice::~FloppyDevice() {
}

FloppyDevice::FloppyDevice(FloppyController *c) {
	controller = c;
	select = false;
	int13_disk = -1;
}

FloppyMechanics &FloppyDevice::mech() {
	return floppy_mech[int13_disk >= 0 ? (int13_disk & 3) : 3];
}

void FloppyDevice::set_select(bool enable) {
	select = enable;
}

void FloppyDevice::set_motor(bool enable) {
	floppy_motor(mech(),enable);
}

bool FloppyDevice::motor() {
	return mech().motor;
}

bool FloppyDevice::track0() {
	return mech().track == 0;
}

void FloppyDevice::motor_step(int dir) {
	FloppyMechanics &m = mech();

	if (dir < 0 && m.track > 0) m.track--;
	else if (dir > 0 && m.track < FLOPPY_MAX_TRACK) m.track++;

	/* a step pulse with a disk in the drive resets the disk change line */
	if (int13_disk >= 0 && int13_disk < MAX_DISK_IMAGES && getImage() != NULL)
		imageDiskChange[int13_disk] = false;
}

/* the disk in the drive, as the controller sees it. The "empty" swap list entry is an image object
 * that stands for a drive with no disk in it, so it is no disk here: the disk change line stays
 * active and a step pulse does not reset it, reads find no address mark, and on the uPD765 the drive
 * is not ready. */
imageDisk *FloppyDevice::getImage() {
	if (int13_disk >= 0) {
		imageDisk *image = GetINT13FloppyDrive((unsigned char)int13_disk);
		if (image != NULL && image->class_id == imageDisk::ID_EMPTY_DRIVE) return NULL;
		return image;
	}

	return NULL;
}

/* ------------------------------------------------------------------------------------------- */

double FloppyController::delay(double ms) {
	return instant_mode ? FDC_INSTANT_DELAY_MS : (ms > FDC_INSTANT_DELAY_MS ? ms : FDC_INSTANT_DELAY_MS);
}

double FloppyController::step_ms() {
	return floppy_srt_ms(spec_srt,data_rate);
}

/* head load time, if the head has unloaded by time t */
double FloppyController::head_load_ms(FloppyMechanics &m,double t) {
	if (t < m.head_unload_at) return 0;
	return floppy_hlt_ms(spec_hlt,data_rate);
}

void FloppyController::on_reset() {
	FDC_ClearEvents(interface_index);
	for (size_t i=0;i < 4;i++) {
		current_cylinder[i] = 0;
		positioning[i] = false;
		seek[i].active = false;
		sense_pending[i] = false;
	}
	reset_sense_count = 0;
	exec_phase = EXEC_NONE;
	data_register_ready = 1;
	data_read_expected = 0;
	non_dma_mode = 0;
	busy_status = 0;
	ST[0] = ST[1] = ST[2] = 0;
	if (!lock) reset_configure();
	reset_io();
	lower_irq();
}

/* the controller comes out of reset. With drive polling enabled, the polling loop finds every
 * drive "changed" from not ready to ready, which causes one interrupt, acknowledged by one
 * SENSE INTERRUPT STATUS per drive (ST0 = C0h-C3h). */
void FloppyController::on_reset_complete() {
	/* the uPD765 has a real READY input: after reset every drive is taken to be not ready,
	 * and the polling finds the ones that are ready, each causing an interrupt */
	if (chip == FDC_CHIP_UPD765) {
		for (unsigned int d=0;d < 4;d++) drive_ready[d] = false;
		poll_ready();
		return;
	}

	if (!cfg_poll) {
		reset_sense_count = 4;
		raise_irq();
	}
}

/* READY from the drive: a disk in it and the motor on, or forced ready (PC-98 FRY). The 82077AA ties READY true. */
bool FloppyController::drive_ready_now(unsigned int drive) {
	FloppyDevice *dev = device[drive&3];

	if (chip != FDC_CHIP_UPD765) return true;
	if (IS_PC98_ARCH && (pc98_control & 0x40)) return true;
	return dev != NULL && dev->getImage() != NULL && dev->motor();
}

/* uPD765 drive polling: a drive that became ready or not ready causes an interrupt,
 * SENSE INTERRUPT STATUS gives ST0 with IC=11 (and NR if it is now not ready) */
void FloppyController::poll_ready() {
	if (chip != FDC_CHIP_UPD765 || in_reset()) return;

	for (unsigned int d=0;d < 4;d++) {
		const bool r = drive_ready_now(d);
		if (r == drive_ready[d]) continue;

		drive_ready[d] = r;
		/* a drive that is seeking reports its own interrupt when done */
		if (seek[d].active) continue;
		sense_st0[d] = (uint8_t)(0xC0 | (r ? 0x00 : 0x08) | d);
		sense_pending[d] = true;
		raise_irq();
	}
}

/* PC-98: the motor is on if MTON is set, or always if motor control is not enabled (EMTON, I/O 0BEh bit 2) */
void FloppyController::pc98_update_motors() {
	const bool on = !(pc98_mode & 0x04) || (pc98_control & 0x08);

	for (unsigned int i=0;i < 4;i++) {
		if (device[i] != NULL) device[i]->set_motor(on);
	}
}

/* I/O 94h write */
void FloppyController::pc98_control_write(uint8_t val) {
	const uint8_t chg = pc98_control ^ val;
	pc98_control = val;

	/* RST: the controller is held in reset while set */
	if (chg & 0x80) {
		if (val & 0x80) {
			LOG_MSG("FDC: Reset\n");
			on_reset();
		}
		else {
			LOG_MSG("FDC: Reset complete\n");
			PIC_RemoveSpecificEvents(FDC_ResetEvent,interface_index);
			PIC_AddEvent(FDC_ResetEvent,delay(0.25),interface_index);
		}
	}

	if (chg & 0x08) LOG(LOG_MISC,LOG_DEBUG)("FDC: PC-98 motor %s",(val & 0x08) ? "on" : "off");

	/* TTRG: the VFO timer interrupts 100ms later, if TMSK lets it */
	if (val & 0x01) {
		PIC_RemoveSpecificEvents(FDC_TimerEvent,interface_index);
		PIC_AddEvent(FDC_TimerEvent,instant_mode ? FDC_INSTANT_DELAY_MS : 100.0,interface_index);
	}

	pc98_update_motors();
	poll_ready();
}

/* I/O 94h read: interface switches. Dual-use FD I/F in 1MB I/F mode: FINT0=1, DMACH=0, built-in drives are #1 and #2 */
uint8_t FloppyController::pc98_control_read() {
	return 0x44;
}

void FloppyController::reset_configure() {
	cfg_eis = false;
	cfg_efifo = true;
	cfg_poll = false;
	cfg_fifothr = 0;
	cfg_pretrk = 0;
	perpendicular = 0;
}

bool FloppyController::in_reset() {
	/* PC-98: RST bit of the control register */
	if (IS_PC98_ARCH) return (pc98_control & 0x80) != 0;
	return !(digital_output_register & 0x04);
}

bool FloppyController::drive_motor_on(unsigned char index) {
	if (index < 4) return !!((digital_output_register >> (4u + index)) & 1);
	return false;
}

int FloppyController::drive_selected() {
	return (digital_output_register & 3);
}

bool FloppyController::dma_enabled() {
	/* PC-98: DMAE bit of the control register */
	if (IS_PC98_ARCH) return (pc98_control & 0x10) != 0;

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
			FDC_ClearEvents(i);
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
	for (size_t i=0;i < 4;i++) floppy_mech[i] = FloppyMechanics();
	FDC_Init(control->GetSection("fdc, primary"),0);
}

void FDC_Primary_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing floppy controller emulation");

	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(FDC_OnReset));
}

uint8_t FloppyController::make_ST3(unsigned int drive,unsigned int head) {
	FloppyDevice *dev = device[drive&3];

	/* The 82077AA ties READY true internally, the uPD765 takes it from the drive */
	uint8_t r =
		(drive_ready_now(drive) ? 0x20/*RDY*/ : 0x00) +
		0x08/*DOUBLE-SIDED SIGNAL*/+
		((head&1) << 2) +
		(drive&3);/*DRIVE SELECT*/

	if (dev != NULL && dev->track0())
		r |= 0x10;/*TRACK 0 signal from device*/
	if (dev != NULL && dev->getImage() != NULL && dev->getImage()->Is_Write_Protected())
		r |= 0x40;/*WRITE PROTECT*/

	return r;
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

	interface_index = index;
	data_register_ready = 1;
	data_read_expected = 0;
	non_dma_mode = 0;
	busy_status = 0;
	for (i=0;i < 4;i++) positioning[i] = false;
	for (i=0;i < 4;i++) ST[i] = 0x00;
	for (i=0;i < 4;i++) {
		seek[i].active = false;
		seek[i].recalibrate = false;
		seek[i].dir = 0;
		seek[i].target = 0;
		seek[i].steps_left = 0;
		sense_pending[i] = false;
		sense_st0[i] = 0;
	}
	irq_pending = false;
	irq_from_result = false;
	reset_sense_count = 0;
	exec_phase = EXEC_NONE;
	exec_drive = exec_hds = 0;
	exec_C = exec_H = exec_R = exec_N = 0;
	exec_count = 0;
	exec_fmt_sc = 0;
	exec_fmt_fill = 0;
	exec_slot_ms = 0;
	exec_pos = exec_size = 0;
	exec_tc = false;
	exec_data_error = false;
	exec_index = 0;
	exec_diag_nd = false;
	exec_cm = false;
	exec_skip = false;
	exec_density = 1.0;
	lock = false;
	last_eot = 18;
	reset_configure();
	reset_io();

	/* state as left by the BIOS POST: out of reset, DMA/IRQ enabled, data rate and
	 * SPECIFY timings from a 1.44MB disk base table */
	digital_output_register = IS_PC98_ARCH ? 0x00 : 0x0C;
	/* PC-98: DMA enabled, motor on, 1MB I/F mode with 1MB access, motor always on (as the BIOS leaves it) */
	pc98_control = 0x18;
	pc98_mode = 0x03;
	pc98_mode144 = 0;
	for (i=0;i < 4;i++) drive_ready[i] = false;
	data_rate = 0;
	spec_srt = 0xA;
	spec_hut = 0xF;
	spec_hlt = 0x01;
	spec_nd = false;

	device[0] = NULL;
	device[1] = NULL;
	device[2] = NULL;
	device[3] = NULL;
	base_io = 0;
	IRQ = -1;
	DMA = -1;

	int13fakev86io = section->Get_bool("int13fakev86io");
	instant_mode = section->Get_bool("instant mode");
	register_pnp = section->Get_bool("pnp");

	{
		std::string s = section->Get_string("mode");
		if (s == "ps2_model30") mode = FDC_MODE_PS2_MODEL30;
		else if (s == "at") mode = FDC_MODE_AT;
		else if (s == "xt") mode = FDC_MODE_XT;
		else mode = FDC_MODE_PS2;

		s = section->Get_string("chip");
		if (s == "82072") chip = FDC_CHIP_82072;
		else if (s == "nec_uPD765" || s == "none") chip = FDC_CHIP_UPD765;
		else chip = FDC_CHIP_82077AA;

		/* PC-98 uses the NEC uPD765 */
		if (IS_PC98_ARCH) chip = FDC_CHIP_UPD765;
		/* the PC/XT controller only does 250Kbps */
		if (mode == FDC_MODE_XT) data_rate = 2;
	}

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
			if (interface_index == 0) {
				WriteHandler[3].Install(0xBE,fdc_pc98_mode_w,IO_MB);
				ReadHandler[3].Install(0xBE,fdc_pc98_mode_r,IO_MB);
				WriteHandler[4].Install(0x4BE,fdc_pc98_mode144_w,IO_MB);
				ReadHandler[4].Install(0x4BE,fdc_pc98_mode144_r,IO_MB);
			}
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
	digital_output_register = b;

	/* !RESET line */
	if (chg & 0x04) {
		if (!(b&0x04)) { /* if bit 2 == 0 s/w is resetting the controller */
			LOG_MSG("FDC: Reset\n");
			on_reset();
		}
		else {
			LOG_MSG("FDC: Reset complete\n");
			/* the drive polling interrupt follows shortly after leaving reset */
			PIC_RemoveSpecificEvents(FDC_ResetEvent,interface_index);
			PIC_AddEvent(FDC_ResetEvent,delay(0.25),interface_index);
		}
	}

	/* drive select */
	if (chg & 0x03) {
		int o,n;

		o = (int)((b ^ chg) & 3);
		n = b & 3;
		LOG_MSG("FDC: Drive select from %c to %c\n",o+'A',n+'A');
		if (device[o] != NULL) device[o]->set_select(false);
		if (device[n] != NULL) device[n]->set_select(true);
	}

	/* DMA/IRQ enable */
	if (chg & 0x08 && IRQ >= 0) {
		if (irq_enabled() && irq_pending) PIC_ActivateIRQ((unsigned int)IRQ);
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
}

/* drive select and motor bits of the DOR changed by the (emulated) BIOS, without the logging */
void FloppyController::set_dor(unsigned char b) {
	b = (unsigned char)((digital_output_register & 0x0C) | (b & 0xF3));
	digital_output_register = b;

	for (unsigned int i=0;i < 4;i++) {
		if (device[i] != NULL) {
			device[i]->set_select((b&3) == i);
			device[i]->set_motor((b&(0x10<<i))?true:false);
		}
	}
}

void FloppyController::set_data_rate(uint8_t rate) {
	/* the PC/XT controller has no data rate select */
	if (mode == FDC_MODE_XT) return;
	data_rate = rate & 3;
}

/* IDE code needs to know if port 3F7 will be taken by FDC emulation */
bool fdc_takes_port_3F7() {
	return (match_fdc_controller(0x3F7) != NULL);
}

void FloppyController::prepare_res_phase(uint8_t len) {
	data_register_ready = 1;
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
			/* reading the result of a read/write/etc. clears the interrupt it raised */
			if (irq_from_result) {
				irq_from_result = false;
				lower_irq();
			}

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

/* ------------------------------------------------------------------------------------------- */
/* SEEK and RECALIBRATE. During the execution phase the controller is not busy, it only sets the
 * drive's busy (positioning) bit in the MSR and can accept other commands. */

void FloppyController::start_seek(unsigned int drive,uint8_t target,bool recalibrate) {
	drive &= 3;
	seek[drive].active = true;
	seek[drive].recalibrate = recalibrate;
	seek[drive].target = recalibrate ? 0 : target;
	sense_pending[drive] = false;
	positioning[drive] = true;

	if (recalibrate) {
		/* step out until TRK0, at most 79 step pulses */
		seek[drive].dir = -1;
		seek[drive].steps_left = 79;
	}
	else {
		seek[drive].dir = target > current_cylinder[drive] ? 1 : -1;
		seek[drive].steps_left = (unsigned int)abs((int)target - (int)current_cylinder[drive]);
	}

	PIC_RemoveSpecificEvents(FDC_SeekEvent,interface_index * 4u + drive);
	PIC_AddEvent(FDC_SeekEvent,delay(seek[drive].steps_left > 0 ? step_ms() : 0.05),interface_index * 4u + drive);
}

void FloppyController::seek_step(unsigned int drive) {
	FloppyDevice *dev = device[drive&3];
	bool done = false,fail = false;

	drive &= 3;
	if (!seek[drive].active) return;

	if (seek[drive].recalibrate) {
		if (dev != NULL && dev->track0()) {
			done = true;
		}
		else if (seek[drive].steps_left == 0) {
			/* TRK0 never came: no drive, or the head was beyond track 79 */
			done = fail = true;
		}
		else {
			seek[drive].steps_left--;
			if (dev != NULL) dev->motor_step(-1);
		}
	}
	else {
		if (seek[drive].steps_left == 0) {
			done = true;
		}
		else {
			seek[drive].steps_left--;
			current_cylinder[drive] = (uint8_t)(current_cylinder[drive] + seek[drive].dir);
			if (dev != NULL) dev->motor_step(seek[drive].dir);
		}
	}

	if (!done) {
		PIC_AddEvent(FDC_SeekEvent,delay(step_ms()),interface_index * 4u + drive);
		return;
	}

	seek[drive].active = false;
	positioning[drive] = false;
	if (seek[drive].recalibrate) current_cylinder[drive] = 0;

	/* SE, plus EC and abnormal termination if TRK0 was not found. Head address is always 0. */
	sense_st0[drive] = (uint8_t)(0x20 | drive | (fail ? 0x50 : 0x00));
	sense_pending[drive] = true;
	raise_irq();
}

/* ------------------------------------------------------------------------------------------- */
/* READ DATA, WRITE DATA, VERIFY, READ ID, FORMAT TRACK: timed execution phase */

FloppyDevice *FloppyController::exec_device() {
	return device[exec_drive&3];
}

imageDisk *FloppyController::exec_image() {
	FloppyDevice *dev = exec_device();
	return dev != NULL ? dev->getImage() : NULL;
}

/* is there a disk the controller can read at all? What is on the track is up to the sector search */
bool FloppyController::exec_media_ok(bool check_cylinder) {
	FloppyDevice *dev = exec_device();
	imageDisk *image = exec_image();
	(void)check_cylinder;

	if (dev == NULL || image == NULL) return false;
	/* NTS: On real hardware a data rate that does not match the media means the data separator never
	 *      locks on and no ID is ever found. That is not enforced, because OS/2 1.x accesses the 1.44MB
	 *      images it is commonly distributed on at 250Kbps as if they were 720K disks. */
	if (!IS_PC98_ARCH && !floppy_rate_matches(image,data_rate))
		LOG(LOG_MISC,LOG_DEBUG)("FDC: Data rate %u does not match the media (%u), accessing it anyway",data_rate,floppy_media_rate(image));
	return true;
}

void FloppyController::exec_schedule(FDCExecPhase phase,double at) {
	exec_phase = phase;
	PIC_RemoveSpecificEvents(FDC_ExecEvent,interface_index);

	double d = at - PIC_FullIndex();
	/* waiting for the motor is polled at the same rate in instant mode */
	PIC_AddEvent(FDC_ExecEvent,phase == EXEC_WAIT_MOTOR ? (d > 0 ? d : 1.0) : delay(d),interface_index);
}

/* the sector could not be found: the controller gives up after the second index pulse */
void FloppyController::exec_fail_after_index(uint8_t st1,uint8_t st2) {
	FloppyDevice *dev = exec_device();
	imageDisk *image = exec_image();
	const double now = PIC_FullIndex();
	const double rev = floppy_rev_ms(image);
	double at = now + 2.0 * rev;

	if (dev != NULL && dev->motor())
		at = now + floppy_time_to_angle(dev->mech(),now,0,rev) + rev;

	ST[1] = st1;
	ST[2] = st2;
	exec_schedule(EXEC_FAIL,at);
}

void FloppyController::exec_finish(uint8_t st0_ic,uint8_t st1,uint8_t st2) {
	FloppyDevice *dev = exec_device();

	exec_phase = EXEC_NONE;
	ST[0] = (uint8_t)(st0_ic | (exec_hds << 2) | exec_drive);
	ST[1] = st1;
	ST[2] = st2;

	// FIXME: Real IBM PC hardware (at least Pentium motherboard) behavior says that the FDC is too lazy
	//        to clear/reset ST1/ST2 unless another error occurs.
	//
	//        PC-98 booter game Star Cruiser seems to expect these to zero out if no error.
	//
	//        Fix this compromise later when real behavior is determined.

	reset_res();
	prepare_res_phase(7);
	out_res[0] = ST[0];
	out_res[1] = ST[1];
	out_res[2] = ST[2];
	out_res[3] = exec_C;
	out_res[4] = exec_H;
	out_res[5] = exec_R;
	out_res[6] = exec_N;

	/* the head unloads the head unload time after the end of the command */
	if (dev != NULL) dev->mech().head_unload_at = PIC_FullIndex() + floppy_hut_ms(spec_hut,data_rate);

	// NTS: The busy bit (CB) stays set through the result phase until the last result byte is
	//      read, as the uPD765A datasheet says. PC-98 code relies on it: Dragon Buster's INT42
	//      handler reads the result if CB is set and otherwise issues SENSE INTERRUPT STATUS.
	//      (This used to be cleared at the end of the execution phase on PC-98 for Arsys Star
	//      Cruiser, which was not retested when this was changed.)

	LOG(LOG_MISC,LOG_DEBUG)("FDC: Result %02x %02x %02x %02x %02x %02x %02x",
		out_res[0],out_res[1],out_res[2],out_res[3],out_res[4],out_res[5],out_res[6]);

	irq_from_result = true;
	raise_irq();
}

/* command phase of READ/WRITE/VERIFY/READ ID/FORMAT is complete: start the execution phase */
void FloppyController::exec_begin() {
	const uint8_t cmd = in_cmd[0]&0x1F;
	FloppyDevice *dev;
	imageDisk *image;
	double t = PIC_FullIndex();

	exec_drive = in_cmd[1]&3;
	exec_hds = (in_cmd[1]>>2)&1;
	exec_count = 0;
	exec_tc = false;
	dev = exec_device();
	image = exec_image();

	data_register_ready = 0;
	data_read_expected = 0;
	busy_status = 1;

	if (cmd == 0x0A/*READ ID*/ || cmd == 0x0D/*FORMAT*/) {
		exec_C = dev != NULL ? (uint8_t)dev->mech().track : 0;
		exec_H = exec_hds;
		exec_R = 1;
		exec_N = cmd == 0x0D ? in_cmd[2] : 2;
	}
	else {
		exec_C = in_cmd[2];
		exec_H = in_cmd[3];
		exec_R = in_cmd[4];
		exec_N = in_cmd[5];
		last_eot = in_cmd[6];
	}

	/* uPD765: a read/write to a drive that is not ready ends right away with NR */
	if (chip == FDC_CHIP_UPD765 && !drive_ready_now(exec_drive)) {
		exec_finish(0x48/*AT, NR*/,0x00,0x00);
		return;
	}

	if (dev == NULL) {
		/* no drive: no index pulses, no ID marks */
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	FloppyMechanics &m = dev->mech();

	/* implied seek (CONFIGURE EIS): seek to the cylinder in the command first */
	if (cfg_eis && cmd != 0x0A && cmd != 0x0D && in_cmd[2] != current_cylinder[exec_drive]) {
		const unsigned int steps = (unsigned int)abs((int)in_cmd[2] - (int)current_cylinder[exec_drive]);
		const int dir = in_cmd[2] > current_cylinder[exec_drive] ? 1 : -1;

		for (unsigned int s=0;s < steps;s++) dev->motor_step(dir);
		current_cylinder[exec_drive] = in_cmd[2];
		t += steps * step_ms();
	}

	/* the disk has to turn. If the motor is off, nothing happens until it is turned on. */
	if (!m.motor) {
		exec_schedule(EXEC_WAIT_MOTOR,t + 100.0);
		return;
	}
	const double t_cmd = PIC_FullIndex();
	if (t < floppy_ready_at(m) && !instant_mode) t = floppy_ready_at(m);
	const double t_ready = t;

	t += head_load_ms(m,t);
	LOG(LOG_MISC,LOG_DEBUG)("FDC: Execution timing: motor on %.1fms ago, spin-up wait %.1fms, head load %.1fms",
		t_cmd - m.motor_on_at,t_ready - t_cmd,t - t_ready);

	if (!exec_media_ok(true)) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	if (cmd == 0x0D/*FORMAT*/) {
		const FloppyTrackLayout L = floppy_layout(image);
		exec_fmt_sc = in_cmd[3] != 0 ? in_cmd[3] : L.spt;
		exec_fmt_fill = in_cmd[5];
		exec_slot_ms = L.rev_ms / exec_fmt_sc;
		/* starts at the index hole */
		t += L.time_to(m,t,0) + exec_slot_ms;
		exec_schedule(EXEC_FORMAT,t);
		return;
	}

	if (cmd == 0x02/*READ DIAGNOSTIC*/) {
		/* reads the data fields one after another from the index hole, whatever their IDs */
		const FloppyTrackLayout L = floppy_layout(image);
		exec_diag_nd = false;
		exec_index = 0;
		exec_find_from_index(t + L.time_to(m,t,0));
		return;
	}

	if (cmd != 0x0A/*READ ID*/ && exec_N > FLOPPY_MAX_SECTOR_SIZE_SZCODE) {
		exec_fail_after_index(0x04/*ND*/,0x00);
		return;
	}

	exec_find_sector(t);
}

/* the sectors of the track under the head, and where their ID fields are */
void FloppyController::exec_track(FloppyTrackMap &M) {
	FloppyDevice *dev = exec_device();
	floppy_track_map(exec_image(),dev != NULL ? dev->mech().track : 0,exec_hds,M);
}

/* Look for the ID field that matches the command (or for READ ID, any ID field), starting at time t.
 * The controller compares each ID field of the right density that passes under the head, from its
 * beginning, and gives up at the second index pulse. */
void FloppyController::exec_find_sector(double t) {
	const uint8_t cmd = in_cmd[0]&0x1F;
	const bool fm = !(in_cmd[0]&0x40);
	FloppyDevice *dev = exec_device();
	FloppyTrackMap M;

	exec_track(M);

	FloppyMechanics &m = dev->mech();
	const double rev = M.L.rev_ms;
	const double first_index = t + M.L.time_to(m,t,0);
	const double give_up = first_index + rev;
	bool any_id = false,wrong_cyl = false,bad_cyl = false;
	double best = 1e30;
	int found = -1;

	for (size_t k=0;k < M.secs.size();k++) {
		const imageDiskTrackSector &ts = M.secs[k];
		if (ts.fm != fm || ts.id_crc_error) continue;
		any_id = true;

		/* this ID passes under the head at w, and again every revolution after */
		double w = t + M.L.time_to(m,t,M.id_pos[k]);
		if (w > give_up) continue;

		if (cmd != 0x0A/*READ ID*/) {
			if (ts.c != exec_C) {
				if (ts.c == 0xFF) bad_cyl = true;
				else wrong_cyl = true;
				continue;
			}
			if (ts.h != exec_H || ts.r != exec_R || ts.n != exec_N) continue;
		}

		if (w < best) {
			best = w;
			found = (int)k;
		}
	}

	if (found < 0) {
		/* nothing with the right density at all: missing address mark, otherwise no data */
		if (!any_id) exec_fail_after_index(0x01/*MA*/,0x00);
		else exec_fail_after_index(0x04/*ND*/,(uint8_t)((wrong_cyl ? 0x10/*WC*/ : 0x00) | (bad_cyl ? 0x02/*BC*/ : 0x00)));
		return;
	}

	exec_index = (unsigned int)found;
	const double id_bytes = 22.0 * (fm ? 2.0 : 1.0);
	exec_schedule(cmd == 0x0A ? EXEC_READID : EXEC_ID,best + id_bytes * M.L.byte_ms);
}

/* READ DIAGNOSTIC: the next sector in physical order (exec_index), from time t */
void FloppyController::exec_find_from_index(double t) {
	FloppyDevice *dev = exec_device();
	FloppyTrackMap M;

	exec_track(M);
	if (exec_index >= M.secs.size()) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	const double id_bytes = 22.0 * (M.secs[exec_index].fm ? 2.0 : 1.0);
	exec_schedule(EXEC_ID,t + M.L.time_to(dev->mech(),t,M.id_pos[exec_index]) + id_bytes * M.L.byte_ms);
}

/* the ID field of the sector passed under the head: the data field follows after gap 2 */
void FloppyController::exec_sector_id() {
	const uint8_t cmd = in_cmd[0]&0x1F;
	imageDisk *image = exec_image();
	FloppyDevice *dev = exec_device();
	FloppyTrackMap M;

	if (!exec_media_ok(true) || !dev->motor()) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	exec_track(M);
	if (exec_index >= M.secs.size()) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	const imageDiskTrackSector &ts = M.secs[exec_index];
	const double density = ts.fm ? 2.0 : 1.0;

	if (cmd == 0x02/*READ DIAGNOSTIC*/) {
		/* the data is read whether or not the ID matches, a mismatch is reported at the end */
		if (ts.c != exec_C || ts.h != exec_H || ts.r != exec_R || ts.n != exec_N) exec_diag_nd = true;
	}

	if (ts.no_data) {
		/* an ID with no data address mark after it */
		exec_finish(0x40,0x01/*MA*/,0x01/*MD*/);
		return;
	}

	/* a data field with the other kind of address mark (deleted/normal) than the command reads:
	 * with SK set it is skipped, otherwise it is read and the command ends after it */
	exec_cm = false;
	if (cmd == 0x06 || cmd == 0x0C || cmd == 0x16) {
		const bool want_deleted = (cmd == 0x0C);
		if (ts.deleted != want_deleted) {
			if (in_cmd[0]&0x20/*SK*/) {
				exec_skip = true;
				exec_size = 0;
				exec_pos = 0;
				exec_schedule(EXEC_DATA,PIC_FullIndex() + (22.0 + 16.0 + ts.data_size + 2.0) * density * M.L.byte_ms);
				return;
			}
			exec_cm = true;
		}
	}
	exec_skip = false;

	/* N=0: the data length is DTL */
	exec_size = exec_N != 0 ? (1u << (exec_N + 7u)) : (in_cmd[8] != 0 && in_cmd[8] < 128 ? in_cmd[8] : 128u);
	if (exec_size > FLOPPY_MAX_SECTOR_SIZE) exec_size = FLOPPY_MAX_SECTOR_SIZE;
	exec_pos = 0;
	exec_data_error = ts.data_crc_error;
	exec_density = density;

	if (cmd == 0x05/*WRITE*/ || cmd == 0x09/*WRITE DELETED*/) {
		if (image->Is_Write_Protected()) {
			exec_finish(0x40,0x02/*NW*/,0x00);
			return;
		}
		memset(exec_buf,0,sizeof(exec_buf));
	}
	else {
		Int13Status err = image->Read_Track_Sector(dev->mech().track,exec_hds,exec_index,exec_buf,exec_size);
		if (err == Int13Status::DataError) exec_data_error = true;
		else if (err != Int13Status::NoError) {
			exec_fail_after_index(0x04/*ND*/,0x00);
			return;
		}
	}

	/* gap 2, the data address mark, then the first bytes of data */
	const unsigned int first = MIN(FDC_DMA_CHUNK,exec_size);
	exec_schedule(EXEC_DATA,PIC_FullIndex() + (22.0 + 16.0 + first) * density * M.L.byte_ms);
}

/* the next bytes of the data field passed under the head: move them through DMA */
void FloppyController::exec_data() {
	const uint8_t cmd = in_cmd[0]&0x1F;
	const FloppyTrackLayout L = floppy_layout(exec_image());
	const bool writing = (cmd == 0x05/*WRITE*/ || cmd == 0x09/*WRITE DELETED*/);

	/* a skipped sector: nothing transferred, on to the next one */
	if (exec_skip) {
		exec_sector_done();
		return;
	}

	const unsigned int n = MIN(instant_mode ? exec_size : FDC_DMA_CHUNK,exec_size - exec_pos);

	if (cmd != 0x16/*VERIFY, no data transfer*/ && !exec_tc) {
		/* DMA must take or give the bytes as they pass, the FIFO is too small to wait.
		 * If the DMA channel is not ready, the result is an overrun (underrun for writes). */
		dma = GetDMAChannel(DMA);
		if (dma == NULL || dma->masked || !dma_enabled()) {
			LOG(LOG_MISC,LOG_DEBUG)("FDC: DMA not ready during sector transfer, overrun");
			exec_finish(0x40,0x10/*OR*/,0x00);
			return;
		}

		if (exec_pos == 0 && exec_count == 0 && dma->tcount) {
			LOG(LOG_MISC,LOG_DEBUG)("FDC: DMA terminal count at transfer start. Resetting terminal count. Hope a new count was written!");
			dma->tcount = false;
		}

		dma->Register_Callback(nullptr);
		const Bitu got = writing ? dma->Read(n,exec_buf + exec_pos) : dma->Write(n,exec_buf + exec_pos);
		if (got == 0 && exec_pos == 0 && exec_count == 0) {
			LOG(LOG_MISC,LOG_DEBUG)("FDC: DMA transfer failed: channel mode %u (want %u) count %u masked %u tc %u",
				(unsigned int)dma->transfer_mode,(unsigned int)(writing ? DMAT_READ : DMAT_WRITE),
				(unsigned int)dma->currcnt,dma->masked ? 1u : 0u,dma->tcount ? 1u : 0u);
			exec_finish(0x40,0x10/*OR*/,0x00);
			return;
		}

		/* terminal count: the transfer ends, the controller finishes the sector (a write fills the rest with zeros) */
		if (got < n || dma->tcount) exec_tc = true;
	}

	exec_pos += n;
	if (exec_pos < exec_size) {
		const unsigned int next = MIN(instant_mode ? exec_size : FDC_DMA_CHUNK,exec_size - exec_pos);
		exec_schedule(EXEC_DATA,PIC_FullIndex() + next * exec_density * L.byte_ms);
		return;
	}

	exec_sector_done();
}

/* the whole data field passed under the head: go on to the next sector or finish */
void FloppyController::exec_sector_done() {
	const uint8_t cmd = in_cmd[0]&0x1F;
	imageDisk *image = exec_image();
	FloppyDevice *dev = exec_device();

	if (!exec_skip) {
		if (cmd == 0x05/*WRITE*/ || cmd == 0x09/*WRITE DELETED*/) {
			Int13Status err = image->Write_Track_Sector(dev->mech().track,exec_hds,exec_index,exec_buf,exec_size);
			if (err == Int13Status::WriteProtected) { exec_finish(0x40,0x02/*NW*/,0x00); return; }
			if (err != Int13Status::NoError) { exec_fail_after_index(0x04/*ND*/,0x00); return; }
		}
		else if (exec_data_error) {
			/* real controllers transfer the data of a sector with a CRC error, then stop */
			exec_finish(0x40,0x20/*DE*/,0x20/*DD*/);
			return;
		}

		/* the other kind of data address mark without SK: the command ends after this sector,
		 * the sector address is not advanced */
		if (exec_cm) {
			exec_finish(0x40,0x00,0x40/*CM*/);
			return;
		}

		exec_count++;
	}

	FloppyTrackMap M;
	exec_track(M);

	const bool mt = (in_cmd[0]&0x80) != 0 && cmd != 0x02;
	const unsigned int spt = (unsigned int)M.secs.size();
	/* NTS: An end of track below the number of sectors on the track is apparently ignored. If it were not,
	 *      OS/2 1.x would have failed to read things properly on real hardware, because reading/writing a
	 *      1.44MB floppy (18 sectors/track) with an end track number like you're a 720K floppy (end of
	 *      track == 9) is something IBM would do. */
	const unsigned int eot = cmd == 0x02 ? in_cmd[6] : (in_cmd[6] > spt ? in_cmd[6] : spt);
	const bool at_eot = (cmd == 0x02) ? (exec_index + 1u >= eot || exec_index + 1u >= spt) : (exec_R >= eot);
	bool tc;

	if (cmd == 0x16/*VERIFY*/) {
		/* EC=1: SC sectors (in place of DTL), otherwise up to the end of the track */
		if (in_cmd[1]&0x80) tc = exec_count >= (in_cmd[8] != 0 ? in_cmd[8] : 256u);
		else tc = at_eot && !(mt && exec_H == 0 && image->heads > 1);
	}
	else {
		tc = exec_tc;
	}

	/* ID for the next sector, also the result phase ID (82077AA datasheet table 5-6) */
	if (at_eot) {
		exec_R = 1;
		if (mt) {
			if (exec_H & 1) exec_C++;
			exec_H ^= 1;
			exec_hds ^= 1;
		}
		else {
			exec_C++;
		}
	}
	else {
		exec_R++;
	}

	if (tc) {
		exec_finish(0x00,cmd == 0x02 && exec_diag_nd ? 0x04/*ND*/ : 0x00,0x00);
		return;
	}

	/* the end of the track (both sides for multi-track) without terminal count: end of cylinder */
	if (at_eot && (!mt || exec_hds == 0/*came back around from side 1*/)) {
		if (mt) exec_hds ^= 1; /* report the side it ended on */
		exec_finish(0x40,(uint8_t)(0x80/*EN*/ | (cmd == 0x02 && exec_diag_nd ? 0x04/*ND*/ : 0x00)),0x00);
		return;
	}

	if (exec_hds >= image->heads) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	if (cmd == 0x02) {
		/* READ DIAGNOSTIC goes on with the next sector in physical order */
		exec_index++;
		exec_find_from_index(PIC_FullIndex());
		return;
	}

	/* on to the next sector, its ID field comes along after gap 3 (side 1 sector 1 follows the index hole) */
	exec_find_sector(PIC_FullIndex());
}

void FloppyController::exec_read_id_done() {
	FloppyTrackMap M;

	if (!exec_media_ok(false) || !exec_device()->motor()) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	exec_track(M);
	if (exec_index >= M.secs.size()) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	/* the ID field as it is on the disk, whatever the physical cylinder and head */
	const imageDiskTrackSector &ts = M.secs[exec_index];
	exec_C = ts.c;
	exec_H = ts.h;
	exec_R = ts.r;
	exec_N = ts.n;

	exec_finish(0x00,0x00,0x00);
}

/* FORMAT TRACK: the ID (C,H,R,N) of each sector comes from DMA, the data field is filled with D */
void FloppyController::exec_format_sector() {
	imageDisk *image = exec_image();
	FloppyDevice *dev = exec_device();
	uint8_t id[4];

	if (!exec_media_ok(false) || !dev->motor()) {
		exec_fail_after_index(0x01/*MA*/,0x00);
		return;
	}

	if (image->Is_Write_Protected()) {
		exec_finish(0x40,0x02/*NW*/,0x00);
		return;
	}

	dma = GetDMAChannel(DMA);
	if (dma == NULL || dma->masked || !dma_enabled()) {
		exec_finish(0x40,0x10/*OR*/,0x00);
		return;
	}

	dma->Register_Callback(nullptr);
	if (dma->Read(4,id) != 4) {
		exec_finish(0x40,0x10/*OR*/,0x00);
		return;
	}

	exec_C = id[0];
	exec_H = id[1];
	exec_R = id[2];
	exec_N = id[3];

	/* An image can only take the data of sectors that are already on the track with the same ID
	 * and size, a format that changes the layout of the track is not possible. */
	{
		FloppyTrackMap M;
		int found = -1;

		exec_track(M);
		for (size_t k=0;k < M.secs.size();k++) {
			const imageDiskTrackSector &ts = M.secs[k];
			if (ts.c == exec_C && ts.h == exec_H && ts.r == exec_R && ts.n == exec_N) {
				found = (int)k;
				break;
			}
		}

		if (found >= 0 && exec_N <= FLOPPY_MAX_SECTOR_SIZE_SZCODE) {
			unsigned char sector[FLOPPY_MAX_SECTOR_SIZE];
			memset(sector,exec_fmt_fill,sizeof(sector));

			Int13Status err = image->Write_Track_Sector(dev->mech().track,exec_hds,(unsigned int)found,sector,128u << exec_N);
			if (err == Int13Status::WriteProtected) {
				exec_finish(0x40,0x02/*NW*/,0x00);
				return;
			}
		}
		else {
			LOG(LOG_MISC,LOG_DEBUG)("FDC: Format sector ID C=%u H=%u R=%u N=%u does not fit the image, not written",exec_C,exec_H,exec_R,exec_N);
		}
	}

	if (++exec_count >= exec_fmt_sc) {
		/* the command ends at the next index pulse */
		exec_finish(0x00,0x00,0x00);
		return;
	}

	exec_schedule(EXEC_FORMAT,PIC_FullIndex() + exec_slot_ms);
}

void FloppyController::exec_event() {
	switch (exec_phase) {
		case EXEC_WAIT_MOTOR:
			exec_phase = EXEC_NONE;
			exec_begin();
			break;
		case EXEC_ID:
			exec_sector_id();
			break;
		case EXEC_DATA:
			exec_data();
			break;
		case EXEC_READID:
			exec_read_id_done();
			break;
		case EXEC_FORMAT:
			exec_format_sector();
			break;
		case EXEC_FAIL:
			exec_finish(0x40,ST[1],ST[2]);
			break;
		default:
			break;
	}
}

/* ------------------------------------------------------------------------------------------- */

/* commands each chip knows */
bool FloppyController::command_supported(uint8_t cmd) {
	switch (cmd&0x1F) {
		case 0x02: /* READ DIAGNOSTIC */
		case 0x03: /* SPECIFY */
		case 0x04: /* SENSE DRIVE STATUS */
		case 0x05: /* WRITE DATA */
		case 0x06: /* READ DATA */
		case 0x07: /* RECALIBRATE */
		case 0x08: /* SENSE INTERRUPT STATUS */
		case 0x09: /* WRITE DELETED DATA */
		case 0x0A: /* READ ID */
		case 0x0C: /* READ DELETED DATA */
		case 0x0D: /* FORMAT TRACK */
		case 0x0F: /* SEEK */
			return true;
		case 0x0E: /* DUMPREG */
		case 0x13: /* CONFIGURE */
			return chip == FDC_CHIP_82077AA || chip == FDC_CHIP_82072;
		case 0x10: /* VERSION */
		case 0x12: /* PERPENDICULAR MODE */
		case 0x14: /* LOCK */
		case 0x16: /* VERIFY */
			return chip == FDC_CHIP_82077AA;
		default:
			break;
	}

	return false;
}

void FloppyController::on_fdc_in_command() {
	int devidx;

	in_cmd_state = false;
	devidx = in_cmd[1]&3;

	LOG(LOG_MISC,LOG_DEBUG)("FDC: Command len=%u %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		in_cmd_len,
		in_cmd[0],in_cmd[1],in_cmd[2],in_cmd[3],in_cmd[4],
		in_cmd[5],in_cmd[6],in_cmd[7],in_cmd[8]);

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
			spec_srt = in_cmd[1] >> 4;
			spec_hut = in_cmd[1] & 15;
			spec_hlt = in_cmd[2] >> 1;
			spec_nd = (in_cmd[2] & 1) != 0;
			if (spec_nd) LOG(LOG_MISC,LOG_WARN)("FDC: Non-DMA mode is not supported, DMA will be used");
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
			out_res[0] = ST[3] = make_ST3((unsigned int)devidx,(in_cmd[1]>>2)&1);
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
		case 0x06: /* Read data (0110) */
			/* same result as Write data */
		case 0x16: /* Verify */
			/* same result as Read data, no data transfer */
			exec_begin();
			return;
		case 0x07: /* Calibrate drive */
			/* no result phase. The controller is free for other commands while the head moves,
			 * the end of the command is signalled by an interrupt (SENSE INTERRUPT STATUS). */
			start_seek((unsigned int)devidx,0,true);
			reset_io();
			return;
		case 0x08: /* Check Interrupt Status */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0 |               Register ST0
			 *   1 |              Current Cylinder
			 * -----------------------------------------------
			 *   2     total
			 */
			reset_res();
			if (reset_sense_count > 0) {
				/* drive polling interrupt after reset: one status per drive, ST0 = C0h-C3h */
				const unsigned int drive = 4u - reset_sense_count;
				reset_sense_count--;
				prepare_res_phase(2);
				out_res[0] = ST[0] = (uint8_t)(0xC0 | drive);
				out_res[1] = current_cylinder[drive];
			}
			else {
				int found = -1;
				for (int d=0;d < 4;d++) {
					if (sense_pending[d]) {
						found = d;
						break;
					}
				}

				if (found >= 0) {
					sense_pending[found] = false;
					prepare_res_phase(2);
					out_res[0] = ST[0] = sense_st0[found];
					out_res[1] = current_cylinder[found];
				}
				else {
					/* if no pending interrupt, the result is a single byte 80h (invalid command).
					 * this is considered standard floppy controller behavior.
					 * this also fixes an issue where Windows 3.1 QIC tape backup software like Norton Backup (Norton Desktop 3.0)
					 * will "hang" Windows 3.1 polling the FDC in an endless loop if we don't return this error to say that no
					 * more IRQs are pending. */
					invalid_command_code();
				}
			}

			/* the interrupt goes away when there is nothing more to sense */
			{
				bool more = reset_sense_count > 0;
				for (int d=0;d < 4;d++) more = more || sense_pending[d];
				if (!more) lower_irq();
			}
			irq_from_result = false;
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
			exec_begin();
			return;
		case 0x0C: /* Read deleted data (1100) */
			/* same as Read data, for data fields with a deleted data address mark */
		case 0x02: /* Read diagnostic (read track) */
			/* same result as Read data */
			exec_begin();
			return;
		case 0x0D: /* Format track */
			/*     |   7    6    5    4    3    2    1    0
			 * ----+------------------------------------------
			 *   0..6 ST0, ST1, ST2, then C, H, R, N (undefined)
			 */
			exec_begin();
			return;
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
			out_res[4] = (uint8_t)((spec_srt << 4) | spec_hut);
			out_res[5] = (uint8_t)((spec_hlt << 1) | (spec_nd ? 1 : 0));
			out_res[6] = last_eot;
			out_res[7] = (uint8_t)((lock ? 0x80 : 0) | (perpendicular & 0x3F));
			out_res[8] = (uint8_t)((cfg_eis ? 0x40 : 0) | (cfg_efifo ? 0x20 : 0) | (cfg_poll ? 0x10 : 0) | (cfg_fifothr & 15));
			out_res[9] = cfg_pretrk;
			break;
		case 0x0F: /* Seek Head */
			/* no result phase, see Calibrate drive */
			start_seek((unsigned int)devidx,in_cmd[2],false);
			reset_io();
			return;
		case 0x10: /* Version */
			reset_res();
			prepare_res_phase(1);
			out_res[0] = 0x90; /* enhanced controller (82077AA) */
			break;
		case 0x12: /* Perpendicular mode */
			if (in_cmd[1] & 0x80) perpendicular = (uint8_t)((perpendicular & 0x03) | ((in_cmd[1] & 0x3C)));
			perpendicular = (uint8_t)((perpendicular & 0x3C) | (in_cmd[1] & 0x03));
			reset_io();
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
			cfg_eis = (in_cmd[2] & 0x40) != 0;
			cfg_efifo = (in_cmd[2] & 0x20) != 0;
			cfg_poll = (in_cmd[2] & 0x10) != 0;
			cfg_fifothr = in_cmd[2] & 15;
			cfg_pretrk = in_cmd[3];
			reset_io();
			break;
		case 0x14: /* Lock */
			lock = (in_cmd[0] & 0x80) != 0;
			reset_res();
			prepare_res_phase(1);
			out_res[0] = lock ? 0x10 : 0x00;
			break;
		default:
			LOG_MSG("FDC: Unknown command %02xh (somehow passed first check)\n",in_cmd[0]);
			reset_io();
			break;
	}

	LOG(LOG_MISC,LOG_DEBUG)("FDC: Response len=%u %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		out_res_len,
		out_res[0],out_res[1],out_res[2],out_res[3],out_res[4],
		out_res[5],out_res[6],out_res[7],out_res[8],out_res[9]);
}

void FloppyController::fdc_data_write(uint8_t b) {
	if (in_reset()) {
		LOG(LOG_MISC,LOG_DEBUG)("FDC: Data write while controller is held in reset, ignored");
		return;
	}

	if (!busy_status) {
		/* we're starting another command */
		reset_io();
		in_cmd[0] = b;
		in_cmd_len = 1;
		in_cmd_pos = 1;
		busy_status = true;
		in_cmd_state = true;

		/* writing a command clears the interrupt of the last read/write result */
		if (irq_from_result) {
			irq_from_result = false;
			lower_irq();
		}

		if (!command_supported(in_cmd[0])) {
			LOG_MSG("FDC: Unknown command (first byte %02xh)\n",in_cmd[0]);
			/* give Invalid Command Code 0x80 as response */
			invalid_command_code();
			reset_cmd();
			return;
		}

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
			case 0x16: /* Verify (same as Read data, byte 1 bit 7 = EC, byte 8 = SC if EC) */
			case 0x02: /* Read diagnostic (same as Read data) */
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
			case 0x0D: /* Format track */
				/*     |   7    6    5    4    3    2    1    0
				 * ----+------------------------------------------
				 *   0 |   0  MFM    0    0    1    1    0    1
				 *   1 |   0    0    0    0    0   HD  DR1  DR0
				 *   2 |               Sector size (N)
				 *   3 |             Sectors per track (SC)
				 *   4 |               Gap 3 length
				 *   5 |                Filler byte
				 * -----------------------------------------------
				 *   6     total
				 */
				in_cmd_len = 6;
				break;
			case 0x0E: /* Dump registers */
			case 0x10: /* Version */
			case 0x14: /* Lock */
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
			case 0x12: /* Perpendicular mode */
				in_cmd_len = 2;
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

/* ------------------------------------------------------------------------------------------- */

uint8_t FloppyController::main_status() {
	/* nothing is ready while the controller is held in reset */
	if (in_reset()) return 0x00;

	return	(data_register_ready ? 0x80 : 0x00) +
		(data_read_expected ? 0x40 : 0x00) +
		(non_dma_mode ? 0x20 : 0x00) +
		(busy_status ? 0x10 : 0x00) +
		(positioning[3] ? 0x08 : 0x00) +
		(positioning[2] ? 0x04 : 0x00) +
		(positioning[1] ? 0x02 : 0x00) +
		(positioning[0] ? 0x01 : 0x00);
}

/* digital input register (3F7h read) */
uint8_t FloppyController::digital_input_register() {
	FloppyDevice *dev = device[drive_selected()&3];
	bool dskchg = false;

	/* the disk change line of the selected drive. It stays active while there is no disk. */
	if (dev != NULL) {
		if (dev->getImage() == NULL) dskchg = true;
		else if (dev->int13_disk >= 0 && dev->int13_disk < MAX_DISK_IMAGES && imageDiskChange[dev->int13_disk]) dskchg = true;
	}

	switch (mode) {
		case FDC_MODE_PS2_MODEL30:
			/* bit 7 = !DSKCHG, bit 3 = DMA gate, bits 1-0 = data rate */
			return (uint8_t)((dskchg ? 0x00 : 0x80) | (dma_enabled() ? 0x08 : 0x00) | (data_rate & 3));
		case FDC_MODE_AT:
			/* bit 7 = DSKCHG, the other bits belong to the hard disk controller */
			return (uint8_t)((dskchg ? 0x80 : 0x00) | 0x7F);
		case FDC_MODE_PS2:
		default:
			/* bit 7 = DSKCHG, bits 6-3 = 1, bits 2-1 = data rate, bit 0 = !high density */
			return (uint8_t)((dskchg ? 0x80 : 0x00) | 0x78 | ((data_rate & 3) << 1) |
				((data_rate == 0 || data_rate == 3) ? 0x00 : 0x01));
	}
}

/* status register A (3F0h read, PS/2 modes) */
uint8_t FloppyController::status_register_a() {
	FloppyDevice *dev = device[drive_selected()&3];
	const bool trk0 = dev != NULL && dev->track0();
	const bool drv2 = device[1] != NULL;

	if (mode == FDC_MODE_PS2_MODEL30) {
		/* INT, DRQ, STEP, TRK0, !HDSEL, INDX, WP, !DIR */
		return (uint8_t)((irq_pending ? 0x80 : 0) | (trk0 ? 0x10 : 0) | 0x08 | 0x01);
	}

	/* INT, !DRV2, STEP, !TRK0, HDSEL, !INDX, !WP, !DIR */
	return (uint8_t)((irq_pending ? 0x80 : 0) | (drv2 ? 0 : 0x40) | (trk0 ? 0 : 0x10) | 0x04 | 0x02 | 0x01);
}

/* status register B (3F1h read, PS/2 modes) */
uint8_t FloppyController::status_register_b() {
	const uint8_t dor = digital_output_register;

	if (mode == FDC_MODE_PS2_MODEL30) {
		/* !DRV2, !DS1, !DS0, WRDATA, RDDATA, WE, !DS3, !DS2 */
		return (uint8_t)((device[1] != NULL ? 0x00 : 0x80) | ((dor&3) == 1 ? 0 : 0x40) | ((dor&3) == 0 ? 0 : 0x20) |
			((dor&3) == 3 ? 0 : 0x02) | ((dor&3) == 2 ? 0 : 0x01));
	}

	/* 1, 1, DS0, WRDATA, RDDATA, WE, MOT EN1, MOT EN0 */
	return (uint8_t)(0xC0 | ((dor&1) ? 0x20 : 0) | ((dor & 0x20) ? 0x02 : 0) | ((dor & 0x10) ? 0x01 : 0));
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
				LOG_MSG("WARNING: FDC data write %02xh when data port ready but expecting I/O read (MSR %02xh)\n",(unsigned int)val,fdc->main_status());
			}
			else {
				fdc->fdc_data_write(val&0xFF);
			}
			break;
		case 4: /* control register */
			fdc->pc98_control_write(val&0xFF);
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
			/* the uPD765 polls the drives while idle, notice a disk going in or out */
			fdc->poll_ready();
			return fdc->main_status();
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
		case 4: /* interface switches */
			return fdc->pc98_control_read();
		default:
			LOG_MSG("DEBUG: FDC read port %03xh len=%u\n",(int)port,(int)iolen);
			break;
	}

	return ~(0UL);
}

/* PC-98 I/O 0BEh: dual-use FD I/F mode */
static void fdc_pc98_mode_w(Bitu port,Bitu val,Bitu iolen) {
	(void)port;(void)iolen;
	FloppyController *fdc = floppycontroller[0];
	if (fdc == NULL) return;

	fdc->pc98_mode = (uint8_t)(val & 0x07);
	LOG(LOG_MISC,LOG_DEBUG)("FDC: PC-98 mode %02x: %s I/F, %s access, motor control %s",(unsigned int)val,
		(val & 1) ? "1MB" : "640KB",(val & 2) ? "1MB" : "640KB",(val & 4) ? "enabled" : "disabled (always on)");
	fdc->pc98_update_motors();
	fdc->poll_ready();
}

static Bitu fdc_pc98_mode_r(Bitu port,Bitu iolen) {
	(void)port;(void)iolen;
	FloppyController *fdc = floppycontroller[0];
	if (fdc == NULL) return ~0ul;

	/* DIP SW 3-2 off (1MB), 3-1 off (automatic), access mode, I/F mode */
	return 0x08u | (fdc->pc98_mode & 0x03u);
}

/* PC-98 I/O 4BEh: 3-mode FD I/F (1.44MB). The drives emulated here do not do 1.44MB. */
static void fdc_pc98_mode144_w(Bitu port,Bitu val,Bitu iolen) {
	(void)port;(void)iolen;
	FloppyController *fdc = floppycontroller[0];
	if (fdc == NULL) return;

	if (val & 0x10) fdc->pc98_mode144 = (uint8_t)(val & 1);
}

static Bitu fdc_pc98_mode144_r(Bitu port,Bitu iolen) {
	(void)port;(void)iolen;
	FloppyController *fdc = floppycontroller[0];
	if (fdc == NULL) return ~0ul;

	return 0xEEu | (fdc->pc98_mode144 & 1u);
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
		case 4: /* data rate select (DSR) */
			if (fdc->mode == FDC_MODE_XT) break;
			fdc->set_data_rate(val&3);
			/* software reset, which clears itself */
			if (val & 0x80) {
				LOG_MSG("FDC: Reset (DSR)\n");
				fdc->on_reset();
				PIC_AddEvent(FDC_ResetEvent,fdc->delay(0.25),fdc->interface_index);
			}
			break;
		case 5: /* data */
			if (!fdc->data_register_ready) {
				LOG_MSG("WARNING: FDC data write when data port not ready\n");
			}
			else if (fdc->data_read_expected) {
				LOG_MSG("WARNING: FDC data write %02xh when data port ready but expecting I/O read (MSR %02xh)\n",(unsigned int)val,fdc->main_status());
			}
			else {
				fdc->fdc_data_write(val&0xFF);
			}
			break;
		case 7: /* configuration control register (CCR) */
			fdc->set_data_rate(val&3);
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
		case 0: /* status register A */
			if (fdc->mode == FDC_MODE_PS2 || fdc->mode == FDC_MODE_PS2_MODEL30) return fdc->status_register_a();
			break;
		case 1: /* status register B */
			if (fdc->mode == FDC_MODE_PS2 || fdc->mode == FDC_MODE_PS2_MODEL30) return fdc->status_register_b();
			break;
		case 2: /* digital output register */
			if (fdc->mode != FDC_MODE_XT) return fdc->digital_output_register;
			break;
		case 4: /* main status */
			b = fdc->main_status();
			//LOG_MSG("FDC: read status 0x%02x\n",b);
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
			//LOG_MSG("FDC: read data 0x%02x\n",b);
			return b;
		case 7: /* digital input register */
			if (fdc->mode == FDC_MODE_XT) break;
			return fdc->digital_input_register();
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

/* ---------------------------------------------------------------------------------------------
 * INT 13h emulation interface
 * --------------------------------------------------------------------------------------------- */

/* the FDC state a real BIOS leaves behind after programming the controller for an access */
void FloppyController::int13_sync(unsigned char drive,uint8_t cylinder,uint8_t rate,uint8_t spec1,uint8_t spec2) {
	drive &= 3;
	current_cylinder[drive] = cylinder;
	set_data_rate(rate);
	spec_srt = spec1 >> 4;
	spec_hut = spec1 & 15;
	spec_hlt = spec2 >> 1;
	spec_nd = (spec2 & 1) != 0;
	/* select the drive, its motor on, the other motors off */
	set_dor((unsigned char)((0x10u << drive) | drive));
}

/* INT 13h AH=00h resets the controller and handles the drive polling interrupts itself */
void FloppyController::int13_reset() {
	if (busy_status || exec_phase != EXEC_NONE) return; /* not while the guest is using the controller */

	FDC_ClearEvents(interface_index);
	for (size_t i=0;i < 4;i++) {
		current_cylinder[i] = 0;
		positioning[i] = false;
		seek[i].active = false;
		sense_pending[i] = false;
	}
	reset_sense_count = 0;
	if (!lock) reset_configure();
	reset_io();
	lower_irq();
}

static void floppy_wait_until(double t) {
	/* the BIOS waits with interrupts enabled */
	CPU_STI();
	while (PIC_FullIndex() < t)
		CALLBACK_Idle();
}

void FLOPPY_INT13_Access(unsigned char drive,unsigned int cylinder,unsigned int head,unsigned int sector,
	unsigned int transferred,bool failed,FloppyInt13Op op) {
	(void)head;

	if (drive >= 2) return;

	imageDisk *image = GetINT13FloppyDrive(drive);
	if (image == NULL) return;

	FloppyMechanics &m = floppy_mech[drive];
	const bool instant = FLOPPY_InstantMode();
	const PhysPt dpt = Real2Phys(RealGetVec(0x1E)); /* the BIOS works from the disk base table */
	const uint8_t spec1 = mem_readb(dpt+0);		/* SRT/HUT */
	const uint8_t spec2 = mem_readb(dpt+1);		/* HLT/ND */
	const uint8_t motor_off_ticks = mem_readb(dpt+2);
	const uint8_t head_settle_ms = mem_readb(dpt+9);
	const uint8_t motor_start = mem_readb(dpt+10);	/* 1/8 seconds */
	const unsigned int rate = floppy_media_rate(image);
	const double srt = floppy_srt_ms(spec1 >> 4,rate);
	const FloppyTrackLayout L = floppy_layout(image);
	const double now = PIC_FullIndex();
	double t = now;

	/* keep the motor timeout from running out while the BIOS works */
	mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,0xFF);

	/* motor: the BIOS turns on the motor of this drive (and off the other) and,
	 * if the motor was not already running, waits the motor start time */
	{
		const uint8_t running = mem_readb(BIOS_DRIVE_RUNNING);
		const bool was_on = (running & (1u << drive)) != 0;

		floppy_motor(floppy_mech[drive ^ 1],false);
		floppy_motor(m,true);
		if (!was_on) t += (motor_start != 0 ? motor_start : 1) * 125.0;
		if (t < floppy_ready_at(m)) t = floppy_ready_at(m);

		mem_writeb(BIOS_DRIVE_RUNNING,(uint8_t)((1u << drive) | (drive << 4) |
			((op == FloppyInt13Op::Write || op == FloppyInt13Op::Format) ? 0x80 : 0x00)));
	}

	/* after a reset the BIOS recalibrates the drive first */
	{
		const uint8_t recal = mem_readb(BIOS_DRIVE_ACTIVE);
		if (!(recal & (1u << drive))) {
			const unsigned int steps = m.track < 79 ? m.track : 79;
			t += steps * srt + (steps != 0 ? head_settle_ms : 0);
			m.track = m.track > steps ? m.track - steps : 0;
			mem_writeb(BIOS_DRIVE_ACTIVE,(uint8_t)(recal | (1u << drive)));
		}
	}

	/* seek, then wait for the head to settle */
	if (m.track != cylinder) {
		const unsigned int steps = (unsigned int)abs((int)m.track - (int)cylinder);
		t += steps * srt + head_settle_ms;
		m.track = cylinder < FLOPPY_MAX_TRACK ? cylinder : FLOPPY_MAX_TRACK;
	}

	/* the FDC loads the head unless it is still loaded from the last access */
	if (t >= m.head_unload_at) t += floppy_hlt_ms(spec2 >> 1,rate);

	if (op == FloppyInt13Op::Format) {
		/* from the index hole, one revolution */
		t += L.time_to(m,t,0) + L.rev_ms;
	}
	else {
		/* wait for the ID field of the first sector to come around, then the sectors pass under the head */
		t += L.time_to(m,t,L.id_start(sector));
		if (transferred > 0)
			t += ((transferred - 1u) * L.sector_bytes + (L.data_end(sector) - L.id_start(sector))) * L.byte_ms;
		/* a sector that cannot be found takes two index pulses */
		if (failed) t += L.time_to(m,t,0) + L.rev_ms;
	}

	m.head_unload_at = t + floppy_hut_ms(spec1 & 15,rate);

	/* the FDC now has the state the BIOS left it in */
	if (floppycontroller[0] != NULL)
		floppycontroller[0]->int13_sync(drive,(uint8_t)cylinder,(uint8_t)rate,spec1,spec2);

	mem_writeb(BIOS_FLOPPY_DRIVE_0_CURRENT_TRACK + drive,(uint8_t)cylinder);
	{
		/* last data rate selected, bits 7-6 */
		static const uint8_t bda_rate[4] = { 0x00/*500K*/, 0x40/*300K*/, 0x80/*250K*/, 0xC0/*1M*/ };
		mem_writeb(BIOS_FLOPPY_MEDIA_CONTROL,(uint8_t)((mem_readb(BIOS_FLOPPY_MEDIA_CONTROL) & 0x3F) | bda_rate[rate]));
	}

	if (!instant) floppy_wait_until(t);

	/* the motor goes off when the timeout counted down by IRQ 0 runs out */
	mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,motor_off_ticks);
}

void FLOPPY_INT13_Reset(void) {
	/* drives must be recalibrated, the FDC forgets the cylinder of each drive */
	mem_writeb(BIOS_DRIVE_ACTIVE,mem_readb(BIOS_DRIVE_ACTIVE) & 0x70);

	if (floppycontroller[0] != NULL)
		floppycontroller[0]->int13_reset();
}

void FLOPPY_BIOS_MotorTimeout(uint8_t running) {

	for (unsigned char d=0;d < 2;d++) {
		if (!(running & (1u << d))) continue;

		floppy_motor(floppy_mech[d],false);
		if (floppycontroller[0] != NULL) {
			FloppyController *fdc = floppycontroller[0];
			fdc->set_dor((unsigned char)(fdc->digital_output_register & ~(0x10u << d)));
		}
	}
}

/* ---------------------------------------------------------------------------------------------
 * PC-98 disk BIOS (INT 1Bh) interface
 * --------------------------------------------------------------------------------------------- */

/* Find the next ID field from time t that matches (or any ID field, for read ID), giving up at the
 * second index pulse like the uPD765. Returns 00h and the sector's index and the time its ID field
 * starts, or the INT 1Bh error code: Cxh no data, Exh missing address mark. */
static uint8_t floppy_pc98_find(const FloppyMechanics &m,const FloppyTrackMap &M,double &t,bool fm,bool any,
	uint8_t c,uint8_t h,uint8_t r,uint8_t n,unsigned int &index) {
	const double give_up = t + M.L.time_to(m,t,0) + M.L.rev_ms;
	double best = 1e30;
	bool any_id = false;
	int found = -1;

	for (size_t k=0;k < M.secs.size();k++) {
		const imageDiskTrackSector &ts = M.secs[k];
		if (ts.fm != fm || ts.id_crc_error) continue;
		any_id = true;

		const double w = t + M.L.time_to(m,t,M.id_pos[k]);
		if (w > give_up) continue;
		if (!any && (ts.c != c || ts.h != h || ts.r != r || ts.n != n)) continue;

		if (w < best) {
			best = w;
			found = (int)k;
		}
	}

	if (found < 0) {
		t = give_up;
		return any_id ? 0xC0 : 0xE0;
	}

	index = (unsigned int)found;
	t = best;
	return 0x00;
}

uint8_t FLOPPY_PC98_BIOS_Access(FloppyPC98BiosRequest &rq) {
	const unsigned char drive = rq.drive & 3;
	if (drive >= 2) return 0x60;

	imageDisk *image = GetINT13FloppyDrive(drive);
	if (image == NULL || image->class_id == imageDisk::ID_EMPTY_DRIVE) return 0x60; /* not ready: no disk */

	FloppyController *fdc = floppycontroller[0];
	FloppyMechanics &m = floppy_mech[drive];
	const bool instant = FLOPPY_InstantMode();
	const unsigned int rate = floppy_media_rate(image);
	/* the BIOS programs the FDC with its own SPECIFY values, the FDC keeps them */
	const double srt = floppy_srt_ms(fdc != NULL ? fdc->spec_srt : 0xD,rate);
	const double hlt = floppy_hlt_ms(fdc != NULL ? fdc->spec_hlt : 0x01,rate);
	const double hut = floppy_hut_ms(fdc != NULL ? fdc->spec_hut : 0x0F,rate);
	const double head_settle_ms = 15.0;
	const bool transfer = (rq.command == 0x01 || rq.command == 0x02 || rq.command == 0x05 ||
		rq.command == 0x06 || rq.command == 0x09 || rq.command == 0x0C);
	const bool writing = (rq.command == 0x05 || rq.command == 0x09);
	uint8_t code = 0x00;
	double t = PIC_FullIndex();

	/* the transfer area may not cross a 64KB boundary (DMA) */
	if (transfer && rq.bytes != 0 && ((rq.memaddr & 0xFFFFu) + rq.bytes) > 0x10000u)
		return 0x20;

	/* the motor has to be on and the disk up to speed */
	if (!m.motor) {
		if (fdc != NULL && IS_PC98_ARCH) {
			fdc->pc98_control |= 0x08;
			fdc->pc98_update_motors();
		}
		if (!m.motor) floppy_motor(m,true);
	}
	if (t < floppy_ready_at(m)) t = floppy_ready_at(m);

	/* seek (recalibrate: to cylinder 0), then wait for the head to settle */
	if (rq.seek || rq.command == 0x00 || rq.command == 0x07) {
		const unsigned int target = rq.command == 0x07 ? 0u : MIN((unsigned int)rq.c,FLOPPY_MAX_TRACK);
		if (m.track != target) {
			const unsigned int steps = (unsigned int)abs((int)m.track - (int)target);
			t += steps * srt + head_settle_ms;
			m.track = target;
			/* a step pulse with a disk in the drive resets the disk change line */
			if (drive < MAX_DISK_IMAGES) imageDiskChange[drive] = false;
		}
	}

	if (rq.command != 0x00 && rq.command != 0x07) {
		const bool fm = !rq.mfm;
		/* The head select of the FDC command is DH bit 0 combined with AL bit 2: AL bits 2-0 line up
		 * with the HD and drive select bits of the uPD765 command, and adding the head bit into them
		 * flips the side when AL bit 2 is set. Ys' disk has each logical track on the side after it,
		 * and reads it with AL bit 2 set: ID head 0 on side 1, ID head 1 on side 0 (then the decoy
		 * copy of the same IDs on side 1 with AL bit 2 clear). */
		uint8_t hds = (uint8_t)((rq.h ^ (rq.side1 ? 1 : 0)) & 1);
		FloppyTrackMap M;

		floppy_track_map(image,m.track,hds,M);
		if (t >= m.head_unload_at) t += hlt;

		if (rq.command == 0x0A) {
			/* read ID: the first good ID field that comes along */
			unsigned int k = 0;
			code = floppy_pc98_find(m,M,t,fm,true,0,0,0,0,k);
			if (code == 0x00) {
				const imageDiskTrackSector &ts = M.secs[k];
				rq.c = ts.c;
				rq.h = ts.h;
				rq.r = ts.r;
				rq.n = ts.n;
				t += 22.0 * (fm ? 2.0 : 1.0) * M.L.byte_ms;
			}
		}
		else if (rq.command == 0x0D) {
			/* format track: from the index hole, one revolution. The data in the image stays as it is. */
			t += M.L.time_to(m,t,0) + M.L.rev_ms;
			LOG(LOG_MISC,LOG_DEBUG)("FDC: INT 1Bh format track is not carried out on the image (C/H %u/%u)",m.track,hds);
		}
		else if (transfer) {
			const unsigned int size = rq.n <= 7 ? (128u << rq.n) : 16384u;
			std::vector<unsigned char> buf(size);
			uint32_t remaining = rq.bytes;
			uint32_t memaddr = rq.memaddr;
			uint8_t r = rq.r,h = rq.h;
			unsigned int diag_index = 0;
			bool first = true;

			/* read diagnostic starts at the index hole, with the first sector on the track */
			if (rq.command == 0x02) t += M.L.time_to(m,t,0);

			while (remaining > 0) {
				unsigned int k = 0;

				if (rq.command == 0x02) {
					if (diag_index >= M.secs.size()) { code = 0x30; break; }
					k = diag_index++;
					t += M.L.time_to(m,t,M.id_pos[k]);
				}
				else {
					code = floppy_pc98_find(m,M,t,fm,false,rq.c,h,r,rq.n,k);
					/* multi-track: the end of side 0 continues with sector 1 of side 1 */
					if (code != 0x00 && rq.mt && hds == 0 && !first) {
						hds = 1;
						h ^= 1;
						r = 1;
						floppy_track_map(image,m.track,hds,M);
						code = floppy_pc98_find(m,M,t,fm,false,rq.c,h,r,rq.n,k);
					}
					if (code != 0x00) break;
				}

				const imageDiskTrackSector &ts = M.secs[k];
				const double density = ts.fm ? 2.0 : 1.0;
				const uint32_t now = remaining < size ? remaining : size;

				/* ID field, gap 2, data address mark */
				t += (22.0 + 22.0 + 16.0) * density * M.L.byte_ms;
				first = false;

				if (ts.no_data) { code = 0xE0; break; }

				/* deleted data address mark: verify skips the sector, read data/read deleted data
				 * stop after it with the control mark code, read diagnostic reads on */
				const bool cm = (rq.command == 0x0C) ? !ts.deleted : ts.deleted;
				if (cm && rq.command == 0x01) {
					t += (ts.data_size + 2.0) * density * M.L.byte_ms;
					r++;
					continue;
				}

				if (writing) {
					if (image->Is_Write_Protected()) { code = 0x70; break; }
					for (uint32_t i=0;i < size;i++) buf[i] = i < now ? mem_readb(memaddr+i) : 0;
					Int13Status err = image->Write_Track_Sector(m.track,hds,k,buf.data(),size);
					if (err == Int13Status::WriteProtected) { code = 0x70; break; }
					if (err != Int13Status::NoError) { code = 0xC0; break; }
				}
				else {
					Int13Status err = image->Read_Track_Sector(m.track,hds,k,buf.data(),size);
					if (err != Int13Status::NoError && err != Int13Status::DataError) { code = 0xC0; break; }
					if (rq.command != 0x01) {
						for (uint32_t i=0;i < now;i++) mem_writeb(memaddr+i,buf[i]);
					}
					if (err == Int13Status::DataError && rq.command != 0x02) { code = 0xA0; break; }
				}

				t += (size + 2.0) * density * M.L.byte_ms;
				memaddr += now;
				remaining -= now;

				if (ts.data_crc_error && rq.command != 0x02) { code = 0xA0; break; }
				if (cm && rq.command != 0x02) { code = 0x10; break; }
				r++;
			}

			/* the BIOS clears the DMA terminal count after a read (Arsys Star Cruiser) */
			if (!writing && IS_PC98_ARCH) {
				DmaChannel *dma = GetDMAChannel(2);
				if (dma) dma->tcount = false;
			}
		}

		m.head_unload_at = t + hut;
	}

	/* the FDC now has the state the BIOS left it in */
	if (fdc != NULL) fdc->current_cylinder[drive] = (uint8_t)m.track;

	if (!instant) floppy_wait_until(t);
	return code;
}
