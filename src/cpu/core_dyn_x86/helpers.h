/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

static bool dyn_helper_divb(Bit8u val) {
	if (!val) return CPU_PrepareException(0,0);
	Bitu quo=reg_ax / val;
	Bit8u rem=(Bit8u)(reg_ax % val);
	Bit8u quo8=(Bit8u)(quo&0xff);
	if (quo>0xff) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8;
	return false;
}

static bool dyn_helper_idivb(Bit8s val) {
	if (!val) return CPU_PrepareException(0,0);
	Bits quo=(Bit16s)reg_ax / val;
	Bit8s rem=(Bit8s)((Bit16s)reg_ax % val);
	Bit8s quo8s=(Bit8s)(quo&0xff);
	if (quo!=(Bit16s)quo8s) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8s;
	return false;
}

static bool dyn_helper_divw(Bit16u val) {
	if (!val) return CPU_PrepareException(0,0);
	Bitu num=(reg_dx<<16)|reg_ax;
	Bitu quo=num/val;
	Bit16u rem=(Bit16u)(num % val);
	Bit16u quo16=(Bit16u)(quo&0xffff);
	if (quo!=(Bit32u)quo16) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16;
	return false;
}

static bool dyn_helper_idivw(Bit16s val) {
	if (!val) return CPU_PrepareException(0,0);
	Bits num=(reg_dx<<16)|reg_ax;
	Bits quo=num/val;
	Bit16s rem=(Bit16s)(num % val);
	Bit16s quo16s=(Bit16s)quo;
	if (quo!=(Bit32s)quo16s) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16s;
	return false;
}

static bool dyn_helper_divd(Bit32u val) {
	if (!val) return CPU_PrepareException(0,0);
	Bit64u num=(((Bit64u)reg_edx)<<32)|reg_eax;
	Bit64u quo=num/val;
	Bit32u rem=(Bit32u)(num % val);
	Bit32u quo32=(Bit32u)(quo&0xffffffff);
	if (quo!=(Bit64u)quo32) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32;
	return false;
}

static bool dyn_helper_idivd(Bit32s val) {
	if (!val) return CPU_PrepareException(0,0);
	Bit64s num=(((Bit64u)reg_edx)<<32)|reg_eax;
	Bit64s quo=num/val;
	Bit32s rem=(Bit32s)(num % val);
	Bit32s quo32s=(Bit32s)(quo&0xffffffff);
	if (quo!=(Bit64s)quo32s) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32s;
	return false;
}
