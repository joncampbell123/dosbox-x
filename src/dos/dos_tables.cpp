/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
#include "mem.h"
#include "dos_inc.h"
#include "callback.h"
#include "control.h"
#include <assert.h>

extern int maxfcb;
extern Bitu DOS_PRIVATE_SEGMENT_Size;

void CALLBACK_DeAllocate(Bitu in);

std::list<DOS_GetMemLog_Entry> DOS_GetMemLog;

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct DOS_TableCase {	
	uint16_t size;
	uint8_t chars[256];
}
GCC_ATTRIBUTE (packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

RealPt DOS_DriveDataListHead=0;       // INT 2Fh AX=0803h DRIVER.SYS drive data table list
RealPt DOS_TableUpCase;
RealPt DOS_TableLowCase;

static Bitu call_casemap = 0;

void DOS_Casemap_Free(void) {
    if (call_casemap != 0) {
        CALLBACK_DeAllocate(call_casemap);
        call_casemap = 0;
    }
}

static uint16_t dos_memseg=0;//DOS_PRIVATE_SEGMENT;

extern Bitu VGA_BIOS_SEG_END;
bool DOS_GetMemory_unmapped = false;

void DOS_GetMemory_reset() {
	dos_memseg = 0;
}

void DOS_GetMemory_reinit() {
    DOS_GetMemory_unmapped = false;
    DOS_GetMemory_reset();
}

void DOS_GetMemory_unmap() {
	if (DOS_PRIVATE_SEGMENT != 0) {
		LOG(LOG_DOSMISC,LOG_DEBUG)("Unmapping DOS private segment 0x%04x-0x%04x",DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1u);
		if (DOS_PRIVATE_SEGMENT >= 0xA000u) MEM_unmap_physmem((Bitu)DOS_PRIVATE_SEGMENT<<4u,((Bitu)DOS_PRIVATE_SEGMENT_END<<4u)-1u);
		DOS_GetMemory_unmapped = true;
		DOS_PRIVATE_SEGMENT_END = 0;
		DOS_PRIVATE_SEGMENT = 0;
		dos_memseg = 0;
	}
}

bool DOS_User_Wants_UMBs() {
    const Section_prop* section = static_cast<Section_prop*>(control->GetSection("dos"));
    return section->Get_bool("umb");
}

void DOS_GetMemory_Choose() {
	if (DOS_PRIVATE_SEGMENT == 0) {
        /* DOSBox-X non-compatible: Position ourself just past the VGA BIOS */
        /* NTS: Code has been arranged so that DOS kernel init follows BIOS INT10h init */
        DOS_PRIVATE_SEGMENT=(uint16_t)VGA_BIOS_SEG_END;
        DOS_PRIVATE_SEGMENT_END= (uint16_t)(DOS_PRIVATE_SEGMENT + DOS_PRIVATE_SEGMENT_Size);

        if (IS_PC98_ARCH) {
            bool PC98_FM_SoundBios_Enabled(void);

            /* Do not let the private segment overlap with anything else after segment C800:0000 including the SOUND ROM at CC00:0000.
             * Limiting to 32KB also leaves room for UMBs if enabled between C800:0000 and the EMS page frame at (usually) D000:0000 */
            unsigned int limit = 0xD000;

            if (PC98_FM_SoundBios_Enabled()) {
                // TODO: What about sound BIOSes larger than 16KB?
                if (limit > 0xCC00)
                    limit = 0xCC00;
            }

            if (DOS_User_Wants_UMBs()) {
                // leave room for UMBs, things are cramped a bit in PC-98 mode
                if (limit > 0xC600)
                    limit = 0xC600;
            }

            if (DOS_PRIVATE_SEGMENT_END > limit)
                DOS_PRIVATE_SEGMENT_END = limit;

            if (DOS_PRIVATE_SEGMENT >= DOS_PRIVATE_SEGMENT_END)
                E_Exit("Insufficient room in upper memory area for private area");
        }

		if (DOS_PRIVATE_SEGMENT >= 0xA000) {
			memset(GetMemBase()+((Bitu)DOS_PRIVATE_SEGMENT<<4u),0x00,(Bitu)(DOS_PRIVATE_SEGMENT_END-DOS_PRIVATE_SEGMENT)<<4u);
			MEM_map_RAM_physmem((Bitu)DOS_PRIVATE_SEGMENT<<4u,((Bitu)DOS_PRIVATE_SEGMENT_END<<4u)-1u);
		}

		LOG(LOG_DOSMISC,LOG_DEBUG)("DOS private segment set to 0x%04x-0x%04x",DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1);
	}
}

uint16_t DOS_GetMemory(uint16_t pages,const char *who) {
	if (who == NULL) who = "";
	if (dos_memseg == 0) {
		if (DOS_GetMemory_unmapped) E_Exit("DOS:Attempt to use DOS_GetMemory() when private area was unmapped by BOOT");
		if (DOS_PRIVATE_SEGMENT == 0) DOS_GetMemory_Choose();
		dos_memseg = DOS_PRIVATE_SEGMENT;
		if (dos_memseg == 0) E_Exit("DOS:DOS_GetMemory() before private area has been initialized");
	}

	if (((Bitu)pages+(Bitu)dos_memseg) > DOS_PRIVATE_SEGMENT_END) {
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS_GetMemory(%u) failed for '%s' (alloc=0x%04x segment=0x%04x end=0x%04x)",
			pages,who,dos_memseg,DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END);
		E_Exit("DOS:Not enough memory for internal tables");
	}
	uint16_t page=dos_memseg;
	LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_GetMemory(0x%04x pages,\"%s\") = 0x%04x",pages,who,page);

    {
        DOS_GetMemLog_Entry ent;

        ent.segbase = page;
        ent.pages = pages;
        ent.who = who;

        DOS_GetMemLog.push_back(ent);
    }

	dos_memseg+=pages;
	return page;
}

static Bitu DOS_CaseMapFunc(void) {
	//LOG(LOG_DOSMISC,LOG_ERROR)("Case map routine called : %c",reg_al);
	return CBRET_NONE;
}

static uint8_t country_info[0x22] = {
/* Date format      */  0x00, 0x00,
/* Currencystring   */  0x24, 0x00, 0x00, 0x00, 0x00,
/* Thousands sep    */  0x2c, 0x00,
/* Decimal sep      */  0x2e, 0x00,
/* Date sep         */  0x2d, 0x00,
/* time sep         */  0x3a, 0x00,
/* currency form    */  0x00,
/* digits after dec */  0x02,
/* Time format      */  0x00,
/* Casemap          */  0x00, 0x00, 0x00, 0x00,
/* Data sep         */  0x2c, 0x00,
/* Reservered 5     */  0x00, 0x00, 0x00, 0x00, 0x00,
/* Reservered 5     */  0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t country_info_pc98[0x22] = {
/* Date format      */  0x02, 0x00,
/* Currencystring   */  0x5C, 0x00, 0x00, 0x00, 0x00,
/* Thousands sep    */  0x2c, 0x00,
/* Decimal sep      */  0x2e, 0x00,
/* Date sep         */  0x2d, 0x00,
/* time sep         */  0x3a, 0x00,
/* currency form    */  0x00,
/* digits after dec */  0x00,
/* Time format      */  0x01,
/* Casemap          */  0x00, 0x00, 0x00, 0x00,
/* Data sep         */  0x2c, 0x00,
/* Reservered 5     */  0x00, 0x00, 0x00, 0x00, 0x00,
/* Reservered 5     */  0x00, 0x00, 0x00, 0x00, 0x00
};

extern bool enable_dbcs_tables;
extern bool enable_filenamechar;
extern bool enable_collating_uppercase;

PhysPt DOS_Get_DPB(unsigned int dos_drive) {
    if (dos_drive >= DOS_DRIVES)
        return 0;

    return PhysMake(dos.tables.dpb,dos_drive*dos.tables.dpb_size);
}

void DOS_SetupTables(void) {
	uint16_t seg;uint16_t i;
	dos.tables.tempdta=RealMake(DOS_GetMemory(4,"dos.tables.tempdta"),0);
	dos.tables.tempdta_fcbdelete=RealMake(DOS_GetMemory(4,"dos.tables.fcbdelete"),0);
	/* Create the DOS Info Block */
	dos_infoblock.SetLocation(DOS_INFOBLOCK_SEG); //c2woody
   
	/* create SDA */
	DOS_SDA(DOS_SDA_SEG,0).Init();

	/* Some weird files >20 detection routine */
	/* Possibly obselete when SFT is properly handled */
	real_writed(DOS_CONSTRING_SEG,0x0a,0x204e4f43);
	real_writed(DOS_CONSTRING_SEG,0x1a,0x204e4f43);
	real_writed(DOS_CONSTRING_SEG,0x2a,0x204e4f43);

	/* create a CON device driver */
	seg=DOS_CONDRV_SEG;
 	real_writed(seg,0x00,0xffffffff);	// next ptr
 	real_writew(seg,0x04,0x8013);		// attributes
  	real_writed(seg,0x06,0xffffffff);	// strategy routine
  	real_writed(seg,0x0a,0x204e4f43);	// driver name
  	real_writed(seg,0x0e,0x20202020);	// driver name
	dos_infoblock.SetDeviceChainStart(RealMake(seg,0));
   
	/* Create a fake Current Directory Structure */
	seg=DOS_CDS_SEG;
	real_writed(seg,0x00,0x005c3a43);
	dos_infoblock.SetCurDirStruct(RealMake(seg,0));



    /* Allocate DCBS DOUBLE BYTE CHARACTER SET LEAD-BYTE TABLE */
    if (enable_dbcs_tables) {
        dos.tables.dbcs=RealMake(DOS_GetMemory(12,"dos.tables.dbcs"),0);

        if (IS_PC98_ARCH) {
            // write a valid table, or else Windows 3.1 is unhappy.
            // Values are copied from INT 21h AX=6300h as returned by an MS-DOS 6.22 boot disk
            mem_writeb(Real2Phys(dos.tables.dbcs)+0,0x81);  // low/high DBCS pair 1
            mem_writeb(Real2Phys(dos.tables.dbcs)+1,0x9F);
            mem_writeb(Real2Phys(dos.tables.dbcs)+2,0xE0);  // low/high DBCS pair 2
            mem_writeb(Real2Phys(dos.tables.dbcs)+3,0xFC);
            mem_writed(Real2Phys(dos.tables.dbcs)+4,0);
        }
        else {
            mem_writed(Real2Phys(dos.tables.dbcs),0); //empty table
        }
    }
    else {
        dos.tables.dbcs=0;
    }
	/* FILENAME CHARACTER TABLE */
	if (enable_filenamechar) {
		dos.tables.filenamechar=RealMake(DOS_GetMemory(2,"dos.tables.filenamechar"),0);
		mem_writew(Real2Phys(dos.tables.filenamechar)+0x00,0x16);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x02,0x01);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x03,0x00);	// allowed chars from
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x04,0xff);	// ...to
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x05,0x00);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x06,0x00);	// excluded chars from
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x07,0x20);	// ...to
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x08,0x02);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x09,0x0e);	// number of illegal separators
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x0a,0x2e);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x0b,0x22);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x0c,0x2f);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x0d,0x5c);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x0e,0x5b);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x0f,0x5d);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x10,0x3a);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x11,0x7c);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x12,0x3c);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x13,0x3e);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x14,0x2b);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x15,0x3d);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x16,0x3b);
		mem_writeb(Real2Phys(dos.tables.filenamechar)+0x17,0x2c);
	}
	else {
		dos.tables.filenamechar = 0;
	}
	/* COLLATING SEQUENCE TABLE + UPCASE TABLE*/
	// 256 bytes for col table, 128 for upcase, 4 for number of entries
	if (enable_collating_uppercase) {
		dos.tables.collatingseq=RealMake(DOS_GetMemory(25,"dos.tables.collatingseq"),0);
		mem_writew(Real2Phys(dos.tables.collatingseq),0x100);
		for (i=0; i<256; i++) mem_writeb(Real2Phys(dos.tables.collatingseq)+i+2,(uint8_t)i);
		dos.tables.upcase=dos.tables.collatingseq+258;
		mem_writew(Real2Phys(dos.tables.upcase),0x80);
		for (i=0; i<128; i++) mem_writeb(Real2Phys(dos.tables.upcase)+i+2,(uint8_t)0x80+i);
	}
	else {
		dos.tables.collatingseq = 0;
		dos.tables.upcase = 0;
	}

	/* Create a fake FCB SFT */
	seg=DOS_GetMemory(4,"Fake FCB SFT");
	real_writed(seg,0,0xffffffff);		//Last File Table
	real_writew(seg,4,maxfcb);			//File Table supports 100 files
	dos_infoblock.SetFCBTable(RealMake(seg,0));

	/* Create a fake DPB */
	dos.tables.dpb=DOS_GetMemory(((DOS_DRIVES*dos.tables.dpb_size)+15u)/16u,"dos.tables.dpb");
    dos.tables.mediaid_offset=0x17;	//Media ID offset in DPB (MS-DOS 4.x-6.x)
	dos.tables.mediaid=RealMake(dos.tables.dpb,dos.tables.mediaid_offset);
	for (i=0;i<DOS_DRIVES;i++) {
        real_writeb(dos.tables.dpb,i*dos.tables.dpb_size,(uint8_t)i);             // drive number
        real_writeb(dos.tables.dpb,i*dos.tables.dpb_size+1,(uint8_t)i);           // unit number
        real_writew(dos.tables.dpb,i*dos.tables.dpb_size+2,0x0200);     // bytes per sector
        real_writew(dos.tables.dpb,i*dos.tables.dpb_size+6,0x0001);     // reserved sectors at the beginning of the drive
        mem_writew(Real2Phys(dos.tables.mediaid)+i*dos.tables.dpb_size,0u);
        real_writew(dos.tables.dpb,i*dos.tables.dpb_size+0x1F,0xFFFF);      // number of free clusters or 0xFFFF if unknown

        // next DPB pointer
        if ((i+1) < DOS_DRIVES)
            real_writed(dos.tables.dpb,i*dos.tables.dpb_size+0x19,RealMake(dos.tables.dpb,(i+1)*dos.tables.dpb_size));
        else
            real_writed(dos.tables.dpb,i*dos.tables.dpb_size+0x19,0xFFFFFFFF); // ED4.EXE (provided by Yksoft1) expects this, or else loops forever
	}
    dos_infoblock.SetFirstDPB(RealMake(dos.tables.dpb,0));

	/* Create a fake disk buffer head */
	seg=DOS_GetMemory(6,"Fake disk buffer head");
	for (uint8_t ct=0; ct<0x20; ct++) real_writeb(seg,ct,0);
	real_writew(seg,0x00,0xffff);		// forward ptr
	real_writew(seg,0x02,0xffff);		// backward ptr
	real_writeb(seg,0x04,0xff);			// not in use
	real_writeb(seg,0x0a,0x01);			// number of FATs
	real_writed(seg,0x0d,0xffffffff);	// pointer to DPB
	dos_infoblock.SetDiskBufferHeadPt(RealMake(seg,0));

	/* Set buffers to a nice value */
	dos_infoblock.SetBuffers(50,50);

	/* case map routine INT 0x21 0x38 */
	call_casemap = CALLBACK_Allocate();
	CALLBACK_Setup(call_casemap,DOS_CaseMapFunc,CB_RETF,"DOS CaseMap");
	/* Add it to country structure */
    if (IS_PC98_ARCH) {
        host_writed(country_info_pc98 + 0x12, CALLBACK_RealPointer(call_casemap));
        dos.tables.country=country_info_pc98;
    }
    else {
        host_writed(country_info + 0x12, CALLBACK_RealPointer(call_casemap));
        dos.tables.country=country_info;
    }

    /* PC-98 INT 1Bh device list (60:6Ch-7Bh).
     * For now, just write a fake list to satisfy any PC-98 game that
     * requires a "master disk" to run even if running from an HDI.
     * See also: [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20NEC%20PC%2d98/Collections/Undocumented%209801%2c%209821%20Volume%202%20%28webtech.co.jp%29/memdos%2etxt]
     * This is needed to run "Legend of Heroes III" */
    if (IS_PC98_ARCH) {
        // FIXME: This is just a fake list. At some point in the future, this
        //        list needs to reflect the state of all MOUNT/IMGMOUNT commands
        //        while in the DOS environment provided by this emulation.
        //
        //        The byte values seem to match drive letter assignment, as noted:
        //        [https://github.com/joncampbell123/dosbox-x/issues/1226]
        for (i=0;i < 0x10;i++) real_writeb(0x60,0x6C+i,0);
        real_writeb(0x60,0x6C,0xA0);    /* hard drive */
        real_writeb(0x60,0x6D,0x90);    /* floppy drive */
    }

    /* fake DRIVER.SYS data table list, to satisfy Windows 95 setup.
     * The list is supposed to be a linked list of drive BPBs, INT 13h info, etc.
     * terminated by offset 0xFFFF. For now, just point at a 0xFFFF.
     * Note that Windows 95 setup and SCANDISK.EXE have different criteria on
     * the return value of INT 2Fh AX=803h and returning without a pointer
     * really isn't an option. According to RBIL this interface is built into
     * MS-DOS. */
    DOS_DriveDataListHead = RealMake(DOS_GetMemory(1/*paragraph*/,"driver.sys.data.list"),0);
    mem_writew(Real2Phys(DOS_DriveDataListHead)+0x00,0xFFFF); /* list termination */
    mem_writew(Real2Phys(DOS_DriveDataListHead)+0x02,0xFFFF);
    mem_writew(Real2Phys(DOS_DriveDataListHead)+0x04,0x0000);
}

// save state support
void POD_Save_DOS_Tables( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &DOS_TableUpCase, DOS_TableUpCase );
	WRITE_POD( &DOS_TableLowCase, DOS_TableLowCase );

	WRITE_POD( &dos_memseg, dos_memseg );
}

void POD_Load_DOS_Tables( std::istream& stream )
{
	// - pure data
	READ_POD( &DOS_TableUpCase, DOS_TableUpCase );
	READ_POD( &DOS_TableLowCase, DOS_TableLowCase );

	READ_POD( &dos_memseg, dos_memseg );
}
