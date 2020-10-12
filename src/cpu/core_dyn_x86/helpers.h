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

static bool dyn_helper_divb(uint8_t val) {
	if (!val) return CPU_PrepareException(0,0);
	Bitu quo=reg_ax / val;
	uint8_t rem=(uint8_t)(reg_ax % val);
	uint8_t quo8=(uint8_t)(quo&0xff);
	if (quo>0xff) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8;
	return false;
}

static bool dyn_helper_idivb(int8_t val) {
	if (!val) return CPU_PrepareException(0,0);
	Bits quo=(int16_t)reg_ax / val;
	int8_t rem=(int8_t)((int16_t)reg_ax % val);
	int8_t quo8s=(int8_t)(quo&0xff);
	if (quo!=(int16_t)quo8s) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8s;
	return false;
}

static bool dyn_helper_divw(uint16_t val) {
	if (!val) return CPU_PrepareException(0,0);
	uint32_t num=(((uint32_t)reg_dx)<<16)|reg_ax;
	uint32_t quo=num/val;
	uint16_t rem=(uint16_t)(num % val);
	uint16_t quo16=(uint16_t)(quo&0xffff);
	if (quo!=(uint32_t)quo16) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16;
	return false;
}

static bool dyn_helper_idivw(int16_t val) {
	if (!val) return CPU_PrepareException(0,0);
	int32_t num=(((uint32_t)reg_dx)<<16)|reg_ax;
	int32_t quo=num/val;
	int16_t rem=(int16_t)(num % val);
	int16_t quo16s=(int16_t)quo;
	if (quo!=(int32_t)quo16s) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16s;
	return false;
}

static bool dyn_helper_divd(uint32_t val) {
	if (!val) return CPU_PrepareException(0,0);
	uint64_t num=(((uint64_t)reg_edx)<<32)|reg_eax;
	uint64_t quo=num/val;
	uint32_t rem=(uint32_t)(num % val);
	uint32_t quo32=(uint32_t)(quo&0xffffffff);
	if (quo!=(uint64_t)quo32) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32;
	return false;
}

static bool dyn_helper_idivd(int32_t val) {
	if (!val) return CPU_PrepareException(0,0);
	int64_t num=(((uint64_t)reg_edx)<<32)|reg_eax;
	int64_t quo=num/val;
	int32_t rem=(int32_t)(num % val);
	int32_t quo32s=(int32_t)(quo&0xffffffff);
	if (quo!=(int64_t)quo32s) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32s;
	return false;
}
