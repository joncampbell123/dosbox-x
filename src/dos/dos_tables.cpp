/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "callback.h"
#include <assert.h>

extern Bitu DOS_PRIVATE_SEGMENT_Size;

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct DOS_TableCase {	
	Bit16u size;
	Bit8u chars[256];
}
GCC_ATTRIBUTE (packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

RealPt DOS_TableUpCase;
RealPt DOS_TableLowCase;

extern bool mainline_compatible_mapping;

static Bitu call_casemap;

static Bit16u dos_memseg=0;//DOS_PRIVATE_SEGMENT;

extern Bitu VGA_BIOS_SEG_END;
bool DOS_GetMemory_unmapped = false;

void DOS_GetMemory_reset() {
	dos_memseg = 0;
}

void DOS_GetMemory_unmap() {
	if (DOS_PRIVATE_SEGMENT != 0) {
		LOG(LOG_MISC,LOG_DEBUG)("Unmapping DOS private segment 0x%04x-0x%04x",DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1);
		if (DOS_PRIVATE_SEGMENT >= 0xA000) MEM_unmap_physmem(DOS_PRIVATE_SEGMENT<<4,(DOS_PRIVATE_SEGMENT_END<<4)-1);
		DOS_GetMemory_unmapped = true;
		DOS_PRIVATE_SEGMENT_END = 0;
		DOS_PRIVATE_SEGMENT = 0;
		dos_memseg = 0;
	}
}

void DOS_GetMemory_Choose() {
	if (DOS_PRIVATE_SEGMENT == 0) {
		if (mainline_compatible_mapping) {
			/* DOSBox mainline compatible: private area 0xC800-0xCFFF */
			DOS_PRIVATE_SEGMENT=0xc800;
			DOS_PRIVATE_SEGMENT_END=0xc800 + DOS_PRIVATE_SEGMENT_Size;
		}
		else {
			/* DOSBox-X non-compatible: Position ourself just past the VGA BIOS */
			/* NTS: Code has been arranged so that DOS kernel init follows BIOS INT10h init */
			DOS_PRIVATE_SEGMENT=VGA_BIOS_SEG_END;
			DOS_PRIVATE_SEGMENT_END=DOS_PRIVATE_SEGMENT + DOS_PRIVATE_SEGMENT_Size;
		}

		if (DOS_PRIVATE_SEGMENT >= 0xA000) {
			memset(GetMemBase()+(DOS_PRIVATE_SEGMENT<<4),0x00,(DOS_PRIVATE_SEGMENT_END-DOS_PRIVATE_SEGMENT)<<4);
			MEM_map_RAM_physmem(DOS_PRIVATE_SEGMENT<<4,(DOS_PRIVATE_SEGMENT_END<<4)-1);
		}

		LOG(LOG_MISC,LOG_DEBUG)("DOS private segment set to 0x%04x-0x%04x",DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1);
	}
}

Bit16u DOS_GetMemory(Bit16u pages,const char *who) {
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
	Bit16u page=dos_memseg;
	LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_GetMemory(0x%04x pages,\"%s\") = 0x%04x",pages,who,page);
	dos_memseg+=pages;
	return page;
}

static Bitu DOS_CaseMapFunc(void) {
	//LOG(LOG_DOSMISC,LOG_ERROR)("Case map routine called : %c",reg_al);
	return CBRET_NONE;
}

static Bit8u country_info[0x22] = {
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

extern bool enable_dbcs_tables;
extern bool enable_filenamechar;
extern bool enable_collating_uppercase;

void DOS_SetupTables(void) {
	Bit16u seg;Bitu i;
	dos.tables.mediaid=RealMake(DOS_GetMemory(4,"dos.tables.mediaid"),0);
	dos.tables.tempdta=RealMake(DOS_GetMemory(4,"dos.tables.tempdta"),0);
	dos.tables.tempdta_fcbdelete=RealMake(DOS_GetMemory(4,"dos.tables.fcbdelete"),0);
	for (i=0;i<DOS_DRIVES;i++) mem_writew(Real2Phys(dos.tables.mediaid)+i*2,0);
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
		mem_writed(Real2Phys(dos.tables.dbcs),0); //empty table
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
		for (i=0; i<256; i++) mem_writeb(Real2Phys(dos.tables.collatingseq)+i+2,i);
		dos.tables.upcase=dos.tables.collatingseq+258;
		mem_writew(Real2Phys(dos.tables.upcase),0x80);
		for (i=0; i<128; i++) mem_writeb(Real2Phys(dos.tables.upcase)+i+2,0x80+i);
	}
	else {
		dos.tables.collatingseq = 0;
		dos.tables.upcase = 0;
	}

	/* Create a fake FCB SFT */
	seg=DOS_GetMemory(4,"Fake FCB SFT");
	real_writed(seg,0,0xffffffff);		//Last File Table
	real_writew(seg,4,100);				//File Table supports 100 files
	dos_infoblock.SetFCBTable(RealMake(seg,0));

	/* Create a fake DPB */
	dos.tables.dpb=DOS_GetMemory(2,"dos.tables.dpb");
	for(Bitu d=0;d<26;d++) real_writeb(dos.tables.dpb,d,d);

	/* Create a fake disk buffer head */
	seg=DOS_GetMemory(6,"Fake disk buffer head");
	for (Bitu ct=0; ct<0x20; ct++) real_writeb(seg,ct,0);
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
	host_writed(country_info + 0x12, CALLBACK_RealPointer(call_casemap));
	dos.tables.country=country_info;
}

