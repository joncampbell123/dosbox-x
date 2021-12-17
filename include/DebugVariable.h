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
#include "support.h"
#include <vector>

#if C_DEBUG

class DebugVariable
{
    public:
        DebugVariable(char* _name, PhysPt _adr);
        char* GetName(void);
        PhysPt GetAdr(void);
        void SetValue(bool has, uint16_t val);
        uint16_t GetValue(void);
        bool HasValue(void);

        // static
        static std::vector<DebugVariable*> allVariables;
        static void InsertVariable(char* name, PhysPt adr);
        static DebugVariable* FindVar(PhysPt pt);
        static void DeleteAll();
        static bool SaveVars(char* name);
        static bool LoadVars(char* name);

    private:
        PhysPt  adr;
        char    name[16];
        bool    hasvalue;
        uint16_t  value;
};

#endif // C_DEBUG
