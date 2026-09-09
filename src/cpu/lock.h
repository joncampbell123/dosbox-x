#ifndef DOSBOX_CPU_LOCK_H
#define DOSBOX_CPU_LOCK_H

/*
 *  Copyright (C) 2026  The DOSBox-X Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <cstdint>

template <typename Address, typename ReadByte>
static INLINE bool CPU_LockPrefixValid(Address ip, ReadByte read_byte) {
	uint8_t opcode;
	do {
		opcode=read_byte(ip++);
	} while (opcode==0x26 || opcode==0x2e || opcode==0x36 || opcode==0x3e ||
			 opcode==0x64 || opcode==0x65 || opcode==0x66 || opcode==0x67);

	if (opcode==0x0f) {
		opcode=read_byte(ip++);
		if (opcode==0xba) {
			const uint8_t modrm=read_byte(ip);
			return (modrm>>6)!=3 && ((modrm>>3)&7)>=5;
		}

		if (opcode!=0xa3 && opcode!=0xab && opcode!=0xb0 && opcode!=0xb1 &&
			opcode!=0xb3 && opcode!=0xbb && opcode!=0xc0 && opcode!=0xc1 &&
			opcode!=0xc7)
			return false;
		const uint8_t modrm=read_byte(ip);
		return (modrm>>6)!=3 &&
			(opcode!=0xc7 || ((modrm>>3)&7)==1);
	}

	const uint8_t modrm=read_byte(ip);
	if (opcode>=0x80 && opcode<=0x83)
		return (modrm>>6)!=3;
	if (opcode==0xfe || opcode==0xff)
		return (modrm>>6)!=3 && ((modrm>>3)&7)<=1;
	if (opcode==0xf6 || opcode==0xf7)
		return (modrm>>6)!=3 &&
			(((modrm>>3)&7)==2 || ((modrm>>3)&7)==3);
	return false;
}

#endif
