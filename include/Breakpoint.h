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

#include "dosbox.h"
#include "mem.h"
#include <list>

#if C_DEBUG

class Breakpoint
{
    public:
        static const int BPINT_ALL = 0x100;
        static std::list<Breakpoint*> allBreakpoints;

        enum BreakpointType {
            BP_UNKNOWN,
            BP_PHYSICAL,
            BP_INTERRUPT,
            BP_MEMORY,
            BP_MEMORY_PROT,
            BP_MEMORY_LINEAR
        };

        BreakpointType type = BP_UNKNOWN;
        // Physical
        PhysPt location = 0;
#if !C_HEAVY_DEBUG
        uint8_t oldData;
#endif
        uint16_t segment = 0;
        uint32_t offset = 0;
        // Int
        uint8_t	intNr = 0;
        uint16_t ahValue = 0;
        uint16_t alValue = 0;
        // Shared
        bool active = false;
        bool once = false;

        Breakpoint(void);
        void Activate(bool _active);
        void SetAddress(uint16_t seg, uint32_t off);
        void SetAddress(PhysPt adr);
        void SetInt(uint8_t _intNr, uint16_t ah, uint16_t al);
        void SetValue(uint8_t value);
        void SetOther(uint8_t other);
        bool IsActive(void);
        uint16_t GetValue(void);
        uint16_t GetOther(void);

        // static methods
        static Breakpoint* AddBreakpoint(uint16_t seg, uint32_t off, bool once);
        static Breakpoint* AddIntBreakpoint(uint8_t intNum, uint16_t ah, uint16_t al, bool once);
        static Breakpoint* AddMemBreakpoint(uint16_t seg, uint32_t off);
        static void	DeactivateBreakpoints();
        static void	ActivateBreakpoints();
        static void	ActivateBreakpointsExceptAt(PhysPt adr);
        static bool	CheckBreakpoint(uint16_t seg, uint32_t off);
        static bool	CheckIntBreakpoint(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue);
        static Breakpoint* FindPhysBreakpoint(uint16_t seg, uint32_t off, bool once);
        static Breakpoint* FindOtherActiveBreakpoint(PhysPt adr, Breakpoint* skip);
        static bool	IsBreakpoint(uint16_t seg, uint32_t off);
        static bool	DeleteBreakpoint(uint16_t seg, uint32_t off);
        static bool	DeleteByIndex(uint16_t index);
        static void	DeleteAll(void);
        static void	ShowList(void);
#if C_HEAVY_DEBUG
        friend bool DEBUG_HeavyIsBreakpoint(void);
#endif
};

#endif // C_DEBUG
