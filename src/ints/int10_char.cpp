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


/* Character displaying moving functions */

#include "dosbox.h"
#include "bios.h"
#include "logging.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "shiftjis.h"
#include "callback.h"
#include "dos_inc.h"
#include "jfont.h"
#include "regs.h"
#include <string.h>

uint8_t prevchr = 0;
uint8_t prevattr = 0;
uint16_t pcol = 0, prow = 0, pchr = 0;
bool CheckBoxDrawing(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4);
uint8_t DefaultANSIAttr();
uint16_t GetTextSeg();
uint8_t *GetSbcs24Font(Bitu code);
uint8_t *GetDbcs24Font(Bitu code);

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

static void MCGA2_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    BIOS_CHEIGHT;
    PhysPt dest=base+((CurMode->twidth*rnew)*cheight+cleft);
    PhysPt src=base+((CurMode->twidth*rold)*cheight+cleft);
    Bitu copy=(Bitu)(cright-cleft);
    Bitu nextline=CurMode->twidth;
    for (Bitu i=0;i<cheight;i++) {
        MEM_BlockCopy(dest,src,copy);
        dest+=nextline;src+=nextline;
    }
}

static void CGA2_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    BIOS_CHEIGHT;
    PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/2)+cleft);
    PhysPt src=base+((CurMode->twidth*rold)*(cheight/2)+cleft);
    Bitu copy=(Bitu)(cright-cleft);
    Bitu nextline=CurMode->twidth;
    for (Bitu i=0;i<cheight/2U;i++) {
        MEM_BlockCopy(dest,src,copy);
        MEM_BlockCopy(dest+8*1024,src+8*1024,copy);
        dest+=nextline;src+=nextline;
    }
}

static void CGA4_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    BIOS_CHEIGHT;
    PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/2)+cleft)*2;
    PhysPt src=base+((CurMode->twidth*rold)*(cheight/2)+cleft)*2;   
    Bitu copy=(Bitu)(cright-cleft)*2u;Bitu nextline=(Bitu)CurMode->twidth*2u;
    for (Bitu i=0;i<cheight/2U;i++) {
        MEM_BlockCopy(dest,src,copy);
        MEM_BlockCopy(dest+8*1024,src+8*1024,copy);
        dest+=nextline;src+=nextline;
    }
}

static void TANDY16_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    BIOS_CHEIGHT;
    uint8_t banks=CurMode->twidth/10;
    PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/banks)+cleft)*4;
    PhysPt src=base+((CurMode->twidth*rold)*(cheight/banks)+cleft)*4;
    Bitu copy=(Bitu)(cright-cleft)*4u;Bitu nextline=(Bitu)CurMode->twidth*4u;
    for (Bitu i=0;i<static_cast<Bitu>(cheight/banks);i++) {
		for (Bitu b=0;b<banks;b++) MEM_BlockCopy(dest+b*8*1024,src+b*8*1024,copy);
        dest+=nextline;src+=nextline;
    }
}

static inline uint8_t StartCopyBankSelect(PhysPt &src, PhysPt &dest)
{
	uint8_t select = 0;

	if(svgaCard == SVGA_TsengET4K || svgaCard == SVGA_S3Trio) {
		select = 0x00;
		if(src >= 0x20000) {
			select |= 0x20;
			src -= 0x20000;
		} else if(src >= 0x10000) {
			select |= 0x10;
			src -= 0x10000;
		}
		if(dest >= 0x20000) {
			select |= 0x02;
			dest -= 0x20000;
		} else if(dest >= 0x10000) {
			select |= 0x01;
			dest -= 0x10000;
		}
		if(svgaCard == SVGA_TsengET4K) {
			IO_Write(0x3cd, select);
		}
	}
	return select;
}

static inline uint8_t CheckCopyBankSelect(uint8_t select, PhysPt &src, PhysPt &dest, PhysPt &src_pt, PhysPt &dest_pt)
{
	if(svgaCard == SVGA_TsengET4K || svgaCard == SVGA_S3Trio) {
		if(src_pt >= 0x10000) {
			if((select & 0xf0) == 0x00) {
				select = (select & 0x0f) | 0x10;
			} else if((select & 0xf0) == 0x10) {
				select = (select & 0x0f) | 0x20;
			}
			src_pt -= 0x10000;
			src -= 0x10000;
			if(svgaCard == SVGA_TsengET4K) {
				IO_Write(0x3cd, select);
			}
		}
		if(dest_pt >= 0x10000) {
			if((select & 0x0f) == 0x00) {
				select = (select & 0xf0) | 0x01;
			} else if((select & 0x0f) == 0x01) {
				select = (select & 0xf0) | 0x02;
			}
			dest_pt -= 0x10000;
			dest -= 0x10000;
			if(svgaCard == SVGA_TsengET4K) {
				IO_Write(0x3cd, select);
			}
		}
	}
	return select;
}

static void EGA16_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	PhysPt src,dest;Bitu copy;
    BIOS_CHEIGHT;
    Bitu nextline=(Bitu)CurMode->twidth;
    /* Setup registers correctly */
    IO_Write(0x3ce,5);IO_Write(0x3cf,1);		/* Memory transfer mode */
    IO_Write(0x3c4,2);IO_Write(0x3c5,0xf);		/* Enable all Write planes */
    /* Do some copying */
    Bitu rowsize=(Bitu)(cright-cleft);
    if(svgaCard != SVGA_TsengET4K && svgaCard != SVGA_S3Trio) {
        dest=base+(CurMode->twidth*rnew)*cheight+cleft;
        src=base+(CurMode->twidth*rold)*cheight+cleft;
        copy=cheight;
        for (;copy>0;copy--) {
            for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,mem_readb(src+x));
            dest+=nextline;src+=nextline;
        }
    } else {
        uint8_t select, data;
        uint8_t src_bank = 0, dest_bank = 0, last_bank = 0xff;

        dest=(CurMode->twidth*rnew)*cheight+cleft;
        src=(CurMode->twidth*rold)*cheight+cleft;

        select = StartCopyBankSelect(src, dest);
        if(svgaCard == SVGA_S3Trio) {
            src_bank = (select >> 4) & 0x0f;
            dest_bank = select & 0x0f;
        }
        for(copy = 0 ; copy < cheight ; copy++) {
            PhysPt src_pt = src;
            PhysPt dest_pt = dest;
            for(Bitu x = 0 ; x < rowsize ; x++) {
                select = CheckCopyBankSelect(select, src, dest, src_pt, dest_pt);
                if(svgaCard == SVGA_S3Trio) {
                    src_bank = (select >> 4) & 0x0f;
                    dest_bank = select & 0x0f;
                }
                if(svgaCard == SVGA_S3Trio && src_bank != last_bank) {
                    IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, src_bank);
                    last_bank = src_bank;
                }
                data = mem_readb(base + src_pt);
                if(svgaCard == SVGA_S3Trio && dest_bank != last_bank) {
                    IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, dest_bank);
                    last_bank = dest_bank;
                }
                mem_writeb(base + dest_pt, data);
                src_pt++;
                dest_pt++;
            }
            dest += nextline;
            src += nextline;
        }
        IO_Write(0x3cd, 0);
    }
    /* Restore registers */
    IO_Write(0x3ce,5);IO_Write(0x3cf,0);		/* Normal transfer mode */
}

static uint8_t CopyRowMask(PhysPt base, PhysPt dest_pt, PhysPt src_pt, uint8_t mask, uint8_t src_bank, uint8_t dest_bank, uint8_t last_bank)
{
    uint8_t no;
    uint8_t plane[4];

    IO_Write(0x3ce, 5); IO_Write(0x3cf, 0);
    for(no = 0 ; no < 4 ; no++) {
        IO_Write(0x3ce, 4); IO_Write(0x3cf, no);
        if(svgaCard == SVGA_S3Trio && dest_bank != last_bank) {
            IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, dest_bank);
            last_bank = dest_bank;
        }
        plane[no] = mem_readb(base + dest_pt) & (mask ^ 0xff);
        if(svgaCard == SVGA_S3Trio && src_bank != last_bank) {
            IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, src_bank);
            last_bank = src_bank;
        }
        plane[no] |= mem_readb(base + src_pt) & mask;
    }
    IO_Write(0x3ce, 5); IO_Write(0x3cf, 8);
    IO_Write(0x3ce, 1); IO_Write(0x3cf, 0);
    IO_Write(0x3ce, 7); IO_Write(0x3cf, 0);
    IO_Write(0x3ce, 3); IO_Write(0x3cf, 0);
    IO_Write(0x3ce, 8); IO_Write(0x3cf, 0xff);
    for(no = 0 ; no < 4 ; no++) {
        IO_Write(0x3c4, 2); IO_Write(0x3c5, 1 << no);
        if(svgaCard == SVGA_S3Trio && dest_bank != last_bank) {
            IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, dest_bank);
            last_bank = dest_bank;
        }
        mem_writeb(base + dest_pt, plane[no]);
    }
    IO_Write(0x3ce, 5); IO_Write(0x3cf, 1);
    IO_Write(0x3c4, 2); IO_Write(0x3c5, 0xf);

    return last_bank;
}

static void EGA16_CopyRow_24(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    PhysPt src, dest;
    Bitu rowsize;
    Bitu start = (cleft * 12) / 8;
    Bitu width = (real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) == 85) ? 128 : 160;

    rowsize = (cright - cleft) * 12 / 8;
    if((cleft & 1) || (cright & 1)) {
        rowsize++;
    }
    dest = (width * rnew) * 24 + start;
    src = (width * rold) * 24 + start;
    IO_Write(0x3ce,5); IO_Write(0x3cf,1);
    IO_Write(0x3c4,2); IO_Write(0x3c5,0xf);

    uint8_t select, data;
	uint8_t src_bank = 0, dest_bank = 0, last_bank = 0xff;

    select = StartCopyBankSelect(src, dest);
    if(svgaCard == SVGA_S3Trio) {
        src_bank = (select >> 4) & 0x0f;
        dest_bank = select & 0x0f;
    }
    for(Bitu copy = 0 ; copy < 24 ; copy++) {
        PhysPt src_pt = src;
        PhysPt dest_pt = dest;

        for(Bitu x = 0 ; x < rowsize ; x++) {
            select = CheckCopyBankSelect(select, src, dest, src_pt, dest_pt);
            if(svgaCard == SVGA_S3Trio) {
                src_bank = (select >> 4) & 0x0f;
                dest_bank = select & 0x0f;
            }
            if(x == 0 && (cleft & 1)) {
                last_bank = CopyRowMask(base, dest_pt, src_pt, 0x0f, src_bank, dest_bank, last_bank);
            } else if(x == rowsize - 1 && (cright & 1)) {
                last_bank = CopyRowMask(base, dest_pt, src_pt, 0xf0, src_bank, dest_bank, last_bank);
            } else {
                if(svgaCard == SVGA_S3Trio && src_bank != last_bank) {
                    IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, src_bank);
                    last_bank = src_bank;
                }
                data = mem_readb(base + src_pt);
                if(svgaCard == SVGA_S3Trio && dest_bank != last_bank) {
                    IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, dest_bank);
                    last_bank = dest_bank;
                }
                mem_writeb(base + dest_pt, data);
            }
            src_pt++;
            dest_pt++;
        }
        dest += width;
        src += width;
    }
    IO_Write(0x3ce,5);IO_Write(0x3cf,0);
    IO_Write(0x3cd, 0);
}

static void VGA_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    PhysPt src,dest;Bitu copy;
    BIOS_CHEIGHT;
    dest=base+8u*((CurMode->twidth*rnew)*cheight+cleft);
    src=base+8u*((CurMode->twidth*rold)*cheight+cleft);
    Bitu nextline=8u*(Bitu)CurMode->twidth;
    Bitu rowsize=8u*(Bitu)(cright-cleft);
    copy=cheight;
    for (;copy>0;copy--) {
        for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,mem_readb(src+x));
        dest+=nextline;src+=nextline;
    }
}

static void TEXT_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base,uint8_t ncols) {
    PhysPt src,dest;
    src=base+(rold*ncols+cleft)*2u;
    dest=base+(rnew*ncols+cleft)*2u;
    MEM_BlockCopy(dest,src,(Bitu)(cright-cleft)*2u);
}

static void PC98_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
    PhysPt src,dest;

    /* character data */
    src=base+(rold*CurMode->twidth+cleft)*2;
    dest=base+(rnew*CurMode->twidth+cleft)*2;
    MEM_BlockCopy(dest,src,(Bitu)(cright-cleft)*2u);

    /* attribute data */
    MEM_BlockCopy(dest+0x2000,src+0x2000,(Bitu)(cright-cleft)*2u);
}

static void DCGA_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	uint8_t cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
	PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/4)+cleft);
	PhysPt src=base+((CurMode->twidth*rold)*(cheight/4)+cleft);
	Bitu copy=(cright-cleft);
	Bitu nextline=CurMode->twidth;
	for (Bitu i=0;i<cheight/4U;i++) {
		MEM_BlockCopy(dest,src,copy);
		MEM_BlockCopy(dest+8*1024,src+8*1024,copy);
		MEM_BlockCopy(dest+8*1024*2,src+8*1024*2,copy);
		MEM_BlockCopy(dest+8*1024*3,src+8*1024*3,copy);

		dest+=nextline;src+=nextline;
	}
}

static void DCGA_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	uint8_t cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
	PhysPt dest=base+((CurMode->twidth*row)*(cheight/4)+cleft);
	Bitu copy=(cright-cleft);
	Bitu nextline=CurMode->twidth;
	attr = 0x00;
	for (Bitu i=0;i<cheight/4U;i++) {
		for (Bitu x=0;x<copy;x++) {
			mem_writeb(dest+x,attr);
			mem_writeb(dest+8*1024+x,attr);
			mem_writeb(dest+8*1024*2+x,attr);
			mem_writeb(dest+8*1024*3+x,attr);
		}
		dest+=nextline;
	}
}

static void DOSV_Text_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew) {
	Bitu textcopy = (cright - cleft);
	Bitu dest = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 2 * rnew + cleft * 2;
	Bitu src = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 2 * rold + cleft * 2;
	uint16_t seg = GetTextSeg();
	for(Bitu x = 0 ; x < textcopy ; x++) {
		real_writeb(seg, dest + x * 2, real_readb(seg, src + x * 2));
		real_writeb(seg, dest + x * 2 + 1, real_readb(seg, src + x * 2 + 1));
	}
}

static void DOSV_Text_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,uint8_t attr) {
	Bitu textdest = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 2 * row + cleft * 2;
	Bitu textcopy = (cright - cleft);
	uint16_t seg = GetTextSeg();
	for (Bitu x = 0 ; x < textcopy ; x++) {
		real_writeb(seg, textdest + x * 2, 0x20);
		real_writeb(seg, textdest + x * 2 + 1, attr);
	}
}

static void MCGA2_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    BIOS_CHEIGHT;
    PhysPt dest=base+((CurMode->twidth*row)*cheight+cleft);
    Bitu copy=(Bitu)(cright-cleft);
    Bitu nextline=CurMode->twidth;
    attr=(attr & 0x3) | ((attr & 0x3) << 2) | ((attr & 0x3) << 4) | ((attr & 0x3) << 6);
    for (Bitu i=0;i<cheight;i++) {
        for (Bitu x=0;x<copy;x++) {
            mem_writeb(dest+x,attr);
        }
        dest+=nextline;
    }
}

static void CGA2_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    BIOS_CHEIGHT;
    PhysPt dest=base+((CurMode->twidth*row)*(cheight/2)+cleft);
    Bitu copy=(Bitu)(cright-cleft);
    Bitu nextline=CurMode->twidth;
    attr=(attr & 0x3) | ((attr & 0x3) << 2) | ((attr & 0x3) << 4) | ((attr & 0x3) << 6);
    for (Bitu i=0;i<cheight/2U;i++) {
        for (Bitu x=0;x<copy;x++) {
            mem_writeb(dest+x,attr);
            mem_writeb(dest+8*1024+x,attr);
        }
        dest+=nextline;
    }
}

static void CGA4_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    BIOS_CHEIGHT;
    PhysPt dest=base+((CurMode->twidth*row)*(cheight/2)+cleft)*2;
    Bitu copy=(Bitu)(cright-cleft)*2u;Bitu nextline=CurMode->twidth*2;
    attr=(attr & 0x3) | ((attr & 0x3) << 2) | ((attr & 0x3) << 4) | ((attr & 0x3) << 6);
    for (Bitu i=0;i<cheight/2U;i++) {
        for (Bitu x=0;x<copy;x++) {
            mem_writeb(dest+x,attr);
            mem_writeb(dest+8*1024+x,attr);
        }
        dest+=nextline;
    }
}

static void TANDY16_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    BIOS_CHEIGHT;
    uint8_t banks=CurMode->twidth/10;
    PhysPt dest=base+((CurMode->twidth*row)*(cheight/banks)+cleft)*4;
    Bitu copy=(Bitu)(cright-cleft)*4u;Bitu nextline=CurMode->twidth*4;
    attr=(attr & 0xf) | (attr & 0xf) << 4;
    for (Bitu i=0;i<static_cast<Bitu>(cheight/banks);i++) {	
        for (Bitu x=0;x<copy;x++) {
            for (Bitu b=0;b<banks;b++) mem_writeb(dest+b*8*1024+x,attr);
        }
        dest+=nextline;
    }
}

static uint8_t EGA4ColorRemap(const uint8_t a) {
	const uint8_t x = a & 0x5;
	return x | (x << 1u);
}

static void EGA16_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    /* EGA 640x350 4-color needs bits 0+1 and 2+3 ORed together as the IBM EGA BIOS does [https://ibmmuseum.com/Adapters/Video/EGA/IBM_EGA_Manual.pdf] */
    if (machine == MCH_EGA && vga.mem.memsize < (128*1024) && CurMode->sheight==350) attr=EGA4ColorRemap(attr);
    /* Set Bitmask / Color / Full Set Reset */
    IO_Write(0x3ce,0x8);IO_Write(0x3cf,0xff);
    IO_Write(0x3ce,0x0);IO_Write(0x3cf,attr);
    IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
    /* Enable all Write planes */
    IO_Write(0x3c4,2);IO_Write(0x3c5,0xf);
    /* Write some bytes */
    BIOS_CHEIGHT;
    PhysPt dest=base+(CurMode->twidth*row)*cheight+cleft;   
    Bitu nextline=CurMode->twidth;
    Bitu copy = cheight;Bitu rowsize=(Bitu)(cright-cleft);
    for (;copy>0;copy--) {
        for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,0xff);
        dest+=nextline;
    }
    IO_Write(0x3cf,0);
}

static void VGA_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    /* Write some bytes */
    BIOS_CHEIGHT;
    PhysPt dest=base+8*((CurMode->twidth*row)*cheight+cleft);
    Bitu nextline=8*CurMode->twidth;
    Bitu copy = cheight;Bitu rowsize=8u*(Bitu)(cright-cleft);
    for (;copy>0;copy--) {
        for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,attr);
        dest+=nextline;
    }
}

static void PC98_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
    /* Do some filing */
    PhysPt dest;
    dest=base+(row*CurMode->twidth+cleft)*2;
    uint16_t fill=' ';
    uint16_t fattr=attr ? attr : DefaultANSIAttr();
    for (uint8_t x=0;x<(Bitu)(cright-cleft);x++) {
        mem_writew(dest,fill);
        mem_writew(dest+0x2000,fattr);
        dest+=2;
    }
}

static void TEXT_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr,uint8_t ncols) {
    /* Do some filing */
    PhysPt dest;
    dest=base+(row*ncols+cleft)*2;
    uint16_t fill=(attr<<8)+' ';
    for (uint8_t x=0;x<(Bitu)(cright-cleft);x++) {
        mem_writew(dest,fill);
        dest+=2;
    }
}

#define _pushregs \
    uint16_t tmp_ax = reg_ax, tmp_bx = reg_bx, tmp_cx = reg_cx, tmp_dx = reg_dx;
#define _popregs \
    reg_ax = tmp_ax, reg_bx = tmp_bx, reg_cx = tmp_cx, reg_dx = tmp_dx;

void INT10_SetCursorPos_viaRealInt(uint8_t row, uint8_t col, uint8_t page) {
	_pushregs;
	reg_ah = 0x02;
	if (page == 0xFF) page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	reg_bh = page;
	reg_dl = col;
	reg_dh = row;
	CALLBACK_RunRealInt(0x10);
	_popregs;
}

void INT10_WriteChar_viaRealInt(uint8_t chr, uint8_t attr, uint8_t page, uint16_t count, bool showattr) {
	_pushregs;
	if (page == 0xFF) page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	reg_ah = showattr ? 0x09 : 0x0a;
	reg_al = chr;
	reg_bh = page;
	reg_bl = attr;
	reg_cx = count;
	CALLBACK_RunRealInt(0x10);
	_popregs;
}

void INT10_ScrollWindow_viaRealInt(uint8_t rul, uint8_t cul, uint8_t rlr, uint8_t clr, int8_t nlines, uint8_t attr, uint8_t page) {
    (void)page;//UNUSED
	BIOS_NCOLS;
	BIOS_NROWS;

	_pushregs;

	if (nrows == 256 || nrows == 1) nrows = 25;
	if (nlines > 0) {
		reg_ah = 0x07;
		reg_al = (uint8_t)nlines;
	}
	else {
		reg_ah = 0x06;
		reg_al = (uint8_t)(-nlines);
	}
	/* only works with active page */
	/* if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE); */

	if (clr >= ncols) clr = (uint8_t)(ncols - 1);
	if (rlr >= nrows) rlr = nrows - 1;

	reg_bh = attr;
	reg_cl = cul;
	reg_ch = rul;
	reg_dl = clr;
	reg_dh = rlr;
	CALLBACK_RunRealInt(0x10);

	_popregs;
}

uint8_t *GetSbcsFont(Bitu code);
uint8_t *GetSbcs19Font(Bitu code);
uint8_t *GetDbcsFont(Bitu code);

static uint8_t ExtendAttribute[160*64];

static bool IsExtendAttributeMode()
{
	uint8_t vmode = GetTrueVideoMode();
	return (vmode == 0x71 || vmode == 0x73);
}

void DOSV_FillScreen() {
    BIOS_NCOLS;BIOS_NROWS;
    for (int i=0; i<nrows; i++) DOSV_Text_FillRow(0,ncols,i,7);
    if(IsExtendAttributeMode()) memset(ExtendAttribute, 0, sizeof(ExtendAttribute));
}

static uint16_t font_net_data[2][16] = {
	{ 0x2449, 0x0000, 0x0000, 0x2449, 0x0000, 0x0000, 0x2449, 0x0000, 0x0000, 0x2449, 0x0000, 0x0000, 0x2449, 0x0000, 0x0000, 0x2449 },
	{ 0x0000, 0x4444, 0x0000, 0x0000, 0x0000, 0x4444, 0x0000, 0x0000, 0x0000, 0x4444, 0x0000, 0x0000, 0x0000, 0x4444, 0x0000, 0x0000 }
};

void WriteCharDCGASbcs(uint16_t col, uint16_t row, uint8_t chr, uint8_t attr)
{
	Bitu x, y, off, net, pos;
	uint8_t data;
	uint8_t *font = NULL;
	RealPt fontdata = 0;

	if(J3_IsJapanese() && chr >= 0x20) {
		font = GetSbcsFont(chr);
	} else {
		fontdata = RealGetVec(0x43);
		fontdata = RealMake(RealSeg(fontdata), RealOff(fontdata) + chr * 16);
	}
	net = ((attr & 0xf0) == 0xe0) ? 1 : 0;
	pos = 0;
	x = col;
	y = row * 16;
	off = (y >> 2) * 80 + 8 * 1024 * (y & 3) + x;
	for(uint8_t h = 0 ; h < 16 ; h++) {
		if(font && J3_IsJapanese() && chr >= 0x20) {
			data = *font++;
		} else {
			data = mem_readb(Real2Phys(fontdata++));
		}
		if((attr & 0x07) == 0x00) {
			data ^= 0xff;
		}
		if(attr & 0x80) {
			if(attr & 0x70) {
				data |= (font_net_data[net][pos++] & 0xff);
			} else {
				data ^= real_readb(0xb800, off);
			}
		}
		if(h == 15 && (attr & 0x08) == 0x08) {
			data = 0xff;
		}
		real_writeb(0xb800, off, data);
		off += 0x2000;
		if(off >= 0x8000) {
			off -= 0x8000;
			off += 80;
		}
	}
}

void WriteCharDCGADbcs(uint16_t col, uint16_t row, uint16_t chr, uint8_t attr)
{
	Bitu x, y, off, net, pos;
	uint16_t data;
	uint16_t *font;

	net = ((attr & 0xf0) == 0xe0) ? 1 : 0;
	pos = 0;
	font = (uint16_t *)GetDbcsFont(chr);
	x = col;
	y = row * 16;
	off = (y >> 2) * 80 + 8 * 1024 * (y & 3) + x;
	for(uint8_t h = 0 ; h < 16 ; h++) {
		data = *font++;
		if((attr & 0x07) == 0x00) {
			data ^= 0xffff;
		}
		if(attr & 0x80) {
			if(attr & 0x70) {
				data |= font_net_data[net][pos++];
			} else {
				data ^= real_readw(0xb800, off);
			}
		}
		if(h == 15 && (attr & 0x08) == 0x08) {
			data = 0xffff;
		}
		real_writew(0xb800, off, data);
		off += 0x2000;
		if(off >= 0x8000) {
			off -= 0x8000;
			off += 80;
		}
	}
}

uint8_t StartBankSelect(Bitu &off)
{
	uint8_t select = 0x00;

	if(svgaCard == SVGA_TsengET4K || svgaCard == SVGA_S3Trio) {
		if(off >= 0x20000) {
			select = 0x22;
			off -= 0x20000;
		} else if(off >= 0x10000) {
			select = 0x11;
			off -= 0x10000;
		}
		if(svgaCard == SVGA_TsengET4K) {
			IO_Write(0x3cd, select);
		} else {
			IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, select & 0x0f);
		}
	}
	return select;
}

uint8_t CheckBankSelect(uint8_t select, Bitu &off)
{
	if(svgaCard == SVGA_TsengET4K || svgaCard == SVGA_S3Trio) {
		if(off >= 0x10000) {
			if(select == 0x00) {
				select = 0x11;
			} else if(select == 0x11) {
				select = 0x22;
			}
			off -= 0x10000;
			if(svgaCard == SVGA_TsengET4K) {
				IO_Write(0x3cd, select);
			} else {
				IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, select & 0x0f);
			}
		}
	}
	return select;
}

void WriteCharDOSVSbcs24(uint8_t col, uint8_t row, uint8_t chr, uint8_t attr)
{
	uint8_t back, select;
	uint8_t *font;
	Bitu off;
	Bitu width = (real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) == 85) ? 128 : 160;
	volatile uint8_t dummy;

	font = GetSbcs24Font(chr);

	back = attr >> 4;
	attr &= 0x0f;

	off = row * width * 24 + (col * 12) / 8;
	select = StartBankSelect(off);

	IO_Write(0x3ce, 5); IO_Write(0x3cf, 0);
	IO_Write(0x3ce, 1); IO_Write(0x3cf, 0xf);

	uint8_t data[2];
	uint8_t mask_data[2][2] = {{ 0xff, 0xf0 }, { 0x0f, 0xff }};
	for(uint8_t y = 0 ; y < 24 ; y++) {
		if(col & 1) {
			data[0] = *font >> 4;
			data[1] = (*font << 4) | (*(font + 1) >> 4);
			font += 2;
		} else {
			data[0] = *font++;
			data[1] = *font++;
		}
		for(uint8_t x = 0 ; x < 2 ; x++) {
			IO_Write(0x3ce, 8); IO_Write(0x3cf, data[x]);
			IO_Write(0x3ce, 0); IO_Write(0x3cf, attr);
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0xff);

			IO_Write(0x3ce, 8); IO_Write(0x3cf, data[x] ^ mask_data[col & 1][x]);
			IO_Write(0x3ce, 0); IO_Write(0x3cf, back);
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0xff);

			off++;
			select = CheckBankSelect(select, off);
		}
		off += width - 2;
		select = CheckBankSelect(select, off);
	}
	IO_Write(0x3ce, 8); IO_Write(0x3cf, 0xff);
	IO_Write(0x3ce, 1); IO_Write(0x3cf, 0);
	IO_Write(0x3cd, 0x00);
}

void WriteCharDOSVDbcs24(uint16_t col, uint16_t row, uint16_t chr, uint8_t attr)
{
	uint8_t back, select;
	uint8_t *font;
	Bitu off;
	Bitu width = (real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) == 85) ? 128 : 160;
	volatile uint8_t dummy;

	font = GetDbcs24Font(chr);

	back = attr >> 4;
	attr &= 0x0f;

	off = row * width * 24 + (col * 12) / 8;
	select = StartBankSelect(off);

	IO_Write(0x3ce, 5); IO_Write(0x3cf, 0);
	IO_Write(0x3ce, 1); IO_Write(0x3cf, 0xf);

	uint8_t data[4];
	uint8_t mask_data[2][4] = {{ 0xff, 0xff, 0xff, 0x00 }, { 0x0f, 0xff, 0xff, 0xf0 }};
	for(uint8_t y = 0 ; y < 24 ; y++) {
		if(col & 1) {
			data[0] = *font >> 4;
			data[1] = (*font << 4) | (*(font + 1) >> 4);
			data[2] = (*(font + 1) << 4) | (*(font + 2) >> 4);
			data[3] = *(font + 2) << 4;
			font += 3;
		} else {
			data[0] = *font++;
			data[1] = *font++;
			data[2] = *font++;
		}
		for(uint8_t x = 0 ; x < 4 ; x++) {
			if(mask_data[col & 1][x] != 0) {
				IO_Write(0x3ce, 8); IO_Write(0x3cf, data[x]);
				IO_Write(0x3ce, 0); IO_Write(0x3cf, attr);
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);

				IO_Write(0x3ce, 8); IO_Write(0x3cf, data[x] ^ mask_data[col & 1][x]);
				IO_Write(0x3ce, 0); IO_Write(0x3cf, back);
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
			}
			off++;
			select = CheckBankSelect(select, off);
		}
		off += width - 4;
		select = CheckBankSelect(select, off);
	}
	IO_Write(0x3ce, 8); IO_Write(0x3cf, 0xff);
	IO_Write(0x3ce, 1); IO_Write(0x3cf, 0);
	IO_Write(0x3cd, 0x00);
}

void WriteCharDOSVSbcs(uint16_t col, uint16_t row, uint8_t chr, uint8_t attr) {
	Bitu off;
	uint8_t data, select = 0;
	uint8_t *font;

	if(IS_DOSV && real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT) == 24) {
		WriteCharDOSVSbcs24(col, row, chr, attr);
		return;
	}

	if(real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) == 0x72) {
		if(attr & 0x80) {
			IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
			IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr & 0x0f);
			IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x18);

			volatile uint8_t dummy;
			Bitu width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
			uint8_t height = real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
			font = height == 16 ? GetSbcsFont(chr) : GetSbcs19Font(chr);
			off = row * width * height + col;
			select = StartBankSelect(off);
			for(uint8_t h = 0 ; h < height ; h++) {
				dummy = real_readb(0xa000, off);
				data = *font++;
				real_writeb(0xa000, off, data);
				off += width;
				select = CheckBankSelect(select, off);
			}
			IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
			return;
		}
		attr &= 0x0f;
	}
	volatile uint8_t dummy;
	Bitu width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	uint8_t height = real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
	font = height == 16 ? GetSbcsFont(chr) : GetSbcs19Font(chr);
	off = row * width * height + col;
	select = StartBankSelect(off);

	IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
	IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr >> 4);
	real_writeb(0xa000, off, 0xff); dummy = real_readb(0xa000, off);
	IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr & 0x0f);
	for(uint8_t h = 0 ; h < height ; h++) {
		data = *font++;
		real_writeb(0xa000, off, data);
		off += width;
		select = CheckBankSelect(select, off);
	}
}

static void DrawCharDOSVDbcsHalf(Bitu off, uint8_t *font, uint8_t attr, Bitu width, uint8_t height, uint8_t select, bool xor_flag)
{
	volatile uint8_t dummy;
	uint8_t data;
	if(xor_flag) {
		IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
		IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr & 0x0f);
		IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x18);
	} else {
		IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
		IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr >> 4);
		real_writeb(0xa000, off, 0xff); dummy = real_readb(0xa000, off);
		IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr & 0x0f);
	}
	for(uint8_t h = 0 ; h < height ; h++) {
		if(xor_flag) {
			dummy = real_readb(0xa000, off);
		}
		if(height == 19 && (h == 0 || h > 16)) {
			data = 0;
		} else {
			data = *font;
			font += 2;
		}
		real_writeb(0xa000, off, data);
		off += width;
		select = CheckBankSelect(select, off);
	}
	if(xor_flag) {
		IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
	}
}

static inline void DrawCharDOSVDbcs(Bitu off, uint16_t *font, uint8_t attr, Bitu width, uint8_t height, uint8_t select)
{
	volatile uint16_t dummy;
	uint16_t data;

	IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
	IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr >> 4);
	real_writew(0xa000, off, 0xffff); dummy = real_readw(0xa000, off);
	IO_Write(0x3ce, 0x00); IO_Write(0x3cf, attr & 0x0f);

	for(uint8_t h = 0 ; h < height ; h++) {
		if(height == 19 && (h == 0 || h > 16)) {
			data = 0;
		} else {
			data = *font++;
		}
		real_writew(0xa000, off, data);
		off += width;
		select = CheckBankSelect(select, off);
	}
}

void WriteCharDOSVDbcs(uint16_t col, uint16_t row, uint16_t chr, uint8_t attr) {
	if(IS_DOSV && real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT) == 24) {
		WriteCharDOSVDbcs24(col, row, chr, attr);
		return;
	}

	if (IS_DOSV && !IS_JDOSV && col == pcol+2 && row == prow && CheckBoxDrawing(pchr / 0x100, pchr & 0xFF, chr / 0x100, chr & 0xFF)) {
		WriteCharDOSVSbcs(col - 2, row, pchr / 0x100, attr);
		WriteCharDOSVSbcs(col - 1, row, pchr & 0xFF, attr);
		WriteCharDOSVSbcs(col, row, chr / 0x100, attr);
		WriteCharDOSVSbcs(col + 1, row, chr & 0xFF, attr);
		pcol = col;prow = row;pchr = chr;
		return;
	}
	Bitu width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	uint8_t height = real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
	if(col == 0xffff) {
		col = width - 1;
		row--;
	}
	uint8_t *font = GetDbcsFont(chr);
	Bitu off = row * width * height + col;
	uint8_t select = StartBankSelect(off);
	if(real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) == 0x72) {
		if(attr & 0x80) {
			DrawCharDOSVDbcsHalf(off, font, prevattr, width, height, select, true);
			if(col != width - 1) {
				off++;
				select = CheckBankSelect(select, off);
				DrawCharDOSVDbcsHalf(off, font + 1, attr, width, height, select, true);
			}
			return;
		}
	}
	if(attr != prevattr || col == width - 1) {
		DrawCharDOSVDbcsHalf(off, font, prevattr, width, height, select, false);
		if(col == width - 1) {
			off = (row + 1) * width * height;
		} else {
			off++;
		}
		select = CheckBankSelect(select, off);
		DrawCharDOSVDbcsHalf(off, font + 1, attr, width, height, select, false);
	} else {
		if(real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) == 0x72) {
			attr &= 0x0f;
		}
		DrawCharDOSVDbcs(off, (uint16_t *)font, attr, width, height, select);
	}
	pcol = col;prow = row;pchr = chr;
}

void WriteCharTopView(uint16_t off, int count) {
	uint16_t seg = GetTextSeg();
	uint8_t code, attr;
	uint16_t col, row;
	uint16_t width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	col = (off / 2) % width;
	row = (off / 2) / width;
	while (count > 0) {
		code = real_readb(seg, off);
		attr = real_readb(seg, off + 1);
		if (isKanji1(code) && isKanji2(real_readb(seg, off + 2)) && (IS_JDOSV || dos.loaded_codepage == 932 || col < width - 1)) {
			off += 2;
			if (IS_J3100 && J3_IsJapanese())
				WriteCharDCGADbcs(col, row, ((uint16_t)code << 8) | real_readb(seg, off), attr);
			else {
				prevattr = attr;
				WriteCharDOSVDbcs(col, row, ((uint16_t)code << 8) | real_readb(seg, off), attr);
			}
			count--;
			col++;
			if (col >= width) {
				col = 0;
				row++;
			}
		} else {
			if (IS_J3100 && J3_IsJapanese())
				WriteCharDCGASbcs(col, row, code, attr);
			else
				WriteCharDOSVSbcs(col, row, code, attr);
		}
		col++;
		if (col >= width) {
			col = 0;
			row++;
		}
		off += 2;
		count--;
	}
}

void INT10_ScrollWindow(uint8_t rul,uint8_t cul,uint8_t rlr,uint8_t clr,int8_t nlines,uint8_t attr,uint8_t page) {
    /* Do some range checking */
    if(IS_DOSV && DOSV_CheckCJKVideoMode()) DOSV_OffCursor();
    else if(J3_IsJapanese()) J3_OffCursor();
    if (CurMode->type!=M_TEXT) page=0xff;
    BIOS_NCOLS;BIOS_NROWS;
    if(rul>rlr) return;
    if(cul>clr) return;
    if(rlr>=nrows) rlr=(uint8_t)nrows-1;
    if(clr>=ncols) clr=(uint8_t)ncols-1;
    clr++;

    /* Get the correct page: current start address for current page (0xFF),
       otherwise calculate from page number and page size */
    PhysPt base=CurMode->pstart;
    if (page==0xff) base+=real_readw(BIOSMEM_SEG,BIOSMEM_CURRENT_START);
    else base+=(unsigned int)page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
    
    if (GCC_UNLIKELY(machine==MCH_PCJR)) {
        if (real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) >= 9) {
            // PCJr cannot handle these modes at 0xb800
            // See INT10_PutPixel M_TANDY16
            Bitu cpupage =
                (real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;

            base = cpupage << 14;
            if (page!=0xff)
                base += (unsigned int)page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
        }
    }

    /* See how much lines need to be copied */
    uint8_t start,end;Bits next;
    /* Copy some lines */
    if (nlines>0) {
        start=rlr-nlines+1;
        end=rul;
        next=-1;
    } else if (nlines<0) {
        start=rul-nlines-1;
        end=rlr;
        next=1;
    } else {
        nlines=rlr-rul+1;
        goto filling;
    }
    while (start!=end) {
        start+=next;
        switch (CurMode->type) {
        case M_PC98:
            PC98_CopyRow(cul,clr,start,start+nlines,base);break;
        case M_TEXT:
            TEXT_CopyRow(cul,clr,start,start+nlines,base,ncols);break;
        case M_DCGA:
            DCGA_CopyRow(cul,clr,start,start+nlines,base);
            if(J3_IsJapanese()) {
                DOSV_Text_CopyRow(cul,clr,start,start+nlines);
            }
            break;
        case M_CGA2:
            if (machine == MCH_MCGA && CurMode->mode == 0x11)
                MCGA2_CopyRow(cul,clr,start,start+nlines,base);
            else
                CGA2_CopyRow(cul,clr,start,start+nlines,base);
            break;
        case M_CGA4:
            CGA4_CopyRow(cul,clr,start,start+nlines,base);break;
        case M_TANDY16:
            TANDY16_CopyRow(cul,clr,start,start+nlines,base);break;
        case M_EGA:
            EGA16_CopyRow(cul,clr,start,start+nlines,base);
            if (IS_DOSV && DOSV_CheckCJKVideoMode()) DOSV_Text_CopyRow(cul,clr,start,start+nlines);
            break;
        case M_VGA:
            VGA_CopyRow(cul,clr,start,start+nlines,base);break;
        case M_LIN4:
            if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
                if(real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT) == 24) {
                    EGA16_CopyRow_24(cul,clr,start,start+nlines,base);
                } else {
                    EGA16_CopyRow(cul,clr,start,start+nlines,base);
                }
                DOSV_Text_CopyRow(cul,clr,start,start+nlines);
                break;
            }
            if ((machine==MCH_VGA) && (svgaCard==SVGA_TsengET4K) && (CurMode->swidth<=800)) {
                // the ET4000 BIOS supports text output in 800x600 SVGA
                EGA16_CopyRow(cul,clr,start,start+nlines,base);
                break;
            }
            // fall-through
        default:
            LOG(LOG_INT10,LOG_ERROR)("Unhandled mode %d for scroll",CurMode->type);
        }   
    }
    /* Fill some lines */
filling:
    if (nlines>0) {
        start=rul;
    } else {
        nlines=-nlines;
        start=rlr-nlines+1;
    }
    for (;nlines>0;nlines--) {
        switch (CurMode->type) {
        case M_PC98:
            PC98_FillRow(cul,clr,start,base,attr);break;
        case M_TEXT:
            TEXT_FillRow(cul,clr,start,base,attr,ncols);break;
        case M_DCGA:
            DCGA_FillRow(cul,clr,start,base,attr);
            if(J3_IsJapanese()) {
                DOSV_Text_FillRow(cul,clr,start,attr);
            }
            break;
        case M_CGA2:
            if (machine == MCH_MCGA && CurMode->mode == 0x11)
                MCGA2_FillRow(cul,clr,start,base,attr);
            else
                CGA2_FillRow(cul,clr,start,base,attr);
            break;
        case M_CGA4:
            CGA4_FillRow(cul,clr,start,base,attr);break;
        case M_TANDY16:     
            TANDY16_FillRow(cul,clr,start,base,attr);break;
        case M_EGA:
            if (IS_DOSV && DOSV_CheckCJKVideoMode()) {
                DOSV_Text_FillRow(cul,clr,start,attr);
                WriteCharTopView(real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 2 * start + cul * 2, clr - cul);
            } else
                EGA16_FillRow(cul,clr,start,base,attr);
            break;
        case M_VGA:
            VGA_FillRow(cul,clr,start,base,attr);break;
        case M_LIN4:
            if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
                DOSV_Text_FillRow(cul,clr,start,attr);
                WriteCharTopView(real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 2 * start + cul * 2, clr - cul);
                break;
            }
            if ((machine==MCH_VGA) && (svgaCard==SVGA_TsengET4K) && (CurMode->swidth<=800)) {
                EGA16_FillRow(cul,clr,start,base,attr);
                break;
            }
            // fall-through
        default:
            LOG(LOG_INT10,LOG_ERROR)("Unhandled mode %d for scroll",CurMode->type);
        }   
        start++;
    }
}

void INT10_SetActivePage(uint8_t page) {
    uint16_t mem_address;
    if (page>7) LOG(LOG_INT10,LOG_ERROR)("INT10_SetActivePage page %d",page);

    if (IS_EGAVGA_ARCH && (svgaCard==SVGA_S3Trio)) page &= 7;

    mem_address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
    /* Write the new page start */
    real_writew(BIOSMEM_SEG,BIOSMEM_CURRENT_START,mem_address);
    if (IS_EGAVGA_ARCH) {
        if (CurMode->mode<8) mem_address>>=1;
        // rare alternative: if (CurMode->type==M_TEXT)  mem_address>>=1;
    } else {
        mem_address>>=1;
    }
    /* Write the new start address in vgahardware */
    uint16_t base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
    IO_Write(base,0x0cu);
    IO_Write(base+1u,(uint8_t)(mem_address>>8u));
    IO_Write(base,0x0du);
    IO_Write(base+1u,(uint8_t)mem_address);

    // And change the BIOS page
    real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,page);
    uint8_t cur_row=CURSOR_POS_ROW(page);
    uint8_t cur_col=CURSOR_POS_COL(page);
    // Display the cursor, now the page is active
    INT10_SetCursorPos(cur_row,cur_col,page);
}

extern const char* RunningProgram;
void INT10_SetCursorShape(uint8_t first,uint8_t last) {
    real_writew(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE,last|(first<<8u));
    if (machine==MCH_CGA || IS_TANDY_ARCH) goto dowrite;
    /* Skip CGA cursor emulation if EGA/VGA system is active */
    if (machine==MCH_HERC || !(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0x8)) { /* if video subsystem is ACTIVE (bit is cleared) [https://www.stanislavs.org/helppc/bios_data_area.html] */
        /* Check for CGA type 01, invisible */
        if ((first & 0x60) == 0x20) {
            first=0x1e | 0x20; /* keep the cursor invisible! */
            last=0x00;
            goto dowrite;
        }
        /* Check if we need to convert CGA Bios cursor values */
        /* FIXME: Some sources including [https://www.stanislavs.org/helppc/bios_data_area.html] [https://www.matrix-bios.nl/system/bda.html]
         *        suggest CGA alphanumeric cursor emulation occurs when bit 0 is SET. This code checks whether the bit is CLEARED.
         *        This test and emulation may have the bit backwards. VERIFY ON REAL HARDWARE -- J.C */
        if (machine==MCH_HERC || !(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0x1)) { // set by int10 fun12 sub34
//          if (CurMode->mode>0x3) goto dowrite;    //Only mode 0-3 are text modes on cga
            if ((first & 0xe0) || (last & 0xe0)) goto dowrite;
            uint8_t cheight=((machine==MCH_HERC)?14:real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT))-1;
            /* Creative routine i based of the original ibmvga bios */

            if (last<first) {
                if (!last) goto dowrite;
                first=last;
                last=cheight;
            /* Test if this might be a cga style cursor set, if not don't do anything */
            } else if (((first | last)>=cheight) || !(last==(cheight-1)) || !(first==cheight) ) {
                if (last<=3) goto dowrite;
                if (first+2<last) {
                    if (first>2) {
                        first=(cheight+1)/2;
                        last=cheight;
                    } else {
                        last=cheight;
                    }
                } else {
                    first=(first-last)+cheight;
                    last=cheight;

                    if (cheight>0xc) { // vgatest sets 15 15 2x where only one should be decremented to 14 14
                        first--;     // implementing int10 fun12 sub34 fixed this.
                        last--;
                    }
                }
            }

        }
    }
dowrite:
    uint16_t base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
    IO_Write(base,0xa);IO_Write(base+1u,first);
    IO_Write(base,0xb);IO_Write(base+1u,last);
}

void vga_pc98_direct_cursor_pos(uint16_t address);

void INT10_GetScreenColumns(uint16_t *cols)
{
    if (IS_PC98_ARCH)
        *cols = 80; //TODO
    else
        *cols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
}

void INT10_GetCursorPos(uint8_t *row, uint8_t*col, const uint8_t page)
{
    if (IS_PC98_ARCH) {
        *col = real_readb(0x60, 0x11C);
        *row = real_readb(0x60, 0x110);
    }
    else {
        *col = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + page * 2u);
        *row = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + page * 2u + 1u);
    }
}

void INT10_SetCursorPos(uint8_t row,uint8_t col,uint8_t page) {
    // Get the dimensions
    BIOS_NCOLS;
    BIOS_NROWS;

    // EGA/VGA: Emulate a VGA BIOS that range-checks at least the row.
    //          The DOS installer for Elder Scrolla Arena is obviously trying to put something on the last row of
    //          the display, however it sets the cursor position to row=25. row is 0-based so that would actually put
    //          it one row beyond the end of the display. This fixes the "Press F1 for"... message at the bottom.
    // Assume MDA/CGA do not range check because the rows value in the BDA did not exist in the original IBM PC/XT BIOS.
    if (IS_EGAVGA_ARCH && !IS_PC98_ARCH) {
        if (nrows && row >= nrows)
		row = nrows - 1;
    }

    if (IS_DOSV && DOSV_CheckCJKVideoMode()) DOSV_OffCursor();
    else if(J3_IsJapanese()) J3_OffCursor();
    if (page>7) LOG(LOG_INT10,LOG_ERROR)("INT10_SetCursorPos page %d",page);
    // Bios cursor pos
    if (IS_PC98_ARCH) {
        real_writeb(0x60,0x11C,col);
        real_writeb(0x60,0x110,row);
        page = 0;
    }
    else {
        real_writeb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2u,col);
        real_writeb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2u+1u,row);
    }
    // Set the hardware cursor
    uint8_t current=IS_PC98_ARCH ? 0 : real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
    if(page==current) {
        // Calculate the address knowing nbcols nbrows and page num
        // NOTE: BIOSMEM_CURRENT_START counts in colour/flag pairs
        uint16_t address=(ncols*row)+col+(IS_PC98_ARCH ? 0 : (real_readw(BIOSMEM_SEG,BIOSMEM_CURRENT_START)/2));
        if (IS_PC98_ARCH) {
            vga_pc98_direct_cursor_pos(address);
        }
        else {
            // CRTC regs 0x0e and 0x0f
            uint16_t base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
            IO_Write(base,0x0eu);
            IO_Write(base+1u,(uint8_t)(address>>8u));
            IO_Write(base,0x0fu);
            IO_Write(base+1u,(uint8_t)address);
        }
    }
}

void ReadCharAttr(uint16_t col,uint16_t row,uint8_t page,uint16_t * result) {
    /* Externally used by the mouse routine */
    RealPt fontdata;
    uint16_t cols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    BIOS_CHEIGHT;
    bool split_chr = false;
    switch (CurMode->type) {
    case M_TEXT:
        {   
            // Compute the address  
            uint16_t address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
            address+=(row*cols+col)*2;
            // read the char 
            PhysPt where = CurMode->pstart+address;
            *result=mem_readw(where);
        }
        return;
    case M_CGA4:
    case M_CGA2:
    case M_DCGA:
    case M_TANDY16:
        split_chr = true;
        switch (machine) {
        case MCH_CGA:
        case MCH_HERC:
            fontdata=RealMake(0xf000,0xfa6e);
            break;
        case TANDY_ARCH_CASE:
            fontdata=RealGetVec(0x44);
            break;
        default:
            fontdata=RealGetVec(0x43);
            break;
        }
        break;
    default:
		if (isJEGAEnabled()) {
			if (real_readw(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR) != 0) {
				ReadVTRAMChar(col, row, result);
				return;
			}
		} else if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
			*result = real_readw(GetTextSeg(), (row * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) + col) * 2);
			return;
		}
        fontdata=RealGetVec(0x43);
        break;
    }

    Bitu x=col*8u,y=(unsigned int)row*cheight*(cols/CurMode->twidth);

    for (uint16_t chr=0;chr<256;chr++) {

        if (chr==128 && split_chr) fontdata=RealGetVec(0x1f);

        bool error=false;
        uint16_t ty=(uint16_t)y;
        for (uint8_t h=0;h<cheight;h++) {
            uint8_t bitsel=128;
            uint8_t bitline=mem_readb(Real2Phys(fontdata));
            fontdata=RealMake(RealSeg(fontdata),RealOff(fontdata)+1);
            uint8_t res=0;
            uint8_t vidline=0;
            uint16_t tx=(uint16_t)x;
            while (bitsel) {
                //Construct bitline in memory
                INT10_GetPixel(tx,ty,page,&res);
                if(res) vidline|=bitsel;
                tx++;
                bitsel>>=1;
            }
            ty++;
            if(bitline != vidline){
                /* It's not character 'chr', move on to the next */
                fontdata=RealMake(RealSeg(fontdata),RealOff(fontdata)+cheight-h-1);
                error = true;
                break;
            }
        }
        if(!error){
            /* We found it */
            *result = chr;
            return;
        }
    }
    LOG(LOG_INT10,LOG_ERROR)("ReadChar didn't find character");
    *result = 0;
}

void INT10_ReadCharAttr(uint16_t * result,uint8_t page) {
    if(CurMode->ptotal==1) page=0;
    if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
    uint8_t cur_row=CURSOR_POS_ROW(page);
    uint8_t cur_col=CURSOR_POS_COL(page);
    ReadCharAttr(cur_col,cur_row,page,result);
}

void INT10_ReadString(uint8_t row, uint8_t col, uint8_t flag, uint8_t attr, PhysPt string, uint16_t count,uint8_t page) {
    (void)flag;//UNUSED
    (void)attr;//UNUSED
	uint16_t result;
	while (count > 0) {
		ReadCharAttr(col, row, page, &result);
		mem_writew(string, result);
		string += 2;
		if(flag == 0x11) {
			mem_writeb(string, IsExtendAttributeMode() ? ExtendAttribute[row * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) + col] : 0);
			string++;
			mem_writeb(string, 0);
			string++;
		}
		col++;
		if(col == real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)) {
			col = 0;
			if(row == real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1) {
				break;
			}
			row++;
		}
		count--;
	}
}

void INT10_PC98_CurMode_Relocate(void) {
    /* deprecated */
}

/* Draw DBCS char in graphics mode*/
void WriteCharJ(uint16_t col, uint16_t row, uint8_t page, uint8_t chr, uint8_t attr, bool useattr)
{
	Bitu x, y, pos = row*real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) + col;
	uint8_t back, cheight = real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
	uint16_t sjischr = prevchr;
	sjischr <<= 8;
	sjischr |= chr;

	if (GCC_UNLIKELY(!useattr)) { //Set attribute(color) to a sensible value
		static bool warned_use = false;
		if (GCC_UNLIKELY(!warned_use)) {
			LOG(LOG_INT10, LOG_ERROR)("writechar used without attribute in non-textmode %c %X", chr, chr);
			warned_use = true;
		}
		attr = 0xf;
	}

	//Attribute behavior of mode 6; mode 11 does something similar but
	//it is in INT 10h handler because it only applies to function 09h
	if (CurMode->mode == 0x06) attr = (attr & 0x80) | 1;

	switch (CurMode->type) {
	case M_VGA:
	case M_EGA:
		/* enable all planes for EGA modes (Ultima 1 colour bug) */
		/* might be put into INT10_PutPixel but different vga bios
		implementations have different opinions about this */
		IO_Write(0x3c4, 0x2); IO_Write(0x3c5, 0xf);
		// fall-through
	default:
		back = attr & 0x80;
		break;
	}

	x = (pos%CurMode->twidth - 1) * 8;//move right 1 column.
	y = (pos / CurMode->twidth)*cheight;

	uint16_t ty = (uint16_t)y;
	uint8_t *font = GetDbcsFont(sjischr);
	for (uint8_t h = 0; h<16 ; h++) {
		uint16_t bitsel = 0x8000;
		uint16_t bitline = font[h * 2];
		bitline <<= 8;
		bitline |= font[h * 2 + 1];
		uint16_t tx = (uint16_t)x;
		while (bitsel) {
			INT10_PutPixel(tx, ty, page, (bitline&bitsel) ? attr : back);
			tx++;
			bitsel >>= 1;
		}
		ty++;
	}
}

/* Read char code and attribute from Virtual Text RAM (for AX)*/
void ReadVTRAMChar(uint16_t col, uint16_t row, uint16_t * result) {
	Bitu addr = real_readw(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR);
	addr <<= 4;
	addr += 2 * (80 * row + col);
	*result = mem_readw(addr);
}

/* Write char code and attribute into Virtual Text RAM (for AX)*/
void SetVTRAMChar(uint16_t col, uint16_t row, uint8_t chr, uint8_t attr)
{
	Bitu addr = real_readw(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR);
	addr <<= 4;
	addr += 2 * (80 * row + col);
	mem_writeb(addr,chr);
	mem_writeb(addr+1, attr);
}

void WriteCharV(uint16_t col,uint16_t row,uint8_t chr,uint8_t attr,bool useattr)
{
    DOSV_OffCursor();

    uint16_t seg = GetTextSeg();
    uint16_t width = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    real_writeb(seg, row * width * 2 + col * 2, chr);
    if(useattr) {
        real_writeb(seg, row * width * 2 + col * 2 + 1, attr);
    }
    if (isKanji1(chr) && prevchr == 0) {
        prevchr = chr;
        prevattr = attr;
    } else if (isKanji2(chr) && prevchr != 0) {
        WriteCharDOSVDbcs(col - 1, row, (prevchr << 8) | chr, attr);
        prevchr = 0;
    } else {
        WriteCharDOSVSbcs(col, row, chr, attr);
    }
    return;
}

void WriteChar(uint16_t col,uint16_t row,uint8_t page,uint16_t chr,uint8_t attr,bool useattr) {
    /* Externally used by the mouse routine */
    PhysPt fontdata;
    uint16_t cols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    uint8_t back, cheight = IS_PC98_ARCH ? 16 : (machine == MCH_CGA || machine == MCH_AMSTRAD) ? 8 : real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);

    if (CurMode->type != M_PC98)
        chr &= 0xFF;

    switch (CurMode->type) {
    case M_TEXT:
        {   
            // Compute the address  
            uint16_t address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
            address+=(row*cols+col)*2;
            // Write the char 
            PhysPt where = CurMode->pstart+address;
            mem_writeb(where,chr);
            if (useattr) mem_writeb(where+1,attr);
        }
        return;
    case M_PC98:
        {
            // Compute the address  
            uint16_t address=((row*80)+col)*2;
            // Write the char 
            PhysPt where = CurMode->pstart+address;
            mem_writew(where,chr);
            if (useattr) {
                mem_writeb(where+0x2000,attr);
            }

            // some chars are double-wide and need to fill both cells.
            // however xx08h-xx0Bh are single-wide encodings such as the
            // single-wide box characters used by DOSBox-X's introductory
            // message.
            if ((chr & 0xFF00u) != 0u && (chr & 0xFCu) != 0x08u) {
                where += 2u;
                mem_writew(where,chr);
                if (useattr) {
                    mem_writeb(where+0x2000,attr);
                }
            }
        }
        return;
    case M_DCGA:
        {
            if(J3_IsJapanese()) {
                J3_OffCursor();
                real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_MODE, real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_MODE) | 0x20);
                uint16_t seg = GetTextSeg();
                real_writeb(seg, row * 80 * 2 + col * 2, chr);
                if(useattr) {
                    real_writeb(seg, row * 80 * 2 + col * 2 + 1, attr);
                }
                if (isKanji1(chr) && prevchr == 0) {
                    prevchr = chr;
                } else if (isKanji2(chr) && prevchr != 0) {
                    WriteCharDCGADbcs(col - 1, row, (prevchr << 8) | chr, attr);
                    prevchr = 0;
                    return;
                }
            }
            WriteCharDCGASbcs(col, row, chr, attr);
            if(J3_IsJapanese()) {
                real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_MODE, real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_MODE) & 0xdf);
            }
        }
        return;
    case M_CGA4:
    case M_CGA2:
    case M_TANDY16:
        if (chr>=128) {
            chr-=128;
            fontdata=RealGetVec(0x1f);
            break;
        }
        switch (machine) {
        case MCH_CGA:
        case MCH_HERC:
            fontdata=RealMake(0xf000,0xfa6e);
            break;
        case TANDY_ARCH_CASE:
            fontdata=RealGetVec(0x44);
            break;
        default:
            fontdata=RealGetVec(0x43);
            break;
        }
        break;
    default:
		if (isJEGAEnabled()) {
			if (real_readw(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR) != 0)
				SetVTRAMChar(col, row, chr, attr);
			if (isKanji1(chr) && prevchr == 0)
				prevchr = chr;
			else if (isKanji2(chr) && prevchr != 0) {
				WriteCharJ(col, row, page, chr, attr, useattr);
				prevchr = 0;
				return;
			}
		} else if (IS_DOSV && DOSV_CheckCJKVideoMode()) {
			DOSV_OffCursor();
			uint16_t seg = GetTextSeg();
			uint16_t width = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
			real_writeb(seg, row * width * 2 + col * 2, chr);
			if (useattr)
				real_writeb(seg, row * width * 2 + col * 2 + 1, attr);
			if (isKanji1(chr) && prevchr == 0 && (IS_JDOSV || col < width - 1)) {
				prevchr = chr;
				prevattr = attr;
				prow = row;
			} else if (isKanji2(chr) && prevchr != 0 && ((IS_JDOSV & !col) || (col && row == prow))) {
				WriteCharDOSVDbcs((col?col:cols) - 1, row - (!row||col?0:1), (prevchr << 8) | chr, attr);
				prevchr = 0;
				return;
			} else
				prevchr = 0;
			WriteCharDOSVSbcs(col, row, chr, attr);
			return;
		}
		fontdata=RealGetVec(0x43);
		break;
    }
    fontdata=RealMake(RealSeg(fontdata),RealOff(fontdata)+chr*cheight);

    if(GCC_UNLIKELY(!useattr)) { //Set attribute(color) to a sensible value
        static bool warned_use = false;
        if(GCC_UNLIKELY(!warned_use)){ 
            LOG(LOG_INT10,LOG_ERROR)("writechar used without attribute in non-textmode %c %X",chr,chr);
            warned_use = true;
        }
        switch(CurMode->type) {
        case M_CGA4:
            attr = 0x3;
            break;
        case M_CGA2:
            attr = 0x1;
            break;
        case M_TANDY16:
        case M_EGA:
        default:
            attr = 0x7;
            break;
        }
    }

    //Attribute behavior of mode 6; mode 11 does something similar but
    //it is in INT 10h handler because it only applies to function 09h
    if (CurMode->mode==0x06) attr=(attr&0x80)|1;

    switch (CurMode->type) {
    case M_VGA:
    case M_LIN8:
        // 256-color modes have background color instead of page
        back=page;
        page=0;
        break;
    case M_EGA:
        if (machine == MCH_EGA && vga.mem.memsize < (128*1024) && CurMode->sheight==350) attr=EGA4ColorRemap(attr);
        /* enable all planes for EGA modes (Ultima 1 colour bug) */
        /* might be put into INT10_PutPixel but different vga bios
           implementations have different opinions about this */
        IO_Write(0x3c4,0x2);IO_Write(0x3c5,0xf);
        /* fall through */
    default:
        back=attr&0x80;
        break;
    }

    Bitu x=col*8u,y=(unsigned int)(row*(unsigned int)cheight*(cols/CurMode->twidth));

    uint16_t ty=(uint16_t)y;
    for (uint8_t h=0;h<cheight;h++) {
        uint8_t bitsel=128;
        uint8_t bitline=mem_readb(Real2Phys(fontdata));
        fontdata=RealMake(RealSeg(fontdata),RealOff(fontdata)+1);
        uint16_t tx=(uint16_t)x;
        while (bitsel) {
            INT10_PutPixel(tx,ty,page,(bitline&bitsel)?attr:back);
            tx++;
            bitsel>>=1;
        }
        ty++;
    }
}

void INT10_WriteChar(uint16_t chr,uint8_t attr,uint8_t page,uint16_t count,bool showattr) {
    uint8_t pospage=page;
    if (CurMode->type!=M_TEXT) {
        showattr=true; //Use attr in graphics mode always
        switch (machine) {
            case EGAVGA_ARCH_CASE:
                switch (CurMode->type) {
                case M_VGA:
                case M_LIN8:
                    pospage=0;
                    break;
                default:
                    page%=CurMode->ptotal;
                    pospage=page;
                    break;
                }
                break;
            case MCH_CGA:
            case MCH_MCGA:
            case MCH_PCJR:
                page=0;
                pospage=0;
                break;
            default:
                break;
        }
    }

    uint8_t cur_row=CURSOR_POS_ROW(pospage);
    uint8_t cur_col=CURSOR_POS_COL(pospage);
    BIOS_NCOLS;
    while (count>0) {
        WriteChar(cur_col,cur_row,page,chr,attr,showattr);
        count--;
        cur_col++;
        if(cur_col==ncols) {
            cur_col=0;
            cur_row++;
        }
    }

    if (CurMode->type==M_EGA) {
        // Reset write ops for EGA graphics modes
        IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x0);
    }
}

static void INT10_TeletypeOutputAttr(uint8_t chr,uint8_t attr,bool useattr,uint8_t page) {
	BIOS_NCOLS;BIOS_NROWS;
	uint8_t cur_row=CURSOR_POS_ROW(page);
	uint8_t cur_col=CURSOR_POS_COL(page);
	switch (chr) {
		case 8:
			if(cur_col>0) cur_col--;
			break;
		case '\r':
			cur_col=0;
			break;
		case '\n':
//			cur_col=0; // Seems to break an old chess game
//			           // That's absolutely right, \r CR and \n LF, this is why DOS requires \r\n for newline --J.C>
			cur_row++;
			break;
		case 7: /* Beep */
			// Prepare PIT counter 2 for ~900 Hz square wave
			IO_Write(0x43, 0xb6);
			IO_Write(0x42, 0x28);
			IO_Write(0x42, 0x05);
			// Speaker on
			IO_Write(0x61, IO_Read(0x61) | 0x3);
			// Idle for 1/3rd of a second
			double start;
			start = PIC_FullIndex();
			while ((PIC_FullIndex() - start) < 333.0) CALLBACK_Idle();
			// Speaker off
			IO_Write(0x61, IO_Read(0x61) & ~0x3);
			if (CurMode->type==M_TEXT) {
				uint16_t chat;
				INT10_ReadCharAttr(&chat,page);
				if ((uint8_t)(chat>>8)!=7) return;
			}
			return; /* don't do anything else, not even scrollup on last line (fix for Elder Scrolls Arena installer) */
		default:
			/* Return if the char code is DBCS at the end of the line (for DOS/V) */
			if (cur_col + 1 == ncols && IS_DOSV && DOSV_CheckCJKVideoMode() && isKanji1(chr) && prevchr == 0) {
				INT10_TeletypeOutputAttr(' ', attr, useattr, page);
				cur_row = CURSOR_POS_ROW(page);
				cur_col = CURSOR_POS_COL(page);
			}
			/* Draw the actual Character */
			if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
				WriteCharV(cur_col,cur_row,chr,attr,useattr);
			} else {
				WriteChar(cur_col,cur_row,page,chr,attr,useattr);
			}
			cur_col++;
	}
	if(cur_col==ncols) {
		cur_col=0;
		cur_row++;
	}
	// Do we need to scroll ?
	if(cur_row==nrows) {
		//Fill with black on non-text modes
		uint8_t fill = 0;
		if (IS_PC98_ARCH && CurMode->type == M_TEXT) {
			//Fill with the default ANSI attribute on textmode
			fill = DefaultANSIAttr();
		}
		else if (CurMode->type==M_TEXT || DOSV_CheckCJKVideoMode()) {
			//Fill with attribute at cursor on textmode
			uint16_t chat;
			INT10_ReadCharAttr(&chat,page);
			fill=(uint8_t)(chat>>8);
		}
		INT10_ScrollWindow(0,0,(uint8_t)(nrows-1),(uint8_t)(ncols-1),-1,fill,page);
		cur_row--;
	}
	// Set the cursor for the page
	INT10_SetCursorPos(cur_row,cur_col,page);
}

void INT10_TeletypeOutputAttr(uint8_t chr,uint8_t attr,bool useattr) {
    INT10_TeletypeOutputAttr(chr,attr,useattr,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE));
}

void INT10_TeletypeOutput(uint8_t chr,uint8_t attr) {
    INT10_TeletypeOutputAttr(chr,attr,CurMode->type!=M_TEXT);
}

#define	EXTEND_ATTRIBUTE_HORIZON_LINE	0x04
#define	EXTEND_ATTRIBUTE_VERTICAL_LINE	0x08
#define	EXTEND_ATTRIBUTE_UNDER_LINE		0x80
#define	EXTEND_ATTRIBUTE_ALL			(EXTEND_ATTRIBUTE_HORIZON_LINE|EXTEND_ATTRIBUTE_VERTICAL_LINE|EXTEND_ATTRIBUTE_UNDER_LINE)

static void DrawExtendAttribute(uint16_t col, uint16_t row, uint8_t attr, uint8_t ex_attr)
{
	Bitu off, off_start;
	volatile uint8_t dummy;
	Bitu width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	uint8_t height = real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
	uint8_t no, select;
	const uint8_t mask_data[2][2] = {{ 0xff, 0xf0 }, { 0x0f, 0xff }};

	if(height == 24) {
		width = (width == 85) ? 128 : 160;
		off = row * width * 24 + (col * 12) / 8;
	} else {
		off = row * width * height + col;
	}
	off_start = off;
	IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x0a);
	IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
	if(ex_attr & EXTEND_ATTRIBUTE_HORIZON_LINE) {
		select = StartBankSelect(off);
		if(height == 24) {
			IO_Write(0x3ce, 0x08); IO_Write(0x3cf, mask_data[col & 1][0]);
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0x07);
			off++;
			select = CheckBankSelect(select, off);
			IO_Write(0x3ce, 0x08); IO_Write(0x3cf, mask_data[col & 1][1]);
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0x07);
		} else {
			IO_Write(0x3ce, 0x08); IO_Write(0x3cf, 0xff);
			real_writeb(0xa000, off, 0x07);
		}
	}
	if(ex_attr & EXTEND_ATTRIBUTE_VERTICAL_LINE) {
		off = off_start;
		select = StartBankSelect(off);
		IO_Write(0x3ce, 0x08); 
		if(height != 24 || (col & 1) == 0) {
			IO_Write(0x3cf, 0x80);
		} else {
			IO_Write(0x3cf, 0x08);
		}
		for(no = 0 ; no < height ; no++) {
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0x07);
			off += width;
			select = CheckBankSelect(select, off);
		}
	}
	if(ex_attr & EXTEND_ATTRIBUTE_UNDER_LINE) {
		off = off_start + (width * (height - 1));
		select = StartBankSelect(off);
		if(height == 24) {
			IO_Write(0x3ce, 0x08); IO_Write(0x3cf, mask_data[col & 1][0]);
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, attr & 0x0f);
			off++;
			select = CheckBankSelect(select, off);
			IO_Write(0x3ce, 0x08); IO_Write(0x3cf, mask_data[col & 1][1]);
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, attr & 0x0f);
		} else {
			IO_Write(0x3ce, 0x08); IO_Write(0x3cf, 0xff);
			real_writeb(0xa000, off, attr & 0x0f);
		}
	}
	IO_Write(0x3ce, 0x08); IO_Write(0x3cf, 0xff);
}

void INT10_WriteString(uint8_t row,uint8_t col,uint8_t flag,uint8_t attr,PhysPt string,uint16_t count,uint8_t page) {
    uint8_t cur_row=CURSOR_POS_ROW(page);
    uint8_t cur_col=CURSOR_POS_COL(page);
    uint8_t ex_attr;
    uint8_t dbcs_ex_attr = 0;
    uint8_t dbcs_attr = 0;
    
    // if row=0xff special case : use current cursor position
    if (row==0xff) {
        row=cur_row;
        col=cur_col;
    }
    INT10_SetCursorPos(row,col,page);
    while (count>0) {
        uint8_t chr=mem_readb(string);
        string++;
        if ((flag & 2) != 0 || (DOSV_CheckCJKVideoMode() && (flag == 0x20 || flag == 0x21))) {
            attr=mem_readb(string);
            string++;
            if(flag == 0x21) {
                ex_attr = mem_readb(string);
                string += 2;
                ExtendAttribute[row * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) + col] = ex_attr;
            }
        }
        if (DOSV_CheckCJKVideoMode() && (flag == 0x20 || flag == 0x21)) {
            WriteCharV(col, row, chr, attr, true);
            if(flag == 0x21 && IsExtendAttributeMode()) {
                if(ex_attr & EXTEND_ATTRIBUTE_ALL) {
                    DrawExtendAttribute(col, row, attr, ex_attr);
                }
                if(dbcs_ex_attr & EXTEND_ATTRIBUTE_ALL) {
                    DrawExtendAttribute(col - 1, row, dbcs_attr, dbcs_ex_attr);
                }
                dbcs_attr = 0;
                dbcs_ex_attr = 0;
                if(isKanji1(chr) && (ex_attr & EXTEND_ATTRIBUTE_ALL)) {
                    dbcs_attr = attr;
                    dbcs_ex_attr = ex_attr;
                }
            }
            col++;
            if (col == real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)) {
                col = 0;
                if(row == real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS) + 1)
                    break;
                row++;
            }
        } else
            INT10_TeletypeOutputAttr(chr,attr,true,page);
        count--;
    }
    if (!(flag&1) || flag == 0x21) {
        INT10_SetCursorPos(cur_row,cur_col,page);
    }
}

bool pc98_doskey_insertmode = false;

bool INT10_GetInsertState()
{
    if (IS_PC98_ARCH) {
        /* state is internal to DOSKEY */
        return pc98_doskey_insertmode;
    }
    else {
        const auto flags = mem_readb(BIOS_KEYBOARD_FLAGS1);
        const auto state =static_cast<bool>(flags & BIOS_KEYBOARD_FLAGS1_INSERT_ACTIVE);
        return state;
    }
}
