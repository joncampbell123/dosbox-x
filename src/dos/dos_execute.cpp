/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: dos_execute.cpp,v 1.68 2009-10-04 14:28:07 c2woody Exp $ */

#include <string.h>
#include <ctype.h>
#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "regs.h"
#include "callback.h"
#include "debug.h"
#include "cpu.h"

const char * RunningProgram="DOSBOX";

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct EXE_Header {
	Bit16u signature;					/* EXE Signature MZ or ZM */
	Bit16u extrabytes;					/* Bytes on the last page */
	Bit16u pages;						/* Pages in file */
	Bit16u relocations;					/* Relocations in file */
	Bit16u headersize;					/* Paragraphs in header */
	Bit16u minmemory;					/* Minimum amount of memory */
	Bit16u maxmemory;					/* Maximum amount of memory */
	Bit16u initSS;
	Bit16u initSP;
	Bit16u checksum;
	Bit16u initIP;
	Bit16u initCS;
	Bit16u reloctable;
	Bit16u overlay;
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif

#define MAGIC1 0x5a4d
#define MAGIC2 0x4d5a
#define MAXENV 32768u
#define ENV_KEEPFREE 83				 /* keep unallocated by environment variables */
#define LOADNGO 0
#define LOAD    1
#define OVERLAY 3



static void SaveRegisters(void) {
	reg_sp-=18;
	mem_writew(SegPhys(ss)+reg_sp+ 0,reg_ax);
	mem_writew(SegPhys(ss)+reg_sp+ 2,reg_cx);
	mem_writew(SegPhys(ss)+reg_sp+ 4,reg_dx);
	mem_writew(SegPhys(ss)+reg_sp+ 6,reg_bx);
	mem_writew(SegPhys(ss)+reg_sp+ 8,reg_si);
	mem_writew(SegPhys(ss)+reg_sp+10,reg_di);
	mem_writew(SegPhys(ss)+reg_sp+12,reg_bp);
	mem_writew(SegPhys(ss)+reg_sp+14,SegValue(ds));
	mem_writew(SegPhys(ss)+reg_sp+16,SegValue(es));
}

static void RestoreRegisters(void) {
	reg_ax=mem_readw(SegPhys(ss)+reg_sp+ 0);
	reg_cx=mem_readw(SegPhys(ss)+reg_sp+ 2);
	reg_dx=mem_readw(SegPhys(ss)+reg_sp+ 4);
	reg_bx=mem_readw(SegPhys(ss)+reg_sp+ 6);
	reg_si=mem_readw(SegPhys(ss)+reg_sp+ 8);
	reg_di=mem_readw(SegPhys(ss)+reg_sp+10);
	reg_bp=mem_readw(SegPhys(ss)+reg_sp+12);
	SegSet16(ds,mem_readw(SegPhys(ss)+reg_sp+14));
	SegSet16(es,mem_readw(SegPhys(ss)+reg_sp+16));
	reg_sp+=18;
}

extern void GFX_SetTitle(Bit32s cycles,Bits frameskip,bool paused);
void DOS_UpdatePSPName(void) {
	DOS_MCB mcb(dos.psp()-1);
	static char name[9];
	mcb.GetFileName(name);
	if (!strlen(name)) strcpy(name,"DOSBOX");
	RunningProgram=name;
	GFX_SetTitle(-1,-1,false);
}

void DOS_Terminate(Bit16u pspseg,bool tsr,Bit8u exitcode) {

	dos.return_code=exitcode;
	dos.return_mode=(tsr)?(Bit8u)RETURN_TSR:(Bit8u)RETURN_EXIT;
	
	DOS_PSP curpsp(pspseg);
	if (pspseg==curpsp.GetParent()) return;
	/* Free Files owned by process */
	if (!tsr) curpsp.CloseFiles();
	
	/* Get the termination address */
	RealPt old22 = curpsp.GetInt22();
	/* Restore vector 22,23,24 */
	curpsp.RestoreVectors();
	/* Set the parent PSP */
	dos.psp(curpsp.GetParent());
	DOS_PSP parentpsp(curpsp.GetParent());

	/* Restore the SS:SP to the previous one */
	SegSet16(ss,RealSeg(parentpsp.GetStack()));
	reg_sp = RealOff(parentpsp.GetStack());		
	/* Restore the old CS:IP from int 22h */
	RestoreRegisters();
	/* Set the CS:IP stored in int 0x22 back on the stack */
	mem_writew(SegPhys(ss)+reg_sp+0,RealOff(old22));
	mem_writew(SegPhys(ss)+reg_sp+2,RealSeg(old22));
	/* set IOPL=3 (Strike Commander), nested task set,
	   interrupts enabled, test flags cleared */
	mem_writew(SegPhys(ss)+reg_sp+4,0x7202);
	// Free memory owned by process
	if (!tsr) DOS_FreeProcessMemory(pspseg);
	DOS_UpdatePSPName();

	if ((!(CPU_AutoDetermineMode>>CPU_AUTODETERMINE_SHIFT)) || (cpu.pmode)) return;

	CPU_AutoDetermineMode>>=CPU_AUTODETERMINE_SHIFT;
	if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) {
		CPU_CycleAutoAdjust=false;
		CPU_CycleLeft=0;
		CPU_Cycles=0;
		CPU_CycleMax=CPU_OldCycleMax;
		GFX_SetTitle(CPU_OldCycleMax,-1,false);
	} else {
		GFX_SetTitle(-1,-1,false);
	}
#if (C_DYNAMIC_X86) || (C_DYNREC)
	if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CORE) {
		cpudecoder=&CPU_Core_Normal_Run;
		CPU_CycleLeft=0;
		CPU_Cycles=0;
	}
#endif

	return;
}

static bool MakeEnv(char * name,Bit16u * segment) {
	/* If segment to copy environment is 0 copy the caller's environment */
	DOS_PSP psp(dos.psp());
	PhysPt envread,envwrite;
	Bit16u envsize=1;
	bool parentenv=true;

	if (*segment==0) {
		if (!psp.GetEnvironment()) parentenv=false;				//environment seg=0
		envread=PhysMake(psp.GetEnvironment(),0);
	} else {
		if (!*segment) parentenv=false;						//environment seg=0
		envread=PhysMake(*segment,0);
	}

	if (parentenv) {
		for (envsize=0; ;envsize++) {
			if (envsize>=MAXENV - ENV_KEEPFREE) {
				DOS_SetError(DOSERR_ENVIRONMENT_INVALID);
				return false;
			}
			if (mem_readw(envread+envsize)==0) break;
		}
		envsize += 2;									/* account for trailing \0\0 */
	}
	Bit16u size=long2para(envsize+ENV_KEEPFREE);
	if (!DOS_AllocateMemory(segment,&size)) return false;
	envwrite=PhysMake(*segment,0);
	if (parentenv) {
		MEM_BlockCopy(envwrite,envread,envsize);
//		mem_memcpy(envwrite,envread,envsize);
		envwrite+=envsize;
	} else {
		mem_writeb(envwrite++,0);
	}
	mem_writew(envwrite,1);
	envwrite+=2;
	char namebuf[DOS_PATHLENGTH];
	if (DOS_Canonicalize(name,namebuf)) {
		MEM_BlockWrite(envwrite,namebuf,(Bitu)(strlen(namebuf)+1));
		return true;
	} else return false;
}

bool DOS_NewPSP(Bit16u segment, Bit16u size) {
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	Bit16u parent_psp_seg=psp.GetParent();
	DOS_PSP psp_parent(parent_psp_seg);
	psp.CopyFileTable(&psp_parent,false);
	// copy command line as well (Kings Quest AGI -cga switch)
	psp.SetCommandTail(RealMake(parent_psp_seg,0x80));
	return true;
}

bool DOS_ChildPSP(Bit16u segment, Bit16u size) {
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	Bit16u parent_psp_seg = psp.GetParent();
	DOS_PSP psp_parent(parent_psp_seg);
	psp.CopyFileTable(&psp_parent,true);
	psp.SetCommandTail(RealMake(parent_psp_seg,0x80));
	psp.SetFCB1(RealMake(parent_psp_seg,0x5c));
	psp.SetFCB2(RealMake(parent_psp_seg,0x6c));
	psp.SetEnvironment(psp_parent.GetEnvironment());
	psp.SetSize(size);
	return true;
}

static void SetupPSP(Bit16u pspseg,Bit16u memsize,Bit16u envseg) {
	/* Fix the PSP for psp and environment MCB's */
	DOS_MCB mcb((Bit16u)(pspseg-1));
	mcb.SetPSPSeg(pspseg);
	mcb.SetPt((Bit16u)(envseg-1));
	mcb.SetPSPSeg(pspseg);

	DOS_PSP psp(pspseg);
	psp.MakeNew(memsize);
	psp.SetEnvironment(envseg);

	/* Copy file handles */
	DOS_PSP oldpsp(dos.psp());
	psp.CopyFileTable(&oldpsp,true);

}

static void SetupCMDLine(Bit16u pspseg,DOS_ParamBlock & block) {
	DOS_PSP psp(pspseg);
	// if cmdtail==0 it will inited as empty in SetCommandTail
	psp.SetCommandTail(block.exec.cmdtail);
}

bool DOS_Execute(char * name,PhysPt block_pt,Bit8u flags) {
	EXE_Header head;Bitu i;
	Bit16u fhandle;Bit16u len;Bit32u pos;
	Bit16u pspseg,envseg,loadseg,memsize,readsize;
	PhysPt loadaddress;RealPt relocpt;
	Bitu headersize=0,imagesize=0;
	DOS_ParamBlock block(block_pt);

	block.LoadData();
	//Remove the loadhigh flag for the moment!
	if(flags&0x80) LOG(LOG_EXEC,LOG_ERROR)("using loadhigh flag!!!!!. dropping it");
	flags &= 0x7f;
	if (flags!=LOADNGO && flags!=OVERLAY && flags!=LOAD) {
		DOS_SetError(DOSERR_FORMAT_INVALID);
		return false;
//		E_Exit("DOS:Not supported execute mode %d for file %s",flags,name);
	}
	/* Check for EXE or COM File */
	bool iscom=false;
	if (!DOS_OpenFile(name,OPEN_READ,&fhandle)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	len=sizeof(EXE_Header);
	if (!DOS_ReadFile(fhandle,(Bit8u *)&head,&len)) {
		DOS_CloseFile(fhandle);
		return false;
	}
	if (len<sizeof(EXE_Header)) {
		if (len==0) {
			/* Prevent executing zero byte files */
			DOS_SetError(DOSERR_ACCESS_DENIED);
			DOS_CloseFile(fhandle);
			return false;
		}
		/* Otherwise must be a .com file */
		iscom=true;
	} else {
		/* Convert the header to correct endian, i hope this works */
		HostPt endian=(HostPt)&head;
		for (i=0;i<sizeof(EXE_Header)/2;i++) {
			*((Bit16u *)endian)=host_readw(endian);
			endian+=2;
		}
		if ((head.signature!=MAGIC1) && (head.signature!=MAGIC2)) iscom=true;
		else {
			if(head.pages & ~0x07ff) /* 1 MB dos maximum address limit. Fixes TC3 IDE (kippesoep) */
				LOG(LOG_EXEC,LOG_NORMAL)("Weird header: head.pages > 1 MB");
			head.pages&=0x07ff;
			headersize = head.headersize*16;
			imagesize = head.pages*512-headersize; 
			if (imagesize+headersize<512) imagesize = 512-headersize;
		}
	}
	Bit8u * loadbuf=(Bit8u *)new Bit8u[0x10000];
	if (flags!=OVERLAY) {
		/* Create an environment block */
		envseg=block.exec.envseg;
		if (!MakeEnv(name,&envseg)) {
			DOS_CloseFile(fhandle);
			return false;
		}
		/* Get Memory */		
		Bit16u minsize,maxsize;Bit16u maxfree=0xffff;DOS_AllocateMemory(&pspseg,&maxfree);
		if (iscom) {
			minsize=0x1000;maxsize=0xffff;
			if (machine==MCH_PCJR) {
				/* try to load file into memory below 96k */ 
				pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
				Bit16u dataread=0x1800;
				DOS_ReadFile(fhandle,loadbuf,&dataread);
				if (dataread<0x1800) maxsize=dataread;
				if (minsize>maxsize) minsize=maxsize;
			}
		} else {	/* Exe size calculated from header */
			minsize=long2para(imagesize+(head.minmemory<<4)+256);
			if (head.maxmemory!=0) maxsize=long2para(imagesize+(head.maxmemory<<4)+256);
			else maxsize=0xffff;
		}
		if (maxfree<minsize) {
			if (iscom) {
				/* Reduce minimum of needed memory size to filesize */
				pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
				Bit16u dataread=0xf800;
				DOS_ReadFile(fhandle,loadbuf,&dataread);
				if (dataread<0xf800) minsize=((dataread+0x10)>>4)+0x20;
			}
			if (maxfree<minsize) {
				DOS_CloseFile(fhandle);
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				DOS_FreeMemory(envseg);
				return false;
			}
		}
		if (maxfree<maxsize) memsize=maxfree;
		else memsize=maxsize;
		if (!DOS_AllocateMemory(&pspseg,&memsize)) E_Exit("DOS:Exec error in memory");
		if (iscom && (machine==MCH_PCJR) && (pspseg<0x2000)) {
			maxsize=0xffff;
			/* resize to full extent of memory block */
			DOS_ResizeMemory(pspseg,&maxsize);
			/* now try to lock out memory above segment 0x2000 */
			if ((real_readb(0x2000,0)==0x5a) && (real_readw(0x2000,1)==0) && (real_readw(0x2000,3)==0x7ffe)) {
				/* MCB after PCJr graphics memory region is still free */
				if (pspseg+maxsize==0x17ff) {
					DOS_MCB cmcb((Bit16u)(pspseg-1));
					cmcb.SetType(0x5a);		// last block
				}
			}
		}
		loadseg=pspseg+16;
		if (!iscom) {
			/* Check if requested to load program into upper part of allocated memory */
			if ((head.minmemory == 0) && (head.maxmemory == 0))
				loadseg = (Bit16u)(((pspseg+memsize)*0x10-imagesize)/0x10);
		}
	} else loadseg=block.overlay.loadseg;
	/* Load the executable */
	loadaddress=PhysMake(loadseg,0);

	if (iscom) {	/* COM Load 64k - 256 bytes max */
		pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
		readsize=0xffff-256;
		DOS_ReadFile(fhandle,loadbuf,&readsize);
		MEM_BlockWrite(loadaddress,loadbuf,readsize);
	} else {	/* EXE Load in 32kb blocks and then relocate */
		pos=headersize;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
		while (imagesize>0x7FFF) {
			readsize=0x8000;DOS_ReadFile(fhandle,loadbuf,&readsize);
			MEM_BlockWrite(loadaddress,loadbuf,readsize);
//			if (readsize!=0x8000) LOG(LOG_EXEC,LOG_NORMAL)("Illegal header");
			loadaddress+=0x8000;imagesize-=0x8000;
		}
		if (imagesize>0) {
			readsize=(Bit16u)imagesize;DOS_ReadFile(fhandle,loadbuf,&readsize);
			MEM_BlockWrite(loadaddress,loadbuf,readsize);
//			if (readsize!=imagesize) LOG(LOG_EXEC,LOG_NORMAL)("Illegal header");
		}
		/* Relocate the exe image */
		Bit16u relocate;
		if (flags==OVERLAY) relocate=block.overlay.relocation;
		else relocate=loadseg;
		pos=head.reloctable;DOS_SeekFile(fhandle,&pos,0);
		for (i=0;i<head.relocations;i++) {
			readsize=4;DOS_ReadFile(fhandle,(Bit8u *)&relocpt,&readsize);
			relocpt=host_readd((HostPt)&relocpt);		//Endianize
			PhysPt address=PhysMake(RealSeg(relocpt)+loadseg,RealOff(relocpt));
			mem_writew(address,mem_readw(address)+relocate);
		}
	}
	delete[] loadbuf;
	DOS_CloseFile(fhandle);

	/* Setup a psp */
	if (flags!=OVERLAY) {
		// Create psp after closing exe, to avoid dead file handle of exe in copied psp
		SetupPSP(pspseg,memsize,envseg);
		SetupCMDLine(pspseg,block);
	};
	CALLBACK_SCF(false);		/* Carry flag cleared for caller if successfull */
	if (flags==OVERLAY) return true;			/* Everything done for overlays */
	RealPt csip,sssp;
	if (iscom) {
		csip=RealMake(pspseg,0x100);
		sssp=RealMake(pspseg,0xfffe);
		mem_writew(PhysMake(pspseg,0xfffe),0);
	} else {
		csip=RealMake(loadseg+head.initCS,head.initIP);
		sssp=RealMake(loadseg+head.initSS,head.initSP);
		if (head.initSP<4) LOG(LOG_EXEC,LOG_ERROR)("stack underflow/wrap at EXEC");
	}

	if (flags==LOAD) {
		SaveRegisters();
		DOS_PSP callpsp(dos.psp());
		/* Save the SS:SP on the PSP of calling program */
		callpsp.SetStack(RealMakeSeg(ss,reg_sp));
		reg_sp+=18;
		/* Switch the psp's */
		dos.psp(pspseg);
		DOS_PSP newpsp(dos.psp());
		dos.dta(RealMake(newpsp.GetSegment(),0x80));
		/* First word on the stack is the value ax should contain on startup */
		real_writew(RealSeg(sssp-2),RealOff(sssp-2),0xffff);
		block.exec.initsssp = sssp-2;
		block.exec.initcsip = csip;
		block.SaveData();
		return true;
	}

	if (flags==LOADNGO) {
		if ((reg_sp>0xfffe) || (reg_sp<18)) LOG(LOG_EXEC,LOG_ERROR)("stack underflow/wrap at EXEC");
		/* Get Caller's program CS:IP of the stack and set termination address to that */
		RealSetVec(0x22,RealMake(mem_readw(SegPhys(ss)+reg_sp+2),mem_readw(SegPhys(ss)+reg_sp)));
		SaveRegisters();
		DOS_PSP callpsp(dos.psp());
		/* Save the SS:SP on the PSP of calling program */
		callpsp.SetStack(RealMakeSeg(ss,reg_sp));
		/* Switch the psp's and set new DTA */
		dos.psp(pspseg);
		DOS_PSP newpsp(dos.psp());
		dos.dta(RealMake(newpsp.GetSegment(),0x80));
		/* save vectors */
		newpsp.SaveVectors();
		/* copy fcbs */
		newpsp.SetFCB1(block.exec.fcb1);
		newpsp.SetFCB2(block.exec.fcb2);
		/* Set the stack for new program */
		SegSet16(ss,RealSeg(sssp));reg_sp=RealOff(sssp);
		/* Add some flags and CS:IP on the stack for the IRET */
		CPU_Push16(RealSeg(csip));
		CPU_Push16(RealOff(csip));
		/* DOS starts programs with a RETF, so critical flags
		 * should not be modified (IOPL in v86 mode);
		 * interrupt flag is set explicitly, test flags cleared */
		reg_flags=(reg_flags&(~FMASK_TEST))|FLAG_IF;
		//Jump to retf so that we only need to store cs:ip on the stack
		reg_ip++;
		/* Setup the rest of the registers */
		reg_ax=reg_bx=0;reg_cx=0xff;
		reg_dx=pspseg;
		reg_si=RealOff(csip);
		reg_di=RealOff(sssp);
		reg_bp=0x91c;	/* DOS internal stack begin relict */
		SegSet16(ds,pspseg);SegSet16(es,pspseg);
#if C_DEBUG		
		/* Started from debug.com, then set breakpoint at start */
		DEBUG_CheckExecuteBreakpoint(RealSeg(csip),RealOff(csip));
#endif
		/* Add the filename to PSP and environment MCB's */
		char stripname[8]= { 0 };Bitu index=0;
		while (char chr=*name++) {
			switch (chr) {
			case ':':case '\\':case '/':index=0;break;
			default:if (index<8) stripname[index++]=(char)toupper(chr);
			}
		}
		index=0;
		while (index<8) {
			if (stripname[index]=='.') break;
			if (!stripname[index]) break;	
			index++;
		}
		memset(&stripname[index],0,8-index);
		DOS_MCB pspmcb(dos.psp()-1);
		pspmcb.SetFileName(stripname);
		DOS_UpdatePSPName();
		return true;
	}
	return false;
}
