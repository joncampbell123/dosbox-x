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
#if C_DEBUG

#include <string.h>
#include <list>
#include <vector>
#include <ctype.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
using namespace std;

#include "debug.h"
#include "cross.h" //snprintf
#include "cpu.h"
#include "fpu.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "mapper.h"
#include "cpu.h"
#include "pc98_gdc.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "timer.h"
#include "paging.h"
#include "support.h"
#include "shell.h"
#include "programs.h"
#include "debug_inc.h"
#include "../cpu/lazyflags.h"
#include "keyboard.h"
#include "setup.h"
#include "control.h"

#ifdef WIN32
void WIN32_Console();
#else
#include <termios.h>
#include <unistd.h>
static struct termios consolesettings;
#endif
int old_cursor_state;

const char *egc_fgc_modes[4] = {
    "pattern",
    "background",
    "foreground",
    "(invalid)",
};

bool pc98_pegc_linear_framebuffer_enabled(void);
void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);

extern bool                 dos_kernel_disabled;
extern bool                 is_paused;
extern bool                 pc98_crt_mode;
extern uint8_t              GDC_display_plane;
extern uint8_t              GDC_display_plane_pending;
extern bool                 pc98_256kb_boundary;
extern bool                         gdc_5mhz_mode;
extern bool                         enable_pc98_egc;
extern bool                         enable_pc98_grcg;
extern bool                         enable_pc98_16color;
extern bool                         enable_pc98_256color;
extern bool                         enable_pc98_256color_planar;
extern bool                         enable_pc98_188usermod;
extern bool                         GDC_vsync_interrupt;
extern bool                         pc98_graphics_hide_odd_raster_200line;
extern bool                         pc98_attr4_graphic;
extern bool                         egc_enable_enable;
extern uint8_t                      pc98_gdc_tile_counter;
extern uint8_t                      pc98_gdc_modereg;
extern egc_quad                     pc98_gdc_tiles;

extern uint16_t                     pc98_egc_raw_values[8];

extern uint16_t                     a1_font_load_addr;
extern uint8_t                      a1_font_char_offset;

extern egc_quad             pc98_egc_bgcm;
extern egc_quad             pc98_egc_fgcm;

extern uint8_t                     pc98_egc_access;
extern uint8_t                     pc98_egc_srcmask[2]; /* host given (Neko: egc.srcmask) */
extern uint8_t                     pc98_egc_maskef[2]; /* effective (Neko: egc.mask2) */
extern uint8_t                     pc98_egc_mask[2]; /* host given (Neko: egc.mask) */

extern uint8_t                     pc98_egc_fgc;
extern uint8_t                     pc98_egc_lead_plane;
extern uint8_t                     pc98_egc_compare_lead;
extern uint8_t                     pc98_egc_lightsource;
extern uint8_t                     pc98_egc_shiftinput;
extern uint8_t                     pc98_egc_regload;
extern uint8_t                     pc98_egc_rop;
extern uint8_t                     pc98_egc_foreground_color;
extern uint8_t                     pc98_egc_background_color;

extern bool                        pc98_egc_shift_descend;
extern uint8_t                     pc98_egc_shift_destbit;
extern uint8_t                     pc98_egc_shift_srcbit;
extern uint16_t                    pc98_egc_shift_length;

extern unsigned char        pc98_text_first_row_scanline_start;  /* port 70h */
extern unsigned char        pc98_text_first_row_scanline_end;    /* port 72h */
extern unsigned char        pc98_text_row_scanline_blank_at;     /* port 74h */
extern unsigned char        pc98_text_row_scroll_lines;          /* port 76h */
extern unsigned char        pc98_text_row_scroll_count_start;    /* port 78h */
extern unsigned char        pc98_text_row_scroll_num_lines;      /* port 7Ah */

extern bool logBuffSuppressConsole;
extern bool logBuffSuppressConsoleNeedUpdate;

// Forwards
static void DrawCode(void);
static void DrawInput(void);
static void DEBUG_RaiseTimerIrq(void);
static void SaveMemory(uint16_t seg, uint32_t ofs1, uint32_t num);
static void SaveMemoryBin(uint16_t seg, uint32_t ofs1, uint32_t num);
static void LogMCBS(void);
static void LogGDT(void);
static void LogLDT(void);
static void LogIDT(void);
static void LogXMS(void);
static void LogEMS(void);
static void LogFNKEY(void);
static void LogPages(char* selname);
static void LogCPUInfo(void);
static void OutputVecTable(char* filename);
static void DrawVariables(void);
static void LogDOSKernMem(void);
static void LogBIOSMem(void);

bool inhibit_int_breakpoint=false;

void DEBUG_DrawInput(void) {
    DrawInput();
}

void DEBUG_BeginPagedContent(void);
void DEBUG_EndPagedContent(void);
Bitu MEM_PageMaskActive(void);
uint32_t MEM_get_address_bits();
Bitu MEM_TotalPages(void);
Bitu MEM_PageMask(void);

static void LogEMUMachine(void) {
    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("Emulator machine:");

    {
        const char *m = "?";
        const char *cardName = "";

        switch (machine) {
            case MCH_HERC:      m="Hercules";   break;
            case MCH_CGA:       m="CGA";        break;
            case MCH_TANDY:     m="Tandy";      break;
            case MCH_PCJR:      m="PCjr";       break;
            case MCH_EGA:       m="EGA";        break;
            case MCH_VGA:       m="VGA";        break;
            case MCH_AMSTRAD:   m="Amstrad";    break;
            case MCH_PC98:      m="PC-98";      break;
            case MCH_FM_TOWNS:  m="FM Towns";   break;
            case MCH_MCGA:      m="MCGA";       break;
            case MCH_MDA:       m="MDA";        break;
            default:            break;
        }

        switch (svgaCard) {
            case SVGA_None:             cardName ="";                break;
            case SVGA_S3Trio:           cardName ="S3 Trio";         break;
            case SVGA_TsengET4K:        cardName ="Tseng ET4000";    break;
            case SVGA_TsengET3K:        cardName ="Tseng ET3000";    break;
            case SVGA_ParadisePVGA1A:   cardName ="Paradise PVGA1A"; break;
        }

        DEBUG_ShowMsg("Machine: %s %s",m, cardName);
    }

    DEBUG_EndPagedContent();
}

static void LogEMUMem(void) {
    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("Emulator memory:");
    DEBUG_ShowMsg("A20 gate:                    %s",MEM_A20_Enabled() ? "ON" : "OFF");
    DEBUG_ShowMsg("CPU address bits:            %u",(unsigned int)MEM_get_address_bits());
    DEBUG_ShowMsg("CPU address mask:            0x%lx",((unsigned long)MEM_PageMask() << 12UL) | 0xFFFUL);
    DEBUG_ShowMsg("CPU address mask current:    0x%lx",((unsigned long)MEM_PageMaskActive() << 12UL) | 0xFFFUL);
    DEBUG_ShowMsg("Memory reported size:        %lu bytes",(unsigned long)MEM_TotalPages() << 12UL);

    DEBUG_EndPagedContent();
}

bool XMS_Active(void);

Bitu XMS_GetTotalHandles(void);
bool XMS_GetHandleInfo(Bitu &phys_location,Bitu &size,Bitu &lockcount,bool &free,Bitu handle);

LoopHandler *old_loop = NULL;

char* AnalyzeInstruction(char* inst, bool saveSelector);
uint32_t GetHexValue(char* const str, char* &hex,bool *parsed=NULL);
void SkipSpace(char*& hex);

#if 0
class DebugPageHandler : public PageHandler {
public:
	uint8_t readb(PhysPt /*addr*/) {
	}
	uint16_t readw(PhysPt /*addr*/) {
	}
	uint32_t readd(PhysPt /*addr*/) {
	}
	void writeb(PhysPt /*addr*/,uint8_t /*val*/) {
	}
	void writew(PhysPt /*addr*/,uint16_t /*val*/) {
	}
	void writed(PhysPt /*addr*/,uint32_t /*val*/) {
	}
};
#endif


class DEBUG;

//DEBUG*	pDebugcom	= 0;
bool	exitLoop	= false;


// Heavy Debugging Vars for logging
#if C_HEAVY_DEBUG
static ofstream 	cpuLogFile;
static bool		cpuLog			= false;
static int		cpuLogCounter	= 0;
static int		cpuLogType		= 1;	// log detail
static bool zeroProtect = false;
bool	logHeavy	= false;
#endif



static struct  {
	uint32_t eax,ebx,ecx,edx,esi,edi,ebp,esp,eip;
} oldregs;

static char curSelectorName[3] = { 0,0,0 };

static Segment oldsegs[6];
static Bitu oldflags,oldcpucpl;
DBGBlock dbg;
extern Bitu cycle_count;
static bool debugging = false;
static bool debug_running = false;
static bool check_rescroll = false;

static FPU_rec oldfpu;

bool IsDebuggerActive(void) {
    return debugging;
}

bool IsDebuggerRunwatch(void) {
    return debug_running;
}

static void SetColor(Bitu test) {
	if (test) {
		if (has_colors()) { wattrset(dbg.win_reg,COLOR_PAIR(PAIR_BYELLOW_BLACK));}
	} else {
		if (has_colors()) { wattrset(dbg.win_reg,0);}
	}
}

#define MAXCMDLEN 254
struct SCodeViewData {
	int     cursorPos;
	uint16_t  firstInstSize;
	uint16_t  useCS;
	uint32_t  useEIPlast, useEIPmid;
	uint32_t  useEIP;
	uint16_t  cursorSeg;
	uint32_t  cursorOfs;
	bool    ovrMode;
	char    inputStr[MAXCMDLEN+1];
	char    suspInputStr[MAXCMDLEN+1];
	int     inputPos;
} codeViewData;

static uint16_t  dataSeg;
static uint32_t  dataOfs;
static bool    showExtend = true;
static bool    showPrintable = true;

static void ClearInputLine(void) {
	codeViewData.inputStr[0] = 0;
	codeViewData.inputPos = 0;
}

// History stuff
#define MAX_HIST_BUFFER 50
static list<string> histBuff;
static list<string>::iterator histBuffPos = histBuff.end();

/***********/
/* Helpers */
/***********/

/* dest here must be a string with a minimum length of 11. */
static char* F80ToString(int regIndex, char* dest) {
#if C_FPU_X86
	snprintf(dest, 11, "%08.2Lf", reinterpret_cast<long double&>(fpu.p_regs[regIndex]));
#elif defined(HAS_LONG_DOUBLE)
	snprintf(dest, 11, "%08.2Lf", fpu.regs_80[regIndex].v);
#else
	snprintf(dest, 11, "%08.2f", fpu.regs[regIndex].d);
#endif
	
	return dest;
}

static bool F80TestUpdate(int regIndex) {
	if(fpu.top != oldfpu.top) { /* If the top changed then all registers rotated places, thus updated. */
		return true;
	}
	
#if C_FPU_X86
	return !(fpu.p_regs[regIndex].m1 == oldfpu.p_regs[regIndex].m1 && fpu.p_regs[regIndex].m2 == oldfpu.p_regs[regIndex].m2 && fpu.p_regs[regIndex].m3 == oldfpu.p_regs[regIndex].m3);
#elif defined(HAS_LONG_DOUBLE)
	return fpu.regs_80[regIndex].v != oldfpu.regs_80[regIndex].v; /* I'm certain that strict float equality can be used here. */
#else
	return fpu.regs[regIndex].d != oldfpu.regs[regIndex].d;
#endif
}

static const uint64_t mem_no_address = (uint64_t)(~0ULL);

uint64_t LinMakeProt(uint16_t selector, uint32_t offset)
{
	Descriptor desc;

    if (cpu.gdt.GetDescriptor(selector,desc)) {
        if (selector >= 8 && desc.Type() != 0) {
            if (offset <= desc.GetLimit())
                return desc.GetBase()+(uint64_t)offset;
        }
    }

	return mem_no_address;
}

uint64_t GetAddress(uint16_t seg, uint32_t offset)
{
	if (cpu.pmode && !(reg_flags & FLAG_VM))
        return LinMakeProt(seg,offset);

	if (seg==SegValue(cs)) return SegPhys(cs)+(uint64_t)offset;
	return ((uint64_t)seg<<4u)+offset;
}

static char empty_sel[] = { ' ',' ',0 };

bool GetDescriptorInfo(char* selname, char* out1, char* out2)
{
	Bitu sel;
	Descriptor desc;

	if (strstr(selname,"cs") || strstr(selname,"CS")) sel = SegValue(cs);
	else if (strstr(selname,"ds") || strstr(selname,"DS")) sel = SegValue(ds);
	else if (strstr(selname,"es") || strstr(selname,"ES")) sel = SegValue(es);
	else if (strstr(selname,"fs") || strstr(selname,"FS")) sel = SegValue(fs);
	else if (strstr(selname,"gs") || strstr(selname,"GS")) sel = SegValue(gs);
	else if (strstr(selname,"ss") || strstr(selname,"SS")) sel = SegValue(ss);
	else {
		sel = GetHexValue(selname,selname);
		if (*selname==0) selname=empty_sel;
	}
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		switch (desc.Type()) {
			case DESC_TASK_GATE:
				sprintf(out1,"%s: s:%08lX type:%02X p",selname,(unsigned long)desc.GetSelector(),(int)desc.saved.gate.type);
				sprintf(out2,"    TaskGate   dpl : %01X %1X",desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
			case DESC_LDT:
			case DESC_286_TSS_A:
			case DESC_286_TSS_B:
			case DESC_386_TSS_A:
			case DESC_386_TSS_B:
				sprintf(out1,"%s: b:%08lX type:%02X pag",selname,(unsigned long)desc.GetBase(),(int)desc.saved.seg.type);
				sprintf(out2,"    l:%08lX dpl : %01X %1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.g);
				return true;
			case DESC_286_CALL_GATE:
			case DESC_386_CALL_GATE:
				sprintf(out1,"%s: s:%08lX type:%02X p params: %02X",selname,(unsigned long)desc.GetSelector(),desc.saved.gate.type,desc.saved.gate.paramcount);
				sprintf(out2,"    o:%08lX dpl : %01X %1X",(unsigned long)desc.GetOffset(),desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
			case DESC_286_INT_GATE:
			case DESC_286_TRAP_GATE:
			case DESC_386_INT_GATE:
			case DESC_386_TRAP_GATE:
				sprintf(out1,"%s: s:%08lX type:%02X p",selname,(unsigned long)desc.GetSelector(),desc.saved.gate.type);
				sprintf(out2,"    o:%08lX dpl : %01X %1X",(unsigned long)desc.GetOffset(),desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
		}
		sprintf(out1,"%s: b:%08lX type:%02X parbg",selname,(unsigned long)desc.GetBase(),desc.saved.seg.type);
		sprintf(out2,"    l:%08lX dpl : %01X %1X%1X%1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		return true;
	} else {
		strcpy(out1,"                                  ");
		strcpy(out2,"                                  ");
	}
	return false;
}

/********************/
/* DebugVar   stuff */
/********************/

class CDebugVar
{
public:
	CDebugVar(char* _name, PhysPt _adr) { adr=_adr; safe_strncpy(name,_name,16); hasvalue = false; value = 0; };

	char*  GetName (void)                 { return name; };
	PhysPt GetAdr  (void)                 { return adr; };
	void   SetValue(bool has, uint16_t val) { hasvalue = has; value=val; };
	uint16_t GetValue(void)                 { return value; };
	bool   HasValue(void)                 { return hasvalue; };

private:
	PhysPt  adr;
	char    name[16];
	bool    hasvalue;
	uint16_t  value;

public:
	static void       InsertVariable(char* name, PhysPt adr);
	static CDebugVar* FindVar       (PhysPt pt);
	static void       DeleteAll     ();
	static bool       SaveVars      (char* name);
	static bool       LoadVars      (char* name);

	static std::vector<CDebugVar*> varList;
};

std::vector<CDebugVar*> CDebugVar::varList;


/********************/
/* Breakpoint stuff */
/********************/

bool mustCompleteInstruction = false;
bool skipFirstInstruction = false;

enum EBreakpoint { BKPNT_UNKNOWN, BKPNT_PHYSICAL, BKPNT_INTERRUPT, BKPNT_MEMORY, BKPNT_MEMORY_PROT, BKPNT_MEMORY_LINEAR };

#define BPINT_ALL 0x100

class CBreakpoint
{
public:

	CBreakpoint(void);
	void					SetAddress		(uint16_t seg, uint32_t off)	{ location = (PhysPt)GetAddress(seg,off); type = BKPNT_PHYSICAL; segment = seg; offset = off; };
	void					SetAddress		(PhysPt adr)				{ location = adr; type = BKPNT_PHYSICAL; };
	void					SetInt			(uint8_t _intNr, uint16_t ah, uint16_t al)	{ intNr = _intNr, ahValue = ah; alValue = al; type = BKPNT_INTERRUPT; };
	void					SetOnce			(bool _once)				{ once = _once; };
	void					SetType			(EBreakpoint _type)			{ type = _type; };
	void					SetValue		(uint8_t value)				{ ahValue = value; };
	void					SetOther		(uint8_t other)				{ alValue = other; };	

	bool					IsActive		(void)						{ return active; };
	void					Activate		(bool _active);

	EBreakpoint				GetType			(void)						{ return type; };
	bool					GetOnce			(void)						{ return once; };
	PhysPt					GetLocation		(void)						{ return location; };
	uint16_t					GetSegment		(void)						{ return segment; };
	uint32_t					GetOffset		(void)						{ return offset; };
	uint8_t					GetIntNr		(void)						{ return intNr; };
	uint16_t					GetValue		(void)						{ return ahValue; };
	uint16_t					GetOther		(void)						{ return alValue; };

	// statics
	static CBreakpoint*		AddBreakpoint		(uint16_t seg, uint32_t off, bool once);
	static CBreakpoint*		AddIntBreakpoint	(uint8_t intNum, uint16_t ah, uint16_t al, bool once);
	static CBreakpoint*		AddMemBreakpoint	(uint16_t seg, uint32_t off);
	static void				DeactivateBreakpoints();
	static void				ActivateBreakpoints	();
	static void				ActivateBreakpointsExceptAt(PhysPt adr);
	static bool				CheckBreakpoint		(uint16_t seg, uint32_t off);
	static bool				CheckIntBreakpoint	(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue);
	static CBreakpoint*		FindPhysBreakpoint	(uint16_t seg, uint32_t off, bool once);
	static CBreakpoint*		FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip);
	static bool				IsBreakpoint		(uint16_t seg, uint32_t off);
	static bool				DeleteBreakpoint	(uint16_t seg, uint32_t off);
	static bool				DeleteByIndex		(uint16_t index);
	static void				DeleteAll			(void);
	static void				ShowList			(void);


private:
	EBreakpoint	type;
	// Physical
	PhysPt		location;
#if !C_HEAVY_DEBUG
	uint8_t		oldData;
#endif
	uint16_t		segment;
	uint32_t		offset;
	// Int
	uint8_t		intNr;
	uint16_t		ahValue;
	uint16_t		alValue;
	// Shared
	bool		active;
	bool		once;

	static std::list<CBreakpoint*>	BPoints;
};

CBreakpoint::CBreakpoint(void):type(BKPNT_UNKNOWN),location(0),
#if !C_HEAVY_DEBUG
oldData(0xCC),
#endif
segment(0),offset(0),intNr(0),ahValue(0),alValue(0),active(false),once(false) { }

void CBreakpoint::Activate(bool _active)
{
#if !C_HEAVY_DEBUG
	if (GetType() == BKPNT_PHYSICAL) {
		if (_active) {
			// Set 0xCC and save old value
			uint8_t data = mem_readb(location);
			if (data != 0xCC) {
				oldData = data;
				mem_writeb(location,0xCC);
			} else if (!active) {
				// Another activate breakpoint is already here.
				// Find it, and copy its oldData value
				CBreakpoint *bp = FindOtherActiveBreakpoint(location, this);

				if (!bp || bp->oldData == 0xCC) {
					// This might also happen if there is a real 0xCC instruction here
					DEBUG_ShowMsg("DEBUG: Internal error while activating breakpoint.\n");
					oldData = 0xCC;
				} else
					oldData = bp->oldData;
			};
		} else {
			if (mem_readb(location) == 0xCC) {
				if (oldData == 0xCC)
					DEBUG_ShowMsg("DEBUG: Internal error while deactivating breakpoint.\n");

				// Check if we are the last active breakpoint at this location
				bool otherActive = (FindOtherActiveBreakpoint(location, this) != 0);

				// If so, remove 0xCC and set old value
				if (!otherActive)
					mem_writeb(location, oldData);
			};
		}
	}
#endif
	active = _active;
}

// Statics
std::list<CBreakpoint*> CBreakpoint::BPoints;

CBreakpoint* CBreakpoint::AddBreakpoint(uint16_t seg, uint32_t off, bool once)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(once);
	BPoints.push_front	(bp);
	return bp;
}

CBreakpoint* CBreakpoint::AddIntBreakpoint(uint8_t intNum, uint16_t ah, uint16_t al, bool once)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetInt			(intNum,ah,al);
	bp->SetOnce			(once);
	BPoints.push_front	(bp);
	return bp;
}

CBreakpoint* CBreakpoint::AddMemBreakpoint(uint16_t seg, uint32_t off)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(false);
	bp->SetType			(BKPNT_MEMORY);
	BPoints.push_front	(bp);
	return bp;
}

void CBreakpoint::ActivateBreakpoints()
{
	// activate all breakpoints
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i)
		(*i)->Activate(true);
}

void CBreakpoint::DeactivateBreakpoints()
{
	// deactivate all breakpoints
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i)
		(*i)->Activate(false);
}

void CBreakpoint::ActivateBreakpointsExceptAt(PhysPt adr)
{
	// activate all breakpoints, except those at adr
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		// Do not activate breakpoints at adr
		if (bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr)
			continue;
		bp->Activate(true);
	}
}

bool CBreakpoint::CheckBreakpoint(uint16_t seg, uint32_t off)
// Checks if breakpoint is valid and should stop execution
{
	// Quick exit if there are no breakpoints
	if (BPoints.empty()) return false;

	// Search matching breakpoint
	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint *bp = (*i);

		if ((bp->GetType() == BKPNT_PHYSICAL) && bp->IsActive() &&
		    (bp->GetLocation() == GetAddress(seg, off))) {
			// Found
			if (bp->GetOnce()) {
				// delete it, if it should only be used once
				(BPoints.erase)(i);
				bp->Activate(false);
				delete bp;
			} else {
				// Also look for once-only breakpoints at this address
				bp = FindPhysBreakpoint(seg, off, true);
				if (bp) {
					BPoints.remove(bp);
					bp->Activate(false);
					delete bp;
				}
			}
			return true;
		}
#if C_HEAVY_DEBUG
		// Memory breakpoint support
		else if (bp->IsActive()) {
			if ((bp->GetType()==BKPNT_MEMORY) || (bp->GetType()==BKPNT_MEMORY_PROT) || (bp->GetType()==BKPNT_MEMORY_LINEAR)) {
				// Watch Protected Mode Memoryonly in pmode
				if (bp->GetType()==BKPNT_MEMORY_PROT) {
					// Check if pmode is active
					if (!cpu.pmode) return false;
					// Check if descriptor is valid
					Descriptor desc;
					if (!cpu.gdt.GetDescriptor(bp->GetSegment(),desc)) return false;
					if (desc.GetLimit()==0) return false;
				}

				Bitu address; 
				if (bp->GetType()==BKPNT_MEMORY_LINEAR) address = bp->GetOffset();
				else address = (Bitu)GetAddress(bp->GetSegment(),bp->GetOffset());
				uint8_t value=0;
				if (mem_readb_checked((PhysPt)address,&value)) return false;
				if (bp->GetValue() != value) {
					// Yup, memory value changed
					DEBUG_ShowMsg("DEBUG: Memory breakpoint %s: %04X:%04X - %02X -> %02X\n",(bp->GetType()==BKPNT_MEMORY_PROT)?"(Prot)":"",bp->GetSegment(),bp->GetOffset(),bp->GetValue(),value);
					bp->SetValue(value);
					return true;
				}
			}
		}
#endif
	}
	return false;
}

bool CBreakpoint::CheckIntBreakpoint(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue)
// Checks if interrupt breakpoint is valid and should stop execution
{
	if (BPoints.empty()) return false;

    // unused
    (void)adr;

	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if ((bp->GetType()==BKPNT_INTERRUPT) && bp->IsActive() && (bp->GetIntNr()==intNr)) {
			if (((bp->GetValue()==BPINT_ALL) || (bp->GetValue()==ahValue)) && ((bp->GetOther()==BPINT_ALL) || (bp->GetOther()==alValue))) {
				// Ignore it once ?
				// Found
				if (bp->GetOnce()) {
					// delete it, if it should only be used once
					(BPoints.erase)(i);
					bp->Activate(false);
					delete bp;
				}
				return true;
			}
		}
	}
	return false;
}

void CBreakpoint::DeleteAll()
{
	std::list<CBreakpoint*>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		bp->Activate(false);
		delete bp;
	}
	(BPoints.clear)();
}


bool CBreakpoint::DeleteByIndex(uint16_t index)
{
	// Search matching breakpoint
	int nr = 0;
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		if (nr==index) {
			bp = (*i);
			(BPoints.erase)(i);
			bp->Activate(false);
			delete bp;
			return true;
		}
		nr++;
	}
	return false;
}

CBreakpoint* CBreakpoint::FindPhysBreakpoint(uint16_t seg, uint32_t off, bool once)
{
	if (BPoints.empty()) return 0;
#if !C_HEAVY_DEBUG
	PhysPt adr = GetAddress(seg, off);
#endif
	// Search for matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		bp = (*i);
#if C_HEAVY_DEBUG
		// Heavy debugging breakpoints are triggered by matching seg:off
		bool atLocation = bp->GetSegment() == seg && bp->GetOffset() == off;
#else
		// Normal debugging breakpoints are triggered at an address
		bool atLocation = bp->GetLocation() == adr;
#endif

		if (bp->GetType() == BKPNT_PHYSICAL && atLocation && bp->GetOnce() == once)
			return bp;
	}

	return 0;
}

CBreakpoint* CBreakpoint::FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip)
{
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp != skip && bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr && bp->IsActive())
			return bp;
	}
	return 0;
}

// is there a permanent breakpoint at address ?
bool CBreakpoint::IsBreakpoint(uint16_t seg, uint32_t off)
{
	return FindPhysBreakpoint(seg, off, false) != 0;
}

bool CBreakpoint::DeleteBreakpoint(uint16_t seg, uint32_t off)
{
	CBreakpoint* bp = FindPhysBreakpoint(seg, off, false);
	if (bp) {
		BPoints.remove(bp);
		delete bp;
		return true;
	}

	return false;
}


void CBreakpoint::ShowList(void)
{
	// iterate list
	int nr = 0;
	std::list<CBreakpoint*>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetType()==BKPNT_PHYSICAL) {
			DEBUG_ShowMsg("%02X. BP %04X:%04X\n",nr,bp->GetSegment(),bp->GetOffset());
		} else if (bp->GetType()==BKPNT_INTERRUPT) {
			if (bp->GetValue()==BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X\n",nr,bp->GetIntNr());
			else if (bp->GetOther()==BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X\n",nr,bp->GetIntNr(),bp->GetValue());
			else DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X AL=%02X\n",nr,bp->GetIntNr(),bp->GetValue(),bp->GetOther());
		} else if (bp->GetType()==BKPNT_MEMORY) {
			DEBUG_ShowMsg("%02X. BPMEM %04X:%04X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
		} else if (bp->GetType()==BKPNT_MEMORY_PROT) {
			DEBUG_ShowMsg("%02X. BPPM %04X:%08X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
		} else if (bp->GetType()==BKPNT_MEMORY_LINEAR ) {
			DEBUG_ShowMsg("%02X. BPLM %08X (%02X)\n",nr,bp->GetOffset(),bp->GetValue());
		}
		nr++;
	}
}

bool DEBUG_Breakpoint(void)
{
	/* First get the physical address and check for a set Breakpoint */
	if (!CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) return false;
	// Found. Breakpoint is valid
//	PhysPt where=(PhysPt)GetAddress(SegValue(cs),reg_eip);
	CBreakpoint::DeactivateBreakpoints();	// Deactivate all breakpoints
	return true;
}

bool DEBUG_IntBreakpoint(uint8_t intNum)
{
    if (inhibit_int_breakpoint) return false; /* or else stepping over INT 21h when BPINT 21h does nothing */
	/* First get the physical address and check for a set Breakpoint */
	PhysPt where=(PhysPt)GetAddress(SegValue(cs),reg_eip);
	if (!CBreakpoint::CheckIntBreakpoint(where,intNum,reg_ah,reg_al)) return false;
	// Found. Breakpoint is valid
	CBreakpoint::DeactivateBreakpoints();	// Deactivate all breakpoints
	return true;
}

static bool StepOver()
{
	exitLoop = false;
	PhysPt start=(PhysPt)GetAddress(SegValue(cs),reg_eip);
	char dline[200];Bitu size;
	size=DasmI386(dline, start, reg_eip, cpu.code.big);

	if (strstr(dline,"call") || strstr(dline,"int") || strstr(dline,"loop") || strstr(dline,"rep")) {
		// Don't add a temporary breakpoint if there's already one here
		if (!CBreakpoint::FindPhysBreakpoint(SegValue(cs), (uint32_t)(reg_eip+size), true))
			CBreakpoint::AddBreakpoint(SegValue(cs),(uint32_t)(reg_eip+size), true);
		debugging=false;

        logBuffSuppressConsole = false;
        if (logBuffSuppressConsoleNeedUpdate) {
            logBuffSuppressConsoleNeedUpdate = false;
            DEBUG_RefreshPage(0);
        }

		DrawCode();
		mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
		return true;
	} 
	return false;
}

bool DEBUG_ExitLoop(void)
{
#if C_HEAVY_DEBUG
	DrawVariables();
#endif

	if (exitLoop) {
		exitLoop = false;
		return true;
	}
	return false;
}

/********************/
/*   Draw windows   */
/********************/

static void DrawData(void) {
	if (dbg.win_main == NULL || dbg.win_data == NULL)
		return;

	uint8_t ch;
	uint32_t add = dataOfs;
	uint64_t address;
    int w,h,y;

	/* Data win */	
    getmaxyx(dbg.win_data,h,w);

    if ((paging.enabled || cpu.pmode) && dbg.data_view != DBGBlock::DATV_PHYSICAL) h--;

	for (y=0;y<h;y++) {
		// Address
        if (dbg.data_view == DBGBlock::DATV_SEGMENTED) {
            wattrset (dbg.win_data,0);
            mvwprintw (dbg.win_data,y,0,"%04X:%08X ",dataSeg,add);
        }
        else {
            wattrset (dbg.win_data,0);
            mvwprintw (dbg.win_data,y,0,"     %08X ",add);
        }

        if (dbg.data_view == DBGBlock::DATV_PHYSICAL) {
            for (int x=0; x<16; x++) {
                address = add;

                /* save the original page addr.
                 * we must hack the phys page tlb to make the hardware handler map 1:1 the page for this call. */
                PhysPt opg = paging.tlb.phys_page[address>>12];

                paging.tlb.phys_page[address>>12] = (uint32_t)(address>>12);

                PageHandler *ph = MEM_GetPageHandler((Bitu)(address>>12));

                if (ph->flags & PFLAG_READABLE)
	                ch = ph->GetHostReadPt((Bitu)(address>>12))[address&0xFFF];
                else
                    ch = ph->readb((PhysPt)address);

                paging.tlb.phys_page[address>>12] = opg;

                wattrset (dbg.win_data,0);
                mvwprintw (dbg.win_data,y,14+3*x,"%02X",ch);
                if(showPrintable) {
                    if (ch<32 || !isprint(*reinterpret_cast<unsigned char*>(&ch))) ch='.';
                    mvwprintw (dbg.win_data,y,63+x,"%c",ch);
                } else mvwaddch(dbg.win_data,y,63+x,ch);

                add++;
            }
        }
        else {
            for (int x=0; x<16; x++) {
                if (dbg.data_view == DBGBlock::DATV_SEGMENTED)
                    address = GetAddress(dataSeg,add);
                else
                    address = add;

                if (address != mem_no_address) {
                    if (!mem_readb_checked((PhysPt)address,&ch)) {
                        wattrset (dbg.win_data,0);
                        mvwprintw (dbg.win_data,y,14+3*x,"%02X",ch);
                        if(showPrintable) {
                            if (ch<32 || !isprint(*reinterpret_cast<unsigned char*>(&ch))) ch='.';
                            mvwprintw (dbg.win_data,y,63+x,"%c",ch);
                        } else mvwaddch(dbg.win_data,y,63+x,ch);
                    }
                    else {
                        wattrset (dbg.win_data, COLOR_PAIR(PAIR_BYELLOW_BLACK));
                        mvwprintw (dbg.win_data,y,14+3*x,"pf");
                        mvwprintw (dbg.win_data,y,63+x,".");
                    }
                }
                else {
                    wattrset (dbg.win_data, COLOR_PAIR(PAIR_BYELLOW_BLACK));
                    mvwprintw (dbg.win_data,y,14+3*x,"na");
                    mvwprintw (dbg.win_data,y,63+x,".");
                }

                add++;
            }
        }
	}

    if ((paging.enabled || cpu.pmode) && dbg.data_view != DBGBlock::DATV_PHYSICAL) {
        /* one line was set aside for this information */
        wattrset (dbg.win_data,0);
        if (dbg.data_view == DBGBlock::DATV_SEGMENTED) {
            address = GetAddress(dataSeg,dataOfs);
            if (address != mem_no_address)
                mvwprintw (dbg.win_data,y,0," LIN=%08X ",(unsigned int)address);
            else
                mvwprintw (dbg.win_data,y,0," LIN=XXXXXXXX ");
        }
        else {
            address = dataOfs;
            mvwprintw (dbg.win_data,y,0,"              ");
        }

        if (!mem_readb_checked((PhysPt)address,&ch)) {
            Bitu naddr = PAGING_GetPhysicalAddress((PhysPt)address);
            mvwprintw (dbg.win_data,y,14,"PHY=%08X ",(unsigned int)naddr);
        }
        else {
            mvwprintw (dbg.win_data,y,14,"PHY=XXXXXXXX ");
        }

        wclrtoeol(dbg.win_data);

        y++;
    }

	wrefresh(dbg.win_data);
}

void DrawRegistersUpdateOld(void) {
	/* Main Registers */
	oldregs.eax=reg_eax;
	oldregs.ebx=reg_ebx;
	oldregs.ecx=reg_ecx;
	oldregs.edx=reg_edx;

	oldregs.esi=reg_esi;
	oldregs.edi=reg_edi;
	oldregs.ebp=reg_ebp;
	oldregs.esp=reg_esp;
	oldregs.eip=reg_eip;

	oldsegs[ds].val=SegValue(ds);
	oldsegs[es].val=SegValue(es);
	oldsegs[fs].val=SegValue(fs);
	oldsegs[gs].val=SegValue(gs);
	oldsegs[ss].val=SegValue(ss);
	oldsegs[cs].val=SegValue(cs);
	
	oldfpu = fpu; /* Slow? */

	/*Individual flags*/
	oldflags = reg_flags;

	oldcpucpl=cpu.cpl;
}

static void DrawRegisters(void) {
	if (dbg.win_main == NULL || dbg.win_reg == NULL)
		return;

	/* Main Registers */
	SetColor(reg_eax!=oldregs.eax);mvwprintw (dbg.win_reg,0,4,"%08X",reg_eax);
	SetColor(reg_ebx!=oldregs.ebx);mvwprintw (dbg.win_reg,1,4,"%08X",reg_ebx);
	SetColor(reg_ecx!=oldregs.ecx);mvwprintw (dbg.win_reg,2,4,"%08X",reg_ecx);
	SetColor(reg_edx!=oldregs.edx);mvwprintw (dbg.win_reg,3,4,"%08X",reg_edx);

	SetColor(reg_esi!=oldregs.esi);mvwprintw (dbg.win_reg,0,18,"%08X",reg_esi);
	SetColor(reg_edi!=oldregs.edi);mvwprintw (dbg.win_reg,1,18,"%08X",reg_edi);
	SetColor(reg_ebp!=oldregs.ebp);mvwprintw (dbg.win_reg,2,18,"%08X",reg_ebp);
	SetColor(reg_esp!=oldregs.esp);mvwprintw (dbg.win_reg,3,18,"%08X",reg_esp);
	SetColor(reg_eip!=oldregs.eip);mvwprintw (dbg.win_reg,1,42,"%08X",reg_eip);

	SetColor(SegValue(ds)!=oldsegs[ds].val);mvwprintw (dbg.win_reg,0,31,"%04X",SegValue(ds));
	SetColor(SegValue(es)!=oldsegs[es].val);mvwprintw (dbg.win_reg,0,41,"%04X",SegValue(es));
	SetColor(SegValue(fs)!=oldsegs[fs].val);mvwprintw (dbg.win_reg,0,51,"%04X",SegValue(fs));
	SetColor(SegValue(gs)!=oldsegs[gs].val);mvwprintw (dbg.win_reg,0,61,"%04X",SegValue(gs));
	SetColor(SegValue(ss)!=oldsegs[ss].val);mvwprintw (dbg.win_reg,0,71,"%04X",SegValue(ss));
	SetColor(SegValue(cs)!=oldsegs[cs].val);mvwprintw (dbg.win_reg,1,31,"%04X",SegValue(cs));
	
	char x87buf[12] = {};
	SetColor(F80TestUpdate(STV(0)));mvwprintw (dbg.win_reg,4,4,"%s", F80ToString(STV(0), x87buf));
	SetColor(F80TestUpdate(STV(4)));mvwprintw (dbg.win_reg,5,4,"%s", F80ToString(STV(4), x87buf));
	
	SetColor(F80TestUpdate(STV(1)));mvwprintw (dbg.win_reg,4,18,"%s", F80ToString(STV(1), x87buf));
	SetColor(F80TestUpdate(STV(5)));mvwprintw (dbg.win_reg,5,18,"%s", F80ToString(STV(5), x87buf));
	
	SetColor(F80TestUpdate(STV(2)));mvwprintw (dbg.win_reg,4,32,"%s", F80ToString(STV(2), x87buf));
	SetColor(F80TestUpdate(STV(6)));mvwprintw (dbg.win_reg,5,32,"%s", F80ToString(STV(6), x87buf));
	
	SetColor(F80TestUpdate(STV(3)));mvwprintw (dbg.win_reg,4,46,"%s", F80ToString(STV(3), x87buf));
	SetColor(F80TestUpdate(STV(7)));mvwprintw (dbg.win_reg,5,46,"%s", F80ToString(STV(7), x87buf));

	/*Individual flags*/
	Bitu changed_flags = reg_flags ^ oldflags;

	SetColor(changed_flags&FLAG_CF);
	mvwprintw (dbg.win_reg,1,53,"%01X",GETFLAG(CF) ? 1:0);
	SetColor(changed_flags&FLAG_ZF);
	mvwprintw (dbg.win_reg,1,56,"%01X",GETFLAG(ZF) ? 1:0);
	SetColor(changed_flags&FLAG_SF);
	mvwprintw (dbg.win_reg,1,59,"%01X",GETFLAG(SF) ? 1:0);
	SetColor(changed_flags&FLAG_OF);
	mvwprintw (dbg.win_reg,1,62,"%01X",GETFLAG(OF) ? 1:0);
	SetColor(changed_flags&FLAG_AF);
	mvwprintw (dbg.win_reg,1,65,"%01X",GETFLAG(AF) ? 1:0);
	SetColor(changed_flags&FLAG_PF);
	mvwprintw (dbg.win_reg,1,68,"%01X",GETFLAG(PF) ? 1:0);


	SetColor(changed_flags&FLAG_DF);
	mvwprintw (dbg.win_reg,1,71,"%01X",GETFLAG(DF) ? 1:0);
	SetColor(changed_flags&FLAG_IF);
	mvwprintw (dbg.win_reg,1,74,"%01X",GETFLAG(IF) ? 1:0);
	SetColor(changed_flags&FLAG_TF);
	mvwprintw (dbg.win_reg,1,77,"%01X",GETFLAG(TF) ? 1:0);

	SetColor(changed_flags&FLAG_IOPL);
	mvwprintw (dbg.win_reg,2,72,"%01X",GETFLAG(IOPL)>>12);


	SetColor(cpu.cpl ^ oldcpucpl);
	mvwprintw (dbg.win_reg,2,78,"%01X",cpu.cpl);

	if (cpu.pmode) {
		if (reg_flags & FLAG_VM) mvwprintw(dbg.win_reg,0,76,"VM86");
		else if (cpu.code.big) mvwprintw(dbg.win_reg,0,76,"Pr32");
		else mvwprintw(dbg.win_reg,0,76,"Pr16");
		mvwprintw(dbg.win_reg,2,62,paging.enabled ? "PAGE" : "NOPG");
	} else {
		mvwprintw(dbg.win_reg,0,76,"Real");
		mvwprintw(dbg.win_reg,2,62,"NOPG");
    }

	// Selector info, if available
	if ((cpu.pmode) && curSelectorName[0]) {
		char out1[200], out2[200];
		GetDescriptorInfo(curSelectorName,out1,out2);
		mvwprintw(dbg.win_reg,2,28,out1);
		mvwprintw(dbg.win_reg,3,28,out2);
	}

	wattrset(dbg.win_reg,0);
	mvwprintw(dbg.win_reg,3,60,"%u       ",cycle_count);
	wrefresh(dbg.win_reg);
}

bool DEBUG_IsPagingOutput(void);

static void DrawInput(void) {
    if (!debugging) {
        if (has_colors())
        {
        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        }

        mvwprintw(dbg.win_inp,0,0,"%s","(Running)");
        wclrtoeol(dbg.win_inp);
    } else if (debug_running) {
        if (has_colors())
        {
        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        }

        mvwprintw(dbg.win_inp,0,0,"%s","(Running/watching)");
        wclrtoeol(dbg.win_inp);
    } else if (DEBUG_IsPagingOutput()) {
        if (has_colors())
        {
        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        }

        mvwprintw(dbg.win_inp,0,0,"%s","^ Paged content: Hit ENTER to continue, Q to exit paging");
        wclrtoeol(dbg.win_inp);
    } else {
        //TODO long lines
        char* dispPtr = codeViewData.inputStr;
        char* curPtr = &codeViewData.inputStr[codeViewData.inputPos];

        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_BLACK_GREY));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_BLACK_GREY));
        mvwprintw(dbg.win_inp,0,0,"%c-> %s%c",
                (codeViewData.ovrMode?'O':'I'),dispPtr,(*curPtr?' ':'_'));
        wclrtoeol(dbg.win_inp); // not correct in pdcurses if full line
        mvwchgat(dbg.win_inp,10,0,3,0,(PAIR_BLACK_GREY),NULL);
        if (*curPtr) {
            mvwchgat(dbg.win_inp,0,(int)(curPtr-dispPtr+4),1,0,(PAIR_BLACK_GREY),NULL);
        } 
    }

    wattrset(dbg.win_inp,0);
    wrefresh(dbg.win_inp);
}

static void DrawCode(void) {
	if (dbg.win_main == NULL || dbg.win_code == NULL)
		return;

	uint32_t disEIP = codeViewData.useEIP;
	char dline[200];Bitu size;Bitu c;
	static char line20[21] = "                    ";
    int w,h;

    getmaxyx(dbg.win_code,h,w);
	for (int i=0;i<h;i++) {
        uint64_t start = GetAddress(codeViewData.useCS,disEIP);

		bool saveSel = false;
		if (has_colors()) {
			if ((codeViewData.useCS==SegValue(cs)) && (disEIP == reg_eip)) {
				if (codeViewData.cursorPos==-1) {
					codeViewData.cursorPos = i; // Set Cursor 
				}
				if (i == codeViewData.cursorPos) {
					codeViewData.cursorSeg = SegValue(cs);
					codeViewData.cursorOfs = disEIP;
				}
				saveSel = (i == codeViewData.cursorPos);

                if (i == codeViewData.cursorPos) {
                    wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREEN));
                    wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREEN));
                }
                else {
                    wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));
                    wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));
                }
            } else if (i == codeViewData.cursorPos) {
                wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));
				codeViewData.cursorSeg = codeViewData.useCS;
				codeViewData.cursorOfs = disEIP;
				saveSel = true;
			} else if (CBreakpoint::IsBreakpoint(codeViewData.useCS, disEIP)) {
                wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));
			} else {
                wbkgdset(dbg.win_code,0);
                wattrset(dbg.win_code,0);
			}
		}


		bool toolarge = false;
        bool no_bytes = false;
		Bitu drawsize;

        if (start != mem_no_address) {
            drawsize=size=DasmI386(dline, (PhysPt)start, disEIP, cpu.code.big);
        }
        else {
            drawsize=size=1;
            dline[0]=0;
        }
		mvwprintw(dbg.win_code,i,0,"%04X:%08X ",codeViewData.useCS,disEIP);

		if (drawsize>10) { toolarge = true; drawsize = 9; }
 
        if (start != mem_no_address) {
            for (c=0;c<drawsize;c++) {
                uint8_t value;
                if (!mem_readb_checked((PhysPt)(start+c),&value)) {
                    wattrset (dbg.win_code,0);
                    wprintw(dbg.win_code,"%02X",value);
                }
                else {
                    no_bytes = true;
                    wattrset (dbg.win_code, COLOR_PAIR(PAIR_BYELLOW_BLACK));
                    wprintw(dbg.win_code,"pf");
                }
            }
        }
        else {
            wattrset (dbg.win_code, COLOR_PAIR(PAIR_BYELLOW_BLACK));
            wprintw(dbg.win_code,"na");
        }

        wattrset (dbg.win_code,0);
        if (toolarge) { waddstr(dbg.win_code,".."); drawsize++; }
		// Spacepad up to 20 characters
		if(drawsize && (drawsize < 11)) {
			line20[20 - drawsize*2] = 0;
			waddstr(dbg.win_code,line20);
			line20[20 - drawsize*2] = ' ';
		} else waddstr(dbg.win_code,line20);

		char empty_res[] = { 0 };
		char* res = empty_res;
        wattrset (dbg.win_code,0);
        if (showExtend) res = AnalyzeInstruction(dline, saveSel);
		// Spacepad it up to 28 characters
        if (no_bytes) dline[0] = 0;
		size_t dline_len = strlen(dline);
        if (dline_len < 28) {
            memset(dline + dline_len, ' ', 28 - dline_len);
            dline[28] = 0;
        }
		waddstr(dbg.win_code,dline);
		// Spacepad it up to 20 characters
		size_t res_len = strlen(res);
		if(res_len && (res_len < 21)) {
			waddstr(dbg.win_code,res);
			line20[20-res_len] = 0;
			waddstr(dbg.win_code,line20);
			line20[20-res_len] = ' ';
		} else 	waddstr(dbg.win_code,line20);

        wclrtoeol(dbg.win_code);

        disEIP+=(uint32_t)size;

		if (i==0) codeViewData.firstInstSize = (uint16_t)size;
		if (i==4) codeViewData.useEIPmid	 = disEIP;
	}

	codeViewData.useEIPlast = disEIP;

	wrefresh(dbg.win_code);
}

static void SetCodeWinStart()
{
	if ((SegValue(cs)==codeViewData.useCS) && (reg_eip>=codeViewData.useEIP) && (reg_eip<=codeViewData.useEIPlast)) {
		// in valid window - scroll ?
		if (reg_eip>=codeViewData.useEIPmid) codeViewData.useEIP += codeViewData.firstInstSize;

	} else {
		// totally out of range.
		codeViewData.useCS	= SegValue(cs);
		codeViewData.useEIP	= reg_eip;
	}
	codeViewData.cursorPos = -1;	// Recalc Cursor position
}

void DEBUG_CheckCSIP() {
    SetCodeWinStart();
}

/********************/
/*    User input    */
/********************/

void SkipSpace(char*& hex) {
    while (*hex == ' ') hex++;
}

uint32_t GetHexValue(char* const str, char* &hex,bool *parsed)
{
    uint32_t	value = 0;
    uint32_t regval = 0;
    hex = str;
    while (*hex == ' ') hex++;

    // The user can enclose a value in double quotations to enter hex values that
    // would collide with a flag name (AC, AF, CF, and DF).
    if (*hex == '\"') { hex++; }
    else if (strncmp(hex, "EFLAGS", 6) == 0) { hex += 6; regval = (uint32_t)reg_flags; }
    else if (strncmp(hex, "FLAGS", 5) == 0) { hex += 5; regval = (uint32_t)reg_flags; }
    else if (strncmp(hex, "IOPL", 4) == 0) { hex += 4; regval = (reg_flags & FLAG_IOPL) >> 12u; }
    else if (strncmp(hex, "CR0", 3) == 0) { hex += 3; regval = (uint32_t)cpu.cr0; }
    else if (strncmp(hex, "CR2", 3) == 0) { hex += 3; regval = (uint32_t)paging.cr2; }
    else if (strncmp(hex, "CR3", 3) == 0) { hex += 3; regval = (uint32_t)paging.cr3; }
    else if (strncmp(hex, "EAX", 3) == 0) { hex += 3; regval = reg_eax; }
    else if (strncmp(hex, "EBX", 3) == 0) { hex += 3; regval = reg_ebx; }
    else if (strncmp(hex, "ECX", 3) == 0) { hex += 3; regval = reg_ecx; }
    else if (strncmp(hex, "EDX", 3) == 0) { hex += 3; regval = reg_edx; }
    else if (strncmp(hex, "ESI", 3) == 0) { hex += 3; regval = reg_esi; }
    else if (strncmp(hex, "EDI", 3) == 0) { hex += 3; regval = reg_edi; }
    else if (strncmp(hex, "EBP", 3) == 0) { hex += 3; regval = reg_ebp; }
    else if (strncmp(hex, "ESP", 3) == 0) { hex += 3; regval = reg_esp; }
    else if (strncmp(hex, "EIP", 3) == 0) { hex += 3; regval = reg_eip; }
    else if (strncmp(hex, "AX", 2) == 0) { hex += 2; regval = reg_ax; }
    else if (strncmp(hex, "BX", 2) == 0) { hex += 2; regval = reg_bx; }
    else if (strncmp(hex, "CX", 2) == 0) { hex += 2; regval = reg_cx; }
    else if (strncmp(hex, "DX", 2) == 0) { hex += 2; regval = reg_dx; }
    else if (strncmp(hex, "SI", 2) == 0) { hex += 2; regval = reg_si; }
    else if (strncmp(hex, "DI", 2) == 0) { hex += 2; regval = reg_di; }
    else if (strncmp(hex, "BP", 2) == 0) { hex += 2; regval = reg_bp; }
    else if (strncmp(hex, "SP", 2) == 0) { hex += 2; regval = reg_sp; }
    else if (strncmp(hex, "IP", 2) == 0) { hex += 2; regval = reg_ip; }
    else if (strncmp(hex, "AL", 2) == 0) { hex += 2; regval = reg_al; }
    else if (strncmp(hex, "BL", 2) == 0) { hex += 2; regval = reg_bl; }
    else if (strncmp(hex, "CL", 2) == 0) { hex += 2; regval = reg_cl; }
    else if (strncmp(hex, "DL", 2) == 0) { hex += 2; regval = reg_dl; }
    else if (strncmp(hex, "AH", 2) == 0) { hex += 2; regval = reg_ah; }
    else if (strncmp(hex, "BH", 2) == 0) { hex += 2; regval = reg_bh; }
    else if (strncmp(hex, "CH", 2) == 0) { hex += 2; regval = reg_ch; }
    else if (strncmp(hex, "DH", 2) == 0) { hex += 2; regval = reg_dh; }
    else if (strncmp(hex, "CS", 2) == 0) { hex += 2; regval = SegValue(cs); }
    else if (strncmp(hex, "DS", 2) == 0) { hex += 2; regval = SegValue(ds); }
    else if (strncmp(hex, "ES", 2) == 0) { hex += 2; regval = SegValue(es); }
    else if (strncmp(hex, "FS", 2) == 0) { hex += 2; regval = SegValue(fs); }
    else if (strncmp(hex, "GS", 2) == 0) { hex += 2; regval = SegValue(gs); }
    else if (strncmp(hex, "SS", 2) == 0) { hex += 2; regval = SegValue(ss); }
    else if (strncmp(hex, "AC", 2) == 0) { hex += 2; regval = GETFLAG(AC); }
    else if (strncmp(hex, "AF", 2) == 0) { hex += 2; regval = GETFLAG(AF); }
    else if (strncmp(hex, "CF", 2) == 0) { hex += 2; regval = GETFLAG(CF); }
    else if (strncmp(hex, "DF", 2) == 0) { hex += 2; regval = GETFLAG(DF); }
    else if (strncmp(hex, "ID", 2) == 0) { hex += 2; regval = GETFLAG(ID); }
    else if (strncmp(hex, "IF", 2) == 0) { hex += 2; regval = GETFLAG(IF); }
    else if (strncmp(hex, "NT", 2) == 0) { hex += 2; regval = GETFLAG(NT); }
    else if (strncmp(hex, "OF", 2) == 0) { hex += 2; regval = GETFLAG(OF); }
    else if (strncmp(hex, "PF", 2) == 0) { hex += 2; regval = GETFLAG(PF); }
    else if (strncmp(hex, "SF", 2) == 0) { hex += 2; regval = GETFLAG(SF); }
    else if (strncmp(hex, "TF", 2) == 0) { hex += 2; regval = GETFLAG(TF); }
    else if (strncmp(hex, "VM", 2) == 0) { hex += 2; regval = GETFLAG(VM); }
    else if (strncmp(hex, "ZF", 2) == 0) { hex += 2; regval = GETFLAG(ZF); }

    else if (strncmp(hex,"DTASEG", 6) == 0) { hex += 6; regval = (!dos_kernel_disabled) ? (dos.dta() >> 16u)    : 0; }
    else if (strncmp(hex,"DTAOFF", 6) == 0) { hex += 6; regval = (!dos_kernel_disabled) ? (dos.dta() & 0xFFFFu) : 0; }
    else if (strncmp(hex,"PSPSEG", 6) == 0) { hex += 6; regval = (!dos_kernel_disabled) ?  dos.psp()            : 0; }

    while (*hex && *hex != '\"') {
        if ((*hex >= '0') && (*hex <= '9')) value = (value << 4u) + ((uint32_t)(*hex)) - '0';
        else if ((*hex >= 'A') && (*hex <= 'F')) value = (value << 4u) + ((uint32_t)(*hex)) - 'A' + 10u;
        else {
            if (*hex == '+') { hex++; return regval + value + GetHexValue(hex, hex, parsed); }
            else
                if (*hex == '-') { hex++; return regval + value - GetHexValue(hex, hex, parsed); }
                else break; // No valid char
        }
        hex++;
    }

    // If there is a closing quote, skip over it.
    if (*hex == '\"') {
        hex++;
    }

    if (parsed != NULL)
        *parsed = (hex != str);

    return regval + value;
}

bool ChangeRegister(char* const str)
{
	char* hex = str;

    while (*hex) {
    	while (*hex==' ') hex++;

    	if (strstr(hex,"EFLAGS")==hex) { hex+=6; CPU_SetFlags(GetHexValue(hex,hex),FMASK_ALL); } else
    	if (strstr(hex,"FLAGS")==hex) { hex+=5; CPU_SetFlags(GetHexValue(hex,hex),FMASK_ALL & 0xFFFFu); } else

        //             "IOPL"

    	if (strncmp(hex,"EAX",3) == 0) { hex+=3; reg_eax = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EBX",3) == 0) { hex+=3; reg_ebx = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"ECX",3) == 0) { hex+=3; reg_ecx = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EDX",3) == 0) { hex+=3; reg_edx = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"ESI",3) == 0) { hex+=3; reg_esi = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EDI",3) == 0) { hex+=3; reg_edi = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EBP",3) == 0) { hex+=3; reg_ebp = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"ESP",3) == 0) { hex+=3; reg_esp = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EIP",3) == 0) { hex+=3; reg_eip = GetHexValue(hex,hex); } else

    	if (strncmp(hex,"AX",2) == 0) { hex+=2; reg_ax = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BX",2) == 0) { hex+=2; reg_bx = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"CX",2) == 0) { hex+=2; reg_cx = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DX",2) == 0) { hex+=2; reg_dx = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"SI",2) == 0) { hex+=2; reg_si = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DI",2) == 0) { hex+=2; reg_di = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BP",2) == 0) { hex+=2; reg_bp = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"SP",2) == 0) { hex+=2; reg_sp = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"IP",2) == 0) { hex+=2; reg_ip = (uint16_t)GetHexValue(hex,hex); } else

	    if (strncmp(hex,"AL",2) == 0) { hex+=2; reg_al = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BL",2) == 0) { hex+=2; reg_bl = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"CL",2) == 0) { hex+=2; reg_cl = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DL",2) == 0) { hex+=2; reg_dl = (uint8_t)GetHexValue(hex,hex); } else

    	if (strncmp(hex,"AH",2) == 0) { hex+=2; reg_ah = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BH",2) == 0) { hex+=2; reg_bh = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"CH",2) == 0) { hex+=2; reg_ch = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DH",2) == 0) { hex+=2; reg_dh = (uint8_t)GetHexValue(hex,hex); } else

    	if (strncmp(hex,"CS",2) == 0) { hex+=2; SegSet16(cs,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"DS",2) == 0) { hex+=2; SegSet16(ds,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"ES",2) == 0) { hex+=2; SegSet16(es,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"FS",2) == 0) { hex+=2; SegSet16(fs,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"GS",2) == 0) { hex+=2; SegSet16(gs,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"SS",2) == 0) { hex+=2; SegSet16(ss,(uint16_t)GetHexValue(hex,hex)); } else

        if (strncmp(hex,"AC",2) == 0) { hex+=2; SETFLAGBIT(AC,GetHexValue(hex,hex)); } else
        if (strncmp(hex,"AF",2) == 0) { hex+=2; SETFLAGBIT(AF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"CF",2) == 0) { hex+=2; SETFLAGBIT(CF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"DF",2) == 0) { hex+=2; SETFLAGBIT(DF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"ID",2) == 0) { hex+=2; SETFLAGBIT(ID,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"IF",2) == 0) { hex+=2; SETFLAGBIT(IF,GetHexValue(hex,hex)); } else
        //             "NT"
    	if (strncmp(hex,"OF",2) == 0) { hex+=2; SETFLAGBIT(OF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"PF",2) == 0) { hex+=2; SETFLAGBIT(PF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"SF",2) == 0) { hex+=2; SETFLAGBIT(SF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"TF",2) == 0) { hex+=2; SETFLAGBIT(TF,GetHexValue(hex,hex)); } else
        //             "VM"
	    if (strncmp(hex,"ZF",2) == 0) { hex+=2; SETFLAGBIT(ZF,GetHexValue(hex,hex)); } else
    	{ return false; }
    }

	return true;
}

void DEBUG_GUI_Rebuild(void);
void DBGUI_NextWindowIfActiveHidden(void);

void DEBUG_BeginPagedContent(void);
void DEBUG_EndPagedContent(void);

std::string pc98_egc_shift_debug_status(void);

static void LogFPUInfo(void);

bool ParseCommand(char* str) {
    std::string copy_str = str;
    for (auto &c : copy_str) c = toupper(c);
    copy_str += '\0'; /* paranoid */
	char* found = &(copy_str[0]); /* cannot use std::string c_str(), that is const char* */

	found = trim(found);
	string s_found(found);
	istringstream stream(s_found);
	string command;
	stream >> command;
	string::size_type next = s_found.find_first_not_of(' ',command.size());
	if(next == string::npos) next = command.size();
	(s_found.erase)(0,next);
	found = const_cast<char*>(s_found.c_str());

    if (command == "QUIT") {
        void DoKillSwitch(void);
        DoKillSwitch();
        return true;
    }

    if (command == "MOVEWINDN") { // MOVE WINDOW DOWN (by swapping)
        int order1 = dbg.win_find_order((int)dbg.active_win);
        int order2 = dbg.win_next_by_order(order1);

        if (order1 >= 0 && order2 >= 0 && order1 < order2) {
            dbg.swap_order(order1,order2);
            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
        }

        return true;
    }

    if (command == "MOVEWINUP") { // MOVE WINDOW UP (by swapping)
        int order1 = dbg.win_find_order((int)dbg.active_win);
        int order2 = dbg.win_prev_by_order(order1);

        if (order1 >= 0 && order2 >= 0 && order1 > order2) {
            dbg.swap_order(order1,order2);
            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
        }

        return true;
    }

    if (command == "SHOWWIN") { // SHOW WINDOW <name>
        int win = dbg.name_to_win(found);
        if (win >= 0) {
            dbg.win_vis[win] = true;

            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
            return true;
        }
        else {
            LOG_MSG("No such window '%s'. Windows are: %s",found,dbg.windowlist_by_name().c_str());
            return false;
        }
    }

    if (command == "HIDEWIN") { // HIDE WINDOW <name>
        int win = dbg.name_to_win(found);
        if (win >= 0) {
            dbg.win_vis[win] = false;

            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
            return true;
        }
        else {
            LOG_MSG("No such window '%s'. Windows are: %s",found,dbg.windowlist_by_name().c_str());
            return false;
        }
    }

	if (command == "MEMDUMP") { // Dump memory to file
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		uint32_t num = GetHexValue(found,found); found++;
		SaveMemory(seg,ofs,num);
		return true;
	}

	if (command == "MEMDUMPBIN") { // Dump memory to file binary
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		uint32_t num = GetHexValue(found,found); found++;
		SaveMemoryBin(seg,ofs,num);
		return true;
	}

	if (command == "IV") { // Insert variable
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = (uint16_t)GetHexValue(found,found); found++;
		char name[16];
		for (int i=0; i<16; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[15] = 0;

		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Created debug var %s at %04X:%04X\n",name,seg,ofs);
		CDebugVar::InsertVariable(name,(PhysPt)GetAddress(seg,ofs));
		return true;
	}

	if (command == "SV") { // Save variables
		char name[13];
		for (int i=0; i<12; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[12] = 0;
		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Variable list save (%s) : %s.\n",name,(CDebugVar::SaveVars(name)?"ok":"failure"));
		return true;
	}

	if (command == "LV") { // load variables
		char name[13];
		for (int i=0; i<12; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[12] = 0;
		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Variable list load (%s) : %s.\n",name,(CDebugVar::LoadVars(name)?"ok":"failure"));
		return true;
	}

    if (command == "EV") { // echo value (for viewing contents through GetHexValue
        std::string cpptmp;
        char tmp[128];
        bool parsed;

        SkipSpace(found);
        DEBUG_ShowMsg("EV of '%s' is:",found);

        while (*found) {
            uint32_t value = GetHexValue(found,found,&parsed); SkipSpace(found);
            if (!parsed) {
                DEBUG_ShowMsg("GetHexValue parse error at %s",found);
                break;
            }
            sprintf(tmp,"%lx",(unsigned long)value);
            if (!cpptmp.empty()) cpptmp += " ";
            cpptmp += tmp;
        }

        DEBUG_ShowMsg("%s",cpptmp.c_str());
        return true;
    }
	
	if (command == "ADDLOG") {
		if(found && *found)	DEBUG_ShowMsg("NOTICE: %s\n",found);
		return true;
	}

	if (command == "SR") { // Set register value
		DEBUG_ShowMsg("DEBUG: Set Register %s.\n",(ChangeRegister(found)?"success":"failure"));
		return true;
	}

	if (command == "SM") { // Set memory with following values
		uint16_t seg = (uint16_t)GetHexValue(found,found);
        if (*found == ':') { // allow seg:off syntax
            found++;
            SkipSpace(found);
        }
		uint32_t ofs = GetHexValue(found,found); SkipSpace(found);
		uint16_t count = 0;
        bool parsed;

        while (*found) {
            char prefix = 'B';
            uint32_t value;

            /* allow d: w: b: prefixes */
            if ((*found == 'B' || *found == 'W' || *found == 'D') && found[1] == ':') {
                prefix = *found; found += 2;
                value = GetHexValue(found,found,&parsed);
            }
            else {
                value = GetHexValue(found,found,&parsed);
            }

            SkipSpace(found);
            if (!parsed) {
                DEBUG_ShowMsg("GetHexValue parse error at %s",found);
                break;
            }

            if (prefix == 'D') {
                mem_writed_checked((PhysPt)GetAddress(seg,ofs+count),value);
                count += 4;
            }
            else if (prefix == 'W') {
                mem_writew_checked((PhysPt)GetAddress(seg,ofs+count),value);
                count += 2;
            }
            else if (prefix == 'B') {
                mem_writeb_checked((PhysPt)GetAddress(seg,ofs+count),value);
                count++;
            }
        }

        if (count > 0)
            DEBUG_ShowMsg("DEBUG: Memory changed (%u bytes)\n",(unsigned int)count);

		return true;
	}

	if (command == "BP") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint::AddBreakpoint(seg,ofs,false);
		DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n",seg,ofs);
		return true;
	}

#if C_HEAVY_DEBUG

	if (command == "BPM") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint::AddMemBreakpoint(seg,ofs);
		DEBUG_ShowMsg("DEBUG: Set memory breakpoint at %04X:%04X\n",seg,ofs);
		return true;
	}

	if (command == "BPPM") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(seg,ofs);
		if (bp)	{
			bp->SetType(BKPNT_MEMORY_PROT);
			DEBUG_ShowMsg("DEBUG: Set prot-mode memory breakpoint at %04X:%08X\n",seg,ofs);
		}
		return true;
	}

	if (command == "BPLM") { // Add new breakpoint
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(0,ofs);
		if (bp) bp->SetType(BKPNT_MEMORY_LINEAR);
		DEBUG_ShowMsg("DEBUG: Set linear memory breakpoint at %08X\n",ofs);
		return true;
	}

#endif

	if (command == "BPINT") { // Add Interrupt Breakpoint
		uint8_t intNr	= (uint8_t)GetHexValue(found,found);
		bool all = !(*found);
		uint8_t valAH = (uint8_t)GetHexValue(found,found);
		if ((valAH==0x00) && (*found=='*' || all)) {
			CBreakpoint::AddIntBreakpoint(intNr,BPINT_ALL,BPINT_ALL,false);
			DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X\n",intNr);
		} else {
			all = !(*found);
			uint8_t valAL = (uint8_t)GetHexValue(found,found);
			if ((valAL==0x00) && (*found=='*' || all)) {
				CBreakpoint::AddIntBreakpoint(intNr,valAH,BPINT_ALL,false);
				DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X\n",intNr,valAH);
			} else {
				CBreakpoint::AddIntBreakpoint(intNr,valAH,valAL,false);
				DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X AL=%02X\n",intNr,valAH,valAL);
			}
		}
		return true;
	}

	if (command == "BPLIST") {
        DEBUG_BeginPagedContent();
		DEBUG_ShowMsg("Breakpoint list:\n");
		DEBUG_ShowMsg("-------------------------------------------------------------------------\n");
		CBreakpoint::ShowList();
        DEBUG_EndPagedContent();
		return true;
	}

	if (command == "BPDEL") { // Delete Breakpoints
		uint8_t bpNr	= (uint8_t)GetHexValue(found,found); 
		if ((bpNr==0x00) && (*found=='*')) { // Delete all
			CBreakpoint::DeleteAll();
			DEBUG_ShowMsg("DEBUG: Breakpoints deleted.\n");
		} else {
			// delete single breakpoint
			DEBUG_ShowMsg("DEBUG: Breakpoint deletion %s.\n",(CBreakpoint::DeleteByIndex(bpNr)?"success":"failure"));
		}
		return true;
	}

    if (command == "RUN") {
        DrawRegistersUpdateOld();
        debugging=false;

        logBuffSuppressConsole = false;
        if (logBuffSuppressConsoleNeedUpdate) {
            logBuffSuppressConsoleNeedUpdate = false;
            DEBUG_RefreshPage(0);
        }

        Bits DEBUG_NullCPUCore(void);

        CPU_Cycles = 1;
        inhibit_int_breakpoint = true;
        if (cpudecoder != DEBUG_NullCPUCore)
            (*cpudecoder)();

        inhibit_int_breakpoint = false;

        void DEBUG_DrawScreen(void);
        DEBUG_DrawScreen();

        CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
        DOSBOX_SetNormalLoop();	
        GFX_SetTitle(-1,-1,-1,is_paused);
        return true;
    }

    if (command == "RUNWATCH") {
        debug_running = true;
        DEBUG_DrawScreen();
        return true;
    }

    if (command == "A20") {
        void MEM_A20_Enable(bool enabled);
        bool MEM_A20_Enabled(void);

        command.clear();
		stream >> command;

        if (command == "ON" || command == "1")
            MEM_A20_Enable(true);
        else if (command == "OFF" || command == "0")
            MEM_A20_Enable(false);
        else if (command == "")
            DEBUG_ShowMsg("A20 gate is %s",MEM_A20_Enabled() ? "ON" : "OFF");
        else
            return false;

        return true;
    }

    if (command == "PIC") { // interrupt controller state/controls
        void DEBUG_PICSignal(int irq,bool raise);
        void DEBUG_PICMask(int irq,bool mask);
        void DEBUG_PICAck(int irq);
        void DEBUG_LogPIC(void);

        DEBUG_BeginPagedContent();

        command.clear();
		stream >> command;

        if (command == "MASKIRQ") {
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICMask(irq,true);
        }
        else if (command == "UNMASKIRQ") {
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICMask(irq,false);
        }
        else if (command == "ACKIRQ") { /* debugging: manually acknowledge an IRQ where the DOS program failed to do so */
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICAck(irq);
        }
        else if (command == "LOWERIRQ") { /* manually lower an IRQ signal */
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICSignal(irq,false);
        }
        else if (command == "RAISEIRQ") { /* manually raise an IRQ signal */
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICSignal(irq,true);
        }
        else if (command == "") {
            DEBUG_LogPIC();
        }
        else {
            DEBUG_EndPagedContent();
            return false;
        }

        DEBUG_EndPagedContent();

        return true;
    }

	if (command == "C") { // Set code overview
		uint16_t codeSeg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t codeOfs = GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Set code overview to %04X:%04X\n",codeSeg,codeOfs);
		codeViewData.useCS	= codeSeg;
		codeViewData.useEIP = codeOfs;
		codeViewData.cursorPos = 0;
		return true;
	}

	if (command == "D") { // Set data overview
		dataSeg = (uint16_t)GetHexValue(found,found); SkipSpace(found);
        if (*found == ':') { // allow seg:off syntax
            found++;
            SkipSpace(found);
        }
		dataOfs = GetHexValue(found,found); SkipSpace(found);
        dbg.set_data_view(DBGBlock::DATV_SEGMENTED);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

	if (command == "DV") { // Set data overview
        dataSeg = 0;
		dataOfs = GetHexValue(found,found); SkipSpace(found);
        dbg.set_data_view(DBGBlock::DATV_VIRTUAL);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

	if (command == "DP") { // Set data overview
        dataSeg = 0;
		dataOfs = GetHexValue(found,found); SkipSpace(found);
        dbg.set_data_view(DBGBlock::DATV_PHYSICAL);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

#if C_HEAVY_DEBUG

	if (command == "LOG") { // Create Cpu normal log file
		cpuLogType = 1;
		command = "logcode";
	}

	if (command == "LOGS") { // Create Cpu short log file
		cpuLogType = 0;
		command = "logcode";
	}

	if (command == "LOGL") { // Create Cpu long log file
		cpuLogType = 2;
		command = "logcode";
	}

	if (command == "LOGC") { // Create Cpu coverage log file
		cpuLogType = 3;
		command = "logcode";
	}

	if (command == "logcode") { //Shared code between all logs
		DEBUG_ShowMsg("DEBUG: Starting log\n");
		cpuLogFile.open("LOGCPU.TXT");
		if (!cpuLogFile.is_open()) {
			DEBUG_ShowMsg("DEBUG: Logfile couldn't be created.\n");
			return false;
		}
		//Initialize log object
		cpuLogFile << hex << noshowbase << setfill('0') << uppercase;
		cpuLog = true;
		cpuLogCounter = (int)GetHexValue(found,found);

		debugging = false;
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		DOSBOX_SetNormalLoop();	
		return true;
	}

#endif

	if (command == "INTT") { //trace int.
		uint8_t intNr = (uint8_t)GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Tracing INT %02X\n",intNr);
		CPU_HW_Interrupt(intNr);
		SetCodeWinStart();
		return true;
	}

	if (command == "INT") { // start int.
		uint8_t intNr = (uint8_t)GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Starting INT %02X\n",intNr);
		CBreakpoint::AddBreakpoint(SegValue(cs),reg_eip, true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip-1);
		debugging = false;
		DrawCode();
		mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
		DOSBOX_SetNormalLoop();
		CPU_HW_Interrupt(intNr);
		return true;
	}

	if (command == "SELINFO") {
		while (found[0] == ' ') found++;
		char out1[200],out2[200];
		GetDescriptorInfo(found,out1,out2);
		DEBUG_ShowMsg("SelectorInfo %s:\n%s\n%s\n",found,out1,out2);
		return true;
	}

	if (command == "DOS") {
		stream >> command;
		if (command == "MCBS") LogMCBS();
        else if (command == "KERN") LogDOSKernMem();
        else if (command == "XMS") LogXMS();
        else if (command == "EMS") LogEMS();
        else if (command == "FNKEY") LogFNKEY();
        else return false;

		return true;
	}

    if (command == "BIOS") {
        stream >> command;

        if (command == "MEM") LogBIOSMem();
        else return false;

        return true;
    }

    if (command == "VGA") {
        stream >> command;

        if (IS_PC98_ARCH) {
            DEBUG_ShowMsg("VGA debugger commands not available in PC-98 mode");
            return false;
        }

        if (command == "CRTC") {
            DEBUG_ShowMsg("VGA CRTC info: index=%02xh readonly=%u",vga.crtc.index,vga.crtc.read_only?1:0);
            DEBUG_ShowMsg("htotal=%02xh hdend=%02xh strhb=%02xh endhb=%02xh strrt=%02xh endrt=%02xh",
                vga.crtc.horizontal_total,              vga.crtc.horizontal_display_end,
                vga.crtc.start_horizontal_blanking,     vga.crtc.end_horizontal_blanking,
                vga.crtc.start_horizontal_retrace,      vga.crtc.end_horizontal_retrace);
            DEBUG_ShowMsg("vtotal=%02xh overflow=%02xh prerwscn=%02xh maxscnl=%02xh offset=%02xh",
                vga.crtc.vertical_total,                vga.crtc.overflow,
                vga.crtc.preset_row_scan,               vga.crtc.maximum_scan_line,
                vga.crtc.offset);
            DEBUG_ShowMsg("curs-st=%02xh curs-en=%02xh start-addr=%02x%02xh curs-loc=%02x%02xh",
                vga.crtc.cursor_start,                  vga.crtc.cursor_end,
                vga.crtc.start_address_high,            vga.crtc.start_address_low,
                vga.crtc.cursor_location_high,          vga.crtc.cursor_location_low);
            DEBUG_ShowMsg("strvrt=%02xh endvrt=%02xh vdend=%02xh underline=%02xh modectrl=%02xh",
                vga.crtc.vertical_retrace_start,        vga.crtc.vertical_retrace_end,
                vga.crtc.vertical_display_end,          vga.crtc.underline_location,
                vga.crtc.mode_control);
            DEBUG_ShowMsg("strvhb=%02xh endvhb=%02xh linecomp=%02xh",
                vga.crtc.start_vertical_blanking,       vga.crtc.end_vertical_blanking,
                vga.crtc.line_compare);
        }
        else if (command == "GC") {
            DEBUG_ShowMsg("VGA Graphics controller info: index=%02xh setreset=%02xh",vga.gfx.index,vga.gfx.set_reset);
            DEBUG_ShowMsg("enablesetreset=%02xh color-comp=%02xh data-rotate=%02xh rdmapsel=%02xh",
                vga.gfx.enable_set_reset,   vga.gfx.color_compare,  vga.gfx.data_rotate,
                vga.gfx.read_map_select);
            DEBUG_ShowMsg("mode=%02xh misc=%02xh color-dont-care=%02xh bitmask=%02xh",
                vga.gfx.mode,               vga.gfx.miscellaneous,  vga.gfx.color_dont_care,
                vga.gfx.bit_mask);
        }
        else if (command == "DAC") {
            // FIXME: Remove this var, use vga.dac.bits == 8 instead!
            extern bool vga_8bit_dac;

            DEBUG_ShowMsg("VGA DAC info: bits=%u pel_mask=%02xh pel_index[RGB]=%02xh",
                vga_8bit_dac ? 8 : 6,//FIXME
                vga.dac.pel_mask,       vga.dac.pel_index);
            DEBUG_ShowMsg("state=%02xh write_index=%02xh read_index=%02xh first_changed=%u",
                vga.dac.state,          vga.dac.write_index,
                vga.dac.read_index,     (unsigned int)vga.dac.first_changed);
            DEBUG_ShowMsg("hidac_counter=%u reg02=%02xh",
                vga.dac.hidac_counter,  vga.dac.reg02);
        }
        else if (command == "DACPAL") {
            std::string cpptmp;
            char tmp[64];

            DEBUG_BeginPagedContent();

            DEBUG_ShowMsg("VGA DAC palette (RGB):");
            for (unsigned int i=0;i < 256;i += 8) {
                sprintf(tmp,"%02x: ",i);
                cpptmp = tmp;

                for (unsigned int c=0;c < 8;c++) {
                    sprintf(tmp,"%02x%02x%02x%c",
                        vga.dac.rgb[i+c].red,
                        vga.dac.rgb[i+c].green,
                        vga.dac.rgb[i+c].blue,
                        c == 3 ? '-' : ' '
                    );
                    cpptmp += tmp;
                }

                DEBUG_ShowMsg("%s",cpptmp.c_str());
            }

            DEBUG_EndPagedContent();
        }
        else if (command == "SEQ") {
            DEBUG_ShowMsg("VGA Sequencer info: index=%02xh",vga.seq.index);
            DEBUG_ShowMsg("reset=%02xh clockmode=%02xh map_mask=%02xh charmapsel=%02xh memmode=%02xh",
                vga.seq.reset,      vga.seq.clocking_mode,      vga.seq.map_mask,
                vga.seq.character_map_select,                   vga.seq.memory_mode);
        }
        else if (command == "AC") {
            std::string cpptmp;
            char tmp[64];

            DEBUG_ShowMsg("VGA Attribute Controller info: attrindexmode=%u",vga.internal.attrindex?1:0);
            DEBUG_ShowMsg("mode_control=%02xh horz-pel-pan=%02xh overscan-color=%02xh",
                vga.attr.mode_control,  vga.attr.horizontal_pel_panning,
                vga.attr.overscan_color);
            DEBUG_ShowMsg("color-plane-en=%02xh color-select=%02xh index=%02xh",
                vga.attr.color_plane_enable,    vga.attr.color_select,  vga.attr.index);
            DEBUG_ShowMsg("disabled-by-index=%u disabled-by-idx1-bit5=%u index-written=%u",
                (vga.attr.disabled & 1) ? 1 : 0,
                (vga.attr.disabled & 2) ? 1 : 0,
                vga.internal.attrindex);

            cpptmp = " ";
            for (unsigned int i=0;i < 16;i++) {
                sprintf(tmp,"%02x%c",vga.attr.palette[i],((i&3) == 3 && i != 15) ? '-' : ' ');
                cpptmp += tmp;
            }

            DEBUG_ShowMsg("palette={%s}",cpptmp.c_str());
        }
        else if (command == "DRAW") {
            DEBUG_ShowMsg("VGA draw info: resizing=%u width=%lu height=%lu blocks=%lu",
                vga.draw.resizing?1:0,(unsigned long)vga.draw.width,(unsigned long)vga.draw.height,(unsigned long)vga.draw.blocks);
            DEBUG_ShowMsg("address=%lxh panning=%lu bytes-skip=%lu lin-mask=%lx pln-mask=%lx",
                (unsigned long)vga.draw.address,(unsigned long)vga.draw.panning,(unsigned long)vga.draw.bytes_skip,
                (unsigned long)vga.draw.linear_mask,(unsigned long)vga.draw.planar_mask);
            DEBUG_ShowMsg("addr-add=%lxh line-length=%lu addrline-total=%lu addrline=%lu",
                (unsigned long)vga.draw.address_add,(unsigned long)vga.draw.line_length,
                (unsigned long)vga.draw.address_line_total,(unsigned long)vga.draw.address_line);
            DEBUG_ShowMsg("line-total=%lu vblank-skip=%lu lines-done=%lu split-line=%lu",
                (unsigned long)vga.draw.lines_total,(unsigned long)vga.draw.vblank_skip,
                (unsigned long)vga.draw.lines_done,(unsigned long)vga.draw.split_line);
            DEBUG_ShowMsg("byte-pan-shft=%lu render-stop=%lu render-max=%lu scrn-ratio=%.3f",
                (unsigned long)vga.draw.byte_panning_shift,(unsigned long)vga.draw.render_step,
                (unsigned long)vga.draw.render_max,(double)vga.draw.screen_ratio);
            DEBUG_ShowMsg("blinking=%lu blink=%u char9dot=%u has-split=%u vret-trig=%u",
                (unsigned long)vga.draw.blinking,vga.draw.blink?1:0,
                vga.draw.char9dot?1:0,vga.draw.has_split?1:0,
                vga.draw.vret_triggered?1:0);
            DEBUG_ShowMsg("bpp=%lu curs-addr=%lxh curs-s=%u curs-e=%u curs-c=%u curs-d=%u curs-en=%u",
                (unsigned long)vga.draw.bpp,
                (unsigned long)vga.draw.cursor.address,
                vga.draw.cursor.sline,
                vga.draw.cursor.eline,
                vga.draw.cursor.count,
                vga.draw.cursor.delay,
                vga.draw.cursor.enabled);
// TODO: Remove vga.draw.refresh, appears to be unused
        }
        else if (command == "MODE") {
            DEBUG_ShowMsg("VGA mode info:");
            DEBUG_ShowMsg("mode=%s vref=%.3fHz href=%.3fHz chrclk=%.3fHz dotclk=%.3fHz",
                mode_texts[vga.mode],1000.0/vga.draw.delay.vtotal,1000.0/vga.draw.delay.htotal,
                vga.draw.clock,vga.draw.oscclock);
            DEBUG_ShowMsg("disp-start=%lxh real-start=%lxh retrace=%u scanlen=%lu cursor-start=%lxh",
                (unsigned long)vga.config.display_start,
                (unsigned long)vga.config.real_start,
                vga.config.retrace?1:0,
                (unsigned long)vga.config.scan_len,
                (unsigned long)vga.config.cursor_start);
            DEBUG_ShowMsg("line-compare=%lu chained=%u compat-chain4=%u pel-pan=%u hline-skip=%u",
                (unsigned long)vga.config.line_compare,
                vga.config.chained,
                (unsigned int)vga.config.compatible_chain4,
                (unsigned int)vga.config.pel_panning,
                (unsigned int)vga.config.hlines_skip);
            DEBUG_ShowMsg("byte-skip=%u addr-shift=%u rd-mode=%u wr-mode=%u rdmap-sel=%u",
                (unsigned int)vga.config.bytes_skip,
                (unsigned int)vga.config.addr_shift,
                (unsigned int)vga.config.read_mode,
                (unsigned int)vga.config.write_mode,
                (unsigned int)vga.config.read_map_select);
            DEBUG_ShowMsg("col-dont-care=%u color-compare=%u data-rotate=%u raster-op=%02xh",
                (unsigned int)vga.config.color_dont_care,
                (unsigned int)vga.config.color_compare,
                (unsigned int)vga.config.data_rotate,
                (unsigned int)vga.config.raster_op);
            DEBUG_ShowMsg("fbmsk=%x fmmsk=%x fnmmsk=%x fsr=%x fnesr=%x fesr=%x feasr=%x",
                (unsigned int)vga.config.full_bit_mask,
                (unsigned int)vga.config.full_map_mask,
                (unsigned int)vga.config.full_not_map_mask,
                (unsigned int)vga.config.full_set_reset,
                (unsigned int)vga.config.full_not_enable_set_reset,
                (unsigned int)vga.config.full_enable_set_reset,
                (unsigned int)vga.config.full_enable_and_set_reset);
        }
        else {
            return false;
        }

        return true;
   }

    if (command == "PC98") {
        stream >> command;

        if (!IS_PC98_ARCH) {
            DEBUG_ShowMsg("PC-98 debugger commands not available except in PC-98 mode");
            return false;
        }

        if (command == "MODE") {
            DEBUG_ShowMsg("Mode info:");
            DEBUG_ShowMsg("mode=%s vref=%.3fHz href=%.3fHz chrclk=%.3fHz dotclk=%.3fHz",
                mode_texts[vga.mode],1000.0/vga.draw.delay.vtotal,1000.0/vga.draw.delay.htotal,
                vga.draw.clock,vga.draw.oscclock);
        }
        else if (command == "GRAPHICS") {
            const auto &gdc = pc98_gdc[GDC_SLAVE];
            std::string cpptmp;

            cpptmp.clear();
            if (gdc_analog) {
                if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
                    cpptmp += "'256-color packed' ";
                else
                    cpptmp += "'16-color planar' ";
            }
            else {
                cpptmp += "'8-color planar' ";
            }

            if (egc_enable_enable) /* Port 6Ah, 0x06/0x07 to enable EGC enable commands 0x04/0x05 */
                cpptmp += "EGC-ENABL ";
            if (pc98_gdc_vramop & (1 << VOPBIT_EGC)) /* Port 6Ah, 0x04/0x05 */
                cpptmp += "EGC ";
            if (pc98_gdc_vramop & (2 << VOPBIT_GRCG)) /* Port 7Ch, bits [7:6]. bit 7 (2 << VOPBIT_GRCG) is the enable. */
                cpptmp += "GRCG ";

            if (gdc.display_enable)
                cpptmp += "ENABLED ";
            else
                cpptmp += "DISABLED ";

            if (gdc.doublescan) {
                if (pc98_graphics_hide_odd_raster_200line)
                    cpptmp += "200-LINE-EVEN-SCAN ";
                else
                    cpptmp += "DOUBLESCAN ";
            }

            if (pc98_pegc_linear_framebuffer_enabled())
                cpptmp += "PEGC-LFB ";

            if (pc98_gdc_vramop & (1 << VOPBIT_PEGC_PLANAR))
                cpptmp += "256-PLANAR ";

            DEBUG_ShowMsg("PC-98 graphics mode: %s",cpptmp.c_str());

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 page display: cpu=%u display-pending=%u display-active=%u pitch=%u",
                    (pc98_gdc_vramop & (1 << VOPBIT_ACCESS))?1:0,
                    GDC_display_plane_pending,
                    GDC_display_plane,
                    gdc.display_pitch);

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 status: gdc5mhz=%u vsync-int-trig=%u rowheight=%u lines-drawn=%u",
                gdc_5mhz_mode,GDC_vsync_interrupt,gdc.row_height,(unsigned int)vga.draw.lines_done);
            DEBUG_ShowMsg("  cur-row-line=%u cur-scan=0x%x cur-partition=%u/%u part-remline=%u",
                gdc.row_line,gdc.scan_address,gdc.display_partition,gdc.display_partition_mask+1,gdc.display_partition_rem_lines);
            DEBUG_ShowMsg("  vram-bound=%uKB",
                pc98_256kb_boundary?256:128);

            /*--------------------*/

            cpptmp.clear();
            {
                char tmp[16];
                for (unsigned int i=0;i < 16;i++) {
                    sprintf(tmp,"%02x%c",gdc.param_ram[i],((i&3) == 3 && i != 15) ? '-' : ' ');
                    cpptmp += tmp;
                }
            }
            DEBUG_ShowMsg("PC-98 GDC PRAM: wptr=%u %s",gdc.param_ram_wptr,cpptmp.c_str());

            /*---------------------*/
            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 GRCG: active=%u mode=%s tilecount=%u modereg(7Ch)=%02xh",
                (pc98_gdc_vramop & (2 << VOPBIT_GRCG))?1:0,
                (pc98_gdc_vramop & (1 << VOPBIT_GRCG))?"Read/Modify/Write":"TCR/TDW",
                pc98_gdc_tile_counter,
                pc98_gdc_modereg);

            /*---------------------*/
            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 GRGC tiles: [%02x %02x] [%02x %02x] [%02x %02x] [%02x %02x]",
                pc98_gdc_tiles[0].b[0],
                pc98_gdc_tiles[0].b[1],
                pc98_gdc_tiles[1].b[0],
                pc98_gdc_tiles[1].b[1],
                pc98_gdc_tiles[2].b[0],
                pc98_gdc_tiles[2].b[1],
                pc98_gdc_tiles[3].b[0],
                pc98_gdc_tiles[3].b[1]);
        }
        else if (command == "TEXT") {
            const auto &gdc = pc98_gdc[GDC_MASTER];
            std::string cpptmp;

            cpptmp.clear();
            if (gdc.display_enable)
                cpptmp += "ENABLED ";
            else
                cpptmp += "DISABLED ";

            if (gdc.doublescan)
                cpptmp += "DOUBLESCAN ";

            if (pc98_attr4_graphic)
                cpptmp += "SIMPLE-GRPH ";
            else
                cpptmp += "VERT-LINE ";

            DEBUG_ShowMsg("PC-98 text mode: %s",cpptmp.c_str());

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 display/cursor: pitch=%u curs-en=%u curs-blink=%u curs-blinkrate=%u",
                gdc.display_pitch,
                gdc.cursor_enable,
                gdc.cursor_blink,
                gdc.cursor_blink_rate);

            DEBUG_ShowMsg("  curs-blink-count=%u curs-blink-state=%u/4 crt-mode=%u",
                gdc.cursor_blink_count,
                gdc.cursor_blink_state,
                pc98_crt_mode?1:0);

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 status: gdc5mhz=%u vsync-int-trig=%u rowheight=%u lines-drawn=%u",
                gdc_5mhz_mode,GDC_vsync_interrupt,gdc.row_height,(unsigned int)vga.draw.lines_done);
            DEBUG_ShowMsg("  cur-row-line=%u cur-scan=0x%x cur-partition=%u/%u part-remline=%u",
                gdc.row_line,gdc.scan_address,gdc.display_partition,gdc.display_partition_mask+1,gdc.display_partition_rem_lines);

            /*--------------------*/

            cpptmp.clear();
            {
                char tmp[16];
                for (unsigned int i=0;i < 16;i++) {
                    sprintf(tmp,"%02x%c",gdc.param_ram[i],((i&3) == 3 && i != 15) ? '-' : ' ');
                    cpptmp += tmp;
                }
            }
            DEBUG_ShowMsg("PC-98 GDC PRAM: wptr=%u %s",gdc.param_ram_wptr,cpptmp.c_str());

            /*--------------------*/

            DEBUG_ShowMsg("PC-98 CG raster: row-scan=[start=%u/32 end-incl=%u/32] blank-at-in-charcell=%u/32",
                    pc98_text_first_row_scanline_start,
                    pc98_text_first_row_scanline_end,
                    pc98_text_row_scanline_blank_at);

            DEBUG_ShowMsg("PC-98 CG scrollregion: countstart=%u char-rows=%u lines=%u",
                    pc98_text_row_scroll_count_start,
                    pc98_text_row_scroll_num_lines,
                    pc98_text_row_scroll_lines);
        }
        else if (command == "CGIO") {
            DEBUG_ShowMsg("PC-98 CG I/O port state (A1h-A9h odd): char-row=%u char-code=0x%04x",
                    a1_font_char_offset,
                    a1_font_load_addr);
        }
        else if (command == "EGC") {
            DEBUG_ShowMsg("PC-98 EGC Raw registers:");
            DEBUG_ShowMsg("  4A0h even: %04xh %04xh %04xh %04xh",
                pc98_egc_raw_values[0],
                pc98_egc_raw_values[1],
                pc98_egc_raw_values[2],
                pc98_egc_raw_values[3]);
            DEBUG_ShowMsg("  4A8h even: %04xh %04xh %04xh %04xh bitplane-access=%02xh",
                pc98_egc_raw_values[4],
                pc98_egc_raw_values[5],
                pc98_egc_raw_values[6],
                pc98_egc_raw_values[7],
                pc98_egc_access);
            DEBUG_ShowMsg("  bg-color=%02xh bgcm=[%02xh %02xh] [%02xh %02xh] [%02xh %02xh] [%02xh %02xh]",
                pc98_egc_background_color,
                pc98_egc_bgcm[0].b[0],
                pc98_egc_bgcm[0].b[1],
                pc98_egc_bgcm[1].b[0],
                pc98_egc_bgcm[1].b[1],
                pc98_egc_bgcm[2].b[0],
                pc98_egc_bgcm[2].b[1],
                pc98_egc_bgcm[3].b[0],
                pc98_egc_bgcm[3].b[1]);
            DEBUG_ShowMsg("  fg-color=%02xh fgcm=[%02xh %02xh] [%02xh %02xh] [%02xh %02xh] [%02xh %02xh]",
                pc98_egc_foreground_color,
                pc98_egc_fgcm[0].b[0],
                pc98_egc_fgcm[0].b[1],
                pc98_egc_fgcm[1].b[0],
                pc98_egc_fgcm[1].b[1],
                pc98_egc_fgcm[2].b[0],
                pc98_egc_fgcm[2].b[1],
                pc98_egc_fgcm[3].b[0],
                pc98_egc_fgcm[3].b[1]);
            DEBUG_ShowMsg("  fgc-select=%d=%s lead-plane=%u srcmask=[%02xh %02xh]",
                pc98_egc_fgc,
                egc_fgc_modes[pc98_egc_fgc],
                pc98_egc_lead_plane,
                pc98_egc_srcmask[0],pc98_egc_srcmask[1]);
            DEBUG_ShowMsg("  mask=[%02xh %02xh] mask-effective=[%02xh %02xh] ROP=%02xh",
                pc98_egc_mask[0],pc98_egc_mask[1],
                pc98_egc_maskef[0],pc98_egc_maskef[1],
                pc98_egc_rop);
            DEBUG_ShowMsg("  compare-lead-plane=%u lightsource=%u shiftinput=%u regload=%u",
                pc98_egc_compare_lead,
                pc98_egc_lightsource,
                pc98_egc_shiftinput,
                pc98_egc_regload);
            DEBUG_ShowMsg("  shift-desc=%u shf-destbit=%u shf-srcbit=%u shf-length=%u",
                pc98_egc_shift_descend,
                pc98_egc_shift_destbit,
                pc98_egc_shift_srcbit,
                pc98_egc_shift_length);
            DEBUG_ShowMsg("  %s",
                pc98_egc_shift_debug_status().c_str());
        }
        else {
            return false;
        }

        return true;
    }

    if (command == "EMU") {
        stream >> command;

        if (command == "MEM") LogEMUMem();
        else if (command == "MACHINE") LogEMUMachine();
        else return false;

        return true;
    }

    if (command == "CALLBACKS") {
        DEBUG_BeginPagedContent();

        void DBG_CALLBACK_Dump(void);
        DBG_CALLBACK_Dump();

        DEBUG_EndPagedContent();
        return true;
    }

    if (command == "INP" || command == "INB") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint8_t r = IO_ReadB(port);
        DEBUG_ShowMsg("Result: %x",(unsigned int)r);
        return true;
    }

    if (command == "INW") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint16_t r = IO_ReadW(port);
        DEBUG_ShowMsg("Result: %x",(unsigned int)r);
        return true;
    }

    if (command == "IND") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint32_t r = IO_ReadD(port);
        DEBUG_ShowMsg("Result: %x",(unsigned int)r);
        return true;
    }

    if (command == "OUTP" || command == "OUTB") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint8_t r = (uint8_t)GetHexValue(found,found);
        IO_WriteB(port,r);
        return true;
    }

    if (command == "OUTW") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint16_t r = (uint16_t)GetHexValue(found,found);
        IO_WriteW(port,r);
        return true;
    }

    if (command == "OUTD") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint32_t r = (uint32_t)GetHexValue(found,found);
        IO_WriteD(port,r);
        return true;
    }

	if (command == "GDT") {LogGDT(); return true;}

	if (command == "LDT") {LogLDT(); return true;}

	if (command == "IDT") {LogIDT(); return true;}

	if (command == "PAGING") {LogPages(found); return true;}

	if (command == "CPU") {LogCPUInfo(); return true;}

	if (command == "FPU") {LogFPUInfo(); return true;}

	if (command == "INTVEC") {
		if (found[0] != 0) {
			OutputVecTable(found);
			return true;
		}
	}

	if (command == "INTHAND") {
		if (found[0] != 0) {
			uint8_t intNr = (uint8_t)GetHexValue(found,found);
			DEBUG_ShowMsg("DEBUG: Set code overview to interrupt handler %X\n",intNr);
            if (cpu.pmode) {
                Descriptor gate;
                if (cpu.idt.GetDescriptor((Bitu)intNr<<3u,gate)) {
                    codeViewData.useCS	= (uint16_t)gate.GetSelector();
                    codeViewData.useEIP = (uint32_t)gate.GetOffset();
                }
                else {
                    DEBUG_ShowMsg("INTHAND unable to retrieve vector");
                }
            }
            else {
                codeViewData.useCS	= mem_readw(intNr*4u+2u);
                codeViewData.useEIP = mem_readw(intNr*4u);
            }
			codeViewData.cursorPos = 0;
			return true;
		}
	}

	if(command == "EXTEND") { //Toggle additional data.
		showExtend = !showExtend;
		return true;
	}

	if(command == "TIMERIRQ") { //Start a timer irq
		DEBUG_RaiseTimerIrq(); 
		DEBUG_ShowMsg("Debug: Timer Int started.\n");
		return true;
	}


#if C_HEAVY_DEBUG
	if (command == "HEAVYLOG") { // Create Cpu log file
		logHeavy = !logHeavy;
		DEBUG_ShowMsg("DEBUG: Heavy cpu logging %s.\n",logHeavy?"on":"off");
		return true;
	}

	if (command == "ZEROPROTECT") { //toggle zero protection
		zeroProtect = !zeroProtect;
		DEBUG_ShowMsg("DEBUG: Zero code execution protection %s.\n",zeroProtect?"on":"off");
		return true;
	}

#endif
	if (command == "HELP" || command == "?") {
        DEBUG_BeginPagedContent();
		DEBUG_ShowMsg("Debugger commands (enter all values in hex or as register):\n");
		DEBUG_ShowMsg("Commands------------------------------------------------\n");
		DEBUG_ShowMsg("BP     [segment]:[offset] - Set breakpoint.\n");
		DEBUG_ShowMsg("BPINT  [intNr] *          - Set interrupt breakpoint.\n");
		DEBUG_ShowMsg("BPINT  [intNr] [ah] *     - Set interrupt breakpoint with ah.\n");
		DEBUG_ShowMsg("BPINT  [intNr] [ah] [al]  - Set interrupt breakpoint with ah and al.\n");
#if C_HEAVY_DEBUG
		DEBUG_ShowMsg("BPM    [segment]:[offset] - Set memory breakpoint (memory change).\n");
		DEBUG_ShowMsg("BPPM   [selector]:[offset]- Set pmode-memory breakpoint (memory change).\n");
		DEBUG_ShowMsg("BPLM   [linear address]   - Set linear memory breakpoint (memory change).\n");
#endif
		DEBUG_ShowMsg("BPLIST                    - List breakpoints.\n");
		DEBUG_ShowMsg("BPDEL  [bpNr] / *         - Delete breakpoint nr / all.\n");
		DEBUG_ShowMsg("C / D  [segment]:[offset] - Set code / data view address.\n");
		DEBUG_ShowMsg("DOS MCBS                  - Show Memory Control Block chain.\n");
        DEBUG_ShowMsg("DOS KERN                  - Show DOS kernel memory blocks.\n");
        DEBUG_ShowMsg("DOS XMS                   - Show XMS memory handles.\n");
        DEBUG_ShowMsg("DOS EMS                   - Show EMS memory handles.\n");
        DEBUG_ShowMsg("DOS FNKEY                 - Show PC-98 FnKey mapping.\n");
        DEBUG_ShowMsg("BIOS MEM                  - Show BIOS memory blocks.\n");
		DEBUG_ShowMsg("INT [nr] / INTT [nr]      - Execute / Trace into interrupt.\n");
#if C_HEAVY_DEBUG
		DEBUG_ShowMsg("LOG [num]                 - Write cpu log file.\n");
		DEBUG_ShowMsg("LOGS/LOGL/LOGC [num]      - Write short/long/cs:ip-only cpu log file.\n");
		DEBUG_ShowMsg("HEAVYLOG                  - Enable/Disable automatic cpu log when DOSBox-X exits.\n");
		DEBUG_ShowMsg("ZEROPROTECT               - Enable/Disable zero code execution detection.\n");
#endif
		DEBUG_ShowMsg("SR [reg] [value]          - Set register value. Multiple pairs allowed.\n");
		DEBUG_ShowMsg("SM [seg]:[off] [val] [.]..- Set memory with following values.\n");
        DEBUG_ShowMsg("EV [value [value] ...]    - Show register value(s).\n");
	
		DEBUG_ShowMsg("IV [seg]:[off] [name]     - Create var name for memory address.\n");
		DEBUG_ShowMsg("SV [filename]             - Save var list in file.\n");
		DEBUG_ShowMsg("LV [filename]             - Load var list from file.\n");
		
		DEBUG_ShowMsg("ADDLOG [message]          - Add message to the log file.\n");

		DEBUG_ShowMsg("MEMDUMP [seg]:[off] [len] - Write memory to file memdump.txt.\n");
		DEBUG_ShowMsg("MEMDUMPBIN [s]:[o] [len]  - Write memory to file memdump.bin.\n");
		DEBUG_ShowMsg("SELINFO [segName]         - Show selector info.\n");

		DEBUG_ShowMsg("INTVEC [filename]         - Writes interrupt vector table to file.\n");
		DEBUG_ShowMsg("INTHAND [intNum]          - Set code view to interrupt handler.\n");

		DEBUG_ShowMsg("CPU                       - Display CPU status information.\n");
        DEBUG_ShowMsg("FPU                       - Display FPU status information.\n");
		DEBUG_ShowMsg("GDT                       - Lists descriptors of the GDT.\n");
		DEBUG_ShowMsg("LDT                       - Lists descriptors of the LDT.\n");
		DEBUG_ShowMsg("IDT                       - Lists descriptors of the IDT.\n");
		DEBUG_ShowMsg("PAGING [page]             - Display content of page table.\n");
		DEBUG_ShowMsg("EXTEND                    - Toggle additional info.\n");
		DEBUG_ShowMsg("TIMERIRQ                  - Run the system timer.\n");

        DEBUG_ShowMsg("IN[P|W|D] [port]          - I/O port read byte/word/dword.\n");
        DEBUG_ShowMsg("OUT[P|W|D] [port] [data]  - I/O port write byte/word/dword.\n");

		DEBUG_ShowMsg("HELP                      - Help\n");
		DEBUG_ShowMsg("Keys------------------------------------------------\n");
		DEBUG_ShowMsg("F3/F6                     - Previous command in history.\n");
		DEBUG_ShowMsg("F4/F7                     - Next command in history.\n");
		DEBUG_ShowMsg("F5                        - Run.\n");
		DEBUG_ShowMsg("F8                        - Toggle printable characters.\n");
		DEBUG_ShowMsg("F9                        - Set/Remove breakpoint.\n");
		DEBUG_ShowMsg("F10/F11                   - Step over / trace into instruction.\n");
		DEBUG_ShowMsg("ALT + D/E/S/X/B           - Set data view to DS:SI/ES:DI/SS:SP/DS:DX/ES:BX.\n");
		DEBUG_ShowMsg("Escape                    - Clear input line.");
		DEBUG_ShowMsg("Up/Down                   - Scroll up/down in the current window.\n");
		DEBUG_ShowMsg("Page Up/Down              - Page up/down in the current window.\n");
		DEBUG_ShowMsg("Home/End                  - Move to begin/end of the current window.\n");
        DEBUG_ShowMsg("TAB                       - Select next window\n");
        DEBUG_EndPagedContent();

		return true;
	}

	return false;
}

char* AnalyzeInstruction(char* inst, bool saveSelector) {
	static char result[256];

	char instu[256];

	strcpy(instu,inst);
	upcase(instu);

	result[0] = 0;
	char* pos = strchr(instu,'[');
	if (pos) {
		char prefix[3];
		uint16_t seg;
		// Segment prefix ?
		if (*(pos-1)==':') {
			char* segpos = pos-3;
			prefix[0] = tolower(*segpos);
			prefix[1] = tolower(*(segpos+1));
			prefix[2] = 0;
			seg = (uint16_t)GetHexValue(segpos,segpos);
		} else {
			if (strstr(pos,"SP") || strstr(pos,"BP")) {
				seg = SegValue(ss);
				strcpy(prefix,"ss");
			} else {
				seg = SegValue(ds);
				strcpy(prefix,"ds");
			}
		}

		pos++;
		uint32_t adr = GetHexValue(pos,pos);
		while (*pos!=']') {
			if (*pos=='+') {
				pos++;
				adr += GetHexValue(pos,pos);
			} else if (*pos=='-') {
				pos++;
				adr -= GetHexValue(pos,pos);
			} else
				pos++;
		}
		uint32_t address = (uint32_t)GetAddress(seg,adr);
		if (!(get_tlb_readhandler(address)->flags & PFLAG_INIT)) {
			static char outmask[] = "%s:[%04X]=%02X";

			if (cpu.pmode) outmask[6] = '8';
				switch (DasmLastOperandSize()) {
				case 8 : {	uint8_t val = mem_readb(address);
							outmask[12] = '2';
							sprintf(result,outmask,prefix,adr,val);
						}	break;
				case 16: {	uint16_t val = mem_readw(address);
							outmask[12] = '4';
							sprintf(result,outmask,prefix,adr,val);
						}	break;
				case 32: {	uint32_t val = mem_readd(address);
							outmask[12] = '8';
							sprintf(result,outmask,prefix,adr,val);
						}	break;
			}
		} else {
			sprintf(result,"[illegal]");
		}
		// Variable found ?
		CDebugVar* var = CDebugVar::FindVar(address);
		if (var) {
			// Replace occurrence
			char* pos1 = strchr(inst,'[');
			char* pos2 = strchr(inst,']');
			if (pos1 && pos2) {
				char temp[256];
				strcpy(temp,pos2);				// save end
				pos1++; *pos1 = 0;				// cut after '['
				strcat(inst,var->GetName());	// add var name
				strcat(inst,temp);				// add end
			}
		}
		// show descriptor info, if available
		if ((cpu.pmode) && saveSelector) {
			strcpy(curSelectorName,prefix);
		}
	}
	// If it is a callback add additional info
	pos = strstr(inst,"callback");
	if (pos) {
		pos += 9;
		Bitu nr = GetHexValue(pos,pos);
		const char* descr = CALLBACK_GetDescription(nr);
		if (descr) {
			strcat(inst,"  ("); strcat(inst,descr); strcat(inst,")");
		}
	}
	// Must be a jump
	if (instu[0] == 'J')
	{
		bool jmp = false;
		switch (instu[1]) {
		case 'A' :	{	jmp = (get_CF()?false:true) && (get_ZF()?false:true); // JA
					}	break;
		case 'B' :	{	if (instu[2] == 'E') {
							jmp = (get_CF()?true:false) || (get_ZF()?true:false); // JBE
						} else {
							jmp = get_CF()?true:false; // JB
						}
					}	break;
		case 'C' :	{	if (instu[2] == 'X') {
							jmp = reg_cx == 0; // JCXZ
						} else {
							jmp = get_CF()?true:false; // JC
						}
					}	break;
		case 'E' :	{	jmp = get_ZF()?true:false; // JE
					}	break;
		case 'G' :	{	if (instu[2] == 'E') {
							jmp = (get_SF()?true:false)==(get_OF()?true:false); // JGE
						} else {
							jmp = (get_ZF()?false:true) && ((get_SF()?true:false)==(get_OF()?true:false)); // JG
						}
					}	break;
		case 'L' :	{	if (instu[2] == 'E') {
							jmp = (get_ZF()?true:false) || ((get_SF()?true:false)!=(get_OF()?true:false)); // JLE
						} else {
							jmp = (get_SF()?true:false)!=(get_OF()?true:false); // JL
						}
					}	break;
		case 'M' :	{	jmp = true; // JMP
					}	break;
		case 'N' :	{	switch (instu[2]) {
						case 'B' :	
						case 'C' :	{	jmp = get_CF()?false:true;	// JNB / JNC
									}	break;
						case 'E' :	{	jmp = get_ZF()?false:true;	// JNE
									}	break;
						case 'O' :	{	jmp = get_OF()?false:true;	// JNO
									}	break;
						case 'P' :	{	jmp = get_PF()?false:true;	// JNP
									}	break;
						case 'S' :	{	jmp = get_SF()?false:true;	// JNS
									}	break;
						case 'Z' :	{	jmp = get_ZF()?false:true;	// JNZ
									}	break;
						}
					}	break;
		case 'O' :	{	jmp = get_OF()?true:false; // JO
					}	break;
		case 'P' :	{	if (instu[2] == 'O') {
							jmp = get_PF()?false:true; // JPO
						} else {
							jmp = get_SF()?true:false; // JP / JPE
						}
					}	break;
		case 'S' :	{	jmp = get_SF()?true:false; // JS
					}	break;
		case 'Z' :	{	jmp = get_ZF()?true:false; // JZ
					}	break;
		}
		if (jmp) {
			pos = strchr(instu,'$');
			if (pos) {
				pos = strchr(instu,'+');
				if (pos) {
					strcpy(result,"(down)");
				} else {
					strcpy(result,"(up)");
				}
			}
		} else {
			sprintf(result,"(no jmp)");
		}
	}
	return result;
}

// data window
void win_data_ui_down(int count) {
    if (count > 0)
        dataOfs += (unsigned)count * 16;
}

void win_data_ui_up(int count) {
    if (count > 0)
        dataOfs -= (unsigned)count * 16;
}

// code window
void win_code_ui_down(int count) {
    if (dbg.win_code != NULL) {
        int y,x;

        getmaxyx(dbg.win_code,y,x);

        while (count-- > 0) {
            if (codeViewData.cursorPos < (y-1)) codeViewData.cursorPos++;
            else codeViewData.useEIP += codeViewData.firstInstSize;
        }
    }
}

void win_code_ui_up(int count) {
    if (dbg.win_code != NULL) {
        int y,x;

        getmaxyx(dbg.win_code,y,x);

        (void)y; // SET, BUT UNUSED

        while (count-- > 0) {
            if (codeViewData.cursorPos>0)
                codeViewData.cursorPos--;
            else {
                uint32_t newEIP = codeViewData.useEIP - 1;
                if(codeViewData.useEIP) {
                    Bitu bytes = 0;
                    char dline[200];
                    for (; bytes < 10; bytes++) {
                        PhysPt start = (PhysPt)GetAddress(codeViewData.useCS,newEIP);
                        Bitu size = DasmI386(dline, start, newEIP, cpu.code.big);
                        if(codeViewData.useEIP == newEIP+size) break;
                        newEIP--;
                    }
                    if (bytes>=10) newEIP = codeViewData.useEIP - 1;
                }
                codeViewData.useEIP = newEIP;
            }
        }
    }
}

#ifdef WIN32
extern "C" INPUT_RECORD * _pdcurses_hax_inputrecord(void);
#endif

uint32_t DEBUG_CheckKeys(void) {
	Bits ret=0;
	bool numberrun = false;
	bool skipDraw = false;
	int key=getch();

    if (key == KEY_RESIZE) {
#ifdef WIN32 /* BUG: pdcurses notifies us immediately upon getting a resize event but does not update it's
                     internal structures to reflect the new console size. For example a Win32 console event
                     reporting a change to 80x41 would still return 80x40 if we asked pdcurses right now.
                     To make resizing the console window less painful, look at pdcurses internal structure
                     directly instead.

                     Note that we need to call resize_term() or pdcurses will never notify us about console
                     resize again. */
            /* FIXME: Windows will notify us if the user enlarges the window vertically, but will NOT notify
                      us if the user vertically shrinks the window (adds scrollbar instead). How do we disable
                      that behavior? */
        {
            INPUT_RECORD *r = _pdcurses_hax_inputrecord();
            if (r->EventType == WINDOW_BUFFER_SIZE_EVENT) {
                resize_term(r->Event.WindowBufferSizeEvent.dwSize.Y, r->Event.WindowBufferSizeEvent.dwSize.X);
            }
        }
#endif
        void DEBUG_GUI_OnResize(void);
        DEBUG_GUI_OnResize();
        DEBUG_DrawScreen();
        return 0;
    }
	
	if (key >='1' && key <='5' && strlen(codeViewData.inputStr) == 0) {
		const int32_t v[] ={5,500,1000,5000,10000};
		CPU_Cycles= v[key - '1'];

		skipFirstInstruction = true;

		ret = (*cpudecoder)();
		SetCodeWinStart();

		/* Setup variables so we end up at the proper ret processing */
		numberrun = true;

		// Don't redraw the screen if it's going to get redrawn immediately
		// afterwards, to avoid resetting oldregs.
		if (ret == (Bits)debugCallback)
			skipDraw = true;
		key = -1;
	}

	if (key>0 || numberrun) {
#if defined(WIN32) && defined(__PDCURSES__)
		switch (key) {
		case PADENTER:	key=0x0A;	break;
		case PADSLASH:	key='/';	break;
		case PADSTAR:	key='*';	break;
		case PADMINUS:	key='-';	break;
		case PADPLUS:	key='+';	break;
		case ALT_D:
			if (ungetch('D') != ERR) key=27;
			break;
		case ALT_E:
			if (ungetch('E') != ERR) key=27;
			break;
		case ALT_X:
			if (ungetch('X') != ERR) key=27;
			break;
		case ALT_B:
			if (ungetch('B') != ERR) key=27;
			break;
		case ALT_S:
			if (ungetch('S') != ERR) key=27;
			break;
		}
#endif
		switch (toupper(key)) {
		case 27:	// escape (a bit slow): Clears line. and processes alt commands.
			key=getch();
			if(key < 0) { //Purely escape Clear line
				ClearInputLine();
				break;
			}

			switch(toupper(key)) {
			case 'D' : // ALT - D: DS:SI
				dataSeg = SegValue(ds);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_esi;
				else dataOfs = reg_si;
				break;
			case 'E' : //ALT - E: es:di
				dataSeg = SegValue(es);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_edi;
				else dataOfs = reg_di;
				break;
			case 'X': //ALT - X: ds:dx
				dataSeg = SegValue(ds);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_edx;
				else dataOfs = reg_dx;
				break;
			case 'B' : //ALT -B: es:bx
				dataSeg = SegValue(es);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_ebx;
				else dataOfs = reg_bx;
				break;
			case 'S': //ALT - S: ss:sp
				dataSeg = SegValue(ss);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_esp;
				else dataOfs = reg_sp;
				break;
			default:
				break;
			}
			break;
        case KEY_PPAGE: // page up
            switch (dbg.active_win) {
                case DBGBlock::WINI_CODE:
                    if (dbg.win_code != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_code,h,w);
                        win_code_ui_up(h-1);
                    }
                    break;
                case DBGBlock::WINI_DATA:
                     if (dbg.win_data != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_data,h,w);
                        win_data_ui_up(h);
                    }
                    break;
                case DBGBlock::WINI_OUT:
                    if (dbg.win_out != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_out,h,w);
                        DEBUG_RefreshPage(-h);
                    }
                    break;
            }
            break;

        case KEY_NPAGE:	// page down
            switch (dbg.active_win) {
                case DBGBlock::WINI_CODE:
                    if (dbg.win_code != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_code,h,w);
                        win_code_ui_down(h-1);
                    }
                    break;
                case DBGBlock::WINI_DATA:
                     if (dbg.win_data != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_data,h,w);
                        win_data_ui_down(h);
                    }
                    break;
                case DBGBlock::WINI_OUT:
                    if (dbg.win_out != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_out,h,w);
                        DEBUG_RefreshPage(h);
                    }
                    break;
            }
            break;

		case KEY_DOWN:	// down
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        win_code_ui_down(1);
                        break;
                    case DBGBlock::WINI_DATA:
                        win_data_ui_down(1);
                        break;
                    case DBGBlock::WINI_OUT:
                        DEBUG_RefreshPage(1);
                        break;
                }
                break;
        case KEY_UP:	// up 
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        win_code_ui_up(1);
                        break;
                    case DBGBlock::WINI_DATA:
                        win_data_ui_up(1);
                        break;
                    case DBGBlock::WINI_OUT:
                        DEBUG_RefreshPage(-1);
                        break;
                }
				break;

		case KEY_HOME:	// Home
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        // and do what?
                        break;
                    case DBGBlock::WINI_DATA:
                        // and do what?
                        break;
                    case DBGBlock::WINI_OUT:
                        void DEBUG_ScrollHomeOutput(void);
                        DEBUG_ScrollHomeOutput();
                        break;
                }
				break;

		case KEY_END:	// End
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        // and do what?
                        break;
                    case DBGBlock::WINI_DATA:
                        // and do what?
                        break;
                    case DBGBlock::WINI_OUT:
                        void DEBUG_ScrollToEndOutput(void);
                        DEBUG_ScrollToEndOutput();
                        break;
                }
				break;

        case KEY_IC:	// Insert: toggle insert/overwrite
				codeViewData.ovrMode = !codeViewData.ovrMode;
				break;
		case KEY_LEFT:	// move to the left in command line
				if (codeViewData.inputPos > 0) codeViewData.inputPos--;
 				break;
		case KEY_RIGHT:	// move to the right in command line
				if (codeViewData.inputStr[codeViewData.inputPos]) codeViewData.inputPos++;
				break;
		case KEY_F(6):	// previous command (f1-f4 generate rubbish at my place)
		case KEY_F(3):	// previous command
				if (histBuffPos == histBuff.begin()) break;
				if (histBuffPos == histBuff.end()) {
					// copy inputStr to suspInputStr so we can restore it
					safe_strncpy(codeViewData.suspInputStr, codeViewData.inputStr, sizeof(codeViewData.suspInputStr));
				}
				safe_strncpy(codeViewData.inputStr,(*--histBuffPos).c_str(),sizeof(codeViewData.inputStr));
				codeViewData.inputPos = (int)strlen(codeViewData.inputStr);
				break;
		case KEY_F(7):	// next command (f1-f4 generate rubbish at my place)
		case KEY_F(4):	// next command
				if (histBuffPos == histBuff.end()) break;
				if (++histBuffPos != histBuff.end()) {
					safe_strncpy(codeViewData.inputStr,(*histBuffPos).c_str(),sizeof(codeViewData.inputStr));
				} else {
					// copy suspInputStr back into inputStr
					safe_strncpy(codeViewData.inputStr, codeViewData.suspInputStr, sizeof(codeViewData.inputStr));
				}
				codeViewData.inputPos = (int)strlen(codeViewData.inputStr);
				break;
		case KEY_F(5):	// Run Program
                DrawRegistersUpdateOld();
				debugging=false;
				DrawCode();
                DrawInput();
                logBuffSuppressConsole = false;
                if (logBuffSuppressConsoleNeedUpdate) {
                    logBuffSuppressConsoleNeedUpdate = false;
                    DEBUG_RefreshPage(0);
                }

                Bits DEBUG_NullCPUCore(void);

				CPU_Cycles = 1;
                inhibit_int_breakpoint = true;
                if (cpudecoder == DEBUG_NullCPUCore)
                    ret = -1; /* DEBUG_Loop() must exit */
                else
                    ret = (*cpudecoder)();

                inhibit_int_breakpoint = false;
                mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);

				skipFirstInstruction = true; // for heavy debugger
				CPU_Cycles = 1;

				// ensure all breakpoints are activated
				CBreakpoint::ActivateBreakpoints();

				skipDraw = true; // don't update screen after this instruction

				DOSBOX_SetNormalLoop();
				break;
		case KEY_F(8):	// Toggle printable characters
				showPrintable = !showPrintable;
				break;
		case KEY_F(9):	// Set/Remove Breakpoint
				if (CBreakpoint::IsBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs)) {
					if (CBreakpoint::DeleteBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs))
						DEBUG_ShowMsg("DEBUG: Breakpoint deletion success.\n");
					else
						DEBUG_ShowMsg("DEBUG: Failed to delete breakpoint.\n");
				}
				else {
					CBreakpoint::AddBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs, false);
					DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n",codeViewData.cursorSeg,codeViewData.cursorOfs);
				}
				break;
		case KEY_F(10):	// Step over inst
                DrawRegistersUpdateOld();
				if (StepOver()) {
					mustCompleteInstruction = true;
					inhibit_int_breakpoint = true;
                    skipFirstInstruction = true; // for heavy debugger
					CPU_Cycles = 1;
					ret=(*cpudecoder)();
                    inhibit_int_breakpoint = false;
					mustCompleteInstruction = false;

					DOSBOX_SetNormalLoop();

					// ensure all breakpoints are activated
					CBreakpoint::ActivateBreakpoints();
					skipDraw = true;
					break;
				}
				// If we aren't stepping over something, do a normal step.
				// NB: Fall-through
		case KEY_F(11):	// trace into
                DrawRegistersUpdateOld();
				exitLoop = false;
				skipFirstInstruction = true; // for heavy debugger
				mustCompleteInstruction = true;
				CPU_Cycles = 1;
				ret = (*cpudecoder)();
				mustCompleteInstruction = false;
				SetCodeWinStart();
				break;
        case 0x09: //TAB
                void DBGUI_NextWindow(void);
                DBGUI_NextWindow();
                break;
		case 0x0A: //Parse typed Command
                if (codeViewData.inputPos > 0) {
                    codeViewData.inputStr[MAXCMDLEN] = '\0';
                    if(ParseCommand(codeViewData.inputStr)) {
                        char* cmd = ltrim(codeViewData.inputStr);
                        if (histBuff.empty() || *--histBuff.end()!=cmd)
                            histBuff.push_back(cmd);
                        if (histBuff.size() > MAX_HIST_BUFFER) histBuff.pop_front();
                        histBuffPos = histBuff.end();
                        ClearInputLine();
                    } else {
                        DEBUG_ShowMsg("*** Debugger command not recognized");
                        codeViewData.inputPos = (int)strlen(codeViewData.inputStr);
                    }
                }
				break;
		case KEY_BACKSPACE: //backspace (linux)
		case 0x7f:	// backspace in some terminal emulators (linux)
		case 0x08:	// delete 
				if (codeViewData.inputPos == 0) break;
				codeViewData.inputPos--;
				// fallthrough
		case KEY_DC: // delete character
				if ((codeViewData.inputPos<0) || (codeViewData.inputPos>=MAXCMDLEN)) break;
				if (codeViewData.inputStr[codeViewData.inputPos] != 0) {
						codeViewData.inputStr[MAXCMDLEN] = '\0';
						for(char* p=&codeViewData.inputStr[codeViewData.inputPos];(*p=*(p+1));p++) {}
				}
 				break;
		default:
				if ((key>=32) && (key<127)) {
					if ((codeViewData.inputPos<0) || (codeViewData.inputPos>=MAXCMDLEN)) break;
					codeViewData.inputStr[MAXCMDLEN] = '\0';
					if (codeViewData.inputStr[codeViewData.inputPos] == 0) {
							codeViewData.inputStr[codeViewData.inputPos++] = char(key);
							codeViewData.inputStr[codeViewData.inputPos] = '\0';
					} else if (!codeViewData.ovrMode) {
						int len = (int) strlen(codeViewData.inputStr);
						if (len < MAXCMDLEN) {
							for(len++;len>codeViewData.inputPos;len--)
								codeViewData.inputStr[len]=codeViewData.inputStr[len-1];
							codeViewData.inputStr[codeViewData.inputPos++] = char(key);
						}
					} else {
						codeViewData.inputStr[codeViewData.inputPos++] = char(key);
					}
				} else if (key==killchar()) {
					ClearInputLine();
				}
				break;
		}
		if (ret<0) return (uint32_t)ret;
		if (ret>0) {
			if (GCC_UNLIKELY(ret >= (Bits)CB_MAX)) 
				ret = 0;
			else
				ret = (Bits)(*CallBack_Handlers[ret])();
			if (ret) {
				exitLoop=true;
				CPU_Cycles=CPU_CycleLeft=0;
				return (uint32_t)ret;
			}
		}
		ret=0;
		if (!skipDraw)
			DEBUG_DrawScreen();
	}
	return (uint32_t)ret;
}

Bitu DEBUG_LastRunningUpdate = 0;

LoopHandler *DOSBOX_GetLoop(void);
Bitu DEBUG_Loop(void);

void DEBUG_Wait(void) {
    while (DOSBOX_GetLoop() == DEBUG_Loop)
        DOSBOX_RunMachine();
}

Bits DEBUG_NullCPUCore(void) {
    return CBRET_NONE;
}

void DEBUG_WaitNoExecute(void) {
    /* the caller uses this version to indicate a fatal error
     * in a condition where single-stepping or executing any
     * more x86 instructions is very unwise */
    auto oldcore = cpudecoder;
    cpudecoder = DEBUG_NullCPUCore;
    DEBUG_Wait();
    cpudecoder = oldcore;
}

Bitu DEBUG_Loop(void) {
    if (debug_running) {
        Bitu now = SDL_GetTicks();

        if ((DEBUG_LastRunningUpdate + 33) < now) {
            DEBUG_LastRunningUpdate = now;
            SetCodeWinStart();
            DEBUG_DrawScreen();
        }

        return old_loop();
    }
    else {
        //TODO Disable sound
        GFX_Events();
        // Interrupt started ? - then skip it
        uint16_t oldCS	= SegValue(cs);
        uint32_t oldEIP	= reg_eip;
        PIC_runIRQs();
        SDL_Delay(1);
        if ((oldCS!=SegValue(cs)) || (oldEIP!=reg_eip)) {
            CBreakpoint::AddBreakpoint(oldCS,oldEIP,true);
            CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
            debugging=false;

            logBuffSuppressConsole = false;
            if (logBuffSuppressConsoleNeedUpdate) {
                logBuffSuppressConsoleNeedUpdate = false;
                DEBUG_RefreshPage(0);
            }

			mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
            DOSBOX_SetNormalLoop();
            DrawRegistersUpdateOld();
            return 0;
        }

        /* between DEBUG_Enable and DEBUG_Loop CS:EIP can change */
        if (check_rescroll) {
            uint16_t ocs;
            uint32_t oip;
            int ocr;

            check_rescroll = false;
            ocs = codeViewData.useCS;
            oip = codeViewData.useEIP;
            ocr = codeViewData.cursorPos;
            SetCodeWinStart();
            if (ocs != codeViewData.useCS ||
                    oip != codeViewData.useEIP) {
                DEBUG_DrawScreen();
            }
            else {
                /* SetCodeWinStart() resets cursor position */
                codeViewData.cursorPos = ocr;
            }
        }

        if (logBuffSuppressConsoleNeedUpdate) {
            logBuffSuppressConsoleNeedUpdate = false;
            DEBUG_RefreshPage(0);
        }

    	return DEBUG_CheckKeys();
    }
}

void DEBUG_FlushInput(void);

static bool hidedebugger=false;
void DEBUG_Enable_Handler(bool pressed) {
	if (!pressed || control->opt_display2)
		return;

#if defined(WIN32)
    if (hidedebugger) {
        ShowWindow(GetConsoleWindow(), SW_SHOW);
        hidedebugger=false;
    }
#endif

    /* this command is now a toggle! */
    /* However this should break back into the debugger if RUNWATCH is active */
    if (debug_running) {
        debug_running = false;
        DrawRegistersUpdateOld();
        SetCodeWinStart();
        DEBUG_DrawScreen();
        return;
    }
    else if (debugging) {
        DrawRegistersUpdateOld();
        debugging=false;
#if defined(WIN32)
        hidedebugger=true;
        ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

        logBuffSuppressConsole = false;
        if (logBuffSuppressConsoleNeedUpdate) {
            logBuffSuppressConsoleNeedUpdate = false;
            DEBUG_RefreshPage(0);
        }

        void DEBUG_DrawScreen(void);
        DEBUG_DrawScreen();

        CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
        DOSBOX_SetNormalLoop();	
        GFX_SetTitle(-1,-1,-1,is_paused);
        return;
    }

	static bool showhelp=false;

#if defined(MACOSX) || defined(LINUX)
	/* Mac OS X does not have a console for us to just allocate on a whim like Windows does.
	   So the debugger interface is useless UNLESS the user has started us from a terminal
	   (whether over SSH or from the Terminal app). */
    bool allow = true;

    if (!isatty(0) || !isatty(1) || !isatty(2))
	    allow = false;

    if (!allow) {
# if defined(MACOSX)
	    LOG_MSG("Debugger in Mac OS X is not available unless you start DOSBox-X from a terminal or from the Terminal application");
# else
	    LOG_MSG("Debugger is not available unless you start DOSBox-X from a terminal");
# endif
	    return;
    }
#endif

    CPU_CycleLeft+=CPU_Cycles;
    CPU_Cycles=0;

    logBuffSuppressConsole = true;

    LoopHandler *ol = DOSBOX_GetLoop();
    if (ol != DEBUG_Loop) old_loop = ol;

	debugging=true;
    debug_running=false;
    check_rescroll=true;
    DrawRegistersUpdateOld();
    DEBUG_SetupConsole();
    DEBUG_FlushInput();
	SetCodeWinStart();
	DEBUG_DrawScreen();
	DOSBOX_SetLoop(&DEBUG_Loop);
	mainMenu.get_item("mapper_debugger").check(true).refresh_item(mainMenu);
	if(!showhelp) { 
		showhelp=true;
		DEBUG_ShowMsg("***| TYPE HELP (+ENTER) TO GET AN OVERVIEW OF ALL COMMANDS |***\n");
	}
	KEYBOARD_ClrBuffer();
    GFX_SetTitle(-1,-1,-1,false);
}

void DEBUG_DrawScreen(void) {
	DrawData();
	DrawCode();
    DrawInput();
	DrawRegisters();
	DrawVariables();
}

static void DEBUG_RaiseTimerIrq(void) {
	PIC_ActivateIRQ(0);
}

// Display the content of the MCB chain starting with the MCB at the specified segment.
static void LogMCBChain(uint16_t mcb_segment) {
	DOS_MCB mcb(mcb_segment);
	char filename[9]; // 8 characters plus a terminating NUL
	const char *psp_seg_note;
	uint16_t DOS_dataOfs = static_cast<uint16_t>(dataOfs); //Realmode addressing only
	PhysPt dataAddr = PhysMake(dataSeg,DOS_dataOfs);// location being viewed in the "Data Overview"

	// loop forever, breaking out of the loop once we've processed the last MCB
	while (true) {
		// verify that the type field is valid
		if (mcb.GetType()!=0x4d && mcb.GetType()!=0x5a) {
			DEBUG_ShowMsg("MCB chain broken at %04X:0000!",mcb_segment);
			return;
		}

		mcb.GetFileName(filename);

		// some PSP segment values have special meanings
		switch (mcb.GetPSPSeg()) {
			case MCB_FREE:
				psp_seg_note = "(free)";
				break;
			case MCB_DOS:
				psp_seg_note = "(DOS)";
				break;
			default:
				psp_seg_note = "";
		}

        DEBUG_ShowMsg("   %04X  %12u     %04X %-7s  %s",mcb_segment,mcb.GetSize() << 4,mcb.GetPSPSeg(), psp_seg_note, filename);

		// print a message if dataAddr is within this MCB's memory range
		PhysPt mcbStartAddr = PhysMake(mcb_segment+1,0);
		PhysPt mcbEndAddr = PhysMake(mcb_segment+1+mcb.GetSize(),0);
		if (dataAddr >= mcbStartAddr && dataAddr < mcbEndAddr) {
            DEBUG_ShowMsg("   (data addr %04hX:%04X is %u bytes past this MCB)",dataSeg,DOS_dataOfs,dataAddr - mcbStartAddr);
		}

		// if we've just processed the last MCB in the chain, break out of the loop
		if (mcb.GetType()==0x5a) {
			break;
		}
		// else, move to the next MCB in the chain
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
}

#include "regionalloctracking.h"

extern bool dos_kernel_disabled;
extern RegionAllocTracking rombios_alloc;

static void LogBIOSMem(void) {
    char tmp[192];

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("BIOS memory blocks:");
    DEBUG_ShowMsg("Region            Status What");
    for (auto i=rombios_alloc.alist.begin();i!=rombios_alloc.alist.end();i++) {
        sprintf(tmp,"%08lx-%08lx %s",
            (unsigned long)(i->start),
            (unsigned long)(i->end),
            i->free ? "FREE  " : "ALLOC ");
        DEBUG_ShowMsg("%s %s",tmp,i->who.c_str());
    }

    DEBUG_EndPagedContent();
}

Bitu XMS_GetTotalHandles(void);
bool XMS_GetHandleInfo(Bitu &phys_location,Bitu &size,Bitu &lockcount,bool &free,Bitu handle);

bool EMS_GetHandle(Bitu &size,PhysPt &addr,std::string &name,Bitu handle);
const char *EMS_Type_String(void);
Bitu EMS_Max_Handles(void);
bool EMS_Active(void);

static void LogFNKEY(void) {
    DEBUG_BeginPagedContent();

    void DEBUG_INTDC_FnKeyMapInfo(void);
    DEBUG_INTDC_FnKeyMapInfo();

    DEBUG_EndPagedContent();
}

static void LogEMS(void) {
    Bitu h_size;
    PhysPt xh_addr;
    std::string h_name;

    if (dos_kernel_disabled) {
        DEBUG_ShowMsg("Cannot enumerate EMS memory while DOS kernel is inactive.");
        return;
    }

    if (!EMS_Active()) {
        DEBUG_ShowMsg("Cannot enumerate EMS memory while EMS is inactive.");
        return;
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("EMS memory (type %s) handles:",EMS_Type_String());
    DEBUG_ShowMsg("Handle Address  Size (bytes)    Name");
    for (Bitu h=0;h < EMS_Max_Handles();h++) {
        if (EMS_GetHandle(/*&*/h_size,/*&*/xh_addr,/*&*/h_name,h)) {
            DEBUG_ShowMsg("%6lu %08lx %08lx %s",
                (unsigned long)h,
                (unsigned long)xh_addr,
                (unsigned long)h_size,
                h_name.c_str());
        }
    }

    bool EMS_GetMapping(Bitu &handle,uint16_t &log_page,Bitu ems_page);
    Bitu GetEMSPageFrameSegment(void);
    Bitu GetEMSPageFrameSize(void);

    DEBUG_ShowMsg("EMS page frame 0x%08lx-0x%08lx",
        GetEMSPageFrameSegment()*16UL,
        (GetEMSPageFrameSegment()*16UL)+GetEMSPageFrameSize()-1UL);
    DEBUG_ShowMsg("Handle Page(p/l) Address");

    for (Bitu p=0;p < (GetEMSPageFrameSize() >> 14UL);p++) {
        uint16_t log_page;
        Bitu handle;

        if (EMS_GetMapping(handle,log_page,p)) {
            char tmp[192] = {0};

            xh_addr = 0;
            h_size = 0;
            h_name.clear();
            EMS_GetHandle(/*&*/h_size,/*&*/xh_addr,/*&*/h_name,handle);

            if (xh_addr != 0)
                sprintf(tmp," virt -> %08lx-%08lx phys",
                    (unsigned long)(xh_addr + ((PhysPt)log_page << 14u)),
                    (unsigned long)(xh_addr + ((PhysPt)log_page << 14u) + (1u << 14u) - 1u));

            DEBUG_ShowMsg("%6lu %4lu/%4lu %08lx-%08lx%s",(unsigned long)handle,
                (unsigned long)p,(unsigned long)log_page,
                (GetEMSPageFrameSegment()*16UL)+(p << 14UL),
                (GetEMSPageFrameSegment()*16UL)+((p+1UL) << 14UL)-1,
                tmp);
        }
        else {
            DEBUG_ShowMsg("--     %4lu/     %08lx-%08lx",(unsigned long)p,
                (GetEMSPageFrameSegment()*16UL)+(p << 14UL),
                (GetEMSPageFrameSegment()*16UL)+((p+1UL) << 14UL)-1);
        }
    }

    DEBUG_EndPagedContent();
}

static void LogXMS(void) {
    Bitu phys_location;
    Bitu lockcount;
    bool free;
    Bitu size;

    if (dos_kernel_disabled) {
        DEBUG_ShowMsg("Cannot enumerate XMS memory while DOS kernel is inactive.");
        return;
    }

    if (!XMS_Active()) {
        DEBUG_ShowMsg("Cannot enumerate XMS memory while XMS is inactive.");
        return;
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("XMS memory handles:");
    DEBUG_ShowMsg("Handle Status Location Size (bytes)");
    for (Bitu h = 1; h < XMS_GetTotalHandles(); h++) {
        if (XMS_GetHandleInfo(/*&*/phys_location,/*&*/size,/*&*/lockcount,/*&*/free, h)) {
            DEBUG_ShowMsg("%6lu %s 0x%08lx %lu",
                (unsigned long)h,
                free ? "FREE " : "ALLOC ",
                (unsigned long)phys_location,
                (unsigned long)size << 10UL); /* KB -> bytes */
        }
    }

    DEBUG_EndPagedContent();
}

static void LogDOSKernMem(void) {
    char tmp[192];

    if (dos_kernel_disabled) {
        DEBUG_ShowMsg("Cannot enumerate DOS kernel memory while DOS kernel is inactive.");
        return;
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("DOS kernel memory blocks:");
    DEBUG_ShowMsg("Seg      Size (bytes)     What");
    for (auto i=DOS_GetMemLog.begin();i!=DOS_GetMemLog.end();i++) {
        sprintf(tmp,"%04x     %8lu     ",
                (unsigned int)(i->segbase),
                (unsigned long)(i->pages << 4UL));

        DEBUG_ShowMsg("%s    %s",tmp,i->who.c_str());
    }

    DEBUG_EndPagedContent();
}

// Display the content of all Memory Control Blocks.
static void LogMCBS(void) {
    if (dos_kernel_disabled) {
        if (boothax == BOOTHAX_MSDOS) {
            if (guest_msdos_LoL == 0 || guest_msdos_mcb_chain == 0) {
                DEBUG_ShowMsg("Cannot enumerate MCB list while DOS kernel is inactive, and DOSBox-X has not yet determined the MCB list of the guest MS-DOS operating system");
                return;
            }

            DEBUG_BeginPagedContent();

            try {
                DEBUG_ShowMsg("MCB Seg  Size (bytes)  PSP Seg (notes)  Filename");
                DEBUG_ShowMsg("Conventional memory:");
                LogMCBChain(guest_msdos_mcb_chain);
            }
            catch (GuestPageFaultException &pf) {
                (void)pf;//unused
                DEBUG_ShowMsg("(Enumeration caused page fault within the guest)");
            }

            DEBUG_EndPagedContent();
            return;
        }
        else {
            DEBUG_ShowMsg("Cannot enumerate MCB list while DOS kernel is inactive.");
            return;
        }
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("MCB Seg  Size (bytes)  PSP Seg (notes)  Filename");
    DEBUG_ShowMsg("Conventional memory:");
    LogMCBChain(dos.firstMCB);

    if (dos_infoblock.GetStartOfUMBChain() != 0xFFFF) {
        DEBUG_ShowMsg("Upper memory:");
        LogMCBChain(dos_infoblock.GetStartOfUMBChain());
    }

    DEBUG_EndPagedContent();
}

static void LogGDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu length = cpu.gdt.GetLimit();
	PhysPt address = cpu.gdt.GetBase();
	PhysPt max	   = (PhysPt)(address + length);
	Bitu i = 0;

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("GDT Base:%08lX Limit:%08lX",(unsigned long)address,(unsigned long)length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08lX type: %02X parbg",(int)(i<<3),(unsigned long)desc.GetBase(),desc.saved.seg.type);
        DEBUG_ShowMsg("%s",out1);
		sprintf(out1,"      l:%08lX dpl : %01X  %1X%1X%1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
        DEBUG_ShowMsg("%s",out1);
		address+=8; i++;
	}

    DEBUG_EndPagedContent();
}

static void LogLDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu ldtSelector = cpu.gdt.SLDT();
	if (!cpu.gdt.GetDescriptor(ldtSelector,desc)) return;
	Bitu length = desc.GetLimit();
	PhysPt address = desc.GetBase();
	PhysPt max	   = (PhysPt)(address + length);
	Bitu i = 0;

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("LDT Base:%08lX Limit:%08lX",(unsigned long)address,(unsigned long)length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08lX type: %02X parbg",(int)((i<<3)|4),(unsigned long)desc.GetBase(),desc.saved.seg.type);
        DEBUG_ShowMsg("%s",out1);
		sprintf(out1,"      l:%08lX dpl : %01X  %1X%1X%1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
        DEBUG_ShowMsg("%s",out1);
		address+=8; i++;
	}

    DEBUG_EndPagedContent();
}

static void LogIDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu address = 0;

    DEBUG_BeginPagedContent();

	while (address<256*8) {
		if (cpu.idt.GetDescriptor(address,desc)) {
			sprintf(out1,"%04X: sel:%04X off:%02X",(unsigned int)(address/8),(int)desc.GetSelector(),(int)desc.GetOffset());
            DEBUG_ShowMsg("%s",out1);
		}
		address+=8;
	}

    DEBUG_EndPagedContent();
}

void LogPages(char* selname) {
    DEBUG_BeginPagedContent();

	if (paging.enabled) {
		char out1[512];
		Bitu sel = GetHexValue(selname,selname);
		if ((sel==0x00) && ((*selname==0) || (*selname=='*'))) {
			for (unsigned int i=0; i<0xfffff; i++) {
				Bitu table_addr=((Bitu)paging.base.page<<12u)+(i >> 10u)*(Bitu)4u;
				X86PageEntry table;
				table.load=phys_readd((PhysPt)table_addr);
				if (table.block.p) {
					X86PageEntry entry;
                    PhysPt entry_addr=((PhysPt)table.block.base<<12u)+(i & 0x3ffu)* 4u;
					entry.load=phys_readd(entry_addr);
					if (entry.block.p) {
						sprintf(out1,"page %05Xxxx -> %04Xxxx  flags [uw] %x:%x::%x:%x [d=%x|a=%x]",
							i,entry.block.base,entry.block.us,table.block.us,
							entry.block.wr,table.block.wr,entry.block.d,entry.block.a);
                        DEBUG_ShowMsg("%s",out1);
					}
				}
			}
		} else {
			Bitu table_addr=(paging.base.page<<12u)+(sel >> 10u)*4u;
			X86PageEntry table;
			table.load=phys_readd((PhysPt)table_addr);
			if (table.block.p) {
				X86PageEntry entry;
				Bitu entry_addr=((Bitu)table.block.base<<12u)+(sel & 0x3ffu)*4u;
				entry.load=phys_readd((PhysPt)entry_addr);
				sprintf(out1,"page %05lXxxx -> %04lXxxx  flags [puw] %x:%x::%x:%x::%x:%x",
					(unsigned long)sel,
					(unsigned long)entry.block.base,
					entry.block.p,table.block.p,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
                DEBUG_ShowMsg("%s",out1);
			} else {
				sprintf(out1,"pagetable %03X not present, flags [puw] %x::%x::%x",
					(int)(sel >> 10),
					(int)table.block.p,
					(int)table.block.us,
					(int)table.block.wr);
                DEBUG_ShowMsg("%s",out1);
			}
		}
	}

    DEBUG_EndPagedContent();
}

const char *FPU_tag(unsigned int i) {
    switch (i) {
        case TAG_Valid: return "Valid";
        case TAG_Zero:  return "Zero";
        case TAG_Weird: return "Weird";
        case TAG_Empty: return "Empty";
    }

    return "?";
}

static void LogFPUInfo(void) {
    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("FPU TOP=%u",(unsigned int)fpu.top);

    for (unsigned int i=0;i < 8;i++) {
        unsigned int adj = STV(i);

#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
        DEBUG_ShowMsg(" st(%u): %s val=%.9f",i,FPU_tag(fpu.tags[adj]),(double)fpu.regs_80[adj].v);
#else
        DEBUG_ShowMsg(" st(%u): %s use80=%u val=%.9f",i,FPU_tag(fpu.tags[adj]),fpu.use80[adj],fpu.regs[adj].d);
#endif
    }

    DEBUG_EndPagedContent();
}

static void LogCPUInfo(void) {
	char out1[512];

    DEBUG_BeginPagedContent();

	sprintf(out1,"cr0:%08lX cr2:%08lX cr3:%08lX  cpl=%lx",
		(unsigned long)cpu.cr0,
		(unsigned long)paging.cr2,
		(unsigned long)paging.cr3,
		(unsigned long)cpu.cpl);
    DEBUG_ShowMsg("%s",out1);
	sprintf(out1,"eflags:%08lX [vm=%x iopl=%x nt=%x]",
		(unsigned long)reg_flags,
		(int)(GETFLAG(VM)>>17),
		(int)(GETFLAG(IOPL)>>12),
		(int)(GETFLAG(NT)>>14));
    DEBUG_ShowMsg("%s",out1);
	sprintf(out1,"GDT base=%08lX limit=%08lX",
		(unsigned long)cpu.gdt.GetBase(),
		(unsigned long)cpu.gdt.GetLimit());
    DEBUG_ShowMsg("%s",out1);
	sprintf(out1,"IDT base=%08lX limit=%08lX",
		(unsigned long)cpu.idt.GetBase(),
		(unsigned long)cpu.idt.GetLimit());
    DEBUG_ShowMsg("%s",out1);

	Bitu sel=CPU_STR();
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"TR selector=%04X, base=%08lX limit=%08lX*%X",(int)sel,(unsigned long)desc.GetBase(),(unsigned long)desc.GetLimit(),desc.saved.seg.g?0x4000:1);
        DEBUG_ShowMsg("%s",out1);
	}
	sel=CPU_SLDT();
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"LDT selector=%04X, base=%08lX limit=%08lX*%X",(int)sel,(unsigned long)desc.GetBase(),(unsigned long)desc.GetLimit(),desc.saved.seg.g?0x4000:1);
        DEBUG_ShowMsg("%s",out1);
	}

    DEBUG_EndPagedContent();
}

#if C_HEAVY_DEBUG
static void LogInstruction(uint16_t segValue, uint32_t eipValue,  ofstream& out) {
	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	if (cpuLogType == 3) { //Log only cs:ip.
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << endl;
		return;
	}

	PhysPt start = (PhysPt)GetAddress(segValue,eipValue);
	char dline[200];Bitu size;
	size = DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (showExtend && (cpuLogType > 0) ) {
		res = AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
        if (reslen < 22) {
            memset(res + reslen, ' ', 22 - reslen);
            res[22] = 0;
        }
	}
	Bitu len = strlen(dline);
    if (len < 30) {
        memset(dline + len, ' ', 30 - len);
        dline[30] = 0;
    }

	// Get register values

	if(cpuLogType == 0) {
		out << setw(4) << SegValue(cs) << ":" << setw(4) << reg_eip << "  " << dline;
	} else if (cpuLogType == 1) {
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res;
	} else if (cpuLogType == 2) {
		char ibytes[200]="";	char tmpc[200];
		for (Bitu i=0; i<size; i++) {
			uint8_t value;
			if (mem_readb_checked((PhysPt)(start+i),&value)) sprintf(tmpc,"%s","?? ");
			else sprintf(tmpc,"%02X ",value);
			strcat(ibytes,tmpc);
		}
		len = strlen(ibytes);
        if (len < 21) {
            for (Bitu i = 0; i < 21 - len; i++) ibytes[len + i] = ' ';
            ibytes[21] = 0;
        }
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res << "  " << ibytes;
	}

	out << " EAX:" << setw(8) << reg_eax << " EBX:" << setw(8) << reg_ebx
	    << " ECX:" << setw(8) << reg_ecx << " EDX:" << setw(8) << reg_edx
	    << " ESI:" << setw(8) << reg_esi << " EDI:" << setw(8) << reg_edi
	    << " EBP:" << setw(8) << reg_ebp << " ESP:" << setw(8) << reg_esp
	    << " DS:"  << setw(4) << SegValue(ds)<< " ES:"  << setw(4) << SegValue(es);

	if(cpuLogType == 0) {
		out << " SS:"  << setw(4) << SegValue(ss) << " C"  << (get_CF()>0)  << " Z"   << (get_ZF()>0)
		    << " S" << (get_SF()>0) << " O"  << (get_OF()>0) << " I"  << GETFLAGBOOL(IF);
	} else {
		out << " FS:"  << setw(4) << SegValue(fs) << " GS:"  << setw(4) << SegValue(gs)
		    << " SS:"  << setw(4) << SegValue(ss)
		    << " CF:"  << (get_CF()>0)  << " ZF:"   << (get_ZF()>0)  << " SF:"  << (get_SF()>0)
		    << " OF:"  << (get_OF()>0)  << " AF:"   << (get_AF()>0)  << " PF:"  << (get_PF()>0)
		    << " IF:"  << GETFLAGBOOL(IF);
	}
	if(cpuLogType == 2) {
		out << " TF:" << GETFLAGBOOL(TF) << " VM:" << GETFLAGBOOL(VM) <<" FLG:" << setw(8) << reg_flags
		    << " CR0:" << setw(8) << cpu.cr0;
	}
	out << endl;
}
#endif

#if 0
// DEBUG.COM stuff

class DEBUG : public Program {
public:
	DEBUG()		{ pDebugcom	= this;	active = false; };
	~DEBUG()	{ pDebugcom	= 0; };

	bool IsActive() { return active; };

	void Run(void)
	{
		if(cmd->FindExist("/NOMOUSE",false)) {
	        	real_writed(0,0x33<<2,0);
			return;
		}

		uint16_t commandNr = 1;
		if (!cmd->FindCommand(commandNr++,temp_line)) return;
		// Get filename
		char filename[128];
		safe_strncpy(filename,temp_line.c_str(),128);
		// Setup commandline
		char args[256+1];
		args[0]	= 0;
		bool found = cmd->FindCommand(commandNr++,temp_line);
		while (found) {
			if (strlen(args)+temp_line.length()+1>256) break;
			strcat(args,temp_line.c_str());
			found = cmd->FindCommand(commandNr++,temp_line);
			if (found) strcat(args," ");
		}
		// Start new shell and execute prog
		active = true;
		// Save cpu state....
		uint16_t oldcs	= SegValue(cs);
		uint32_t oldeip	= reg_eip;
		uint16_t oldss	= SegValue(ss);
		uint32_t oldesp	= reg_esp;

		// Start shell
		DOS_Shell shell;
		shell.Execute(filename,args);

		// set old reg values
		SegSet16(ss,oldss);
		reg_esp = oldesp;
		SegSet16(cs,oldcs);
		reg_eip = oldeip;
	};

private:
	bool	active;
};
#endif

#if C_DEBUG
extern bool debugger_break_on_exec;
#endif

void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off)
{
#if C_DEBUG
    if (debugger_break_on_exec) {
		CBreakpoint::AddBreakpoint(seg,off,true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
        debugger_break_on_exec = false;
    }
#endif
#if 0
	if (pDebugcom && pDebugcom->IsActive()) {
		CBreakpoint::AddBreakpoint(seg,off,true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		pDebugcom = 0;
	};
#endif
}

Bitu DEBUG_EnableDebugger(void)
{
	exitLoop = true;

	if (!debugging)
		DEBUG_Enable_Handler(true);

	CPU_Cycles=CPU_CycleLeft=0;
	return 0;
}

// INIT

void DBGBlock::set_data_view(unsigned int view) {
    void DrawBars(void);

    if (data_view != view) {
        data_view  = view;

        if (win_data) wclear(win_data);

        switch (view) {
            case DATV_SEGMENTED:
                win_title[DBGBlock::WINI_DATA] = "Data view (segmented)";
                break;
            case DATV_VIRTUAL:
                win_title[DBGBlock::WINI_DATA] = "Data view (virtual)";
                break;
            case DATV_PHYSICAL:
                win_title[DBGBlock::WINI_DATA] = "Data view (physical)";
                break;
        }

        DrawBars();
    }
}

void DEBUG_SetupConsole(void) {
	if (dbg.win_main == NULL) {
        LOG(LOG_MISC, LOG_DEBUG)("DEBUG_SetupConsole initializing GUI");

        dbg.set_data_view(DBGBlock::DATV_SEGMENTED);

#ifdef WIN32
		WIN32_Console();
#else
		tcgetattr(0,&consolesettings);
#endif	
		//	dbg.active_win=3;
		/* Start the Debug Gui */
		DBGUI_StartUp();
	}
}

void DEBUG_ShutDown(Section * /*sec*/) {
	CBreakpoint::DeleteAll();
	CDebugVar::DeleteAll();
	if (dbg.win_main != NULL) {
        LOG(LOG_MISC, LOG_DEBUG)("DEBUG_Shutdown freeing ncurses state");
		curs_set(old_cursor_state);

        void DEBUG_GUI_DestroySubWindows(void);
        DEBUG_GUI_DestroySubWindows();

//      if (dbg.win_main) delwin(dbg.win_main);
		dbg.win_main = NULL;

        endwin();

#ifndef WIN32
		tcsetattr(0,TCSANOW,&consolesettings);
#endif
	}
}

Bitu debugCallback;

void DEBUG_ReinitCallback(void) {
    /* this is REQUIRED after loading a custom BIOS */
	debugCallback=CALLBACK_Allocate();
	CALLBACK_Setup(debugCallback,DEBUG_EnableDebugger,CB_RETF,"debugger");
}

void DEBUG_Init() {
	DOSBoxMenu::item *item;

    LOG(LOG_MISC, LOG_DEBUG)("Initializing debug system");

	/* Add some keyhandlers */
	#if defined(MACOSX)
		// OSX NOTE: ALT-F12 to launch debugger. pause maps to F16 on macOS,
		// which is not easy to input on a modern mac laptop
		MAPPER_AddHandler(DEBUG_Enable_Handler,MK_f12,MMOD2,"debugger","Show debugger", &item);
	#else
		MAPPER_AddHandler(DEBUG_Enable_Handler,MK_pause,MMOD2,"debugger","Show debugger",&item);
	#endif
	item->set_text("Show debugger");
	/* Reset code overview and input line */
	memset((void*)&codeViewData,0,sizeof(codeViewData));
	/* Setup callback */
	debugCallback=CALLBACK_Allocate();
	CALLBACK_Setup(debugCallback,DEBUG_EnableDebugger,CB_RETF,"debugger");

#if defined(MACOSX) || defined(LINUX)
	/* Mac OS X does not have a console for us to just allocate on a whim like Windows does.
	   So the debugger interface is useless UNLESS the user has started us from a terminal
	   (whether over SSH or from the Terminal app).
       
       Linux/X11 also does not have a console we can allocate on a whim. You either run
       this program from XTerm for the debugger, or not. */
    bool allow = true;

    if (!isatty(0) || !isatty(1) || !isatty(2))
	    allow = false;

    mainMenu.get_item("mapper_debugger").enable(allow).refresh_item(mainMenu);
#endif

	/* shutdown function */
	AddExitFunction(AddExitFunctionFuncPair(DEBUG_ShutDown));
}

// DEBUGGING VAR STUFF

void CDebugVar::InsertVariable(char* name, PhysPt adr)
{
	varList.push_back(new CDebugVar(name,adr));
}

void CDebugVar::DeleteAll(void)
{
	std::vector<CDebugVar*>::iterator i;
	for(i=varList.begin(); i != varList.end(); ++i) {
		CDebugVar* bp = static_cast<CDebugVar*>(*i);
		delete bp;
	}
	(varList.clear)();
}

CDebugVar* CDebugVar::FindVar(PhysPt pt)
{
	if (varList.empty()) return 0;

	std::vector<CDebugVar*>::size_type s = varList.size();
	for(std::vector<CDebugVar*>::size_type i = 0; i != s; i++) {
		CDebugVar* bp = static_cast<CDebugVar*>(varList[i]);
		if (bp->GetAdr() == pt) return bp;
	}
	return 0;
}

bool CDebugVar::SaveVars(char* name) {
	if (varList.size() > 65535) return false;

	FILE* f = fopen(name,"wb+");
	if (!f) return false;

	// write number of vars
	uint16_t num = (uint16_t)varList.size();
	fwrite(&num,1,sizeof(num),f);

	std::vector<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); ++i) {
		bp = static_cast<CDebugVar*>(*i);
		// name
		fwrite(bp->GetName(),1,16,f);
		// adr
		PhysPt adr = bp->GetAdr();
		fwrite(&adr,1,sizeof(adr),f);
	}
	fclose(f);
	return true;
}

bool CDebugVar::LoadVars(char* name)
{
	FILE* f = fopen(name,"rb");
	if (!f) return false;

	// read number of vars
	uint16_t num;
	if (fread(&num,sizeof(num),1,f) != 1) {
		fclose(f);
		return false;
	}

	for (uint16_t i=0; i<num; i++) {
		char name2[16];
		// name
		if (fread(name2,16,1,f) != 1) break;
		// adr
		PhysPt adr;
		if (fread(&adr,sizeof(adr),1,f) != 1) break;
		// insert
		InsertVariable(name2,adr);
	}
	fclose(f);
	return true;
}

static void SaveMemory(uint16_t seg, uint32_t ofs1, uint32_t num) {
	FILE* f = fopen("MEMDUMP.TXT","wt");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Memory dump failed.\n");
		return;
	}

	char buffer[128];
	char temp[16];

	while (num>16) {
		sprintf(buffer,"%04X:%04X   ",seg,ofs1);
		for (uint16_t x=0; x<16; x++) {
			uint8_t value;
			if (mem_readb_checked((PhysPt)GetAddress(seg,ofs1+x),&value)) sprintf(temp,"%s","?? ");
			else sprintf(temp,"%02X ",value);
			strcat(buffer,temp);
		}
		ofs1+=16;
		num-=16;

		fprintf(f,"%s\n",buffer);
	}
	if (num>0) {
		sprintf(buffer,"%04X:%04X   ",seg,ofs1);
		for (uint16_t x=0; x<num; x++) {
			uint8_t value;
			if (mem_readb_checked((PhysPt)GetAddress(seg,ofs1+x),&value)) sprintf(temp,"%s","?? ");
			else sprintf(temp,"%02X ",value);
			strcat(buffer,temp);
		}
		fprintf(f,"%s\n",buffer);
	}
	fclose(f);
	DEBUG_ShowMsg("DEBUG: Memory dump success.\n");
}

static void SaveMemoryBin(uint16_t seg, uint32_t ofs1, uint32_t num) {
	FILE* f = fopen("MEMDUMP.BIN","wb");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Memory binary dump failed.\n");
		return;
	}

	for (uint32_t x = 0; x < num;x++) {
		uint8_t val;
		if (mem_readb_checked((PhysPt)GetAddress(seg,ofs1+x),&val)) val=0;
		fwrite(&val,1,1,f);
	}

	fclose(f);
	DEBUG_ShowMsg("DEBUG: Memory dump binary success.\n");
}

static void OutputVecTable(char* filename) {
	FILE* f = fopen(filename, "wt");
	if (!f)
	{
		DEBUG_ShowMsg("DEBUG: Output of interrupt vector table failed.\n");
		return;
	}

	for (unsigned int i=0; i<256; i++)
		fprintf(f,"INT %02X:  %04X:%04X\n", i, mem_readw(i * 4u + 2u), mem_readw(i * 4u));

	fclose(f);
	DEBUG_ShowMsg("DEBUG: Interrupt vector table written to %s.\n", filename);
}

#define DEBUG_VAR_BUF_LEN 16
static void DrawVariables(void) {
	if (CDebugVar::varList.empty()) return;

	char buffer[DEBUG_VAR_BUF_LEN];
	std::vector<CDebugVar*>::size_type s = CDebugVar::varList.size();
	bool windowchanges = false;

	for(std::vector<CDebugVar*>::size_type i = 0; i != s; i++) {

		if (i == 4*3) {
			/* too many variables */
			break;
		}

		CDebugVar *dv = static_cast<CDebugVar*>(CDebugVar::varList[i]);
		uint16_t value;
		bool varchanges = false;
		bool has_no_value = mem_readw_checked(dv->GetAdr(),&value);
		if (has_no_value) {
			snprintf(buffer,DEBUG_VAR_BUF_LEN, "%s", "??????");
			dv->SetValue(false,0);
			varchanges = true;
		} else {
			if ( dv->HasValue() && dv->GetValue() == value) {
				//It already had a value and it didn't change (most likely case)
			} else {
				dv->SetValue(true,value);
				snprintf(buffer,DEBUG_VAR_BUF_LEN, "0x%04x", value);
				varchanges = true;
			}
		}

		if (varchanges) {
			unsigned int y = (unsigned int)(i / 3u);
			unsigned int x = (i % 3u) * 26u;
			mvwprintw(dbg.win_var, (int)y,  (int)x, dv->GetName());
			mvwprintw(dbg.win_var, (int)y, ((int)x + DEBUG_VAR_BUF_LEN + 1) , buffer);
			windowchanges = true; //Something has changed in this window
		}
	}

	if (windowchanges) wrefresh(dbg.win_var);
}
#undef DEBUG_VAR_BUF_LEN
// HEAVY DEBUGGING STUFF

#if C_HEAVY_DEBUG

const uint32_t LOGCPUMAX = 20000;

static uint32_t logCount = 0;

struct TLogInst {
	uint16_t s_cs;
	uint32_t eip;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t esp;
	uint16_t s_ds;
	uint16_t s_es;
	uint16_t s_fs;
	uint16_t s_gs;
	uint16_t s_ss;
	bool c;
	bool z;
	bool s;
	bool o;
	bool a;
	bool p;
	bool i;
	char dline[31];
	char res[23];
};

TLogInst logInst[LOGCPUMAX];

void DEBUG_HeavyLogInstruction(void) {

	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	PhysPt start = (PhysPt)GetAddress(SegValue(cs),reg_eip);
	char dline[200];
	DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (showExtend) {
		res = AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
        if (reslen < 22) {
            memset(res + reslen, ' ', 22 - reslen);
            res[22] = 0;
        }
	}

	Bitu len = strlen(dline);
	if (len < 30) memset(dline + len,' ',30 - len);
	dline[30] = 0;

	TLogInst & inst = logInst[logCount];
	strcpy(inst.dline,dline);
	inst.s_cs = SegValue(cs);
	inst.eip  = reg_eip;
	strcpy(inst.res,res);
	inst.eax  = reg_eax;
	inst.ebx  = reg_ebx;
	inst.ecx  = reg_ecx;
	inst.edx  = reg_edx;
	inst.esi  = reg_esi;
	inst.edi  = reg_edi;
	inst.ebp  = reg_ebp;
	inst.esp  = reg_esp;
	inst.s_ds = SegValue(ds);
	inst.s_es = SegValue(es);
	inst.s_fs = SegValue(fs);
	inst.s_gs = SegValue(gs);
	inst.s_ss = SegValue(ss);
	inst.c    = get_CF()>0;
	inst.z    = get_ZF()>0;
	inst.s    = get_SF()>0;
	inst.o    = get_OF()>0;
	inst.a    = get_AF()>0;
	inst.p    = get_PF()>0;
	inst.i    = GETFLAGBOOL(IF);

	if (++logCount >= LOGCPUMAX) logCount = 0;
}

void DEBUG_HeavyWriteLogInstruction(void) {
	if (!logHeavy) return;
	logHeavy = false;

	DEBUG_ShowMsg("DEBUG: Creating cpu log LOGCPU_INT_CD.TXT\n");

	ofstream out("LOGCPU_INT_CD.TXT");
	if (!out.is_open()) {
		DEBUG_ShowMsg("DEBUG: Failed.\n");
		return;
	}
	out << hex << noshowbase << setfill('0') << uppercase;
	uint32_t startLog = logCount;
	do {
		// Write Instructions
		TLogInst & inst = logInst[startLog];
		out << setw(4) << inst.s_cs << ":" << setw(8) << inst.eip << "  "
		    << inst.dline << "  " << inst.res << " EAX:" << setw(8)<< inst.eax
		    << " EBX:" << setw(8) << inst.ebx << " ECX:" << setw(8) << inst.ecx
		    << " EDX:" << setw(8) << inst.edx << " ESI:" << setw(8) << inst.esi
		    << " EDI:" << setw(8) << inst.edi << " EBP:" << setw(8) << inst.ebp
		    << " ESP:" << setw(8) << inst.esp << " DS:"  << setw(4) << inst.s_ds
		    << " ES:"  << setw(4) << inst.s_es<< " FS:"  << setw(4) << inst.s_fs
		    << " GS:"  << setw(4) << inst.s_gs<< " SS:"  << setw(4) << inst.s_ss
		    << " CF:"  << inst.c  << " ZF:"   << inst.z  << " SF:"  << inst.s
		    << " OF:"  << inst.o  << " AF:"   << inst.a  << " PF:"  << inst.p
		    << " IF:"  << inst.i  << endl;

/*		fprintf(f,"%04X:%08X   %s  %s  EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X DS:%04X ES:%04X FS:%04X GS:%04X SS:%04X CF:%01X ZF:%01X SF:%01X OF:%01X AF:%01X PF:%01X IF:%01X\n",
			logInst[startLog].s_cs,logInst[startLog].eip,logInst[startLog].dline,logInst[startLog].res,logInst[startLog].eax,logInst[startLog].ebx,logInst[startLog].ecx,logInst[startLog].edx,logInst[startLog].esi,logInst[startLog].edi,logInst[startLog].ebp,logInst[startLog].esp,
		        logInst[startLog].s_ds,logInst[startLog].s_es,logInst[startLog].s_fs,logInst[startLog].s_gs,logInst[startLog].s_ss,
		        logInst[startLog].c,logInst[startLog].z,logInst[startLog].s,logInst[startLog].o,logInst[startLog].a,logInst[startLog].p,logInst[startLog].i);*/
		if (++startLog >= LOGCPUMAX) startLog = 0;
	} while (startLog != logCount);

	out.close();
	DEBUG_ShowMsg("DEBUG: Done.\n");
}

bool DEBUG_HeavyIsBreakpoint(void) {
	if (cpuLog) {
		if (cpuLogCounter>0) {
			LogInstruction(SegValue(cs),reg_eip,cpuLogFile);
			cpuLogCounter--;
		}
		if (cpuLogCounter<=0) {
			cpuLogFile.flush();
			cpuLogFile.close();
			DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT created\n");
			cpuLog = false;
			DEBUG_EnableDebugger();
			return true;
		}
	}
	// LogInstruction
	if (logHeavy) DEBUG_HeavyLogInstruction();
	if (zeroProtect) {
		static Bitu zero_count = 0;
		uint32_t value = 0;
		if (!mem_readd_checked(SegPhys(cs)+reg_eip,&value)) {
			if (value == 0) zero_count++;
			else zero_count = 0;
		}
		if (GCC_UNLIKELY(zero_count == 10)) E_Exit("running zeroed code");
	}

	if (skipFirstInstruction) {
		skipFirstInstruction = false;
		return false;
	}
	if (CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) {
		return true;
	}
	return false;
}

/* this is for the BIOS, to stop the log upon BIOS POST. */
void DEBUG_StopLog(void) {
	if (cpuLog) {
        cpuLogCounter = 0;
        cpuLogFile.close();
        DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT stopped\n");
        cpuLog = false;
    }
}

#endif // HEAVY DEBUG


#endif // DEBUG


