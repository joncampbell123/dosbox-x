/*
 * Floppy drive and controller emulation for DOSBox-X: interface to the BIOS (INT 13h) emulation
 * (C) 2012 Jonathan Campbell
 */

#ifndef DOSBOX_FLOPPY_H
#define DOSBOX_FLOPPY_H

#include <stdint.h>

/* What an INT 13h call did to a floppy drive, for FLOPPY_INT13_Access() */
enum class FloppyInt13Op {
	Read,
	Write,
	Verify,
	Format
};

/* True if floppy operations should be carried out without the delays of a real
 * floppy drive ([fdc, primary] "instant mode"). Applies to the FDC and INT 13h alike. */
bool FLOPPY_InstantMode(void);

/* INT 13h floppy access: account for what a real BIOS programming the FDC would do
 * (motor on, recalibrate, seek, head load, waiting for the sectors to rotate under the
 * head, transfer), keep the emulated drive, FDC and BIOS data area in the state the access
 * would leave them in, and wait as long as the access would take (unless instant mode).
 *
 * drive:       INT 13h floppy drive number (0 or 1)
 * cylinder, head, sector: first sector of the access (for Format, the track)
 * transferred: number of sectors that were transferred
 * failed:      true if the access stopped with an error after "transferred" sectors */
void FLOPPY_INT13_Access(unsigned char drive,unsigned int cylinder,unsigned int head,unsigned int sector,
	unsigned int transferred,bool failed,FloppyInt13Op op);

/* INT 13h AH=00h for a floppy drive: the BIOS resets the FDC, after which drives must be recalibrated */
void FLOPPY_INT13_Reset(void);

/* The BIOS floppy motor timeout (BIOS data area 40:40h) ran out: turn the motors off */
void FLOPPY_BIOS_MotorTimeout(void);

/* PC-98 disk BIOS (INT 1Bh) floppy access, done the way the BIOS would do it with the uPD765:
 * on the same drive mechanics and the same tracks (sectors found by their ID fields as they turn
 * past the head) as the FDC emulation, waiting as long as the access takes (unless instant mode),
 * and leaving the FDC in the state the BIOS would. */
struct FloppyPC98BiosRequest {
	unsigned char	drive = 0;		/* AL bits 1-0 */
	uint8_t		command = 0;		/* AH bits 3-0: 01h verify, 02h read diagnostic, 05h write, 06h read,
						 * 07h recalibrate, 09h write deleted, 0Ah read ID, 0Ch read deleted,
						 * 0Dh format, 00h seek */
	bool		mt = false;		/* AH bit 7: multi-track */
	bool		mfm = true;		/* AH bit 6: double density */
	bool		seek = false;		/* AH bit 4: seek to cylinder CL first */
	uint8_t		c = 0,h = 0,r = 0,n = 0;/* CL, DH, DL, CH */
	bool		side1 = false;		/* AL bit 2: the physical side is the other one than DH says (DH is the H of the ID) */
	uint32_t	bytes = 0;		/* BX */
	uint32_t	memaddr = 0;		/* ES:BP as a physical address */
};

/* returns the INT 1Bh return code (AH), CF set unless 00h or 1xh. For read ID, the ID is returned in c,h,r,n. */
uint8_t FLOPPY_PC98_BIOS_Access(FloppyPC98BiosRequest &rq);

#endif
