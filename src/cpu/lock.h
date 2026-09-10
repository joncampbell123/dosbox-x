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

// ModR/M helpers: mod==3 selects a register operand (no memory access,
// so not a valid LOCK target); reg is the middle 3-bit opcode-extension
// field used by several instruction groups.
static INLINE bool ModRmIsMemory(uint8_t modrm) { return (modrm>>6)!=3; }
static INLINE uint8_t ModRmReg(uint8_t modrm) { return (modrm>>3)&7; }

template <typename Address, typename ReadByte>
static INLINE bool CPU_LockPrefixValid(Address ip, ReadByte read_byte) {
	uint8_t opcode;
	do {
		opcode=read_byte(ip++);
	} while (opcode==0x26 || opcode==0x2e || opcode==0x36 || opcode==0x3e ||
			 opcode==0x64 || opcode==0x65 || opcode==0x66 || opcode==0x67);

	if (opcode==0x0f) {
		opcode=read_byte(ip++);
		switch (opcode) {
			case 0xab: // BTS
			case 0xb0: case 0xb1: // CMPXCHG
			case 0xb3: // BTR
				break;
			case 0xba: { // BTS/BTR/BTC imm8 (reg>=5; BT itself, reg==4, is excluded)
				const uint8_t modrm=read_byte(ip);
				return ModRmIsMemory(modrm) && ModRmReg(modrm)>=5;
			}
			case 0xbb: // BTC
			case 0xc0: case 0xc1: // XADD
				break;
			case 0xc7: { // CMPXCHG8B/CMPXCHG16B
				const uint8_t modrm=read_byte(ip);
				return ModRmIsMemory(modrm) && ModRmReg(modrm)==1;
			}
			default:
				return false;
		}
		const uint8_t modrm=read_byte(ip);
		return ModRmIsMemory(modrm);
	}

	switch (opcode) {
		// ADD/OR/ADC/SBB/AND/SUB/XOR Eb/Ev,Gb/Gv (r/m is destination;
		// CMP's forms 0x38/0x39 are excluded since CMP does not write
		// its operand)
		case 0x00: case 0x01: case 0x08: case 0x09:
		case 0x10: case 0x11: case 0x18: case 0x19:
		case 0x20: case 0x21: case 0x28: case 0x29:
		case 0x30: case 0x31:
		// XCHG Eb/Ev,Gb/Gv
		case 0x86: case 0x87: {
			const uint8_t modrm=read_byte(ip);
			return ModRmIsMemory(modrm);
		}
		case 0x80: case 0x81: case 0x82: case 0x83: { // ADD/OR/../XOR imm (excl. CMP)
			const uint8_t modrm=read_byte(ip);
			return ModRmIsMemory(modrm) && ModRmReg(modrm)!=7;
		}
		case 0xfe: case 0xff: { // INC/DEC
			const uint8_t modrm=read_byte(ip);
			return ModRmIsMemory(modrm) && ModRmReg(modrm)<=1;
		}
		case 0xf6: case 0xf7: { // NOT/NEG
			const uint8_t modrm=read_byte(ip);
			return ModRmIsMemory(modrm) &&
				(ModRmReg(modrm)==2 || ModRmReg(modrm)==3);
		}
		default:
			return false;
	}
}

#endif
