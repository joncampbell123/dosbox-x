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

{
	extern int cpu_rep_max;
	static PhysPt  si_base,di_base;
	static Bit32u	si_index,di_index;
	static Bit32u	add_mask;
	static Bitu	count,count_left;
	static Bits	add_index;

	count_left=0;

	add_index=cpu.direction;
	if (inst.prefix & PREFIX_SEG) si_base=inst.seg.base;
	else si_base=SegBase(ds);
	di_base=SegBase(es);

	if (inst.prefix & PREFIX_ADDR) {
		add_mask=0xFFFFFFFF;
		si_index=reg_esi;
		di_index=reg_edi;
		count=reg_ecx;
	} else {
		add_mask=0xFFFF;
		si_index=reg_si;
		di_index=reg_di;
		count=reg_cx;
	}

	if (!(inst.prefix & PREFIX_REP)) {
		count=1;
	} else {
		/* we allow the user to cap our count as a way of making REP string operations interruptable (and at what granularity) */
		/* NTS: This condition is less important now that the loops themselves break when CPU_Cycles <= 0. when this code was
		 *      initially implemented the string ops stubbornly counted through the bytes regardless of pending interrupts and
		 *      it caused problems with code that needed fine timing i.e. demos that played music through the LPT DAC while
		 *      using REP OUTSB to the VGA palette suffered from audio quality problems. at this phase of implementation the
		 *      "interruptible string ops" parameter is now merely a testing parameter that can be used to verify this code
		 *      breaks and restarts string ops correctly. */
		if (cpu_rep_max > 0 && count > (unsigned int)cpu_rep_max) {
			count_left+=count-(unsigned int)cpu_rep_max;
			count=(unsigned int)cpu_rep_max;
		}
	}

	if (count != 0) {
		try {
			switch (inst.code.op) {
				case R_OUTSB:
					do {
						IO_WriteB(reg_dx,LoadMb(si_base+si_index));
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_OUTSW:
					add_index<<=1;
					do {
						IO_WriteW(reg_dx,LoadMw(si_base+si_index));
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_OUTSD:
					add_index<<=2;
					do {
						IO_WriteD(reg_dx,LoadMd(si_base+si_index));
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;

				case R_INSB:
					do {
						SaveMb(di_base+di_index,IO_ReadB(reg_dx));
						di_index=(di_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_INSW:
					add_index<<=1;
					do {
						SaveMw(di_base+di_index,IO_ReadW(reg_dx));
						di_index=(di_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_INSD:
					add_index<<=2;
					do {
						SaveMd(di_base+di_index,IO_ReadD(reg_dx));
						di_index=(di_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;

				case R_STOSB:
					do {
						SaveMb(di_base+di_index,reg_al);
						di_index=(di_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_STOSW:
					add_index<<=1;
					do {
						SaveMw(di_base+di_index,reg_ax);
						di_index=(di_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_STOSD:
					add_index<<=2;
					do {
						SaveMd(di_base+di_index,reg_eax);
						di_index=(di_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;

				case R_MOVSB:
					do {
						SaveMb(di_base+di_index,LoadMb(si_base+si_index));
						di_index=(di_index+(Bitu)add_index) & add_mask;
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_MOVSW:
					add_index<<=1;
					do {
						SaveMw(di_base+di_index,LoadMw(si_base+si_index));
						di_index=(di_index+(Bitu)add_index) & add_mask;
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_MOVSD:
					add_index<<=2;
					do {
						SaveMd(di_base+di_index,LoadMd(si_base+si_index));
						di_index=(di_index+(Bitu)add_index) & add_mask;
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;

				case R_LODSB:
					do {
						reg_al=LoadMb(si_base+si_index);
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_LODSW:
					add_index<<=1;
					do {
						reg_ax=LoadMw(si_base+si_index);
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;
				case R_LODSD:
					add_index<<=2;
					do {
						reg_eax=LoadMd(si_base+si_index);
						si_index=(si_index+(Bitu)add_index) & add_mask;
						count--;

						if ((--CPU_Cycles) <= 0) break;
					} while (count != 0); break;

				case R_SCASB:
					{
						uint8_t val2;
						do {
							val2=LoadMb(di_base+di_index);
							di_index=(di_index+(Bitu)add_index) & add_mask;
							count--;

							if ((--CPU_Cycles) <= 0) break;
							if ((reg_al==val2)!=inst.repz) break;
						} while (count != 0);
						CMPB(reg_al,val2,LoadD,0);
					}
					break;
				case R_SCASW:
					add_index<<=1;
					{
						uint16_t val2;
						do {
							val2=LoadMw(di_base+di_index);
							di_index=(di_index+(Bitu)add_index) & add_mask;
							count--;

							if ((--CPU_Cycles) <= 0) break;
							if ((reg_ax==val2)!=inst.repz) break;
						} while (count != 0);
						CMPW(reg_ax,val2,LoadD,0);
					}
					break;
				case R_SCASD:
					add_index<<=2;
					{
						Bit32u val2;
						do {
							val2=LoadMd(di_base+di_index);
							di_index=(di_index+(Bitu)add_index) & add_mask;
							count--;

							if ((--CPU_Cycles) <= 0) break;
							if ((reg_eax==val2)!=inst.repz) break;
						} while (count != 0);
						CMPD(reg_eax,val2,LoadD,0);
					}
					break;

				case R_CMPSB:
					{
						uint8_t val1,val2;
						do {
							val1=LoadMb(si_base+si_index);
							val2=LoadMb(di_base+di_index);
							si_index=(si_index+(Bitu)add_index) & add_mask;
							di_index=(di_index+(Bitu)add_index) & add_mask;
							count--;

							if ((--CPU_Cycles) <= 0) break;
							if ((val1==val2)!=inst.repz) break;
						} while (count != 0);
						CMPB(val1,val2,LoadD,0);
					}
					break;
				case R_CMPSW:
					add_index<<=1;
					{
						uint16_t val1,val2;
						do {
							val1=LoadMw(si_base+si_index);
							val2=LoadMw(di_base+di_index);
							si_index=(si_index+(Bitu)add_index) & add_mask;
							di_index=(di_index+(Bitu)add_index) & add_mask;
							count--;

							if ((--CPU_Cycles) <= 0) break;
							if ((val1==val2)!=inst.repz) break;
						} while (count != 0);
						CMPW(val1,val2,LoadD,0);
					}
					break;
				case R_CMPSD:
					add_index<<=2;
					{
						Bit32u val1,val2;
						do {
							val1=LoadMd(si_base+si_index);
							val2=LoadMd(di_base+di_index);
							si_index=(si_index+(Bitu)add_index) & add_mask;
							di_index=(di_index+(Bitu)add_index) & add_mask;
							count--;

							if ((--CPU_Cycles) <= 0) break;
							if ((val1==val2)!=inst.repz) break;
						} while (count != 0);
						CMPD(val1,val2,LoadD,0);
					}
					break;

				default:
					LOG(LOG_CPU,LOG_ERROR)("Unhandled string op %d",inst.code.op);
			}

			/* Clean up after certain amount of instructions */
			reg_esi&=(~add_mask);
			reg_esi|=(si_index & add_mask);
			reg_edi&=(~add_mask);
			reg_edi|=(di_index & add_mask);
			if (inst.prefix & PREFIX_REP) {
				count+=count_left;
				reg_ecx&=(~add_mask);
				reg_ecx|=(count & add_mask);

				/* if the count is still nonzero, then there is still work to do and the
				 * instruction has not finished executing---it needs to be restarted.
				 * if the string op was REP CMPSB or REP SCASB then it also matters
				 * whether the ZF flag matches the REP condition on whether or not we
				 * restart the instruction. */
				if (count != 0) {
					if (inst.code.op < R_SCASB) {
						/* if count != 0 then restart the instruction */
						LoadIP();
					}
					else {
						/* if ZF matches the REP condition, restart the instruction */
						if ((get_ZF()?1:0) == (inst.repz?1:0)) {
							LoadIP();
						}
					}
				}
			}
		}
		catch (GuestPageFaultException &pf) {
			(void)pf;
			/* Clean up after certain amount of instructions */
			reg_esi&=(~add_mask);
			reg_esi|=(si_index & add_mask);
			reg_edi&=(~add_mask);
			reg_edi|=(di_index & add_mask);
			if (inst.prefix & PREFIX_REP) {
				count+=count_left;
				reg_ecx&=(~add_mask);
				reg_ecx|=(count & add_mask);
			}

			/* rethrow the exception.
			 * NOTE: this means the normal core has no chance to execute SAVEIP, therefore
			 *       when the guest OS has finished handling the page fault the instruction
			 *       pointer will come right back to the string op that caused the fault
			 *       and the string op will restart where it left off. */
			throw;
		}
	}
}

