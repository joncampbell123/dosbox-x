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


#include <string.h>
#include <stdlib.h>
#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "support.h"
#include "drives.h"

uint8_t sdriv[260], sattr[260], fattr;
char sname[260][LFN_NAMELENGTH+1],storect[CTBUF];
extern int lfn_filefind_handle;

struct finddata {
       uint8_t attr;
       uint8_t fres1[19];
       uint32_t mtime;
       uint32_t mdate;
       uint32_t hsize;
       uint32_t size;
       uint8_t fres2[8];
       char lname[260];
       char sname[14];
} fd;

void DOS_ParamBlock::Clear(void) {
	memset(&exec,0,sizeof(exec));
	memset(&overlay,0,sizeof(overlay));
}

void DOS_ParamBlock::LoadData(void) {
	exec.envseg=(uint16_t)sGet(sExec,envseg);
	exec.cmdtail=sGet(sExec,cmdtail);
	exec.fcb1=sGet(sExec,fcb1);
	exec.fcb2=sGet(sExec,fcb2);
	exec.initsssp=sGet(sExec,initsssp);
	exec.initcsip=sGet(sExec,initcsip);
	overlay.loadseg=(uint16_t)sGet(sOverlay,loadseg);
	overlay.relocation=(uint16_t)sGet(sOverlay,relocation);
}

void DOS_ParamBlock::SaveData(void) {
	sSave(sExec,envseg,exec.envseg);
	sSave(sExec,cmdtail,exec.cmdtail);
	sSave(sExec,fcb1,exec.fcb1);
	sSave(sExec,fcb2,exec.fcb2);
	sSave(sExec,initsssp,exec.initsssp);
	sSave(sExec,initcsip,exec.initcsip);
}

extern int maxdrive;
void DOS_InfoBlock::SetLocation(uint16_t segment) {
	seg = segment;
	pt=PhysMake(seg,0);
	/* Clear the initial Block */
	for(uint8_t i=0;i<sizeof(sDIB);i++) mem_writeb(pt+i,0xff);
	for(uint8_t i=0;i<14;i++) mem_writeb(pt+i,0);
	
	sSave(sDIB,regCXfrom5e,(uint16_t)0);
	sSave(sDIB,countLRUcache,(uint16_t)0);
	sSave(sDIB,countLRUopens,(uint16_t)0);

	sSave(sDIB,protFCBs,(uint16_t)0);
	sSave(sDIB,specialCodeSeg,(uint16_t)0);
	sSave(sDIB,joindedDrives,(uint8_t)0);
	sSave(sDIB,lastdrive,(uint8_t)maxdrive);//increase this if you add drives to cds-chain

	sSave(sDIB,diskInfoBuffer,RealMake(segment,offsetof(sDIB,diskBufferHeadPt)));
	sSave(sDIB,setverPtr,(uint32_t)0);

	sSave(sDIB,a20FixOfs,(uint16_t)0);
	sSave(sDIB,pspLastIfHMA,(uint16_t)0);
	sSave(sDIB,blockDevices,(uint8_t)0);
	
	sSave(sDIB,bootDrive,(uint8_t)0);
	sSave(sDIB,useDwordMov,(uint8_t)1);
	sSave(sDIB,extendedSize,(uint16_t)(MEM_TotalPages()*4-1024));
	sSave(sDIB,magicWord,(uint16_t)0x0001);		// dos5+

	sSave(sDIB,sharingCount,(uint16_t)0);
	sSave(sDIB,sharingDelay,(uint16_t)0);
	sSave(sDIB,ptrCONinput,(uint16_t)0);			// no unread input available
	sSave(sDIB,maxSectorLength,(uint16_t)0x200);

	sSave(sDIB,dirtyDiskBuffers,(uint16_t)0);
	sSave(sDIB,lookaheadBufPt,(uint32_t)0);
	sSave(sDIB,lookaheadBufNumber,(uint16_t)0);
	sSave(sDIB,bufferLocation,(uint8_t)0);		// buffer in base memory, no workspace
	sSave(sDIB,workspaceBuffer,(uint32_t)0);

	sSave(sDIB,minMemForExec,(uint16_t)0);
	sSave(sDIB,memAllocScanStart,(uint16_t)DOS_MEM_START);
	sSave(sDIB,startOfUMBChain,(uint16_t)0xffff);
	sSave(sDIB,chainingUMB,(uint8_t)0);

	sSave(sDIB,nulNextDriver,(uint32_t)0xffffffff);
	sSave(sDIB,nulAttributes,(uint16_t)0x8004);
	sSave(sDIB,nulStrategy,(uint16_t)0x0000);
	sSave(sDIB,nulInterrupt,(uint16_t)0x0000);
	sSave(sDIB,nulString[0],(uint8_t)0x4e);
	sSave(sDIB,nulString[1],(uint8_t)0x55);
	sSave(sDIB,nulString[2],(uint8_t)0x4c);
	sSave(sDIB,nulString[3],(uint8_t)0x20);
	sSave(sDIB,nulString[4],(uint8_t)0x20);
	sSave(sDIB,nulString[5],(uint8_t)0x20);
	sSave(sDIB,nulString[6],(uint8_t)0x20);
	sSave(sDIB,nulString[7],(uint8_t)0x20);

	/* Create a fake SFT, so programs think there are 100 file handles */
	uint16_t sftOffset=offsetof(sDIB,firstFileTable)+0xa2;
	sSave(sDIB,firstFileTable,RealMake(segment,sftOffset));
	real_writed(segment,sftOffset+0x00,RealMake(segment+0x26,0));	//Next File Table
	real_writew(segment,sftOffset+0x04,100);		//File Table supports 100 files
	real_writed(segment+0x26,0x00,0xffffffff);		//Last File Table
	real_writew(segment+0x26,0x04,100);				//File Table supports 100 files
}

void DOS_InfoBlock::SetFirstMCB(uint16_t _firstmcb) {
	sSave(sDIB,firstMCB,_firstmcb); //c2woody
}

void DOS_InfoBlock::SetBuffers(uint16_t x,uint16_t y) {
	sSave(sDIB,buffers_x,x);
	sSave(sDIB,buffers_y,y);
}

void DOS_InfoBlock::SetCurDirStruct(uint32_t _curdirstruct) {
	sSave(sDIB,curDirStructure,_curdirstruct);
}

void DOS_InfoBlock::SetFCBTable(uint32_t _fcbtable) {
	sSave(sDIB,fcbTable,_fcbtable);
}

void DOS_InfoBlock::SetDeviceChainStart(uint32_t _devchain) {
	sSave(sDIB,nulNextDriver,_devchain);
}

void DOS_InfoBlock::SetDiskBufferHeadPt(uint32_t _dbheadpt) {
	sSave(sDIB,diskBufferHeadPt,_dbheadpt);
}

uint16_t DOS_InfoBlock::GetStartOfUMBChain(void) {
	return (uint16_t)sGet(sDIB,startOfUMBChain);
}

void DOS_InfoBlock::SetStartOfUMBChain(uint16_t _umbstartseg) {
	sSave(sDIB,startOfUMBChain,_umbstartseg);
}

uint8_t DOS_InfoBlock::GetUMBChainState(void) {
	return (uint8_t)sGet(sDIB,chainingUMB);
}

void DOS_InfoBlock::SetUMBChainState(uint8_t _umbchaining) {
	sSave(sDIB,chainingUMB,_umbchaining);
}

void DOS_InfoBlock::SetBlockDevices(uint8_t _count) {
	sSave(sDIB,blockDevices,_count);
}

void DOS_InfoBlock::SetFirstDPB(uint32_t _first_dpb) {
    sSave(sDIB,firstDPB,_first_dpb);
}

RealPt DOS_InfoBlock::GetPointer(void) {
	return RealMake(seg,offsetof(sDIB,firstDPB));
}

uint32_t DOS_InfoBlock::GetDeviceChain(void) {
	return sGet(sDIB,nulNextDriver);
}


/* program Segment prefix */

uint16_t DOS_PSP::rootpsp = 0;

#define CPM_MAX_SEG_SIZE	0xfffu

extern unsigned char cpm_compat_mode;

void DOS_PSP::MakeNew(uint16_t mem_size) {
	/* get previous */
//	DOS_PSP prevpsp(dos.psp());
	/* Clear it first */
	uint16_t i;
	for (i=0;i<sizeof(sPSP);i++) mem_writeb(pt+i,0);
	// Set size
	sSave(sPSP,next_seg,(unsigned int)seg+mem_size);
    /* cpm_entry is an alias for the int 30 vector (0:c0 = c:0); the offset is also the maximum program size */
    if (cpm_compat_mode == CPM_COMPAT_MSDOS2) { /* MS-DOS 2.x behavior, where offset is the memory size (for some reason) */
	    /* far call opcode */
        sSave(sPSP,far_call,0x9a);
        /* and where to call to */
        if (mem_size>CPM_MAX_SEG_SIZE) mem_size=CPM_MAX_SEG_SIZE;
        mem_size-=0x10;
        sSave(sPSP,cpm_entry,RealMake(0xc-mem_size,mem_size<<4));
    }
    else if (cpm_compat_mode == CPM_COMPAT_MSDOS5) { /* MS-DOS 5.x behavior, for whatever reason */
	    /* far call opcode */
        sSave(sPSP,far_call,0x9a);
        /* and where to call to.
         * if 64KB or more, call far to F01D:FEF0.
         * else, call far to 0000:00C0.
         * don't ask me why MS-DOS 5.0 does this, ask Microsoft. */
        if (mem_size >= CPM_MAX_SEG_SIZE)
            sSave(sPSP,cpm_entry,RealMake(0xF01D,0xFEF0));
        else
            sSave(sPSP,cpm_entry,RealMake(0x0000,0x00C0));
    }
    else if (cpm_compat_mode == CPM_COMPAT_DIRECT) {
        /* direct and to the point, though impossible to hook INT 21h without patching all PSP segments
         * which is probably why Microsoft never did this in the DOS kernel. Choosing this method
         * removes the need for the copy of INT 30h in the HMA area, and therefore opens up all 64KB of
         * HMA if you want. */
        uint32_t DOS_Get_CPM_entry_direct(void);

	    /* far call opcode */
        sSave(sPSP,far_call,0x9a);
        /* and where to call to */
        sSave(sPSP,cpm_entry,DOS_Get_CPM_entry_direct());
    }
    else { /* off */
        /* stick an INT 20h in there to make calling the CP/M entry point an immediate exit */
        /* this is NOT default, but an option for users who are absolutely certain no CP/M code is going to run in their DOS box. */
        sSave(sPSP,far_call,0xCD);      // INT 20h + NOP NOP NOP
        sSave(sPSP,cpm_entry,0x90909020);
    }
	/* Standard blocks,int 20  and int21 retf */
	sSave(sPSP,exit[0],0xcd);
	sSave(sPSP,exit[1],0x20);
	sSave(sPSP,service[0],0xcd);
	sSave(sPSP,service[1],0x21);
	sSave(sPSP,service[2],0xcb);
	/* psp and psp-parent */
	sSave(sPSP,psp_parent,dos.psp());
	sSave(sPSP,prev_psp,0xffffffff);
	sSave(sPSP,dos_version,0x0005);
	/* terminate 22,break 23,crititcal error 24 address stored */
	SaveVectors();

	/* FCBs are filled with 0 */
	// ....
	/* Init file pointer and max_files */
	sSave(sPSP,file_table,RealMake(seg,offsetof(sPSP,files)));
	sSave(sPSP,max_files,20);
	for (uint16_t ct=0;ct<20;ct++) SetFileHandle(ct,0xff);

	/* User Stack pointer */
//	if (prevpsp.GetSegment()!=0) sSave(sPSP,stack,prevpsp.GetStack());

	if (rootpsp==0) rootpsp = seg;
}

uint8_t DOS_PSP::GetFileHandle(uint16_t index) {
	if (index>=sGet(sPSP,max_files)) return 0xff;
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	return mem_readb(files+index);
}

void DOS_PSP::SetFileHandle(uint16_t index, uint8_t handle) {
	if (index<sGet(sPSP,max_files)) {
		PhysPt files=Real2Phys(sGet(sPSP,file_table));
		mem_writeb(files+index,handle);
	}
}

uint16_t DOS_PSP::FindFreeFileEntry(void) {
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	for (uint16_t i=0;i<sGet(sPSP,max_files);i++) {
		if (mem_readb(files+i)==0xff) return i;
	}	
	return 0xff;
}

uint16_t DOS_PSP::FindEntryByHandle(uint8_t handle) {
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	for (uint16_t i=0;i<sGet(sPSP,max_files);i++) {
		if (mem_readb(files+i)==handle) return i;
	}	
	return 0xFF;
}

void DOS_PSP::CopyFileTable(DOS_PSP* srcpsp,bool createchildpsp) {
	/* Copy file table from calling process */
	for (uint16_t i=0;i<20;i++) {
		uint8_t handle = srcpsp->GetFileHandle(i);
		if(createchildpsp)
		{	//copy obeying not inherit flag.(but dont duplicate them)
			bool allowCopy = true;//(handle==0) || ((handle>0) && (FindEntryByHandle(handle)==0xff));
			if((handle<DOS_FILES) && Files[handle] && !(Files[handle]->flags & DOS_NOT_INHERIT) && allowCopy)
			{   
				Files[handle]->AddRef();
				SetFileHandle(i,handle);
			}
			else
			{
				SetFileHandle(i,0xff);
			}
		}
		else
		{	//normal copy so don't mind the inheritance
			SetFileHandle(i,handle);
		}
	}
}

void DOS_PSP::CloseFiles(void) {
	for (uint16_t i=0;i<sGet(sPSP,max_files);i++) {
		DOS_CloseFile(i);
	}
}

void DOS_PSP::SaveVectors(void) {
	/* Save interrupt 22,23,24 */
	sSave(sPSP,int_22,RealGetVec(0x22));
	sSave(sPSP,int_23,RealGetVec(0x23));
	sSave(sPSP,int_24,RealGetVec(0x24));
}

void DOS_PSP::RestoreVectors(void) {
	/* Restore interrupt 22,23,24 */
	RealSetVec(0x22,sGet(sPSP,int_22));
	RealSetVec(0x23,sGet(sPSP,int_23));
	RealSetVec(0x24,sGet(sPSP,int_24));
}

void DOS_PSP::SetCommandTail(RealPt src) {
	if (src) {	// valid source
        MEM_BlockCopy(pt+offsetof(sPSP,cmdtail),Real2Phys(src),CTBUF+1);
	} else {	// empty
		sSave(sPSP,cmdtail.count,0x00);
		mem_writeb(pt+offsetof(sPSP,cmdtail.buffer),0x0d);
	}
}

void DOS_PSP::StoreCommandTail() {
	int len=(int)mem_strlen(pt+offsetof(sPSP,cmdtail.buffer));
	MEM_StrCopy(pt+offsetof(sPSP,cmdtail.buffer),storect,len>CTBUF?CTBUF:len);
}

void DOS_PSP::RestoreCommandTail() {
	mem_writeb(pt+offsetof(sPSP,cmdtail.count),strlen(storect)>0?(uint8_t)(strlen(storect)-1):0);
	MEM_BlockWrite(pt+offsetof(sPSP,cmdtail.buffer),storect,strlen(storect));
}

void DOS_PSP::SetFCB1(RealPt src) {
	if (src) MEM_BlockCopy(PhysMake(seg,offsetof(sPSP,fcb1)),Real2Phys(src),16);
}

void DOS_PSP::SetFCB2(RealPt src) {
	if (src) MEM_BlockCopy(PhysMake(seg,offsetof(sPSP,fcb2)),Real2Phys(src),16);
}

bool DOS_PSP::SetNumFiles(uint16_t fileNum) {
	//20 minimum. clipper program.
	if (fileNum < 20) fileNum = 20;
	 
	if (fileNum > 20 && ((fileNum+2) > sGet(sPSP,max_files))) {
		// Allocate needed paragraphs
		fileNum+=2;	// Add a few more files for safety
		uint16_t para = (fileNum/16)+((fileNum%16)>0);
		RealPt data	= RealMake(DOS_GetMemory(para,"SetNumFiles data"),0);
		for (uint16_t i=0; i<fileNum; i++) mem_writeb(Real2Phys(data)+i,(i<20)?GetFileHandle(i):0xFF);
		sSave(sPSP,file_table,data);
	}
	sSave(sPSP,max_files,fileNum);
	return true;
}


void DOS_DTA::SetupSearch(uint8_t _sdrive,uint8_t _sattr,char * pattern) {
	unsigned int i;

	if (lfn_filefind_handle<LFN_FILEFIND_NONE) {
		sdriv[lfn_filefind_handle]=_sdrive;
		sattr[lfn_filefind_handle]=_sattr;
		for (i=0;i<LFN_NAMELENGTH;i++) {
			if (pattern[i]==0) break;
				sname[lfn_filefind_handle][i]=pattern[i];
		}
		while (i<=LFN_NAMELENGTH) sname[lfn_filefind_handle][i++]=0;
	}
    for (i=0;i<11;i++) mem_writeb(pt+offsetof(sDTA,spname)+i,0);
	
	sSave(sDTA,sdrive,_sdrive);
	sSave(sDTA,sattr,_sattr);
    const char* find_ext;
    find_ext=strchr(pattern,'.');
    if (find_ext) {
        Bitu size=(Bitu)(find_ext-pattern);
		if (size>8) size=8;
			MEM_BlockWrite(pt+offsetof(sDTA,spname),pattern,size);
			find_ext++;
			MEM_BlockWrite(pt+offsetof(sDTA,spext),find_ext,(strlen(find_ext)>3) ? 3 : (Bitu)strlen(find_ext));
	} else {
		MEM_BlockWrite(pt+offsetof(sDTA,spname),pattern,(strlen(pattern) > 8) ? 8 : (Bitu)strlen(pattern));
	}
}

void DOS_DTA::SetResult(const char * _name, const char * _lname, uint32_t _size,uint16_t _date,uint16_t _time,uint8_t _attr) {
	fd.hsize=0;
	fd.size=_size;
	fd.mdate=_date;
	fd.mtime=_time;
	fd.attr=_attr;
	strcpy(fd.lname,_lname);
	strcpy(fd.sname,_name);
	if (!strcmp(fd.lname,fd.sname)) fd.sname[0]=0;
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) {
		MEM_BlockWrite(pt+offsetof(sDTA,name),(void *)_name,strlen(_name)+1);
		sSave(sDTA,size,_size);
		sSave(sDTA,date,_date);
		sSave(sDTA,time,_time);
		sSave(sDTA,attr,_attr);
	}
}


void DOS_DTA::GetResult(char * _name, char * _lname,uint32_t & _size,uint16_t & _date,uint16_t & _time,uint8_t & _attr) {
	strcpy(_lname,fd.lname);
	if (fd.sname[0]!=0) strcpy(_name,fd.sname);
	else if (strlen(fd.lname)<DOS_NAMELENGTH_ASCII) strcpy(_name,fd.lname);
	_size = fd.size;
	_date = fd.mdate;
	_time = fd.mtime;
	_attr = fd.attr;
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX) {
		MEM_BlockRead(pt+offsetof(sDTA,name),_name,DOS_NAMELENGTH_ASCII);
		_size=sGet(sDTA,size);
		_date=(uint16_t)sGet(sDTA,date);
		_time=(uint16_t)sGet(sDTA,time);
		_attr=(uint8_t)sGet(sDTA,attr);
	}
}

int DOS_DTA::GetFindData(int fmt, char * fdstr, int *c) {
	if (fmt==1)
		sprintf(fdstr,"%-1s%-19s%-2s%-2s%-4s%-4s%-4s%-8s%-260s%-14s",(char*)&fd.attr,(char*)&fd.fres1,(char*)&fd.mtime,(char*)&fd.mdate,(char*)&fd.mtime,(char*)&fd.hsize,(char*)&fd.size,(char*)&fd.fres2,(char*)&fd.lname,(char*)&fd.sname);
	else
		sprintf(fdstr,"%-1s%-19s%-4s%-4s%-4s%-4s%-8s%-260s%-14s",(char*)&fd.attr,(char*)&fd.fres1,(char*)&fd.mtime,(char*)&fd.mdate,(char*)&fd.hsize,(char*)&fd.size,(char*)&fd.fres2,(char*)&fd.lname,(char*)&fd.sname);
	for (int i=0;i<4;i++) fdstr[28+i]=0;
    fdstr[32]=(char)fd.size%256;
    fdstr[33]=(char)((fd.size%65536)/256);
    fdstr[34]=(char)((fd.size%16777216)/65536);
    fdstr[35]=(char)(fd.size/16777216);
    fdstr[44+strlen(fd.lname)]=0;
    fdstr[304+strlen(fd.sname)]=0;
	if (!strcmp(fd.sname,"?")&&strlen(fd.lname))
		*c=2;
	else
		*c=!strchr(fd.sname,'?')&&strchr(fd.lname,'?')?1:0;
    return (sizeof(fd));
}

uint8_t DOS_DTA::GetSearchDrive(void) {
	return lfn_filefind_handle>=LFN_FILEFIND_MAX?(uint8_t)sGet(sDTA,sdrive):sdriv[lfn_filefind_handle];
}

void DOS_DTA::GetSearchParams(uint8_t & attr,char * pattern, bool lfn) {
    if (lfn&&lfn_filefind_handle<LFN_FILEFIND_NONE) {
		attr=sattr[lfn_filefind_handle];
        memcpy(pattern,sname[lfn_filefind_handle],LFN_NAMELENGTH);
        pattern[LFN_NAMELENGTH]=0;
    } else {
		attr=(uint8_t)sGet(sDTA,sattr);
        char temp[11];
        MEM_BlockRead(pt+offsetof(sDTA,spname),temp,11);
        for (int i=0;i<13;i++) pattern[i]=0;
            memcpy(pattern,temp,8);
            pattern[strlen(pattern)]='.';
            memcpy(&pattern[strlen(pattern)],&temp[8],3);
    }
}

DOS_FCB::DOS_FCB(uint16_t seg,uint16_t off,bool allow_extended) { 
	SetPt(seg,off); 
	real_pt=pt;
	if (allow_extended) {
		if (sGet(sFCB,drive)==0xff) {
			pt+=7;
			extended=true;
		}
	}
}

bool DOS_FCB::Extended(void) {
	return extended;
}

void DOS_FCB::Create(bool _extended) {
	uint8_t fill;
	if (_extended) fill=33+7;
	else fill=33;
	uint8_t i;
	for (i=0;i<fill;i++) mem_writeb(real_pt+i,0);
	pt=real_pt;
	if (_extended) {
		mem_writeb(real_pt,0xff);
		pt+=7;
		extended=true;
	} else extended=false;
}

void DOS_FCB::SetName(uint8_t _drive, const char* _fname, const char* _ext) {
	sSave(sFCB,drive,_drive);
	MEM_BlockWrite(pt+offsetof(sFCB,filename),_fname,8);
	MEM_BlockWrite(pt+offsetof(sFCB,ext),_ext,3);
}

void DOS_FCB::SetSizeDateTime(uint32_t _size,uint16_t _date,uint16_t _time) {
	sSave(sFCB,filesize,_size);
	sSave(sFCB,date,_date);
	sSave(sFCB,time,_time);
}

void DOS_FCB::GetSizeDateTime(uint32_t & _size,uint16_t & _date,uint16_t & _time) {
	_size=sGet(sFCB,filesize);
	_date=(uint16_t)sGet(sFCB,date);
	_time=(uint16_t)sGet(sFCB,time);
}

void DOS_FCB::GetRecord(uint16_t & _cur_block,uint8_t & _cur_rec) {
	_cur_block=(uint16_t)sGet(sFCB,cur_block);
	_cur_rec=(uint8_t)sGet(sFCB,cur_rec);

}

void DOS_FCB::SetRecord(uint16_t _cur_block,uint8_t _cur_rec) {
	sSave(sFCB,cur_block,_cur_block);
	sSave(sFCB,cur_rec,_cur_rec);
}

void DOS_FCB::GetSeqData(uint8_t & _fhandle,uint16_t & _rec_size) {
	_fhandle=(uint8_t)sGet(sFCB,file_handle);
	_rec_size=(uint16_t)sGet(sFCB,rec_size);
}

void DOS_FCB::SetSeqData(uint8_t _fhandle,uint16_t _rec_size) {
	sSave(sFCB,file_handle,_fhandle);
	sSave(sFCB,rec_size,_rec_size);
}

void DOS_FCB::GetRandom(uint32_t & _random) {
	_random=sGet(sFCB,rndm);
}

void DOS_FCB::SetRandom(uint32_t _random) {
	sSave(sFCB,rndm,_random);
}

void DOS_FCB::ClearBlockRecsize(void) {
	sSave(sFCB,cur_block,0);
	sSave(sFCB,rec_size,0);
}

void DOS_FCB::FileOpen(uint8_t _fhandle) {
	sSave(sFCB,drive,GetDrive()+1u);
	sSave(sFCB,file_handle,_fhandle);
	sSave(sFCB,cur_block,0u);
	sSave(sFCB,rec_size,128u);
//	sSave(sFCB,rndm,0); // breaks Jewels of darkness. 
	uint32_t size = 0;
	Files[_fhandle]->Seek(&size,DOS_SEEK_END);
	sSave(sFCB,filesize,size);
	size = 0;
	Files[_fhandle]->Seek(&size,DOS_SEEK_SET);
	sSave(sFCB,time,Files[_fhandle]->time);
	sSave(sFCB,date,Files[_fhandle]->date);
}

bool DOS_FCB::Valid() {
	//Very simple check for Oubliette
	if(sGet(sFCB,filename[0]) == 0 && sGet(sFCB,file_handle) == 0) return false;
	return true;
}

void DOS_FCB::FileClose(uint8_t & _fhandle) {
	_fhandle=(uint8_t)sGet(sFCB,file_handle);
	sSave(sFCB,file_handle,0xff);
}

uint8_t DOS_FCB::GetDrive(void) {
	uint8_t drive=(uint8_t)sGet(sFCB,drive);
	if (!drive) return  DOS_GetDefaultDrive();
	else return drive-1;
}

void DOS_FCB::GetVolumeName(char * fillname) {
	MEM_BlockRead(pt+offsetof(sFCB,filename),&fillname[0],8);
	MEM_BlockRead(pt+offsetof(sFCB,ext),&fillname[8],3);
    fillname[11]=0;
}

void DOS_FCB::GetName(char * fillname) {
	fillname[0]=GetDrive()+'A';
	fillname[1]=':';
	MEM_BlockRead(pt+offsetof(sFCB,filename),&fillname[2],8);
	fillname[10]='.';
	MEM_BlockRead(pt+offsetof(sFCB,ext),&fillname[11],3);
	fillname[14]=0;
}

void DOS_FCB::GetAttr(uint8_t& attr) {
	if(extended) attr=mem_readb(pt - 1);
}

void DOS_FCB::SetAttr(uint8_t attr) {
	if(extended) mem_writeb(pt - 1,attr);
}

void DOS_FCB::SetResult(uint32_t size,uint16_t date,uint16_t time,uint8_t attr) {
	mem_writed(pt + 0x1d,size);
	mem_writew(pt + 0x19,date);
	mem_writew(pt + 0x17,time);
	mem_writeb(pt + 0x0c,attr);
}

void DOS_SDA::Init() {
	/* Clear */
	for(uint8_t i=0;i<sizeof(sSDA);i++) mem_writeb(pt+i,0x00);
	sSave(sSDA,drive_crit_error,0xff);   
}
