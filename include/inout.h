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


#ifndef DOSBOX_INOUT_H
#define DOSBOX_INOUT_H

#include <stdio.h>
#include <stdint.h>

#define IO_MAX (64*1024+3)

#define IO_MB	0x1
#define IO_MW	0x2
#define IO_MD	0x4
#define IO_MA	(IO_MB | IO_MW | IO_MD )

class IO_CalloutObject;

typedef Bitu IO_ReadHandler(Bitu port,Bitu iolen);
typedef void IO_WriteHandler(Bitu port,Bitu val,Bitu iolen);

typedef IO_ReadHandler* (IO_ReadCalloutHandler)(IO_CalloutObject &co,Bitu port,Bitu iolen);
typedef IO_WriteHandler* (IO_WriteCalloutHandler)(IO_CalloutObject &co,Bitu port,Bitu iolen);

extern IO_WriteHandler * io_writehandlers[3][IO_MAX];
extern IO_ReadHandler * io_readhandlers[3][IO_MAX];

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range=1);
void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range=1);

void IO_InvalidateCachedHandler(Bitu port,Bitu range=1);

void IO_WriteB(Bitu port,uint8_t val);
void IO_WriteW(Bitu port,uint16_t val);
void IO_WriteD(Bitu port,Bit32u val);

uint8_t IO_ReadB(Bitu port);
uint16_t IO_ReadW(Bitu port);
Bit32u IO_ReadD(Bitu port);

static const Bitu IOMASK_ISA_10BIT = 0x3FFU; /* ISA 10-bit decode */
static const Bitu IOMASK_ISA_12BIT = 0xFFFU; /* ISA 12-bit decode */
static const Bitu IOMASK_FULL = 0xFFFFU; /* full 16-bit decode */

/* WARNING: Will only produce a correct result if 'x' is a nonzero power of two.
 * For use with IOMASK_Combine.
 *
 * A device with 16 I/O ports would produce a range mask of:
 *
 *       ~(16 - 1) = ~15 = ~0xF = 0xFFFFFFF0
 *
 *       or
 *
 *       ~(0x10 - 1) = ~0xF = 0xFFFFFFF0
 *
 */
static inline constexpr Bitu IOMASK_Range(const Bitu x) {
    return ~((Bitu)x - (Bitu)1U);
}

/* combine range mask with IOMASK value.
 *
 * Example: Sound Blaster 10-bit decode with 16 I/O ports:
 *
 *     IOMASK_Combine(IOMASK_ISA_10BIT,IOMASK_Range(16));
 *
 */
static inline constexpr Bitu IOMASK_Combine(const Bitu a,const Bitu b) {
    return a & b;
}

/* Classes to manage the IO objects created by the various devices.
 * The io objects will remove itself on destruction.*/
class IO_Base{
protected:
	bool installed;
	Bitu m_port, m_mask/*IO_MB, etc.*/, m_range/*number of ports*/;
public:
	IO_Base() : installed(false), m_port(0), m_mask(0), m_range(0) {};
};
/* NTS: To explain the Install() method, the caller not only provides the IOMASK_.. value, but ANDs
 *      the least significant bits to define the range of I/O ports to respond to. An ISA Sound Blaster
 *      for example would set portmask = (IOMASK_ISA_10BIT & (~0xF)) in order to respond to 220h-22Fh,
 *      240h-24Fh, etc. At I/O callout time, the callout object is tested
 *      if (cpu_ioport & io_mask) == (m_port & io_mask)
 *
 *      This does not prevent emulation of devices that start on non-aligned ports or strange port ranges,
 *      because the callout handler is free to decline the I/O request, leading the callout process to
 *      move on to the next device or mark the I/O port as empty. */
class IO_CalloutObject: private IO_Base {
public:
    IO_CalloutObject() : IO_Base(), io_mask(0xFFFFU), range_mask(0U), alias_mask(0xFFFFU), getcounter(0), m_r_handler(NULL), m_w_handler(NULL), alloc(false) {};
    void InvalidateCachedHandlers(void);
	void Install(Bitu port,Bitu portmask/*IOMASK_ISA_10BIT, etc.*/,IO_ReadCalloutHandler *r_handler,IO_WriteCalloutHandler *w_handler);
	void Uninstall();
public:
    uint16_t io_mask;
    uint16_t range_mask;
    uint16_t alias_mask;
    unsigned int getcounter;
    IO_ReadCalloutHandler *m_r_handler;
    IO_WriteCalloutHandler *m_w_handler;
    bool alloc;
public:
    inline bool MatchPort(const uint16_t p) {
        /* (p & io_mask) == (m_port & io_mask) but this also works.
         * apparently modern x86 processors are faster at addition/subtraction than bitmasking.
         * for this to work, m_port must be a multiple of the I/O range. For example, if the I/O
         * range is 16 ports, then m_port must be a multiple of 16. */
        return ((p - m_port) & io_mask) == 0;
    }
    inline bool isInstalled(void) {
        return installed;
    }
};
class IO_ReadHandleObject: private IO_Base {
public:
    IO_ReadHandleObject() : IO_Base() {};
	void Install(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
	void Uninstall();
	~IO_ReadHandleObject();
};
class IO_WriteHandleObject: private IO_Base{
public:
    IO_WriteHandleObject() : IO_Base() {};
	void Install(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);
	void Uninstall();
	~IO_WriteHandleObject();
};

static INLINE void IO_Write(Bitu port,uint8_t val) {
	IO_WriteB(port,val);
}
static INLINE uint8_t IO_Read(Bitu port){
	return IO_ReadB(port);
}

enum IO_Type_t {
    IO_TYPE_NONE=0,
    IO_TYPE_MIN=1,
    IO_TYPE_ISA=1,
    IO_TYPE_PCI,
    IO_TYPE_MB,

    IO_TYPE_MAX
};

void IO_InitCallouts(void);

typedef uint32_t IO_Callout_t;

static inline constexpr uint32_t IO_Callout_t_comb(const enum IO_Type_t t,const uint32_t idx) {
    return ((uint32_t)t << (uint32_t)28) + idx;
}

static inline constexpr enum IO_Type_t IO_Callout_t_type(const IO_Callout_t t) {
    return (enum IO_Type_t)(t >> 28);
}

static inline constexpr uint32_t IO_Callout_t_index(const IO_Callout_t t) {
    return t & (((uint32_t)1 << (uint32_t)28) - (uint32_t)1);
}

static const IO_Callout_t IO_Callout_t_none = (IO_Callout_t)0;

IO_Callout_t IO_AllocateCallout(IO_Type_t t);
void IO_FreeCallout(IO_Callout_t c);
IO_CalloutObject *IO_GetCallout(IO_Callout_t c);
void IO_PutCallout(IO_CalloutObject *obj);

#endif
