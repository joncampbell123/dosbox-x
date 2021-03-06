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
bool date_host_forced=false;
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
        bool enabled;
        uint8_t div;
        float delay;
        bool acknowledged;
    } timer;
    struct {
        double timer;
        double ended;
        double alarm;
    } last;
    bool update_ended;
    time_t time_diff;           // difference between real UTC and DOSbox UTC
    struct timeval locktime;    // UTC time of setting lock bit
    struct timeval safetime;    // UTC time of last safe time
} cmos;

static void cmos_timerevent(Bitu val) {
    (void)val;//UNUSED
    if (cmos.timer.acknowledged) {
        cmos.timer.acknowledged=false;
        PIC_ActivateIRQ(8);
    }
    if (cmos.timer.enabled) {
        PIC_AddEvent(cmos_timerevent,cmos.timer.delay);
        cmos.regs[0xc] = 0xC0;//Contraption Zack (music)
    }
}

static void cmos_checktimer(void) {
    PIC_RemoveEvents(cmos_timerevent);
    if (cmos.timer.div<=2) cmos.timer.div+=7;
    cmos.timer.delay=(1000.0f/(32768.0f / (1 << (cmos.timer.div - 1))));
    if (!cmos.timer.div || !cmos.timer.enabled) return;
    LOG(LOG_PIT,LOG_NORMAL)("RTC Timer at %.2f hz",1000.0/cmos.timer.delay);
//  PIC_AddEvent(cmos_timerevent,cmos.timer.delay);
    /* A rtc is always running */
    double remd=fmod(PIC_FullIndex(),(double)cmos.timer.delay);
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
    if (date_host_forced && (cmos.reg <= 0x09 || cmos.reg == 0x32)) {   // date/time related registers
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

        struct tm *loctime;         // local dosbox time (based on dosbox UTC)

        if (cmos.lock)              // if locked, use locktime instead of current time
        {
            loctime = localtime((time_t*)&cmos.locktime.tv_sec);
        }
        else                        // not locked, use current time
        {
            struct timeval curtime;
            gettimeofday(&curtime, NULL);
            curtime.tv_sec += cmos.time_diff;
            loctime = localtime((time_t*)&curtime.tv_sec);
        }

        switch (cmos.reg)
        {
        case 0x00:      /* Seconds */
            if (val > 59) return;       // invalid seconds value
            loctime->tm_sec = (int)val;
            break;

        case 0x02:      /* Minutes */
            if (val > 59) return;       // invalid minutes value
            loctime->tm_min = (int)val;
            break;

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

            loctime->tm_hour = (int)val;         
            break;

        case 0x06:      /* Day of week */
            // seems silly to set this, as it is calculated? ignore for now
            break;

        case 0x07:      /* Date of month */
            if (val > 31) return;               // invalid date value (mktime() should catch the rest)
            loctime->tm_mday = (int)val;
            break;

        case 0x08:      /* Month */
            if (val > 12) return;               // invalid month value
            loctime->tm_mon = (int)val;
            break;

        case 0x09:      /* Year */
            loctime->tm_year = (int)val;
            break;

        case 0x32:      /* Century */
            if (val < 19) return;               // invalid century value?
            loctime->tm_year += (int)((val * 100) - 1900);
            break;

        case 0x01:      /* Seconds Alarm */
        case 0x03:      /* Minutes Alarm */
        case 0x05:      /* Hours Alarm */
            LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Trying to set alarm");
            cmos.regs[cmos.reg] = (uint8_t)val;
            return;     // done
        }

        time_t newtime = mktime(loctime);       // convert new local time back to dosbox UTC

        if (newtime != (time_t)-1)
        {
            if (!cmos.lock)         // no lock, takes immediate effect
            {
                cmos.time_diff = newtime - time(NULL);  // calculate new diff
            }
            else
            {
                cmos.locktime.tv_sec = newtime;         // store for later use
                // no need to set usec, we don't use it
            }
        }

        return;
    }

    switch (cmos.reg) {
    case 0x00:      /* Seconds */
    case 0x02:      /* Minutes */
    case 0x04:      /* Hours */
    case 0x06:      /* Day of week */
    case 0x07:      /* Date of month */
    case 0x08:      /* Month */
    case 0x09:      /* Year */
    case 0x32:              /* Century */
        /* Ignore writes to change alarm */
        if(!date_host_forced) break;
    case 0x01:      /* Seconds Alarm */
    case 0x03:      /* Minutes Alarm */
    case 0x05:      /* Hours Alarm */
        if(!date_host_forced) {
            LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Trying to set alarm");
            cmos.regs[cmos.reg]=(uint8_t)val;
            break;
        }
    case 0x0a:      /* Status reg A */
        cmos.regs[cmos.reg]=val & 0x7f;
        if ((val & 0x70)!=0x20) LOG(LOG_BIOS,LOG_ERROR)("CMOS Illegal 22 stage divider value");
        cmos.timer.div=(val & 0xf);
        cmos_checktimer();
        break;
    case 0x0b:      /* Status reg B */
        if(date_host_forced) {
            bool waslocked = cmos.lock;

            cmos.ampm = !(val & 0x02);
            cmos.bcd = !(val & 0x04);
            if ((val & 0x10) != 0) LOG(LOG_BIOS,LOG_ERROR)("CMOS:Updated ended interrupt not supported yet");
            cmos.timer.enabled = (val & 0x40) > 0;
            cmos.lock = (val & 0x80) != 0;

            if (cmos.lock)              // if locked, set locktime for later use
            {
                if (!waslocked)         // if already locked, no further action
                {
                    // locked for the first time, calculate dosbox UTC
                    gettimeofday(&cmos.locktime, NULL);
                    cmos.locktime.tv_sec += cmos.time_diff;
                }
            }
            else if (waslocked)         // time was locked, now unlock
            {
                // calculate new diff between real UTC and dosbox UTC
                cmos.time_diff = cmos.locktime.tv_sec - time(NULL);
            }

            cmos.regs[cmos.reg] = (uint8_t)val;
            cmos_checktimer();
        } else {
            cmos.bcd=!(val & 0x4);
            cmos.regs[cmos.reg]=val & 0x7f;
            cmos.timer.enabled=(val & 0x40)>0;
            if (val&0x10) LOG(LOG_BIOS,LOG_ERROR)("CMOS:Updated ended interrupt not supported yet");
            cmos_checktimer();
        }
        break;
    case 0x0c:
        if(date_host_forced) break;
    case 0x0d:/* Status reg D */
        if(!date_host_forced) {
            cmos.regs[cmos.reg]=val & 0x80; /*Bit 7=1:RTC Pown on*/
        }
        break;
    case 0x0f:      /* Shutdown status byte */
        cmos.regs[cmos.reg]=val & 0x7f;
        break;
    default:
        cmos.regs[cmos.reg]=val & 0x7f;
        LOG(LOG_BIOS,LOG_ERROR)("CMOS:WRite to unhandled register %x",cmos.reg);
    }
}

unsigned char CMOS_GetShutdownByte() {
    return cmos.regs[0x0F];
}

#define MAKE_RETURN(_VAL) ((unsigned char)(cmos.bcd ? (((((unsigned int)_VAL) / 10U) << 4U) | (((unsigned int)_VAL) % 10U)) : ((unsigned int)_VAL)))

static Bitu cmos_readreg(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    if (cmos.reg>0x3f) {
        LOG(LOG_BIOS,LOG_ERROR)("CMOS:Read from illegal register %x",cmos.reg);
        return 0xff;
    }

    // JAL_20060817 - rewrote most of the date/time part
    if (date_host_forced && (cmos.reg <= 0x09 || cmos.reg == 0x32)) {       // date/time related registers
        struct tm* loctime;

        if (cmos.lock)              // if locked, use locktime instead of current time
        {
            loctime = localtime((time_t*)&cmos.locktime.tv_sec);
        }
        else                        // not locked, get current time
        {
            struct timeval curtime;
            gettimeofday(&curtime, NULL);
    
            // allow a little more leeway (1 sec) than the .244 sec officially given
            if (curtime.tv_sec - cmos.safetime.tv_sec == 1 &&
                curtime.tv_usec < cmos.safetime.tv_usec)
            {
                curtime = cmos.safetime;        // within safe range, use safetime instead of current time
            }

            curtime.tv_sec += cmos.time_diff;
            loctime = localtime((time_t*)&curtime.tv_sec);
        }

        switch (cmos.reg)
        {
        case 0x00:      // seconds
            return MAKE_RETURN(loctime->tm_sec);
        case 0x02:      // minutes
            return MAKE_RETURN(loctime->tm_min);
        case 0x04:      // hours
            if (cmos.ampm && loctime->tm_hour > 12)     // time pm, convert
            {
                loctime->tm_hour -= 12;
                loctime->tm_hour += (cmos.bcd) ? 80 : 0x80;
            }
            return MAKE_RETURN(loctime->tm_hour);
        case 0x06:      /* Day of week */
            return MAKE_RETURN(loctime->tm_wday + 1);
        case 0x07:      /* Date of month */
            return MAKE_RETURN(loctime->tm_mday);
        case 0x08:      /* Month */
            return MAKE_RETURN(loctime->tm_mon + 1);
        case 0x09:      /* Year */
            return MAKE_RETURN(loctime->tm_year % 100);
        case 0x32:      /* Century */
            return MAKE_RETURN(loctime->tm_year / 100 + 19);

        case 0x01:      /* Seconds Alarm */
        case 0x03:      /* Minutes Alarm */
        case 0x05:      /* Hours Alarm */
            return MAKE_RETURN(cmos.regs[cmos.reg]);
        }
    }

    Bitu drive_a, drive_b;
    uint8_t hdparm;
    time_t curtime;
    struct tm *loctime;
    /* Get the current time. */
    curtime = time (NULL);

    /* Convert it to local time representation. */
    loctime = localtime (&curtime);

    switch (cmos.reg) {
    case 0x00:      /* Seconds */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_sec);
    case 0x02:      /* Minutes */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_min);
    case 0x04:      /* Hours */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_hour);
    case 0x06:      /* Day of week */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_wday + 1);
    case 0x07:      /* Date of month */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_mday);
    case 0x08:      /* Month */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_mon + 1);
    case 0x09:      /* Year */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_year % 100);
    case 0x32:      /* Century */
        if(!date_host_forced) return    MAKE_RETURN(loctime->tm_year / 100 + 19);
    case 0x01:      /* Seconds Alarm */
    case 0x03:      /* Minutes Alarm */
    case 0x05:      /* Hours Alarm */
        if(!date_host_forced) return cmos.regs[cmos.reg];
    case 0x0a:      /* Status register A */
        if(date_host_forced) {
            // take bit 7 of reg b into account (if set, never updates)
            gettimeofday (&cmos.safetime, NULL);        // get current UTC time
            if (cmos.lock ||                            // if lock then never updated, so reading safe
                cmos.safetime.tv_usec < (1000-244)) {   // if 0, at least 244 usec should be available
                return cmos.regs[0x0a];                 // reading safe
            } else {
                return cmos.regs[0x0a] | 0x80;          // reading not safe!
            }
        } else {
            if (PIC_TickIndex()<0.002) {
                return (cmos.regs[0x0a]&0x7f) | 0x80;
            } else {
                return (cmos.regs[0x0a]&0x7f);
            }
        }
    case 0x0c:      /* Status register C */
        cmos.timer.acknowledged=true;
        if (cmos.timer.enabled) {
            /* In periodic interrupt mode only care for those flags */
            uint8_t val=cmos.regs[0xc];
            cmos.regs[0xc]=0;
            return val;
        } else {
            /* Give correct values at certain times */
            uint8_t val=0;
            double index=PIC_FullIndex();
            if (index>=(cmos.last.timer+cmos.timer.delay)) {
                cmos.last.timer=index;
                val|=0x40;
            } 
            if (index>=(cmos.last.ended+1000)) {
                cmos.last.ended=index;
                val|=0x10;
            }
            if(date_host_forced) cmos.regs[0xc] = 0;        // JAL_20060817 - reset here too!
            return val;
        }
    case 0x10:      /* Floppy size */
        drive_a = 0;
        drive_b = 0;
        if(imageDiskList[0] != NULL) drive_a = imageDiskList[0]->GetBiosType();
        if(imageDiskList[1] != NULL) drive_b = imageDiskList[1]->GetBiosType();
        return ((drive_a << 4) | (drive_b));
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
        if(imageDiskList[2] != NULL) return 0xff;
        return 0;
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
        if(imageDiskList[3] != NULL) return 0xff;
        return 0;
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
        LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Read from reg %X",cmos.reg);
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

    WriteHandler[0].Install(0x70,cmos_selreg,IO_MB);
    WriteHandler[1].Install(0x71,cmos_writereg,IO_MB);
    ReadHandler[0].Install(0x71,cmos_readreg,IO_MB);
    cmos.timer.enabled=false;
    cmos.timer.acknowledged=true;
    cmos.reg=0xa;
    cmos_writereg(0x71,0x26,1);
    cmos.reg=0xb;
    cmos_writereg(0x71,0x2,1);  //Struct tm *loctime is of 24 hour format,
    if(date_host_forced) {
        cmos.regs[0x0d]=(uint8_t)0x80;
    } else {
        cmos.reg=0xd;
        cmos_writereg(0x71,0x80,1); /* RTC power on */
    }
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
    if (date_host_forced) {
        cmos.time_diff = 0;
        cmos.locktime.tv_sec = 0;
    }
}

void CMOS_Init() {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing CMOS/RTC");

    if (control->opt_date_host_forced) {
        LOG_MSG("Synchronize date with host: Forced");
        date_host_forced=true;
    }

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
        registerPOD(cmos.timer.enabled);
        registerPOD(cmos.timer.div);
        registerPOD(cmos.timer.delay);
        registerPOD(cmos.timer.acknowledged);
        registerPOD(cmos.last.timer);
        registerPOD(cmos.last.ended);
        registerPOD(cmos.last.alarm);
        registerPOD(cmos.update_ended);
    }
} dummy;
}
