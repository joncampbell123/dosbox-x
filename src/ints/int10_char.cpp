/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


/* Character displaying moving functions */

#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "shiftjis.h"
#include "callback.h"

Bit8u DefaultANSIAttr();

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

static void MCGA2_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    PhysPt dest=base+((CurMode->twidth*rnew)*cheight+cleft);
    PhysPt src=base+((CurMode->twidth*rold)*cheight+cleft);
    Bitu copy=(Bitu)(cright-cleft);
    Bitu nextline=CurMode->twidth;
    for (Bitu i=0;i<cheight;i++) {
        MEM_BlockCopy(dest,src,copy);
        dest+=nextline;src+=nextline;
    }
}

static void CGA2_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
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

static void CGA4_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/2)+cleft)*2;
    PhysPt src=base+((CurMode->twidth*rold)*(cheight/2)+cleft)*2;   
    Bitu copy=(Bitu)(cright-cleft)*2u;Bitu nextline=(Bitu)CurMode->twidth*2u;
    for (Bitu i=0;i<cheight/2U;i++) {
        MEM_BlockCopy(dest,src,copy);
        MEM_BlockCopy(dest+8*1024,src+8*1024,copy);
        dest+=nextline;src+=nextline;
    }
}

static void TANDY16_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    Bit8u banks=CurMode->twidth/10;
    PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/banks)+cleft)*4;
    PhysPt src=base+((CurMode->twidth*rold)*(cheight/banks)+cleft)*4;
    Bitu copy=(Bitu)(cright-cleft)*4u;Bitu nextline=(Bitu)CurMode->twidth*4u;
    for (Bitu i=0;i<static_cast<Bitu>(cheight/banks);i++) {
		for (Bitu b=0;b<banks;b++) MEM_BlockCopy(dest+b*8*1024,src+b*8*1024,copy);
        dest+=nextline;src+=nextline;
    }
}

static void EGA16_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    PhysPt src,dest;Bitu copy;
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    dest=base+(CurMode->twidth*rnew)*cheight+cleft;
    src=base+(CurMode->twidth*rold)*cheight+cleft;
    Bitu nextline=(Bitu)CurMode->twidth;
    /* Setup registers correctly */
    IO_Write(0x3ce,5);IO_Write(0x3cf,1);        /* Memory transfer mode */
    IO_Write(0x3c4,2);IO_Write(0x3c5,0xf);      /* Enable all Write planes */
    /* Do some copying */
    Bitu rowsize=(Bitu)(cright-cleft);
    copy=cheight;
    for (;copy>0;copy--) {
        for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,mem_readb(src+x));
        dest+=nextline;src+=nextline;
    }
    /* Restore registers */
    IO_Write(0x3ce,5);IO_Write(0x3cf,0);        /* Normal transfer mode */
}

static void VGA_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    PhysPt src,dest;Bitu copy;
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
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

static void TEXT_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    PhysPt src,dest;
    src=base+(rold*CurMode->twidth+cleft)*2u;
    dest=base+(rnew*CurMode->twidth+cleft)*2u;
    MEM_BlockCopy(dest,src,(Bitu)(cright-cleft)*2u);
}

static void PC98_CopyRow(Bit8u cleft,Bit8u cright,Bit8u rold,Bit8u rnew,PhysPt base) {
    PhysPt src,dest;

    /* character data */
    src=base+(rold*CurMode->twidth+cleft)*2;
    dest=base+(rnew*CurMode->twidth+cleft)*2;
    MEM_BlockCopy(dest,src,(Bitu)(cright-cleft)*2u);

    /* attribute data */
    MEM_BlockCopy(dest+0x2000,src+0x2000,(Bitu)(cright-cleft)*2u);
}

static void MCGA2_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
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

static void CGA2_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
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

static void CGA4_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
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

static void TANDY16_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    Bit8u banks=CurMode->twidth/10;
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

static void EGA16_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    /* Set Bitmask / Color / Full Set Reset */
    IO_Write(0x3ce,0x8);IO_Write(0x3cf,0xff);
    IO_Write(0x3ce,0x0);IO_Write(0x3cf,attr);
    IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
    /* Enable all Write planes */
    IO_Write(0x3c4,2);IO_Write(0x3c5,0xf);
    /* Write some bytes */
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    PhysPt dest=base+(CurMode->twidth*row)*cheight+cleft;   
    Bitu nextline=CurMode->twidth;
    Bitu copy = cheight;Bitu rowsize=(Bitu)(cright-cleft);
    for (;copy>0;copy--) {
        for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,0xff);
        dest+=nextline;
    }
    IO_Write(0x3cf,0);
}

static void VGA_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    /* Write some bytes */
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    PhysPt dest=base+8*((CurMode->twidth*row)*cheight+cleft);
    Bitu nextline=8*CurMode->twidth;
    Bitu copy = cheight;Bitu rowsize=8u*(Bitu)(cright-cleft);
    for (;copy>0;copy--) {
        for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,attr);
        dest+=nextline;
    }
}

static void PC98_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    /* Do some filing */
    PhysPt dest;
    dest=base+(row*CurMode->twidth+cleft)*2;
    Bit16u fill=' ';
    Bit16u fattr=attr ? attr : DefaultANSIAttr();
    for (Bit8u x=0;x<(Bitu)(cright-cleft);x++) {
        mem_writew(dest,fill);
        mem_writew(dest+0x2000,fattr);
        dest+=2;
    }
}

static void TEXT_FillRow(Bit8u cleft,Bit8u cright,Bit8u row,PhysPt base,Bit8u attr) {
    /* Do some filing */
    PhysPt dest;
    dest=base+(row*CurMode->twidth+cleft)*2;
    Bit16u fill=(attr<<8)+' ';
    for (Bit8u x=0;x<(Bitu)(cright-cleft);x++) {
        mem_writew(dest,fill);
        dest+=2;
    }
}


void INT10_ScrollWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8s nlines,Bit8u attr,Bit8u page) {
/* Do some range checking */
    if (CurMode->type!=M_TEXT) page=0xff;
    BIOS_NCOLS;BIOS_NROWS;
    if(rul>rlr) return;
    if(cul>clr) return;
    if(rlr>=nrows) rlr=(Bit8u)nrows-1;
    if(clr>=ncols) clr=(Bit8u)ncols-1;
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
    Bit8u start,end;Bits next;
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
            TEXT_CopyRow(cul,clr,start,start+nlines,base);break;
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
            EGA16_CopyRow(cul,clr,start,start+nlines,base);break;
        case M_VGA:     
            VGA_CopyRow(cul,clr,start,start+nlines,base);break;
        case M_LIN4:
            if ((machine==MCH_VGA) && (svgaCard==SVGA_TsengET4K) &&
                    (CurMode->swidth<=800)) {
                // the ET4000 BIOS supports text output in 800x600 SVGA
                EGA16_CopyRow(cul,clr,start,start+nlines,base);break;
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
            TEXT_FillRow(cul,clr,start,base,attr);break;
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
            EGA16_FillRow(cul,clr,start,base,attr);break;
        case M_VGA:     
            VGA_FillRow(cul,clr,start,base,attr);break;
        case M_LIN4:
            if ((machine==MCH_VGA) && (svgaCard==SVGA_TsengET4K) &&
                    (CurMode->swidth<=800)) {
                EGA16_FillRow(cul,clr,start,base,attr);break;
            }
            // fall-through
        default:
            LOG(LOG_INT10,LOG_ERROR)("Unhandled mode %d for scroll",CurMode->type);
        }   
        start++;
    } 
}

void INT10_SetActivePage(Bit8u page) {
    Bit16u mem_address;
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
    Bit16u base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
    IO_Write(base,0x0cu);
    IO_Write(base+1u,(Bit8u)(mem_address>>8u));
    IO_Write(base,0x0du);
    IO_Write(base+1u,(Bit8u)mem_address);

    // And change the BIOS page
    real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,page);
    Bit8u cur_row=CURSOR_POS_ROW(page);
    Bit8u cur_col=CURSOR_POS_COL(page);
    // Display the cursor, now the page is active
    INT10_SetCursorPos(cur_row,cur_col,page);
}

void INT10_SetCursorShape(Bit8u first,Bit8u last) {
    real_writew(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE,last|(first<<8u));
    if (machine==MCH_CGA || machine==MCH_MCGA || machine==MCH_AMSTRAD) goto dowrite;
    if (IS_TANDY_ARCH) goto dowrite;
    /* Skip CGA cursor emulation if EGA/VGA system is active */
    if (!(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0x8)) {
        /* Check for CGA type 01, invisible */
        if ((first & 0x60) == 0x20) {
            first=0x1e;
            last=0x00;
            goto dowrite;
        }
        /* Check if we need to convert CGA Bios cursor values */
        if (!(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0x1)) { // set by int10 fun12 sub34
//          if (CurMode->mode>0x3) goto dowrite;    //Only mode 0-3 are text modes on cga
            if ((first & 0xe0) || (last & 0xe0)) goto dowrite;
            Bit8u cheight=real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT)-1;
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
    Bit16u base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
    IO_Write(base,0xa);IO_Write(base+1u,first);
    IO_Write(base,0xb);IO_Write(base+1u,last);
}

void vga_pc98_direct_cursor_pos(Bit16u address);

void INT10_GetScreenColumns(Bit16u *cols)
{
    if (IS_PC98_ARCH)
        *cols = 80; //TODO
    else
        *cols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
}

void INT10_GetCursorPos(Bit8u *row, Bit8u*col, const Bit8u page)
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

void INT10_SetCursorPos(Bit8u row,Bit8u col,Bit8u page) {
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
    Bit8u current=IS_PC98_ARCH ? 0 : real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
    if(page==current) {
        // Get the dimensions
        BIOS_NCOLS;
        // Calculate the address knowing nbcols nbrows and page num
        // NOTE: BIOSMEM_CURRENT_START counts in colour/flag pairs
        Bit16u address=(ncols*row)+col+(IS_PC98_ARCH ? 0 : (real_readw(BIOSMEM_SEG,BIOSMEM_CURRENT_START)/2));
        if (IS_PC98_ARCH) {
            vga_pc98_direct_cursor_pos(address);
        }
        else {
            // CRTC regs 0x0e and 0x0f
            Bit16u base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
            IO_Write(base,0x0eu);
            IO_Write(base+1u,(Bit8u)(address>>8u));
            IO_Write(base,0x0fu);
            IO_Write(base+1u,(Bit8u)address);
        }
    }
}

void ReadCharAttr(Bit16u col,Bit16u row,Bit8u page,Bit16u * result) {
    /* Externally used by the mouse routine */
    PhysPt fontdata;
    Bit16u cols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    Bit8u cheight = real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
    bool split_chr = false;
    switch (CurMode->type) {
    case M_TEXT:
        {   
            // Compute the address  
            Bit16u address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
            address+=(row*cols+col)*2;
            // read the char 
            PhysPt where = CurMode->pstart+address;
            *result=mem_readw(where);
        }
        return;
    case M_PC98:
        {
            // Compute the address  
            Bit16u address=((row*80)+col)*2;
            // read the char 
            PhysPt where = CurMode->pstart+address;
            *result=mem_readw(where);
        }
        return;
    case M_CGA4:
    case M_CGA2:
    case M_TANDY16:
        split_chr = true;
        switch (machine) {
        case MCH_CGA:
        case MCH_HERC:
            fontdata=PhysMake(0xf000,0xfa6e);
            break;
        case TANDY_ARCH_CASE:
            fontdata=Real2Phys(RealGetVec(0x44));
            break;
        default:
            fontdata=Real2Phys(RealGetVec(0x43));
            break;
        }
        break;
    default:
        fontdata=Real2Phys(RealGetVec(0x43));
        break;
    }

    Bitu x=col*8u,y=(unsigned int)row*cheight*(cols/CurMode->twidth);

    for (Bit16u chr=0;chr<256;chr++) {

        if (chr==128 && split_chr) fontdata=Real2Phys(RealGetVec(0x1f));

        bool error=false;
        Bit16u ty=(Bit16u)y;
        for (Bit8u h=0;h<cheight;h++) {
            Bit8u bitsel=128;
            Bit8u bitline=mem_readb(fontdata++);
            Bit8u res=0;
            Bit8u vidline=0;
            Bit16u tx=(Bit16u)x;
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
                fontdata+=(unsigned int)(cheight-(unsigned int)h-1u);
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
void INT10_ReadCharAttr(Bit16u * result,Bit8u page) {
    if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
    Bit8u cur_row=CURSOR_POS_ROW(page);
    Bit8u cur_col=CURSOR_POS_COL(page);
    ReadCharAttr(cur_col,cur_row,page,result);
}

void INT10_PC98_CurMode_Relocate(void) {
    /* deprecated */
}

void WriteChar(Bit16u col,Bit16u row,Bit8u page,Bit16u chr,Bit8u attr,bool useattr) {
    /* Externally used by the mouse routine */
    PhysPt fontdata;
    Bit16u cols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    Bit8u back, cheight = IS_PC98_ARCH ? 16 : real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);

    if (CurMode->type != M_PC98)
        chr &= 0xFF;

    switch (CurMode->type) {
    case M_TEXT:
        {   
            // Compute the address  
            Bit16u address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
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
            Bit16u address=((row*80)+col)*2;
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
    case M_CGA4:
    case M_CGA2:
    case M_TANDY16:
        if (chr>=128) {
            chr-=128;
            fontdata=Real2Phys(RealGetVec(0x1f));
            break;
        }
        switch (machine) {
        case MCH_CGA:
        case MCH_HERC:
            fontdata=PhysMake(0xf000,0xfa6e);
            break;
        case TANDY_ARCH_CASE:
            fontdata=Real2Phys(RealGetVec(0x44));
            break;
        default:
            fontdata=Real2Phys(RealGetVec(0x43));
            break;
        }
        break;
    default:
        fontdata=Real2Phys(RealGetVec(0x43));
        break;
    }
    fontdata+=(unsigned int)chr*(unsigned int)cheight;

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

    Bit16u ty=(Bit16u)y;
    for (Bit8u h=0;h<cheight;h++) {
        Bit8u bitsel=128;
        Bit8u bitline=mem_readb(fontdata++);
        Bit16u tx=(Bit16u)x;
        while (bitsel) {
            INT10_PutPixel(tx,ty,page,(bitline&bitsel)?attr:back);
            tx++;
            bitsel>>=1;
        }
        ty++;
    }
}

void INT10_WriteChar(Bit16u chr,Bit8u attr,Bit8u page,Bit16u count,bool showattr) {
    Bit8u pospage=page;
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

    Bit8u cur_row=CURSOR_POS_ROW(pospage);
    Bit8u cur_col=CURSOR_POS_COL(pospage);
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

static void INT10_TeletypeOutputAttr(Bit8u chr,Bit8u attr,bool useattr,Bit8u page) {
    BIOS_NCOLS;BIOS_NROWS;
    Bit8u cur_row=CURSOR_POS_ROW(page);
    Bit8u cur_col=CURSOR_POS_COL(page);
    switch (chr) {
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
        // No change in position
        return;
    case 8:
        if(cur_col>0) cur_col--;
        break;
    case '\r':
        cur_col=0;
        break;
    case '\n':
//      cur_col=0; //Seems to break an old chess game
        cur_row++;
        break;
    default:
        /* Draw the actual Character */
        WriteChar(cur_col,cur_row,page,chr,attr,useattr);
        cur_col++;
    }
    if(cur_col==ncols) {
        cur_col=0;
        cur_row++;
    }
    // Do we need to scroll ?
    if(cur_row==nrows) {
        //Fill with black on non-text modes
        Bit8u fill = 0;
        if (IS_PC98_ARCH && CurMode->type == M_TEXT) {
            //Fill with the default ANSI attribute on textmode
            fill = DefaultANSIAttr();
        }
        else if (CurMode->type==M_TEXT) {
            //Fill with attribute at cursor on textmode
            Bit16u chat;
            INT10_ReadCharAttr(&chat,page);
            fill=(Bit8u)(chat>>8);
        }
        INT10_ScrollWindow(0,0,(Bit8u)(nrows-1),(Bit8u)(ncols-1),-1,fill,page);
        cur_row--;
    }
    // Set the cursor for the page
    INT10_SetCursorPos(cur_row,cur_col,page);
}

void INT10_TeletypeOutputAttr(Bit8u chr,Bit8u attr,bool useattr) {
    INT10_TeletypeOutputAttr(chr,attr,useattr,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE));
}

void INT10_TeletypeOutput(Bit8u chr,Bit8u attr) {
    INT10_TeletypeOutputAttr(chr,attr,CurMode->type!=M_TEXT);
}

void INT10_WriteString(Bit8u row,Bit8u col,Bit8u flag,Bit8u attr,PhysPt string,Bit16u count,Bit8u page) {
    Bit8u cur_row=CURSOR_POS_ROW(page);
    Bit8u cur_col=CURSOR_POS_COL(page);
    
    // if row=0xff special case : use current cursor position
    if (row==0xff) {
        row=cur_row;
        col=cur_col;
    }
    INT10_SetCursorPos(row,col,page);
    while (count>0) {
        Bit8u chr=mem_readb(string);
        string++;
        if (flag&2) {
            attr=mem_readb(string);
            string++;
        }
        INT10_TeletypeOutputAttr(chr,attr,true,page);
        count--;
    }
    if (!(flag&1)) {
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
