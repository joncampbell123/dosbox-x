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


#ifndef DOSBOX_CPU_H
#define DOSBOX_CPU_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h" 
#endif
#ifndef DOSBOX_REGS_H
#include "regs.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#define CPU_AUTODETERMINE_NONE		0x00
#define CPU_AUTODETERMINE_CORE		0x01
#define CPU_AUTODETERMINE_CYCLES	0x02

#define CPU_AUTODETERMINE_SHIFT		0x02
#define CPU_AUTODETERMINE_MASK		0x03

#define CPU_CYCLES_LOWER_LIMIT		200


#define CPU_ARCHTYPE_MIXED			0xff
#define CPU_ARCHTYPE_8086			0x05
#define CPU_ARCHTYPE_80186			0x15
#define CPU_ARCHTYPE_286			0x25
#define CPU_ARCHTYPE_386			0x35
#define CPU_ARCHTYPE_486OLD			0x40
#define CPU_ARCHTYPE_486NEW			0x45
#define CPU_ARCHTYPE_PENTIUM		0x50
#define CPU_ARCHTYPE_PMMXSLOW		0x55
#define CPU_ARCHTYPE_PPROSLOW		0x60

/* CPU Cycle Timing */
extern cpu_cycles_count_t CPU_Cycles;
extern cpu_cycles_count_t CPU_CycleLeft;
extern cpu_cycles_count_t CPU_CycleMax;
extern cpu_cycles_count_t CPU_OldCycleMax;
extern cpu_cycles_count_t CPU_CyclePercUsed;
extern cpu_cycles_count_t CPU_CycleLimit;
extern cpu_cycles_count_t CPU_IODelayRemoved;
extern cpu_cycles_count_t CPU_CyclesSet;
extern unsigned char CPU_AutoDetermineMode;
extern char core_mode[16];

extern bool CPU_CycleAutoAdjust;
extern bool CPU_SkipCycleAutoAdjust;

extern bool enable_weitek;

extern unsigned char CPU_ArchitectureType;

extern unsigned int CPU_PrefetchQueueSize;

/* Some common Defines */
/* A CPU Handler */
typedef Bits (CPU_Decoder)(void);
extern CPU_Decoder * cpudecoder;

Bits CPU_Core_Normal_Run(void);
Bits CPU_Core_Normal_Trap_Run(void);
Bits CPU_Core_Simple_Run(void);
Bits CPU_Core_Simple_Trap_Run(void);
Bits CPU_Core_Full_Run(void);
Bits CPU_Core_Dyn_X86_Run(void);
Bits CPU_Core_Dyn_X86_Trap_Run(void);
Bits CPU_Core_Dynrec_Run(void);
Bits CPU_Core_Dynrec_Trap_Run(void);
Bits CPU_Core_Prefetch_Run(void);
Bits CPU_Core_Prefetch_Trap_Run(void);

Bits CPU_Core286_Normal_Run(void);
Bits CPU_Core286_Normal_Trap_Run(void);

Bits CPU_Core8086_Normal_Run(void);
Bits CPU_Core8086_Normal_Trap_Run(void);

Bits CPU_Core286_Prefetch_Run(void);

Bits CPU_Core8086_Prefetch_Run(void);

void CPU_Enable_SkipAutoAdjust(void);
void CPU_Disable_SkipAutoAdjust(void);
void CPU_Reset_AutoAdjust(void);


//CPU Stuff

extern Bit16u parity_lookup[256];

void CPU_SetCPL(Bitu newcpl);
bool CPU_LLDT(Bitu selector);
bool CPU_LTR(Bitu selector);
void CPU_LIDT(Bitu limit,Bitu base);
void CPU_LGDT(Bitu limit,Bitu base);

Bitu CPU_STR(void);
Bitu CPU_SLDT(void);
Bitu CPU_SIDT_base(void);
Bitu CPU_SIDT_limit(void);
Bitu CPU_SGDT_base(void);
Bitu CPU_SGDT_limit(void);

void CPU_ARPL(Bitu & dest_sel,Bitu src_sel);
void CPU_LAR(Bitu selector,Bitu & ar);
void CPU_LSL(Bitu selector,Bitu & limit);

void CPU_SET_CRX(Bitu cr,Bitu value);
bool CPU_WRITE_CRX(Bitu cr,Bitu value);
Bitu CPU_GET_CRX(Bitu cr);
bool CPU_READ_CRX(Bitu cr,Bit32u & retvalue);

bool CPU_WRITE_DRX(Bitu dr,Bitu value);
bool CPU_READ_DRX(Bitu dr,Bit32u & retvalue);

bool CPU_WRITE_TRX(Bitu tr,Bitu value);
bool CPU_READ_TRX(Bitu tr,Bit32u & retvalue);

Bitu CPU_SMSW(void);
bool CPU_LMSW(Bitu word);

void CPU_VERR(Bitu selector);
void CPU_VERW(Bitu selector);

void CPU_JMP(bool use32,Bitu selector,Bitu offset,Bit32u oldeip);
void CPU_CALL(bool use32,Bitu selector,Bitu offset,Bit32u oldeip);
void CPU_RET(bool use32,Bitu bytes,Bit32u oldeip);
void CPU_IRET(bool use32,Bit32u oldeip);
void CPU_HLT(Bit32u oldeip);

bool CPU_POPF(Bitu use32);
bool CPU_PUSHF(Bitu use32);
bool CPU_CLI(void);
bool CPU_STI(void);

bool CPU_IO_Exception(Bitu port,Bitu size);

void CPU_ENTER(bool use32,Bitu bytes,Bitu level);
void init_vm86_fake_io();

#define CPU_INT_SOFTWARE		0x1
#define CPU_INT_EXCEPTION		0x2
#define CPU_INT_HAS_ERROR		0x4
#define CPU_INT_NOIOPLCHECK		0x8

extern bool CPU_NMI_gate;
extern bool CPU_NMI_active;
extern bool CPU_NMI_pending;

extern bool do_seg_limits;

void CPU_Interrupt(Bitu num,Bitu type,Bit32u oldeip);
void CPU_Check_NMI();
void CPU_Raise_NMI();
void CPU_NMI_Interrupt();
static INLINE void CPU_HW_Interrupt(Bitu num) {
	CPU_Interrupt(num,0,reg_eip);
}
static INLINE void CPU_SW_Interrupt(Bitu num,Bit32u oldeip) {
	CPU_Interrupt(num,CPU_INT_SOFTWARE,oldeip);
}
static INLINE void CPU_SW_Interrupt_NoIOPLCheck(Bitu num,Bit32u oldeip) {
	CPU_Interrupt(num,CPU_INT_SOFTWARE|CPU_INT_NOIOPLCHECK,oldeip);
}

bool CPU_PrepareException(Bitu which,Bitu error);
void CPU_Exception(Bitu which,Bitu error=0);

bool CPU_SetSegGeneral(SegNames seg,Bit16u value);
bool CPU_PopSeg(SegNames seg,bool use32);

bool CPU_CPUID(void);
Bit16u CPU_Pop16(void);
Bit32u CPU_Pop32(void);
void CPU_Push16(Bit16u value);
void CPU_Push32(Bit32u value);

void CPU_SetFlags(Bitu word,Bitu mask);


#define EXCEPTION_UD			6u
#define EXCEPTION_DF            8u
#define EXCEPTION_TS			10u
#define EXCEPTION_NP			11u
#define EXCEPTION_SS			12u
#define EXCEPTION_GP			13u
#define EXCEPTION_PF			14u

#define CR0_PROTECTION			0x00000001u
#define CR0_MONITORPROCESSOR	0x00000002u
#define CR0_FPUEMULATION		0x00000004u
#define CR0_TASKSWITCH			0x00000008u
#define CR0_FPUPRESENT			0x00000010u
#define CR0_WRITEPROTECT		0x00010000u
#define CR0_PAGING				0x80000000u


// *********************************************************************
// Descriptor
// *********************************************************************

#define DESC_INVALID				0x00u
#define DESC_286_TSS_A				0x01u
#define DESC_LDT					0x02u
#define DESC_286_TSS_B				0x03u
#define DESC_286_CALL_GATE			0x04u
#define DESC_TASK_GATE				0x05u
#define DESC_286_INT_GATE			0x06u
#define DESC_286_TRAP_GATE			0x07u

#define DESC_386_TSS_A				0x09u
#define DESC_386_TSS_B				0x0bu
#define DESC_386_CALL_GATE			0x0cu
#define DESC_386_INT_GATE			0x0eu
#define DESC_386_TRAP_GATE			0x0fu

/* EU/ED Expand UP/DOWN RO/RW Read Only/Read Write NA/A Accessed */
#define DESC_DATA_EU_RO_NA			0x10u
#define DESC_DATA_EU_RO_A			0x11u
#define DESC_DATA_EU_RW_NA			0x12u
#define DESC_DATA_EU_RW_A			0x13u
#define DESC_DATA_ED_RO_NA			0x14u
#define DESC_DATA_ED_RO_A			0x15u
#define DESC_DATA_ED_RW_NA			0x16u
#define DESC_DATA_ED_RW_A			0x17u

/* N/R Readable  NC/C Confirming A/NA Accessed */
#define DESC_CODE_N_NC_A			0x18u
#define DESC_CODE_N_NC_NA			0x19u
#define DESC_CODE_R_NC_A			0x1au
#define DESC_CODE_R_NC_NA			0x1bu
#define DESC_CODE_N_C_A				0x1cu
#define DESC_CODE_N_C_NA			0x1du
#define DESC_CODE_R_C_A				0x1eu
#define DESC_CODE_R_C_NA			0x1fu

#ifdef _MSC_VER
#pragma pack (1)
#endif

struct S_Descriptor {
#ifdef WORDS_BIGENDIAN
	Bit32u base_0_15	:16;
	Bit32u limit_0_15	:16;
	Bit32u base_24_31	:8;
	Bit32u g			:1;
	Bit32u big			:1;
	Bit32u r			:1;
	Bit32u avl			:1;
	Bit32u limit_16_19	:4;
	Bit32u p			:1;
	Bit32u dpl			:2;
	Bit32u type			:5;
	Bit32u base_16_23	:8;
#else
	Bit32u limit_0_15	:16;
	Bit32u base_0_15	:16;
	Bit32u base_16_23	:8;
	Bit32u type			:5;
	Bit32u dpl			:2;
	Bit32u p			:1;
	Bit32u limit_16_19	:4;
	Bit32u avl			:1;
	Bit32u r			:1;
	Bit32u big			:1;
	Bit32u g			:1;
	Bit32u base_24_31	:8;
#endif
}GCC_ATTRIBUTE(packed);

struct G_Descriptor {
#ifdef WORDS_BIGENDIAN
	Bit32u selector:	16;
	Bit32u offset_0_15	:16;
	Bit32u offset_16_31	:16;
	Bit32u p			:1;
	Bit32u dpl			:2;
	Bit32u type			:5;
	Bit32u reserved		:3;
	Bit32u paramcount	:5;
#else
	Bit32u offset_0_15	:16;
	Bit32u selector		:16;
	Bit32u paramcount	:5;
	Bit32u reserved		:3;
	Bit32u type			:5;
	Bit32u dpl			:2;
	Bit32u p			:1;
	Bit32u offset_16_31	:16;
#endif
} GCC_ATTRIBUTE(packed);

struct TSS_16 {	
    Bit16u back;                 /* Back link to other task */
    Bit16u sp0;				     /* The CK stack pointer */
    Bit16u ss0;					 /* The CK stack selector */
	Bit16u sp1;                  /* The parent KL stack pointer */
    Bit16u ss1;                  /* The parent KL stack selector */
	Bit16u sp2;                  /* Unused */
    Bit16u ss2;                  /* Unused */
    Bit16u ip;                   /* The instruction pointer */
    Bit16u flags;                /* The flags */
    Bit16u ax, cx, dx, bx;       /* The general purpose registers */
    Bit16u sp, bp, si, di;       /* The special purpose registers */
    Bit16u es;                   /* The extra selector */
    Bit16u cs;                   /* The code selector */
    Bit16u ss;                   /* The application stack selector */
    Bit16u ds;                   /* The data selector */
    Bit16u ldt;                  /* The local descriptor table */
} GCC_ATTRIBUTE(packed);

struct TSS_32 {	
    Bit32u back;                /* Back link to other task */
	Bit32u esp0;		         /* The CK stack pointer */
    Bit32u ss0;					 /* The CK stack selector */
	Bit32u esp1;                 /* The parent KL stack pointer */
    Bit32u ss1;                  /* The parent KL stack selector */
	Bit32u esp2;                 /* Unused */
    Bit32u ss2;                  /* Unused */
	Bit32u cr3;                  /* The page directory pointer */
    Bit32u eip;                  /* The instruction pointer */
    Bit32u eflags;               /* The flags */
    Bit32u eax, ecx, edx, ebx;   /* The general purpose registers */
    Bit32u esp, ebp, esi, edi;   /* The special purpose registers */
    Bit32u es;                   /* The extra selector */
    Bit32u cs;                   /* The code selector */
    Bit32u ss;                   /* The application stack selector */
    Bit32u ds;                   /* The data selector */
    Bit32u fs;                   /* And another extra selector */
    Bit32u gs;                   /* ... and another one */
    Bit32u ldt;                  /* The local descriptor table */
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif
class Descriptor
{
public:
	Descriptor() { saved.fill[0]=saved.fill[1]=0; }

	void Load(PhysPt address);
	void Save(PhysPt address);

    PhysPt GetBase (void) const {
        return (PhysPt)(
            ((PhysPt)saved.seg.base_24_31 << (PhysPt)24U) |
            ((PhysPt)saved.seg.base_16_23 << (PhysPt)16U) |
             (PhysPt)saved.seg.base_0_15);
    }
	bool GetExpandDown (void) {
#if 0
	Bit32u limit_0_15	:16;
	Bit32u base_0_15	:16;
	Bit32u base_16_23	:8;
	Bit32u type			:5;
	Bit32u dpl			:2;
	Bit32u p			:1;
	Bit32u limit_16_19	:4;
	Bit32u avl			:1;
	Bit32u r			:1;
	Bit32u big			:1;
	Bit32u g			:1;
	Bit32u base_24_31	:8;
#endif
		if (!(saved.seg.type & 0x10)) /* must be storage type descriptor */
			return false;

		/* type: 1 0 E W A for data */
		/* type: 1 1 C R A for code */
		if (saved.seg.type & 0x08)
			return false;

		/* it's data. return the 'E' bit */
		return (saved.seg.type & 4) != 0;
	}
	Bitu GetLimit (void) const {
		const Bitu limit = ((Bitu)saved.seg.limit_16_19 << (Bitu)16U) | (Bitu)saved.seg.limit_0_15;
		if (saved.seg.g) return ((Bitu)limit << (Bitu)12U) | (Bitu)0xFFFU;
		return limit;
	}
	Bitu GetOffset(void) const {
		return ((Bitu)saved.gate.offset_16_31 << (Bitu)16U) | (Bitu)saved.gate.offset_0_15;
	}
	Bitu GetSelector(void) const {
		return saved.gate.selector;
	}
	Bitu Type(void) const {
		return saved.seg.type;
	}
	Bitu Conforming(void) const {
		return saved.seg.type & 8U;
	}
	Bitu DPL(void) const {
		return saved.seg.dpl;
	}
	Bitu Big(void) const {
		return saved.seg.big;
	}
public:
	union {
		S_Descriptor seg;
		G_Descriptor gate;
		Bit32u fill[2];
	} saved;
};

class DescriptorTable {
public:
    PhysPt  GetBase         (void) const    { return table_base;    }
    Bitu    GetLimit        (void) const    { return table_limit;   }
    void    SetBase         (PhysPt _base)  { table_base = _base;   }
    void    SetLimit        (Bitu _limit)   { table_limit= _limit;  }

    bool GetDescriptor  (Bitu selector, Descriptor& desc) {
        selector&=~7U;
        if (selector>=table_limit) return false;
        desc.Load((PhysPt)(table_base+selector));
        return true;
    }
	
	virtual void SaveState( std::ostream& stream );
	virtual void LoadState( std::istream& stream );


protected:
    PhysPt table_base;
    Bitu table_limit;
};

class GDTDescriptorTable : public DescriptorTable {
public:
	bool GetDescriptor(Bitu selector, Descriptor& desc) {
		Bitu address=selector & ~7U;
		if (selector & 4U) {
			if (address>=ldt_limit) return false;
			desc.Load((PhysPt)(ldt_base+address));
			return true;
		} else {
			if (address>=table_limit) return false;
			desc.Load((PhysPt)(table_base+address));
			return true;
		}
	}
	bool SetDescriptor(Bitu selector, Descriptor& desc) {
		Bitu address=selector & ~7U;
		if (selector & 4U) {
			if (address>=ldt_limit) return false;
			desc.Save((PhysPt)(ldt_base+address));
			return true;
		} else {
			if (address>=table_limit) return false;
			desc.Save((PhysPt)(table_base+address));
			return true;
		}
	} 
	Bitu SLDT(void) const {
		return ldt_value;
	}
	bool LLDT(Bitu value) {
		if ((value&0xfffc)==0) {
			ldt_value=0;
			ldt_base=0;
			ldt_limit=0;
			return true;
		}
		Descriptor desc;
		if (!GetDescriptor(value,desc)) return !CPU_PrepareException(EXCEPTION_GP,value);
		if (desc.Type()!=DESC_LDT) return !CPU_PrepareException(EXCEPTION_GP,value);
		if (!desc.saved.seg.p) return !CPU_PrepareException(EXCEPTION_NP,value);
		ldt_base=desc.GetBase();
		ldt_limit=desc.GetLimit();
		ldt_value=value;
		return true;
	}

	virtual void SaveState( std::ostream& stream );
	virtual void LoadState( std::istream& stream );

private:
	PhysPt ldt_base;
	Bitu ldt_limit;
	Bitu ldt_value;
};

class TSS_Descriptor : public Descriptor {
public:
	Bitu IsBusy(void) const {
		return saved.seg.type & 2;
	}
	Bitu Is386(void) const {
		return saved.seg.type & 8;
	}
	void SetBusy(const bool busy) {
		if (busy) saved.seg.type|=(2U);
		else saved.seg.type&=(~2U); /* -Wconversion cannot silence without hard-coding ~2U & 0x1F */
	}
};


struct CPUBlock {
	Bitu cpl;							/* Current Privilege */
	Bitu mpl;
	Bitu cr0;
	bool pmode;							/* Is Protected mode enabled */
	GDTDescriptorTable gdt;
	DescriptorTable idt;
	struct {
		Bitu cr0_and;
		Bitu cr0_or;
		Bitu eflags;
	} masks;
	struct {
		Bit32u mask,notmask;
		bool big;
	} stack;
	struct {
		bool big;
	} code;
	struct {
		Bitu cs,eip;
		CPU_Decoder * old_decoder;
	} hlt;
	struct {
		Bitu which,error;
	} exception;
	Bits direction;
	bool trap_skip;
	Bit32u drx[8];
	Bit32u trx[8];
};

extern CPUBlock cpu;

static INLINE void CPU_SetFlagsd(const Bitu word) {
	const Bitu mask=cpu.cpl ? FMASK_NORMAL : FMASK_ALL;
	CPU_SetFlags(word,mask);
}

static INLINE void CPU_SetFlagsw(const Bitu word) {
	const Bitu mask=(cpu.cpl ? FMASK_NORMAL : FMASK_ALL) & 0xffff;
	CPU_SetFlags(word,mask);
}

Bitu CPU_ForceV86FakeIO_In(Bitu port,Bitu len);
void CPU_ForceV86FakeIO_Out(Bitu port,Bitu val,Bitu len);

#endif
