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

#ifndef DOSBOX_FPU_STATE_H
#define DOSBOX_FPU_STATE_H

#include <cstdint>
#include <string>

#include "cpu.h"
#include "fpu_types.h"

enum FPU_Tag {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

template<class T, unsigned bitno, unsigned nbits=1>
struct RegBit
{
	enum { basemask = (1 << nbits) - 1 };
	enum { mask = basemask << bitno };
	T data;
	RegBit(const T& reg) : data(reg) {}
	template <class T2> RegBit& operator=(T2 val)
	{
		data = (data & ~mask) | ((nbits > 1 ? val & basemask : !!val) << bitno);
		return *this;
	}
	operator unsigned() const { return (data & mask) >> bitno; }
};

struct FPUControlWord
{
	union
	{
		uint16_t reg = initValue;
		RegBit<decltype(reg), 0>     IM;  // Invalid operation mask
		RegBit<decltype(reg), 1>     DM;  // Denormalized operand mask
		RegBit<decltype(reg), 2>     ZM;  // Zero divide mask
		RegBit<decltype(reg), 3>     OM;  // Overflow mask
		RegBit<decltype(reg), 4>     UM;  // Underflow mask
		RegBit<decltype(reg), 5>     PM;  // Precision mask
		RegBit<decltype(reg), 7>     M;   // Interrupt mask   (8087-only)
		RegBit<decltype(reg), 8, 2>  PC;  // Precision control
		RegBit<decltype(reg), 10, 2> RC;  // Rounding control
		RegBit<decltype(reg), 12>    IC;  // Infinity control (8087/80287-only)
	};

	enum
	{
		mask8087     = 0x1fff,
		maskNon8087  = 0x1f7f,
		reservedMask = 0x40,
		initValue    = 0x37f
	};
	enum RoundMode
	{
		Nearest = 0,
		Down    = 1,
		Up      = 2,
		Chop    = 3
	};

	FPUControlWord() {}
	FPUControlWord(const FPUControlWord& other) = default;
	FPUControlWord& operator=(const FPUControlWord& other)
	{
		reg = other.reg;
		return *this;
	}
	template<class T>
	FPUControlWord& operator=(T val)
	{
		reg = (val & (FPU_ArchitectureType<=FPU_ARCHTYPE_8087 ? mask8087 : maskNon8087)) | reservedMask;
		return *this;
	}
	operator unsigned() const
	{
		return reg;
	}
	template <class T>
	FPUControlWord& operator |=(T val)
	{
		*this = reg | val;
		return *this;
	}
	void init() { reg = initValue; }
	FPUControlWord allMasked() const
	{
		auto masked = *this;
		masked |= IM.mask | DM.mask | ZM.mask | OM.mask | UM.mask | PM.mask;
		return masked;
	}
};

struct FPUStatusWord
{
	union
	{
		uint16_t reg = 0;
		RegBit<decltype(reg), 0>     IE;  // Invalid operation
		RegBit<decltype(reg), 1>     DE;  // Denormalized operand
		RegBit<decltype(reg), 2>     ZE;  // Divide-by-zero
		RegBit<decltype(reg), 3>     OE;  // Overflow
		RegBit<decltype(reg), 4>     UE;  // Underflow
		RegBit<decltype(reg), 5>     PE;  // Precision
		RegBit<decltype(reg), 6>     SF;  // Stack Flag (non-8087/802087)
		RegBit<decltype(reg), 7>     IR;  // Interrupt request (8087-only)
		RegBit<decltype(reg), 7>     ES;  // Error summary     (non-8087)
		RegBit<decltype(reg), 8>     C0;  // Condition flag
		RegBit<decltype(reg), 9>     C1;  // Condition flag
		RegBit<decltype(reg), 10>    C2;  // Condition flag
		RegBit<decltype(reg), 11, 3> top; // Top of stack pointer
		RegBit<decltype(reg), 14>    C3;  // Condition flag
		RegBit<decltype(reg), 15>    B;   // Busy flag
	};

	FPUStatusWord() {}
	FPUStatusWord(const FPUStatusWord& other) = default;
	FPUStatusWord& operator=(const FPUStatusWord& other)
	{
		reg = other.reg;
		return *this;
	}
	template<class T>
	FPUStatusWord& operator=(T val)
	{
		reg = val;
		return *this;
	}
	operator unsigned() const
	{
		return reg;
	}
	template <class T>
	FPUStatusWord& operator |=(T val)
	{
		*this = reg | val;
		return *this;
	}
	void init() { reg = 0; }
	void clearExceptions()
	{
		IE = false; DE = false; ZE = false; OE = false; UE = false; PE = false;
		ES = false;
	}
	enum
	{
		conditionMask = 0x4700,
		conditionAndExceptionMask = 0x47bf
	};
	std::string to_string() const;
};


struct FPU
{
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	FPU_Reg		    _do_not_use__regs[9];
#else
	FPU_Reg		    regs[9];
#endif
	union {/*these two have the same format, so alias them as an anon union to make switching between dynamic and normal core easier!*/
		FPU_P_Reg	p_regs[9];
		FPU_Reg_80	regs_80[9];
	};
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	bool		    _do_not_use__use80[9];		// if set, use the 80-bit precision version
#else
	bool		    use80[9];		// if set, use the 80-bit precision version
#endif
	FPU_Tag		    tags[9];
	FPUControlWord  cw;
	FPUStatusWord   sw;
	XMM_Reg			xmmreg[8]; // SSE emulation
	uint32_t		mxcsr; // SSE control register
};

extern FPU fpu;

static INLINE Bitu FPU_StackIndex(Bitu index)
{
	return (fpu.sw.top + index) & 7;
}

#endif // DOSBOX_FPU_STATE_H
