/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h"
#include "bios_disk.h"
#include "../dos/drives.h"
#include "imagedisk_msdosblockdev.h"

#if !defined(OSFREE)
Int13Status imageDiskMSDOSBlockDevice::Read_AbsoluteSector(uint32_t sectnum, void * data) {
	const unsigned int max_sects = (bdevbuf_sz - 16) / sector_size;
	if (max_sects == 0) return Int13Status::SectorNotFound;

	const uint16_t count = 1;
	const uint32_t sector = sectnum;
	const uint16_t strategy = mem_readw(devhdr+6);
	const uint16_t intrupt = mem_readw(devhdr+8);
	unsigned char *p_data = (unsigned char*)data;

	uint16_t oldbx = reg_bx;
	uint16_t oldds = SegValue(ds);
	uint16_t oldes = SegValue(es);

	DOS_DEVHDR::req_rwio req = {0};
	req.hdr.record_length = sizeof(req);
	req.hdr.unit_code = unit_code;
	req.hdr.cmd_code = DEVFUNC_READ;
	req.xfer_addr = RealMake(bdevbuf_seg+1,0);
	req.count = count;
	req.ptr_volid = RealMake(bdevbuf_seg,0);
	if (attr & DEVATTRBLK_EXTENDED) {
		req.start_sector = 0xFFFF;
		req.start_sector32 = sector;
	}
	else {
		if (sector > 0xFFFFu) return Int13Status::SectorNotFound;
		req.start_sector = sector;
		req.start_sector32 = 0;
	}
	req.media_dpb = media_dpb;
	MEM_BlockWrite(PhysMake(dos.dcp,0),&req,sizeof(req));

	LOG(LOG_MISC,LOG_DEBUG)("Block device read devseg=%x sectnum %x devseg %x strat %x intr %x xfer=%x:%x",
		devseg,sectnum,devseg,strategy,intrupt,
		req.xfer_addr>>16,req.xfer_addr&0xFFFFu);

	reg_bx = 0;
	SegSet16(ds, devseg);
	SegSet16(es, dos.dcp);
	CALLBACK_RunRealFar(devseg, strategy);
	CALLBACK_RunRealFar(devseg, intrupt);
	reg_bx = oldbx;
	SegSet16(es, oldes);
	SegSet16(ds, oldds);

	MEM_BlockRead(PhysMake(dos.dcp,0),&req,sizeof(req));

	LOG(LOG_MISC,LOG_DEBUG)("--result status=%x count=%u",
		req.hdr.status,req.count);

	if (req.hdr.status & 0x8000) return Int13Status::ControllerFailure;/*error*/
	if (req.count == 0) return Int13Status::ControllerFailure;/*error*/

	MEM_BlockRead(PhysMake(bdevbuf_seg+1,0),p_data,sector_size);
        return Int13Status::NoError;
}

Int13Status imageDiskMSDOSBlockDevice::Write_AbsoluteSector(uint32_t sectnum, const void * data) {
	const unsigned int max_sects = (bdevbuf_sz - 16) / sector_size;
	if (max_sects == 0) return Int13Status::SectorNotFound;

	const uint16_t count = 1;
	const uint32_t sector = sectnum;
	const uint16_t strategy = mem_readw(devhdr+6);
	const uint16_t intrupt = mem_readw(devhdr+8);
	const unsigned char *p_data = (const unsigned char*)data;

	uint16_t oldbx = reg_bx;
	uint16_t oldds = SegValue(ds);
	uint16_t oldes = SegValue(es);

	DOS_DEVHDR::req_rwio req = {0};
	req.hdr.record_length = sizeof(req);
	req.hdr.unit_code = unit_code;
	req.hdr.cmd_code = DEVFUNC_WRITE;
	req.xfer_addr = RealMake(bdevbuf_seg+1,0);
	req.count = count;
	req.ptr_volid = RealMake(bdevbuf_seg,0);
	if (attr & DEVATTRBLK_EXTENDED) {
		req.start_sector = 0xFFFF;
		req.start_sector32 = sector;
	}
	else {
		if (sector > 0xFFFFu) return Int13Status::SectorNotFound;
		req.start_sector = sector;
		req.start_sector32 = 0;
	}
	req.media_dpb = media_dpb;
	MEM_BlockWrite(PhysMake(dos.dcp,0),&req,sizeof(req));
	MEM_BlockWrite(PhysMake(bdevbuf_seg+1,0),p_data,sector_size);

	LOG(LOG_MISC,LOG_DEBUG)("Block device write devseg=%x sectnum %x devseg %x strat %x intr %x xfer=%x:%x",
		devseg,sectnum,devseg,strategy,intrupt,
		req.xfer_addr>>16,req.xfer_addr&0xFFFFu);

	reg_bx = 0;
	SegSet16(ds, devseg);
	SegSet16(es, dos.dcp);
	CALLBACK_RunRealFar(devseg, strategy);
	CALLBACK_RunRealFar(devseg, intrupt);
	reg_bx = oldbx;
	SegSet16(es, oldes);
	SegSet16(ds, oldds);

	MEM_BlockRead(PhysMake(dos.dcp,0),&req,sizeof(req));

	LOG(LOG_MISC,LOG_DEBUG)("--result status=%x count=%u",
		req.hdr.status,req.count);

	if (req.hdr.status & 0x8000) return Int13Status::ControllerFailure;/*error*/
	if (req.count == 0) return Int13Status::ControllerFailure;/*error*/

        return Int13Status::NoError;
}

bool imageDiskMSDOSBlockDevice::detectDiskChange(void) {
	return false;//TODO
}

imageDiskMSDOSBlockDevice::imageDiskMSDOSBlockDevice() : imageDisk(ID_MSDOSBLOCKDEV) {
}

imageDiskMSDOSBlockDevice::~imageDiskMSDOSBlockDevice() {
}
#endif
