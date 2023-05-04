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


#include <time.h>
#include <math.h>

#include "dosbox.h"
#include "timer.h"
#include "cpu.h"
#include "pic.h"
#include "inout.h"
#include "mem.h"
#include "bios_disk.h"
#include "setup.h"
#include "cross.h" //fmod on certain platforms
#include "control.h"
#if defined (WIN32) && !defined (__MINGW32__)
#include "sys/timeb.h"
#else
#include "sys/time.h"
#endif

// sigh... Windows doesn't know gettimeofday
#if defined (WIN32) && !defined (__MINGW32__)
typedef Bitu suseconds_t;

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

static void gettimeofday (timeval* ptime, void* pdummy) {
    (void)pdummy;
    struct _timeb thetime;
    _ftime(&thetime);

    ptime->tv_sec = thetime.time;
    ptime->tv_usec = (Bitu)thetime.millitm;
}

#endif

static struct {
    uint8_t regs[0x40];
    bool nmi;
    bool bcd;
    bool ampm;                  // am/pm mode (false = 24h mode)
    bool lock;                  // lock bit set (no updates)
    uint8_t reg;
    struct {
        uint8_t div;
        float delay;
        bool acknowledged;
    } timer;
    struct {
        uint8_t sec = 0,min = 0,hour = 0;
        uint8_t weekday = 1,day = 1,month = 1;
        uint16_t year = 1980;
    } clock;
    struct {
        uint8_t sec = 0,min = 0,hour = 0;
    } alarm;
    time_t clock_time_t = 0;
    struct {
        double ended;
    } last;
} cmos;

const uint8_t BIOS_DATE_months[] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

extern bool         sync_time;

void cmos_sync_time(time_t t) {
    struct tm *tm = localtime(&t);

    cmos.clock.sec = tm->tm_sec;
    cmos.clock.min = tm->tm_min;
    cmos.clock.hour = tm->tm_hour;
    cmos.clock.weekday = tm->tm_wday + 1;
    cmos.clock.day = tm->tm_mday;
    cmos.clock.month = tm->tm_mon + 1;
    cmos.clock.year = tm->tm_year + 1900;
    cmos.clock_time_t = t;

    LOG(LOG_MISC,LOG_DEBUG)("CMOS sync to %04u-%02u-%02u %02u:%02u:%02u",cmos.clock.year,cmos.clock.month,cmos.clock.day,cmos.clock.hour,cmos.clock.min,cmos.clock.sec);
}

bool cmos_sync_flag = false;
uint8_t cmos_sync_sec = 0,cmos_sync_min = 0,cmos_sync_hour = 0;

static void cmos_tick(void) {
    ++cmos.clock_time_t;

    if (sync_time) {
        time_t now = time(NULL);
        long dt = (long)now - (long)cmos.clock_time_t;
        if (labs(dt) >= 5l) {
            cmos_sync_time(now);
            cmos_sync_flag = true;
            cmos_sync_sec = cmos.clock.sec;
            cmos_sync_min = cmos.clock.min;
            cmos_sync_hour = cmos.clock.hour;
            dos.date.year = cmos.clock.year;
            dos.date.month = cmos.clock.month;
            dos.date.day = cmos.clock.day;
            return;
        }
        else if ((unsigned int)now & 1u) {
            if (dt >= 2l) cmos_sync_time(cmos.clock_time_t+1l);
            else if (dt <= -2l) cmos_sync_time(cmos.clock_time_t-1l);
            if (labs(dt) >= 2l) {
                cmos_sync_flag = true;
                cmos_sync_sec = cmos.clock.sec;
                cmos_sync_min = cmos.clock.min;
                cmos_sync_hour = cmos.clock.hour;
                dos.date.year = cmos.clock.year;
                dos.date.month = cmos.clock.month;
                dos.date.day = cmos.clock.day;
            }
            return;
        }
    }

    if (++cmos.clock.sec < 60) return;
    cmos.clock.sec = 0;

    if (++cmos.clock.min < 60) return;
    cmos.clock.min = 0;

    if (++cmos.clock.hour < 24) return;
    cmos.clock.hour = 0;

    if (++cmos.clock.weekday > 7)
        cmos.clock.weekday = 1;

    if (cmos.clock.month < 1 || cmos.clock.month > 12) cmos.clock.month = 1;
    uint8_t mdays = BIOS_DATE_months[cmos.clock.month];
    if (cmos.clock.month == 2 && cmos.clock.year%4==0 && (cmos.clock.year%100!=0 || cmos.clock.year%400==0)) mdays++; /* Feb 29th leap year */
    if (++cmos.clock.day < mdays) return;
    cmos.clock.day = 1;

    if (++cmos.clock.month < 12) return;
    cmos.clock.month = 1;

    ++cmos.clock.year;
}

static void cmos_checkalarm(void) {
    if (cmos.clock.sec == cmos.alarm.sec && cmos.clock.min == cmos.alarm.min && cmos.clock.hour == cmos.alarm.hour)
        cmos.regs[0xc] |= 0x20; /* Alarm Flag (AF) */
}

static void cmos_timerevent(Bitu val) {
    (void)val;//UNUSED
    {
        double index = PIC_FullIndex();
        double remd = fma((index/(double)cmos.timer.delay), -(double)cmos.timer.delay, index);
        //double remd = fmod(index, (double)cmos.timer.delay); // original delay calculation
        //double remd = index - trunc(index / (double)cmos.timer.delay) * (double)cmos.timer.delay; // alternative fix
        //LOG_MSG("cmos timerevent: index=%f, interval=%f", index, cmos.timer.delay - remd);

        // Periodic Interrupt Flag (PF)
        if (cmos.regs[0xb] & 0x40) cmos.regs[0xc] |= 0x40;

        if (index >= (cmos.last.ended + 1000 - 0.001)) { // consider sometimes index is slightly before 1.0sec
            //LOG_MSG("cmos timerevent: index=%f, interval=%f", index, cmos.last.ended - index);
            if (cmos.last.ended < (index-1000)) cmos.last.ended = (index-1000);
            cmos.last.ended -= fmod(cmos.last.ended,1000);
            cmos.last.ended += 1000;

            if (!cmos.lock) {
                cmos_tick();
                if (cmos.regs[0xb] & 0x20/*AIE*/) cmos_checkalarm();
            }

            // Update-Ended Interrupt Flag (UF)
            if (cmos.regs[0xb] & 0x10) cmos.regs[0xc] |= 0x10;
        }

        if (cmos.regs[0xb] & 0x40) { /* PIE */
            PIC_AddEvent(cmos_timerevent, (float)((double)cmos.timer.delay - remd));
        }
        else { /* UIE, or the RTC always ticks once a second anyway */
            double delay = (double)cmos.last.ended + 1000 - index;
            if (delay < 0.01) delay = 0.01;
            PIC_AddEvent(cmos_timerevent, (float)delay);
        }
    }
    if (cmos.timer.acknowledged && (cmos.regs[0x0c] & 0x70/*PIE|AIE|UIE*/)) {
        cmos.timer.acknowledged=false;
        PIC_ActivateIRQ(8);
    }
}

static void cmos_checktimer(void) {
    PIC_RemoveEvents(cmos_timerevent);
    if (cmos.timer.div<=2) cmos.timer.div+=7;
    cmos.timer.delay=(1000.0f/(32768.0f / (1 << (cmos.timer.div - 1))));
    if (!cmos.timer.div) return;
    LOG(LOG_PIT,LOG_NORMAL)("RTC Timer at %.2f hz",1000.0/cmos.timer.delay);
//  PIC_AddEvent(cmos_timerevent,cmos.timer.delay);
    /* A rtc is always running */
    //double remd=fmod(PIC_FullIndex(),(double)cmos.timer.delay);
    double index = PIC_FullIndex();
    double remd = fma((index / (double)cmos.timer.delay), -(double)cmos.timer.delay, index);
    PIC_AddEvent(cmos_timerevent,(float)((double)cmos.timer.delay-remd)); //Should be more like a real pc. Check
//  status reg A reading with this (and with other delays actually)
}

void cmos_selreg(Bitu port,Bitu val,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    if (machine != MCH_PCJR) {
        /* bit 7 also controls NMI masking, if set, NMI is disabled */
        CPU_NMI_gate = (val&0x80) ? false : true;
    }

    cmos.reg=val & 0x3f;
    cmos.nmi=(val & 0x80)>0;
}

static void cmos_writereg(Bitu port,Bitu val,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    if (cmos.reg <= 0x09 || cmos.reg == 0x32 || cmos.reg == 0x37) {   // date/time related registers
        if (cmos.bcd)           // values supplied are BCD, convert to binary values
        {
            if ((val & 0xf0) > 0x90 || (val & 0x0f) > 0x09) return;     // invalid BCD value
            // other checks for valid values are done in case-switch

            // convert pm hours differently (bcd 81-92 corresponds to hex 81-8c)
            if (cmos.reg == 0x04 && val >= 0x80)
            {
                val = (val < 90) ? 0x80 : 0x8a + (val & 0x0f);
            }
            else
            {
                val = ((val >> 4) * 10) + (val & 0x0f);
            }
        }
    }

    switch (cmos.reg) {
        case 0x00:      /* Seconds */
            if (val > 59) return;       // invalid seconds value
            cmos.clock.sec = val; break;
        case 0x01:      /* Seconds, alarm */
            if (val > 59) return;       // invalid seconds value
            cmos.alarm.sec = val; break;
        case 0x02:      /* Minutes */
            if (val > 59) return;       // invalid minutes value
            cmos.clock.min = val; break;
        case 0x03:      /* Minutes, alarm */
            if (val > 59) return;       // invalid minutes value
            cmos.alarm.min = val; break;
        case 0x04:      /* Hours */
            if (cmos.ampm)              // 12h am/pm mode
            {
                if ((val > 12 && val < 0x81) || val > 0x8c) return; // invalid hour value
                if (val > 12) val -= (0x80-12);         // convert pm to 24h
            }
            else                        // 24h mode
            {
                if (val > 23) return;                               // invalid hour value
            }

            cmos.clock.hour = val; break;
        case 0x05:      /* Hours, alarm */
            if (cmos.ampm)              // 12h am/pm mode
            {
                if ((val > 12 && val < 0x81) || val > 0x8c) return; // invalid hour value
                if (val > 12) val -= (0x80-12);         // convert pm to 24h
            }
            else                        // 24h mode
            {
                if (val > 23) return;                               // invalid hour value
            }

            cmos.alarm.hour = val; break;
        case 0x06:      /* Day of week */
            cmos.clock.weekday = val; break;
        case 0x07:      /* Date of month */
            if (val > 31) return;               // invalid date value (mktime() should catch the rest)
            cmos.clock.day = val; break;
        case 0x08:      /* Month */
            if (val < 1 || val > 12) return;               // invalid month value
            cmos.clock.month = val; break;
        case 0x09:      /* Year */
            cmos.clock.year -= cmos.clock.year % 100;
            cmos.clock.year += val;
            break;
        case 0x32:      /* Century */
        case 0x37:      /* Century (alternate used by Windows NT/2000/XP) */
            if (val < 19) return;               // invalid century value?
            cmos.clock.year %= 100;
            cmos.clock.year += val * 100;
            break;
        case 0x0a:      /* Status reg A */
            cmos.regs[cmos.reg]=val & 0x7f;
            if ((val & 0x70)!=0x20) LOG(LOG_BIOS,LOG_ERROR)("CMOS:Illegal 22 stage divider value");
            cmos.timer.div=(val & 0xf);
            cmos_checktimer();
            break;
        case 0x0b:      /* Status reg B */
            {
                cmos.ampm = !(val & 0x02);
                cmos.bcd = !(val & 0x04);
                cmos.lock = (val & 0x80) != 0;
                if (cmos.lock) val &= ~0x10; /* Setting bit 7 clears bit 4 (UEI) */
                cmos.regs[cmos.reg] = (uint8_t)val;
                cmos_checktimer();
            }
            break;
        case 0x0c:      /* Status reg C */
            break;
        case 0x0d:      /* Status reg D */
            cmos.regs[cmos.reg]=val & 0x80; /*Bit 7=1:RTC Power on*/
            break;
        case 0x0f:      /* Shutdown status byte */
            cmos.regs[cmos.reg]=val & 0x7f;
            break;
        default:
            LOG(LOG_BIOS, LOG_NORMAL)("CMOS:Writing to register %x", cmos.reg);
            cmos.regs[cmos.reg]=val;
            break;
    }
}

unsigned char CMOS_GetShutdownByte() {
    return cmos.regs[0x0F];
}

#define MAKE_RETURN(_VAL) ((unsigned char)(cmos.bcd ? (((((unsigned int)_VAL) / 10U) << 4U) | (((unsigned int)_VAL) % 10U)) : ((unsigned int)_VAL)))

/* NOTES: Some implementations of the RTC and how they differ.
 *
 *        Compaq Elite 486 laptop: If you write port 70h, you are allowed *ONE* read or write to 71h.
 *                                 If you read or write again, it is ignored until you write port 70h again.
 *                                 Here at DOSBox-X, as many DOSBox forks are, we're not that stingy.
 */

static Bitu cmos_readreg(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    if (cmos.reg>0x3f) {
        LOG(LOG_BIOS,LOG_ERROR)("CMOS:Read attempted from illegal register %x",cmos.reg);
        return 0xff;
    }

    Bitu drive_a, drive_b;
    uint8_t hdparm;

    switch (cmos.reg) {
    case 0x00:      /* Seconds */
        return    MAKE_RETURN(cmos.clock.sec);
    case 0x01:      /* Seconds, alarm */
        return    MAKE_RETURN(cmos.alarm.sec);
    case 0x02:      /* Minutes */
        return    MAKE_RETURN(cmos.clock.min);
    case 0x03:      /* Minutes, alarm */
        return    MAKE_RETURN(cmos.alarm.min);
    case 0x04:      /* Hours */
        return    MAKE_RETURN(cmos.clock.hour);
    case 0x05:      /* Hours, alarm */
        return    MAKE_RETURN(cmos.alarm.hour);
    case 0x06:      /* Day of week */
        return    MAKE_RETURN(cmos.clock.weekday);
    case 0x07:      /* Date of month */
        return    MAKE_RETURN(cmos.clock.day);
    case 0x08:      /* Month */
        return    MAKE_RETURN(cmos.clock.month);
    case 0x09:      /* Year */
        return    MAKE_RETURN(cmos.clock.year % 100);
    case 0x32:      /* Century */
    case 0x37:      /* Century (alternate used by Windows NT/2000/XP) */
        return    MAKE_RETURN(cmos.clock.year / 100);
    case 0x0a:      /* Status register A */
	{ // take bit 7 of reg b into account (if set, never updates)
            pic_tickindex_t dt = PIC_FullIndex() - cmos.last.ended;

            if (cmos.lock ||                            // if lock then never updated, so reading safe
                dt >= 0.244) {                          // if 0, at least 244 usec should be available
                return cmos.regs[0x0a];                 // reading safe
            } else {
                return cmos.regs[0x0a] | 0x80;          // reading not safe!
            }
	}
    case 0x0c:      /* Status register C */
    {
        cmos.timer.acknowledged=true;
        uint8_t val = cmos.regs[0xc];
        if(((cmos.regs[0xb] & 0x40) > 0) && ((cmos.regs[0xc] & 0x40) > 0)) { // If both PF and PIE are 1
            val |= 0x80 | 0x40; // Set Interrupt Request Flag (IRQF) to 1, PF = 1
        }
        if(((cmos.regs[0xb] & 0x10) > 0) && ((cmos.regs[0xc] & 0x10) > 0)) { // If both UF and UIE are 1
            val |= 0x80 | 0x10; // Set Interrupt Request Flag (IRQF) to 1, UF = 1
        }
        if(((cmos.regs[0xb] & 0x20) > 0) && ((cmos.regs[0xc] & 0x20) > 0)) { // If both AF and AIE are 1
            val |= 0x80 | 0x20; // Set Interrupt Request Flag (IRQF) to 1, AF = 1
        }
        cmos.regs[0xc] = 0; // All flags are cleared by reading the register
        return val;
    }
    case 0x10:      /* Floppy size */
        drive_a = 0;
        drive_b = 0;
        if(imageDiskList[0] != NULL) drive_a = imageDiskList[0]->GetBiosType();
        if(imageDiskList[1] != NULL) drive_b = imageDiskList[1]->GetBiosType();
        return ((drive_a << 4) | drive_b);
    /* First harddrive info */
    case 0x12:
        /* NTS: DOSBox 0.74 mainline has these backwards: the upper nibble is the first hard disk,
           the lower nibble is the second hard disk. It makes a big difference to stupid OS's like
           Windows 95. */
        hdparm = 0;
        if(imageDiskList[3] != NULL) hdparm |= 0xf;
        if(imageDiskList[2] != NULL) hdparm |= 0xf0;
//      hdparm = 0;
        return hdparm;
    case 0x19:
        if(imageDiskList[2] != NULL) return 47; /* User defined type */
        return 0;
    case 0x1b:
        if(imageDiskList[2] != NULL) return (imageDiskList[2]->cylinders & 0xff);
        return 0;
    case 0x1c:
        if(imageDiskList[2] != NULL) return ((imageDiskList[2]->cylinders & 0xff00)>>8);
        return 0;
    case 0x1d:
        if(imageDiskList[2] != NULL) return (imageDiskList[2]->heads);
        return 0;
    case 0x1e:
    case 0x1f:
        if(imageDiskList[2] != NULL) return 0xff;
        return 0;
    case 0x20:
        if(imageDiskList[2] != NULL) return (0xc0 | (((imageDiskList[2]->heads) > 8) << 3));
        return 0;
    case 0x21:
        if(imageDiskList[2] != NULL) return (imageDiskList[2]->cylinders & 0xff);
        return 0;
    case 0x22:
        if(imageDiskList[2] != NULL) return ((imageDiskList[2]->cylinders & 0xff00)>>8);
        return 0;
    case 0x23:
        if(imageDiskList[2] != NULL) return (imageDiskList[2]->sectors);
        return 0;
    /* Second harddrive info */
    case 0x1a:
        if(imageDiskList[3] != NULL) return 47; /* User defined type */
        return 0;
    case 0x24:
        if(imageDiskList[3] != NULL) return (imageDiskList[3]->cylinders & 0xff);
        return 0;
    case 0x25:
        if(imageDiskList[3] != NULL) return ((imageDiskList[3]->cylinders & 0xff00)>>8);
        return 0;
    case 0x26:
        if(imageDiskList[3] != NULL) return (imageDiskList[3]->heads);
        return 0;
    case 0x27:
    case 0x28:
        if(imageDiskList[3] != NULL) return 0xff;
        return 0;
    case 0x29:
        if(imageDiskList[3] != NULL) return (0xc0 | (((imageDiskList[3]->heads) > 8) << 3));
        return 0;
    case 0x2a:
        if(imageDiskList[3] != NULL) return (imageDiskList[3]->cylinders & 0xff);
        return 0;
    case 0x2b:
        if(imageDiskList[3] != NULL) return ((imageDiskList[3]->cylinders & 0xff00)>>8);
        return 0;
    case 0x2c:
        if(imageDiskList[3] != NULL) return (imageDiskList[3]->sectors);
        return 0;
    case 0x39:
        return 0;
    case 0x3a:
        return 0;


    case 0x0b:      /* Status register B */
    case 0x0d:      /* Status register D */
    case 0x0f:      /* Shutdown status byte */
    case 0x14:      /* Equipment */
    case 0x15:      /* Base Memory KB Low Byte */
    case 0x16:      /* Base Memory KB High Byte */
    case 0x17:      /* Extended memory in KB Low Byte */
    case 0x18:      /* Extended memory in KB High Byte */
    case 0x30:      /* Extended memory in KB Low Byte */
    case 0x31:      /* Extended memory in KB High Byte */
//      LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Read from reg %X : %04X",cmos.reg,cmos.regs[cmos.reg]);
        return cmos.regs[cmos.reg];
    case 0x2F:
        extern bool PS1AudioCard;
        if( PS1AudioCard )
            return 0xFF;
    default:
        LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Reading from register %X",cmos.reg);
        return cmos.regs[cmos.reg];
    }
}

void CMOS_SetRegister(Bitu regNr, uint8_t val) {
    cmos.regs[regNr] = val;
}


static IO_ReadHandleObject ReadHandler[2];
static IO_WriteHandleObject WriteHandler[2];

void CMOS_Destroy(Section* sec) {
    (void)sec;//UNUSED
}

void CMOS_Reset(Section* sec) {
    (void)sec;//UNUSED
    LOG(LOG_MISC,LOG_DEBUG)("CMOS_Reset(): reinitializing CMOS/RTC controller");

    WriteHandler[0].Uninstall();
    WriteHandler[1].Uninstall();
    ReadHandler[0].Uninstall();
    ReadHandler[1].Uninstall();

    if (IS_PC98_ARCH)
        return;

    cmos.last.ended = PIC_FullIndex() - 1000; /* PIC time resets when the guest VM is reset */
    WriteHandler[0].Install(0x70,cmos_selreg,IO_MB);
    WriteHandler[1].Install(0x71,cmos_writereg,IO_MB);
    ReadHandler[0].Install(0x71,cmos_readreg,IO_MB);
    cmos.timer.acknowledged=true;
    cmos.reg=0xa;
    cmos_writereg(0x71,0x26,1);
    cmos.reg=0xb;
    cmos_writereg(0x71,0x2,1);
    cmos.regs[0x0c] = 0;
    cmos.regs[0x0d]=(uint8_t)0x80;
    // Equipment is updated from bios.cpp and bios_disk.cpp
    /* Fill in base memory size, it is 640K always */
    cmos.regs[0x15]=(uint8_t)0x80;
    cmos.regs[0x16]=(uint8_t)0x02;
    /* Fill in extended memory size */
    Bitu exsize=MEM_TotalPages()*4;
    if (exsize >= 1024) exsize -= 1024;
    else exsize = 0;
    if (exsize > 65535) exsize = 65535; /* cap at 64MB. this value is returned as-is by INT 15H AH=0x88 in a 16-bit register */
    cmos.regs[0x17]=(uint8_t)exsize;
    cmos.regs[0x18]=(uint8_t)(exsize >> 8);
    cmos.regs[0x30]=(uint8_t)exsize;
    cmos.regs[0x31]=(uint8_t)(exsize >> 8);
}

void CMOS_Init() {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing CMOS/RTC");

    cmos_sync_time(time(NULL));

    AddExitFunction(AddExitFunctionFuncPair(CMOS_Destroy),true);
    AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(CMOS_Reset));
}

// save state support
void *cmos_timerevent_PIC_Event = (void*)((uintptr_t)cmos_timerevent);

namespace
{
class SerializeCmos : public SerializeGlobalPOD
{
public:
    SerializeCmos() : SerializeGlobalPOD("CMOS")
    {
        registerPOD(cmos.regs);
        registerPOD(cmos.nmi);
        registerPOD(cmos.reg);
        registerPOD(cmos.timer.div);
        registerPOD(cmos.timer.delay);
        registerPOD(cmos.timer.acknowledged);
        registerPOD(cmos.last.ended);
    }
} dummy;
}
