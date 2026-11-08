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

#endif
