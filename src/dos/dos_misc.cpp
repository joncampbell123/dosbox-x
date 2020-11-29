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
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "control.h"
#include <list>

uint32_t DOS_HMA_LIMIT();
uint32_t DOS_HMA_FREE_START();
uint32_t DOS_HMA_GET_FREE_SPACE();
void DOS_HMA_CLAIMED(uint16_t bytes);
bool ANSI_SYS_installed();

extern bool enable_share_exe_fake;

extern Bitu XMS_EnableA20(bool enable);

bool enable_a20_on_windows_init = false;

static Bitu call_int2f,call_int2a;

static std::list<MultiplexHandler*> Multiplex;
typedef std::list<MultiplexHandler*>::iterator Multiplex_it;

const char *Win_NameThatVXD(uint16_t devid) {
	switch (devid) {
		case 0x0006:	return "V86MMGR";
		case 0x000C:	return "VMD";
		case 0x000D:	return "VKD";
		case 0x0010:	return "BLOCKDEV";
		case 0x0014:	return "VNETBIOS";
		case 0x0015:	return "DOSMGR";
		case 0x0018:	return "VMPOLL";
		case 0x0021:	return "PAGEFILE";
		case 0x002D:	return "W32S";
		case 0x0040:	return "IFSMGR";
		case 0x0446:	return "VADLIBD";
		case 0x0484:	return "IFSMGR";
		case 0x0487:	return "NWSUP";
		case 0x28A1:	return "PHARLAP";
		case 0x7A5F:	return "SIWVID";
	}

	return NULL;
}

void DOS_AddMultiplexHandler(MultiplexHandler * handler) {
	Multiplex.push_front(handler);
}

void DOS_DelMultiplexHandler(MultiplexHandler * handler) {
	for(Multiplex_it it =Multiplex.begin();it != Multiplex.end();++it) {
		if(*it == handler) {
			Multiplex.erase(it);
			return;
		}
	}
}

static Bitu INT2F_Handler(void) {
	for(Multiplex_it it = Multiplex.begin();it != Multiplex.end();++it)
		if( (*it)() ) return CBRET_NONE;
   
	LOG(LOG_DOSMISC,LOG_DEBUG)("DOS:INT 2F Unhandled call AX=%4X",reg_ax);
	return CBRET_NONE;
}


static Bitu INT2A_Handler(void) {
	return CBRET_NONE;
}

extern bool i4dos, shellrun, clipboard_dosapi;
extern RealPt DOS_DriveDataListHead;       // INT 2Fh AX=0803h DRIVER.SYS drive data table list

// INT 2F
char regpath[CROSS_LEN+1]="C:\\WINDOWS\\SYSTEM.DAT";
static bool DOS_MultiplexFunctions(void) {
    char name[256];
	switch (reg_ax) {
    case 0x0800:    /* DRIVER.SYS function */
    case 0x0801:    /* DRIVER.SYS function */
    case 0x0802:    /* DRIVER.SYS function */
        LOG(LOG_DOSMISC,LOG_DEBUG)("Unhandled DRIVER.SYS call AX=%04x BX=%04x CX=%04x DX=%04x BP=%04x",reg_ax,reg_bx,reg_cx,reg_dx,reg_bp);
        break;
    case 0x0803:    /* DRIVER.SYS function */
        LOG(LOG_DOSMISC,LOG_DEBUG)("Unhandled DRIVER.SYS call AX=%04x BX=%04x CX=%04x DX=%04x BP=%04x",reg_ax,reg_bx,reg_cx,reg_dx,reg_bp);
        // FIXME: Windows 95 SCANDISK.EXE relies on the drive data table list pointer provided by this call.
        //        Returning DS:DI unmodified or set to 0:0 will only send it off into the weeds chasing random data
        //        as a linked list. However looking at the code DI=0xFFFF is sufficient to prevent that until
        //        DOSBox-X emulates DRIVER.SYS functions and provides the information it expects according to RBIL.
        //        BUT, Windows 95 setup checks if the pointer is NULL, and considers 0:FFFF valid >_<.
        //        It's just easier to return a pointer to a dummy table.
        //        [http://www.ctyme.com/intr/rb-4283.htm]
        SegSet16(ds,DOS_DriveDataListHead >> 16);
        reg_di = DOS_DriveDataListHead;
        break;
	/* ert, 20100711: Locking extensions */
    case 0x1000:    /* SHARE.EXE installation check */
        if (enable_share_exe_fake) {
            reg_al = 0xff; /* Pretend that share.exe is installed.. Of course it's a bloody LIE! */
        }
        return true;
	case 0x1216:	/* GET ADDRESS OF SYSTEM FILE TABLE ENTRY */
		// reg_bx is a system file table entry, should coincide with
		// the file handle so just use that
		LOG(LOG_DOSMISC,LOG_ERROR)("Some BAD filetable call used bx=%X",reg_bx);
		if(reg_bx <= DOS_FILES) CALLBACK_SCF(false);
		else CALLBACK_SCF(true);
		if (reg_bx<16) {
			RealPt sftrealpt=mem_readd(Real2Phys(dos_infoblock.GetPointer())+4);
			PhysPt sftptr=Real2Phys(sftrealpt);
			uint32_t sftofs=0x06u+reg_bx*0x3bu;

			if (Files[reg_bx]) mem_writeb(sftptr+sftofs, (uint8_t)(Files[reg_bx]->refCtr));
			else mem_writeb(sftptr+sftofs,0);

			if (!Files[reg_bx]) return true;

			uint32_t handle=RealHandle(reg_bx);
			if (handle>=DOS_FILES) {
				mem_writew(sftptr+sftofs+0x02,0x02);	// file open mode
				mem_writeb(sftptr+sftofs+0x04,0x00);	// file attribute
				mem_writew(sftptr+sftofs+0x05,Files[reg_bx]->GetInformation());	// device info word
				mem_writed(sftptr+sftofs+0x07,0);		// device driver header
				mem_writew(sftptr+sftofs+0x0d,0);		// packed time
				mem_writew(sftptr+sftofs+0x0f,0);		// packed date
				mem_writew(sftptr+sftofs+0x11,0);		// size
				mem_writew(sftptr+sftofs+0x15,0);		// current position
			} else {
				uint8_t drive=Files[reg_bx]->GetDrive();

				mem_writew(sftptr+sftofs+0x02,(uint16_t)(Files[reg_bx]->flags&3));	// file open mode
				mem_writeb(sftptr+sftofs+0x04,(uint8_t)(Files[reg_bx]->attr));		// file attribute
				mem_writew(sftptr+sftofs+0x05,0x40|drive);							// device info word
				mem_writed(sftptr+sftofs+0x07,RealMake(dos.tables.dpb,drive*dos.tables.dpb_size));	// dpb of the drive
				mem_writew(sftptr+sftofs+0x0d,Files[reg_bx]->time);					// packed file time
				mem_writew(sftptr+sftofs+0x0f,Files[reg_bx]->date);					// packed file date
				uint32_t curpos=0;
				Files[reg_bx]->Seek(&curpos,DOS_SEEK_CUR);
				uint32_t endpos=0;
				Files[reg_bx]->Seek(&endpos,DOS_SEEK_END);
				mem_writed(sftptr+sftofs+0x11,endpos);		// size
				mem_writed(sftptr+sftofs+0x15,curpos);		// current position
				Files[reg_bx]->Seek(&curpos,DOS_SEEK_SET);
			}

			// fill in filename in fcb style
			// (space-padded name (8 chars)+space-padded extension (3 chars))
			const char* filename=(const char*)Files[reg_bx]->GetName();
			if (strrchr(filename,'\\')) filename=strrchr(filename,'\\')+1;
			if (strrchr(filename,'/')) filename=strrchr(filename,'/')+1;
			if (!filename) return true;
			const char* dotpos=strrchr(filename,'.');
			if (dotpos) {
				dotpos++;
				size_t nlen=strlen(filename);
				size_t extlen=strlen(dotpos);
				Bits nmelen=(Bits)nlen-(Bits)extlen;
				if (nmelen<1) return true;
				nlen-=(extlen+1);

				if (nlen>8) nlen=8;
				size_t i;

				for (i=0; i<nlen; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20u+i),(unsigned char)filename[i]);
				for (i=nlen; i<8; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20u+i),(unsigned char)' ');
				
				if (extlen>3) extlen=3;
				for (i=0; i<extlen; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x28u+i),(unsigned char)dotpos[i]);
				for (i=extlen; i<3; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x28u+i),(unsigned char)' ');
			} else {
				size_t i;
				size_t nlen=strlen(filename);
				if (nlen>8) nlen=8;
				for (i=0; i<nlen; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20u+i),(unsigned char)filename[i]);
				for (i=nlen; i<11; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20u+i),(unsigned char)' ');
			}

			SegSet16(es,RealSeg(sftrealpt));
			reg_di=RealOff(sftrealpt+sftofs);
			reg_ax=0xc000;

		}
		return true;
    case 0x1300:
    case 0x1302:
        reg_ax=0;
        return true;
    case 0x1611:    /* Get shell parameters */
		{
			if (dos.version.major < 7) return false;
			char psp_name[9];
			DOS_MCB psp_mcb(dos.psp()-1);
			psp_mcb.GetFileName(psp_name);
			if (!strcmp(psp_name, "DOSSETUP") || !strcmp(psp_name, "KRNL386")) {
				/* Hack for Windows 98 SETUP.EXE (Wengier) */
				return false;
			}
			strcpy(name,i4dos&&!shellrun?"4DOS.COM":"COMMAND.COM");
			MEM_BlockWrite(SegPhys(ds)+reg_dx,name,(Bitu)(strlen(name)+1));
			strcpy(name+1,"/P /D /K AUTOEXEC");
			name[0]=(char)strlen(name+1);
			MEM_BlockWrite(SegPhys(ds)+reg_si,name,(Bitu)(strlen(name+1)+2));
			reg_ax=0;
			reg_bx=0;
			return true;
		}
    case 0x1612:
		if (dos.version.major < 7) return false;
        reg_ax=0;
        name[0]=1;
        name[1]=0;
        MEM_BlockWrite(SegPhys(es)+reg_bx,name,0x20);
        return true;
    case 0x1613:    /* Get SYSTEM.DAT path */
		if (dos.version.major < 7) return false;
        strcpy(name,regpath);
        MEM_BlockWrite(SegPhys(es)+reg_di,name,(Bitu)(strlen(name)+1));
        reg_ax=0;
        reg_cx=(uint16_t)strlen(name);
        return true;
    case 0x1614:    /* Set SYSTEM.DAT path */
		if (dos.version.major < 7) return false;
        MEM_StrCopy(SegPhys(es)+reg_di,regpath,CROSS_LEN+1);
        reg_ax=0;
        return true;
    case 0x1600:    /* Windows enhanced mode installation check */
        // Leave AX as 0x1600, indicating that neither Windows 3.x enhanced mode, Windows/386 2.x
        // nor Windows 95 are running, nor is XMS version 1 driver installed
#ifdef WIN32
		if (!control->SecureMode() && (reg_sp == 0xFFF6 && mem_readw(SegPhys(ss)+reg_sp) == 0x142A || reg_sp >= 0xFF7A && reg_sp <= 0xFF8F && mem_readw(SegPhys(ss)+reg_sp) == reg_sp + 21))
			reg_ax = 0x301;
#endif
        return true;
	case 0x1605:	/* Windows init broadcast */
		if (enable_a20_on_windows_init) {
			/* This hack exists because Windows 3.1 doesn't seem to enable A20 first during an
			 * initial critical period where it assumes it's on, prior to checking and enabling/disabling it.
			 *
			 * Note that Windows 3.1 also makes this mistake in Standard/286 mode, but it doesn't even
			 * make this callout, so this hack is useless unless you are using Enhanced/386 mode.
			 * If you want to run Windows 3.1 Standard mode with a20=mask you will have to run builtin
			 * command "a20gate on" to turn on the A20 gate prior to starting Windows. */
			LOG_MSG("Enabling A20 gate for Windows in response to INIT broadcast");
			XMS_EnableA20(true);
		}

		/* TODO: Maybe future parts of DOSBox-X will do something with this */
		/* TODO: Don't show this by default. Show if the user wants it by a) setting something to "true" in dosbox.conf or b) running a builtin command in Z:\ */
		LOG_MSG("DEBUG: INT 2Fh Windows 286/386 DOSX init broadcast issued (ES:BX=%04x:%04x DS:SI=%04x:%04x CX=%04x DX=%04x DI=%04x(aka version %u.%u))",
			SegValue(es),reg_bx,
			SegValue(ds),reg_si,
			reg_cx,reg_dx,reg_di,
			reg_di>>8,reg_di&0xFF);
		if (reg_dx & 0x0001)
			LOG_MSG(" [286 DOS extender]");
		else
			LOG_MSG(" [Enhanced mode]");
		LOG_MSG("\n");

		/* NTS: The way this protocol works, is that when you (the program hooking this call) receive it,
		 *      you first pass the call down to the previous INT 2Fh handler with registers unmodified,
		 *      and then when the call unwinds back up the chain, THEN you modify the results to notify
		 *      yourself to Windows. So logically, since we're the DOS kernel at the end of the chain,
		 *      we should still see ES:BX=0000:0000 and DS:SI=0000:0000 and CX=0000 unmodified from the
		 *      way the Windows kernel issued the call. If that's not the case, then we need to issue
		 *      a warning because some bastard on the call chain is ruining it for all of us. */
		if (SegValue(es) != 0 || reg_bx != 0 || SegValue(ds) != 0 || reg_si != 0 || reg_cx != 0) {
			LOG_MSG("WARNING: Some registers at this point (the top of the call chain) are nonzero.\n");
			LOG_MSG("         That means a TSR or other entity has modified registers on the way down\n");
			LOG_MSG("         the call chain. The Windows init broadcast is supposed to be handled\n");
			LOG_MSG("         going down the chain by calling the previous INT 2Fh handler with registers\n");
			LOG_MSG("         unmodified, and only modify registers on the way back up the chain!\n");
		}

		return false; /* pass it on to other INT 2F handlers */
	case 0x1606:	/* Windows exit broadcast */
		/* TODO: Maybe future parts of DOSBox-X will do something with this */
		/* TODO: Don't show this by default. Show if the user wants it by a) setting something to "true" in dosbox.conf or b) running a builtin command in Z:\ */
		LOG_MSG("DEBUG: INT 2Fh Windows 286/386 DOSX exit broadcast issued (DX=0x%04x)",reg_dx);
		if (reg_dx & 0x0001)
			LOG_MSG(" [286 DOS extender]");
		else
			LOG_MSG(" [Enhanced mode]");
		LOG_MSG("\n");
		return false; /* pass it on to other INT 2F handlers */
	case 0x1607:
		/* TODO: Don't show this by default. Show if the user wants it by a) setting something to "true" in dosbox.conf or b) running a builtin command in Z:\
		 *       Additionally, if the user WANTS to see every invocation of the IDLE call, then allow them to enable that too */
		if (reg_bx != 0x18) { /* don't show the idle call. it's used too often */
			const char *str = Win_NameThatVXD(reg_bx);

			if (str == NULL) str = "??";
			LOG_MSG("DEBUG: INT 2Fh Windows virtual device '%s' callout (BX(deviceID)=0x%04x CX(function)=0x%04x)\n",
				str,reg_bx,reg_cx);
		}

		if (reg_bx == 0x15) { /* DOSMGR */
			switch (reg_cx) {
				case 0x0000:		// query instance
					reg_cx = 0x0001;
					reg_dx = 0x50;		// dos driver segment
					SegSet16(es,0x50);	// patch table seg
					reg_bx = 0x60;		// patch table ofs
					return true;
				case 0x0001:		// set patches
					reg_ax = 0xb97c;
					reg_bx = (reg_dx & 0x16);
					reg_dx = 0xa2ab;
					return true;
				case 0x0003:		// get size of data struc
					if (reg_dx==0x0001) {
						// CDS size requested
						reg_ax = 0xb97c;
						reg_dx = 0xa2ab;
						reg_cx = 0x000e;	// size
					}
					return true;
				case 0x0004:		// instanced data
					reg_dx = 0;		// none
					return true;
				case 0x0005:		// get device driver size
					reg_ax = 0;
					reg_dx = 0;
					return true;
				default:
					return false;
			}
		}
		else if (reg_bx == 0x18) { /* VMPoll (idle) */
			return true;
		}
		else return false;
	case 0x160A:
	{
		char psp_name[9];
		DOS_MCB psp_mcb(dos.psp()-1);
		psp_mcb.GetFileName(psp_name);
		// Report Windows version 4.0 (95) to NESTICLE x.xx so that it uses LFN when available
		if (uselfn && (!strcmp(psp_name, "NESTICLE") || (reg_sp == 0x220A && mem_readw(SegPhys(ss)+reg_sp)/0x100 == 0x1F))) {
			reg_ax = 0;
			reg_bx = 0x400;
			reg_cx = 2;
			return true;
		}
		return false;
	}
	case 0x1680:	/*  RELEASE CURRENT VIRTUAL MACHINE TIME-SLICE */
		//TODO Maybe do some idling but could screw up other systems :)
		return true; //So no warning in the debugger anymore
	case 0x1689:	/*  Kernel IDLE CALL */
	case 0x168f:	/*  Close awareness crap */
	   /* Removing warning */
		return true;
#ifdef WIN32
	case 0x1700:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_al = 1;
		reg_ah = 1;
		return true;
	case 0x1701:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_ax=0;
		if (OpenClipboard(NULL)) {
			reg_ax=1;
			CloseClipboard();
		}
		return true;
	case 0x1702:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_ax=0;
		if (OpenClipboard(NULL))
			{
			reg_ax=EmptyClipboard()?1:0;
			CloseClipboard();
			}
		return true;
	case 0x1703:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			char *text, *buffer;
			text = new char[reg_cx];
			MEM_StrCopy(SegPhys(es)+reg_bx,text,reg_cx);
			*(text+reg_cx-1)=0;
			HGLOBAL clipbuffer;
			EmptyClipboard();
			clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
			buffer = (char*)GlobalLock(clipbuffer);
			strcpy(buffer, text);
			delete[] text;
			GlobalUnlock(clipbuffer);
			SetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT,clipbuffer);
			reg_ax++;
			CloseClipboard();
			}
		return true;
	case 0x1704:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			if (HANDLE text = GetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT))
				{
				reg_ax=(uint16_t)strlen((char *)text)+1;
				reg_dx=(uint16_t)((strlen((char *)text)+1)/65536);
				}
			else
				reg_dx=0;
			CloseClipboard();
			}
		return true;
	case 0x1705:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			if (HANDLE text = GetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT))
				{
				MEM_BlockWrite(SegPhys(es)+reg_bx,text,(Bitu)(strlen((char *)text)+1));
				reg_ax++;
				}
			CloseClipboard();
			}
		return true;
	case 0x1708:
		if(control->SecureMode()||!clipboard_dosapi) return false;
		reg_ax=1;
		CloseClipboard();
		return true;
#endif
    case 0x1a00:    /* ANSI.SYS installation check (MS-DOS 4.0 or higher) */
        if (IS_PC98_ARCH) {
            /* NTS: PC-98 MS-DOS has ANSI handling directly within the kernel HOWEVER it does NOT
             *      respond to this INT 2Fh call. */
            return true;
        }
        else if (ANSI_SYS_installed()) {
            /* See also: [http://www.delorie.com/djgpp/doc/rbinter/id/71/46.html] */
            /* Reported behavior was confirmed with ANSI.SYS loaded on a Windows 95 MS-DOS boot disk, result AX=1AFF */
            reg_al = 0xFF; /* DOSBox/DOSBox-X console device emulates ANSI.SYS, so respond like it's installed */
            return true;
        }
        else {
            /* MS-DOS without ANSI.SYS loaded doesn't modify any registers in response to this call. */
            return true;
        }
    case 0x4680:    /* Windows v3.0 check */
        // Leave AX as 0x4680, indicating that Windows 3.0 is not running in real (/R) or standard (/S) mode,
        // nor is DOS 5 DOSSHELL active
        return true;
	case 0x4a01: {	/* Query free hma space */
		uint32_t limit = DOS_HMA_LIMIT();

		if (limit == 0) {
			/* TODO: What does MS-DOS prior to v5.0? */
			reg_bx = 0;
			reg_di = 0xFFFF;
			SegSet16(es,0xFFFF);
			LOG(LOG_DOSMISC,LOG_DEBUG)("HMA query: rejected");
			return true;
		}

		uint32_t start = DOS_HMA_FREE_START();
		reg_bx = limit - start; /* free space in bytes */
		SegSet16(es,0xffff);
		reg_di = (start + 0x10) & 0xFFFF;
		LOG(LOG_DOSMISC,LOG_DEBUG)("HMA query: start=0x%06x limit=0x%06x free=0x%06x -> bx=%u %04x:%04x",
			start,limit,DOS_HMA_GET_FREE_SPACE(),(int)reg_bx,(int)SegValue(es),(int)reg_di);
		} return true;
	case 0x4a02: {	/* ALLOCATE HMA SPACE */
		uint32_t limit = DOS_HMA_LIMIT();

		if (limit == 0) {
			/* TODO: What does MS-DOS prior to v5.0? */
			reg_bx = 0;
			reg_di = 0xFFFF;
			SegSet16(es,0xFFFF);
			LOG(LOG_DOSMISC,LOG_DEBUG)("HMA allocation: rejected");
			return true;
		}

		/* NTS: According to RBIL, Windows 95 adds a deallocate function and changes HMA allocation up to follow a
		 *      MCB chain structure. Which is something we're probably not going to add for awhile. */
		/* FIXME: So, according to Ralph Brown Interrupt List, MS-DOS 5 and 6 liked to round up to the next paragraph? */
		if (dos.version.major < 7 && (reg_bx & 0xF) != 0)
			reg_bx = (reg_bx + 0xF) & (~0xF);

		uint32_t start = DOS_HMA_FREE_START();
		if ((start+reg_bx) > limit) {
			LOG(LOG_DOSMISC,LOG_DEBUG)("HMA allocation: rejected (not enough room) for %u bytes (0x%x + 0x%x > 0x%x)",reg_bx,
                (unsigned int)start,(unsigned int)reg_bx,(unsigned int)limit);
			reg_bx = 0;
			reg_di = 0xFFFF;
			SegSet16(es,0xFFFF);
			return true;
		}

		/* convert the start to segment:offset, normalized to FFFF:offset */
		reg_di = (start + 0x10) & 0xFFFF;
		SegSet16(es,0xFFFF);

		/* let HMA emulation know what was claimed */
		LOG(LOG_DOSMISC,LOG_DEBUG)("HMA allocation: %u bytes at FFFF:%04x",reg_bx,reg_di);
		DOS_HMA_CLAIMED(reg_bx);
		} return true;
    case 0x4a10: { /* Microsoft SmartDrive (SMARTDRV) API */
        LOG(LOG_DOSMISC,LOG_DEBUG)("Unhandled SMARTDRV call AX=%04x BX=%04x CX=%04x DX=%04x BP=%04x",reg_ax,reg_bx,reg_cx,reg_dx,reg_bp);
	    } return true;
    case 0x4a11: { /* Microsoft DoubleSpace (DBLSPACE.BIN) API */
        LOG(LOG_DOSMISC,LOG_DEBUG)("Unhandled DBLSPACE call AX=%04x BX=%04x CX=%04x DX=%04x BP=%04x",reg_ax,reg_bx,reg_cx,reg_dx,reg_bp);
	    } return true;
    case 0x4a16:    /* Open bootlog */
        return true;
    case 0x4a17:    /* Write bootlog */
        MEM_StrCopy(SegPhys(ds)+reg_dx,name,255);
        LOG_MSG("BOOTLOG: %s\n",name);
        return true;
    case 0x4a18:    /* Close bootlog */
        return true;
	case 0x4a33:	/* Check MS-DOS Version 7 */
		if (dos.version.major > 6) {
			reg_ax=0;
			return true;
		}
    }

	return false;
}

void DOS_SetupMisc(void) {
	/* Setup the dos multiplex interrupt */
	call_int2f=CALLBACK_Allocate();
	CALLBACK_Setup(call_int2f,&INT2F_Handler,CB_IRET,"DOS Int 2f");
	RealSetVec(0x2f,CALLBACK_RealPointer(call_int2f));
	DOS_AddMultiplexHandler(DOS_MultiplexFunctions);
	/* Setup the dos network interrupt */
	call_int2a=CALLBACK_Allocate();
	CALLBACK_Setup(call_int2a,&INT2A_Handler,CB_IRET,"DOS Int 2a");
	RealSetVec(0x2A,CALLBACK_RealPointer(call_int2a));
}

extern const char* RunningProgram;
void CALLBACK_DeAllocate(Bitu in);

void DOS_UninstallMisc(void) {
    if (!strcmp(RunningProgram, "LOADLIN")) return;
	/* these vectors shouldn't exist when booting a guest OS */
	if (call_int2a) {
		RealSetVec(0x2a,0);
		CALLBACK_DeAllocate(call_int2a);
		call_int2a=0;
	}
	if (call_int2f) {
		RealSetVec(0x2f,0);
		CALLBACK_DeAllocate(call_int2f);
		call_int2f=0;
	}
}

